// Engineering views compose admitted physical compiler sessions, never rebuild decisions.
use crate::{
    Output,
    ffi::{self, Variant, VariantRequest, raw},
    presentation,
    registry::{Invocation, Refusal},
};
use std::collections::BTreeMap;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
type Fields = Vec<(String, String)>;

// Input syntax only. The native compiler owns feasibility, identities and search.
#[derive(Clone, serde::Deserialize, serde::Serialize)]
#[serde(deny_unknown_fields)]
struct OptimizationFile {
    schema: String,
    target: String,
    source: String,
    models_root: String,
    source_manifest: String,
    backend: Option<String>,
    goal: Option<String>,
    policy: Option<String>,
    imatrix_manifest: Option<String>,
    context: Option<u64>,
    prefill: Option<u64>,
    concurrency: Option<u64>,
    memory_limit: Option<u64>,
    reserve: Option<u64>,
    max_candidates: Option<u64>,
    #[serde(default)]
    allow_approximation: bool,
    #[serde(default)]
    require_routed_matrix: bool,
    technique: Option<String>,
    weight_budget: Option<u64>,
    search_states: Option<u64>,
}

fn optimization_file_schema(input: &OptimizationFile) -> Result<()> {
    if !matches!(
        input.schema.as_str(),
        "yvex.optimization.request.v1" | "yvex.optimization.request.v2"
    ) {
        return Err(grammar("unsupported optimization request schema"));
    }
    if input.schema == "yvex.optimization.request.v1"
        && (input.technique.is_some()
            || input.weight_budget.is_some()
            || input.search_states.is_some())
    {
        return Err(grammar(
            "technique controls require optimization request v2",
        ));
    }
    Ok(())
}

fn optimization_file_words(
    invocation: &Invocation<'_>,
    input: OptimizationFile,
) -> Result<Vec<String>> {
    optimization_file_schema(&input)?;
    let mut words = invocation.operation.command_path.clone();
    for (flag, value) in [
        ("--target", Some(input.target)),
        ("--source", Some(input.source)),
        ("--models-root", Some(input.models_root)),
        ("--source-manifest", Some(input.source_manifest)),
        ("--backend", input.backend),
        ("--goal", input.goal),
        ("--technique", input.technique),
        (
            "--weight-budget",
            input.weight_budget.map(|n| n.to_string()),
        ),
        (
            "--search-states",
            input.search_states.map(|n| n.to_string()),
        ),
        ("--policy", input.policy),
        ("--imatrix-manifest", input.imatrix_manifest),
        ("--context", input.context.map(|n| n.to_string())),
        ("--prefill", input.prefill.map(|n| n.to_string())),
        ("--concurrency", input.concurrency.map(|n| n.to_string())),
        ("--memory-limit", input.memory_limit.map(|n| n.to_string())),
        ("--reserve", input.reserve.map(|n| n.to_string())),
        (
            "--max-candidates",
            input.max_candidates.map(|n| n.to_string()),
        ),
    ] {
        if let Some(value) = value {
            if value.is_empty() || value.contains('\0') || value.starts_with('-') {
                return Err(grammar("empty or ambiguous optimization request value"));
            }
            words.extend([flag.into(), value]);
        }
    }
    for (flag, enabled) in [
        ("--allow-approximation", input.allow_approximation),
        ("--require-routed-matrix", input.require_routed_matrix),
    ] {
        if enabled {
            words.push(flag.into());
        }
    }
    for (flag, values) in &invocation.flags {
        if flag == "--request" {
            continue;
        }
        if !matches!(
            flag.as_str(),
            "--json"
                | "--select"
                | "--out-policy"
                | "--evidence"
                | "--runtime-binding"
                | "--execution-strategy"
        ) {
            return Err(grammar(
                "request file cannot be mixed with individual planning flags",
            ));
        }
        words.push(flag.clone());
        if flag != "--json" {
            words.extend(values.iter().cloned());
        }
    }
    Ok(words)
}

fn optimize_file(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    use std::io::Read;
    use std::os::unix::fs::OpenOptionsExt;
    let file = std::fs::OpenOptions::new()
        .read(true)
        .custom_flags(rustix::fs::OFlags::NONBLOCK.bits() as i32)
        .open(required(invocation, "--request")?)?;
    if !file.metadata()?.is_file() {
        return Err(grammar("optimization request must be a regular file"));
    }
    let mut bytes = Vec::new();
    file.take(65_537).read_to_end(&mut bytes)?;
    if bytes.len() > 65_536 {
        return Err(grammar("optimization request exceeds 65536 bytes"));
    }
    let input: OptimizationFile = serde_json::from_slice(&bytes)?;
    optimization_file_schema(&input)?;
    if invocation.has("--guided") {
        return optimize_guided(invocation, Some(input), width, styled);
    }
    let words = optimization_file_words(invocation, input)?;
    let registry = crate::registry::Registry::embedded()?;
    // Reuse the canonical typed operator grammar and exactly the same native path.
    optimize(&registry.parse(&words)?, width, styled)
}

fn guided_model(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<OptimizationFile> {
    let library = ffi::Library::open(invocation.value("--models-root"), None)?;
    let mut indices = Vec::new();
    let mut labels = Vec::new();
    for index in 0..library.count() {
        let model = library.snapshot(index)?;
        if !model.sources.is_empty() {
            labels.push(crate::catalog::selector(&model));
            indices.push(index);
        }
    }
    if indices.is_empty() {
        return Err(grammar(
            "no catalog source; acquire an exact source with model pull first",
        ));
    }
    let selected =
        crate::catalog::interactive_choice(&labels, "Select exact model source", width, styled)?;
    // The native preparation owner resolves source lineage and canonical paths.
    // Dry inspection neither produces weights nor installs a runtime profile.
    let preparation = ffi::preparation::Preparation::open(
        &library,
        indices[selected],
        ffi::preparation::Request {
            root: invocation.value("--models-root"),
            registry: None,
            quant: None,
            imatrix: None,
            dry: true,
        },
    )?;
    let view = preparation.view()?;
    Ok(OptimizationFile {
        schema: "yvex.optimization.request.v2".into(),
        target: view.target,
        source: view.source,
        models_root: view.models_root,
        source_manifest: view.manifest,
        backend: None,
        goal: None,
        policy: None,
        imatrix_manifest: None,
        context: None,
        prefill: None,
        concurrency: Some(1),
        memory_limit: None,
        reserve: None,
        max_candidates: Some(32),
        allow_approximation: false,
        require_routed_matrix: false,
        technique: Some("presets".into()),
        weight_budget: None,
        search_states: None,
    })
}

fn guided_source_verification(input: &OptimizationFile, width: usize, styled: bool) -> Result<()> {
    use std::io::Write;
    if std::path::Path::new(&input.source_manifest).is_file() {
        return Ok(());
    }
    let choice = crate::catalog::interactive_choice(
        &[
            "Verify acquired checkpoint (may read all shards; verified manifest persists)".into(),
            "Cancel without source verification".into(),
        ],
        "Authenticated source manifest is missing",
        width,
        styled,
    )?;
    if choice != 0 {
        return Err(grammar("selection cancelled; no state changed"));
    }
    let _lease = ffi::preparation::lock(Some(&input.models_root), &input.target)?;
    let library = ffi::Library::open(Some(&input.models_root), None)?;
    // A compiler target ID is not necessarily a product catalog selector.
    // Re-resolve the selected source, then let preparation recheck its target
    // and manifest. Never guess an alias or verify a different acquisition.
    let mut indices = Vec::new();
    for i in 0..library.count() {
        if library
            .snapshot(i)?
            .sources
            .iter()
            .any(|source| ffi::text(&source.path) == input.source)
        {
            indices.push(i);
        }
    }
    if indices.len() != 1 {
        return Err(grammar(
            "catalog selection changed before source verification",
        ));
    }
    let mut preparation = ffi::preparation::Preparation::open(
        &library,
        indices[0],
        ffi::preparation::Request {
            root: Some(&input.models_root),
            registry: None,
            quant: None,
            imatrix: None,
            dry: false,
        },
    )?;
    let view = preparation.view()?;
    if view.source != input.source
        || view.manifest != input.source_manifest
        || view.target != input.target
    {
        return Err(grammar(
            "source selection changed before verification; restart planning",
        ));
    }
    std::io::stdout().write_all(presentation::flow_lines(&[
        "Verifying source payload through the native preparation owner; no artifact or engine is created.".into(),
    ], styled)?.as_bytes())?;
    preparation.verify()?;
    std::io::stdout().write_all(
        presentation::flow_lines(
            &[format!(
                "Source manifest retained: {}",
                input.source_manifest
            )],
            styled,
        )?
        .as_bytes(),
    )?;
    Ok(())
}

fn optimize_guided(
    invocation: &Invocation<'_>,
    input: Option<OptimizationFile>,
    width: usize,
    styled: bool,
) -> Result<Output> {
    use std::io::{IsTerminal, Write};
    if !std::io::stdin().is_terminal()
        || !std::io::stdout().is_terminal()
        || invocation.has("--json")
    {
        return Err(grammar(
            "guided compilation requires a terminal; automation uses --request FILE --json",
        ));
    }
    for flag in invocation.flags.keys() {
        if !matches!(
            flag.as_str(),
            "--guided" | "--request" | "--out-request" | "--models-root"
        ) {
            return Err(grammar(
                "guided compilation accepts only --request, --models-root and --out-request",
            ));
        }
    }
    if input.is_some() && invocation.has("--models-root") {
        return Err(grammar(
            "request owns models root; guided mode does not override it",
        ));
    }
    let catalog_selected = input.is_none();
    let mut input = match input {
        Some(value) => value,
        None => guided_model(invocation, width, styled)?,
    };
    let choose = |title: &str, choices: &[&str]| -> Result<usize> {
        Ok(crate::catalog::interactive_choice(
            &choices.iter().map(|s| (*s).into()).collect::<Vec<_>>(),
            title,
            width,
            styled,
        )?)
    };
    std::io::stdout().write_all(
        presentation::flow_lines(
            &[
                format!("PHYSICAL COMPILER · {}", input.target),
                "Planning only: no engine load, installation or quality promotion.".into(),
                "Hardware availability and full-model compatibility are checked, not assumed."
                    .into(),
            ],
            styled,
        )?
        .as_bytes(),
    )?;
    input.backend = Some(
        ["cpu", "cuda", "metal"][choose(
            "Target backend (verified during resolution)",
            &["CPU", "CUDA", "Metal · full-model support must be admitted"],
        )?]
        .into(),
    );
    input.goal = Some(
        ["balanced", "throughput", "memory", "quality"][choose(
            "Optimization goal",
            &["Balanced", "Throughput", "Memory", "Quality"],
        )?]
        .into(),
    );
    let retain = choose(
        "Workload",
        &[
            "Keep request geometry (default: context 4096 / prefill 512)",
            "Short: context 4096 / prefill 512",
            "Long: context 32768 / prefill 512",
        ],
    )?;
    input.context = Some(if retain == 0 {
        input.context.unwrap_or(4096)
    } else if retain == 1 {
        4096
    } else {
        32768
    });
    input.prefill = Some(if retain == 0 {
        input.prefill.unwrap_or(512)
    } else {
        512
    });
    input.allow_approximation = choose(
        "Numerical search constraint",
        &[
            "Preserve exact source representation",
            "Allow approximate candidates; independent quality remains mandatory",
        ],
    )? == 1;
    input.schema = "yvex.optimization.request.v2".into();
    // The request is the reproducible non-interactive product contract. Never overwrite one.
    let mut flags = invocation.flags.clone();
    flags.retain(|name, _| name == "--request");
    let replay = Invocation {
        operation: invocation.operation,
        positionals: Vec::new(),
        flags,
        ordered_flags: Vec::new(),
    };
    let words = optimization_file_words(&replay, input.clone())?;
    let registry = crate::registry::Registry::embedded()?;
    let parsed = registry.parse(&words)?;
    if catalog_selected {
        guided_source_verification(&input, width, styled)?;
    }
    if let Some(path) = invocation.value("--out-request") {
        let bytes = serde_json::to_vec_pretty(&input)?;
        let mut file = std::fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(path)?;
        file.write_all(&bytes)?;
        file.write_all(b"\n")?;
        file.sync_all()?;
        std::io::stdout().write_all(
            presentation::flow_lines(
                &[
                    format!("Request saved: {path}"),
                    "Replay with: yvex compile optimize --request <saved-file> --json".into(),
                ],
                styled,
            )?
            .as_bytes(),
        )?;
    }
    optimize(&parsed, width, styled)
}

pub(crate) fn optimize(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    if invocation.has("--list-techniques") {
        if invocation
            .flags
            .keys()
            .any(|f| !matches!(f.as_str(), "--list-techniques" | "--json"))
        {
            return Err(grammar(
                "technique inspection cannot be mixed with compilation inputs",
            ));
        }
        let methods = ffi::variant::optimization_techniques()?;
        if invocation.has("--json") {
            return Ok(Output::standard(
                format!(
                    "{}\n",
                    serde_json::json!({
                "schema": "yvex.optimization.techniques.v1", "techniques": methods})
                ),
                0,
            ));
        }
        return Ok(Output::standard(
            presentation::flow_lines(
                &methods
                    .iter()
                    .map(|m| {
                        format!(
                            "{} · {}\n  Inputs: {}\n  Objective: {}\n  Still required: {}",
                            m["name"].as_str().unwrap_or(""),
                            m["identity"].as_str().unwrap_or(""),
                            m["inputs"].as_str().unwrap_or(""),
                            m["objective"].as_str().unwrap_or(""),
                            m["unearned_evidence"].as_str().unwrap_or("")
                        )
                    })
                    .collect::<Vec<_>>(),
                styled,
            )?,
            0,
        ));
    }
    if invocation.has("--request") {
        return optimize_file(invocation, width, styled);
    }
    if invocation.has("--guided") {
        return optimize_guided(invocation, None, width, styled);
    }
    if invocation.has("--out-request") {
        return Err(grammar("--out-request requires --guided"));
    }
    if (invocation.has("--evidence") || invocation.has("--runtime-binding"))
        && invocation.has("--select")
    {
        return Err(grammar(
            "evidence inspection and policy export are separate operations",
        ));
    }
    if invocation.has("--execution-strategy") && !invocation.has("--runtime-binding") {
        return Err(grammar(
            "execution strategy requires produced-binding inspection",
        ));
    }
    if invocation.has("--runtime-binding") && invocation.has("--reserve") {
        return Err(grammar(
            "produced-binding inspection uses the canonical runtime reserve; custom reserve is not projected",
        ));
    }
    if invocation.has("--select") != invocation.has("--out-policy") {
        return Err(grammar(
            "selection requires both --select <candidate identity> and --out-policy",
        ));
    }
    let number = |key: &str, default: u64| -> Result<u64> {
        Ok(invocation
            .value(key)
            .map(str::parse)
            .transpose()?
            .unwrap_or(default))
    };
    let goal = invocation.value("--goal").unwrap_or("balanced");
    let goal_id = match goal {
        "balanced" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_BALANCED,
        "throughput" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_THROUGHPUT,
        "memory" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_MEMORY,
        "quality" => raw::yvex_optimization_goal_YVEX_OPTIMIZATION_QUALITY,
        _ => return Err(grammar("unknown optimization goal")),
    };
    let backend = invocation.value("--backend").unwrap_or("cpu");
    let allocation = invocation.value("--technique") == Some("source-retention");
    if allocation != invocation.has("--weight-budget")
        || (!allocation && invocation.has("--search-states"))
    {
        return Err(grammar(
            "source-retention requires --weight-budget; allocation controls cannot apply to presets",
        ));
    }
    let request = raw::yvex_optimization_request {
        schema_version: raw::YVEX_OPTIMIZATION_SCHEMA_V1,
        goal: goal_id,
        backend: ffi::backend_kind(backend)?,
        device_count: 1,
        maximum_candidates: number("--max-candidates", 32)?.try_into()?,
        context_tokens: number("--context", 32768)?,
        prefill_tokens: number("--prefill", 512)?,
        concurrent_sequences: number("--concurrency", 1)?,
        memory_limit_bytes: number("--memory-limit", 0)?,
        system_reserve_bytes: number("--reserve", 0)?,
        allow_approximation: i32::from(invocation.has("--allow-approximation")),
        require_routed_matrix: i32::from(invocation.has("--require-routed-matrix")),
        ..Default::default()
    };
    let report = ffi::variant::optimize(ffi::variant::OptimizationInput {
        target: required(invocation, "--target")?,
        source: required(invocation, "--source")?,
        models_root: required(invocation, "--models-root")?,
        manifest: required(invocation, "--source-manifest")?,
        policy: invocation.value("--policy"),
        imatrix: invocation.value("--imatrix-manifest"),
        select: invocation.value("--select"),
        out_policy: invocation.value("--out-policy"),
        request,
        weight_budget: allocation
            .then(|| number("--weight-budget", 0))
            .transpose()?,
        search_states: number("--search-states", 4096)?.try_into()?,
    })?;
    let deployment = invocation
        .value("--runtime-binding")
        .map(|path| {
            ffi::variant::deployment_assessment(
                path,
                &report.rows,
                &request,
                invocation.value("--execution-strategy") == Some("speculative"),
            )
        })
        .transpose()?;
    let evidence = crate::qualification::optimization_evidence(
        invocation.value("--evidence"),
        &report.rows,
        deployment.as_ref(),
    )?;
    if invocation.has("--json") {
        return Ok(Output::standard(
            serde_json::to_string_pretty(&optimization_json(
                &report,
                &request,
                invocation,
                &evidence,
                deployment.as_ref(),
            )?)? + "\n",
            0,
        ));
    }
    optimization_human(
        &report, &request, invocation, &evidence, deployment, width, styled,
    )
}

fn optimization_json(
    report: &ffi::variant::OptimizationReport,
    request: &raw::yvex_optimization_request,
    invocation: &Invocation<'_>,
    evidence: &serde_json::Value,
    deployment: Option<&ffi::variant::DeploymentAssessment>,
) -> Result<serde_json::Value> {
    let goal = invocation.value("--goal").unwrap_or("balanced");
    let backend = invocation.value("--backend").unwrap_or("cpu");
    let allocation = invocation.value("--technique") == Some("source-retention");
    let weight_budget = invocation
        .value("--weight-budget")
        .map(str::parse::<u64>)
        .transpose()?;
    let rows = report.rows.iter().map(|c| {
        let facts = c.failure_status == 0;
        serde_json::json!({
        "recipe": ffi::text(&c.recipe), "state": ffi::variant::optimization_state(c.state),
        "candidate_identity": ffi::text(&c.candidate_identity),
        "physical_variant_identity": ffi::text(&c.physical_variant_identity),
        "source_identity": ffi::text(&c.source_identity), "transform_identity": ffi::text(&c.transform_identity),
        "policy_identity": ffi::text(&c.policy_identity), "artifact_bytes": facts.then_some(c.artifact_bytes),
        "encoded_bytes": facts.then_some(c.encoded_bytes),
        "maximum_tensor_bytes": facts.then_some(c.maximum_tensor_bytes),
        "reserve_bytes": facts.then_some(c.reserve_bytes),
        "initial_required_bytes": facts.then_some(c.initial_required_bytes),
        "memory_budget_bytes": facts.then_some(c.memory_budget_bytes), "missing_evidence": c.missing_evidence,
        "approximate_tensors": facts.then_some(c.approximate_tensors),
        "routed_tensors": facts.then_some(c.routed_tensors),
        "matrix_incompatible_tensors": facts.then_some(c.matrix_incompatible_tensors),
        "artifact_catalog_compatibility": match c.artifact_catalog_compatibility {
            1 => "matches-static-constraint", 2 => "incompatible",
            3 => "requires-complete-production-proof", _ => "unknown"
        },
        "reason": ffi::text(&c.reason), "failure_status": c.failure_status,
        "workspace_bytes": null, "state_bytes": null, "prefill_tokens_per_second": null,
        "decode_tokens_per_second": null, "quality": null
    })}).collect::<Vec<_>>();
    Ok(serde_json::json!({
                "schema": "yvex.optimization.search.v2", "goal": goal, "backend": backend,
                "technique": if allocation { "source-retention-allocation-v1" } else { "fixed-recipes-v1" },
                "weight_budget_bytes": weight_budget,
                "resolved_context": {
                    "schema": "yvex.optimization.context.v1",
                    "identity": ffi::text(&report.context.identity),
                    "semantic_identity": ffi::text(&report.context.semantic_identity),
                    "source_identity": ffi::text(&report.context.source_identity),
                    "model_execution_identity": ffi::text(&report.context.model_execution_identity),
                    "family": ffi::text(&report.context.family),
                    "semantic_maximum_context": report.context.maximum_context,
                    "layers": report.context.layers,
                    "attention_layers": report.context.attention_layers,
                    "sequence_mixer_layers": report.context.sequence_mixer_layers,
                    "routed_experts": report.context.routed_experts,
                    "experts_per_row": report.context.experts_per_row,
                    "draft_layers": report.context.draft_layers,
                    "capacity_scope": "semantic envelope only; full runtime capacity requires produced binding",
                    "quality": null, "calibration": "candidate-bound; not held-out quality",
                    "missing_evidence": report.context.missing_evidence
                },
                "target": invocation.value("--target"),
                "source_commit": env!("YVEX_BUILD_COMMIT"),
                "source_tree": env!("YVEX_BUILD_SOURCE_TREE"),
                "source_state": env!("YVEX_BUILD_SOURCE_STATE"),
                "source_delta_identity": env!("YVEX_BUILD_SOURCE_DELTA_IDENTITY"),
                "build_identity": env!("YVEX_BUILD_IDENTITY"),
                "shell_build_identity": env!("YVEX_SHELL_BUILD_IDENTITY"),
                "memory_limit_bytes": request.memory_limit_bytes,
                "allow_approximation": request.allow_approximation != 0,
                "require_routed_matrix": request.require_routed_matrix != 0,
                "context_tokens": request.context_tokens, "prefill_tokens": request.prefill_tokens,
                "concurrent_sequences": request.concurrent_sequences,
                "total_memory_bytes": report.total_memory, "available_memory_bytes": report.available_memory,
                "process_memory_limited": report.process_limited,
                "compute_capability": [report.compute_major, report.compute_minor], "device_count": 1,
                "selected_candidate": invocation.value("--select"), "exported_policy": invocation.value("--out-policy"),
                "experiment_priority": {"model":"static-feasibility-hints-v1",
                    "basis": ffi::variant::optimization_priority(request.goal),
                    "measured_ranking":false, "predicted_rates":null},
                "qualified_recommendation": null, "candidates": rows,
                "qualification_evidence": evidence,
                "deployment_assessment": deployment.as_ref().map(|d| {
                    serde_json::json!({
                        "candidate_identity": d.candidate, "binding_identity": d.binding,
                        "artifact_identity": d.artifact, "status": d.status, "reason": d.reason,
                        "execution_strategy": invocation.value("--execution-strategy").unwrap_or("target-only"),
                        "sampling": "greedy", "context": request.context_tokens,
                        "prefill_chunk": request.prefill_tokens, "concurrency": request.concurrent_sequences,
                        "required_peak_bytes": (d.required != 0).then_some(d.required),
                        "available_bytes": (d.available != 0).then_some(d.available),
                        "plan": d.plan.as_ref().map(|p| serde_json::json!({
                            "identity": ffi::text(&p.identity), "model_bytes": p.model_bytes,
                            "derived_layout_bytes": p.derived_layout_bytes, "workspace_bytes": p.workspace_bytes,
                            "state_pool_bytes": p.state_pool_bytes, "candidate_reserve_bytes": p.candidate_reserve_bytes,
                            "persistent_state_bytes": p.persistent_state_bytes, "prefix_cache_bytes": p.prefix_cache_bytes,
                            "scheduler_bytes": p.scheduler_bytes, "graph_bytes": p.graph_bytes,
                            "system_reserve_bytes": p.system_reserve_bytes, "required_bytes": p.required_bytes
                        })),
                        "scope": "runtime capacity plan only; no weight residency, engine creation or reservation"
                    })
                })
    }))
}

fn optimization_human(
    report: &ffi::variant::OptimizationReport,
    request: &raw::yvex_optimization_request,
    invocation: &Invocation<'_>,
    evidence: &serde_json::Value,
    deployment: Option<ffi::variant::DeploymentAssessment>,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let mut lines = vec![
        "PHYSICAL COMPILER · candidate assessment".into(),
        format!(
            "{} · {} · goal {} · technique {}",
            ffi::text(&report.context.family),
            invocation.value("--backend").unwrap_or("cpu"),
            invocation.value("--goal").unwrap_or("balanced"),
            invocation.value("--technique").unwrap_or("presets")
        ),
        format!(
            "Context {} / semantic maximum {} · prefill {} · sequences {}",
            request.context_tokens,
            report.context.maximum_context,
            request.prefill_tokens,
            request.concurrent_sequences
        ),
        format!(
            "Available {:.2} GiB · reserve {:.2} GiB · full runtime fit requires produced binding",
            report.available_memory as f64 / 1073741824.0,
            report.context.reserve_bytes as f64 / 1073741824.0
        ),
        format!(
            "Experiment order: {}",
            ffi::variant::optimization_priority(request.goal)
        ),
        "No qualified recommendation. All quality and timing claims require independent evidence."
            .into(),
        "".into(),
    ];
    for c in &report.rows {
        lines.push(format!(
            "{} · {}",
            ffi::text(&c.recipe),
            ffi::variant::optimization_state(c.state)
        ));
        if c.failure_status == 0 {
            lines.push(format!("  {}… · weights {:.2} GiB · initial minimum {:.2} GiB · approximate tensors {} · matrix refusals {}/{}",
                ffi::text(&c.candidate_identity).chars().take(12).collect::<String>(),
                c.encoded_bytes as f64 / 1073741824.0, c.initial_required_bytes as f64 / 1073741824.0,
                c.approximate_tensors, c.matrix_incompatible_tensors, c.routed_tensors));
        }
        if c.state != raw::yvex_optimization_state_YVEX_OPTIMIZATION_NEEDS_QUALIFICATION {
            lines.push(format!("  {}", ffi::text(&c.reason)));
        }
    }
    lines.push("Full identities and individual proof gaps: repeat the request with --json. Export requires the full candidate identity.".into());
    let mut output = presentation::flow_lines(&lines, styled)?;
    for item in evidence.as_array().into_iter().flatten() {
        let receipt = &item["receipt"];
        output.push_str(&render(
            "RELATED EVIDENCE · exact recipe match unproven",
            &vec![
                ("receipt".into(), receipt["id"].as_str().unwrap_or("").into()),
                ("target identity".into(), receipt["target_identity"].as_str().unwrap_or("").into()),
                ("quality".into(), receipt["claims"]["representation-quality"]["state"].as_str().unwrap_or("").into()),
                ("performance".into(), receipt["claims"]["deployment-performance"]["state"].as_str().unwrap_or("").into()),
                ("scope".into(), "Only the exact target/workload in this receipt; --json exposes complete context".into()),
            ], width, styled,
        )?);
    }
    if let Some(d) = deployment {
        output.push_str(&render(
            "PRODUCED BINDING · capacity preflight",
            &vec![
                ("binding".into(), d.binding),
                ("candidate".into(), d.candidate),
                ("status".into(), d.status.to_string()),
                ("reason".into(), d.reason),
                (
                    "required peak bytes".into(),
                    if d.required != 0 {
                        d.required.to_string()
                    } else {
                        "unknown".into()
                    },
                ),
                (
                    "available bytes".into(),
                    if d.available != 0 {
                        d.available.to_string()
                    } else {
                        "unknown".into()
                    },
                ),
                (
                    "scope".into(),
                    "No model loaded; live admission must recheck resources".into(),
                ),
            ],
            width,
            styled,
        )?);
    }
    Ok(Output::standard(output, 0))
}

fn grammar(reason: &str) -> Box<dyn std::error::Error> {
    Box::new(Refusal {
        reason: reason.into(),
        hint: None,
    })
}
fn required<'a>(invocation: &'a Invocation<'_>, name: &str) -> Result<&'a str> {
    invocation
        .value(name)
        .filter(|value| !value.is_empty())
        .ok_or_else(|| grammar(&format!("{name} is required")))
}
fn render(title: &str, fields: &Fields, width: usize, styled: bool) -> Result<String> {
    let pairs = fields
        .iter()
        .map(|(name, value)| (name.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record(title, &pairs, width, styled)?)
}

fn open(invocation: &Invocation<'_>) -> Result<Variant> {
    if invocation.has("--component") {
        for name in [
            "--models-root",
            "--source-manifest",
            "--policy",
            "--imatrix-manifest",
            "--role",
        ] {
            if invocation.has(name) {
                return Err(grammar(
                    "component variants do not admit complete-model policy/source options",
                ));
            }
        }
    } else if !invocation.has("--models-root")
        || !invocation.has("--source-manifest")
        || invocation.has("--preset") == invocation.has("--policy")
    {
        return Err(grammar(
            "complete variants require --models-root, --source-manifest and exactly one --preset/--policy",
        ));
    }
    Ok(Variant::open(VariantRequest {
        target: required(invocation, "--target")?,
        source: required(invocation, "--source")?,
        models_root: invocation.value("--models-root"),
        manifest: invocation.value("--source-manifest"),
        preset: invocation.value("--preset"),
        policy: invocation.value("--policy"),
        imatrix: invocation.value("--imatrix-manifest"),
        backend: invocation.value("--backend"),
        component: invocation.value("--component"),
    })?)
}

fn summary(report: &ffi::variant::VariantReport) -> Fields {
    let source = &report.source;
    let plan = &report.plan;
    let writer = &report.writer;
    let component = source.kind == raw::yvex_physical_variant_kind_YVEX_PHYSICAL_VARIANT_COMPONENT;
    let mut fields = vec![
        (
            "status".into(),
            if component {
                "component-physical-variant-plan-complete"
            } else {
                "physical-variant-plan-complete"
            }
            .into(),
        ),
        ("target".into(), ffi::text(&source.target_id)),
        ("family".into(), ffi::text(&source.family)),
        ("profile".into(), ffi::text(&plan.profile_name)),
        (
            "variant_identity".into(),
            ffi::text(&plan.physical_variant_identity),
        ),
        (
            "writer_plan_identity".into(),
            ffi::text(&writer.writer_plan_identity),
        ),
        (
            "predicted_payload_bytes".into(),
            plan.encoded_bytes.to_string(),
        ),
        (
            "predicted_gguf_bytes".into(),
            writer.final_file_bytes.to_string(),
        ),
    ];
    if component {
        fields.extend([
            ("component".into(), ffi::text(&source.component_id)),
            ("source_revision".into(), ffi::text(&source.source_revision)),
            ("source_verified".into(), source.source_verified.to_string()),
            (
                "source_snapshot_identity".into(),
                ffi::text(&source.source_snapshot_identity),
            ),
            (
                "component_identity".into(),
                ffi::text(&source.component_identity),
            ),
            (
                "transform_identity".into(),
                ffi::text(&source.transform_identity),
            ),
            ("shards".into(), source.shards.to_string()),
            ("tensors".into(), source.tensors.to_string()),
            ("elements".into(), source.elements.to_string()),
            (
                "bf16_tensors".into(),
                plan.qtype_tensor_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_BF16 as usize]
                    .to_string(),
            ),
            (
                "f32_tensors".into(),
                plan.qtype_tensor_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_F32 as usize]
                    .to_string(),
            ),
            (
                "transformation_payload_bytes_read".into(),
                source.payload_execution_bytes_read.to_string(),
            ),
            ("artifact_emittable".into(), writer.complete.to_string()),
            ("component_artifact_emitted".into(), "0".into()),
            ("runtime_ready".into(), "0".into()),
            ("backend_ready".into(), "0".into()),
            ("media_generation_ready".into(), "0".into()),
            (
                "next_boundary".into(),
                "emit-and-admit-component-artifacts".into(),
            ),
        ]);
    } else {
        let cpu = plan.complete != 0
            && report
                .decisions
                .iter()
                .all(|row| row.cpu_compute_available != 0);
        let cuda = plan.complete != 0
            && report
                .decisions
                .iter()
                .all(|row| row.cuda_compute_available != 0);
        fields.extend([
            ("policy_identity".into(), ffi::text(&plan.policy_identity)),
            ("imatrix_identity".into(), ffi::text(&plan.imatrix_identity)),
            ("terminal_decisions".into(), plan.decision_count.to_string()),
            (
                "predicted_metadata_bytes".into(),
                (writer.structural_bytes + writer.pre_data_padding_bytes).to_string(),
            ),
            (
                "source_payload_bytes_read".into(),
                plan.payload_bytes_read.to_string(),
            ),
            ("artifact_emittable".into(), plan.complete.to_string()),
            ("cpu_materializable".into(), u8::from(cpu).to_string()),
            ("cuda_materializable".into(), u8::from(cuda).to_string()),
            (
                "cpu_operand_compute_available".into(),
                u8::from(cpu).to_string(),
            ),
            (
                "cuda_operand_compute_available".into(),
                u8::from(cuda).to_string(),
            ),
            (
                "runtime_qualification".into(),
                "not-claimed; plan compatibility only".into(),
            ),
        ]);
        distribution(report, &mut fields);
    }
    fields
}

fn distribution(report: &ffi::variant::VariantReport, fields: &mut Fields) {
    for (qtype, count) in report.plan.qtype_tensor_counts.iter().enumerate() {
        if *count != 0 {
            fields.push((format!("qtype_{qtype}_tensors"), count.to_string()));
            fields.push((
                format!("qtype_{qtype}_bytes"),
                report.plan.qtype_encoded_bytes[qtype].to_string(),
            ));
        }
    }
    // Group existing immutable facts in one pass; do not repeat the old O(layers*decisions) scan.
    let mut aggregates = BTreeMap::<String, u64>::new();
    for row in &report.decisions {
        for key in [
            format!("role_{}_{}_bytes", row.role, ffi::tensor_role(row.role)),
            format!("collection_{}_bytes", row.collection),
            format!("scope_{}_bytes", row.scope),
        ] {
            *aggregates.entry(key).or_default() += row.encoded_bytes;
        }
        if row.scope != raw::yvex_transform_scope_YVEX_TRANSFORM_SCOPE_GLOBAL {
            *aggregates
                .entry(format!("layer_{}_bytes", row.logical_key.layer_index))
                .or_default() += row.encoded_bytes;
        }
    }
    for scope in 0..3 {
        aggregates
            .entry(format!("scope_{scope}_bytes"))
            .or_default();
    }
    fields.extend(
        aggregates
            .into_iter()
            .map(|(key, value)| (key, value.to_string())),
    );
}

fn explain(
    report: &ffi::variant::VariantReport,
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String> {
    let mut output = String::new();
    let mut matched = 0;
    for (ordinal, row) in report.decisions.iter().enumerate() {
        let tensor = ffi::text(&row.physical_tensor_name);
        let role = ffi::tensor_role(row.role);
        if invocation
            .value("--tensor")
            .is_some_and(|value| value != tensor)
            || invocation
                .value("--role")
                .is_some_and(|value| value != role)
        {
            continue;
        }
        let fields = vec![
            ("ordinal".into(), ordinal.to_string()),
            ("role".into(), role),
            ("layer".into(), row.logical_key.layer_index.to_string()),
            ("qtype".into(), row.qtype.to_string()),
            ("rule".into(), row.policy_rule_ordinal.to_string()),
            ("priority".into(), row.policy_priority.to_string()),
            ("label".into(), ffi::text(&row.policy_label)),
            ("imatrix".into(), row.policy_requires_imatrix.to_string()),
            ("bytes".into(), row.encoded_bytes.to_string()),
            ("cpu".into(), row.cpu_compute_available.to_string()),
            ("cuda".into(), row.cuda_compute_available.to_string()),
            ("identity".into(), ffi::text(&row.decision_identity)),
        ];
        output.push_str(&render(
            &format!("DECISION  {tensor}"),
            &fields,
            width,
            styled,
        )?);
        matched += 1;
    }
    if matched == 0 {
        return Err(Box::new(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_FORMAT,
            owner: "quant.decision".into(),
            message: "selector matched no terminal".into(),
        }));
    }
    output.push_str(&format!("matched_decisions: {matched}\n"));
    Ok(output)
}

fn emission_json(
    report: &ffi::variant::VariantReport,
    result: &ffi::variant::VariantEmission,
    destination: &str,
) -> serde_json::Value {
    serde_json::json!({
        "schema": if result.binding.is_some() { "yvex.physical-production.result.v2" }
                  else { "yvex.physical-production.result.v1" }, "status": "emitted",
        "artifact_path": destination,
        "kind": if report.source.kind == raw::yvex_physical_variant_kind_YVEX_PHYSICAL_VARIANT_COMPONENT {
            "component"
        } else { "complete" },
        "identities": {
            "physical_variant": ffi::text(&report.plan.physical_variant_identity),
            "policy": ffi::text(&report.plan.policy_identity),
            "imatrix": ffi::text(&report.plan.imatrix_identity),
            "source_payload": ffi::text(&report.writer.payload_identity),
            "transformation_ir": ffi::text(&report.writer.transform_identity),
            "writer_plan": ffi::text(&report.writer.writer_plan_identity),
            "payload_plan": ffi::text(&report.writer.payload_plan_identity),
            "quant_execution": ffi::text(&result.emission.execution_identity),
            "payload_bytes": ffi::text(&result.emission.payload_byte_identity),
            "artifact": ffi::text(&result.roundtrip.artifact_identity)
        },
        "file_bytes": result.roundtrip.file_bytes,
        "payload_bytes_verified": result.roundtrip.payload_bytes_verified,
        "metadata_count": result.roundtrip.metadata_count,
        "tensor_count": result.roundtrip.tensor_count,
        "terminals_verified": result.roundtrip.terminals_verified,
        "tokenizer_tokens": report.writer.tokenizer_token_count,
        "tokenizer_merges": report.writer.tokenizer_merge_count,
        "native_roundtrip": "accepted",
        "official_reader_admission": if result.binding.is_some() { "accepted" } else { "pending" },
        "runtime_binding": result.binding.as_ref().map(|b| serde_json::json!({
            "path": b.path, "published": b.newly_published,
            "status": if b.failure.is_some() { "refused" } else { "admitted" },
            "failure": b.failure.as_ref().map(|e| serde_json::json!({
                "code": e.code, "owner": e.owner, "reason": e.message
            }))
        })), "quality": null,
        "scope": "native production result; not a reusable admission proof, independent quality qualification or runtime admission"
    })
}

fn emission(
    report: &ffi::variant::VariantReport,
    result: &ffi::variant::VariantEmission,
    destination: &str,
) -> Fields {
    let component =
        report.source.kind == raw::yvex_physical_variant_kind_YVEX_PHYSICAL_VARIANT_COMPONENT;
    let writer = &report.writer;
    let mut fields = vec![
        (
            "status".into(),
            if component {
                "component-physical-artifact-emitted"
            } else {
                "complete-physical-artifact-emitted"
            }
            .into(),
        ),
        ("artifact".into(), destination.into()),
        ("profile".into(), ffi::text(&writer.profile_name)),
        (
            "profile_identity".into(),
            ffi::text(&writer.profile_identity),
        ),
        (
            "physical_variant_identity".into(),
            ffi::text(&report.plan.physical_variant_identity),
        ),
        (
            "source_snapshot_identity".into(),
            format!("{:016x}", writer.source_snapshot_identity),
        ),
        (
            "mapping_identity".into(),
            format!("{:016x}", writer.mapping_identity),
        ),
        (
            "payload_identity".into(),
            ffi::text(&writer.payload_identity),
        ),
        (
            "transform_identity".into(),
            ffi::text(&writer.transform_identity),
        ),
        (
            "payload_plan_identity".into(),
            ffi::text(&writer.payload_plan_identity),
        ),
        (
            "artifact_identity".into(),
            ffi::text(&result.roundtrip.artifact_identity),
        ),
        (
            "writer_plan_identity".into(),
            ffi::text(&writer.writer_plan_identity),
        ),
        (
            "quant_execution_identity".into(),
            ffi::text(&result.emission.execution_identity),
        ),
        (
            "payload_byte_identity".into(),
            ffi::text(&result.emission.payload_byte_identity),
        ),
        ("file_bytes".into(), result.roundtrip.file_bytes.to_string()),
        (
            "payload_bytes".into(),
            result.roundtrip.payload_bytes_verified.to_string(),
        ),
        (
            "metadata_count".into(),
            result.roundtrip.metadata_count.to_string(),
        ),
        (
            "tensor_count".into(),
            result.roundtrip.tensor_count.to_string(),
        ),
        (
            "terminal_count".into(),
            result.roundtrip.terminals_verified.to_string(),
        ),
        (
            "tokenizer_tokens".into(),
            writer.tokenizer_token_count.to_string(),
        ),
        (
            "tokenizer_merges".into(),
            writer.tokenizer_merge_count.to_string(),
        ),
        (
            "policy_identity".into(),
            ffi::text(&report.plan.policy_identity),
        ),
        (
            "imatrix_identity".into(),
            ffi::text(&report.plan.imatrix_identity),
        ),
        ("native_roundtrip".into(), "accepted".into()),
        (
            "official_reader_admission".into(),
            if result.binding.is_some() {
                "accepted"
            } else {
                "pending"
            }
            .into(),
        ),
    ];
    if let Some(binding) = &result.binding {
        fields.push((
            "runtime_binding".into(),
            binding
                .path
                .clone()
                .unwrap_or_else(|| "refused; artifact retained".into()),
        ));
        if let Some(error) = &binding.failure {
            fields.push((
                "binding_refusal".into(),
                format!("{}: {}", error.owner, error.message),
            ));
        }
    }
    if component {
        fields.extend([
            ("component".into(), ffi::text(&report.source.component_id)),
            ("runtime_ready".into(), "0".into()),
            ("media_generation_ready".into(), "0".into()),
        ]);
    }
    fields
}

fn probe(report: &ffi::variant::VariantReport, result: &ffi::variant::VariantProbe) -> Fields {
    let metric = &result.metrics;
    let denominator = metric.finite_count as f64;
    let mean = |sum| {
        if denominator == 0.0 {
            0.0
        } else {
            sum / denominator
        }
    };
    vec![
        ("status".into(), "quant-role-probe-complete".into()),
        (
            "profile_identity".into(),
            ffi::text(&report.plan.profile_identity),
        ),
        (
            "tensor".into(),
            ffi::text(&result.decision.physical_tensor_name),
        ),
        ("role".into(), ffi::tensor_role(result.decision.role)),
        (
            "terminal_ordinal".into(),
            result.decision.terminal_ordinal.to_string(),
        ),
        ("qtype".into(), result.decision.qtype.to_string()),
        ("elements".into(), metric.element_count.to_string()),
        ("finite_elements".into(), metric.finite_count.to_string()),
        (
            "nonfinite_elements".into(),
            metric.nonfinite_count.to_string(),
        ),
        (
            "encoded_bytes".into(),
            result.execution.encoded_output_bytes.to_string(),
        ),
        (
            "payload_bytes_read".into(),
            result.execution.payload_bytes_read.to_string(),
        ),
        (
            "maximum_absolute_error".into(),
            metric.maximum_absolute_error.to_string(),
        ),
        ("rmse".into(), ffi::quant_rmse(metric).to_string()),
        (
            "mean_absolute_error".into(),
            mean(metric.absolute_error_sum).to_string(),
        ),
        (
            "mean_relative_error".into(),
            mean(metric.relative_error_sum).to_string(),
        ),
        (
            "reference_squared_sum".into(),
            metric.reference_squared_sum.to_string(),
        ),
        ("dot_reference".into(), metric.dot_reference.to_string()),
        (
            "dot_reconstructed".into(),
            metric.dot_reconstructed.to_string(),
        ),
        ("wall_seconds".into(), format!("{:.9}", result.seconds)),
    ]
}

fn execute(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let operation = invocation.operation.operation_id.as_str();
    let plan = if operation == "quant.plan" {
        required(invocation, "--out-plan")?
    } else {
        required(invocation, "--plan")?
    };
    if operation == "quant.probe" && invocation.has("--role") {
        return Err(grammar("probe requires a tensor, not a role"));
    }
    if operation == "quant.explain" && invocation.has("--tensor") == invocation.has("--role") {
        return Err(grammar(
            "explain requires exactly one --tensor/--role selector",
        ));
    }
    let destination = if operation == "quant.emit" {
        Some(required(invocation, "--out")?)
    } else {
        None
    };
    let tensor = if operation == "quant.probe" {
        Some(required(invocation, "--tensor")?)
    } else {
        None
    };
    let context = open(invocation)?;
    if operation == "quant.plan" {
        context.write_plan(plan)?;
    } else if matches!(operation, "quant.summarize" | "quant.explain") {
        context.validate_plan(plan)?;
    }
    let report = context.report()?;
    if operation == "quant.emit" {
        let destination = destination.expect("validated emission destination");
        let result = context.emit(plan, destination, invocation.value("--binding-directory"))?;
        let exit = u8::from(result.binding.as_ref().is_some_and(|b| b.failure.is_some()));
        let text = if invocation.has("--json") {
            serde_json::to_string_pretty(&emission_json(&report, &result, destination))? + "\n"
        } else {
            render(
                "PHYSICAL VARIANT  emitted",
                &emission(&report, &result, destination),
                width,
                styled,
            )?
        };
        return Ok(Output::standard(text, exit));
    }
    let output = match operation {
        "quant.plan" | "quant.summarize" => {
            render("PHYSICAL VARIANT  plan", &summary(&report), width, styled)?
        }
        "quant.explain" => explain(&report, invocation, width, styled)?,
        "quant.probe" => render(
            "PHYSICAL VARIANT  probe",
            &probe(&report, &context.probe(plan, tensor.unwrap())?),
            width,
            styled,
        )?,
        _ => unreachable!("registry-selected physical variant operation"),
    };
    Ok(Output::standard(output, 0))
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    execute(invocation, width, styled).or_else(|error| {
        if invocation.has("--json")
            && let Some(native) = error.downcast_ref::<ffi::Error>()
        {
            return Ok(Output::standard(
                serde_json::to_string_pretty(&serde_json::json!({
                    "schema": if invocation.has("--binding-directory") {
                        "yvex.physical-production.result.v2"
                    } else { "yvex.physical-production.result.v1" }, "status": "refused",
                    "code": native.code, "owner": native.owner, "reason": native.message
                }))? + "\n",
                1,
            ));
        }
        if error.is::<ffi::Error>() {
            let mut output = crate::refused_error(error.as_ref(), width, styled);
            output.exit = 1; // Established engineering-command failure contract.
            Ok(output)
        } else {
            Err(error)
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn optimization_input() -> String {
        serde_json::json!({
            "schema":"yvex.optimization.request.v1", "target":"literal-target",
            "source":"source with spaces", "models_root":"models",
            "source_manifest":"manifest.json", "goal":"memory", "backend":"cuda",
            "context":4096, "prefill":512, "allow_approximation":true
        })
        .to_string()
    }

    #[test]
    fn optimization_file_reuses_registry_and_preserves_controls() {
        let registry = crate::registry::Registry::embedded().unwrap();
        let invoke = registry
            .parse(
                &[
                    "compile",
                    "optimize",
                    "--request",
                    "goals.json",
                    "--json",
                    "--runtime-binding",
                    "binding",
                    "--execution-strategy",
                    "speculative",
                ]
                .map(String::from),
            )
            .unwrap();
        let words = optimization_file_words(
            &invoke,
            serde_json::from_str(&optimization_input()).unwrap(),
        )
        .unwrap();
        let result = registry.parse(&words).unwrap();
        assert_eq!(result.value("--source"), Some("source with spaces"));
        assert_eq!(result.value("--context"), Some("4096"));
        assert_eq!(result.value("--prefill"), Some("512"));
        assert_eq!(result.value("--goal"), Some("memory"));
        assert_eq!(result.value("--execution-strategy"), Some("speculative"));
        assert!(result.has("--allow-approximation") && result.has("--json"));
        assert!(!result.has("--request") && !result.has("--require-routed-matrix"));
    }

    #[test]
    fn optimization_file_refuses_ambiguity_and_unsupported_contracts() {
        let registry = crate::registry::Registry::embedded().unwrap();
        let invoke = registry
            .parse(&["compile", "optimize", "--request", "goals.json"].map(String::from))
            .unwrap();
        for (field, value) in [
            ("schema", serde_json::json!("yvex.optimization.request.v99")),
            ("source", serde_json::json!("--request")),
            ("source", serde_json::json!("")),
            ("source", serde_json::json!("bad\0path")),
        ] {
            let mut input: serde_json::Value = serde_json::from_str(&optimization_input()).unwrap();
            input[field] = value;
            assert!(
                optimization_file_words(&invoke, serde_json::from_value(input).unwrap()).is_err()
            );
        }
        for extra in [
            ",\"context\":4096",
            ",\"unknown\":true",
            ",\"concurrency\":-1",
        ] {
            let mut input = optimization_input();
            input.pop();
            input.push_str(extra);
            input.push('}');
            assert!(serde_json::from_str::<OptimizationFile>(&input).is_err());
        }
        let mixed = registry
            .parse(
                &[
                    "compile",
                    "optimize",
                    "--request",
                    "goals.json",
                    "--goal",
                    "quality",
                ]
                .map(String::from),
            )
            .unwrap();
        assert!(
            optimization_file_words(&mixed, serde_json::from_str(&optimization_input()).unwrap())
                .is_err()
        );
        let mut input: OptimizationFile = serde_json::from_str(&optimization_input()).unwrap();
        input.backend = Some("invented".into());
        assert!(
            registry
                .parse(&optimization_file_words(&invoke, input).unwrap())
                .is_err()
        );
    }

    #[test]
    fn production_result_is_typed_and_does_not_promote_admission() {
        let report = ffi::variant::VariantReport {
            source: Default::default(),
            plan: Default::default(),
            writer: Default::default(),
            decisions: Vec::new(),
        };
        let mut result = ffi::variant::VariantEmission {
            emission: Default::default(),
            roundtrip: Default::default(),
            binding: None,
        };
        result.roundtrip.file_bytes = 9_000_000_001;
        result.roundtrip.tensor_count = 42;
        let value = emission_json(&report, &result, "literal path \"quoted\".gguf");
        assert_eq!(value["file_bytes"].as_u64(), Some(9_000_000_001));
        assert_eq!(value["tensor_count"].as_u64(), Some(42));
        assert!(value["quality"].is_null());
        assert!(value["runtime_binding"].is_null());
        assert_eq!(value["official_reader_admission"], "pending");
        assert_eq!(
            serde_json::from_str::<serde_json::Value>(&serde_json::to_string(&value).unwrap())
                .unwrap(),
            value
        );
        result.binding = Some(ffi::variant::BindingPublication {
            path: None,
            newly_published: false,
            failure: Some(ffi::Error {
                code: -4,
                owner: "test.binding".into(),
                message: "refused".into(),
            }),
        });
        let value = emission_json(&report, &result, "retained.gguf");
        assert_eq!(value["schema"], "yvex.physical-production.result.v2");
        assert_eq!(value["status"], "emitted");
        assert_eq!(value["official_reader_admission"], "accepted");
        assert_eq!(value["runtime_binding"]["status"], "refused");
        assert!(value["quality"].is_null());
    }
}
