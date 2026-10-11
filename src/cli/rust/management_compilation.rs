// Bounded public planning adapter. Catalog/preparation resolve local paths;
// native Program P owns all computational choices and feasibility decisions.
use crate::{
    ffi::{self, raw},
    management_models, variant,
};
use serde::Deserialize;
use serde_json::{Value, json};

type Result<T> = std::result::Result<T, ffi::Error>;
fn invalid(reason: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "management.compilation".into(),
        message: reason.into(),
    }
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Plan {
    model: String,
    expected_revision: String,
    backend: String,
    goal: String,
    context: u64,
    prefill: u64,
    concurrency: u64,
    memory_limit: Option<u64>,
    max_candidates: Option<u32>,
    #[serde(default)]
    allow_approximation: bool,
    #[serde(default)]
    require_routed_matrix: bool,
    technique: Option<String>,
    weight_budget: Option<u64>,
    search_states: Option<u32>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct QualificationRead {
    id: String,
}

fn qualification_id(value: &Value) -> Result<String> {
    let request: QualificationRead = serde_json::from_value(value.clone())
        .map_err(|_| invalid("malformed_qualification_input"))?;
    if request.id.is_empty()
        || request.id.len() > 256
        || !request
            .id
            .bytes()
            .all(|c| c.is_ascii_alphanumeric() || b"-_.".contains(&c))
    {
        return Err(invalid("published_qualification_id_required"));
    }
    Ok(request.id)
}

pub(crate) fn qualification(value: &Value) -> Result<Value> {
    crate::qualification::published_record(&qualification_id(value)?)
        .map_err(|_| invalid("published_qualification_record_unavailable"))
}

impl Plan {
    fn parse(value: &Value) -> Result<Self> {
        let p: Self = serde_json::from_value(value.clone())
            .map_err(|_| invalid("malformed_optimization_input"))?;
        if p.model.is_empty()
            || p.model.len() > 512
            || p.model.chars().any(char::is_control)
            || p.model.starts_with(['/', '~'])
            || p.model.split('/').any(|v| v == "..")
            || p.expected_revision.len() != 40
            || !p
                .expected_revision
                .bytes()
                .all(|c| c.is_ascii_digit() || (b'a'..=b'f').contains(&c))
        {
            return Err(invalid("exact_model_revision_required"));
        }
        if !["cpu", "cuda", "metal"].contains(&p.backend.as_str())
            || !["balanced", "throughput", "memory", "quality"].contains(&p.goal.as_str())
            || p.context == 0
            || p.prefill == 0
            || p.prefill > p.context
            || !(1..=32).contains(&p.concurrency)
            || !(1..=32).contains(&p.max_candidates.unwrap_or(32))
        {
            return Err(invalid("invalid_optimization_constraints"));
        }
        let technique = p.technique.as_deref().unwrap_or("fixed-recipes-v1");
        let allocation = technique == "source-retention-allocation-v1";
        if !["fixed-recipes-v1", "source-retention-allocation-v1"].contains(&technique)
            || allocation != p.weight_budget.is_some()
            || p.weight_budget == Some(0)
            || (!allocation && p.search_states.is_some())
            || !(1..=4096).contains(&p.search_states.unwrap_or(4096))
        {
            return Err(invalid("invalid_optimization_search_bounds"));
        }
        Ok(p)
    }
    fn native(&self) -> Result<raw::yvex_optimization_request> {
        Ok(raw::yvex_optimization_request {
            schema_version: raw::YVEX_OPTIMIZATION_SCHEMA_V1,
            goal: match self.goal.as_str() {
                "throughput" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_THROUGHPUT,
                "memory" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_MEMORY,
                "quality" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_QUALITY,
                _ => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_BALANCED,
            },
            backend: ffi::backend_kind(&self.backend)?,
            device_count: 1,
            maximum_candidates: self.max_candidates.unwrap_or(32),
            context_tokens: self.context,
            prefill_tokens: self.prefill,
            concurrent_sequences: self.concurrency,
            memory_limit_bytes: self.memory_limit.unwrap_or(0),
            allow_approximation: self.allow_approximation.into(),
            require_routed_matrix: self.require_routed_matrix.into(),
            ..Default::default()
        })
    }
}

pub(crate) fn validate(operation: &str, value: &Value) -> Result<()> {
    match operation {
        "qualification.get" => {
            qualification_id(value)?;
        }
        "optimization.plan" => {
            Plan::parse(value)?;
        }
        "optimization.techniques" if value.as_object().is_some_and(|o| o.is_empty()) => {}
        _ => return Err(invalid("malformed_optimization_input")),
    }
    Ok(())
}

pub(crate) fn techniques(value: &Value) -> Result<Value> {
    validate("optimization.techniques", value)?;
    Ok(json!({"schema":"yvex.optimization.techniques.v1",
        "techniques":ffi::variant::optimization_techniques()?}))
}

pub(crate) fn plan(value: &Value) -> Result<Value> {
    let input = Plan::parse(value)?;
    // Reuse the preparation lease and selection; no path or command is accepted
    // from the remote caller. This lease is not a GPU/memory reservation.
    let _lease = ffi::preparation::lock(None, &input.model)?;
    let (library, index, _) = management_models::selected(&input.model)?;
    let preparation = ffi::preparation::Preparation::open(
        &library,
        index,
        ffi::preparation::Request {
            root: None,
            registry: None,
            quant: None,
            imatrix: None,
            dry: true,
        },
    )?;
    let source = preparation.view()?;
    if source.revision != input.expected_revision {
        return Err(invalid("source_revision_changed_review_again"));
    }
    let request = input.native()?;
    let report = ffi::variant::optimize(ffi::variant::OptimizationInput {
        target: &source.target,
        source: &source.source,
        models_root: &source.models_root,
        manifest: &source.manifest,
        policy: None,
        imatrix: None,
        select: None,
        out_policy: None,
        request,
        weight_budget: input.weight_budget,
        search_states: input.search_states.unwrap_or(4096),
    })?;
    let mut result = variant::optimization_projection(
        &report,
        &request,
        &source.target,
        &input.goal,
        &input.backend,
        input.weight_budget,
    );
    result["model"] = json!(input.model);
    result["schema"] = json!("yvex.optimization.plan.v1");
    result["revision"] = json!(source.revision);
    result["qualification_evidence"] =
        crate::qualification::published_optimization_evidence(&report.rows)
            .map_err(|_| invalid("published_qualification_projection_refused"))?;
    result["publication"] = json!({"artifact_produced":false,"binding_published":false,
        "engine_loaded":false,"selection":"not_qualified",
        "production":"explicit_local_compile","evaluation":"explicit_local_qualification",
        "deployment":"separately_authorized"});
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn request() -> Value {
        json!({"model":"exact-model", "expected_revision":"a".repeat(40),
            "backend":"cpu", "goal":"memory", "context":32, "prefill":8,
            "concurrency":1})
    }
    #[test]
    fn planning_is_bounded_and_cannot_supply_paths_or_runtime_authority() {
        assert!(validate("optimization.plan", &request()).is_ok());
        for field in [
            "source",
            "source_manifest",
            "models_root",
            "policy",
            "imatrix",
            "out_policy",
            "engine",
            "host_instance",
            "system_reserve_bytes",
        ] {
            let mut p = request();
            p[field] = json!("operator-owned");
            assert!(validate("optimization.plan", &p).is_err(), "{field}");
        }
        for (field, v) in [
            ("context", json!(0)),
            ("prefill", json!(33)),
            ("concurrency", json!(33)),
            ("max_candidates", json!(33)),
            ("expected_revision", json!("main")),
            ("technique", json!("unimplemented")),
            ("search_states", json!(1)),
            ("weight_budget", json!(1)),
        ] {
            let mut p = request();
            p[field] = v;
            assert!(validate("optimization.plan", &p).is_err(), "{field}");
        }
        let mut p = request();
        p["technique"] = json!("source-retention-allocation-v1");
        p["weight_budget"] = json!(1024);
        p["search_states"] = json!(4096);
        assert!(validate("optimization.plan", &p).is_ok());
        p["search_states"] = json!(4097);
        assert!(validate("optimization.plan", &p).is_err());
    }
}
