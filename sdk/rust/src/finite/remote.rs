//! Public remote finite-decision v1 over an explicitly enrolled SSH compute
//! grant. No native library, private Unix wire, lifecycle mutation or retry.
use super::{CandidateScore, Error as FiniteError, Request, ResultObservation};
use crate::{ssh, SshConnection};
use serde::{Deserialize, Serialize};
use std::sync::atomic::AtomicBool;
use std::time::Duration;

pub const REQUEST_SCHEMA: &str = "yvex.finite.request.v1";
pub const RESPONSE_SCHEMA: &str = "yvex.finite.response.v1";
pub const OPERATION: &str = "finite.decision.execute";
pub const MAX_REQUEST_BYTES: usize = 32768;
/// Client receive bound, not a producer total-computation deadline.
pub const MAX_RESPONSE_BYTES: usize = 32768;

fn hex_identity(value: &str) -> bool {
    value.len() == 64
        && value
            .bytes()
            .all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase())
}

/// Exact independently admitted computational lineage. Alias/generation alone
/// do not identify a model across host or engine replacement. This is not a
/// qualification fact: the caller owns the evidence authorizing these values.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ProducerIdentity {
    pub source_identity: String,
    pub logical_model_identity: String,
    pub binding_identity: String,
    pub tokenizer_identity: String,
    pub physical_program_identity: String,
    pub input_policy_identity: String,
}
impl ProducerIdentity {
    pub fn validate(&self) -> Result<(), Error> {
        if [
            &self.source_identity,
            &self.logical_model_identity,
            &self.binding_identity,
            &self.tokenizer_identity,
            &self.physical_program_identity,
            &self.input_policy_identity,
        ]
        .iter()
        .all(|value| hex_identity(value))
        {
            Ok(())
        } else {
            Err(Error::new(
                ErrorKind::InvalidConfiguration,
                DispatchState::NotDispatched,
            ))
        }
    }
    fn matches(&self, r: &ResultObservation) -> bool {
        self.source_identity == r.source_identity
            && self.logical_model_identity == r.logical_model_identity
            && self.binding_identity == r.binding_identity
            && self.tokenizer_identity == r.tokenizer_identity
            && self.physical_program_identity == r.physical_program_identity
            && self.input_policy_identity == r.input_policy_identity
    }
}

/// One fresh invocation. Correlation is inspectable before dispatch but is NOT
/// an idempotency key. Reusing this value may execute the producer again.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Invocation {
    pub schema: String,
    pub request_id: String,
    pub operation: String,
    pub input: Request,
}
impl Invocation {
    pub fn new(input: Request) -> Result<Self, Error> {
        input
            .validate()
            .map_err(|error| Error::new(ErrorKind::Input(error), DispatchState::NotDispatched))?;
        let mut bytes = [0u8; 32];
        getrandom::fill(&mut bytes)
            .map_err(|_| Error::new(ErrorKind::RandomUnavailable, DispatchState::NotDispatched))?;
        Ok(Self {
            schema: REQUEST_SCHEMA.into(),
            request_id: bytes.iter().map(|b| format!("{b:02x}")).collect(),
            operation: OPERATION.into(),
            input,
        })
    }
    fn encoded(&self) -> Result<Vec<u8>, Error> {
        if self.schema != REQUEST_SCHEMA
            || self.operation != OPERATION
            || !hex_identity(&self.request_id)
        {
            return Err(Error::new(
                ErrorKind::InvalidRequest,
                DispatchState::NotDispatched,
            ));
        }
        self.input
            .validate()
            .map_err(|e| Error::new(ErrorKind::Input(e), DispatchState::NotDispatched))?;
        let mut bytes = serde_json::to_vec(self)
            .map_err(|_| Error::new(ErrorKind::InvalidRequest, DispatchState::NotDispatched))?;
        bytes.push(b'\n');
        if bytes.len() > MAX_REQUEST_BYTES {
            return Err(Error::new(
                ErrorKind::InvalidRequest,
                DispatchState::NotDispatched,
            ));
        }
        Ok(bytes)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum DispatchState {
    NotDispatched,
    OutcomeUnavailable,
    Completed,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Observation {
    pub schema: String,
    pub request_id: String,
    pub operation: String,
    pub model_alias: String,
    pub device_identity: String,
    pub authenticated_peer: String,
    pub dispatch_state: DispatchState,
    pub result: ResultObservation,
}

#[derive(Clone, PartialEq, Eq)]
pub enum ErrorKind {
    InvalidConfiguration,
    InvalidRequest,
    RandomUnavailable,
    Input(FiniteError),
    TransportUnavailable,
    Timeout,
    Cancelled,
    ResponseTooLarge,
    InvalidResponse,
    CorrelationMismatch,
    PeerIdentityMismatch,
    ProducerIdentityMismatch,
    InvalidResult(FiniteError),
    Refused {
        reason: String,
    },
    ProducerError {
        code: i64,
        name: String,
        owner: String,
    },
}
impl std::fmt::Debug for ErrorKind {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // Never render untrusted/context-bearing producer strings in ordinary
        // diagnostics. Typed name/owner/reason remain explicitly inspectable.
        match self {
            Self::Refused { .. } => f.write_str("ProducerRefused"),
            Self::ProducerError { code, .. } => write!(f, "ProducerError({code})"),
            Self::Input(e) => write!(f, "Input({e})"),
            Self::InvalidResult(e) => write!(f, "InvalidResult({e})"),
            other => f.write_str(match other {
                Self::InvalidConfiguration => "InvalidConfiguration",
                Self::InvalidRequest => "InvalidRequest",
                Self::RandomUnavailable => "RandomUnavailable",
                Self::TransportUnavailable => "TransportUnavailable",
                Self::Timeout => "Timeout",
                Self::Cancelled => "Cancelled",
                Self::ResponseTooLarge => "ResponseTooLarge",
                Self::InvalidResponse => "InvalidResponse",
                Self::CorrelationMismatch => "CorrelationMismatch",
                Self::PeerIdentityMismatch => "PeerIdentityMismatch",
                Self::ProducerIdentityMismatch => "ProducerIdentityMismatch",
                _ => unreachable!(),
            }),
        }
    }
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    pub kind: ErrorKind,
    pub dispatch_state: DispatchState,
}
impl Error {
    fn new(kind: ErrorKind, dispatch_state: DispatchState) -> Self {
        Self {
            kind,
            dispatch_state,
        }
    }
    fn uncertain(kind: ErrorKind) -> Self {
        Self::new(kind, DispatchState::OutcomeUnavailable)
    }
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "YVEX remote finite client: {:?} ({:?})",
            self.kind, self.dispatch_state
        )
    }
}
impl std::error::Error for Error {}

#[derive(Clone)]
pub struct RemoteClient {
    connection: SshConnection,
    identity: ProducerIdentity,
    timeout: Duration,
}
impl RemoteClient {
    pub fn new(connection: SshConnection, identity: ProducerIdentity) -> Result<Self, Error> {
        connection.validate().map_err(|_| {
            Error::new(
                ErrorKind::InvalidConfiguration,
                DispatchState::NotDispatched,
            )
        })?;
        identity.validate()?;
        Ok(Self {
            connection,
            identity,
            timeout: Duration::from_secs(15),
        })
    }
    pub fn with_timeout(mut self, timeout: Duration) -> Result<Self, Error> {
        if timeout.is_zero() || timeout > Duration::from_secs(120) {
            return Err(Error::new(
                ErrorKind::InvalidConfiguration,
                DispatchState::NotDispatched,
            ));
        }
        self.timeout = timeout;
        Ok(self)
    }
    pub fn execute(&self, invocation: &Invocation) -> Result<Observation, Error> {
        self.execute_cancellable(invocation, &AtomicBool::new(false))
    }
    /// Cancellation bounds the local SSH call. It does not promise immediate
    /// retirement of already-admitted producer work. No automatic redispatch.
    pub fn execute_cancellable(
        &self,
        invocation: &Invocation,
        cancel: &AtomicBool,
    ) -> Result<Observation, Error> {
        let request = invocation.encoded()?;
        let bytes = ssh::invoke(
            &self.connection,
            request,
            self.timeout,
            cancel,
            MAX_RESPONSE_BYTES,
        )
        .map_err(|e| {
            Error::new(
                match e.kind {
                    ssh::FailureKind::Unavailable => ErrorKind::TransportUnavailable,
                    ssh::FailureKind::Timeout => ErrorKind::Timeout,
                    ssh::FailureKind::Cancelled => ErrorKind::Cancelled,
                    ssh::FailureKind::ResponseTooLarge => ErrorKind::ResponseTooLarge,
                },
                if e.started {
                    DispatchState::OutcomeUnavailable
                } else {
                    DispatchState::NotDispatched
                },
            )
        })?;
        self.decode(&bytes, invocation)
    }
    fn decode(&self, bytes: &[u8], invocation: &Invocation) -> Result<Observation, Error> {
        if bytes.len() > MAX_RESPONSE_BYTES {
            return Err(Error::uncertain(ErrorKind::ResponseTooLarge));
        }
        if bytes.last() != Some(&b'\n') || bytes[..bytes.len() - 1].contains(&b'\n') {
            return Err(Error::uncertain(ErrorKind::InvalidResponse));
        }
        let response: Response = serde_json::from_slice(bytes)
            .map_err(|_| Error::uncertain(ErrorKind::InvalidResponse))?;
        match response {
            Response::Refused {
                schema,
                request_id: (),
                dispatch_state,
                reason,
            } => {
                if schema != RESPONSE_SCHEMA
                    || dispatch_state != DispatchState::NotDispatched
                    || reason.is_empty()
                {
                    return Err(Error::uncertain(ErrorKind::InvalidResponse));
                }
                Err(Error::new(
                    ErrorKind::Refused { reason },
                    DispatchState::NotDispatched,
                ))
            }
            Response::Error {
                schema,
                request_id,
                operation,
                model_alias,
                device_identity,
                authenticated_peer,
                dispatch_state,
                reason,
                error,
            } => {
                self.check_envelope(
                    invocation,
                    &schema,
                    &request_id,
                    &operation,
                    &model_alias,
                    &device_identity,
                    &authenticated_peer,
                )?;
                if dispatch_state != DispatchState::OutcomeUnavailable
                    || reason != "producer_error"
                    || error.code >= 0
                {
                    return Err(Error::uncertain(ErrorKind::InvalidResponse));
                }
                Err(Error::uncertain(ErrorKind::ProducerError {
                    code: error.code,
                    name: error.name,
                    owner: error.owner,
                }))
            }
            Response::Ok {
                schema,
                request_id,
                operation,
                model_alias,
                device_identity,
                authenticated_peer,
                dispatch_state,
                result,
            } => {
                self.check_envelope(
                    invocation,
                    &schema,
                    &request_id,
                    &operation,
                    &model_alias,
                    &device_identity,
                    &authenticated_peer,
                )?;
                if dispatch_state != DispatchState::Completed
                    || result.score_kind != "model-logit"
                    || result.candidate_count != invocation.input.candidates.len() as u64
                {
                    return Err(Error::uncertain(ErrorKind::InvalidResponse));
                }
                let observed: ResultObservation = result.into();
                observed
                    .validate_against(&invocation.input)
                    .map_err(|e| Error::uncertain(ErrorKind::InvalidResult(e)))?;
                if !self.identity.matches(&observed) {
                    return Err(Error::uncertain(ErrorKind::ProducerIdentityMismatch));
                }
                Ok(Observation {
                    schema,
                    request_id,
                    operation,
                    model_alias,
                    device_identity,
                    authenticated_peer,
                    dispatch_state,
                    result: observed,
                })
            }
        }
    }
    #[allow(clippy::too_many_arguments)]
    fn check_envelope(
        &self,
        invocation: &Invocation,
        schema: &str,
        request_id: &str,
        operation: &str,
        model_alias: &str,
        device_identity: &str,
        authenticated_peer: &str,
    ) -> Result<(), Error> {
        if schema != RESPONSE_SCHEMA || operation != OPERATION {
            return Err(Error::uncertain(ErrorKind::InvalidResponse));
        }
        if request_id != invocation.request_id || model_alias != invocation.input.model_alias {
            return Err(Error::uncertain(ErrorKind::CorrelationMismatch));
        }
        if device_identity != self.connection.expected_device_identity
            || authenticated_peer != self.connection.expected_peer_identity
        {
            return Err(Error::uncertain(ErrorKind::PeerIdentityMismatch));
        }
        Ok(())
    }
}

#[derive(Deserialize)]
#[serde(tag = "status", deny_unknown_fields)]
enum Response {
    #[serde(rename = "refused")]
    Refused {
        schema: String,
        request_id: (),
        dispatch_state: DispatchState,
        reason: String,
    },
    #[serde(rename = "error")]
    Error {
        schema: String,
        request_id: String,
        operation: String,
        model_alias: String,
        device_identity: String,
        authenticated_peer: String,
        dispatch_state: DispatchState,
        reason: String,
        error: ProducerError,
    },
    #[serde(rename = "ok")]
    Ok {
        schema: String,
        request_id: String,
        operation: String,
        model_alias: String,
        device_identity: String,
        authenticated_peer: String,
        dispatch_state: DispatchState,
        result: WireResult,
    },
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct ProducerError {
    code: i64,
    name: String,
    owner: String,
}

// The remote producer adds score_kind/candidate_count to the existing C result
// projection. A closed wire record avoids flatten's ambiguous unknown-field
// handling; conversion retains the shared public computational result type.
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct WireResult {
    schema_version: u32,
    score_kind: String,
    engine_generation: u64,
    token_count: u64,
    candidate_count: u64,
    model_forward_count: u64,
    sampling_invocation_count: u64,
    generated_token_count: u64,
    resident_backbone_count: u64,
    elapsed_nanoseconds: u64,
    source_mapped_bytes: u64,
    parameter_execution_bytes: u64,
    workspace_host_bytes: u64,
    workspace_device_bytes: u64,
    source_identity: String,
    logical_model_identity: String,
    binding_identity: String,
    tokenizer_identity: String,
    physical_program_identity: String,
    input_policy_identity: String,
    input_identity: String,
    candidate_population_identity: String,
    result_identity: String,
    calibrated: bool,
    candidates: Vec<CandidateScore>,
}
impl From<WireResult> for ResultObservation {
    fn from(r: WireResult) -> Self {
        Self {
            schema_version: r.schema_version,
            engine_generation: r.engine_generation,
            token_count: r.token_count,
            model_forward_count: r.model_forward_count,
            sampling_invocation_count: r.sampling_invocation_count,
            generated_token_count: r.generated_token_count,
            resident_backbone_count: r.resident_backbone_count,
            elapsed_nanoseconds: r.elapsed_nanoseconds,
            source_mapped_bytes: r.source_mapped_bytes,
            parameter_execution_bytes: r.parameter_execution_bytes,
            workspace_host_bytes: r.workspace_host_bytes,
            workspace_device_bytes: r.workspace_device_bytes,
            source_identity: r.source_identity,
            logical_model_identity: r.logical_model_identity,
            binding_identity: r.binding_identity,
            tokenizer_identity: r.tokenizer_identity,
            physical_program_identity: r.physical_program_identity,
            input_policy_identity: r.input_policy_identity,
            input_identity: r.input_identity,
            candidate_population_identity: r.candidate_population_identity,
            result_identity: r.result_identity,
            calibrated: r.calibrated,
            candidates: r.candidates,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::super::Candidate;
    use super::*;
    fn client() -> RemoteClient {
        RemoteClient::new(
            SshConnection {
                pinned_known_hosts: "/tmp/not-used-pins".into(),
                enrolled_client_key: "/tmp/not-used-key".into(),
                address: "example.invalid".into(),
                port: 2222,
                user: "yvex".into(),
                expected_device_identity: format!("ssh-ed25519:sha256:{}", "a".repeat(64)),
                expected_peer_identity: format!("ssh-ed25519:sha256:{}", "b".repeat(64)),
            },
            ProducerIdentity {
                source_identity: "c".repeat(64),
                logical_model_identity: "d".repeat(64),
                binding_identity: "e".repeat(64),
                tokenizer_identity: "f".repeat(64),
                physical_program_identity: "1".repeat(64),
                input_policy_identity: "2".repeat(64),
            },
        )
        .unwrap()
    }
    fn invocation() -> Invocation {
        Invocation::new(Request {
            model_alias: "finite-fixture".into(),
            expected_generation: 7,
            question: "Finite choice".into(),
            context: "".into(),
            candidates: vec![
                Candidate {
                    id: "a".into(),
                    text: "one".into(),
                },
                Candidate {
                    id: "b".into(),
                    text: "two".into(),
                },
            ],
        })
        .unwrap()
    }
    fn success(c: &RemoteClient, r: &Invocation) -> serde_json::Value {
        serde_json::json!({ "schema": RESPONSE_SCHEMA, "request_id": r.request_id, "operation": OPERATION,
            "model_alias": r.input.model_alias, "device_identity": c.connection.expected_device_identity,
            "authenticated_peer": c.connection.expected_peer_identity, "status": "ok", "dispatch_state": "completed",
            "result": { "schema_version": 1, "score_kind": "model-logit", "engine_generation": 7,
                "token_count": 16, "candidate_count": 2, "model_forward_count": 1, "sampling_invocation_count": 0,
                "generated_token_count": 0, "resident_backbone_count": 1, "elapsed_nanoseconds": 1000000,
                "source_mapped_bytes": 128, "parameter_execution_bytes": 64, "workspace_host_bytes": 32, "workspace_device_bytes": 0,
                "source_identity": c.identity.source_identity, "logical_model_identity": c.identity.logical_model_identity,
                "binding_identity": c.identity.binding_identity, "tokenizer_identity": c.identity.tokenizer_identity,
                "physical_program_identity": c.identity.physical_program_identity, "input_policy_identity": c.identity.input_policy_identity,
                "input_identity": "3".repeat(64), "candidate_population_identity": "4".repeat(64), "result_identity": "5".repeat(64),
                "calibrated": false, "candidates": [ {"id":"a", "raw_score":-1.5, "relative_candidate_probability":0.2},
                    {"id":"b", "raw_score":0.5, "relative_candidate_probability":0.8}] } })
    }
    fn line(v: &serde_json::Value) -> Vec<u8> {
        let mut b = serde_json::to_vec(v).unwrap();
        b.push(b'\n');
        b
    }
    #[test]
    fn remote_contract_preserves_exact_uncalibrated_result_without_native_client() {
        let c = client();
        let r = invocation();
        let result = c.decode(&line(&success(&c, &r)), &r).unwrap();
        assert_eq!(result.request_id, r.request_id);
        assert_eq!(result.dispatch_state, DispatchState::Completed);
        assert_eq!(result.result.candidates[1].id, "b");
        assert_eq!(result.result.elapsed_nanoseconds, 1000000);
        assert!(!result.result.calibrated);
        let second = invocation();
        assert_ne!(r.request_id, second.request_id);
    }
    #[test]
    fn refuses_envelope_and_every_approved_lineage_mismatch() {
        let c = client();
        let r = invocation();
        let original = success(&c, &r);
        for field in [
            "request_id",
            "model_alias",
            "operation",
            "schema",
            "device_identity",
            "authenticated_peer",
        ] {
            let mut changed = original.clone();
            changed[field] = "foreign".into();
            let error = c.decode(&line(&changed), &r).unwrap_err();
            assert_eq!(
                error.dispatch_state,
                DispatchState::OutcomeUnavailable,
                "{field}"
            );
        }
        for field in [
            "source_identity",
            "logical_model_identity",
            "binding_identity",
            "tokenizer_identity",
            "physical_program_identity",
            "input_policy_identity",
        ] {
            let mut changed = original.clone();
            changed["result"][field] = "6".repeat(64).into();
            assert_eq!(
                c.decode(&line(&changed), &r).unwrap_err().kind,
                ErrorKind::ProducerIdentityMismatch,
                "{field}"
            );
        }
    }
    #[test]
    fn rejects_stale_foreign_reordered_and_unsupported_result_semantics() {
        let c = client();
        let r = invocation();
        let original = success(&c, &r);
        for (field, value) in [
            ("engine_generation", serde_json::json!(8)),
            ("candidate_count", serde_json::json!(1)),
            ("score_kind", serde_json::json!("confidence")),
            ("calibrated", serde_json::json!(true)),
            ("model_forward_count", serde_json::json!(2)),
            ("generated_token_count", serde_json::json!(1)),
            ("sampling_invocation_count", serde_json::json!(1)),
            ("resident_backbone_count", serde_json::json!(2)),
            ("result_identity", serde_json::json!("not-hex")),
        ] {
            let mut changed = original.clone();
            changed["result"][field] = value;
            assert!(c.decode(&line(&changed), &r).is_err(), "{field}");
        }
        let mut changed = original.clone();
        changed["result"]["candidates"][0]["id"] = "foreign".into();
        assert!(c.decode(&line(&changed), &r).is_err());
        changed = original.clone();
        changed["result"]["candidates"]
            .as_array_mut()
            .unwrap()
            .swap(0, 1);
        assert!(c.decode(&line(&changed), &r).is_err());
        for value in [
            serde_json::Value::Null,
            serde_json::json!(-0.1),
            serde_json::json!(1.1),
        ] {
            changed = original.clone();
            changed["result"]["candidates"][0]["relative_candidate_probability"] = value;
            assert!(c.decode(&line(&changed), &r).is_err());
        }
    }
    #[test]
    fn closes_wire_shapes_and_duplicate_fields() {
        let c = client();
        let r = invocation();
        let original = success(&c, &r);
        for pointer in ["", "/result", "/result/candidates/0"] {
            let mut changed = original.clone();
            changed.pointer_mut(pointer).unwrap()["unexpected"] = true.into();
            assert_eq!(
                c.decode(&line(&changed), &r).unwrap_err().kind,
                ErrorKind::InvalidResponse
            );
        }
        let text = String::from_utf8(line(&original)).unwrap();
        for text in [
            format!("{}{}", text, text),
            text.replace(
                "\"engine_generation\":7",
                "\"engine_generation\":7,\"engine_generation\":7",
            ),
            text.replace("\"status\":\"ok\"", "\"status\":\"ok\",\"status\":\"ok\""),
        ] {
            assert_eq!(
                c.decode(text.as_bytes(), &r).unwrap_err().kind,
                ErrorKind::InvalidResponse
            );
        }
        assert!(c.decode(&[], &r).is_err());
        for (pointer, field) in [
            ("", "request_id"),
            ("", "dispatch_state"),
            ("/result", "candidate_count"),
            ("/result", "score_kind"),
            ("/result/candidates/0", "id"),
        ] {
            let mut changed = original.clone();
            changed
                .pointer_mut(pointer)
                .unwrap()
                .as_object_mut()
                .unwrap()
                .remove(field);
            assert_eq!(
                c.decode(&line(&changed), &r).unwrap_err().kind,
                ErrorKind::InvalidResponse
            );
        }
        let nonfinite = text.replace("\"raw_score\":-1.5", "\"raw_score\":1e999");
        assert_eq!(
            c.decode(nonfinite.as_bytes(), &r).unwrap_err().kind,
            ErrorKind::InvalidResponse
        );
    }
    #[test]
    fn refusal_is_not_dispatch_and_producer_error_is_not_a_result() {
        let c = client();
        let r = invocation();
        let refused = serde_json::json!({"schema":RESPONSE_SCHEMA,"request_id":null,"status":"refused",
            "dispatch_state":"not_dispatched","reason":"peer_revoked_or_authority_unavailable"});
        assert_eq!(
            c.decode(&line(&refused), &r).unwrap_err().dispatch_state,
            DispatchState::NotDispatched
        );
        let mut missing = refused.clone();
        missing.as_object_mut().unwrap().remove("request_id");
        assert_eq!(
            c.decode(&line(&missing), &r).unwrap_err().kind,
            ErrorKind::InvalidResponse
        );
        let mut error = success(&c, &r);
        error.as_object_mut().unwrap().remove("result");
        error["status"] = "error".into();
        error["dispatch_state"] = "outcome_unavailable".into();
        error["reason"] = "producer_error".into();
        error["error"] = serde_json::json!({"code":-1,"name":"synthetic","owner":"not logged"});
        let observed = c.decode(&line(&error), &r).unwrap_err();
        assert_eq!(observed.dispatch_state, DispatchState::OutcomeUnavailable);
        assert!(!format!("{observed:?} {observed}").contains("not logged"));
        error["error"]["code"] = 0.into();
        assert!(c.decode(&line(&error), &r).is_err());
    }
    #[test]
    fn invalid_input_and_early_cancellation_never_start_ssh() {
        let c = client();
        let mut r = invocation();
        assert_eq!(
            c.execute_cancellable(&r, &AtomicBool::new(true))
                .unwrap_err(),
            Error::new(ErrorKind::Cancelled, DispatchState::NotDispatched)
        );
        r.input.candidates[1].id = r.input.candidates[0].id.clone();
        assert_eq!(
            c.execute(&r).unwrap_err().dispatch_state,
            DispatchState::NotDispatched
        );
        let mut identity = c.identity;
        identity.logical_model_identity = "unqualified".into();
        assert!(identity.validate().is_err());
    }
}
