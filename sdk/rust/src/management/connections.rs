//! Shared native client profiles. Public metadata is separate from credentials.
//! There is no plaintext credential fallback and no implicit remote trust.
pub mod ownership;
use super::{
    local::LocalConnection,
    network::{
        self, Credential, HostCandidate, HttpsConnection, PairingObservation, PairingPosture,
    },
    valid_id, Client, Error,
};
pub use ownership::*;
use sha2::{Digest, Sha256};
use std::{
    fs,
    io::Write,
    path::{Path, PathBuf},
    sync::Arc,
    time::Duration,
};
use zeroize::Zeroizing;
#[cfg(feature = "native-credentials")]
const SERVICE: &str = "org.yailabs.yvex.management.v1";
pub use super::network_types::{ConnectionPosture, ConnectionProfile, ConnectionTransport};
impl From<PairingPosture> for ConnectionPosture {
    fn from(p: PairingPosture) -> Self {
        match p {
            PairingPosture::Pending => Self::Pending,
            PairingPosture::Approved => Self::Approved,
            PairingPosture::Revoked => Self::Revoked,
            PairingPosture::Expired => Self::Expired,
            PairingPosture::Refused => Self::Refused,
        }
    }
}
/// Native integrations may inject their qualified credential owner. Implementors
/// must not log values. Tests use an explicit isolated in-memory implementation.
pub trait CredentialStore: Send + Sync {
    fn put(&self, key: &str, value: &[u8]) -> Result<(), Error>;
    fn get(&self, key: &str) -> Result<Zeroizing<Vec<u8>>, Error>;
    fn delete(&self, key: &str) -> Result<(), Error>;
}
#[cfg(feature = "native-credentials")]
struct NativeStore;
#[cfg(feature = "native-credentials")]
impl CredentialStore for NativeStore {
    fn put(&self, key: &str, value: &[u8]) -> Result<(), Error> {
        keyring::Entry::new(SERVICE, key)
            .and_then(|e| e.set_secret(value))
            .map_err(|_| Error::local("credential_store_unavailable"))
    }
    fn get(&self, key: &str) -> Result<Zeroizing<Vec<u8>>, Error> {
        keyring::Entry::new(SERVICE, key)
            .and_then(|e| e.get_secret())
            .map(Zeroizing::new)
            .map_err(|_| Error::local("credential_store_unavailable"))
    }
    fn delete(&self, key: &str) -> Result<(), Error> {
        match keyring::Entry::new(SERVICE, key).and_then(|e| e.delete_credential()) {
            Ok(()) | Err(keyring::Error::NoEntry) => Ok(()),
            Err(_) => Err(Error::local("credential_store_unavailable")),
        }
    }
}
pub struct ConnectionManager {
    root: PathBuf,
    store: Arc<dyn CredentialStore>,
    timeout: Duration,
}
impl ConnectionManager {
    #[cfg(feature = "native-credentials")]
    pub fn native() -> Result<Self, Error> {
        let root = if let Some(value) = std::env::var_os("XDG_CONFIG_HOME")
            .map(PathBuf::from)
            .filter(|p| p.is_absolute())
        {
            value
        } else {
            PathBuf::from(
                std::env::var_os("HOME")
                    .ok_or_else(|| Error::local("profile_storage_unavailable"))?,
            )
            .join(".config")
        };
        Self::native_at(root.join("yvex/management-connections.v1"))
    }
    #[cfg(feature = "native-credentials")]
    pub fn native_at(root: PathBuf) -> Result<Self, Error> {
        Self::with_store(root, Arc::new(NativeStore))
    }
    #[cfg(not(feature = "native-credentials"))]
    pub fn native() -> Result<Self, Error> {
        Err(Error::local("native_credential_feature_required"))
    }
    pub fn with_store(root: PathBuf, store: Arc<dyn CredentialStore>) -> Result<Self, Error> {
        if !root.is_absolute() {
            return Err(Error::local("invalid_profile_root"));
        }
        private_dir(&root)?;
        Ok(Self {
            root,
            store,
            timeout: Duration::from_secs(15),
        })
    }
    pub fn list(&self) -> Result<Vec<ConnectionProfile>, Error> {
        let _guard = self.read_lock()?;
        let mut profiles = Vec::new();
        for entry in
            fs::read_dir(&self.root).map_err(|_| Error::local("profile_storage_unavailable"))?
        {
            let entry = entry.map_err(|_| Error::local("profile_storage_unavailable"))?;
            let name = entry.file_name();
            let Some(name) = name.to_str() else { continue };
            if let Some(id) = name.strip_suffix(".json").filter(|id| valid_id(id)) {
                profiles.push(self.load(id)?);
            }
            if profiles.len() > 256 {
                return Err(Error::local("profile_inventory_bound"));
            }
        }
        profiles.sort_by(|a, b| a.display_name.cmp(&b.display_name));
        Ok(profiles)
    }
    pub fn get(&self, id: &str) -> Result<ConnectionProfile, Error> {
        let _guard = self.read_lock()?;
        self.load(id)
    }
    /// An explicit caller confirmation of an out-of-band fingerprint. It stores
    /// a credential before any pairing request, enabling exact loss recovery.
    pub fn trust(
        &self,
        candidate: &HostCandidate,
        client_name: &str,
    ) -> Result<ConnectionProfile, Error> {
        if candidate.identity.device_identity
            != format!("tls-sha256:{}", candidate.certificate_sha256)
            || !valid_id(&candidate.certificate_sha256)
            || client_name.trim().is_empty()
            || client_name.len() > 80
            || client_name.chars().any(char::is_control)
        {
            return Err(Error::local("invalid_trust_candidate"));
        }
        network::identity(
            &serde_json::to_vec(&candidate.identity)
                .map_err(|_| Error::local("invalid_trust_candidate"))?,
            &candidate.identity.device_identity,
        )?;
        let secret = Credential::generate()?;
        let profile_ref = Credential::generate()?.hash();
        let connection = HttpsConnection::new(
            &candidate.endpoint,
            &candidate.identity.device_identity,
            secret,
        )?;
        let profile = ConnectionProfile {
            profile_ref: profile_ref.clone(),
            transport: ConnectionTransport::Https,
            endpoint: candidate.endpoint.clone(),
            device_identity: candidate.identity.device_identity.clone(),
            peer_identity: connection.peer_identity().into(),
            display_name: candidate.identity.display_name.clone(),
            client_name: client_name.into(),
            pairing_posture: ConnectionPosture::Unpaired,
            expires_at_unix_ms: None,
        };
        let _guard = self.lock()?;
        self.store
            .put(&profile_ref, connection.credential_bytes())?;
        if let Err(error) = self.save(&profile) {
            let _ = self.store.delete(&profile_ref);
            return Err(error);
        }
        Ok(profile)
    }
    /// Explicit operator retry creates a new pending identity. It never revives
    /// a revoked grant or resends a request with an indeterminate result.
    pub fn new_pairing(&self, id: &str) -> Result<ConnectionProfile, Error> {
        let profile = self.get(id)?;
        if profile.transport != ConnectionTransport::Https
            || !matches!(
                profile.pairing_posture,
                ConnectionPosture::Expired
                    | ConnectionPosture::Refused
                    | ConnectionPosture::Revoked
            )
        {
            return Err(Error::local("fresh_pairing_not_applicable"));
        }
        let candidate = HostCandidate {
            endpoint: profile.endpoint,
            certificate_sha256: network::pin(&profile.device_identity)?.into(),
            identity: network::IdentityObservation {
                schema: network::IDENTITY_SCHEMA.into(),
                device_identity: profile.device_identity,
                display_name: profile.display_name,
                protocol: "yvex.management.v2".into(),
                pairing_available: false,
            },
        };
        self.trust(&candidate, &profile.client_name)
    }
    pub fn request_pairing(&self, id: &str) -> Result<PairingObservation, Error> {
        let profile = self.get(id)?;
        // Once dispatched (including a lost acknowledgement), status is the
        // only automatic recovery. Repeated clicks do not post again.
        if profile.pairing_posture != ConnectionPosture::Unpaired {
            return self.pairing_status(id);
        }
        {
            let _guard = self.lock()?;
            let mut latest = self.load(id)?;
            if latest.pairing_posture != ConnectionPosture::Unpaired {
                drop(_guard);
                return self.pairing_status(id);
            }
            latest.pairing_posture = ConnectionPosture::Pending;
            self.save(&latest)?;
        }
        let observation = self
            .https(&profile)?
            .request_pairing(&profile.client_name, self.timeout)?;
        self.record(id, &observation)?;
        Ok(observation)
    }
    pub fn pairing_status(&self, id: &str) -> Result<PairingObservation, Error> {
        let profile = self.get(id)?;
        let observation = self.https(&profile)?.pairing_status(self.timeout)?;
        self.record(id, &observation)?;
        Ok(observation)
    }
    pub fn client(&self, id: &str) -> Result<Client, Error> {
        let profile = self.get(id)?;
        match profile.transport {
            ConnectionTransport::Https => {
                if profile.pairing_posture != ConnectionPosture::Approved {
                    return Err(Error::local("management_pairing_required"));
                }
                Ok(Client::https(self.https(&profile)?))
            }
            ConnectionTransport::Local => Ok(Client::local(LocalConnection::restore(
                &profile.endpoint,
                &profile.device_identity,
                self.timeout,
            )?)),
        }
    }
    /// Local forgetting is not remote revocation. The producer remains the
    /// authority for its grant and revocation controls.
    pub fn forget(&self, id: &str) -> Result<(), Error> {
        let _guard = self.lock()?;
        let profile = self.load(id)?;
        if profile.transport == ConnectionTransport::Https {
            self.store.delete(id)?;
        }
        fs::remove_file(self.path(id)?).map_err(|_| Error::local("profile_storage_unavailable"))
    }
    pub fn detect_local(&self) -> Result<Option<ConnectionProfile>, Error> {
        let Some((connection, identity)) = LocalConnection::detect(self.timeout)? else {
            return Ok(None);
        };
        let id = network::hex(&Sha256::digest(format!(
            "yvex-sdk-local-profile.v1\0{}\0{}",
            connection.device_identity(),
            connection.peer_identity()
        )));
        let profile = ConnectionProfile {
            profile_ref: id,
            transport: ConnectionTransport::Local,
            endpoint: connection.endpoint(),
            device_identity: identity.device_identity,
            peer_identity: connection.peer_identity().into(),
            display_name: identity.display_name,
            client_name: "Local client".into(),
            pairing_posture: ConnectionPosture::Approved,
            expires_at_unix_ms: None,
        };
        let _guard = self.lock()?;
        self.save(&profile)?;
        Ok(Some(profile))
    }
    fn record(&self, id: &str, observation: &PairingObservation) -> Result<(), Error> {
        let _guard = self.lock()?;
        let mut p = self.load(id)?;
        if p.peer_identity != format!("credential-sha256:{}", observation.request_id) {
            return Err(Error::local("pairing_identity_mismatch"));
        }
        p.pairing_posture = observation.posture.clone().into();
        p.expires_at_unix_ms = Some(observation.expires_at_unix_ms);
        self.save(&p)
    }
    fn https(&self, p: &ConnectionProfile) -> Result<HttpsConnection, Error> {
        if p.transport != ConnectionTransport::Https {
            return Err(Error::local("pairing_not_applicable"));
        }
        let bytes = self.store.get(&p.profile_ref)?;
        let mut fixed = Zeroizing::new([0; 32]);
        if bytes.len() != 32 {
            return Err(Error::local("credential_store_invalid"));
        }
        fixed.copy_from_slice(&bytes);
        let connection = HttpsConnection::new(
            &p.endpoint,
            &p.device_identity,
            Credential::from_bytes(*fixed),
        )?;
        if connection.peer_identity() != p.peer_identity {
            return Err(Error::local("credential_identity_mismatch"));
        }
        Ok(connection)
    }
    fn path(&self, id: &str) -> Result<PathBuf, Error> {
        if !valid_id(id) {
            return Err(Error::local("invalid_profile_ref"));
        }
        Ok(self.root.join(format!("{id}.json")))
    }
    fn load(&self, id: &str) -> Result<ConnectionProfile, Error> {
        let path = self.path(id)?;
        safe_file(&path)?;
        let bytes = read_profile(&path)?;
        if bytes.len() > 8192 {
            return Err(Error::local("invalid_connection_profile"));
        }
        let p: ConnectionProfile = serde_json::from_slice(&bytes)
            .map_err(|_| Error::local("invalid_connection_profile"))?;
        if p.profile_ref != id {
            return Err(Error::local("profile_identity_mismatch"));
        }
        network::pin(&p.device_identity)?;
        Ok(p)
    }
    fn save(&self, p: &ConnectionProfile) -> Result<(), Error> {
        let mut file = tempfile::NamedTempFile::new_in(&self.root)
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        file.write_all(
            &serde_json::to_vec(p).map_err(|_| Error::local("invalid_connection_profile"))?,
        )
        .and_then(|_| file.as_file().sync_all())
        .map_err(|_| Error::local("profile_storage_unavailable"))?;
        file.persist(self.path(&p.profile_ref)?)
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        Ok(())
    }
    fn read_lock(&self) -> Result<ProfileLock, Error> {
        ProfileLock::new(
            &self.root.join("registry.lock"),
            false,
            Duration::from_secs(1),
        )
    }
    fn lock(&self) -> Result<ProfileLock, Error> {
        ProfileLock::new(
            &self.root.join("registry.lock"),
            true,
            Duration::from_secs(1),
        )
    }
}
#[cfg(unix)]
fn private_dir(path: &Path) -> Result<(), Error> {
    use std::os::unix::fs::{DirBuilderExt, MetadataExt};
    let uid = unsafe { libc::geteuid() };
    let mut current = PathBuf::from("/");
    for component in path.components() {
        match component {
            std::path::Component::RootDir => continue,
            std::path::Component::Normal(part) => current.push(part),
            _ => return Err(Error::local("invalid_profile_root")),
        }
        match fs::symlink_metadata(&current) {
            Ok(m) => {
                let sticky_root = m.uid() == 0 && m.mode() & 0o1000 != 0;
                if !m.is_dir()
                    || m.file_type().is_symlink()
                    || (m.uid() != uid && m.uid() != 0)
                    || (m.mode() & 0o022 != 0 && !sticky_root)
                {
                    return Err(Error::local("untrusted_profile_storage"));
                }
                if current == path && (m.uid() != uid || m.mode() & 0o777 != 0o700) {
                    return Err(Error::local("untrusted_profile_storage"));
                }
            }
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
                fs::DirBuilder::new()
                    .mode(0o700)
                    .create(&current)
                    .map_err(|_| Error::local("profile_storage_unavailable"))?;
            }
            Err(_) => return Err(Error::local("profile_storage_unavailable")),
        }
    }
    Ok(())
}
#[cfg(not(unix))]
fn private_dir(_: &Path) -> Result<(), Error> {
    Err(Error::local("profile_lock_unsupported"))
}
fn read_profile(path: &Path) -> Result<Vec<u8>, Error> {
    use std::io::Read;
    let mut options = fs::OpenOptions::new();
    options.read(true);
    #[cfg(unix)]
    {
        use std::os::unix::fs::OpenOptionsExt;
        options.custom_flags(libc::O_NOFOLLOW);
    }
    let file = options
        .open(path)
        .map_err(|_| Error::local("profile_storage_unavailable"))?;
    let mut bytes = Vec::new();
    file.take(8193)
        .read_to_end(&mut bytes)
        .map_err(|_| Error::local("profile_storage_unavailable"))?;
    Ok(bytes)
}
fn safe_file(path: &Path) -> Result<(), Error> {
    let m = fs::symlink_metadata(path).map_err(|_| Error::local("profile_storage_unavailable"))?;
    if !m.is_file() || m.file_type().is_symlink() {
        return Err(Error::local("untrusted_profile_storage"));
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        if m.uid() != unsafe { libc::geteuid() } || m.mode() & 0o077 != 0 {
            return Err(Error::local("untrusted_profile_storage"));
        }
    }
    Ok(())
}
struct ProfileLock(fs::File);
impl ProfileLock {
    fn new(path: &Path, exclusive: bool, wait: Duration) -> Result<Self, Error> {
        let mut options = fs::OpenOptions::new();
        options.read(true).write(true).create(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options.mode(0o600).custom_flags(libc::O_NOFOLLOW);
        }
        let file = options
            .open(path)
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        safe_file(path)?;
        #[cfg(unix)]
        {
            use std::os::fd::AsRawFd;
            let operation = if exclusive {
                libc::LOCK_EX
            } else {
                libc::LOCK_SH
            };
            let deadline = std::time::Instant::now() + wait;
            loop {
                if unsafe { libc::flock(file.as_raw_fd(), operation | libc::LOCK_NB) } == 0 {
                    break;
                }
                let error = std::io::Error::last_os_error();
                if !matches!(
                    error.kind(),
                    std::io::ErrorKind::WouldBlock | std::io::ErrorKind::Interrupted
                ) {
                    return Err(Error::local("profile_storage_unavailable"));
                }
                let remaining = deadline.saturating_duration_since(std::time::Instant::now());
                if remaining.is_zero() {
                    return Err(Error::local("profile_storage_busy"));
                }
                // Only local metadata lock acquisition waits. No credential,
                // pairing or management operation is dispatched or retried here.
                std::thread::sleep(remaining.min(Duration::from_millis(5)));
            }
        }
        #[cfg(not(unix))]
        return Err(Error::local("profile_lock_unsupported"));
        #[allow(unreachable_code)]
        Ok(Self(file))
    }
}
impl Drop for ProfileLock {
    fn drop(&mut self) {
        #[cfg(unix)]
        {
            use std::os::fd::AsRawFd;
            unsafe { libc::flock(self.0.as_raw_fd(), libc::LOCK_UN) };
        }
    }
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    struct NoCredentials;
    impl CredentialStore for NoCredentials {
        fn put(&self, _: &str, _: &[u8]) -> Result<(), Error> {
            panic!("read must not touch credentials")
        }
        fn get(&self, _: &str) -> Result<Zeroizing<Vec<u8>>, Error> {
            panic!("read must not touch credentials")
        }
        fn delete(&self, _: &str) -> Result<(), Error> {
            panic!("read must not touch credentials")
        }
    }
    fn manager() -> (tempfile::TempDir, Arc<ConnectionManager>, String) {
        let directory = tempfile::tempdir().unwrap();
        let manager = Arc::new(
            ConnectionManager::with_store(
                directory.path().join("profiles"),
                Arc::new(NoCredentials),
            )
            .unwrap(),
        );
        let id = "1".repeat(64);
        manager
            .save(&ConnectionProfile {
                profile_ref: id.clone(),
                transport: ConnectionTransport::Local,
                endpoint: "unix:/fixture".into(),
                device_identity: format!("tls-sha256:{}", "2".repeat(64)),
                peer_identity: "local-user:1".into(),
                display_name: "Fixture".into(),
                client_name: "Test".into(),
                pairing_posture: ConnectionPosture::Approved,
                expires_at_unix_ms: None,
            })
            .unwrap();
        (directory, manager, id)
    }
    #[test]
    fn parallel_snapshot_reads_share_the_registry_lock() {
        let (_directory, manager, id) = manager();
        let held = manager.read_lock().unwrap();
        let workers: Vec<_> = (0..12)
            .map(|_| {
                let manager = manager.clone();
                let id = id.clone();
                std::thread::spawn(move || {
                    for _ in 0..20 {
                        assert_eq!(manager.get(&id).unwrap().profile_ref, id);
                        assert_eq!(manager.list().unwrap().len(), 1);
                    }
                })
            })
            .collect();
        for worker in workers {
            worker.join().unwrap();
        }
        drop(held);
    }
    #[test]
    fn ordinary_reads_wait_for_short_metadata_write_without_retrying_work() {
        let (_directory, manager, id) = manager();
        let held = manager.lock().unwrap();
        let worker = std::thread::spawn(move || manager.get(&id).unwrap());
        std::thread::sleep(Duration::from_millis(50));
        drop(held);
        assert_eq!(
            worker.join().unwrap().pairing_posture,
            ConnectionPosture::Approved
        );
    }
    #[test]
    fn stalled_lock_wait_has_a_finite_bound() {
        let (_directory, manager, _id) = manager();
        let held = manager.lock().unwrap();
        let began = std::time::Instant::now();
        let error = ProfileLock::new(
            &manager.root.join("registry.lock"),
            false,
            Duration::from_millis(30),
        )
        .err()
        .unwrap();
        assert_eq!(error.code, "profile_storage_busy");
        assert!(began.elapsed() < Duration::from_millis(300));
        drop(held);
    }
}
