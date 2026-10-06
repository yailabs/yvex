//! Pinned HTTPS management and untrusted first-contact observation.
//! Authority: producer docs/contracts/network-management.md.
use super::{valid_id, Error, MAX_RESPONSE_BYTES};
use rustls::{
    client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier},
    pki_types::{CertificateDer, ServerName, UnixTime},
    ClientConfig, ClientConnection, DigitallySignedStruct, SignatureScheme,
};
use sha2::{Digest, Sha256};
use std::{
    io::{Read, Write},
    net::{TcpStream, ToSocketAddrs},
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
    time::{Duration, Instant},
};
use zeroize::Zeroizing;

pub const IDENTITY_SCHEMA: &str = "yvex.management.identity.v1";
pub const PAIRING_SCHEMA: &str = "yvex.management.pairing.v1";
pub use super::network_types::{
    HostCandidate, IdentityObservation, PairingObservation, PairingPosture,
};
/// Native-only secret, deliberately neither Clone nor Serialize.
pub struct Credential(Zeroizing<[u8; 32]>);
impl Credential {
    pub fn generate() -> Result<Self, Error> {
        let mut value = Zeroizing::new([0; 32]);
        getrandom::fill(value.as_mut()).map_err(|_| Error::local("random_unavailable"))?;
        Ok(Self(value))
    }
    pub fn from_bytes(bytes: [u8; 32]) -> Self {
        Self(Zeroizing::new(bytes))
    }
    pub fn hash(&self) -> String {
        hex(&Sha256::digest(self.0.as_ref()))
    }
    pub(super) fn bytes(&self) -> &[u8] {
        self.0.as_ref()
    }
    fn bearer(&self) -> Zeroizing<String> {
        Zeroizing::new(hex(self.0.as_ref()))
    }
}
impl std::fmt::Debug for Credential {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Credential([redacted])")
    }
}
#[derive(Debug)]
pub struct HttpsConnection {
    endpoint: Endpoint,
    pub(super) expected_device_identity: String,
    pub(super) expected_peer_identity: String,
    credential: Credential,
}
impl HttpsConnection {
    pub fn new(
        endpoint: &str,
        device_identity: &str,
        credential: Credential,
    ) -> Result<Self, Error> {
        pin(device_identity)?;
        Ok(Self {
            endpoint: Endpoint::parse(endpoint)?,
            expected_device_identity: device_identity.into(),
            expected_peer_identity: format!("credential-sha256:{}", credential.hash()),
            credential,
        })
    }
    pub(super) fn credential_bytes(&self) -> &[u8] {
        self.credential.bytes()
    }
    pub fn device_identity(&self) -> &str {
        &self.expected_device_identity
    }
    pub fn peer_identity(&self) -> &str {
        &self.expected_peer_identity
    }
    pub fn request_pairing(
        &self,
        client_name: &str,
        timeout: Duration,
    ) -> Result<PairingObservation, Error> {
        if client_name.trim().is_empty()
            || client_name.len() > 80
            || client_name.chars().any(char::is_control)
        {
            return Err(Error::local("invalid_client_name"));
        }
        let body = serde_json::to_vec(
            &serde_json::json!({"schema":"yvex.management.pairing.request.v1",
            "credential_hash":self.credential.hash(),"client_name":client_name}),
        )
        .map_err(|_| Error::local("invalid_pairing_request"))?;
        let response = exchange(
            &self.endpoint,
            Some(pin(&self.expected_device_identity)?),
            "POST",
            "/v1/pairing",
            None,
            &body,
            16384,
            timeout,
            &AtomicBool::new(false),
        )?;
        self.pairing(response)
    }
    pub fn pairing_status(&self, timeout: Duration) -> Result<PairingObservation, Error> {
        let response = exchange(
            &self.endpoint,
            Some(pin(&self.expected_device_identity)?),
            "GET",
            "/v1/pairing/status",
            Some(&self.credential),
            &[],
            16384,
            timeout,
            &AtomicBool::new(false),
        )?;
        self.pairing(response)
    }
    fn pairing(&self, response: HttpResponse) -> Result<PairingObservation, Error> {
        response.success()?;
        let value: PairingObservation = serde_json::from_slice(&response.body)
            .map_err(|_| Error::uncertain("invalid_pairing_observation"))?;
        if value.schema != PAIRING_SCHEMA
            || value.scope != "product-management"
            || value.request_id != self.credential.hash()
        {
            return Err(Error::uncertain("pairing_identity_mismatch"));
        }
        Ok(value)
    }
    pub(super) fn owner_exchange(
        &self,
        method: &str,
        path: &str,
        body: &[u8],
        authenticated: bool,
        timeout: Duration,
    ) -> Result<Vec<u8>, Error> {
        if !path.starts_with("/v1/owner/") || path.len() > 128 || body.len() > 16384 {
            return Err(Error::local("invalid_owner_request"));
        }
        let response = exchange(
            &self.endpoint,
            Some(pin(&self.expected_device_identity)?),
            method,
            path,
            if authenticated {
                Some(&self.credential)
            } else {
                None
            },
            body,
            131072,
            timeout,
            &AtomicBool::new(false),
        )?;
        if response.status == 403 {
            let error: serde_json::Value = serde_json::from_slice(&response.body)
                .map_err(|_| Error::uncertain("invalid_owner_refusal"))?;
            let reason = error["reason"].as_str().filter(|reason| {
                !reason.is_empty()
                    && reason.len() <= 128
                    && reason.bytes().all(|b| b.is_ascii_lowercase() || b == b'_')
            });
            if error["schema"] != "yvex.management.error.v1" || reason.is_none() {
                return Err(Error::uncertain("invalid_owner_refusal"));
            }
            return Err(Error {
                code: "owner_request_refused".into(),
                dispatch_state: super::DispatchState::Completed,
                reason: reason.map(str::to_owned),
                request_id: None,
            });
        }
        response.success()?;
        Ok(response.body)
    }
    pub(super) fn invoke(
        &self,
        bytes: &[u8],
        timeout: Duration,
        cancel: &AtomicBool,
    ) -> Result<Vec<u8>, Error> {
        let response = exchange(
            &self.endpoint,
            Some(pin(&self.expected_device_identity)?),
            "POST",
            "/v1/management",
            Some(&self.credential),
            bytes,
            MAX_RESPONSE_BYTES,
            timeout,
            cancel,
        )?;
        response.success()?;
        Ok(response.body)
    }
}
#[derive(Clone, Debug)]
struct Endpoint {
    origin: String,
    host: String,
    authority: String,
    port: u16,
}
impl Endpoint {
    fn parse(input: &str) -> Result<Self, Error> {
        let url = url::Url::parse(input).map_err(|_| Error::local("invalid_https_endpoint"))?;
        if url.scheme() != "https"
            || !url.username().is_empty()
            || url.password().is_some()
            || url.query().is_some()
            || url.fragment().is_some()
            || url.path() != "/"
        {
            return Err(Error::local("invalid_https_endpoint"));
        }
        let host = url
            .host_str()
            .ok_or_else(|| Error::local("invalid_https_endpoint"))?
            .trim_matches(['[', ']'])
            .to_string();
        let port = url
            .port_or_known_default()
            .filter(|port| *port != 0)
            .ok_or_else(|| Error::local("invalid_https_endpoint"))?;
        let authority = if host.contains(':') {
            format!("[{host}]:{port}")
        } else {
            format!("{host}:{port}")
        };
        Ok(Self {
            origin: url.origin().ascii_serialization(),
            host,
            authority,
            port,
        })
    }
}
pub(super) fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}
pub(super) fn pin(identity: &str) -> Result<&str, Error> {
    identity
        .strip_prefix("tls-sha256:")
        .filter(|v| valid_id(v))
        .ok_or_else(|| Error::local("invalid_tls_identity"))
}
/// Observe without a credential. The caller must verify the fingerprint out of
/// band before explicitly trusting this candidate. It is not a management grant.
pub fn probe(endpoint: &str, timeout: Duration) -> Result<HostCandidate, Error> {
    let endpoint = Endpoint::parse(endpoint)?;
    let response = exchange(
        &endpoint,
        None,
        "GET",
        "/v1/identity",
        None,
        &[],
        16384,
        timeout,
        &AtomicBool::new(false),
    )?;
    response.success()?;
    let identity = identity(
        &response.body,
        &format!("tls-sha256:{}", response.certificate_sha256),
    )?;
    Ok(HostCandidate {
        endpoint: endpoint.origin,
        identity,
        certificate_sha256: response.certificate_sha256,
    })
}
pub(super) fn identity(bytes: &[u8], expected: &str) -> Result<IdentityObservation, Error> {
    let identity: IdentityObservation =
        serde_json::from_slice(bytes).map_err(|_| Error::local("invalid_host_identity"))?;
    if identity.schema != IDENTITY_SCHEMA
        || identity.protocol != "yvex.management.v2"
        || identity.device_identity != expected
        || identity.display_name.trim().is_empty()
        || identity.display_name.len() > 256
        || identity.display_name.chars().any(char::is_control)
    {
        return Err(Error::local("host_identity_mismatch"));
    }
    pin(&identity.device_identity)?;
    Ok(identity)
}
#[derive(Debug)]
struct Verifier {
    expected: Option<String>,
    observed: Arc<Mutex<Option<String>>>,
    algorithms: rustls::crypto::WebPkiSupportedAlgorithms,
}
impl ServerCertVerifier for Verifier {
    fn verify_server_cert(
        &self,
        cert: &CertificateDer<'_>,
        _: &[CertificateDer<'_>],
        _: &ServerName<'_>,
        _: &[u8],
        _: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        if cert.len() > 65536 {
            return Err(rustls::Error::General("certificate_bound".into()));
        }
        let digest = hex(&Sha256::digest(cert.as_ref()));
        if self
            .expected
            .as_ref()
            .is_some_and(|expected| *expected != digest)
        {
            return Err(rustls::Error::General("pinned_identity_mismatch".into()));
        }
        *self
            .observed
            .lock()
            .map_err(|_| rustls::Error::General("observation_lock".into()))? = Some(digest);
        // Exact DER is the explicitly approved trust anchor; cryptographic
        // signature verification still happens below, including during discovery.
        Ok(ServerCertVerified::assertion())
    }
    fn verify_tls12_signature(
        &self,
        msg: &[u8],
        cert: &CertificateDer<'_>,
        sig: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls12_signature(msg, cert, sig, &self.algorithms)
    }
    fn verify_tls13_signature(
        &self,
        msg: &[u8],
        cert: &CertificateDer<'_>,
        sig: &DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls13_signature(msg, cert, sig, &self.algorithms)
    }
    fn supported_verify_schemes(&self) -> Vec<SignatureScheme> {
        self.algorithms.supported_schemes()
    }
}
pub(super) struct HttpResponse {
    pub status: u16,
    pub body: Vec<u8>,
    pub(super) certificate_sha256: String,
}
impl HttpResponse {
    pub(super) fn success(&self) -> Result<(), Error> {
        match self.status {
            200..=299 => Ok(()),
            300..=399 => Err(Error::uncertain("redirect_refused")),
            401 | 403 => Err(Error::local("management_not_authorized")),
            _ => Err(Error::uncertain("https_service_unavailable")),
        }
    }
}
pub(super) fn interrupted(error: &std::io::Error) -> bool {
    matches!(
        error.kind(),
        std::io::ErrorKind::WouldBlock
            | std::io::ErrorKind::TimedOut
            | std::io::ErrorKind::Interrupted
    )
}
pub(super) fn guard(deadline: Instant, cancel: &AtomicBool, dispatched: bool) -> Result<(), Error> {
    let code = if cancel.load(Ordering::Relaxed) {
        Some("transport_cancelled")
    } else if Instant::now() >= deadline {
        Some("timeout")
    } else {
        None
    };
    if let Some(code) = code {
        return Err(if dispatched {
            Error::uncertain(code)
        } else {
            Error::local(code)
        });
    }
    Ok(())
}
/// One Content-Length frame only, shared by TLS and same-user local transport.
pub(super) fn parse_http(bytes: &[u8], limit: usize) -> Result<Option<(usize, usize, u16)>, Error> {
    let mut headers = [httparse::EMPTY_HEADER; 48];
    let mut r = httparse::Response::new(&mut headers);
    let offset = match r
        .parse(bytes)
        .map_err(|_| Error::uncertain("invalid_http_response"))?
    {
        httparse::Status::Partial => {
            if bytes.len() > 32768 {
                return Err(Error::uncertain("response_headers_too_large"));
            }
            return Ok(None);
        }
        httparse::Status::Complete(offset) => offset,
    };
    if offset > 32768 {
        return Err(Error::uncertain("response_headers_too_large"));
    }
    let mut lengths = r
        .headers
        .iter()
        .filter(|h| h.name.eq_ignore_ascii_case("content-length"));
    let length = lengths
        .next()
        .and_then(|h| std::str::from_utf8(h.value).ok())
        .and_then(|v| v.parse::<usize>().ok())
        .ok_or_else(|| Error::uncertain("unbounded_http_response"))?;
    if lengths.next().is_some()
        || r.headers.iter().any(|h| {
            h.name.eq_ignore_ascii_case("transfer-encoding")
                || h.name.eq_ignore_ascii_case("content-encoding")
        })
    {
        return Err(Error::uncertain("unsupported_http_framing"));
    }
    if length > limit {
        return Err(Error::uncertain("response_too_large"));
    }
    Ok(Some((
        offset,
        length,
        r.code
            .ok_or_else(|| Error::uncertain("invalid_http_response"))?,
    )))
}
fn exchange(
    endpoint: &Endpoint,
    expected: Option<&str>,
    method: &str,
    path: &str,
    credential: Option<&Credential>,
    body: &[u8],
    limit: usize,
    timeout: Duration,
    cancel: &AtomicBool,
) -> Result<HttpResponse, Error> {
    if timeout.is_zero() || timeout > Duration::from_secs(120) {
        return Err(Error::local("invalid_timeout"));
    }
    let deadline = Instant::now() + timeout;
    guard(deadline, cancel, false)?;
    let addresses = resolve(&endpoint.host, endpoint.port, deadline, cancel)?;
    let mut socket = None;
    for address in addresses.into_iter().take(16) {
        guard(deadline, cancel, false)?;
        if let Ok(candidate) = TcpStream::connect_timeout(
            &address,
            deadline
                .saturating_duration_since(Instant::now())
                .min(Duration::from_secs(2)),
        ) {
            socket = Some(candidate);
            break;
        }
    }
    let mut socket = socket.ok_or_else(|| Error::local("transport_unavailable"))?;
    socket
        .set_read_timeout(Some(Duration::from_millis(200)))
        .map_err(|_| Error::local("transport_unavailable"))?;
    socket
        .set_write_timeout(Some(Duration::from_millis(200)))
        .map_err(|_| Error::local("transport_unavailable"))?;
    let observed = Arc::new(Mutex::new(None));
    let provider = rustls::crypto::ring::default_provider();
    let verifier = Verifier {
        expected: expected.map(str::to_string),
        observed: observed.clone(),
        algorithms: provider.signature_verification_algorithms,
    };
    let mut config = ClientConfig::builder_with_provider(Arc::new(provider))
        .with_safe_default_protocol_versions()
        .map_err(|_| Error::local("tls_configuration_unavailable"))?
        .dangerous()
        .with_custom_certificate_verifier(Arc::new(verifier))
        .with_no_client_auth();
    config.alpn_protocols = vec![b"http/1.1".to_vec()];
    config.enable_early_data = false;
    let name = ServerName::try_from(endpoint.host.clone())
        .map_err(|_| Error::local("invalid_https_endpoint"))?;
    let mut tls = ClientConnection::new(Arc::new(config), name)
        .map_err(|_| Error::local("tls_configuration_unavailable"))?;
    while tls.is_handshaking() {
        guard(deadline, cancel, false)?;
        match tls.complete_io(&mut socket) {
            Ok(_) => {}
            Err(e) if interrupted(&e) => {}
            Err(_) => return Err(Error::local("tls_identity_unavailable")),
        }
    }
    let certificate_sha256 = observed
        .lock()
        .ok()
        .and_then(|v| v.clone())
        .ok_or_else(|| Error::local("tls_identity_unavailable"))?;
    let mut request=Zeroizing::new(format!("{method} {path} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\nAccept: application/json\r\nContent-Type: application/json\r\nContent-Length: {}\r\n",endpoint.authority,body.len()));
    if let Some(secret) = credential {
        request.push_str("Authorization: Bearer ");
        request.push_str(&secret.bearer());
        request.push_str("\r\n");
    }
    request.push_str("\r\n");
    tls.writer()
        .write_all(request.as_bytes())
        .and_then(|_| tls.writer().write_all(body))
        .map_err(|_| Error::local("request_encoding_failed"))?;
    while tls.wants_write() {
        guard(deadline, cancel, true)?;
        match tls.write_tls(&mut socket) {
            Ok(0) => return Err(Error::uncertain("transport_unavailable")),
            Ok(_) => {}
            Err(e) if interrupted(&e) => {}
            Err(_) => return Err(Error::uncertain("transport_unavailable")),
        }
    }
    let mut bytes = Vec::new();
    let mut header = None;
    loop {
        guard(deadline, cancel, true)?;
        let mut buffer = [0u8; 8192];
        match tls.reader().read(&mut buffer) {
            Ok(0) => return Err(Error::uncertain("incomplete_http_response")),
            Ok(n) => bytes.extend_from_slice(&buffer[..n]),
            Err(e) if interrupted(&e) => {
                match tls.read_tls(&mut socket) {
                    Ok(0) => return Err(Error::uncertain("incomplete_http_response")),
                    Ok(_) => {
                        tls.process_new_packets()
                            .map_err(|_| Error::uncertain("invalid_tls_response"))?;
                    }
                    Err(e) if interrupted(&e) => {}
                    Err(_) => return Err(Error::uncertain("transport_unavailable")),
                }
                continue;
            }
            Err(_) => return Err(Error::uncertain("invalid_tls_response")),
        }
        if header.is_none() {
            header = parse_http(&bytes, limit)?;
        }
        if let Some((offset, length, status)) = header {
            if bytes.len() > offset + length {
                return Err(Error::uncertain("invalid_http_framing"));
            }
            if bytes.len() == offset + length {
                return Ok(HttpResponse {
                    status,
                    body: bytes.split_off(offset),
                    certificate_sha256,
                });
            }
        }
    }
}

// The system resolver is blocking on some platforms. Bound outstanding resolver
// workers so a stalled OS resolver never stalls the UI or creates unbounded work.
fn resolve(
    host: &str,
    port: u16,
    deadline: Instant,
    cancel: &AtomicBool,
) -> Result<Vec<std::net::SocketAddr>, Error> {
    use std::sync::{atomic::AtomicUsize, mpsc};
    static ACTIVE: AtomicUsize = AtomicUsize::new(0);
    if let Ok(ip) = host.parse::<std::net::IpAddr>() {
        return Ok(vec![std::net::SocketAddr::new(ip, port)]);
    }
    ACTIVE
        .fetch_update(Ordering::AcqRel, Ordering::Acquire, |n| {
            (n < 4).then_some(n + 1)
        })
        .map_err(|_| Error::local("name_resolution_busy"))?;
    let name = host.to_string();
    let (tx, rx) = mpsc::sync_channel(1);
    let spawned = std::thread::Builder::new()
        .name("yvex-name-resolution".into())
        .spawn(move || {
            let result = (name.as_str(), port)
                .to_socket_addrs()
                .map(|a| a.take(16).collect::<Vec<_>>());
            ACTIVE.fetch_sub(1, Ordering::AcqRel);
            let _ = tx.send(result);
        });
    if spawned.is_err() {
        ACTIVE.fetch_sub(1, Ordering::AcqRel);
        return Err(Error::local("name_resolution_unavailable"));
    }
    loop {
        guard(deadline, cancel, false)?;
        match rx.recv_timeout(Duration::from_millis(100)) {
            Ok(Ok(addresses)) => return Ok(addresses),
            Ok(Err(_)) | Err(mpsc::RecvTimeoutError::Disconnected) => {
                return Err(Error::local("name_resolution_unavailable"))
            }
            Err(mpsc::RecvTimeoutError::Timeout) => {}
        }
    }
}

#[cfg(test)]
#[path = "network_tests.rs"]
mod tests;
