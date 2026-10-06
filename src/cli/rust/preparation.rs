// The product workflow composes admitted native preparation, compilation and registry owners.
use crate::{
    Output, catalog,
    ffi::{
        self,
        preparation::{Preparation, Request, View},
        raw,
    },
    presentation,
    registry::Invocation,
};
use serde_json::{Value, json};
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn request<'a>(invocation: &'a Invocation<'_>) -> Request<'a> {
    Request {
        root: invocation.value("--models-root"),
        registry: invocation.value("--registry"),
        quant: invocation.value("--quant"),
        imatrix: invocation.value("--imatrix"),
        dry: invocation.has("--dry-run"),
    }
}
fn present(value: Value, machine: bool, width: usize, styled: bool, exit: u8) -> Result<Output> {
    if machine {
        return Ok(Output::standard(format!("{value}\n"), exit));
    }
    let fields = value
        .as_object()
        .expect("preparation result object")
        .iter()
        .filter(|(name, _)| !["schema", "model"].contains(&name.as_str()))
        .map(|(name, value)| {
            (
                name.as_str(),
                value
                    .as_str()
                    .map(str::to_owned)
                    .unwrap_or_else(|| value.to_string()),
            )
        })
        .collect::<Vec<_>>();
    let pairs = fields
        .iter()
        .map(|(name, value)| (*name, value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output {
        text: presentation::record(
            &format!("MODEL  {}", value["model"].as_str().unwrap_or("unknown")),
            &pairs,
            width,
            styled,
        )?,
        exit,
        diagnostic: exit != 0,
    })
}
fn plan(view: &View, selector: &str) -> Value {
    json!({"schema": "yvex.model.prepare.v1", "model": selector,
        "state": "PLANNED", "changed": false, "source": view.source,
        "revision": view.revision, "target": view.target, "quant": view.quant,
        "backend": view.backend, "artifact": view.artifact, "profile": view.profile,
        "action": if view.rebind { "rebind" } else { "materialize" },
        "creation_reproducible": !view.rebind})
}
fn ready(view: &View, selector: &str, changed: bool, published: bool) -> Value {
    json!({"schema": "yvex.model.prepare.v1", "model": selector,
        "state": "READY", "changed": changed, "binding_published": published,
        "target": view.target, "quant": view.quant, "artifact": view.artifact,
        "runtime_binding": view.binding, "profile": view.profile,
        "action": if view.rebind { "rebound" } else { "prepared" },
        "creation_reproducible": !view.rebind})
}
fn exists(path: &str) -> Result<bool> {
    match std::fs::symlink_metadata(path) {
        Ok(_) => Ok(true),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(false),
        Err(error) => Err(error.into()),
    }
}
fn state(reason: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "model.prepare".into(),
        message: reason.into(),
    }
}
fn compile(context: &mut Preparation<'_>, imatrix: Option<&str>, index: u64) -> Result<bool> {
    let view = context.view()?;
    if exists(&view.plan)? {
        return Err(
            state("temporary plan already exists; reconcile the interrupted operation").into(),
        );
    }
    let variant = ffi::Variant::open(ffi::VariantRequest {
        target: &view.target,
        source: &view.source,
        models_root: Some(&view.models_root),
        manifest: Some(&view.manifest),
        preset: Some(&view.quant),
        policy: None,
        imatrix,
        backend: Some(&view.backend),
        component: None,
    })?;
    variant.write_plan(&view.plan)?;
    context.store_plan()?;
    if context.cached(index)? {
        return Ok(true);
    }
    let stored = context.view()?;
    if exists(&stored.artifact)? {
        context.verify_artifact()?;
    } else {
        variant.emit(&stored.plan, &stored.artifact)?;
        context.verify_artifact()?;
    }
    Ok(false)
}
fn publish_profile(view: &View, imatrix: Option<&str>) -> Result<()> {
    ffi::profile_create(
        &ffi::ProfileRequest {
            path: &view.artifact,
            alias: Some(&view.profile),
            family: Some(&view.family),
            model: Some(&view.model),
            scope: Some("runtime"),
            class: Some("prepared"),
            qprofile: Some(&view.quant),
            calibration: Some(if view.rebind {
                "historical-imatrix-identity"
            } else if imatrix.is_some() {
                "imatrix"
            } else {
                "none"
            }),
            support: Some("generation-ready"),
            expected_sha256: None,
            deployment: ffi::Deployment::Single {
                binding: &view.binding,
                target: &view.target,
                backend: &view.backend,
                strategy: &view.strategy,
                context: 4096,
            },
        },
        Some(&view.registry),
    )?;
    Ok(())
}
// Shared product composition; both the CLI and public management consume this owner.
pub(crate) fn prepare_model(selector: &str, request: Request<'_>) -> Result<Value> {
    let _lease = ffi::preparation::lock(request.root, selector)?;
    let library = ffi::Library::open(request.root, request.registry)?;
    let mut selected = None;
    for index in 0..library.count() {
        if library.matches(index, selector)? {
            if selected.is_some() {
                return Err(ffi::Error {
                    code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
                    owner: "model.selector".into(),
                    message: "model selector is ambiguous".into(),
                }
                .into());
            }
            selected = Some(index);
        }
    }
    let index = selected.ok_or_else(|| ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "model.selector".into(),
        message: "model selector not found".into(),
    })?;
    let model = library.snapshot(index)?;
    let selector = catalog::selector(&model);
    if model.entry.profile_launchable != 0 && request.quant.is_none() && request.imatrix.is_none() {
        ffi::preparation::verify_ready(&library, index, request)?;
        return Ok(json!({"schema": "yvex.model.prepare.v1", "model": selector,
            "state": "READY", "changed": false}));
    }
    let dry = request.dry;
    let imatrix = request.imatrix;
    let mut context = Preparation::open(&library, index, request).map_err(|error| ffi::Error {
        code: error.code,
        owner: "model.prepare.blocked".into(),
        message: if model
            .sources
            .first()
            .is_some_and(|source| ffi::text(&source.format).eq_ignore_ascii_case("gguf"))
        {
            concat!(
                "existing GGUF is preserved without requantization, ",
                "but this representation has no admitted runtime binding"
            )
            .into()
        } else {
            error.message
        },
    })?;
    let view = context.view()?;
    if dry {
        return Ok(plan(&view, &selector));
    }
    context.verify()?;
    if !view.rebind && compile(&mut context, imatrix, index)? {
        return Ok(ready(&context.view()?, &selector, false, false));
    }
    let published = context.binding()?;
    let view = context.view()?;
    publish_profile(&view, imatrix)?;
    Ok(ready(&view, &selector, true, published))
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    match prepare_model(&invocation.positionals[0], request(invocation)) {
        Ok(value) => present(value, invocation.has("--json"), width, styled, 0),
        Err(error) => {
            let Some(native) = error.downcast_ref::<ffi::Error>() else {
                return Err(error);
            };
            if native.owner != "model.prepare.blocked" {
                return Err(error);
            }
            present(
                json!({"schema": "yvex.model.prepare.v1", "model": invocation.positionals[0],
                "state": "BLOCKED", "changed": false, "blocker": native.message}),
                invocation.has("--json"),
                width,
                styled,
                if native.code == -10 { 2 } else { 3 },
            )
        }
    }
}

struct RecipePaths {
    source: String,
    artifact: String,
    manifest: String,
    plan: String,
    registry: String,
}
fn joined(directory: &str, leaf: &str) -> Result<String> {
    let path = std::path::Path::new(directory).join(leaf);
    crate::attention::expanded(path.to_str().ok_or_else(|| state("path is not UTF-8"))?)
}
fn recipe_paths(
    invocation: &Invocation<'_>,
    recipe: &ffi::preparation::Recipe,
) -> Result<RecipePaths> {
    use crate::attention::expanded;
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let source = if let Some(path) = invocation.value("--source") {
        expanded(path)?
    } else {
        paths.resolve(&recipe.family, "source")?.0
    };
    let (artifact, directory) = if let Some(path) = invocation.value("--out") {
        let path = expanded(path)?;
        let parent = std::path::Path::new(&path)
            .parent()
            .filter(|parent| !parent.as_os_str().is_empty())
            .unwrap_or_else(|| std::path::Path::new("."))
            .to_str()
            .ok_or_else(|| state("output parent is not UTF-8"))?
            .to_owned();
        (path, parent)
    } else {
        let directory = if let Some(path) = invocation.value("--out-dir") {
            expanded(path)?
        } else {
            paths.resolve(&recipe.family, "reference")?.0
        };
        (joined(&directory, &recipe.artifact_leaf)?, directory)
    };
    let registry = if let Some(path) = invocation.value("--registry") {
        expanded(path)?
    } else {
        ffi::registry_default_path()?
    };
    let manifest = std::str::from_utf8(raw::YVEX_SOURCE_RELEASE_MANIFEST_LEAF)
        .expect("native ASCII manifest filename")
        .trim_end_matches('\0');
    Ok(RecipePaths {
        source,
        artifact,
        manifest: joined(&directory, manifest)?,
        plan: joined(&directory, &recipe.plan_leaf)?,
        registry,
    })
}

fn recipe_convert(
    recipe: &ffi::preparation::Recipe,
    paths: &RecipePaths,
    overwrite: bool,
    plan: bool,
) -> Result<ffi::ConversionReport> {
    Ok(ffi::artifact_convert(ffi::ConversionRequest {
        architecture: &recipe.architecture,
        source: &paths.source,
        manifest: Some(&paths.manifest),
        template: None,
        policy: None,
        imatrix: None,
        out: Some(&paths.artifact),
        plan: plan.then_some(paths.plan.as_str()),
        tensor: Some(&recipe.tensor),
        qtype: Some(&recipe.qtype),
        limit: 0,
        overwrite,
        allow_unsupported: false,
        require_all: false,
    })?)
}

fn recipe_publish(recipe: &ffi::preparation::Recipe, paths: &RecipePaths) -> Result<()> {
    let receipt = ffi::profile_replace(
        &ffi::ProfileRequest {
            path: &paths.artifact,
            alias: Some(&recipe.target),
            family: None,
            model: None,
            scope: None,
            class: None,
            qprofile: None,
            calibration: None,
            support: Some("selected-tensor-materialized"),
            expected_sha256: None,
            deployment: ffi::Deployment::Inspection,
        },
        Some(&paths.registry),
    )?;
    let registered = ffi::profile_verify(&ffi::text(&receipt.alias), Some(&paths.registry))?;
    if !registered.passed {
        return Err(state("registered identity or metadata drifted after preparation").into());
    }
    Ok(())
}

fn recipe_execute(
    invocation: &Invocation<'_>,
    recipe: &ffi::preparation::Recipe,
    paths: &RecipePaths,
) -> Result<Value> {
    if !std::path::Path::new(&paths.source).exists() {
        return Err(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_IO,
            owner: "artifact.prepare".into(),
            message: "source path does not exist".into(),
        }
        .into());
    }
    if exists(&paths.artifact)? && !invocation.has("--overwrite") {
        return Err(state("artifact exists; pass --overwrite to replace it").into());
    }
    for path in [&paths.manifest, &paths.plan, &paths.artifact] {
        if let Some(parent) = std::path::Path::new(path).parent()
            && !parent.as_os_str().is_empty()
        {
            std::fs::create_dir_all(parent)?;
        }
    }
    ffi::source_manifest(ffi::ManifestRequest {
        repository: &recipe.repository,
        revision: &recipe.revision,
        local: &paths.source,
        out: &paths.manifest,
        license: None,
        card: None,
        node: None,
        dry_run: None,
        log: None,
        pid: None,
        command: None,
        status: raw::yvex_source_status_YVEX_SOURCE_STATUS_IN_PROGRESS,
    })?;
    let native = ffi::native_weights(&paths.source, Some(&recipe.tensor), 1).map_err(|error| {
        if error.code == raw::yvex_status_YVEX_ERR_FORMAT {
            state(&format!(
                "required native tensor is missing: {}",
                recipe.tensor
            ))
        } else {
            error
        }
    })?;
    recipe_convert(recipe, paths, invocation.has("--overwrite"), true)?;
    let emitted = recipe_convert(recipe, paths, invocation.has("--overwrite"), false)?;
    let view = ffi::ModelView::open(&paths.artifact)?;
    let descriptor = view.descriptor()?;
    let tensors = view.tensors()?;
    // A selected tensor and successful registration are not a runnable engine.
    if !invocation.has("--no-register") {
        recipe_publish(recipe, paths)?;
    }
    Ok(json!({"status": "model-prepare", "target": recipe.target,
        "native_tensor_count": native.summary.tensor_count,
        "artifact_tensor_count": tensors.len(), "artifact_architecture": descriptor.architecture,
        "bytes_written": emitted.bytes_written,
        "artifact": paths.artifact, "registered": !invocation.has("--no-register"),
        "runtime_execution": "not-performed", "generation": "unsupported"}))
}

pub(crate) fn artifact(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let mode = invocation.value("--output").unwrap_or("normal");
    if invocation.has("--json") || !["normal", "table", "audit"].contains(&mode) {
        return Err(crate::registry::Refusal {
            reason: "artifact preparation admits only human normal|table|audit output".into(),
            hint: None,
        }
        .into());
    }
    let recipe = match ffi::preparation::recipe(&invocation.positionals[0]) {
        Ok(recipe) => recipe,
        Err(error) if error.code == raw::yvex_status_YVEX_ERR_INVALID_ARG => {
            return dynamic_artifact(invocation, width, styled, error);
        }
        Err(error) => return Err(error.into()),
    };
    if !recipe.implemented {
        return present(
            json!({"model": recipe.target, "status": "model-prepare-unsupported",
                "reason": recipe.reason, "runtime_execution": "not-performed", "generation": "unsupported"}),
            false,
            width,
            styled,
            5,
        );
    }
    let paths = recipe_paths(invocation, &recipe)?;
    let mut result = if invocation.has("--dry-run") {
        json!({"status": "model-prepare-dry-run", "target": recipe.target,
            "convert_emit": "planned", "registration_planned": !invocation.has("--no-register"),
            "runtime_execution": "not-performed", "generation": "unsupported"})
    } else {
        recipe_execute(invocation, &recipe, &paths)?
    };
    result["model"] = json!(recipe.target);
    if mode == "audit" || invocation.has("--audit") || invocation.has("--dry-run") {
        result["source_path"] = json!(paths.source);
        result["artifact_path"] = json!(paths.artifact);
        result["source_manifest_path"] = json!(paths.manifest);
        result["conversion_plan_path"] = json!(paths.plan);
        result["registry_path"] = json!(paths.registry);
    }
    present(result, false, width, styled, 0)
}

fn dynamic_artifact(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
    unknown: ffi::Error,
) -> Result<Output> {
    let selector = &invocation.positionals[0];
    let Some(source) = ffi::catalog::acquired_target(invocation.value("--models-root"), selector)?
    else {
        return Err(unknown.into());
    };
    let rows = ffi::catalog::artifact_inventory(
        invocation.value("--models-root"),
        Some(&ffi::text(&source.family)),
    )?;
    let row = rows
        .iter()
        .find(|row| row.target_id == *selector && row.dynamic_source)
        .ok_or_else(|| {
            state("acquisition identity has no corresponding artifact discovery record")
        })?;
    let mut report = json!({"model": selector, "status": "model-prepare-unsupported",
        "family": row.family,
        "source_status": row.source_status, "artifact_status": row.artifact_status,
        "top_blocker": row.top_blocker, "next": row.next,
        "artifact_plan_status": row.artifact_class, "artifact_emission_status": "not-performed",
        "boundary": "prepare dry-run only; no artifact emission/runtime/generation"});
    if invocation.has("--audit") || invocation.value("--output") == Some("audit") {
        report.as_object_mut().expect("preparation object").extend(
            json!({"target_id": selector, "model_class_status": row.source_status,
                "tensor_map_status": row.tensor_map_status,
                "output_head_map_status": row.output_head_map_status,
                "tokenizer_map_status": row.tokenizer_map_status,
                "source_path": ffi::text(&source.local_source_dir),
                "expected_artifact_path": row.expected_path,
                "source_manifest_path": row.source_manifest_path,
                "native_inventory_path": row.native_inventory_path,
                "tensor_map_path": row.tensor_map_path,
                "output_head_map_path": row.output_head_map_path,
                "tokenizer_map_path": row.tokenizer_map_path,
                "prepare_blocker_count": row.prepare_blocker_count,
                "artifact_identity_status": "missing", "reason": row.detail,
                "runtime_execution": "not-performed", "generation": "unsupported"})
            .as_object()
            .expect("preparation audit object")
            .clone(),
        );
    }
    present(report, false, width, styled, 5)
}

const CHECK_STAGES: [&str; 15] = [
    "resolve-target",
    "resolve-artifact",
    "inspect",
    "tensors",
    "metadata",
    "registry-identity",
    "integrity-check",
    "integrity-report",
    "materialize",
    "engine",
    "session",
    "plan",
    "graph-partial",
    "model-gate",
    "materialize-gate",
];
struct CheckReport {
    target: String,
    backend: String,
    level: String,
    path: String,
    registry: String,
    kind: &'static str,
    stages: [&'static str; 15],
    stage: usize,
    graph: &'static str,
    error: Option<String>,
}
impl CheckReport {
    fn step<T>(&mut self, stage: usize, result: Result<T>) -> Result<T> {
        self.stage = stage;
        match result {
            Ok(value) => {
                self.stages[stage] = "pass";
                Ok(value)
            }
            Err(error) => {
                self.stages[stage] = "fail";
                Err(error)
            }
        }
    }
    fn render(&self, audit: bool, width: usize, styled: bool) -> Result<String> {
        let fields = vec![
            ("level", self.level.as_str()),
            ("backend", self.backend.as_str()),
            (
                "status",
                if self.error.is_none() {
                    "model-check-pass"
                } else {
                    "model-check-fail"
                },
            ),
            (
                "boundary",
                "artifact/selected-slice diagnostic; no generation executed",
            ),
        ];
        let mut text =
            presentation::record(&format!("CHECK  {}", self.target), &fields, width, styled)?;
        if let Some(error) = &self.error {
            text.push_str(&presentation::record(
                "FAILURE",
                &[
                    ("stage", CHECK_STAGES[self.stage]),
                    ("reason", error.as_str()),
                ],
                width,
                styled,
            )?);
        }
        if audit {
            let stages = CHECK_STAGES
                .iter()
                .zip(self.stages.iter())
                .map(|(name, status)| (*name, *status))
                .collect::<Vec<_>>();
            text.push_str(&presentation::record("STAGES", &stages, width, styled)?);
            text.push_str(&presentation::record(
                "IDENTITY",
                &[
                    ("artifact_path", self.path.as_str()),
                    ("registry_path", self.registry.as_str()),
                    ("model_input_kind", self.kind),
                    ("graph_partial_reason", self.graph),
                    ("runtime_execution", "not-performed"),
                    ("generation", "not-executed-by-this-check"),
                ],
                width,
                styled,
            )?);
        }
        Ok(text)
    }
}

fn check_reference(invocation: &Invocation<'_>, registry: &str) -> Result<ffi::Reference> {
    let target = &invocation.positionals[0];
    let recipe = ffi::preparation::recipe(target)
        .ok()
        .filter(|recipe| recipe.implemented);
    if let Some(recipe) = &recipe {
        if !invocation.has("--models-root")
            && let Ok(reference) = ffi::Reference::resolve_in(target, Some(registry))
        {
            return Ok(reference);
        }
        let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
        let directory = paths.resolve(&recipe.family, "reference")?.0;
        return Ok(ffi::Reference::resolve(&joined(
            &directory,
            &recipe.artifact_leaf,
        )?)?);
    }
    Ok(ffi::Reference::resolve_in(target, Some(registry))?)
}

fn check_selected_gates(reference: &ffi::Reference, backend: &str) -> Result<bool> {
    // The optional full gate belongs to the real selected-embedding recipe.
    // Smaller conversion fixtures do not masquerade as that numerical lane.
    let recipe = ffi::preparation::recipe("deepseek4-v4-flash-dspark-selected-embed")?;
    let path = reference.path()?;
    let view = ffi::ModelView::open(&path)?;
    if !view.tensors()?.iter().any(|tensor| {
        tensor.name == recipe.artifact_tensor
            && tensor.dtype == recipe.qtype
            && tensor.dimensions == recipe.gate_dims
            && tensor.storage_bytes == recipe.gate_bytes
    }) {
        return Ok(false);
    }
    let digest = reference.registered_digest()?;
    for materialization in [false, true] {
        let result = ffi::artifact_gate(ffi::GateRequest {
            path: &path,
            label: &recipe.gate_label,
            family: &recipe.architecture,
            digest: digest.as_deref(),
            metadata: "pass",
            scope: raw::yvex_materialize_scope_YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR,
            expected: Some(ffi::GateExpected {
                name: &recipe.artifact_tensor,
                dtype: &recipe.qtype,
                dims: &recipe.gate_dims,
                bytes: recipe.gate_bytes,
            }),
            cpu: backend == "cpu",
            cuda: backend == "cuda",
            require_cpu: backend == "cpu",
            require_cuda: backend == "cuda",
            repeat: 1,
            cleanup: materialization,
            materialization,
        })?;
        if let Some(failure) = result.failure {
            return Err(failure.into());
        }
        if !result.passed {
            return Err(state("selected numerical/materialization gate did not pass").into());
        }
    }
    Ok(true)
}

fn check_pipeline(invocation: &Invocation<'_>, report: &mut CheckReport) -> Result<()> {
    let reference_result = check_reference(invocation, &report.registry);
    let reference = report.step(0, reference_result)?;
    report.kind = if reference.is_alias() {
        "alias"
    } else {
        "path"
    };
    report.path = report.step(1, reference.path().map_err(Into::into))?;
    let view = report.step(2, ffi::ModelView::open(&report.path).map_err(Into::into))?;
    report.step(3, view.tensors().map_err(Into::into))?;
    report.step(4, view.metadata().map_err(Into::into))?;
    let (integrity, status) =
        report.step(6, reference.integrity(None, false, 0).map_err(Into::into))?;
    if status != 0 || integrity.passed == 0 {
        return report.step(
            6,
            Err(ffi::Error {
                code: if status == 0 { -4 } else { status },
                owner: "artifact.check".into(),
                message: "artifact integrity check failed".into(),
            }
            .into()),
        );
    }
    let identity = report.step(
        5,
        reference
            .integrity_verification(&integrity)
            .map_err(Into::into),
    )?;
    if !identity.passed {
        return report.step(5, Err(state("registry identity or metadata drift").into()));
    }
    if !reference.is_alias() {
        report.stages[5] = "unregistered";
    }
    if report.level == "quick" {
        report.graph = "quick level does not execute a graph";
        return Ok(());
    }
    let (_, supported) = report.step(
        7,
        ffi::backend_preflight(&report.backend).map_err(Into::into),
    )?;
    if !supported {
        return report.step(
            7,
            Err(ffi::Error {
                code: -5,
                owner: "artifact.check".into(),
                message: "requested backend lacks materialization capability".into(),
            }
            .into()),
        );
    }
    if invocation.has("--no-materialize") {
        report.graph = "disabled by --no-materialize";
        return Ok(());
    }
    let materialization = report.step(
        8,
        view.materialize(&report.backend, true, false)
            .map_err(Into::into),
    )?;
    if let Some(failure) = materialization.failure {
        return report.step(8, Err(failure.into()));
    }
    if !materialization.complete {
        return report.step(
            8,
            Err(state("materialization did not reach weights-materialized").into()),
        );
    }
    report.step(
        11,
        view.plan_proof(1, 16, &report.backend).map_err(Into::into),
    )?;
    report.graph = if invocation.has("--no-graph") {
        "disabled by --no-graph"
    } else {
        "legacy selected-graph diagnostic retired; no computation executed"
    };
    if report.level == "full"
        && report.step(13, check_selected_gates(&reference, &report.backend))?
    {
        report.stages[14] = "pass";
    } else {
        report.stages[13] = "skipped";
    }
    Ok(())
}

pub(crate) fn check(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let mode = invocation.value("--output").unwrap_or("normal");
    let level = invocation.value("--level").unwrap_or("quick");
    if invocation.has("--json")
        || !["normal", "table", "audit"].contains(&mode)
        || !["quick", "runtime", "full"].contains(&level)
    {
        return Err(crate::registry::Refusal {
            reason:
                "artifact check requires level quick|runtime|full and output normal|table|audit"
                    .into(),
            hint: None,
        }
        .into());
    }
    let target = &invocation.positionals[0];
    if let Ok(recipe) = ffi::preparation::recipe(target)
        && !recipe.implemented
    {
        return Ok(Output::standard(
            presentation::record(
                &format!("CHECK  {target}"),
                &[
                    ("status", "model-check-unsupported"),
                    ("reason", &recipe.reason),
                    (
                        "boundary",
                        "source-only/segment preset; no generation executed",
                    ),
                ],
                width,
                styled,
            )?,
            5,
        ));
    }
    let registry = if let Some(path) = invocation.value("--registry") {
        crate::attention::expanded(path)?
    } else {
        ffi::registry_default_path()?
    };
    let mut report = CheckReport {
        target: target.clone(),
        backend: invocation.value("--backend").unwrap_or("cpu").into(),
        level: level.into(),
        path: String::new(),
        registry,
        kind: "unknown",
        stages: ["skipped"; 15],
        stage: 0,
        graph: "not-run",
        error: None,
    };
    report.stages[..7].fill("not-run");
    let exit = match check_pipeline(invocation, &mut report) {
        Ok(()) => 0,
        Err(error) => {
            report.error = Some(error.to_string());
            error
                .downcast_ref::<ffi::Error>()
                .map_or(1, |error| crate::native_exit(error.code))
        }
    };
    if let Some(directory) = invocation.value("--report-dir") {
        let directory = crate::attention::expanded(directory)?;
        let filename = format!(
            "model-check-deepseek4-v4-flash-dspark-selected-embed-{}-{}.txt",
            report.backend, report.level
        );
        std::fs::create_dir_all(&directory)?;
        ffi::publish(
            &joined(&directory, &filename)?,
            report.render(true, 220, false)?.as_bytes(),
            true,
        )?;
    }
    Ok(Output::standard(
        report.render(mode == "audit" || invocation.has("--audit"), width, styled)?,
        exit,
    ))
}
