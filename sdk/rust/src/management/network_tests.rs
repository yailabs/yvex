use super::*;
use std::{net::TcpListener, thread};
fn fixture(
    handler: impl FnOnce(String, Vec<u8>) -> (u16, Vec<u8>) + Send + 'static,
) -> (String, String, thread::JoinHandle<()>) {
    let certificate = rcgen::generate_simple_self_signed(vec!["localhost".into()]).unwrap();
    let der = certificate.cert.der().clone();
    let fingerprint = format!("tls-sha256:{}", hex(&Sha256::digest(der.as_ref())));
    let key = rustls::pki_types::PrivatePkcs8KeyDer::from(certificate.key_pair.serialize_der());
    let config = rustls::ServerConfig::builder_with_provider(Arc::new(
        rustls::crypto::ring::default_provider(),
    ))
    .with_safe_default_protocol_versions()
    .unwrap()
    .with_no_client_auth()
    .with_single_cert(vec![der], key.into())
    .unwrap();
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap();
    let task = thread::spawn(move || {
        let (mut socket, _) = listener.accept().unwrap();
        socket
            .set_read_timeout(Some(Duration::from_secs(3)))
            .unwrap();
        let mut session = rustls::ServerConnection::new(Arc::new(config)).unwrap();
        let mut stream = rustls::Stream::new(&mut session, &mut socket);
        let mut bytes = Vec::new();
        let (header, length) = loop {
            let mut byte = [0u8; 1];
            if stream.read_exact(&mut byte).is_err() {
                return;
            }
            bytes.push(byte[0]);
            if bytes.ends_with(b"\r\n\r\n") {
                let header = String::from_utf8(bytes.clone()).unwrap();
                let length = header
                    .lines()
                    .find_map(|line| line.strip_prefix("Content-Length: "))
                    .unwrap()
                    .parse::<usize>()
                    .unwrap();
                break (header, length);
            }
            assert!(bytes.len() < 32768);
        };
        let mut body = vec![0; length];
        stream.read_exact(&mut body).unwrap();
        let (status, body) = handler(header, body);
        let response = format!(
            "HTTP/1.1 {status} Test\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
            body.len()
        );
        let _ = stream
            .write_all(response.as_bytes())
            .and_then(|_| stream.write_all(&body));
        let _ = stream.flush();
    });
    (format!("https://{address}"), fingerprint, task)
}
#[test]
fn pins_are_checked_before_bearer_dispatch() {
    let (endpoint, _, server) = fixture(|_, _| panic!("must not dispatch on wrong TLS identity"));
    let connection = HttpsConnection::new(
        &endpoint,
        &format!("tls-sha256:{}", "0".repeat(64)),
        Credential::from_bytes([7; 32]),
    )
    .unwrap();
    let error = connection
        .pairing_status(Duration::from_secs(2))
        .unwrap_err();
    assert_eq!(
        error.dispatch_state,
        super::super::DispatchState::NotDispatched
    );
    assert_eq!(error.code, "tls_identity_unavailable");
    server.join().unwrap();
}
#[test]
fn pinned_tls_preserves_bounded_management_body_and_secret_redaction() {
    let secret = Credential::from_bytes([7; 32]);
    let expected = secret.bearer().to_string();
    let (endpoint, pin, server) = fixture(move |header, body| {
        assert!(header.starts_with("POST /v1/management "));
        assert!(header.contains(&format!("Authorization: Bearer {expected}\r\n")));
        assert_eq!(body, b"retained-request\n");
        (200, b"producer-response\n".to_vec())
    });
    let c = HttpsConnection::new(&endpoint, &pin, secret).unwrap();
    assert!(!format!("{c:?}").contains(&"07".repeat(32)));
    assert_eq!(
        c.invoke(
            b"retained-request\n",
            Duration::from_secs(2),
            &AtomicBool::new(false)
        )
        .unwrap(),
        b"producer-response\n"
    );
    server.join().unwrap();
}
#[test]
fn redirects_and_large_payloads_are_never_followed() {
    for (status, body, code) in [
        (302, Vec::new(), "redirect_refused"),
        (200, vec![b'x'; 16385], "response_too_large"),
    ] {
        let (endpoint, pin, server) = fixture(move |_, _| (status, body));
        let c = HttpsConnection::new(&endpoint, &pin, Credential::from_bytes([8; 32])).unwrap();
        assert_eq!(
            c.pairing_status(Duration::from_secs(2)).unwrap_err().code,
            code
        );
        server.join().unwrap();
    }
}
#[test]
fn revocation_is_owner_observation_not_network_inference() {
    let secret = Credential::from_bytes([9; 32]);
    let id = secret.hash();
    let (endpoint, pin, server) = fixture(move |_, _| {
        (200,serde_json::to_vec(&serde_json::json!({"schema":PAIRING_SCHEMA,"request_id":id,"posture":"revoked","scope":"product-management","expires_at_unix_ms":1,"reason":"operator_revoked"})).unwrap())
    });
    let c = HttpsConnection::new(&endpoint, &pin, secret).unwrap();
    assert_eq!(
        c.pairing_status(Duration::from_secs(2)).unwrap().posture,
        PairingPosture::Revoked
    );
    server.join().unwrap();
    let error = c.pairing_status(Duration::from_millis(300)).unwrap_err();
    assert_eq!(error.code, "transport_unavailable");
}
#[test]
fn http_framing_refuses_ambiguous_or_unbounded_responses() {
    for response in [
        b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n".as_slice(),
        b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n",
        b"HTTP/1.1 200 OK\r\n\r\n",
    ] {
        assert!(parse_http(response, 32).is_err());
    }
}
