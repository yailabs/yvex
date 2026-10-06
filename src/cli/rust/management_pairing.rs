// Local approval and private TLS identity for the standalone HTTPS product service.
// Only credential digests enter the ledger. Grants never confer YAI authority.
use crate::management_jobs;
use rustix::fs::{FlockOperation, OFlags, flock};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use sha2::{Digest, Sha256};
use std::{
    fs,
    io::{Read, Write},
    os::unix::fs::{DirBuilderExt, MetadataExt, OpenOptionsExt},
    path::{Path, PathBuf},
    time::{SystemTime, UNIX_EPOCH},
};
pub(crate) type Result<T> = std::result::Result<T, &'static str>;
const STATE_CAP: u64 = 262144;
const WINDOW_MS: u64 = 120000;
const PENDING_CAP: usize = 32;
const GRANT_CAP: usize = 128;
pub(crate) fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}
pub(crate) fn digest(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}
pub(crate) fn credential_hash(raw: &str) -> Result<String> {
    if !management_jobs::identity(raw) {
        return Err("invalid_credential");
    }
    let bytes: Vec<u8> = raw
        .as_bytes()
        .chunks_exact(2)
        .map(|v| {
            let digit = |v: u8| if v <= b'9' { v - b'0' } else { v - b'a' + 10 };
            digit(v[0]) * 16 + digit(v[1])
        })
        .collect();
    Ok(digest(&bytes))
}
#[derive(Clone)]
pub(crate) struct Store {
    root: PathBuf,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct State {
    schema: String,
    open_until: u64,
    peers: Vec<Peer>,
}
#[derive(Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
struct Peer {
    credential_hash: String,
    client_name: String,
    posture: String,
    requested_at: u64,
    expires_at: u64,
    approved_at: Option<u64>,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Identity {
    pub certificate: Vec<u8>,
    pub private_key: Vec<u8>,
}
impl Identity {
    pub(crate) fn device(&self) -> String {
        format!("tls-sha256:{}", digest(&self.certificate))
    }
}
fn safe_file(file: &fs::File) -> Result<()> {
    let m = file.metadata().map_err(|_| "service_storage_unavailable")?;
    if !m.is_file()
        || m.nlink() != 1
        || m.mode() & 0o077 != 0
        || m.uid() != rustix::process::geteuid().as_raw()
    {
        return Err("unsafe_service_storage");
    }
    Ok(())
}
fn read(path: &Path) -> Result<Option<Vec<u8>>> {
    let mut file = match fs::OpenOptions::new()
        .read(true)
        .custom_flags(OFlags::NOFOLLOW.bits() as i32)
        .open(path)
    {
        Ok(file) => file,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(_) => return Err("service_storage_unavailable"),
    };
    safe_file(&file)?;
    let mut bytes = Vec::new();
    (&mut file)
        .take(STATE_CAP + 1)
        .read_to_end(&mut bytes)
        .map_err(|_| "service_storage_unavailable")?;
    if bytes.len() > STATE_CAP as usize {
        return Err("service_storage_bound");
    }
    Ok(Some(bytes))
}
fn save(path: &Path, value: &impl Serialize) -> Result<()> {
    let bytes = serde_json::to_vec(value).map_err(|_| "service_storage_encoding")?;
    if bytes.len() > STATE_CAP as usize {
        return Err("service_storage_bound");
    }
    let temp = path.with_extension(format!("{}.tmp", std::process::id()));
    let mut file = fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .mode(0o600)
        .custom_flags(OFlags::NOFOLLOW.bits() as i32)
        .open(&temp)
        .map_err(|_| "service_storage_publication")?;
    let result = (|| {
        file.write_all(&bytes)?;
        file.sync_all()?;
        fs::rename(&temp, path)?;
        fs::File::open(path.parent().unwrap())?.sync_all()
    })();
    if result.is_err() {
        let _ = fs::remove_file(&temp);
    }
    result.map_err(|_| "service_storage_publication")
}
impl Store {
    pub(crate) fn open(explicit: Option<&str>) -> Result<Self> {
        let root = if let Some(path) = explicit {
            PathBuf::from(path)
        } else {
            std::env::var_os("XDG_CONFIG_HOME")
                .map(PathBuf::from)
                .filter(|path| path.is_absolute())
                .or_else(|| std::env::var_os("HOME").map(|v| PathBuf::from(v).join(".config")))
                .ok_or("service_storage_unavailable")?
                .join("yvex/management-network-v1")
        };
        if !root.is_absolute() || root.as_os_str().len() > 4096 {
            return Err("invalid_service_storage");
        }
        let parent = root.parent().ok_or("invalid_service_storage")?;
        fs::create_dir_all(parent).map_err(|_| "service_storage_unavailable")?;
        let metadata = fs::symlink_metadata(parent).map_err(|_| "unsafe_service_parent")?;
        if !metadata.is_dir()
            || metadata.uid() != rustix::process::geteuid().as_raw()
            || metadata.mode() & 0o022 != 0
        {
            return Err("unsafe_service_parent");
        }
        if !root.exists() {
            fs::DirBuilder::new()
                .mode(0o700)
                .create(&root)
                .map_err(|_| "service_storage_unavailable")?;
        }
        let metadata = fs::symlink_metadata(&root).map_err(|_| "unsafe_service_storage")?;
        if !metadata.is_dir()
            || metadata.uid() != rustix::process::geteuid().as_raw()
            || metadata.mode() & 0o077 != 0
        {
            return Err("unsafe_service_storage");
        }
        Ok(Self { root })
    }
    fn lock(&self) -> Result<fs::File> {
        let file = fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .mode(0o600)
            .custom_flags(OFlags::NOFOLLOW.bits() as i32)
            .open(self.root.join("ledger.lock"))
            .map_err(|_| "service_lock_unavailable")?;
        safe_file(&file)?;
        flock(&file, FlockOperation::LockExclusive).map_err(|_| "service_lock_unavailable")?;
        Ok(file)
    }
    fn state(&self) -> Result<State> {
        let state = match read(&self.root.join("pairing.json"))? {
            Some(bytes) => {
                serde_json::from_slice::<State>(&bytes).map_err(|_| "pairing_ledger_malformed")?
            }
            None => State {
                schema: "yvex.management.pairing.ledger.v1".into(),
                open_until: 0,
                peers: Vec::new(),
            },
        };
        if state.schema != "yvex.management.pairing.ledger.v1"
            || state.peers.len() > PENDING_CAP + GRANT_CAP
            || state.peers.iter().any(|peer| {
                !management_jobs::identity(&peer.credential_hash)
                    || !matches!(
                        peer.posture.as_str(),
                        "pending" | "approved" | "revoked" | "expired"
                    )
                    || !valid_name(&peer.client_name)
            })
        {
            return Err("pairing_ledger_malformed");
        }
        Ok(state)
    }
    fn write(&self, state: &State) -> Result<()> {
        save(&self.root.join("pairing.json"), state)
    }
    pub(crate) fn identity(&self) -> Result<Identity> {
        let _guard = self.lock()?;
        let path = self.root.join("identity.json");
        if let Some(bytes) = read(&path)? {
            let identity: Identity =
                serde_json::from_slice(&bytes).map_err(|_| "service_identity_malformed")?;
            if identity.certificate.is_empty() || identity.private_key.is_empty() {
                return Err("service_identity_malformed");
            }
            return Ok(identity);
        }
        if self.root.join("pairing.json").exists() {
            return Err("service_identity_missing");
        }
        let generated = rcgen::generate_simple_self_signed(vec![
            "localhost".into(),
            "127.0.0.1".into(),
            "::1".into(),
        ])
        .map_err(|_| "service_identity_unavailable")?;
        let identity = Identity {
            certificate: generated.cert.der().to_vec(),
            private_key: generated.key_pair.serialize_der(),
        };
        save(&path, &identity)?;
        Ok(identity)
    }
    pub(crate) fn available(&self) -> Result<bool> {
        let _guard = self.lock()?;
        Ok(self.state()?.open_until > now())
    }
    pub(crate) fn open_window(&self) -> Result<Value> {
        let _guard = self.lock()?;
        let mut state = self.state()?;
        state.open_until = now() + WINDOW_MS;
        self.write(&state)?;
        Ok(json!({"schema":"yvex.management.pairing.window.v1",
            "expires_at_unix_ms":state.open_until,"scope":"product-management"}))
    }
    pub(crate) fn request(&self, hash: &str, name: &str) -> Result<Value> {
        if !management_jobs::identity(hash) || !valid_name(name) {
            return Err("invalid_pairing_request");
        }
        let _guard = self.lock()?;
        let mut state = self.state()?;
        let time = now();
        if let Some(peer) = state.peers.iter().find(|peer| peer.credential_hash == hash) {
            return Ok(projection(peer, time));
        }
        if state.open_until <= time {
            return Err("pairing_window_closed");
        }
        for peer in &mut state.peers {
            if peer.posture == "pending" && peer.expires_at <= time {
                peer.posture = "expired".into();
            }
        }
        if state.peers.len() >= PENDING_CAP + GRANT_CAP
            || state
                .peers
                .iter()
                .filter(|p| p.posture == "pending")
                .count()
                >= PENDING_CAP
        {
            return Err("pairing_pending_capacity");
        }
        let peer = Peer {
            credential_hash: hash.into(),
            client_name: name.into(),
            posture: "pending".into(),
            requested_at: time,
            expires_at: state.open_until,
            approved_at: None,
        };
        let reply = projection(&peer, time);
        state.peers.push(peer);
        self.write(&state)?;
        Ok(reply)
    }
    pub(crate) fn status(&self, hash: &str) -> Result<Value> {
        let _guard = self.lock()?;
        let state = self.state()?;
        let peer = state
            .peers
            .iter()
            .find(|p| p.credential_hash == hash)
            .ok_or("pairing_request_unknown")?;
        Ok(projection(peer, now()))
    }
    pub(crate) fn authorize(&self, hash: &str) -> Result<()> {
        let _guard = self.lock()?;
        let state = self.state()?;
        if state
            .peers
            .iter()
            .any(|p| p.credential_hash == hash && p.posture == "approved")
        {
            Ok(())
        } else {
            Err("peer_not_approved_or_revoked")
        }
    }
    pub(crate) fn decide(&self, hash: &str, approve: bool) -> Result<Value> {
        if !management_jobs::identity(hash) {
            return Err("invalid_pairing_identity");
        }
        let _guard = self.lock()?;
        let mut state = self.state()?;
        let time = now();
        if approve
            && state
                .peers
                .iter()
                .filter(|p| p.posture != "pending")
                .count()
                >= GRANT_CAP
        {
            return Err("pairing_grant_capacity");
        }
        let peer = state
            .peers
            .iter_mut()
            .find(|p| p.credential_hash == hash)
            .ok_or("pairing_request_unknown")?;
        if approve {
            if peer.posture != "pending" || peer.expires_at <= time {
                return Err("pairing_request_not_pending");
            }
            peer.posture = "approved".into();
            peer.approved_at = Some(time);
        } else {
            peer.posture = "revoked".into();
        }
        let reply = projection(peer, time);
        self.write(&state)?;
        Ok(reply)
    }
    pub(crate) fn list(&self) -> Result<Value> {
        let _guard = self.lock()?;
        let state = self.state()?;
        Ok(
            json!({"schema":"yvex.management.pairing.list.v1", "peers":state.peers.iter()
            .map(|p| { let mut value = projection(p, now());
                value["client_name"] = json!(p.client_name); value }).collect::<Vec<_>>()}),
        )
    }
}
pub(crate) fn valid_name(name: &str) -> bool {
    !name.trim().is_empty() && name.len() <= 80 && !name.chars().any(char::is_control)
}
fn projection(peer: &Peer, time: u64) -> Value {
    let expired = peer.posture == "expired" || peer.posture == "pending" && peer.expires_at <= time;
    json!({"schema":"yvex.management.pairing.v1","request_id":peer.credential_hash,
        "posture":if expired {"expired"} else {&peer.posture},
        "scope":"product-management","expires_at_unix_ms":peer.expires_at,
        "reason":if expired {Some("pairing_request_expired")} else {None}})
}
#[cfg(test)]
mod tests {
    use super::*;
    use std::os::unix::fs::PermissionsExt;
    fn store() -> Store {
        let path = std::env::temp_dir().join(format!(
            "yvex-pairing-{}-{}",
            std::process::id(),
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        fs::DirBuilder::new().mode(0o700).create(&path).unwrap();
        Store::open(path.join("service").to_str()).unwrap()
    }
    #[test]
    fn approval_revocation_expiry_and_no_credentials() {
        let store = store();
        let raw = "ab".repeat(32);
        let hash = credential_hash(&raw).unwrap();
        assert_eq!(store.request(&hash, "Studio"), Err("pairing_window_closed"));
        store.open_window().unwrap();
        assert_eq!(
            store.request(&hash, "Studio").unwrap()["posture"],
            "pending"
        );
        assert!(store.authorize(&hash).is_err());
        store.decide(&hash, true).unwrap();
        assert!(store.authorize(&hash).is_ok());
        store.decide(&hash, false).unwrap();
        assert!(store.authorize(&hash).is_err());
        assert!(store.decide(&hash, true).is_err());
        assert!(
            !fs::read_to_string(store.root.join("pairing.json"))
                .unwrap()
                .contains(&raw)
        );
        let mut state = store.state().unwrap();
        state.peers[0].posture = "pending".into();
        state.peers[0].expires_at = 1;
        store.write(&state).unwrap();
        assert_eq!(store.status(&hash).unwrap()["posture"], "expired");
        assert!(store.decide(&hash, true).is_err());
        let another = credential_hash(&"cd".repeat(32)).unwrap();
        store.request(&another, "Another client").unwrap();
        assert_eq!(
            store.request(&hash, "Old client").unwrap()["posture"],
            "expired"
        );
        fs::remove_dir_all(store.root.parent().unwrap()).unwrap();
    }
    #[test]
    fn certificate_is_stable_and_storage_is_private() {
        let store = store();
        let first = store.identity().unwrap();
        assert_eq!(first.device(), store.identity().unwrap().device());
        assert_eq!(
            fs::metadata(store.root.join("identity.json"))
                .unwrap()
                .mode()
                & 0o777,
            0o600
        );
        fs::set_permissions(
            store.root.join("identity.json"),
            fs::Permissions::from_mode(0o644),
        )
        .unwrap();
        assert!(store.identity().is_err());
        fs::remove_dir_all(store.root.parent().unwrap()).unwrap();
    }
    #[test]
    fn credential_encoding_and_scope_are_exact() {
        assert_eq!(credential_hash(&"00".repeat(32)).unwrap(), digest(&[0; 32]));
        assert!(credential_hash(&"AA".repeat(32)).is_err());
        assert!(credential_hash("secret").is_err());
        assert!(!valid_name("bad\nlabel"));
    }
}
