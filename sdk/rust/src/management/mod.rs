//! Identity-bound public product management. Mutations are prepared before
//! dispatch and return producer-owned Jobs. Transport loss never retries work.
pub mod connections;
pub mod discovery;
pub mod local;
pub mod network;
pub mod read_state;
mod network_types;
mod types;
pub use types::*;

use crate::{ssh, SshConnection};
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use serde_json::Value;
use std::{marker::PhantomData, sync::atomic::AtomicBool, time::Duration};

pub const REQUEST_SCHEMA: &str = "yvex.management.request.v2";
pub const RESPONSE_SCHEMA: &str = "yvex.management.response.v2";
pub const GRANT: &str = "product-management";
pub const MAX_REQUEST_BYTES: usize = 128 * 1024;
pub const MAX_RESPONSE_BYTES: usize = 1024 * 1024;

pub trait Operation {
    type Input: Serialize;
    type Output: DeserializeOwned;
    const ID: &'static str;
    const KIND: OperationKind;
}

/// Retain this identity before calling the transport. Reobserve job.get after
/// uncertain delivery; SDK never automatically dispatches this value again.
#[derive(Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Invocation {
    pub schema: String,
    pub request_id: String,
    pub operation: String,
    pub input: Value,
}
impl Invocation {
    pub fn new(operation: &str, input: Value) -> Result<Self, Error> {
        let mut random = [0_u8; 32];
        getrandom::fill(&mut random).map_err(|_| Error::local("random_unavailable"))?;
        let request = Self {
            schema: REQUEST_SCHEMA.into(),
            request_id: random.iter().map(|b| format!("{b:02x}")).collect(),
            operation: operation.into(),
            input,
        };
        request.encoded()?;
        Ok(request)
    }
    pub fn prepare<O: Operation>(input: &O::Input) -> Result<Prepared<O>, Error> {
        let input = serde_json::to_value(input).map_err(|_| Error::local("invalid_input"))?;
        Ok(Prepared {
            invocation: Self::new(O::ID, input)?,
            output: PhantomData,
        })
    }
    fn encoded(&self) -> Result<Vec<u8>, Error> {
        if self.schema != REQUEST_SCHEMA
            || !valid_id(&self.request_id)
            || !OPERATIONS.iter().any(|(op, _)| *op == self.operation)
            || !self.input.is_object()
        {
            return Err(Error::local("invalid_request"));
        }
        let mut bytes = serde_json::to_vec(self).map_err(|_| Error::local("invalid_request"))?;
        bytes.push(b'\n');
        if bytes.len() > MAX_REQUEST_BYTES {
            return Err(Error::local("request_too_large"));
        }
        types::validate_input(&self.operation, &self.input)
            .map_err(|_| Error::local("invalid_input"))?;
        Ok(bytes)
    }
    pub fn kind(&self) -> Result<OperationKind, Error> {
        OPERATIONS
            .iter()
            .find(|(op, _)| *op == self.operation)
            .map(|(_, kind)| *kind)
            .ok_or_else(|| Error::local("unsupported_operation"))
    }
}

pub struct Prepared<O: Operation> {
    invocation: Invocation,
    output: PhantomData<O>,
}
impl<O: Operation> Prepared<O> {
    pub fn invocation(&self) -> &Invocation {
        &self.invocation
    }
    pub fn request_id(&self) -> &str {
        &self.invocation.request_id
    }
}

#[derive(Clone, Serialize, Deserialize)]
pub struct Observation<T> {
    pub request_id: String,
    pub device_identity: String,
    pub authenticated_peer: String,
    pub value: T,
}

/// Safe diagnostics contain no prompt, credential, raw response or peer prose.
/// The producer's reason code is available explicitly, not rendered by Display.
#[derive(Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Error {
    pub code: String,
    pub dispatch_state: DispatchState,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reason: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub request_id: Option<String>,
}
impl Error {
    fn local(code: &str) -> Self {
        Self {
            code: code.into(),
            dispatch_state: DispatchState::NotDispatched,
            reason: None,
            request_id: None,
        }
    }
    fn uncertain(code: &str) -> Self {
        Self {
            dispatch_state: DispatchState::OutcomeUnavailable,
            ..Self::local(code)
        }
    }
    fn correlated(mut self, id: &str) -> Self {
        self.request_id = Some(id.into());
        self
    }
}
impl std::fmt::Debug for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ManagementError")
            .field("code", &self.code)
            .field("dispatch_state", &self.dispatch_state)
            .finish()
    }
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "YVEX product management: {} ({:?})",
            self.code, self.dispatch_state
        )
    }
}
impl std::error::Error for Error {}

#[derive(Deserialize)]
struct Response {
    schema: String,
    request_id: Option<String>,
    device_identity: Option<String>,
    authenticated_peer: Option<String>,
    status: ResponseStatus,
    data: Option<Value>,
    reason: Option<String>,
}

enum Transport {
    Ssh(SshConnection),
    Https(network::HttpsConnection),
    Local(local::LocalConnection),
}
pub struct Client {
    connection: Transport,
    timeout: Duration,
}
impl Client {
    pub fn new(connection: SshConnection) -> Result<Self, Error> {
        connection
            .validate()
            .map_err(|_| Error::local("invalid_connection"))?;
        Ok(Self {
            connection: Transport::Ssh(connection),
            timeout: Duration::from_secs(15),
        })
    }
    pub fn https(connection: network::HttpsConnection) -> Self {
        Self {
            connection: Transport::Https(connection),
            timeout: Duration::from_secs(15),
        }
    }
    pub fn local(connection: local::LocalConnection) -> Self {
        Self {
            connection: Transport::Local(connection),
            timeout: Duration::from_secs(15),
        }
    }
    fn identities(&self) -> (&str, &str) {
        match &self.connection {
            Transport::Ssh(c) => (&c.expected_device_identity, &c.expected_peer_identity),
            Transport::Https(c) => (c.device_identity(), c.peer_identity()),
            Transport::Local(c) => (c.device_identity(), c.peer_identity()),
        }
    }
    pub fn with_timeout(mut self, timeout: Duration) -> Result<Self, Error> {
        if timeout.is_zero() || timeout > Duration::from_secs(120) {
            return Err(Error::local("invalid_timeout"));
        }
        self.timeout = timeout;
        Ok(self)
    }
    pub fn prepare<O: Operation>(&self, input: &O::Input) -> Result<Prepared<O>, Error> {
        Invocation::prepare::<O>(input)
    }
    pub fn execute<O: Operation>(
        &self,
        request: &Prepared<O>,
    ) -> Result<Observation<O::Output>, Error> {
        let observation = self.invoke(&request.invocation)?;
        let value = serde_json::from_value(observation.value)
            .map_err(|_| Error::uncertain("invalid_projection").correlated(request.request_id()))?;
        Ok(Observation {
            request_id: observation.request_id,
            device_identity: observation.device_identity,
            authenticated_peer: observation.authenticated_peer,
            value,
        })
    }
    /// Convenience only for declared reads. Every operational submission must
    /// first expose its exact invocation identity through Invocation::prepare.
    pub fn read<O: Operation>(&self, input: &O::Input) -> Result<Observation<O::Output>, Error> {
        if O::KIND != OperationKind::Read {
            return Err(Error::local("explicit_mutation_identity_required"));
        }
        self.execute(&Invocation::prepare::<O>(input)?)
    }
    pub fn invoke(&self, request: &Invocation) -> Result<Observation<Value>, Error> {
        self.invoke_cancellable(request, &AtomicBool::new(false))
    }
    /// Local transport cancellation is not producer job cancellation.
    pub fn invoke_cancellable(
        &self,
        request: &Invocation,
        cancel: &AtomicBool,
    ) -> Result<Observation<Value>, Error> {
        let bytes = request.encoded()?;
        let output = match &self.connection {
            Transport::Https(c) => c.invoke(&bytes, self.timeout, cancel),
            Transport::Local(c) => c.invoke(&bytes, self.timeout, cancel),
            Transport::Ssh(connection) => {
                ssh::invoke(connection, bytes, self.timeout, cancel, MAX_RESPONSE_BYTES).map_err(
                    |failure| {
                        let code = match failure.kind {
                            ssh::FailureKind::Unavailable => "transport_unavailable",
                            ssh::FailureKind::Timeout => "timeout",
                            ssh::FailureKind::Cancelled => "transport_cancelled",
                            ssh::FailureKind::ResponseTooLarge => "response_too_large",
                        };
                        (if failure.started {
                            Error::uncertain(code)
                        } else {
                            Error::local(code)
                        })
                        .correlated(&request.request_id)
                    },
                )
            }
        }
        .map_err(|e| e.correlated(&request.request_id))?;
        self.decode(&output, request)
            .map_err(|e| e.correlated(&request.request_id))
    }
    fn decode(&self, bytes: &[u8], request: &Invocation) -> Result<Observation<Value>, Error> {
        if bytes.len() > MAX_RESPONSE_BYTES {
            return Err(Error::uncertain("response_too_large"));
        }
        if matches!(self.connection, Transport::Ssh(_))
            && (bytes.last() != Some(&b'\n')
                || bytes[..bytes.len().saturating_sub(1)].contains(&b'\n'))
        {
            return Err(Error::uncertain("invalid_response"));
        }
        let r: Response =
            serde_json::from_slice(bytes).map_err(|_| Error::uncertain("invalid_response"))?;
        if r.schema != RESPONSE_SCHEMA {
            return Err(Error::uncertain("unsupported_contract"));
        }
        // Only two pre-admission refusals may omit payload correlation/identity.
        // The SSH handshake still pins the remote producer. They carry no data.
        if r.status == ResponseStatus::Refused
            && r.request_id.is_none()
            && r.device_identity.is_none()
            && r.authenticated_peer.is_none()
            && r.data.is_none()
            && matches!(
                r.reason.as_deref(),
                Some("malformed_request" | "peer_revoked_or_authority_unavailable")
            )
        {
            return Err(Error {
                reason: r.reason,
                ..Error::local("refused")
            });
        }
        if r.request_id.as_deref() != Some(&request.request_id) {
            return Err(Error::uncertain("correlation_mismatch"));
        }
        if r.device_identity.as_deref() != Some(self.identities().0)
            || r.authenticated_peer.as_deref() != Some(self.identities().1)
        {
            return Err(Error::uncertain("identity_mismatch"));
        }
        if r.status != ResponseStatus::Ok {
            if r.data.is_some() || r.reason.as_deref().is_none_or(str::is_empty) {
                return Err(Error::uncertain("invalid_response"));
            }
            let (code, state) = match r.status {
                ResponseStatus::Refused => ("refused", DispatchState::NotDispatched),
                ResponseStatus::Unsupported => ("unsupported", DispatchState::NotDispatched),
                _ => ("unavailable", DispatchState::OutcomeUnavailable),
            };
            return Err(Error {
                code: code.into(),
                dispatch_state: state,
                reason: r.reason,
                request_id: None,
            });
        }
        if r.reason.is_some() {
            return Err(Error::uncertain("invalid_response"));
        }
        let data = r.data.ok_or_else(|| Error::uncertain("invalid_response"))?;
        types::validate_projection(&request.operation, &data)
            .map_err(|_| Error::uncertain("invalid_projection"))?;
        if request.kind()? == OperationKind::Job {
            let job: Job = serde_json::from_value(data.clone())
                .map_err(|_| Error::uncertain("invalid_job"))?;
            if job.job_id != request.request_id || job.operation != request.operation {
                return Err(Error::uncertain("job_identity_mismatch"));
            }
        }
        if request.kind()? == OperationKind::Job || request.operation == "job.get" {
            let job: Job = serde_json::from_value(data.clone())
                .map_err(|_| Error::uncertain("invalid_job"))?;
            types::validate_job(&job).map_err(|_| Error::uncertain("invalid_job_projection"))?;
        }
        if request.operation == "job.get" && data.get("job_id") != request.input.get("job_id") {
            return Err(Error::uncertain("job_identity_mismatch"));
        }
        Ok(Observation {
            request_id: request.request_id.clone(),
            device_identity: r.device_identity.unwrap(),
            authenticated_peer: r.authenticated_peer.unwrap(),
            value: data,
        })
    }
}

fn valid_id(value: &str) -> bool {
    value.len() == 64
        && value
            .bytes()
            .all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase())
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    fn client() -> Client {
        Client::new(SshConnection {
            pinned_known_hosts: "/tmp/reviewed-pins".into(),
            enrolled_client_key: "/tmp/enrolled-key".into(),
            address: "example.test".into(),
            port: 2222,
            user: "yvex".into(),
            expected_device_identity: format!("ssh-ed25519:sha256:{}", "a".repeat(64)),
            expected_peer_identity: format!("ssh-ed25519:sha256:{}", "b".repeat(64)),
        })
        .unwrap()
    }
    fn response(request: &Invocation, data: Value) -> Value {
        json!({"schema":RESPONSE_SCHEMA,"request_id":request.request_id,
        "device_identity":client().identities().0,"authenticated_peer":client().identities().1,
        "status":"ok","data":data})
    }
    fn bytes(value: Value) -> Vec<u8> {
        let mut bytes = serde_json::to_vec(&value).unwrap();
        bytes.push(b'\n');
        bytes
    }
    fn job(request: &Invocation) -> Value {
        json!({"job_id":request.request_id,"operation":request.operation,"state":"running",
        "submitted_at_unix_ms":10,"updated_at_unix_ms":11,"input":request.input,"result":null,"reason":null})
    }
    #[test]
    fn typed_build_assessment_preserves_legacy_and_refuses_fabricated_evidence() {
        let legacy = json!({"schema":"yvex.model.prepare.v1", "model":"m", "state":"PLANNED", "changed":false});
        assert!(
            serde_json::from_value::<BuildResult>(legacy.clone())
                .unwrap()
                .planning
                .is_none()
        );
        let mut assessed = legacy;
        assessed["planning"] = json!({"schema":"yvex.build.planning.v1", "readiness":"blocked",
            "basis":"not_admitted", "reason":"missing input", "native_status":-10,
            "owner":"model.prepare", "execution_strategy":null,
            "runtime_admission":"not_evaluated", "quality_evidence":"not_evaluated",
            "performance_evidence":"not_evaluated"});
        let result = serde_json::from_value::<BuildResult>(assessed.clone()).unwrap();
        assert_eq!(
            result.planning.unwrap().readiness,
            ProfileReadiness::Blocked
        );
        assessed["planning"]["runtime_admission"] = json!("admitted");
        assert!(serde_json::from_value::<BuildResult>(assessed).is_err());
    }
    #[test]
    fn preparation_preserves_identity_and_fences_before_dispatch() {
        let input = EngineMutationInput {
            model: "exact-model".into(),
            generation: 7,
            host_instance: "c".repeat(64),
        };
        let prepared = Invocation::prepare::<operations::EngineUnload>(&input).unwrap();
        assert!(valid_id(prepared.request_id()));
        assert_eq!(prepared.invocation().input["generation"], 7);
        assert_eq!(prepared.invocation().input["host_instance"], "c".repeat(64));
        assert_eq!(
            prepared.invocation().encoded().unwrap(),
            prepared.invocation().encoded().unwrap()
        );
        assert!(client().read::<operations::EngineUnload>(&input).is_err());
        let mut altered = prepared.invocation().clone();
        altered.request_id = "foreign".into();
        assert_eq!(
            altered.encoded().unwrap_err().dispatch_state,
            DispatchState::NotDispatched
        );
    }
    #[test]
    fn mutation_receipt_requires_exact_job_identity() {
        let request = Invocation::new(
            "engine.load",
            json!({"profile":"model","host_instance":"c".repeat(64)}),
        )
        .unwrap();
        let r = response(&request, job(&request));
        let observation = client().decode(&bytes(r.clone()), &request).unwrap();
        assert_eq!(observation.value["state"], "running");
        for field in ["job_id", "operation"] {
            let mut wrong = r.clone();
            wrong["data"][field] = json!("foreign");
            let error = client().decode(&bytes(wrong), &request).err().unwrap();
            assert_eq!(error.code, "job_identity_mismatch");
            assert_eq!(error.dispatch_state, DispatchState::OutcomeUnavailable);
        }
    }
    #[test]
    fn refusal_unavailability_and_unsupported_remain_distinct() {
        let request = Invocation::new(
            "engine.load",
            json!({"profile":"test","host_instance":"c".repeat(64)}),
        )
        .unwrap();
        for (status, state) in [
            ("refused", DispatchState::NotDispatched),
            ("unsupported", DispatchState::NotDispatched),
            ("unavailable", DispatchState::OutcomeUnavailable),
        ] {
            let mut r = response(&request, Value::Null);
            r["status"] = json!(status);
            r["reason"] = json!("owner_reason");
            let error = client().decode(&bytes(r), &request).err().unwrap();
            assert_eq!(error.code, status);
            assert_eq!(error.dispatch_state, state);
            assert_eq!(error.reason.as_deref(), Some("owner_reason"));
        }
    }
    #[test]
    fn foreign_refusals_never_prove_non_dispatch() {
        let request = Invocation::new(
            "engine.load",
            json!({"profile":"test","host_instance":"c".repeat(64)}),
        )
        .unwrap();
        for field in ["request_id", "device_identity", "authenticated_peer"] {
            let mut r = response(&request, Value::Null);
            r["status"] = json!("refused");
            r["reason"] = json!("stale_host");
            r[field] = json!("foreign");
            assert_eq!(
                client()
                    .decode(&bytes(r), &request)
                    .err()
                    .unwrap()
                    .dispatch_state,
                DispatchState::OutcomeUnavailable
            );
        }
    }
    #[test]
    fn loss_is_not_a_success_or_a_retry() {
        let request = Invocation::new(
            "engine.load",
            json!({"profile":"test","host_instance":"c".repeat(64)}),
        )
        .unwrap();
        for payload in [
            vec![],
            b"{}\n{}\n".to_vec(),
            b"{}".to_vec(),
            vec![b' '; MAX_RESPONSE_BYTES + 1],
        ] {
            assert_eq!(
                client()
                    .decode(&payload, &request)
                    .err()
                    .unwrap()
                    .dispatch_state,
                DispatchState::OutcomeUnavailable
            );
        }
        let cancelled = client()
            .invoke_cancellable(&request, &AtomicBool::new(true))
            .err()
            .unwrap();
        assert_eq!(cancelled.dispatch_state, DispatchState::NotDispatched);
        assert_eq!(
            cancelled.request_id.as_deref(),
            Some(request.request_id.as_str())
        );
    }
    #[test]
    fn pre_admission_refusals_are_bounded_and_redacted() {
        let request = Invocation::new("job.list", json!({})).unwrap();
        let refused = json!({"schema":RESPONSE_SCHEMA,"request_id":null,"device_identity":null,"authenticated_peer":null,
            "status":"refused","reason":"peer_revoked_or_authority_unavailable"});
        assert_eq!(
            client()
                .decode(&bytes(refused.clone()), &request)
                .err()
                .unwrap()
                .dispatch_state,
            DispatchState::NotDispatched
        );
        let mut bad = refused;
        bad["data"] = json!({"host":"secret"});
        assert_eq!(
            client()
                .decode(&bytes(bad), &request)
                .err()
                .unwrap()
                .dispatch_state,
            DispatchState::OutcomeUnavailable
        );
        let error = Error {
            reason: Some("sensitive untrusted producer prose".into()),
            ..Error::local("refused")
        };
        assert!(!format!("{error:?} {error}").contains("sensitive"));
    }
    #[test]
    fn sdk_inputs_refuse_tokens_and_missing_generation_fences() {
        assert!(Invocation::new(
            "acquisition.start",
            json!({
                "repository":"a/b","revision":"abc","representation":"source","token":"secret"
            })
        )
        .is_err());
        assert!(Invocation::new("generation.start", json!({"prompt":"secret"})).is_err());
        assert!(serde_json::from_value::<AcquisitionInput>(
            json!({"repository":"a/b","revision":"abc","representation":"source","token":"secret"})
        )
        .is_err());
        assert!(serde_json::from_value::<SessionMutationInput>(
            json!({"model":"m","generation":1,"name":"s"})
        )
        .is_err());
        let enormous = Invocation::new(
            "generation.start",
            json!({"prompt":"x".repeat(MAX_REQUEST_BYTES)}),
        );
        assert_eq!(enormous.err().unwrap().code, "request_too_large");
    }
    #[test]
    fn unknown_progress_is_not_zero_and_extra_producer_facts_survive() {
        let session: Session = serde_json::from_value(
            json!({"name":"s","identity":"s1","state":"ready","position":10,"turns":1,
            "context_used":null,"kv_used_bytes":null,"producer_fact":42}),
        )
        .unwrap();
        assert!(session.context_used.is_none());
        assert_eq!(session.additional["producer_fact"], 42);
        assert!(serde_json::from_value::<Job>(json!({"state":"new_unknown_state"})).is_err());
    }
}
