// Typed public C producer projection. No private wire codec or model semantics.
use super::{Error, execution::checked, put_text, raw, text};
use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Candidate {
    pub id: String,
    pub text: String,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub(crate) struct Request {
    pub model_alias: String,
    pub expected_generation: u64,
    pub question: String,
    pub context: String,
    pub candidates: Vec<Candidate>,
}

fn malformed() -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "finite.remote.input".into(),
        message: "bounded finite request required".into(),
    }
}

impl Request {
    pub(crate) fn native(&self) -> Result<Box<raw::yvex_finite_producer_request>, Error> {
        if self.expected_generation == 0
            || self.model_alias.is_empty()
            || self.question.is_empty()
            || self.candidates.is_empty()
            || self.candidates.len() > raw::YVEX_FINITE_PRODUCER_MAX_CANDIDATES as usize
        {
            return Err(malformed());
        }
        let mut request = Box::<raw::yvex_finite_producer_request>::default();
        request.schema_version = raw::YVEX_FINITE_PRODUCER_SCHEMA_V1;
        request.expected_generation = self.expected_generation;
        put_text(&mut request.model_alias, &self.model_alias)?;
        put_text(&mut request.question, &self.question)?;
        put_text(&mut request.context, &self.context)?;
        request.candidate_count = self.candidates.len() as u64;
        for (index, candidate) in self.candidates.iter().enumerate() {
            if candidate.id.is_empty()
                || candidate.text.is_empty()
                || self.candidates[..index]
                    .iter()
                    .any(|prior| prior.id == candidate.id)
            {
                return Err(malformed());
            }
            put_text(&mut request.candidates[index].id, &candidate.id)?;
            put_text(&mut request.candidates[index].text, &candidate.text)?;
        }
        Ok(request)
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct CandidateScore {
    pub id: String,
    pub raw_score: f64,
    pub relative_candidate_probability: f64,
}

#[derive(Debug, Serialize)]
pub(crate) struct Observation {
    pub schema_version: u32,
    pub score_kind: &'static str,
    pub engine_generation: u64,
    pub token_count: u64,
    pub candidate_count: u64,
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

pub(crate) fn execute(request: &Request) -> Result<Observation, Error> {
    let request = request.native()?;
    let mut result = Box::<raw::yvex_finite_producer_result>::default();
    let mut failure = raw::yvex_error::default();
    // The public client owns connection, bounded codec, generation/population
    // verification and cleanup. None of its pointers survives this call.
    checked(
        unsafe {
            raw::yvex_finite_producer_execute_local(
                std::ptr::null(),
                request.as_ref(),
                result.as_mut(),
                &mut failure,
            )
        },
        &failure,
    )?;
    if result.schema_version != raw::YVEX_FINITE_PRODUCER_SCHEMA_V1
        || result.score_kind
            != raw::yvex_finite_producer_score_kind_YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT
        || result.candidate_count != request.candidate_count
        || result.candidate_count as usize > result.candidates.len()
    {
        return Err(malformed());
    }
    let candidates = result.candidates[..result.candidate_count as usize]
        .iter()
        .map(|c| CandidateScore {
            id: text(&c.id),
            raw_score: c.raw_score,
            relative_candidate_probability: c.relative_candidate_probability,
        })
        .collect();
    Ok(Observation {
        schema_version: result.schema_version,
        score_kind: "model-logit",
        engine_generation: result.engine_generation,
        token_count: result.token_count,
        candidate_count: result.candidate_count,
        model_forward_count: result.model_forward_count,
        sampling_invocation_count: result.sampling_invocation_count,
        generated_token_count: result.generated_token_count,
        resident_backbone_count: result.resident_backbone_count,
        elapsed_nanoseconds: result.elapsed_nanoseconds,
        source_mapped_bytes: result.source_mapped_bytes,
        parameter_execution_bytes: result.parameter_execution_bytes,
        workspace_host_bytes: result.workspace_host_bytes,
        workspace_device_bytes: result.workspace_device_bytes,
        source_identity: text(&result.source_identity),
        logical_model_identity: text(&result.logical_model_identity),
        binding_identity: text(&result.binding_identity),
        tokenizer_identity: text(&result.tokenizer_identity),
        physical_program_identity: text(&result.physical_program_identity),
        input_policy_identity: text(&result.input_policy_identity),
        input_identity: text(&result.input_identity),
        candidate_population_identity: text(&result.candidate_population_identity),
        result_identity: text(&result.result_identity),
        calibrated: result.calibrated != 0,
        candidates,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    fn fixture() -> Request {
        serde_json::from_value(
            serde_json::json!({"model_alias":"finite", "expected_generation":1,
            "question":"Which?", "context":"", "candidates":[{"id":"one","text":"one"}]}),
        )
        .unwrap()
    }
    #[test]
    fn public_header_bounds_are_enforced_without_truncation() {
        let mut request = fixture();
        assert!(request.native().is_ok());
        request.question = "x".repeat(raw::YVEX_FINITE_PRODUCER_QUESTION_CAP as usize);
        assert!(request.native().is_err());
        request = fixture();
        request.candidates.push(Candidate {
            id: "one".into(),
            text: "two".into(),
        });
        assert!(request.native().is_err());
        request = fixture();
        request.context = "\0".into();
        assert!(request.native().is_err());
        request = fixture();
        request.expected_generation = 0;
        assert!(request.native().is_err());
    }
}
