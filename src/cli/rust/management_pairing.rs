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
        .as_chunks::<2>()
        .0
        .iter()
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
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct State {
    schema: String,
    open_until: u64,
    peers: Vec<Peer>,
    #[serde(default)]
    revision: u64,
    #[serde(default)]
    owners: Vec<Owner>,
    #[serde(default)]
    invitation: Option<Invitation>,
    #[serde(default)]
    owner_receipts: Vec<OwnerReceipt>,
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Owner {
    credential_hash: String,
    client_name: String,
    posture: String,
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Invitation {
    invitation_id: String,
    expires_at: u64,
    consumed_by: Option<String>,
}
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct OwnerReceipt {
    owner_ref: String,
    request_id: String,
    input: Value,
    receipt: Value,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct OwnerBootstrap {
    pub schema: String,
    pub invitation_secret: String,
    pub credential_hash: String,
    pub client_name: String,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct OwnerAction {
    pub schema: String,
    pub request_id: String,
    pub expected_revision: u64,
    pub action: String,
    pub target_ref: Option<String>,
}
#[derive(Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
struct Peer {
    #[serde(default)]
    service_control: bool,
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
    pub(crate) fn control_root(&self) -> PathBuf {
        self.root.join("host-control")
    }
    pub(crate) fn service_control(&self, hash: &str) -> Result<bool> {
        let _guard = self.lock()?;
        let state = self.state()?;
        Ok(state
            .peers
            .iter()
            .any(|p| p.credential_hash == hash && p.posture == "approved" && p.service_control))
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
                revision: 0,
                owners: Vec::new(),
                invitation: None,
                owner_receipts: Vec::new(),
            },
        };
        if !matches!(
            state.schema.as_str(),
            "yvex.management.pairing.ledger.v1" | "yvex.management.pairing.ledger.v2"
        ) || !valid_owner_state(&state)
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
        let mut next = state.clone();
        next.schema = "yvex.management.pairing.ledger.v2".into();
        next.revision = next
            .revision
            .checked_add(1)
            .ok_or("pairing_revision_exhausted")?;
        save(&self.root.join("pairing.json"), &next)
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
            service_control: false,
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
    pub(crate) fn owner_invite(&self, endpoint: &str, output: &str) -> Result<Value> {
        if !endpoint.starts_with("https://")
            || endpoint.len() > 2048
            || endpoint.contains(['@', '?', '#'])
            || endpoint.chars().any(char::is_whitespace)
        {
            return Err("invalid_owner_endpoint");
        }
        let output = Path::new(output);
        if !output.is_absolute() || output.exists() {
            return Err("invalid_invitation_output");
        }
        let parent = output.parent().ok_or("invalid_invitation_output")?;
        let metadata = fs::symlink_metadata(parent).map_err(|_| "invitation_output_unavailable")?;
        if !metadata.is_dir()
            || metadata.uid() != rustix::process::geteuid().as_raw()
            || metadata.mode() & 0o077 != 0
        {
            return Err("unsafe_invitation_output");
        }
        let device = self.identity()?.device();
        let _guard = self.lock()?;
        let mut state = self.state()?;
        if state.owners.iter().any(|owner| owner.posture == "approved") {
            return Err("owner_already_configured");
        }
        let mut secret = [0u8; 32];
        rustls::crypto::ring::default_provider()
            .secure_random
            .fill(&mut secret)
            .map_err(|_| "invitation_random_unavailable")?;
        let invitation_id = digest(&secret);
        let secret = secret
            .iter()
            .map(|byte| format!("{byte:02x}"))
            .collect::<String>();
        let expires_at = now() + 600_000;
        let bundle = json!({"schema":"yvex.management.owner.invitation.v1",
            "endpoint":endpoint,"device_identity":device,"invitation_id":invitation_id,
            "invitation_secret":secret,"expires_at_unix_ms":expires_at});
        let mut file = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .mode(0o600)
            .custom_flags(OFlags::NOFOLLOW.bits() as i32)
            .open(output)
            .map_err(|_| "invitation_output_unavailable")?;
        let result = (|| {
            file.write_all(&serde_json::to_vec(&bundle).map_err(|_| "invitation_encoding")?)
                .and_then(|_| file.sync_all())
                .map_err(|_| "invitation_output_unavailable")?;
            state.invitation = Some(Invitation {
                invitation_id: invitation_id.clone(),
                expires_at,
                consumed_by: None,
            });
            self.write(&state)
        })();
        if result.is_err() {
            let _ = fs::remove_file(output);
        }
        result?;
        Ok(
            json!({"schema":"yvex.management.owner.invitation.created.v1",
            "invitation_id":invitation_id,"device_identity":device,
            "expires_at_unix_ms":expires_at,"output":output}),
        )
    }
    pub(crate) fn claim_owner(&self, request: OwnerBootstrap) -> Result<Value> {
        if request.schema != "yvex.management.owner.bootstrap.v1"
            || !management_jobs::identity(&request.credential_hash)
            || !valid_name(&request.client_name)
        {
            return Err("invalid_owner_bootstrap");
        }
        let invitation_id = credential_hash(&request.invitation_secret)?;
        let _guard = self.lock()?;
        let mut state = self.state()?;
        let invitation = state
            .invitation
            .as_ref()
            .ok_or("owner_invitation_unavailable")?;
        if invitation.invitation_id != invitation_id {
            return Err("owner_invitation_invalid");
        }
        if let Some(consumed_by) = &invitation.consumed_by {
            if consumed_by != &request.credential_hash {
                return Err("owner_invitation_consumed");
            }
            return owner_projection(&state, consumed_by);
        }
        if invitation.expires_at <= now() {
            return Err("owner_invitation_expired");
        }
        if state.owners.iter().any(|owner| owner.posture == "approved") {
            return Err("owner_already_configured");
        }
        if state.owners.len() >= 16 {
            return Err("owner_retention_capacity");
        }
        if state
            .owners
            .iter()
            .any(|owner| owner.credential_hash == request.credential_hash)
        {
            return Err("owner_identity_previously_used");
        }
        state.owners.push(Owner {
            credential_hash: request.credential_hash.clone(),
            client_name: request.client_name,
            posture: "approved".into(),
        });
        state.invitation.as_mut().unwrap().consumed_by = Some(request.credential_hash.clone());
        self.write(&state)?;
        state.revision += 1;
        owner_projection(&state, &request.credential_hash)
    }
    pub(crate) fn owner_status(&self, hash: &str) -> Result<Value> {
        let _guard = self.lock()?;
        owner_projection(&self.state()?, hash)
    }
    pub(crate) fn owner_connections(&self, hash: &str) -> Result<Value> {
        let _guard = self.lock()?;
        let state = self.state()?;
        require_owner(&state, hash)?;
        Ok(json!({"schema":"yvex.management.owner.connections.v1",
            "scope":"pairing-administration","owner_ref":hash,"revision":state.revision,
            "open_until_unix_ms":state.open_until,"available_actions":["pairing_open","pairing_approve","pairing_revoke","owner_revoke","service_control_grant","service_control_revoke"],"peers":state.peers.iter().map(|peer| {
                json!({"request_id":peer.credential_hash,"client_name":peer.client_name,
                    "posture":projection(peer,now())["posture"],"scope":"product-management",
                    "expires_at_unix_ms":peer.expires_at,"service_control_granted":peer.service_control&&peer.posture=="approved"})
            }).collect::<Vec<_>>()}))
    }
    pub(crate) fn owner_action(&self, hash: &str, request: OwnerAction) -> Result<Value> {
        if request.schema != "yvex.management.owner.action.v1"
            || !management_jobs::identity(&request.request_id)
            || !matches!(
                request.action.as_str(),
                "pairing_open"
                    | "pairing_approve"
                    | "pairing_revoke"
                    | "owner_revoke"
                    | "service_control_grant"
                    | "service_control_revoke"
            )
            || (request.action == "pairing_open") != request.target_ref.is_none()
            || request
                .target_ref
                .as_deref()
                .is_some_and(|v| !management_jobs::identity(v))
        {
            return Err("invalid_owner_action");
        }
        let _guard = self.lock()?;
        let mut state = self.state()?;
        require_owner(&state, hash)?;
        let input = serde_json::to_value(&request).map_err(|_| "invalid_owner_action")?;
        if let Some(existing) = state
            .owner_receipts
            .iter()
            .find(|r| r.owner_ref == hash && r.request_id == request.request_id)
        {
            return if existing.input == input {
                Ok(existing.receipt.clone())
            } else {
                Err("owner_request_identity_conflict")
            };
        }
        if state.owner_receipts.len() >= 256 {
            return Err("owner_receipt_retention_capacity");
        }
        let next_revision = state
            .revision
            .checked_add(1)
            .ok_or("pairing_revision_exhausted")?;
        let refusal = if state.revision != request.expected_revision {
            Some("stale_owner_revision")
        } else {
            apply_owner_action(&mut state, &request).err()
        };
        let receipt = json!({"schema":"yvex.management.owner.action-receipt.v1",
            "scope":"pairing-administration","owner_ref":hash,"request_id":request.request_id,
            "action":request.action,"target_ref":request.target_ref,"revision":next_revision,
            "posture":if refusal.is_some() {"refused"} else {"applied"},"reason":refusal});
        state.owner_receipts.push(OwnerReceipt {
            owner_ref: hash.into(),
            request_id: request.request_id,
            input,
            receipt: receipt.clone(),
        });
        self.write(&state)?;
        Ok(receipt)
    }
    pub(crate) fn owner_receipt(&self, hash: &str, request: &str) -> Result<Value> {
        if !management_jobs::identity(request) {
            return Err("invalid_owner_request_identity");
        }
        let _guard = self.lock()?;
        let state = self.state()?;
        // Revocation still permits observation of this credential's own prior outcome.
        owner_projection(&state, hash)?;
        state
            .owner_receipts
            .iter()
            .find(|r| r.owner_ref == hash && r.request_id == request)
            .map(|r| r.receipt.clone())
            .ok_or("owner_request_unknown")
    }
    pub(crate) fn revoke_owner(&self, hash: &str) -> Result<Value> {
        if !management_jobs::identity(hash) {
            return Err("invalid_owner_identity");
        }
        let _guard = self.lock()?;
        let mut state = self.state()?;
        let owner = state
            .owners
            .iter_mut()
            .find(|o| o.credential_hash == hash)
            .ok_or("owner_unknown")?;
        owner.posture = "revoked".into();
        self.write(&state)?;
        state.revision += 1;
        owner_projection(&state, hash)
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
fn valid_owner_state(state: &State) -> bool {
    state.owners.len() <= 16
        && state.owner_receipts.len() <= 256
        && state.owners.iter().all(|o| {
            management_jobs::identity(&o.credential_hash)
                && valid_name(&o.client_name)
                && matches!(o.posture.as_str(), "approved" | "revoked")
        })
        && state
            .owners
            .iter()
            .filter(|o| o.posture == "approved")
            .count()
            <= 1
        && state.invitation.as_ref().is_none_or(|i| {
            management_jobs::identity(&i.invitation_id)
                && i.consumed_by
                    .as_deref()
                    .is_none_or(management_jobs::identity)
        })
        && state.owner_receipts.iter().all(|r| {
            management_jobs::identity(&r.owner_ref)
                && management_jobs::identity(&r.request_id)
                && r.receipt["schema"] == "yvex.management.owner.action-receipt.v1"
                && r.receipt["owner_ref"] == r.owner_ref
                && r.receipt["request_id"] == r.request_id
        })
}
fn require_owner(state: &State, hash: &str) -> Result<()> {
    if state
        .owners
        .iter()
        .any(|o| o.credential_hash == hash && o.posture == "approved")
    {
        Ok(())
    } else {
        Err("owner_administration_required")
    }
}
fn owner_projection(state: &State, hash: &str) -> Result<Value> {
    let owner = state
        .owners
        .iter()
        .find(|o| o.credential_hash == hash)
        .ok_or("owner_unknown")?;
    Ok(
        json!({"schema":"yvex.management.owner.v1","scope":"pairing-administration",
        "owner_ref":hash,"client_name":owner.client_name,"posture":owner.posture,
        "revision":state.revision}),
    )
}
fn apply_owner_action(state: &mut State, request: &OwnerAction) -> Result<()> {
    let target = request.target_ref.as_deref().unwrap_or("");
    match request.action.as_str() {
        "pairing_open" => state.open_until = now() + WINDOW_MS,
        "service_control_grant" | "service_control_revoke" => {
            let peer = state
                .peers
                .iter_mut()
                .find(|p| p.credential_hash == target && p.posture == "approved")
                .ok_or("approved_peer_required")?;
            peer.service_control = request.action == "service_control_grant";
        }
        "owner_revoke" => {
            let owner = state
                .owners
                .iter_mut()
                .find(|o| o.credential_hash == target)
                .ok_or("owner_unknown")?;
            owner.posture = "revoked".into();
        }
        "pairing_approve" | "pairing_revoke" => {
            if request.action == "pairing_approve"
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
                .find(|p| p.credential_hash == target)
                .ok_or("pairing_request_unknown")?;
            if request.action == "pairing_approve" {
                if peer.posture != "pending" || peer.expires_at <= now() {
                    return Err("pairing_request_not_pending");
                }
                peer.posture = "approved".into();
                peer.approved_at = Some(now());
            } else {
                peer.posture = "revoked".into();
            }
        }
        _ => return Err("unsupported_owner_action"),
    }
    Ok(())
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
    fn bootstrap(store: &Store) -> (String, String) {
        let output = store.root.parent().unwrap().join("owner-invite.json");
        let created = store
            .owner_invite("https://127.0.0.1:18080", output.to_str().unwrap())
            .unwrap();
        let bundle: Value = serde_json::from_slice(&fs::read(&output).unwrap()).unwrap();
        assert_eq!(fs::metadata(&output).unwrap().mode() & 0o777, 0o600);
        let secret = bundle["invitation_secret"].as_str().unwrap().to_owned();
        assert!(!created.to_string().contains(&secret));
        let owner = credential_hash(&"12".repeat(32)).unwrap();
        let result = store
            .claim_owner(OwnerBootstrap {
                schema: "yvex.management.owner.bootstrap.v1".into(),
                invitation_secret: secret.clone(),
                credential_hash: owner.clone(),
                client_name: "Owner fixture".into(),
            })
            .unwrap();
        assert_eq!(result["scope"], "pairing-administration");
        assert_eq!(result["posture"], "approved");
        assert!(
            store.authorize(&owner).is_err(),
            "Ownership never grants product management"
        );
        assert!(
            !fs::read_to_string(store.root.join("pairing.json"))
                .unwrap()
                .contains(&secret)
        );
        (owner, secret)
    }
    fn owner_request(state: &State, id: &str, action: &str, target: Option<&str>) -> OwnerAction {
        OwnerAction {
            schema: "yvex.management.owner.action.v1".into(),
            request_id: id.repeat(32),
            expected_revision: state.revision,
            action: action.into(),
            target_ref: target.map(str::to_owned),
        }
    }
    #[test]
    fn headless_owner_requires_invitation_and_keeps_management_separate() {
        let store = store();
        let (owner, secret) = bootstrap(&store);
        let ordinary = credential_hash(&"34".repeat(32)).unwrap();
        assert!(store.owner_connections(&ordinary).is_err());
        assert_eq!(
            store.claim_owner(OwnerBootstrap {
                schema: "yvex.management.owner.bootstrap.v1".into(),
                invitation_secret: secret,
                credential_hash: ordinary.clone(),
                client_name: "Replay".into()
            }),
            Err("owner_invitation_consumed")
        );
        let open = owner_request(&store.state().unwrap(), "01", "pairing_open", None);
        let open_result = store.owner_action(&owner, open).unwrap();
        assert_eq!(open_result["posture"], "applied");
        assert_eq!(
            store.request(&ordinary, "Ordinary client").unwrap()["posture"],
            "pending"
        );
        assert!(store.authorize(&ordinary).is_err());
        let approve = owner_request(
            &store.state().unwrap(),
            "02",
            "pairing_approve",
            Some(&ordinary),
        );
        let approved = store.owner_action(&owner, approve).unwrap();
        assert_eq!(approved["posture"], "applied");
        assert!(store.authorize(&ordinary).is_ok());
        assert!(
            store.owner_connections(&ordinary).is_err(),
            "Management clients cannot administer pairing"
        );
        assert_eq!(
            store.owner_receipt(&owner, &"02".repeat(32)).unwrap(),
            approved
        );
        let revoke = owner_request(
            &store.state().unwrap(),
            "03",
            "pairing_revoke",
            Some(&ordinary),
        );
        store.owner_action(&owner, revoke).unwrap();
        assert!(store.authorize(&ordinary).is_err());
        fs::remove_dir_all(store.root.parent().unwrap()).unwrap();
    }
    #[test]
    fn owner_revision_receipts_and_revocation_do_not_replay_actions() {
        let store = store();
        let (owner, secret) = bootstrap(&store);
        let request = owner_request(&store.state().unwrap(), "04", "pairing_open", None);
        let input = serde_json::to_vec(&request).unwrap();
        let applied = store.owner_action(&owner, request).unwrap();
        let revision = store.state().unwrap().revision;
        assert_eq!(
            store
                .owner_action(&owner, serde_json::from_slice(&input).unwrap())
                .unwrap(),
            applied
        );
        assert_eq!(store.state().unwrap().revision, revision);
        let mut conflict: OwnerAction = serde_json::from_slice(&input).unwrap();
        conflict.expected_revision += 1;
        assert_eq!(
            store.owner_action(&owner, conflict),
            Err("owner_request_identity_conflict")
        );
        let mut stale = owner_request(&store.state().unwrap(), "05", "pairing_open", None);
        stale.expected_revision = 0;
        let old_window = store.state().unwrap().open_until;
        assert_eq!(
            store.owner_action(&owner, stale).unwrap()["reason"],
            "stale_owner_revision"
        );
        assert_eq!(store.state().unwrap().open_until, old_window);
        let revoke = owner_request(&store.state().unwrap(), "06", "owner_revoke", Some(&owner));
        assert_eq!(
            store.owner_action(&owner, revoke).unwrap()["posture"],
            "applied"
        );
        assert!(store.owner_connections(&owner).is_err());
        assert_eq!(store.owner_status(&owner).unwrap()["posture"], "revoked");
        assert_eq!(
            store.owner_receipt(&owner, &"06".repeat(32)).unwrap()["posture"],
            "applied"
        );
        assert_eq!(
            store
                .claim_owner(OwnerBootstrap {
                    schema: "yvex.management.owner.bootstrap.v1".into(),
                    invitation_secret: secret,
                    credential_hash: owner,
                    client_name: "Replay".into()
                })
                .unwrap()["posture"],
            "revoked"
        );
        fs::remove_dir_all(store.root.parent().unwrap()).unwrap();
    }
    #[test]
    fn expired_or_wrong_invitation_never_creates_owner() {
        let store = store();
        let path = store.root.parent().unwrap().join("invite.json");
        store
            .owner_invite("https://127.0.0.1:18080", path.to_str().unwrap())
            .unwrap();
        let bundle: Value = serde_json::from_slice(&fs::read(path).unwrap()).unwrap();
        let mut state = store.state().unwrap();
        state.invitation.as_mut().unwrap().expires_at = 1;
        store.write(&state).unwrap();
        let owner = credential_hash(&"78".repeat(32)).unwrap();
        assert_eq!(
            store.claim_owner(OwnerBootstrap {
                schema: "yvex.management.owner.bootstrap.v1".into(),
                invitation_secret: bundle["invitation_secret"].as_str().unwrap().into(),
                credential_hash: owner.clone(),
                client_name: "Expired".into()
            }),
            Err("owner_invitation_expired")
        );
        assert_eq!(
            store.claim_owner(OwnerBootstrap {
                schema: "yvex.management.owner.bootstrap.v1".into(),
                invitation_secret: "00".repeat(32),
                credential_hash: owner.clone(),
                client_name: "Wrong".into()
            }),
            Err("owner_invitation_invalid")
        );
        assert_eq!(store.owner_status(&owner), Err("owner_unknown"));
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
