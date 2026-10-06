// Standalone bounded HTTPS management; operation truth stays in management_product.
use crate::{
    Output, management_local as local, management_pairing as pairing, management_product,
    registry::Invocation,
};
use pairing::{Result, Store};
use rustls::{
    ServerConfig, ServerConnection, StreamOwned,
    pki_types::{CertificateDer, PrivateKeyDer, PrivatePkcs8KeyDer},
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::{
    io::{self, Read, Write},
    net::{SocketAddr, TcpListener, TcpStream},
    os::unix::net::UnixStream,
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
    thread,
    time::{Duration, Instant},
};
const BODY_CAP: usize = 131072;
const HEADER_CAP: usize = 16384;
const CONNECTION_CAP: usize = 8;
const READ_SECONDS: u64 = 10;
struct Request {
    method: String,
    path: String,
    authorization: Option<String>,
    expected_device: Option<String>,
    body: Vec<u8>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct PairingRequest {
    schema: String,
    credential_hash: String,
    client_name: String,
}
struct Service {
    store: Store,
    device: String,
    name: String,
}
impl Service {
    fn identity(&self) -> Result<Value> {
        Ok(
            json!({"schema":"yvex.management.identity.v1","device_identity":self.device,
            "display_name":self.name,"protocol":"yvex.management.v2",
            "pairing_available":self.store.available()?}),
        )
    }
    fn route(&self, request: Request, uid: Option<u32>) -> (u16, Value) {
        let result = self.handle(request, uid);
        match result {
            Ok(data) => (200, data),
            Err(reason) => (
                403,
                json!({"schema":"yvex.management.error.v1","reason":reason}),
            ),
        }
    }
    fn handle(&self, request: Request, uid: Option<u32>) -> Result<Value> {
        match (request.method.as_str(), request.path.as_str()) {
            ("GET", "/v1/identity") => self.identity(),
            ("POST", "/v1/pairing") if uid.is_none() => {
                let body: PairingRequest =
                    serde_json::from_slice(&request.body).map_err(|_| "invalid_pairing_request")?;
                if body.schema != "yvex.management.pairing.request.v1" {
                    return Err("unsupported_pairing_contract");
                }
                self.store.request(&body.credential_hash, &body.client_name)
            }
            ("GET", "/v1/pairing/status") if uid.is_none() => self
                .store
                .status(&bearer(request.authorization.as_deref())?),
            ("POST", "/v1/owner/bootstrap") if uid.is_none() => {
                let input =
                    serde_json::from_slice(&request.body).map_err(|_| "invalid_owner_bootstrap")?;
                self.store.claim_owner(input)
            }
            ("GET", "/v1/owner/status") if uid.is_none() => self
                .store
                .owner_status(&bearer(request.authorization.as_deref())?),
            ("GET", "/v1/owner/connections") if uid.is_none() => self
                .store
                .owner_connections(&bearer(request.authorization.as_deref())?),
            ("POST", "/v1/owner/actions") if uid.is_none() => {
                let input =
                    serde_json::from_slice(&request.body).map_err(|_| "invalid_owner_action")?;
                self.store
                    .owner_action(&bearer(request.authorization.as_deref())?, input)
            }
            ("GET", path) if uid.is_none() && path.starts_with("/v1/owner/actions/") => {
                self.store.owner_receipt(
                    &bearer(request.authorization.as_deref())?,
                    path.trim_start_matches("/v1/owner/actions/"),
                )
            }
            ("POST", "/v1/management") => {
                if request
                    .expected_device
                    .as_ref()
                    .is_some_and(|value| value != &self.device)
                    || uid.is_some() && request.expected_device.as_deref() != Some(&self.device)
                {
                    return Err("stale_management_device_identity");
                }
                let peer = if let Some(uid) = uid {
                    format!("local-user:{uid}")
                } else {
                    let hash = bearer(request.authorization.as_deref())?;
                    self.store.authorize(&hash)?;
                    format!("credential-sha256:{hash}")
                };
                // Receipt identity is transport-domain separated, unlike the public peer label.
                let journal = pairing::digest(format!("yvex.management.peer.v1:{peer}").as_bytes());
                management_product::network_request(&request.body, &self.device, &peer, &journal)
            }
            _ => Err("unsupported_management_route"),
        }
    }
}
fn bearer(value: Option<&str>) -> Result<String> {
    let raw = value
        .and_then(|v| v.strip_prefix("Bearer "))
        .ok_or("credential_required")?;
    pairing::credential_hash(raw)
}
fn header_end(bytes: &[u8]) -> Option<usize> {
    bytes
        .windows(4)
        .position(|v| v == b"\r\n\r\n")
        .map(|n| n + 4)
}
fn read_request(stream: &mut impl Read) -> Result<Request> {
    let mut bytes = Vec::with_capacity(1024);
    let mut chunk = [0u8; 4096];
    let end = loop {
        if let Some(end) = header_end(&bytes) {
            break end;
        }
        if bytes.len() >= HEADER_CAP {
            return Err("http_headers_bound");
        }
        let count = stream
            .read(&mut chunk)
            .map_err(|_| "http_read_unavailable")?;
        if count == 0 {
            return Err("http_request_incomplete");
        }
        bytes.extend_from_slice(&chunk[..count]);
    };
    if end > HEADER_CAP {
        return Err("http_headers_bound");
    }
    let mut headers = [httparse::EMPTY_HEADER; 48];
    let mut parsed = httparse::Request::new(&mut headers);
    if !parsed
        .parse(&bytes[..end])
        .map_err(|_| "malformed_http")?
        .is_complete()
        || parsed.version != Some(1)
    {
        return Err("malformed_http");
    }
    let method = parsed.method.ok_or("malformed_http")?.to_owned();
    let path = parsed.path.ok_or("malformed_http")?.to_owned();
    if !matches!(method.as_str(), "GET" | "POST")
        || path.len() > 128
        || path.contains('?')
        || path.contains('#')
    {
        return Err("unsupported_management_route");
    }
    let mut authorization = None;
    let mut expected_device = None;
    let mut content_length = None;
    let mut host = false;
    let mut json_type = false;
    for header in parsed.headers.iter() {
        let value = std::str::from_utf8(header.value).map_err(|_| "malformed_http")?;
        match header.name.to_ascii_lowercase().as_str() {
            "origin" | "transfer-encoding" | "expect" => return Err("unsupported_http_header"),
            "authorization" => {
                if authorization.is_some() || value.len() > 80 {
                    return Err("invalid_authorization");
                }
                authorization = Some(value.to_owned());
            }
            "x-yvex-device-identity" => {
                if expected_device.is_some()
                    || value.len() != 75
                    || !value.starts_with("tls-sha256:")
                {
                    return Err("invalid_device_identity_header");
                }
                expected_device = Some(value.to_owned());
            }
            "host" => {
                if host || value.is_empty() {
                    return Err("invalid_host_header");
                }
                host = true;
            }
            "content-length" => {
                if content_length.is_some()
                    || value.is_empty()
                    || !value.bytes().all(|b| b.is_ascii_digit())
                {
                    return Err("invalid_content_length");
                }
                content_length = Some(
                    value
                        .parse::<usize>()
                        .map_err(|_| "invalid_content_length")?,
                );
            }
            "content-type" => {
                if json_type || !value.eq_ignore_ascii_case("application/json") {
                    return Err("unsupported_content_type");
                }
                json_type = true;
            }
            _ => {}
        }
    }
    if !host {
        return Err("host_required");
    }
    let length = content_length.unwrap_or(0);
    if length > BODY_CAP
        || method == "GET" && length != 0
        || method == "POST" && (!json_type || content_length.is_none())
    {
        return Err("invalid_body_framing");
    }
    if bytes.len() > end + length {
        return Err("unexpected_http_bytes");
    }
    while bytes.len() < end + length {
        let remaining = (end + length - bytes.len()).min(chunk.len());
        let count = stream
            .read(&mut chunk[..remaining])
            .map_err(|_| "http_read_unavailable")?;
        if count == 0 {
            return Err("http_request_incomplete");
        }
        bytes.extend_from_slice(&chunk[..count]);
    }
    Ok(Request {
        method,
        path,
        authorization,
        expected_device,
        body: bytes[end..].to_vec(),
    })
}
fn respond(stream: &mut impl Write, status: u16, data: Value) -> io::Result<()> {
    let body = data.to_string();
    let status_text = if status == 200 { "OK" } else { "Forbidden" };
    write!(
        stream,
        concat!(
            "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\n",
            "Content-Length: {}\r\nConnection: close\r\nCache-Control: no-store\r\n",
            "X-Content-Type-Options: nosniff\r\n\r\n{}"
        ),
        status,
        status_text,
        body.len(),
        body
    )?;
    stream.flush()
}
enum Socket {
    Tcp(TcpStream),
    Local(UnixStream),
}
struct DeadlineSocket {
    socket: Socket,
    deadline: Instant,
}
impl DeadlineSocket {
    fn new(socket: Socket) -> Self {
        Self {
            socket,
            deadline: Instant::now() + Duration::from_secs(READ_SECONDS),
        }
    }
    fn remaining(&self) -> io::Result<Duration> {
        self.deadline
            .checked_duration_since(Instant::now())
            .filter(|d| !d.is_zero())
            .ok_or_else(|| io::Error::from(io::ErrorKind::TimedOut))
    }
}
impl Read for DeadlineSocket {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        let timeout = Some(self.remaining()?);
        match &mut self.socket {
            Socket::Tcp(s) => {
                s.set_read_timeout(timeout)?;
                s.read(bytes)
            }
            Socket::Local(s) => {
                s.set_read_timeout(timeout)?;
                s.read(bytes)
            }
        }
    }
}
impl Write for DeadlineSocket {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        let timeout = Some(self.remaining()?);
        match &mut self.socket {
            Socket::Tcp(s) => {
                s.set_write_timeout(timeout)?;
                s.write(bytes)
            }
            Socket::Local(s) => {
                s.set_write_timeout(timeout)?;
                s.write(bytes)
            }
        }
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
fn tls_config(identity: pairing::Identity) -> Result<ServerConfig> {
    let provider = Arc::new(rustls::crypto::ring::default_provider());
    let mut config = ServerConfig::builder_with_provider(provider)
        .with_safe_default_protocol_versions()
        .map_err(|_| "tls_unavailable")?
        .with_no_client_auth()
        .with_single_cert(
            vec![CertificateDer::from(identity.certificate)],
            PrivateKeyDer::Pkcs8(PrivatePkcs8KeyDer::from(identity.private_key)),
        )
        .map_err(|_| "service_identity_malformed")?;
    config.alpn_protocols = vec![b"http/1.1".to_vec()];
    config.max_early_data_size = 0;
    Ok(config)
}
fn tcp_client(socket: TcpStream, config: Arc<ServerConfig>, service: Arc<Service>) {
    let Ok(connection) = ServerConnection::new(config) else {
        return;
    };
    let mut stream = StreamOwned::new(connection, DeadlineSocket::new(Socket::Tcp(socket)));
    let reply = match read_request(&mut stream) {
        Ok(request) => service.route(request, None),
        Err(_) if stream.conn.is_handshaking() => return,
        Err(reason) => (
            403,
            json!({"schema":"yvex.management.error.v1","reason":reason}),
        ),
    };
    stream.sock.deadline = Instant::now() + Duration::from_secs(5);
    let _ = respond(&mut stream, reply.0, reply.1);
}
fn local_client(socket: UnixStream, service: Arc<Service>) {
    let Ok(uid) = local::same_user(&socket) else {
        return;
    };
    let mut stream = DeadlineSocket::new(Socket::Local(socket));
    let reply = match read_request(&mut stream) {
        Ok(request) => service.route(request, Some(uid)),
        Err(reason) => (
            403,
            json!({"schema":"yvex.management.error.v1","reason":reason}),
        ),
    };
    stream.deadline = Instant::now() + Duration::from_secs(5);
    let _ = respond(&mut stream, reply.0, reply.1);
}
fn advertise(address: SocketAddr, service: &Service) -> Result<mdns_sd::ServiceDaemon> {
    if address.ip().is_loopback() {
        return Err("advertisement_requires_explicit_lan_bind");
    }
    let daemon = mdns_sd::ServiceDaemon::new().map_err(|_| "discovery_unavailable")?;
    let suffix = &service.device[11..23];
    let instance = format!("YVEX {suffix}");
    let hostname = format!("yvex-{suffix}.local.");
    let properties = [
        ("device_identity", service.device.as_str()),
        ("protocol", "yvex.management.v2"),
        ("display_name", service.name.as_str()),
    ];
    let address_text = if address.ip().is_unspecified() {
        String::new()
    } else {
        address.ip().to_string()
    };
    let mut record = mdns_sd::ServiceInfo::new(
        "_yvex-management._tcp.local.",
        &instance,
        &hostname,
        address_text.as_str(),
        address.port(),
        &properties[..],
    )
    .map_err(|_| "discovery_record_invalid")?;
    if address.ip().is_unspecified() {
        record = record.enable_addr_auto();
    }
    daemon
        .register(record)
        .map_err(|_| "discovery_unavailable")?;
    Ok(daemon)
}
fn serve(invocation: &Invocation<'_>, store: Store) -> Result<Output> {
    let name = invocation.value("--name").unwrap_or("YVEX");
    if !pairing::valid_name(name) {
        return Err("invalid_service_name");
    }
    let address: SocketAddr = invocation
        .value("--bind")
        .unwrap_or("127.0.0.1:18080")
        .parse()
        .map_err(|_| "invalid_management_bind")?;
    let identity = store.identity()?;
    let device = identity.device();
    let config = Arc::new(tls_config(identity)?);
    let service = Arc::new(Service {
        store,
        device,
        name: name.into(),
    });
    let local = local::bind()?;
    let listener = TcpListener::bind(address).map_err(|_| "management_bind_unavailable")?;
    listener
        .set_nonblocking(true)
        .map_err(|_| "management_bind_unavailable")?;
    let address = listener
        .local_addr()
        .map_err(|_| "management_bind_unavailable")?;
    let mdns = if invocation.has("--advertise") {
        Some(advertise(address, &service)?)
    } else {
        None
    };
    let stop = Arc::new(AtomicBool::new(false));
    signal_hook::flag::register(signal_hook::consts::SIGINT, stop.clone())
        .map_err(|_| "signal_registration_failed")?;
    signal_hook::flag::register(signal_hook::consts::SIGTERM, stop.clone())
        .map_err(|_| "signal_registration_failed")?;
    // Entrypoint output contains only public service identity, never request payloads.
    println!(
        "{}",
        json!({"schema":"yvex.management.service.v1","address":address.to_string(),
        "device_identity":service.device,"local_socket":local::path()?.display().to_string(),
        "discovery_advertised":mdns.is_some()})
    );
    let _ = io::stdout().flush();
    let mut workers: Vec<thread::JoinHandle<()>> = Vec::new();
    while !stop.load(Ordering::Relaxed) {
        let mut index = 0;
        while index < workers.len() {
            if workers[index].is_finished() {
                let _ = workers.swap_remove(index).join();
            } else {
                index += 1;
            }
        }
        if let Ok((socket, _)) = listener.accept()
            && workers.len() < CONNECTION_CAP
        {
            let config = config.clone();
            let service = service.clone();
            workers.push(thread::spawn(move || tcp_client(socket, config, service)));
        }
        if let Ok((socket, _)) = local.listener.accept()
            && workers.len() < CONNECTION_CAP
        {
            let service = service.clone();
            workers.push(thread::spawn(move || local_client(socket, service)));
        }
        thread::sleep(Duration::from_millis(10));
    }
    if let Some(daemon) = mdns {
        let _ = daemon.shutdown();
    }
    Ok(Output::standard(String::new(), 0))
}
pub(crate) fn dispatch(invocation: &Invocation<'_>) -> Result<Output> {
    let store = Store::open(invocation.value("--state-dir"))?;
    // Initialize identity before any approval ledger can be created.
    let _ = store.identity()?;
    let value = match invocation.operation.operation_id.as_str() {
        "management.network.serve" => return serve(invocation, store),
        "management.network.identity" => {
            let identity = store.identity()?;
            json!({"schema":"yvex.management.identity.v1","device_identity":identity.device(),
                "display_name":"YVEX","protocol":"yvex.management.v2",
                "pairing_available":store.available()?})
        }
        "management.owner.invite" => store.owner_invite(
            invocation
                .value("--endpoint")
                .ok_or("owner_endpoint_required")?,
            invocation
                .value("--output")
                .ok_or("invitation_output_required")?,
        )?,
        "management.owner.revoke" => store.revoke_owner(&invocation.positionals[0])?,
        "management.pairing.open" => store.open_window()?,
        "management.pairing.list" => store.list()?,
        "management.pairing.approve" => store.decide(&invocation.positionals[0], true)?,
        "management.pairing.revoke" => store.decide(&invocation.positionals[0], false)?,
        _ => return Err("unsupported_network_operation"),
    };
    Ok(Output::standard(format!("{value}\n"), 0))
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn framing_does_not_accept_ambiguous_or_browser_requests() {
        for extra in [
            "Authorization: a\r\nAuthorization: b\r\n",
            "Origin: http://example\r\n",
            "Transfer-Encoding: chunked\r\n",
            "Content-Length: 0\r\nContent-Length: 0\r\n",
            "Host: duplicate\r\n",
            "Expect: 100-continue\r\n",
        ] {
            let bytes = format!("GET /v1/identity HTTP/1.1\r\nHost: localhost\r\n{extra}\r\n");
            assert!(read_request(&mut bytes.as_bytes()).is_err());
        }
        assert!(
            read_request(&mut b"GET /v1/identity HTTP/1.1\r\nHost: localhost\r\n\r\n".as_slice())
                .is_ok()
        );
        let oversized = concat!(
            "POST /v1/management HTTP/1.1\r\nHost: x\r\n",
            "Content-Length: 9999999\r\nContent-Type: application/json\r\n\r\n"
        );
        assert!(read_request(&mut oversized.as_bytes()).is_err());
    }
}
