// Registry-admitted engineering workflows project native attention facts, not CLI text.
use crate::{
    Output, attention_projection,
    ffi::{self, execution, raw},
    presentation,
    registry::Invocation,
};
use std::{
    path::{Path, PathBuf},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn invalid(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "attention.grammar".into(),
        message: message.into(),
    }
}

fn required<'a>(invocation: &'a Invocation<'_>, name: &str) -> Result<&'a str> {
    invocation
        .value(name)
        .filter(|v| !v.is_empty())
        .ok_or_else(|| invalid(&format!("{name} is required")).into())
}

fn number(invocation: &Invocation<'_>, name: &str, default: u64, zero: bool) -> Result<u64> {
    let Some(value) = invocation.value(name) else {
        return Ok(default);
    };
    value
        .parse::<u64>()
        .ok()
        .filter(|v| zero || *v != 0)
        .ok_or_else(|| {
            invalid(&format!(
                "{name} requires a {} integer",
                if zero { "non-negative" } else { "positive" }
            ))
            .into()
        })
}

fn values(
    invocation: &Invocation<'_>,
    name: &str,
    default: &str,
    accepted: &[&str],
) -> Result<usize> {
    let value = invocation.value(name).unwrap_or(default);
    accepted
        .iter()
        .position(|candidate| *candidate == value)
        .ok_or_else(|| invalid(&format!("unsupported {name}: {value}")).into())
}

struct Options<'a> {
    action: &'a str,
    native: raw::yvex_graph_attention_operator_request,
    target: &'a str,
    backend: &'a str,
    phase: &'a str,
    mode: &'a str,
    scope: &'a str,
    output: &'a str,
}

fn action_code(action: &str) -> Result<raw::yvex_runtime_operator_action> {
    use raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_EXECUTE as execute;
    Ok(match action {
        "execute" | "compare" | "prepare" | "describe" | "benchmark.compare" => execute,
        "plan" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_PLAN,
        "capabilities" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_CAPABILITIES,
        "state.inspect" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_STATE_INSPECT,
        "state.validate" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_STATE_VALIDATE,
        "state.exercise" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_STATE_EXERCISE,
        "residency.inspect" => {
            raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_RESIDENCY_INSPECT
        }
        "capture" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_CAPTURE,
        "replay" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_REPLAY,
        "cuda_graph.list" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_GRAPH_LIST,
        "cuda_graph.inspect" => {
            raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_GRAPH_INSPECT
        }
        "cuda_graph.warmup" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_GRAPH_WARMUP,
        "cuda_graph.update" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_GRAPH_UPDATE,
        "cuda_graph.invalidate" => {
            raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_GRAPH_INVALIDATE
        }
        "cuda_graph.release" => {
            raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_GRAPH_RELEASE
        }
        "trace" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_TRACE,
        "profile" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_PROFILE,
        "benchmark" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_BENCHMARK,
        "qualify" => raw::yvex_runtime_operator_action_YVEX_RUNTIME_OPERATOR_QUALIFY,
        _ => return Err(invalid("registry attention operation has no typed adapter").into()),
    })
}

fn options<'a>(invocation: &'a Invocation<'_>) -> Result<Options<'a>> {
    let action = invocation
        .operation
        .operation_id
        .strip_prefix("execute.graph.attention.")
        .unwrap();
    let cuda = matches!(action, "capture" | "replay") || action.starts_with("cuda_graph.");
    let state = action.starts_with("state.");
    let backend = invocation.value("--backend").unwrap_or(if cuda {
        "cuda"
    } else if state {
        "cpu"
    } else {
        ""
    });
    let phase = invocation
        .value("--phase")
        .unwrap_or(if action == "state.exercise" {
            "prefill"
        } else {
            "decode"
        });
    let mode = invocation
        .value("--mode")
        .unwrap_or(if cuda { "full" } else { "eager" });
    let mut native = raw::yvex_graph_attention_operator_request {
        operator_action: action_code(action)?,
        compare_backends: (action == "compare" || invocation.has("--compare-backends")).into(),
        require_mode: invocation.has("--require-mode").into(),
        token_count: number(
            invocation,
            "--tokens",
            if action == "state.exercise" { 4 } else { 1 },
            false,
        )?,
        chunk_tokens: number(invocation, "--chunk-tokens", 0, false)?,
        context_capacity: number(invocation, "--context-capacity", 0, false)?,
        warmup: number(invocation, "--warmup", 0, true)?,
        repeat: number(invocation, "--repeat", 1, false)?,
        layer_start: number(
            invocation,
            "--layer",
            number(invocation, "--layer-start", 0, true)?,
            true,
        )?,
        layer_count: if invocation.has("--layer") {
            1
        } else {
            number(invocation, "--layer-count", 0, false)?
        },
        select_layer: (invocation.has("--layer") || invocation.has("--layer-start")).into(),
        history_tokens: number(
            invocation,
            "--history-tokens",
            number(invocation, "--position", 0, true)?,
            true,
        )?,
        maximum_host_bytes: number(invocation, "--max-host-bytes", 0, false)?,
        maximum_device_bytes: number(invocation, "--max-device-bytes", 0, false)?,
        ..Default::default()
    };
    native.backend = match backend {
        "" | "cpu" => raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU,
        "cuda" => raw::yvex_backend_kind_YVEX_BACKEND_KIND_CUDA,
        "metal" => raw::yvex_backend_kind_YVEX_BACKEND_KIND_METAL,
        _ => return Err(invalid("unknown backend kind").into()),
    };
    native.phase = match phase {
        "prefill" => raw::yvex_execution_phase_YVEX_EXECUTION_PHASE_PREFILL,
        "decode" => raw::yvex_execution_phase_YVEX_EXECUTION_PHASE_DECODE,
        "mixed" => raw::yvex_execution_phase_YVEX_EXECUTION_PHASE_MIXED,
        "verify" => raw::yvex_execution_phase_YVEX_EXECUTION_PHASE_VERIFY,
        _ => return Err(invalid("unsupported attention phase").into()),
    };
    native.mode = match mode {
        "eager" => raw::yvex_runtime_execution_mode_YVEX_RUNTIME_MODE_EAGER,
        "piecewise" => raw::yvex_runtime_execution_mode_YVEX_RUNTIME_MODE_PIECEWISE,
        "full" => raw::yvex_runtime_execution_mode_YVEX_RUNTIME_MODE_FULL,
        "auto" => raw::yvex_runtime_execution_mode_YVEX_RUNTIME_MODE_AUTO,
        _ => return Err(invalid("unsupported attention mode").into()),
    };
    native.operation_scope = values(
        invocation,
        "--operation-scope",
        "core",
        &["core", "envelope", "release-attention-set"],
    )? as u32;
    native.trace_policy = values(
        invocation,
        "--trace-level",
        if action == "trace" { "summary" } else { "none" },
        &["none", "summary", "stages", "full"],
    )? as u32;
    // Explicit trace none on the trace action has always selected summary.
    if action == "trace" && native.trace_policy == 0 {
        native.trace_policy = 1;
    }
    let scope = invocation.value("--scope").unwrap_or("quick");
    native.scope = match scope {
        "quick" => raw::yvex_attention_probe_scope_YVEX_ATTENTION_PROBE_SCOPE_QUICK,
        "full" => raw::yvex_attention_probe_scope_YVEX_ATTENTION_PROBE_SCOPE_FULL,
        _ => return Err(invalid("unsupported attention scope").into()),
    };
    values(invocation, "--probe", "canonical", &["canonical"])?;
    values(invocation, "--progress", "auto", &["auto", "plain", "off"])?;
    native.probe = if invocation.has("--input-file") {
        raw::yvex_attention_probe_kind_YVEX_ATTENTION_PROBE_UNSPECIFIED
    } else {
        raw::yvex_attention_probe_kind_YVEX_ATTENTION_PROBE_CANONICAL_V2
    };
    let output = if invocation.has("--json") {
        "json"
    } else if invocation.has("--audit") {
        "audit"
    } else {
        invocation.value("--output").unwrap_or("normal")
    };
    if !["normal", "table", "audit", "json", "csv"].contains(&output) {
        return Err(invalid("unknown output mode").into());
    }
    let result = Options {
        action,
        native,
        target: if action == "benchmark.compare" {
            "not_applicable"
        } else {
            required(invocation, "--target")?
        },
        backend,
        phase,
        mode,
        scope,
        output,
    };
    validate(invocation, &result)?;
    Ok(result)
}

fn validate(invocation: &Invocation<'_>, options: &Options<'_>) -> Result<()> {
    let action = options.action;
    let executes = !matches!(
        action,
        "prepare"
            | "describe"
            | "capabilities"
            | "plan"
            | "state.inspect"
            | "state.validate"
            | "residency.inspect"
            | "benchmark.compare"
    );
    let cuda = matches!(action, "capture" | "replay") || action.starts_with("cuda_graph.");
    let unsupported = |message: &str| -> Result<()> { Err(invalid(message).into()) };
    if cuda && (options.backend != "cuda" || options.mode == "eager") {
        return unsupported("CUDA graph actions require CUDA piecewise, full, or auto mode");
    }
    if executes
        && action != "compare"
        && (options.native.compare_backends != 0) == !options.backend.is_empty()
    {
        return unsupported(
            "executing attention requires exactly one backend or --compare-backends",
        );
    }
    if action == "compare" && !options.backend.is_empty() {
        return unsupported("compare does not accept --backend");
    }
    if matches!(
        action,
        "capabilities" | "plan" | "residency.inspect" | "qualify"
    ) && options.backend.is_empty()
    {
        return unsupported("attention inspection requires --backend");
    }
    if invocation.has("--runtime-binding") && invocation.has("--runtime-binding-dir") {
        return unsupported("--runtime-binding conflicts with --runtime-binding-dir");
    }
    if invocation.has("--quant-policy") && invocation.has("--quant-preset") {
        return unsupported("--quant-policy conflicts with --quant-preset");
    }
    let variant = [
        "--quant-policy",
        "--quant-preset",
        "--imatrix-manifest",
        "--physical-variant-plan",
    ];
    if action != "prepare"
        && variant
            .iter()
            .chain(["--source", "--source-manifest"].iter())
            .any(|v| invocation.has(v))
    {
        return unsupported("source/physical-variant options require bench attention prepare");
    }
    let policy = invocation.has("--quant-policy") || invocation.has("--quant-preset");
    if invocation.has("--physical-variant-plan") && !policy {
        return unsupported("--physical-variant-plan requires a compile quant policy or preset");
    }
    if !invocation.has("--physical-variant-plan")
        && (policy || invocation.has("--imatrix-manifest"))
    {
        return unsupported("variant policy and imatrix require --physical-variant-plan");
    }
    if invocation
        .value("--input")
        .is_some_and(|v| v != "tensor-file")
    {
        return unsupported("attention input must be tensor-file");
    }
    if invocation.has("--input") != invocation.has("--input-file") {
        return unsupported("--input tensor-file and --input-file are required together");
    }
    if invocation.has("--input")
        && (action != "execute"
            || options.phase != "prefill"
            || options.mode != "eager"
            || options.scope != "full")
    {
        return unsupported("tensor-file input requires execute prefill/eager/full");
    }
    if !invocation.has("--input")
        && (invocation.has("--chunk-tokens") || invocation.has("--context-capacity"))
    {
        return unsupported("prefill chunk controls require tensor-file input");
    }
    if invocation.has("--layer")
        && (invocation.has("--layer-start") || invocation.has("--layer-count"))
    {
        return unsupported("--layer conflicts with layer range");
    }
    if invocation.has("--layer-start") != invocation.has("--layer-count") {
        return unsupported("--layer-start and --layer-count are required together");
    }
    if invocation.has("--layer-count") && options.native.layer_count != 1 {
        return unsupported("multi-layer ranges are unavailable until range-aware publication");
    }
    if options.native.select_layer != 0 && !executes && action != "plan" {
        return unsupported("layer selection requires an executing attention operation or plan");
    }
    if invocation.has("--class")
        && (options.native.select_layer != 0 || !matches!(action, "state.exercise" | "benchmark"))
    {
        return unsupported(
            "--class requires state exercise or benchmark without explicit layer selection",
        );
    }
    if (invocation.has("--position") || invocation.has("--history-tokens"))
        && !matches!(action, "state.exercise" | "benchmark")
    {
        return unsupported("history geometry requires state exercise or benchmark");
    }
    if invocation.has("--position")
        && invocation.has("--history-tokens")
        && number(invocation, "--position", 0, true)? != options.native.history_tokens
    {
        return unsupported("--position and --history-tokens must agree");
    }
    if [
        "--local-capacity",
        "--compressed-capacity",
        "--indexer-capacity",
    ]
    .iter()
    .any(|v| invocation.has(v))
    {
        return unsupported(
            "explicit state capacities are not admitted by the persistent state provider",
        );
    }
    if invocation.has("--max-regression-bps") && action != "benchmark.compare" {
        return unsupported("regression thresholds require benchmark compare");
    }
    if ["--baseline", "--write-baseline", "--chart"]
        .iter()
        .any(|v| invocation.has(v))
        && !matches!(action, "benchmark" | "profile" | "benchmark.compare")
    {
        return unsupported("baseline/chart options require benchmark or profile");
    }
    if invocation.has("--write-baseline") && !invocation.has("--baseline") {
        return unsupported("--write-baseline requires --baseline FILE");
    }
    if invocation.has("--current") && action != "benchmark.compare" {
        return unsupported("--current requires benchmark compare");
    }
    if invocation
        .value("--chart")
        .is_some_and(|v| !v.ends_with(".svg") || v.len() < 5)
    {
        return unsupported("--chart requires a .svg path");
    }
    if options.native.trace_policy != 0 && !executes {
        return unsupported("trace requires an executing attention operation");
    }
    if invocation.has("--max-device-bytes") && (options.backend == "cpu" || !executes) {
        return unsupported("--max-device-bytes requires accelerator execution");
    }
    if invocation.has("--capture-bucket") && (options.backend != "cuda" || options.mode == "eager")
    {
        return unsupported("--capture-bucket requires CUDA graph mode");
    }
    if invocation.has("--max-host-bytes") && matches!(action, "prepare" | "describe") {
        return unsupported("--max-host-bytes requires a runtime model or execution session");
    }
    if invocation.has("--require-mode") && !executes {
        return unsupported("--require-mode requires execution");
    }
    Ok(())
}

pub(crate) fn expanded(path: &str) -> Result<String> {
    let result = if path == "~" || path.starts_with("~/") {
        PathBuf::from(
            std::env::var_os("HOME")
                .ok_or_else(|| invalid("HOME is unavailable for path expansion"))?,
        )
        .join(path.strip_prefix("~/").unwrap_or(""))
    } else {
        PathBuf::from(path)
    };
    let result = result
        .to_str()
        .ok_or_else(|| invalid("path must be valid UTF-8"))?;
    if result.is_empty() || result.len() >= raw::YVEX_PATH_CAP as usize {
        return Err(invalid("path exceeds native bounds").into());
    }
    Ok(result.into())
}

fn discover(directory: &str) -> Result<String> {
    use rustix::fs::{AtFlags, Dir, FileType, Mode, OFlags, open, statat};
    let fd = open(
        directory,
        OFlags::RDONLY | OFlags::DIRECTORY | OFlags::NOFOLLOW | OFlags::CLOEXEC,
        Mode::empty(),
    )
    .map_err(|_| ffi::Error {
        code: -3,
        owner: "attention.binding".into(),
        message: "runtime binding is missing; run yvex bench attention prepare".into(),
    })?;
    let entries = Dir::read_from(&fd)?;
    let mut selected = Vec::new();
    for entry in entries {
        let entry = entry?;
        let name = entry.file_name();
        let bytes = name.to_bytes();
        if bytes.len() >= 96 || !bytes.ends_with(b".yvex-runtime-binding") || bytes.len() <= 21 {
            continue;
        }
        let Ok(metadata) = statat(&fd, name, AtFlags::SYMLINK_NOFOLLOW) else {
            continue;
        };
        if FileType::from_raw_mode(metadata.st_mode) == FileType::RegularFile {
            selected.push(name.to_str()?.to_owned());
        }
    }
    if selected.len() != 1 {
        return Err(ffi::Error {
            code: if selected.is_empty() { -3 } else { -8 },
            owner: "attention.binding".into(),
            message: "runtime binding registry is missing or ambiguous; use --runtime-binding FILE"
                .into(),
        }
        .into());
    }
    expanded(
        Path::new(directory)
            .join(&selected[0])
            .to_str()
            .ok_or_else(|| invalid("invalid binding directory"))?,
    )
}

struct Paths {
    artifact: String,
    binding: String,
    directory: String,
    source: String,
    manifest: String,
    models: String,
}

fn paths(invocation: &Invocation<'_>, family: &execution::Family, prepare: bool) -> Result<Paths> {
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let gguf = paths.resolve(&family.key, "gguf")?.0;
    let artifact = expanded(
        invocation
            .value("--artifact")
            .unwrap_or(&format!("{gguf}/{}", family.artifact)),
    )?;
    let directory = expanded(
        invocation
            .value("--runtime-binding-dir")
            .unwrap_or(&format!(
                "{}/runtime/{}",
                ffi::text(&paths.operator.registry_root),
                family.key
            )),
    )?;
    let binding = if prepare {
        String::new()
    } else if let Some(path) = invocation.value("--runtime-binding") {
        expanded(path)?
    } else {
        discover(&directory)?
    };
    let source = if prepare {
        expanded(
            invocation
                .value("--source")
                .unwrap_or(&paths.resolve(&family.key, "source")?.0),
        )?
    } else {
        String::new()
    };
    let manifest = if prepare {
        expanded(
            invocation
                .value("--source-manifest")
                .unwrap_or(&format!("{gguf}/{}", family.manifest)),
        )?
    } else {
        String::new()
    };
    Ok(Paths {
        artifact,
        binding,
        directory,
        source,
        manifest,
        models: ffi::text(&paths.operator.models_root),
    })
}

fn external(path: &str, destination: bool) -> Result<String> {
    let path = expanded(path)?;
    let target = Path::new(&path);
    let root = Path::new(env!("YVEX_BUILD_SOURCE_ROOT"));
    let parent = target
        .parent()
        .ok_or_else(|| invalid("benchmark path has no parent"))?;
    if !target.is_absolute()
        || parent
            .canonicalize()
            .map_err(|_| invalid("benchmark parent is unavailable"))?
            != parent
        || target.starts_with(root)
        || target.file_name().is_none()
    {
        return Err(invalid(
            "benchmark assets require canonical absolute paths outside the source repository",
        )
        .into());
    }
    match target.symlink_metadata() {
        Ok(metadata) if metadata.is_symlink() || !metadata.is_file() => {
            Err(invalid("benchmark asset must be a regular file").into())
        }
        Ok(_) if destination => Err(ffi::Error {
            code: -8,
            owner: "benchmark.publication".into(),
            message: "benchmark output path already exists".into(),
        }
        .into()),
        Ok(_) => Ok(path),
        Err(error) if destination && error.kind() == std::io::ErrorKind::NotFound => Ok(path),
        Err(error) => Err(invalid(&format!("benchmark asset path is unavailable: {error}")).into()),
    }
}

fn preflight(invocation: &Invocation<'_>, action: &str) -> Result<()> {
    if action == "benchmark.compare" {
        // Validate the entire static argument contract before touching assets.
        required(invocation, "--baseline")?;
        required(invocation, "--current")?;
        external(required(invocation, "--baseline")?, false)?;
        external(required(invocation, "--current")?, false)?;
    } else if matches!(action, "benchmark" | "profile")
        && let Some(path) = invocation.value("--baseline")
    {
        external(path, invocation.has("--write-baseline"))?;
    }
    if let Some(path) = invocation.value("--chart") {
        external(path, true)?;
    }
    Ok(())
}

fn binding_result(
    options: &Options<'_>,
    paths: &Paths,
    family: &execution::Family,
    summary: &raw::yvex_runtime_binding_summary,
) -> Box<raw::yvex_graph_attention_operator_result> {
    let mut result = Box::<raw::yvex_graph_attention_operator_result>::default();
    execution::copy_text(&mut result.status, "complete");
    execution::copy_text(&mut result.target, options.target);
    execution::copy_text(&mut result.family, &family.name);
    execution::copy_text(
        &mut result.backend,
        if options.backend.is_empty() {
            "not_applicable"
        } else {
            options.backend
        },
    );
    execution::copy_text(
        &mut result.scope,
        if options.action == "prepare" {
            "preparation"
        } else {
            options.scope
        },
    );
    execution::copy_text(&mut result.artifact_path, &paths.artifact);
    execution::copy_text(&mut result.runtime_binding_path, &paths.binding);
    result.runtime_binding_identity = summary.identity;
    result.artifact_identity = summary.artifact_identity;
    result.materialization_identity = summary.materialization_identity;
    result.logical_model_identity = summary.logical_model_identity;
    result.runtime_numeric_identity = summary.runtime_numeric_identity;
    result.runtime_descriptor_identity = summary.runtime_descriptor_identity;
    result.attention_plan_identity = summary.attention_plan_identity;
    result.semantic_graph_identity = summary.semantic_graph_identity;
    result.executable_graph_identity = summary.executable_graph_identity;
    result.main_layers_total = summary.layer_count;
    result.operator_command_available = 1;
    result.production_api_available = 1;
    if options.action == "describe" {
        execution::copy_text(&mut result.operation_scope, "core");
        execution::copy_text(
            &mut result.operation_scope,
            &options_scope(options.native.operation_scope),
        );
        execution::copy_text(&mut result.phase, options.phase);
        execution::copy_text(&mut result.requested_mode, options.mode);
        execution::copy_text(&mut result.selection_reason, "not_applicable");
        result.artifact_transform_identity = summary.artifact_transform_identity;
        result.logical_transform_identity = summary.logical_transform_identity;
        result.internal_live_runner_available = 1;
    }
    result
}

fn options_scope(scope: u32) -> String {
    ["core", "envelope", "release-attention-set"]
        .get(scope as usize)
        .unwrap_or(&"unknown")
        .to_string()
}

fn refused_result(result: &mut raw::yvex_graph_attention_operator_result, error: &ffi::Error) {
    result.completed = 0;
    execution::copy_text(&mut result.status, "refused");
    execution::copy_text(&mut result.failure_code, &ffi::status_name(error.code));
    execution::copy_text(&mut result.failure_where, &error.owner);
    execution::copy_text(&mut result.reason, &error.message);
}

fn present(
    invocation: &Invocation<'_>,
    options: &Options<'_>,
    result: &mut raw::yvex_graph_attention_operator_result,
    failure: Option<&ffi::Error>,
    width: usize,
    styled: bool,
) -> Result<Output> {
    execution::copy_text(
        &mut result.command,
        &invocation.operation.command_path.join(" "),
    );
    let exit = failure.map_or(0, |e| crate::native_exit(e.code));
    let text = match options.output {
        "json" => attention_projection::json(result),
        "csv" => attention_projection::csv(result),
        _ => {
            let all = attention_projection::fields(result);
            let selected = all
                .iter()
                .filter(|(key, _)| {
                    options.output == "audit"
                        || [
                            "status",
                            "target",
                            "backend",
                            "phase",
                            "selected_mode",
                            "scope",
                            "layers_executed",
                            "benchmark_sample_count",
                            "benchmark_p50_seconds",
                            "reason",
                            "failure_code",
                        ]
                        .contains(key)
                })
                .map(|(key, value)| {
                    (
                        *key,
                        value
                            .as_str()
                            .map(str::to_owned)
                            .unwrap_or_else(|| value.to_string()),
                    )
                })
                .filter(|(_, value)| !value.is_empty())
                .collect::<Vec<_>>();
            let pairs = selected
                .iter()
                .map(|(key, value)| (*key, value.as_str()))
                .collect::<Vec<_>>();
            presentation::record("ATTENTION", &pairs, width, styled)?
        }
    };
    if let Some(error) = failure {
        use std::io::Write;
        let diagnostic = presentation::record(
            "ATTENTION refused",
            &[
                ("code", &ffi::status_name(error.code)),
                ("owner", &error.owner),
                ("reason", &error.message),
            ],
            width,
            styled,
        )?;
        std::io::stderr().write_all(diagnostic.as_bytes())?;
    }
    Ok(Output::standard(text, exit))
}

pub(crate) struct Signals {
    pub(crate) cancel: Arc<AtomicBool>,
    ids: Vec<signal_hook::SigId>,
}
impl Signals {
    pub(crate) fn new() -> Result<Self> {
        let mut result = Self {
            cancel: Arc::new(AtomicBool::new(false)),
            ids: Vec::new(),
        };
        for signal in [signal_hook::consts::SIGINT, signal_hook::consts::SIGTERM] {
            result
                .ids
                .push(signal_hook::flag::register(signal, result.cancel.clone())?);
        }
        Ok(result)
    }
}
impl Drop for Signals {
    fn drop(&mut self) {
        for id in self.ids.drain(..) {
            signal_hook::low_level::unregister(id);
        }
    }
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let mut options = options(invocation)?;
    if options.action == "benchmark.compare" {
        return compare(invocation, &options, width, styled);
    }
    preflight(invocation, options.action)?;
    let family = execution::Family::find(options.target)?;
    if family.is_none() && options.action == "prepare" {
        return Err(ffi::Error {
            code: -5,
            owner: "attention.prepare".into(),
            message: "unsupported attention target".into(),
        }
        .into());
    }
    if let Some(class) = invocation.value("--class")
        && let Some(family) = family.as_ref()
    {
        options.native.selection_key = family.select(class)?;
        options.native.select_selection_key = 1;
    }
    if family.is_some() {
        execution::selection(&options.native)?;
    }
    let paths = family
        .as_ref()
        .map(|family| paths(invocation, family, options.action == "prepare"))
        .transpose()?;
    if options.action == "prepare" || options.action == "describe" {
        let family = family.ok_or_else(|| invalid("unsupported attention target"))?;
        let mut paths = paths.expect("registered target paths");
        if options.action == "prepare" {
            let policy = invocation
                .value("--quant-policy")
                .map(expanded)
                .transpose()?;
            let imatrix = invocation
                .value("--imatrix-manifest")
                .map(expanded)
                .transpose()?;
            let plan = invocation
                .value("--physical-variant-plan")
                .map(expanded)
                .transpose()?;
            paths.binding = family.prepare([
                Some(&paths.source),
                Some(&paths.models),
                Some(&paths.manifest),
                Some(&paths.artifact),
                Some(&paths.directory),
                policy.as_deref(),
                invocation.value("--quant-preset"),
                imatrix.as_deref(),
                plan.as_deref(),
            ])?;
        }
        let summary = execution::binding(&paths.binding)?;
        let mut result = binding_result(&options, &paths, &family, &summary);
        return present(invocation, &options, &mut result, None, width, styled);
    }
    let signals = Signals::new()?;
    let mut progress = feedback(invocation, width, styled);
    let mut run = execution::attention(
        options.native,
        [
            Some(options.target),
            Some(paths.as_ref().map_or("", |v| v.artifact.as_str())),
            paths.as_ref().map(|v| v.binding.as_str()),
            invocation.value("--input-file"),
            invocation.value("--capture-bucket"),
            invocation.value("--class"),
        ],
        signals.cancel.clone(),
        &mut progress,
    )?;
    if signals.cancel.load(Ordering::Relaxed) && run.failure.is_none() {
        run.failure = Some(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_CANCELLED,
            owner: "attention.publication".into(),
            message: "attention execution cancelled before command publication".into(),
        });
        refused_result(&mut run.result, run.failure.as_ref().unwrap());
    }
    if run.failure.is_none()
        && matches!(options.action, "benchmark" | "profile")
        && let Err(error) = benchmark_output(invocation, &mut run.result)
    {
        refused_result(&mut run.result, &error);
        run.failure = Some(error);
    }
    if let Err(error) = run.finish() {
        refused_result(&mut run.result, &error);
        run.failure = Some(error);
    }
    if run.failure.is_none() && run.result.completed == 0 {
        run.failure = Some(ffi::Error {
            code: -8,
            owner: "attention.publication".into(),
            message: "attention execution returned an incomplete result".into(),
        });
    }
    if run.result.status[0] == 0
        && let Some(error) = run.failure.take()
    {
        return Err(error.into());
    }
    present(
        invocation,
        &options,
        &mut run.result,
        run.failure.as_ref(),
        width,
        styled,
    )
}

fn feedback(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> impl FnMut(u32, u64, u64) -> bool {
    use std::io::{IsTerminal, Write};
    let enabled = invocation.value("--progress") == Some("plain")
        || (invocation.value("--progress").unwrap_or("auto") == "auto"
            && std::io::stderr().is_terminal());
    let mut previous = None;
    move |phase, completed, total| {
        if !enabled || previous == Some((phase, completed, total)) {
            return true;
        }
        previous = Some((phase, completed, total));
        let state = format!("phase={phase} {completed}/{total}");
        let Ok(text) =
            presentation::record("ATTENTION progress", &[("work", &state)], width, styled)
        else {
            return false;
        };
        std::io::stderr().write_all(text.as_bytes()).is_ok()
    }
}

fn comparison_apply(
    summary: &mut raw::yvex_runtime_benchmark_operator_summary,
    comparison: &raw::yvex_runtime_benchmark_comparison,
) {
    summary.baseline_compatible = comparison.compatible;
    summary.baseline_identity = comparison.baseline_identity;
    summary.baseline_commit = comparison.baseline_commit;
    summary.baseline_source_state = comparison.baseline_source_state;
    summary.current_commit = comparison.current_commit;
    summary.current_source_state = comparison.current_source_state;
    summary.regression_policy_identity = comparison.regression_policy_identity;
    summary.comparison_identity = comparison.comparison_identity;
    summary.regression_basis_points = comparison.regression_policy.basis_points;
    summary.regression_policy_enabled = comparison.regression_policy.enabled;
    summary.performance_passed = comparison.performance_passed;
    let scale = 1.0 / 1e9;
    summary.cold_delta_seconds = comparison.cold_total_delta_ns as f64 * scale;
    summary.device_timing_available = comparison.device_timing_available;
    for index in 0..summary.host_delta_seconds.len() {
        summary.host_delta_seconds[index] = comparison.host_delta_ns[index] as f64 * scale;
        summary.device_delta_seconds[index] = comparison.device_delta_ns[index] as f64 * scale;
    }
}

fn chart_apply(
    summary: &mut raw::yvex_runtime_benchmark_operator_summary,
    chart: &raw::yvex_runtime_benchmark_chart_result,
) {
    summary.chart_generated = 1;
    summary.chart_identity = chart.identity;
    summary.chart_file_bytes = chart.file_bytes;
    summary.chart_path = chart.path;
}

fn benchmark_output(
    invocation: &Invocation<'_>,
    result: &mut raw::yvex_graph_attention_operator_result,
) -> std::result::Result<(), ffi::Error> {
    let current = execution::baseline_from_attention(result)?;
    result.benchmark.identity = current.identity;
    result.benchmark.current_commit = current.key.commit;
    result.benchmark.current_source_state = current.key.build_source_state;
    let mut baseline = None;
    if let Some(path) = invocation.value("--baseline") {
        let path = expanded(path).map_err(|e| invalid(&e.to_string()))?;
        execution::copy_text(&mut result.benchmark.path, &path);
        if invocation.has("--write-baseline") {
            let published = execution::baseline_write(&path, &current)?;
            result.benchmark.baseline_written = 1;
            result.benchmark.file_bytes = published.file_bytes;
        } else {
            baseline = Some(execution::baseline_open(&path)?);
            comparison_apply(
                &mut result.benchmark,
                &execution::baseline_compare(&current, baseline.as_ref().unwrap(), None)?,
            );
        }
    }
    if let Some(path) = invocation.value("--chart") {
        let path = expanded(path).map_err(|e| invalid(&e.to_string()))?;
        chart_apply(
            &mut result.benchmark,
            &execution::chart(&path, &current, baseline.as_ref())?,
        );
    }
    Ok(())
}

fn compare(
    invocation: &Invocation<'_>,
    options: &Options<'_>,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let mut result = Box::<raw::yvex_graph_attention_operator_result>::default();
    result.completed = 1;
    execution::copy_text(&mut result.status, "complete");
    for field in [&mut result.target, &mut result.command] {
        execution::copy_text(field, "not_applicable");
    }
    for field in [&mut result.backend, &mut result.operation_scope] {
        execution::copy_text(field, "not_applicable");
    }
    execution::copy_text(&mut result.phase, "not_applicable");
    for field in [&mut result.requested_mode, &mut result.selected_mode] {
        execution::copy_text(field, "not_applicable");
    }
    execution::copy_text(&mut result.scope, "attention_component");
    execution::copy_text(&mut result.benchmark_scope, "attention_component");
    execution::copy_text(&mut result.trace_policy, "none");
    execution::copy_text(&mut result.selection_reason, "identity_validation_required");
    for (kind, state) in [
        (
            raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_COMPONENT_BENCHMARK,
            "compared",
        ),
        (
            raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_CORRECTNESS,
            "not_evaluated",
        ),
        (
            raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_STRUCTURAL,
            "not_evaluated",
        ),
        (
            raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_PERFORMANCE,
            "measured",
        ),
    ] {
        execution::copy_text(&mut result.quality_status[kind as usize], state);
    }
    let outcome = (|| -> Result<bool> {
        preflight(invocation, options.action)?;
        let current = execution::baseline_open(required(invocation, "--current")?)?;
        let baseline = execution::baseline_open(required(invocation, "--baseline")?)?;
        let threshold = invocation
            .has("--max-regression-bps")
            .then(|| number(invocation, "--max-regression-bps", 0, true))
            .transpose()?;
        let comparison = execution::baseline_compare(&current, &baseline, threshold)?;
        result.benchmark.identity = current.identity;
        comparison_apply(&mut result.benchmark, &comparison);
        execution::copy_text(
            &mut result.benchmark.path,
            required(invocation, "--baseline")?,
        );
        if let Some(path) = invocation.value("--chart") {
            chart_apply(
                &mut result.benchmark,
                &execution::chart(path, &current, Some(&baseline))?,
            );
        }
        execution::copy_text(
            &mut result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_PERFORMANCE as usize],
            if threshold.is_none() {
                "measured"
            } else if comparison.performance_passed != 0 {
                "pass"
            } else {
                "regressed"
            },
        );
        Ok(threshold.is_some() && comparison.performance_passed == 0)
    })();
    match outcome {
        Ok(regressed) => {
            let mut output = present(invocation, options, &mut result, None, width, styled)?;
            output.exit = regressed.into();
            Ok(output)
        }
        Err(error) => {
            let native = error
                .downcast_ref::<ffi::Error>()
                .cloned()
                .unwrap_or_else(|| invalid(&error.to_string()));
            refused_result(&mut result, &native);
            execution::copy_text(
                &mut result.quality_status
                    [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_COMPONENT_BENCHMARK
                        as usize],
                "unavailable",
            );
            execution::copy_text(
                &mut result.quality_status
                    [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_PERFORMANCE as usize],
                "not_measured",
            );
            present(
                invocation,
                options,
                &mut result,
                Some(&native),
                width,
                styled,
            )
        }
    }
}
