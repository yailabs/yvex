//! Same-machine public management companion. OS peer identity is the authority;
//! localhost TCP is never an implicit trust path.
use super::{network, Error, MAX_RESPONSE_BYTES};
use std::{
    path::{Path, PathBuf},
    sync::atomic::AtomicBool,
    time::{Duration, Instant},
};

#[derive(Clone, Debug)]
pub struct LocalConnection {
    path: PathBuf,
    device_identity: String,
    peer_identity: String,
}
impl LocalConnection {
    pub fn device_identity(&self) -> &str {
        &self.device_identity
    }
    pub fn peer_identity(&self) -> &str {
        &self.peer_identity
    }
    pub fn endpoint(&self) -> String {
        format!("unix:{}", self.path.display())
    }
    /// Only a public companion at the canonical protected path may be detected.
    pub fn detect(
        timeout: Duration,
    ) -> Result<Option<(Self, network::IdentityObservation)>, Error> {
        let path = canonical_path()?;
        if !path
            .try_exists()
            .map_err(|_| Error::local("local_service_unavailable"))?
        {
            return Ok(None);
        }
        Self::observe(path, timeout).map(Some)
    }
    pub(super) fn restore(
        endpoint: &str,
        expected: &str,
        timeout: Duration,
    ) -> Result<Self, Error> {
        let path = canonical_path()?;
        if endpoint != format!("unix:{}", path.display()) {
            return Err(Error::local("invalid_local_endpoint"));
        }
        let (connection, _) = Self::observe(path, timeout)?;
        if connection.device_identity != expected {
            return Err(Error::local("local_identity_changed"));
        }
        Ok(connection)
    }
    fn observe(
        path: PathBuf,
        timeout: Duration,
    ) -> Result<(Self, network::IdentityObservation), Error> {
        let response = exchange(
            &path,
            "GET",
            "/v1/identity",
            &[],
            16384,
            None,
            timeout,
            &AtomicBool::new(false),
        )?;
        response.success()?;
        let raw: network::IdentityObservation = serde_json::from_slice(&response.body)
            .map_err(|_| Error::local("invalid_host_identity"))?;
        let identity = network::identity(&response.body, &raw.device_identity)?;
        let connection = Self {
            path,
            device_identity: identity.device_identity.clone(),
            peer_identity: format!("local-user:{}", uid()?),
        };
        Ok((connection, identity))
    }
    pub(super) fn invoke(
        &self,
        body: &[u8],
        timeout: Duration,
        cancel: &AtomicBool,
    ) -> Result<Vec<u8>, Error> {
        // The producer checks the retained exact service identity on the same
        // connection before mutation admission; envelope identity is checked too.
        let response = exchange(
            &self.path,
            "POST",
            "/v1/management",
            body,
            MAX_RESPONSE_BYTES,
            Some(&self.device_identity),
            timeout,
            cancel,
        )?;
        response.success()?;
        Ok(response.body)
    }
}
#[cfg(unix)]
fn uid() -> Result<u32, Error> {
    Ok(unsafe { libc::geteuid() })
}
#[cfg(not(unix))]
fn uid() -> Result<u32, Error> {
    Err(Error::local("local_transport_unsupported"))
}
#[cfg(unix)]
fn protected(path: &Path, socket: bool) -> Result<(), Error> {
    use std::os::unix::fs::{FileTypeExt, MetadataExt};
    let m =
        std::fs::symlink_metadata(path).map_err(|_| Error::local("local_service_unavailable"))?;
    if m.uid() != uid()?
        || m.file_type().is_symlink()
        || m.mode() & 0o777 != if socket { 0o600 } else { 0o700 }
        || if socket {
            !m.file_type().is_socket()
        } else {
            !m.is_dir()
        }
    {
        return Err(Error::local("untrusted_local_path"));
    }
    Ok(())
}
#[cfg(unix)]
fn canonical_path() -> Result<PathBuf, Error> {
    let root = std::env::var_os("XDG_RUNTIME_DIR")
        .map(PathBuf::from)
        .filter(|p| p.is_absolute() && protected(p, false).is_ok());
    Ok(root
        .map(|p| p.join("yvex"))
        .unwrap_or_else(|| PathBuf::from(format!("/tmp/yvex-{}", unsafe { libc::geteuid() })))
        .join("product-management.sock"))
}
#[cfg(not(unix))]
fn canonical_path() -> Result<PathBuf, Error> {
    Err(Error::local("local_transport_unsupported"))
}
#[cfg(unix)]
fn peer(stream: &std::os::unix::net::UnixStream) -> Result<(), Error> {
    use std::os::fd::AsRawFd;
    #[cfg(target_os = "linux")]
    let same = {
        let mut cred: libc::ucred = unsafe { std::mem::zeroed() };
        let mut size = std::mem::size_of::<libc::ucred>() as libc::socklen_t;
        unsafe {
            libc::getsockopt(
                stream.as_raw_fd(),
                libc::SOL_SOCKET,
                libc::SO_PEERCRED,
                (&mut cred as *mut libc::ucred).cast(),
                &mut size,
            ) == 0
                && cred.uid == libc::geteuid()
        }
    };
    #[cfg(any(target_os = "macos", target_os = "freebsd"))]
    let same = {
        let (mut user, mut group) = (0, 0);
        unsafe {
            libc::getpeereid(stream.as_raw_fd(), &mut user, &mut group) == 0
                && user == libc::geteuid()
        }
    };
    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "freebsd")))]
    let same = false;
    if same {
        Ok(())
    } else {
        Err(Error::local("local_peer_identity_mismatch"))
    }
}
#[cfg(unix)]
fn exchange(
    path: &Path,
    method: &str,
    route: &str,
    body: &[u8],
    limit: usize,
    identity: Option<&str>,
    timeout: Duration,
    cancel: &AtomicBool,
) -> Result<network::HttpResponse, Error> {
    use std::{
        io::{Read, Write},
        os::unix::net::UnixStream,
    };
    if timeout.is_zero() || timeout > Duration::from_secs(120) {
        return Err(Error::local("invalid_timeout"));
    }
    let deadline = Instant::now() + timeout;
    network::guard(deadline, cancel, false)?;
    protected(
        path.parent()
            .ok_or_else(|| Error::local("untrusted_local_path"))?,
        false,
    )?;
    protected(path, true)?;
    let mut stream =
        UnixStream::connect(path).map_err(|_| Error::local("local_service_unavailable"))?;
    peer(&stream)?;
    stream
        .set_read_timeout(Some(Duration::from_millis(200)))
        .map_err(|_| Error::local("local_service_unavailable"))?;
    stream
        .set_write_timeout(Some(Duration::from_millis(200)))
        .map_err(|_| Error::local("local_service_unavailable"))?;
    let mut request=format!("{method} {route} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: {}\r\n",body.len()).into_bytes();
    if let Some(identity) = identity {
        network::pin(identity)?;
        request.extend_from_slice(format!("X-Yvex-Device-Identity: {identity}\r\n").as_bytes());
    }
    request.extend_from_slice(b"\r\n");
    request.extend_from_slice(body);
    let mut sent = 0;
    while sent < request.len() {
        network::guard(deadline, cancel, sent > 0)?;
        match stream.write(&request[sent..]) {
            Ok(0) => return Err(Error::uncertain("transport_unavailable")),
            Ok(n) => sent += n,
            Err(e) if network::interrupted(&e) => {}
            Err(_) => return Err(Error::uncertain("transport_unavailable")),
        }
    }
    let mut bytes = Vec::new();
    let mut header = None;
    loop {
        network::guard(deadline, cancel, true)?;
        let mut buf = [0; 8192];
        match stream.read(&mut buf) {
            Ok(0) => return Err(Error::uncertain("incomplete_http_response")),
            Ok(n) => bytes.extend_from_slice(&buf[..n]),
            Err(e) if network::interrupted(&e) => continue,
            Err(_) => return Err(Error::uncertain("transport_unavailable")),
        }
        if header.is_none() {
            header = network::parse_http(&bytes, limit)?;
        }
        if let Some((offset, length, status)) = header {
            if bytes.len() > offset + length {
                return Err(Error::uncertain("invalid_http_framing"));
            }
            if bytes.len() == offset + length {
                return Ok(network::HttpResponse {
                    status,
                    body: bytes.split_off(offset),
                    certificate_sha256: String::new(),
                });
            }
        }
    }
}
#[cfg(not(unix))]
fn exchange(
    _: &Path,
    _: &str,
    _: &str,
    _: &[u8],
    _: usize,
    _: Option<&str>,
    _: Duration,
    _: &AtomicBool,
) -> Result<network::HttpResponse, Error> {
    Err(Error::local("local_transport_unsupported"))
}
