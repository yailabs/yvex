//! Explicit headless owner bootstrap and pairing administration. No product grant is implied.
use super::*;
pub use crate::management::network_types::{
    OwnerActionKind, OwnerActionPosture, OwnerActionReceipt, OwnerActionRequest,
    OwnerAdministrationObservation, OwnerInvitationPreview, OwnerObservation, OwnerPosture,
    OwnerProfile, OwnerProfilePosture,
};
use serde::{Deserialize, Serialize};
use std::io::Read;
use std::time::{SystemTime, UNIX_EPOCH};
const SCOPE: &str = "pairing-administration";
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Invitation {
    schema: String,
    endpoint: String,
    device_identity: String,
    invitation_id: String,
    invitation_secret: String,
    expires_at_unix_ms: u64,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct OwnerRecord {
    profile: OwnerProfile,
    actions: Vec<TrackedAction>,
}
#[derive(Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct TrackedAction {
    request: OwnerActionRequest,
    complete: bool,
}
fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as u64
}
fn invitation(bytes: &[u8]) -> Result<Invitation, Error> {
    let value: Invitation =
        serde_json::from_slice(bytes).map_err(|_| Error::local("invalid_owner_invitation"))?;
    if value.schema != "yvex.management.owner.invitation.v1"
        || !valid_id(&value.invitation_id)
        || !valid_id(&value.invitation_secret)
        || value.expires_at_unix_ms <= now()
    {
        return Err(Error::local("invalid_or_expired_owner_invitation"));
    }
    let mut raw = Zeroizing::new([0u8; 32]);
    for (i, pair) in value
        .invitation_secret
        .as_bytes()
        .chunks_exact(2)
        .enumerate()
    {
        let digit = |b: u8| if b <= b'9' { b - b'0' } else { b - b'a' + 10 };
        raw[i] = digit(pair[0]) * 16 + digit(pair[1]);
    }
    if Credential::from_bytes(*raw).hash() != value.invitation_id {
        return Err(Error::local("owner_invitation_identity_mismatch"));
    }
    HttpsConnection::new(
        &value.endpoint,
        &value.device_identity,
        Credential::from_bytes([0; 32]),
    )?;
    Ok(value)
}
fn invitation_bytes(path: &Path) -> Result<Zeroizing<Vec<u8>>, Error> {
    if !path.is_absolute() {
        return Err(Error::local("invalid_invitation_path"));
    }
    safe_file(path)?;
    let bytes = Zeroizing::new(read_profile(path)?);
    if bytes.len() > 8192 {
        return Err(Error::local("owner_invitation_bound"));
    }
    Ok(bytes)
}
impl OwnerActionRequest {
    pub fn new(
        expected_revision: u64,
        action: OwnerActionKind,
        target_ref: Option<String>,
    ) -> Result<Self, Error> {
        let request = Self {
            schema: "yvex.management.owner.action.v1".into(),
            request_id: Credential::generate()?.hash(),
            expected_revision,
            action,
            target_ref,
        };
        validate_request(&request)?;
        Ok(request)
    }
}
fn validate_request(request: &OwnerActionRequest) -> Result<(), Error> {
    if request.schema != "yvex.management.owner.action.v1"
        || !valid_id(&request.request_id)
        || (request.action == OwnerActionKind::PairingOpen) != request.target_ref.is_none()
        || request.target_ref.as_deref().is_some_and(|v| !valid_id(v))
    {
        return Err(Error::local("invalid_owner_action"));
    }
    Ok(())
}
impl ConnectionManager {
    /// Inspection returns public identity only.
    /// Callers must explicitly confirm the invitation's trusted source.
    pub fn inspect_owner_invitation(path: &Path) -> Result<OwnerInvitationPreview, Error> {
        let value = invitation(&invitation_bytes(path)?)?;
        Ok(OwnerInvitationPreview {
            endpoint: value.endpoint,
            device_identity: value.device_identity,
            invitation_id: value.invitation_id,
            expires_at_unix_ms: value.expires_at_unix_ms,
        })
    }
    /// Native file import; secrets remain in the protected credential store. No request is sent.
    pub fn prepare_owner(&self, path: &Path, client_name: &str) -> Result<OwnerProfile, Error> {
        if client_name.trim().is_empty()
            || client_name.len() > 80
            || client_name.chars().any(char::is_control)
        {
            return Err(Error::local("invalid_client_name"));
        }
        let bytes = invitation_bytes(path)?;
        let value = invitation(&bytes)?;
        let id = Credential::generate()?.hash();
        let connection = HttpsConnection::new(
            &value.endpoint,
            &value.device_identity,
            Credential::generate()?,
        )?;
        let profile = OwnerProfile {
            profile_ref: id.clone(),
            endpoint: value.endpoint,
            device_identity: value.device_identity,
            peer_identity: connection.peer_identity().into(),
            client_name: client_name.into(),
            posture: OwnerProfilePosture::Prepared,
            pending_requests: vec![],
        };
        let _guard = self.lock()?;
        self.store
            .put(&format!("owner:{id}"), connection.credential_bytes())?;
        if let Err(error) = self.store.put(&format!("owner-bootstrap:{id}"), &bytes) {
            let _ = self.store.delete(&format!("owner:{id}"));
            return Err(error);
        }
        if let Err(error) = self.save_owner(&OwnerRecord {
            profile: profile.clone(),
            actions: vec![],
        }) {
            let _ = self.store.delete(&format!("owner:{id}"));
            let _ = self.store.delete(&format!("owner-bootstrap:{id}"));
            return Err(error);
        }
        Ok(profile)
    }
    pub fn owner_list(&self) -> Result<Vec<OwnerProfile>, Error> {
        let _guard = self.read_lock()?;
        let mut profiles = vec![];
        for entry in
            fs::read_dir(&self.root).map_err(|_| Error::local("profile_storage_unavailable"))?
        {
            let entry = entry.map_err(|_| Error::local("profile_storage_unavailable"))?;
            let name = entry.file_name();
            let Some(name) = name.to_str() else { continue };
            if let Some(id) = name.strip_suffix(".owner.json").filter(|id| valid_id(id)) {
                profiles.push(self.load_owner(id)?.profile);
            }
            if profiles.len() > 32 {
                return Err(Error::local("owner_profile_inventory_bound"));
            }
        }
        Ok(profiles)
    }
    pub fn owner_get(&self, id: &str) -> Result<OwnerProfile, Error> {
        let _guard = self.read_lock()?;
        Ok(self.load_owner(id)?.profile)
    }
    pub fn claim_owner(&self, id: &str) -> Result<OwnerObservation, Error> {
        let profile = self.owner_get(id)?;
        if profile.posture != OwnerProfilePosture::Prepared {
            return self.owner_status(id);
        }
        let bytes = self.store.get(&format!("owner-bootstrap:{id}"))?;
        let value = invitation(&bytes)?;
        if value.endpoint != profile.endpoint || value.device_identity != profile.device_identity {
            return Err(Error::local("owner_invitation_identity_mismatch"));
        }
        let connection = self.owner_https(&profile)?;
        let input = serde_json::json!({
            "schema":"yvex.management.owner.bootstrap.v1",
            "invitation_secret":value.invitation_secret,
            "credential_hash":owner_ref(&profile)?,"client_name":profile.client_name
        });
        let body = Zeroizing::new(
            serde_json::to_vec(&input).map_err(|_| Error::local("invalid_owner_bootstrap"))?,
        );
        {
            let _guard = self.lock()?;
            let mut record = self.load_owner(id)?;
            if record.profile.posture != OwnerProfilePosture::Prepared {
                drop(_guard);
                return self.owner_status(id);
            }
            record.profile.posture = OwnerProfilePosture::Claiming;
            self.save_owner(&record)?;
        }
        let reply =
            connection.owner_exchange("POST", "/v1/owner/bootstrap", &body, false, self.timeout)?;
        self.record_owner(id, &reply)
    }
    pub fn owner_status(&self, id: &str) -> Result<OwnerObservation, Error> {
        let profile = self.owner_get(id)?;
        let reply = self.owner_https(&profile)?.owner_exchange(
            "GET",
            "/v1/owner/status",
            &[],
            true,
            self.timeout,
        )?;
        self.record_owner(id, &reply)
    }
    pub fn owner_connections(&self, id: &str) -> Result<OwnerAdministrationObservation, Error> {
        let profile = self.owner_get(id)?;
        let bytes = self.owner_https(&profile)?.owner_exchange(
            "GET",
            "/v1/owner/connections",
            &[],
            true,
            self.timeout,
        )?;
        let value: OwnerAdministrationObservation = serde_json::from_slice(&bytes)
            .map_err(|_| Error::uncertain("invalid_owner_connections"))?;
        if value.schema != "yvex.management.owner.connections.v1"
            || value.scope != SCOPE
            || value.owner_ref != owner_ref(&profile)?
            || value.peers.len() > 160
            || value.peers.iter().any(|p| {
                !valid_id(&p.request_id)
                    || p.scope != "product-management"
                    || p.client_name.len() > 80
                    || p.client_name.chars().any(char::is_control)
            })
        {
            return Err(Error::uncertain("owner_connections_identity_mismatch"));
        }
        Ok(value)
    }
    /// Record the exact attempt before dispatch. Repeated calls only observe its durable receipt.
    pub fn owner_action(
        &self,
        id: &str,
        request: &OwnerActionRequest,
    ) -> Result<OwnerActionReceipt, Error> {
        validate_request(request)?;
        let profile = self.owner_get(id)?;
        let connection = self.owner_https(&profile)?;
        {
            let _guard = self.lock()?;
            let mut record = self.load_owner(id)?;
            if let Some(previous) = record
                .actions
                .iter()
                .find(|a| a.request.request_id == request.request_id)
            {
                if previous.request != *request {
                    return Err(Error::local("owner_request_identity_conflict"));
                }
                drop(_guard);
                return self.owner_action_status(id, &request.request_id);
            }
            if record.profile.posture != OwnerProfilePosture::Approved {
                return Err(Error::local("owner_administration_required"));
            }
            if record.actions.len() >= 256 {
                return Err(Error::local("owner_action_retention_capacity"));
            }
            record.actions.push(TrackedAction {
                request: request.clone(),
                complete: false,
            });
            record
                .profile
                .pending_requests
                .push(request.request_id.clone());
            self.save_owner(&record)?;
        }
        let body = serde_json::to_vec(request).map_err(|_| Error::local("invalid_owner_action"))?;
        let bytes = connection
            .owner_exchange("POST", "/v1/owner/actions", &body, true, self.timeout)
            .map_err(|error| error.correlated(&request.request_id))?;
        self.record_owner_action(id, &request.request_id, &bytes)
    }
    pub fn owner_action_status(
        &self,
        id: &str,
        request_id: &str,
    ) -> Result<OwnerActionReceipt, Error> {
        if !valid_id(request_id) {
            return Err(Error::local("invalid_owner_request_identity"));
        }
        let profile = self.owner_get(id)?;
        let bytes = self
            .owner_https(&profile)?
            .owner_exchange(
                "GET",
                &format!("/v1/owner/actions/{request_id}"),
                &[],
                true,
                self.timeout,
            )
            .map_err(|error| error.correlated(request_id))?;
        self.record_owner_action(id, request_id, &bytes)
    }
    /// Forgetting a native credential is distinct from producer revocation.
    pub fn forget_owner(&self, id: &str) -> Result<(), Error> {
        let _guard = self.lock()?;
        self.load_owner(id)?;
        self.store.delete(&format!("owner:{id}"))?;
        self.store.delete(&format!("owner-bootstrap:{id}"))?;
        fs::remove_file(self.owner_path(id)?)
            .map_err(|_| Error::local("profile_storage_unavailable"))
    }
    fn record_owner(&self, id: &str, bytes: &[u8]) -> Result<OwnerObservation, Error> {
        let observation: OwnerObservation = serde_json::from_slice(bytes)
            .map_err(|_| Error::uncertain("invalid_owner_observation"))?;
        let _guard = self.lock()?;
        let mut record = self.load_owner(id)?;
        if observation.schema != "yvex.management.owner.v1"
            || observation.scope != SCOPE
            || observation.owner_ref != owner_ref(&record.profile)?
            || observation.client_name != record.profile.client_name
        {
            return Err(Error::uncertain("owner_observation_identity_mismatch"));
        }
        record.profile.posture = match observation.posture {
            OwnerPosture::Approved => OwnerProfilePosture::Approved,
            OwnerPosture::Revoked => OwnerProfilePosture::Revoked,
        };
        self.save_owner(&record)?;
        // The one-use invitation is no longer required for recovery after a canonical observation.
        let _ = self.store.delete(&format!("owner-bootstrap:{id}"));
        Ok(observation)
    }
    fn record_owner_action(
        &self,
        id: &str,
        request_id: &str,
        bytes: &[u8],
    ) -> Result<OwnerActionReceipt, Error> {
        let value: OwnerActionReceipt =
            serde_json::from_slice(bytes).map_err(|_| Error::uncertain("invalid_owner_receipt"))?;
        let _guard = self.lock()?;
        let mut record = self.load_owner(id)?;
        if value.schema != "yvex.management.owner.action-receipt.v1"
            || value.scope != SCOPE
            || value.owner_ref != owner_ref(&record.profile)?
            || value.request_id != request_id
            || value.reason.as_ref().is_some_and(|v| {
                v.len() > 128 || !v.bytes().all(|b| b.is_ascii_lowercase() || b == b'_')
            })
        {
            return Err(Error::uncertain("owner_receipt_identity_mismatch"));
        }
        let action = record
            .actions
            .iter_mut()
            .find(|a| a.request.request_id == request_id)
            .ok_or_else(|| Error::local("owner_request_not_retained"))?;
        if action.request.action != value.action || action.request.target_ref != value.target_ref {
            return Err(Error::uncertain("owner_receipt_action_mismatch"));
        }
        if (value.posture == OwnerActionPosture::Applied
            && (value.reason.is_some()
                || action.request.expected_revision.checked_add(1) != Some(value.revision)))
            || (value.posture == OwnerActionPosture::Refused && value.reason.is_none())
        {
            return Err(Error::uncertain("invalid_owner_receipt_posture"));
        }
        action.complete = true;
        record.profile.pending_requests.retain(|v| v != request_id);
        if value.posture == OwnerActionPosture::Applied
            && value.action == OwnerActionKind::OwnerRevoke
            && value.target_ref.as_deref() == Some(owner_ref(&record.profile)?)
        {
            record.profile.posture = OwnerProfilePosture::Revoked;
        }
        self.save_owner(&record)?;
        Ok(value)
    }
    fn owner_https(&self, profile: &OwnerProfile) -> Result<HttpsConnection, Error> {
        let bytes = self.store.get(&format!("owner:{}", profile.profile_ref))?;
        let mut raw = Zeroizing::new([0u8; 32]);
        if bytes.len() != 32 {
            return Err(Error::local("credential_store_invalid"));
        }
        raw.copy_from_slice(&bytes);
        let connection = HttpsConnection::new(
            &profile.endpoint,
            &profile.device_identity,
            Credential::from_bytes(*raw),
        )?;
        if connection.peer_identity() != profile.peer_identity {
            return Err(Error::local("credential_identity_mismatch"));
        }
        Ok(connection)
    }
    fn owner_path(&self, id: &str) -> Result<PathBuf, Error> {
        if !valid_id(id) {
            return Err(Error::local("invalid_owner_profile_ref"));
        }
        Ok(self.root.join(format!("{id}.owner.json")))
    }
    fn load_owner(&self, id: &str) -> Result<OwnerRecord, Error> {
        let path = self.owner_path(id)?;
        safe_file(&path)?;
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
        let mut bytes = vec![];
        file.take(131073)
            .read_to_end(&mut bytes)
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        if bytes.len() > 131072 {
            return Err(Error::local("owner_profile_bound"));
        }
        let record: OwnerRecord =
            serde_json::from_slice(&bytes).map_err(|_| Error::local("invalid_owner_profile"))?;
        if record.profile.profile_ref != id
            || record.actions.len() > 256
            || record.profile.pending_requests.len() > 256
            || record.profile.client_name.trim().is_empty()
            || record.profile.client_name.len() > 80
            || record.profile.client_name.chars().any(char::is_control)
            || record
                .actions
                .iter()
                .any(|a| validate_request(&a.request).is_err())
        {
            return Err(Error::local("owner_profile_identity_mismatch"));
        }
        let pending: Vec<_> = record
            .actions
            .iter()
            .filter(|a| !a.complete)
            .map(|a| a.request.request_id.clone())
            .collect();
        let unique: std::collections::HashSet<_> = record
            .actions
            .iter()
            .map(|a| &a.request.request_id)
            .collect();
        if pending != record.profile.pending_requests || unique.len() != record.actions.len() {
            return Err(Error::local("owner_profile_request_mismatch"));
        }
        network::pin(&record.profile.device_identity)?;
        owner_ref(&record.profile)?;
        Ok(record)
    }
    fn save_owner(&self, record: &OwnerRecord) -> Result<(), Error> {
        let bytes =
            serde_json::to_vec(record).map_err(|_| Error::local("invalid_owner_profile"))?;
        if bytes.len() > 131072 {
            return Err(Error::local("owner_profile_bound"));
        }
        let mut file = tempfile::NamedTempFile::new_in(&self.root)
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        file.write_all(&bytes)
            .and_then(|_| file.as_file().sync_all())
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        file.persist(self.owner_path(&record.profile.profile_ref)?)
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        fs::File::open(&self.root)
            .and_then(|file| file.sync_all())
            .map_err(|_| Error::local("profile_storage_unavailable"))?;
        Ok(())
    }
}
fn owner_ref(profile: &OwnerProfile) -> Result<&str, Error> {
    profile
        .peer_identity
        .strip_prefix("credential-sha256:")
        .filter(|id| valid_id(id))
        .ok_or_else(|| Error::local("invalid_owner_identity"))
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    use std::{collections::HashMap, os::unix::fs::PermissionsExt, sync::Mutex};
    #[derive(Default)]
    struct MemoryStore(Mutex<HashMap<String, Vec<u8>>>);
    impl CredentialStore for MemoryStore {
        fn put(&self, key: &str, value: &[u8]) -> Result<(), Error> {
            self.0.lock().unwrap().insert(key.into(), value.into());
            Ok(())
        }
        fn get(&self, key: &str) -> Result<Zeroizing<Vec<u8>>, Error> {
            self.0
                .lock()
                .unwrap()
                .get(key)
                .cloned()
                .map(Zeroizing::new)
                .ok_or_else(|| Error::local("test_credential_missing"))
        }
        fn delete(&self, key: &str) -> Result<(), Error> {
            self.0.lock().unwrap().remove(key);
            Ok(())
        }
    }
    fn invite(path: &Path) -> String {
        let secret = "42".repeat(32);
        let bytes = serde_json::to_vec(&serde_json::json!({
            "schema":"yvex.management.owner.invitation.v1", "endpoint":"https://127.0.0.1:1",
            "device_identity":format!("tls-sha256:{}", "a".repeat(64)),
            "invitation_id":Credential::from_bytes([0x42;32]).hash(),
            "invitation_secret":secret, "expires_at_unix_ms":now()+60000
        }))
        .unwrap();
        fs::write(path, bytes).unwrap();
        fs::set_permissions(path, fs::Permissions::from_mode(0o600)).unwrap();
        secret
    }
    #[test]
    fn native_import_keeps_owner_secrets_out_of_profiles_and_separate_from_product_clients() {
        let temp = tempfile::tempdir().unwrap();
        let path = temp.path().join("invite.json");
        let secret = invite(&path);
        let store = Arc::new(MemoryStore::default());
        let manager =
            ConnectionManager::with_store(temp.path().join("profiles"), store.clone()).unwrap();
        let preview = ConnectionManager::inspect_owner_invitation(&path).unwrap();
        assert!(!serde_json::to_string(&preview).unwrap().contains(&secret));
        let owner = manager.prepare_owner(&path, "Fixture").unwrap();
        assert_eq!(owner.posture, OwnerProfilePosture::Prepared);
        assert_eq!(manager.owner_list().unwrap(), vec![owner.clone()]);
        assert!(manager.list().unwrap().is_empty());
        assert!(manager.client(&owner.profile_ref).is_err());
        assert_eq!(store.0.lock().unwrap().len(), 2);
        for entry in fs::read_dir(&manager.root).unwrap() {
            let bytes = fs::read(entry.unwrap().path()).unwrap();
            assert!(!String::from_utf8_lossy(&bytes).contains(&secret));
        }
        let restored = ConnectionManager::with_store(manager.root.clone(), store.clone()).unwrap();
        assert_eq!(restored.owner_get(&owner.profile_ref).unwrap(), owner);
        restored.forget_owner(&owner.profile_ref).unwrap();
        assert!(store.0.lock().unwrap().is_empty());
    }
    #[test]
    fn invitation_integrity_expiry_permissions_and_invalid_action_refuse_locally() {
        let temp = tempfile::tempdir().unwrap();
        let path = temp.path().join("invite.json");
        invite(&path);
        fs::set_permissions(&path, fs::Permissions::from_mode(0o644)).unwrap();
        assert!(ConnectionManager::inspect_owner_invitation(&path).is_err());
        fs::set_permissions(&path, fs::Permissions::from_mode(0o600)).unwrap();
        let mut value: serde_json::Value =
            serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        value["invitation_id"] = serde_json::json!("0".repeat(64));
        fs::write(&path, value.to_string()).unwrap();
        assert_eq!(
            ConnectionManager::inspect_owner_invitation(&path)
                .unwrap_err()
                .code,
            "owner_invitation_identity_mismatch"
        );
        invite(&path);
        let mut value: serde_json::Value =
            serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        value["expires_at_unix_ms"] = serde_json::json!(1);
        fs::write(&path, value.to_string()).unwrap();
        assert_eq!(
            ConnectionManager::inspect_owner_invitation(&path)
                .unwrap_err()
                .code,
            "invalid_or_expired_owner_invitation"
        );
        assert!(OwnerActionRequest::new(1, OwnerActionKind::PairingApprove, None).is_err());
        assert!(
            OwnerActionRequest::new(1, OwnerActionKind::PairingOpen, Some("a".repeat(64))).is_err()
        );
    }
    #[test]
    fn incomplete_owner_action_cannot_be_redispatched_or_reassigned_after_restart() {
        let temp = tempfile::tempdir().unwrap();
        let path = temp.path().join("invite.json");
        invite(&path);
        let manager = ConnectionManager::with_store(
            temp.path().join("profiles"),
            Arc::new(MemoryStore::default()),
        )
        .unwrap();
        let profile = manager.prepare_owner(&path, "Fixture").unwrap();
        let mut record = manager.load_owner(&profile.profile_ref).unwrap();
        record.profile.posture = OwnerProfilePosture::Approved;
        let request = OwnerActionRequest::new(4, OwnerActionKind::PairingOpen, None).unwrap();
        record.actions.push(TrackedAction {
            request: request.clone(),
            complete: false,
        });
        record
            .profile
            .pending_requests
            .push(request.request_id.clone());
        manager.save_owner(&record).unwrap();
        let mut conflict = request.clone();
        conflict.expected_revision = 5;
        let error = manager
            .owner_action(&profile.profile_ref, &conflict)
            .unwrap_err();
        assert_eq!(error.code, "owner_request_identity_conflict");
        assert_eq!(
            error.dispatch_state,
            crate::management::DispatchState::NotDispatched
        );
        assert_eq!(
            manager
                .owner_get(&profile.profile_ref)
                .unwrap()
                .pending_requests,
            vec![request.request_id]
        );
    }
    #[test]
    fn lost_claim_and_action_responses_reobserve_exact_identity_without_second_post() {
        use std::{net::TcpListener, thread};
        let cert = rcgen::generate_simple_self_signed(vec!["localhost".into()]).unwrap();
        let pin = format!("tls-sha256:{:x}", Sha256::digest(cert.cert.der()));
        let config = rustls::ServerConfig::builder_with_provider(Arc::new(
            rustls::crypto::ring::default_provider(),
        ))
        .with_safe_default_protocol_versions()
        .unwrap()
        .with_no_client_auth()
        .with_single_cert(
            vec![cert.cert.der().clone()],
            rustls::pki_types::PrivatePkcs8KeyDer::from(cert.key_pair.serialize_der()).into(),
        )
        .unwrap();
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let endpoint = format!("https://{}", listener.local_addr().unwrap());
        let server = thread::spawn(move || {
            let config = Arc::new(config);
            let mut owner = String::new();
            let mut receipt = serde_json::Value::Null;
            for step in 0..4 {
                let (mut socket, _) = listener.accept().unwrap();
                socket
                    .set_read_timeout(Some(Duration::from_secs(3)))
                    .unwrap();
                let mut connection = rustls::ServerConnection::new(config.clone()).unwrap();
                let mut stream = rustls::Stream::new(&mut connection, &mut socket);
                let mut header = vec![];
                while !header.ends_with(b"\r\n\r\n") {
                    let mut byte = [0];
                    stream.read_exact(&mut byte).unwrap();
                    header.push(byte[0]);
                    assert!(header.len() < 16384);
                }
                let header = String::from_utf8(header).unwrap();
                let length: usize = header
                    .lines()
                    .find_map(|l| l.strip_prefix("Content-Length: "))
                    .unwrap()
                    .parse()
                    .unwrap();
                let mut body = vec![0; length];
                stream.read_exact(&mut body).unwrap();
                let response = match step {
                    0 => {
                        assert!(header.starts_with("POST /v1/owner/bootstrap "));
                        let request: serde_json::Value = serde_json::from_slice(&body).unwrap();
                        owner = request["credential_hash"].as_str().unwrap().into();
                        // Producer has committed claim; client receives an unusable response.
                        b"incomplete-response".to_vec()
                    }
                    1 => {
                        assert!(header.starts_with("GET /v1/owner/status "));
                        serde_json::to_vec(&serde_json::json!({"schema":"yvex.management.owner.v1",
                            "scope":SCOPE,"owner_ref":owner,"client_name":"Fixture",
                            "posture":"approved","revision":1}))
                        .unwrap()
                    }
                    2 => {
                        assert!(header.starts_with("POST /v1/owner/actions "));
                        let request: OwnerActionRequest = serde_json::from_slice(&body).unwrap();
                        receipt = serde_json::json!({"schema":"yvex.management.owner.action-receipt.v1",
                            "scope":SCOPE,"owner_ref":owner,"request_id":request.request_id,
                            "action":request.action,"target_ref":request.target_ref,"revision":2,
                            "posture":"applied","reason":null});
                        continue; // Connection lost after the owner applied the action.
                    }
                    _ => {
                        assert!(header.starts_with(&format!(
                            "GET /v1/owner/actions/{} ",
                            receipt["request_id"].as_str().unwrap()
                        )));
                        serde_json::to_vec(&receipt).unwrap()
                    }
                };
                let header = format!(
                    "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                    response.len()
                );
                stream.write_all(header.as_bytes()).unwrap();
                stream.write_all(&response).unwrap();
                stream.flush().unwrap();
            }
        });
        let temp = tempfile::tempdir().unwrap();
        let path = temp.path().join("invite.json");
        invite(&path);
        let mut value: serde_json::Value =
            serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
        value["endpoint"] = serde_json::json!(endpoint);
        value["device_identity"] = serde_json::json!(pin);
        fs::write(&path, value.to_string()).unwrap();
        let store = Arc::new(MemoryStore::default());
        let manager =
            ConnectionManager::with_store(temp.path().join("profiles"), store.clone()).unwrap();
        let profile = manager.prepare_owner(&path, "Fixture").unwrap();
        assert!(manager.claim_owner(&profile.profile_ref).is_err());
        assert_eq!(
            manager.owner_get(&profile.profile_ref).unwrap().posture,
            OwnerProfilePosture::Claiming
        );
        let restored = ConnectionManager::with_store(manager.root.clone(), store).unwrap();
        assert_eq!(
            restored.claim_owner(&profile.profile_ref).unwrap().posture,
            OwnerPosture::Approved
        );
        let request = OwnerActionRequest::new(1, OwnerActionKind::PairingOpen, None).unwrap();
        assert!(restored
            .owner_action(&profile.profile_ref, &request)
            .is_err());
        assert_eq!(
            restored
                .owner_get(&profile.profile_ref)
                .unwrap()
                .pending_requests,
            vec![request.request_id.clone()]
        );
        assert_eq!(
            restored
                .owner_action(&profile.profile_ref, &request)
                .unwrap()
                .posture,
            OwnerActionPosture::Applied
        );
        assert!(restored
            .owner_get(&profile.profile_ref)
            .unwrap()
            .pending_requests
            .is_empty());
        server.join().unwrap();
    }
}
