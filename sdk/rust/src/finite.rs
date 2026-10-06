//! Semantic-neutral finite producer client. The public C client owns transport
//! and model input construction. This module owns neither YAI decisions nor
//! YVEX engines, templates, tokenization or private protocol framing.
use serde::{Deserialize, Serialize};
use std::path::Path;

pub mod remote;

include!(concat!(env!("OUT_DIR"), "/finite_limits.rs"));
pub const CLIENT_SCHEMA: &str = "platform.sdk.yvex.finite.v1";
pub const NATIVE_CLIENT_COMPILED: bool = cfg!(feature = "finite-decision-native");

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Candidate {
    pub id: String,
    pub text: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Request {
    pub model_alias: String,
    pub expected_generation: u64,
    pub question: String,
    pub context: String,
    pub candidates: Vec<Candidate>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct CandidateScore {
    pub id: String,
    pub raw_score: f64,
    /// Relative to exactly this population. Never calibrated confidence.
    pub relative_candidate_probability: f64,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ResultObservation {
    pub schema_version: u32,
    pub engine_generation: u64,
    pub token_count: u64,
    pub model_forward_count: u64,
    pub sampling_invocation_count: u64,
    pub generated_token_count: u64,
    pub resident_backbone_count: u64,
    pub elapsed_nanoseconds: u64,
    pub source_mapped_bytes: u64,
    pub parameter_execution_bytes: u64,
    pub workspace_host_bytes: u64,
    pub workspace_device_bytes: u64,
    pub source_identity: String,
    pub logical_model_identity: String,
    pub binding_identity: String,
    pub tokenizer_identity: String,
    pub physical_program_identity: String,
    pub input_policy_identity: String,
    pub input_identity: String,
    pub candidate_population_identity: String,
    pub result_identity: String,
    pub calibrated: bool,
    pub candidates: Vec<CandidateScore>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Error {
    NativeClientNotInstalled,
    InvalidRequest,
    InvalidSocketPath,
    UnsupportedResult,
    ResultIdentityMismatch,
    InvalidScores,
    ProducerRefused { status: i32, where_code: String },
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // Context-bearing producer messages are deliberately not rendered.
        match self {
            Self::ProducerRefused { status, .. } => {
                write!(f, "YVEX finite producer refused ({status})")
            }
            _ => write!(f, "YVEX finite client: {self:?}"),
        }
    }
}
impl std::error::Error for Error {}

fn bounded(value: &str, cap: usize, allow_empty: bool) -> bool {
    (allow_empty || !value.is_empty()) && value.len() < cap && !value.contains('\0')
}
impl Request {
    pub fn validate(&self) -> Result<(), Error> {
        if !bounded(&self.model_alias, ID_CAP, false)
            || self.expected_generation == 0
            || !bounded(&self.question, QUESTION_CAP, false)
            || !bounded(&self.context, CONTEXT_CAP, true)
            || self.candidates.is_empty()
            || self.candidates.len() > MAX_CANDIDATES
            || self.candidates.iter().enumerate().any(|(i, c)| {
                !bounded(&c.id, ID_CAP, false)
                    || !bounded(&c.text, CANDIDATE_TEXT_CAP, false)
                    || self.candidates[..i]
                        .iter()
                        .any(|previous| previous.id == c.id)
            })
        {
            return Err(Error::InvalidRequest);
        }
        Ok(())
    }
}

impl ResultObservation {
    pub fn validate_against(&self, request: &Request) -> Result<(), Error> {
        request.validate()?;
        if self.schema_version != PRODUCER_SCHEMA
            || self.calibrated
            || self.model_forward_count != 1
            || self.sampling_invocation_count != 0
            || self.generated_token_count != 0
            || self.resident_backbone_count != 1
            || self.token_count == 0
        {
            return Err(Error::UnsupportedResult);
        }
        if self.engine_generation != request.expected_generation
            || self.candidates.len() != request.candidates.len()
            || self
                .candidates
                .iter()
                .zip(&request.candidates)
                .any(|(score, candidate)| score.id != candidate.id)
            || [
                &self.source_identity,
                &self.logical_model_identity,
                &self.binding_identity,
                &self.tokenizer_identity,
                &self.physical_program_identity,
                &self.input_policy_identity,
                &self.input_identity,
                &self.candidate_population_identity,
                &self.result_identity,
            ]
            .iter()
            .any(|id| {
                id.len() != 64
                    || !id
                        .bytes()
                        .all(|c| c.is_ascii_hexdigit() && !c.is_ascii_uppercase())
            })
        {
            return Err(Error::ResultIdentityMismatch);
        }
        if self.candidates.iter().any(|score| {
            !score.raw_score.is_finite()
                || !score.relative_candidate_probability.is_finite()
                || !(0.0..=1.0).contains(&score.relative_candidate_probability)
        }) || (self
            .candidates
            .iter()
            .map(|c| c.relative_candidate_probability)
            .sum::<f64>()
            - 1.0)
            .abs()
            > 1e-8
        {
            return Err(Error::InvalidScores);
        }
        Ok(())
    }
}

/// One exact generation-bound call, with no automatic retry or provider switch.
/// The caller must admit disclosure and qualify the producer independently.
pub fn execute_local(socket: &Path, request: &Request) -> Result<ResultObservation, Error> {
    request.validate()?;
    if !socket.is_absolute() || socket.as_os_str().is_empty() {
        return Err(Error::InvalidSocketPath);
    }
    #[cfg(feature = "finite-decision-native")]
    return native::execute(socket, request);
    #[cfg(not(feature = "finite-decision-native"))]
    Err(Error::NativeClientNotInstalled)
}

#[cfg(feature = "finite-decision-native")]
mod native {
    use super::*;
    #[allow(
        non_camel_case_types,
        non_upper_case_globals,
        non_snake_case,
        dead_code
    )]
    mod abi {
        include!(concat!(env!("OUT_DIR"), "/finite_native.rs"));
    }
    fn put<const N: usize>(target: &mut [std::ffi::c_char; N], value: &str) -> Result<(), Error> {
        if value.len() >= N || value.contains('\0') {
            return Err(Error::InvalidRequest);
        }
        for (target, byte) in target.iter_mut().zip(value.bytes()) {
            *target = byte as _;
        }
        Ok(())
    }
    fn text<const N: usize>(value: &[std::ffi::c_char; N]) -> Result<String, Error> {
        let end = value
            .iter()
            .position(|b| *b == 0)
            .ok_or(Error::UnsupportedResult)?;
        let bytes = value[..end].iter().map(|b| *b as u8).collect::<Vec<_>>();
        String::from_utf8(bytes).map_err(|_| Error::UnsupportedResult)
    }
    pub(super) fn execute(socket: &Path, request: &Request) -> Result<ResultObservation, Error> {
        #[cfg(unix)]
        let socket = {
            use std::os::unix::ffi::OsStrExt;
            std::ffi::CString::new(socket.as_os_str().as_bytes())
                .map_err(|_| Error::InvalidSocketPath)?
        };
        #[cfg(not(unix))]
        let socket = std::ffi::CString::new(socket.to_str().ok_or(Error::InvalidSocketPath)?)
            .map_err(|_| Error::InvalidSocketPath)?;
        let mut input = abi::yvex_finite_producer_request::default();
        input.schema_version = abi::YVEX_FINITE_PRODUCER_SCHEMA_V1;
        input.expected_generation = request.expected_generation;
        input.candidate_count = request.candidates.len() as u64;
        put(&mut input.model_alias, &request.model_alias)?;
        put(&mut input.question, &request.question)?;
        put(&mut input.context, &request.context)?;
        for (target, candidate) in input.candidates.iter_mut().zip(&request.candidates) {
            put(&mut target.id, &candidate.id)?;
            put(&mut target.text, &candidate.text)?;
        }
        let mut result = abi::yvex_finite_producer_result::default();
        let mut error = abi::yvex_error::default();
        // SAFETY: bindings and layouts are generated/checked from the same public
        // installation. All buffers are fixed owned records alive for this call.
        // Only the public C client speaks the private transport; it borrows no
        // Rust memory beyond the call and returns copied bounded records.
        let status = unsafe {
            abi::yvex_finite_producer_execute_local(
                socket.as_ptr(),
                &input,
                &mut result,
                &mut error,
            )
        };
        if status != 0 {
            return Err(Error::ProducerRefused {
                status,
                where_code: text(&error.where_).unwrap_or_else(|_| "invalid_error_context".into()),
            });
        }
        if result.score_kind
            != abi::yvex_finite_producer_score_kind_YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT
            || result.candidate_count as usize > MAX_CANDIDATES
            || !matches!(result.calibrated, 0 | 1)
        {
            return Err(Error::UnsupportedResult);
        }
        let observed = ResultObservation {
            schema_version: result.schema_version,
            engine_generation: result.engine_generation,
            token_count: result.token_count,
            model_forward_count: result.model_forward_count,
            sampling_invocation_count: result.sampling_invocation_count,
            generated_token_count: result.generated_token_count,
            resident_backbone_count: result.resident_backbone_count,
            elapsed_nanoseconds: result.elapsed_nanoseconds,
            source_mapped_bytes: result.source_mapped_bytes,
            parameter_execution_bytes: result.parameter_execution_bytes,
            workspace_host_bytes: result.workspace_host_bytes,
            workspace_device_bytes: result.workspace_device_bytes,
            source_identity: text(&result.source_identity)?,
            logical_model_identity: text(&result.logical_model_identity)?,
            binding_identity: text(&result.binding_identity)?,
            tokenizer_identity: text(&result.tokenizer_identity)?,
            physical_program_identity: text(&result.physical_program_identity)?,
            input_policy_identity: text(&result.input_policy_identity)?,
            input_identity: text(&result.input_identity)?,
            candidate_population_identity: text(&result.candidate_population_identity)?,
            result_identity: text(&result.result_identity)?,
            calibrated: result.calibrated != 0,
            candidates: result.candidates[..result.candidate_count as usize]
                .iter()
                .map(|value| {
                    Ok(CandidateScore {
                        id: text(&value.id)?,
                        raw_score: value.raw_score,
                        relative_candidate_probability: value.relative_candidate_probability,
                    })
                })
                .collect::<Result<Vec<_>, Error>>()?,
        };
        observed.validate_against(request)?;
        Ok(observed)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn request() -> Request {
        Request {
            model_alias: "fixture-finite".into(),
            expected_generation: 7,
            question: "Which option?".into(),
            context: "Fixture only".into(),
            candidates: vec![
                Candidate {
                    id: "opaque:one".into(),
                    text: "First".into(),
                },
                Candidate {
                    id: "opaque:two".into(),
                    text: "Second".into(),
                },
            ],
        }
    }
    fn result() -> ResultObservation {
        let id = "a".repeat(64);
        ResultObservation {
            schema_version: PRODUCER_SCHEMA,
            engine_generation: 7,
            token_count: 12,
            model_forward_count: 1,
            sampling_invocation_count: 0,
            generated_token_count: 0,
            resident_backbone_count: 1,
            elapsed_nanoseconds: 1,
            source_mapped_bytes: 1,
            parameter_execution_bytes: 1,
            workspace_host_bytes: 1,
            workspace_device_bytes: 0,
            source_identity: id.clone(),
            logical_model_identity: id.clone(),
            binding_identity: id.clone(),
            tokenizer_identity: id.clone(),
            physical_program_identity: id.clone(),
            input_policy_identity: id.clone(),
            input_identity: id.clone(),
            candidate_population_identity: id.clone(),
            result_identity: id,
            calibrated: false,
            candidates: vec![
                CandidateScore {
                    id: "opaque:one".into(),
                    raw_score: 1.0,
                    relative_candidate_probability: 0.7,
                },
                CandidateScore {
                    id: "opaque:two".into(),
                    raw_score: -1.0,
                    relative_candidate_probability: 0.3,
                },
            ],
        }
    }
    #[test]
    fn exact_frontier_bounds_are_bytes_and_never_truncated() {
        let mut value = request();
        value.validate().unwrap();
        value.question = "é".repeat(QUESTION_CAP / 2);
        assert_eq!(value.validate(), Err(Error::InvalidRequest));
        value = request();
        value.candidates[1].id = value.candidates[0].id.clone();
        assert_eq!(value.validate(), Err(Error::InvalidRequest));
        value = request();
        value.candidates[0].text.push('\0');
        assert_eq!(value.validate(), Err(Error::InvalidRequest));
        value = request();
        value.expected_generation = 0;
        assert_eq!(value.validate(), Err(Error::InvalidRequest));
    }
    #[test]
    fn output_retains_exact_population_generation_and_uncalibrated_evidence() {
        let request = request();
        let expected = result();
        expected.validate_against(&request).unwrap();
        let mut value = expected.clone();
        value.engine_generation += 1;
        assert_eq!(
            value.validate_against(&request),
            Err(Error::ResultIdentityMismatch)
        );
        value = expected.clone();
        value.candidates.swap(0, 1);
        assert_eq!(
            value.validate_against(&request),
            Err(Error::ResultIdentityMismatch)
        );
        value = expected.clone();
        value.calibrated = true;
        assert_eq!(
            value.validate_against(&request),
            Err(Error::UnsupportedResult)
        );
        value = expected.clone();
        value.generated_token_count = 1;
        assert_eq!(
            value.validate_against(&request),
            Err(Error::UnsupportedResult)
        );
        for bad in [f64::NAN, f64::INFINITY, -0.1, 1.1] {
            value = expected.clone();
            value.candidates[0].relative_candidate_probability = bad;
            assert_eq!(value.validate_against(&request), Err(Error::InvalidScores));
        }
        value = expected;
        value.candidates[0].raw_score = f64::NAN;
        assert_eq!(value.validate_against(&request), Err(Error::InvalidScores));
    }
    #[test]
    #[cfg(not(feature = "finite-decision-native"))]
    fn absent_native_client_refuses_without_a_substitute_or_wire_transport() {
        assert_eq!(
            execute_local(Path::new("/tmp/fixture-unused.socket"), &request()),
            Err(Error::NativeClientNotInstalled)
        );
        assert_eq!(
            execute_local(Path::new("relative.socket"), &request()),
            Err(Error::InvalidSocketPath)
        );
    }
}
