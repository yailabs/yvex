// Product management adapter over source/catalog/compiler owners. Never a CLI dispatcher.
use crate::{
    acquire, acquisition, catalog, client,
    ffi::{self, acquisition as native, raw},
    preparation,
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::{cell::Cell, time::Duration};

type Result<T> = std::result::Result<T, ffi::Error>;
fn invalid(reason: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "management.models".into(),
        message: reason.into(),
    }
}
fn unavailable(reason: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "management.models".into(),
        message: reason.into(),
    }
}
fn domain(error: Box<dyn std::error::Error>) -> ffi::Error {
    match error.downcast::<ffi::Error>() {
        Ok(error) => *error,
        Err(_) => unavailable("domain_operation_failed"),
    }
}
fn input<T: serde::de::DeserializeOwned>(value: &Value) -> Result<T> {
    serde_json::from_value(value.clone()).map_err(|_| invalid("malformed_operation_input"))
}
fn bounded(value: &str, cap: usize) -> Result<()> {
    if value.is_empty() || value.len() > cap || value.chars().any(char::is_control) {
        Err(invalid("invalid_text_extent"))
    } else {
        Ok(())
    }
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Model {
    model: String,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct List {
    query: Option<String>,
    limit: Option<u32>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Search {
    query: Option<String>,
    author: Option<String>,
    filter: Option<String>,
    page: Option<u32>,
    limit: Option<u32>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Inspect {
    repository: String,
    revision: Option<String>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Package {
    model: String,
    package: String,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Evict {
    model: String,
    representation: String,
    kind: String,
    #[serde(default)]
    dry_run: bool,
    #[serde(default)]
    confirm: bool,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Storage {
    model: String,
    #[serde(default)]
    include_caches: bool,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Source {
    source: String,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Cancel {
    source: String,
    expected_operation_id: String,
    expected_generation: u64,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Resume {
    source: String,
    expected_operation_id: String,
    expected_generation: u64,
    authentication: Option<String>,
    credential_ref: Option<String>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Start {
    repository: String,
    revision: String,
    representation: String,
    name: Option<String>,
    authentication: Option<String>,
    credential_ref: Option<String>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Build {
    model: String,
    expected_plan: Option<String>,
    quant: Option<String>,
    #[serde(default)]
    dry_run: bool,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Profile {
    profile: String,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct CreateProfile {
    model: String,
    package: String,
    alias: String,
    base_profile: Option<String>,
    context: Option<u64>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct RemoveProfile {
    profile: String,
    expected_package: String,
    confirm: bool,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Cleanup {
    source: String,
    expected_operation_id: String,
    expected_generation: u64,
    #[serde(default)]
    stale_locks: bool,
    #[serde(default)]
    failed_partials: bool,
    #[serde(default)]
    receipts: bool,
    #[serde(default)]
    logs: bool,
    #[serde(default)]
    provider_cache: bool,
    #[serde(default)]
    dry_run: bool,
    #[serde(default)]
    confirm: bool,
}

fn selected(selector: &str) -> Result<(ffi::Library, u64, ffi::ModelSnapshot)> {
    bounded(selector, 512)?;
    let library = ffi::Library::open(None, None)?;
    let mut selected = None;
    for index in 0..library.count() {
        if library.matches(index, selector)? {
            if selected.is_some() {
                return Err(invalid("ambiguous_model_identity"));
            }
            selected = Some(index);
        }
    }
    let index = selected.ok_or_else(|| invalid("model_not_found"))?;
    let model = library.snapshot(index)?;
    Ok((library, index, model))
}
fn models(value: &Value, single: bool) -> Result<Value> {
    let engines = client::engines(None);
    let posture = if engines.is_ok() {
        "current"
    } else {
        "unavailable"
    };
    let engines = engines.unwrap_or_default();
    if single {
        let request: Model = input(value)?;
        let (_, _, model) = selected(&request.model)?;
        return Ok(
            json!({"schema":"yvex.model.v4", "model": catalog::facts(&model, &engines).contract,
            "runtime_observation":posture}),
        );
    }
    let request: List = input(value)?;
    let limit = request.limit.unwrap_or(100);
    if !(1..=256).contains(&limit) {
        return Err(invalid("invalid_page_limit"));
    }
    if let Some(query) = &request.query {
        bounded(query, 256)?;
    }
    let library = ffi::Library::open(None, None)?;
    let mut rows = Vec::new();
    let mut truncated = false;
    for index in 0..library.count() {
        let model = library.snapshot(index)?;
        if !catalog::query_matches(&model.entry, request.query.as_deref().unwrap_or("")) {
            continue;
        }
        if rows.len() == limit as usize {
            truncated = true;
            break;
        }
        rows.push(catalog::facts(&model, &engines).contract);
    }
    Ok(json!({"schema":"yvex.model.list.v4", "models":rows,
        "runtime_observation":posture,"truncated":truncated}))
}
fn remote(value: &Value, inspect: bool) -> Result<Value> {
    let snapshot = if inspect {
        let request: Inspect = input(value)?;
        bounded(&request.repository, 255)?;
        if let Some(revision) = &request.revision {
            bounded(revision, 127)?;
        }
        ffi::remote_catalog(
            ffi::RemoteRequest::Inspect {
                repository: &request.repository,
                revision: request.revision.as_deref(),
            },
            None,
        )?
    } else {
        let request: Search = input(value)?;
        for value in [&request.query, &request.author, &request.filter]
            .into_iter()
            .flatten()
        {
            bounded(value, 256)?;
        }
        let page = request.page.unwrap_or(1);
        let limit = request.limit.unwrap_or(20);
        if !(1..=100).contains(&page) || !(1..=100).contains(&limit) {
            return Err(invalid("invalid_search_window"));
        }
        ffi::remote_catalog(
            ffi::RemoteRequest::Search {
                query: request.query.as_deref(),
                author: request.author.as_deref(),
                filter: request.filter.as_deref(),
                page,
                limit,
            },
            None,
        )?
    };
    let models = snapshot
        .models
        .iter()
        .map(|row| catalog::remote_json(&snapshot, row))
        .collect::<Result<Vec<_>>>()?;
    Ok(
        json!({"schema":"yvex.model-catalog-projection.v1", "authorities":["remote-provider","local-catalog"],
        "query":snapshot.query,"provider_result_count":snapshot.provider_count,"models":models}),
    )
}
fn source(selector: &str) -> Result<Box<acquisition::Provenance>> {
    bounded(selector, 512)?;
    ffi::catalog::acquired_target(None, selector)?.ok_or_else(|| invalid("source_not_found"))
}
fn acquisition_view(
    provenance: &acquisition::Provenance,
    operation: &native::Operation,
) -> Result<Value> {
    Ok(json!({"acquisition":native::projection(provenance,operation)?}))
}
fn acquisition_get(value: &Value) -> Result<Value> {
    let request: Source = input(value)?;
    let provenance = source(&request.source)?;
    let root = ffi::Paths::with_models_root(None)?;
    acquisition::guarded(&provenance, &ffi::text(&root.operator.models_root)).map_err(domain)?;
    let _lease = ffi::preparation::lock(None, &ffi::text(&provenance.operation_path))?;
    let operation = acquisition::state(&provenance).map_err(domain)?;
    acquisition_view(&provenance, &operation)
}
fn package(value: &Value) -> Result<Value> {
    let request: Package = input(value)?;
    bounded(&request.package, 64)?;
    let (_, _, model) = selected(&request.model)?;
    let (artifact, local) = model
        .artifacts
        .iter()
        .find(|(row, _)| ffi::text(&row.identity) == request.package)
        .ok_or_else(|| invalid("package_not_found"))?;
    let profiles = model
        .profiles
        .iter()
        .filter(|row| ffi::text(&row.artifact_identity) == request.package)
        .map(catalog::profile_json)
        .collect::<Vec<_>>();
    Ok(json!({"model":catalog::selector(&model),
            "package":catalog::artifact_json(&model,artifact,*local),"profiles":profiles}))
}
fn storage(value: &Value) -> Result<Value> {
    let request: Storage = input(value)?;
    let (library, index, _) = selected(&request.model)?;
    let paths = ffi::Paths::with_models_root(None)?;
    Ok(catalog::storage_projection(&library.storage(
        index,
        &ffi::text(&paths.operator.models_root),
        request.include_caches,
    )?))
}
pub(crate) fn read(operation: &str, value: &Value) -> Result<Value> {
    validate(operation, value)?;
    match operation {
        "model.list" => models(value, false),
        "model.get" => models(value, true),
        "model.search" => remote(value, false),
        "model.inspect" => remote(value, true),
        "model.storage" => storage(value),
        "package.get" => package(value),
        "acquisition.get" => acquisition_get(value),
        "registry.accounts" => registry_accounts(value),
        _ => Err(invalid("unsupported_operation")),
    }
}
fn registry_accounts(value: &Value) -> Result<Value> {
    #[derive(Deserialize)]
    #[serde(deny_unknown_fields)]
    struct Empty {}
    let _: Empty = input(value)?;
    let mut providers = Vec::new();
    for name in ["huggingface", "github"] {
        let observation = ffi::Account::open(name, None, None)?.observe()?;
        if observation.raw_token_stored_by_yvex != 0 {
            return Err(unavailable("credential_redaction_invariant"));
        }
        providers.push(
            json!({"provider":name,"auth_state":ffi::text(&observation.auth_state),
            "account":ffi::text(&observation.account_hint),
            "credential_source":ffi::text(&observation.credential_source),
            "cli_present":observation.cli_present!=0,"status":ffi::text(&observation.status),
            "credential_ref":if ["logged-in","env-token-present"]
                .contains(&ffi::text(&observation.auth_state).as_str()) {
                Some(format!("registry:{name}:default")) } else { None },
            "credential_provisioning":"host_provider",
            "blocker":ffi::text(&observation.top_blocker)}),
        );
    }
    Ok(json!({"providers":providers,"credential_configuration":"host_owned"}))
}

fn acquisition_request(
    mut provenance: Box<acquisition::Provenance>,
    includes: Vec<String>,
    excludes: Vec<String>,
    authentication: &str,
    resume: bool,
) -> Result<acquire::Request> {
    let auth = match authentication {
        "anonymous" => "never",
        "configured" => "required",
        _ => return Err(invalid("unsupported_registry_authentication")),
    };
    let paths = ffi::Paths::with_models_root(None)?;
    native::validate_identity(&paths.operator, &provenance)?;
    let account = ffi::Account::open(&ffi::text(&provenance.provider), None, None)?;
    let observation = account.observe()?;
    if observation.cli_present == 0 {
        return Err(unavailable("registry_transport_unavailable"));
    }
    if auth == "required"
        && !["logged-in", "env-token-present"]
            .contains(&ffi::text(&observation.auth_state).as_str())
    {
        return Err(unavailable("registry_authentication_required_on_host"));
    }
    let root = ffi::text(&paths.operator.models_root);
    let selection = native::configure(Some(&root), &mut provenance, &includes, &excludes, !resume)?;
    Ok(acquire::Request {
        provenance,
        selection,
        root,
        observation,
        account,
        auth: auth.into(),
        workers: 8,
        timeout: 10,
        tick: 2,
        stall: 120,
        asset: None,
        resume,
        dry: false,
        no_manifest: false,
        no_inventory: false,
        outcome: Cell::default(),
        progress: "off".into(),
        yes: true,
        force_sidecars: false,
        root_source: ffi::text(&paths.operator.models_root_source),
        clear_stale_locks: false,
    })
}
fn worker_words(request: &acquire::Request) -> Vec<String> {
    // This is the existing source supervisor's private continuation, not a management
    // command dispatcher. CLI and management share start_request and durable identity.
    let source = &request.provenance;
    let mut words: Vec<String> = [
        "source",
        if request.resume { "resume" } else { "acquire" },
        "--repo",
        &ffi::text(&source.repo_id),
        "--family",
        &ffi::text(&source.family),
        "--name",
        &ffi::text(&source.target_id),
        "--revision",
        &ffi::text(&source.revision),
        "--models-root",
        &request.root,
        "--auth",
        &request.auth,
        "--progress",
        "off",
    ]
    .into_iter()
    .map(str::to_owned)
    .collect();
    for (flag, rows, count) in [
        ("--include", &source.includes, source.include_count),
        ("--exclude", &source.excludes, source.exclude_count),
    ] {
        for row in &rows[..count as usize] {
            words.extend([flag.into(), ffi::text(row)]);
        }
    }
    words
}
fn submit(request: acquire::Request, expected_generation: Option<(String, u64)>) -> Result<Value> {
    let words = worker_words(&request);
    let operation = acquire::start_request(
        &request,
        acquire::StartOptions {
            worker_words: words,
            expected_bytes: None,
            selected_shards: None,
            expected_generation,
        },
    )
    .map_err(domain)?;
    acquisition_view(&request.provenance, &operation)
}
fn acquire_start(value: &Value) -> Result<Value> {
    let request: Start = input(value)?;
    bounded(&request.repository, 255)?;
    bounded(&request.representation, 127)?;
    if request.revision.len() != 40
        || !request
            .revision
            .bytes()
            .all(|value| value.is_ascii_hexdigit())
    {
        return Err(invalid("immutable_revision_required"));
    }
    let snapshot = ffi::remote_catalog(
        ffi::RemoteRequest::Inspect {
            repository: &request.repository,
            revision: Some(&request.revision),
        },
        None,
    )?;
    let model = snapshot
        .models
        .iter()
        .find(|row| {
            ffi::text(&row.facts.repository) == request.repository
                && ffi::text(&row.facts.resolved_revision) == request.revision
        })
        .ok_or_else(|| unavailable("exact_remote_revision_unavailable"))?;
    let representation = model
        .representations
        .iter()
        .find(|row| ffi::text(&row.identity) == request.representation)
        .ok_or_else(|| invalid("representation_not_found"))?;
    if representation.provisional != 0 || representation.source_ingest_supported == 0 {
        return Err(unavailable("representation_not_admitted_for_acquisition"));
    }
    let family = ffi::text(&model.facts.family);
    if family.is_empty() || family == "unknown" {
        return Err(unavailable("source_family_not_admitted"));
    }
    let default_name = format!(
        "{}-{}-{}",
        request.repository.rsplit('/').next().unwrap_or("model"),
        &request.revision[..8],
        &ffi::digest(request.representation.as_bytes())?[..8]
    );
    let name = request.name.as_deref().unwrap_or(&default_name);
    bounded(name, 127)?;
    if !name
        .bytes()
        .all(|byte| byte.is_ascii_alphanumeric() || b"-_.".contains(&byte))
        || [".", ".."].contains(&name)
    {
        return Err(invalid("unsafe_source_name"));
    }
    let mut provenance = ffi::catalog::acquired_target(None, name)?.unwrap_or_default();
    if provenance.found != 0
        && (ffi::text(&provenance.repo_id) != request.repository
            || ffi::text(&provenance.revision) != request.revision)
    {
        return Err(invalid("source_name_already_bound"));
    }
    ffi::put_text(&mut provenance.target_id, name)?;
    ffi::put_text(&mut provenance.local_name, name)?;
    ffi::put_text(&mut provenance.family, &family)?;
    ffi::put_text(&mut provenance.provider, "huggingface")?;
    ffi::put_text(&mut provenance.repo_id, &request.repository)?;
    ffi::put_text(&mut provenance.revision, &request.revision)?;
    let mut includes = model
        .files
        .iter()
        .filter(|file| {
            ffi::text(&file.representation) == request.representation
                || matches!(
                    file.kind,
                    raw::yvex_remote_file_kind_YVEX_REMOTE_FILE_CONFIGURATION
                        | raw::yvex_remote_file_kind_YVEX_REMOTE_FILE_TOKENIZER
                        | raw::yvex_remote_file_kind_YVEX_REMOTE_FILE_SIDECAR
                )
        })
        .map(|file| ffi::text(&file.path))
        .collect::<Vec<_>>();
    // Bounded catalog file projections may omit full lists. The exact producer
    // representation pattern is then retained together with normal sidecars.
    if includes.is_empty() || includes.len() > provenance.includes.len() {
        includes = vec![
            ffi::text(&representation.file_pattern),
            "*.json".into(),
            "*.model".into(),
            "tokenizer*".into(),
        ];
    }
    let request = acquisition_request(
        provenance,
        includes,
        native::patterns(true)?,
        request
            .authentication
            .as_deref()
            .unwrap_or(if request.credential_ref.is_some() {
                "configured"
            } else {
                "anonymous"
            }),
        false,
    )?;
    submit(request, None)
}
fn resume(value: &Value) -> Result<Value> {
    let request: Resume = input(value)?;
    let provenance = source(&request.source)?;
    let operation = native::read(&provenance)?;
    if !native::terminal(&operation) {
        return Err(unavailable("acquisition_still_active"));
    }
    let includes = provenance.includes[..provenance.include_count as usize]
        .iter()
        .map(|value| ffi::text(value))
        .collect();
    let excludes = provenance.excludes[..provenance.exclude_count as usize]
        .iter()
        .map(|value| ffi::text(value))
        .collect();
    submit(
        acquisition_request(
            provenance,
            includes,
            excludes,
            request
                .authentication
                .as_deref()
                .unwrap_or(if request.credential_ref.is_some() {
                    "configured"
                } else {
                    "anonymous"
                }),
            true,
        )?,
        Some((request.expected_operation_id, request.expected_generation)),
    )
}

fn cancel(value: &Value) -> Result<Value> {
    let request: Cancel = input(value)?;
    let provenance = source(&request.source)?;
    let paths = ffi::Paths::with_models_root(None)?;
    acquisition::guarded(&provenance, &ffi::text(&paths.operator.models_root)).map_err(domain)?;
    let _lease = ffi::preparation::lock(None, &ffi::text(&provenance.operation_path))?;
    let current = native::read(&provenance)?;
    if ffi::text(&current.operation_id) != request.expected_operation_id
        || current.generation != request.expected_generation
    {
        return Err(unavailable("stale_acquisition_generation"));
    }
    let operation =
        acquisition::stop(&provenance, Duration::from_secs(10), false).map_err(domain)?;
    acquisition_view(&provenance, &operation)
}
fn verify_source(value: &Value) -> Result<Value> {
    let request: Source = input(value)?;
    let provenance = source(&request.source)?;
    let paths = ffi::Paths::with_models_root(None)?;
    let root = ffi::text(&paths.operator.models_root);
    acquisition::guarded(&provenance, &root).map_err(domain)?;
    let target = ffi::source_verification_target(
        &ffi::text(&provenance.repo_id),
        &ffi::text(&provenance.revision),
    )?;
    let (report, trust) = ffi::source_verify(
        &ffi::text(&provenance.local_source_dir),
        &root,
        &ffi::text(&provenance.manifest_path),
        Some(&target),
    )?;
    Ok(
        json!({"source":request.source,"revision":ffi::text(&report.verification.revision),
        "payload_identity":ffi::text(&report.payload.payload_identity),"payload_trust":trust,
        "shards":report.payload.shard_count,"tensors":report.payload.tensor_count,
        "logical_tensor_bytes":report.payload.logical_tensor_bytes,
        "physical_bytes_read":report.stream.physical_bytes_read,
        "published_identity_reused":report.reused_published_identity!=0}),
    )
}
fn verify_package(value: &Value) -> Result<Value> {
    let request: Package = input(value)?;
    let (_, _, model) = selected(&request.model)?;
    let (artifact, local) = model
        .artifacts
        .iter()
        .find(|(row, _)| ffi::text(&row.identity) == request.package)
        .ok_or_else(|| invalid("package_not_found"))?;
    if !local {
        return Err(unavailable("package_not_local"));
    }
    let reference = ffi::Reference::resolve(&ffi::text(&artifact.path))?;
    let (report, status) = reference.integrity(Some(&request.package), false, 0)?;
    Ok(
        json!({"model":request.model,"package":request.package,"checked":report.checked!=0,
        "passed":report.passed!=0,"native_status":status,"sha256":ffi::text(&report.sha256),
        "digest_status":ffi::text(&report.digest_status),"format":ffi::text(&report.format),
        "tensor_count":report.tensor_count,"file_size":report.file_size,
        "error_count":report.error_count,"warning_count":report.warning_count}),
    )
}
fn evict(value: &Value) -> Result<Value> {
    let request: Evict = input(value)?;
    let (library, index, model) = selected(&request.model)?;
    let source = request.kind == "source";
    let selected = if source {
        model
            .sources
            .iter()
            .enumerate()
            .find(|(_, row)| ffi::text(&row.digest) == request.representation)
            .map(|(index, _)| index)
    } else {
        model
            .artifacts
            .iter()
            .enumerate()
            .find(|(_, (row, _))| ffi::text(&row.identity) == request.representation)
            .map(|(index, _)| index)
    }
    .ok_or_else(|| invalid("representation_not_found"))?;
    let paths = ffi::Paths::with_models_root(None)?;
    let result = library.evict(
        index,
        selected as u64,
        source,
        &ffi::text(&paths.operator.models_root),
        request.dry_run,
    )?;
    Ok(
        json!({"schema":"yvex.model.evict.v1","changed":result.changed!=0,
        "local":result.local!=0,"logical_bytes":result.logical_bytes,"allocated_bytes":result.allocated_bytes,
        "path":ffi::text(&result.path)}),
    )
}

fn scan_profiles() -> Result<Value> {
    let paths = ffi::Paths::with_models_root(None)?;
    let candidates = ffi::profile_scan(&ffi::text(&paths.operator.models_root))?;
    Ok(
        json!({"truncated":candidates.len()>256,"candidates":candidates.iter().take(256).map(|row|
        json!({"alias":row.alias,"family":row.family,"model":row.model,"scope":row.scope,
            "artifact_class":row.class,"qprofile":row.qprofile,"calibration":row.calibration,"path":row.path}))
        .collect::<Vec<_>>()}),
    )
}
fn verify_profile(value: &Value) -> Result<Value> {
    let request: Profile = input(value)?;
    let report = ffi::profile_verify(&request.profile, None)?;
    Ok(
        json!({"profile":report.alias,"passed":report.passed,"identity_status":report.identity_status,
        "metadata_status":report.metadata_status,"readiness_status":report.readiness_status,
        "status":report.status,"reason":report.reason,"registered_sha256":report.registered_sha256,
        "current_sha256":ffi::text(&report.current.sha256),"registered_size":report.registered_size,
        "current_size":report.current.file_size}),
    )
}
fn create_profile(value: &Value) -> Result<Value> {
    let request: CreateProfile = input(value)?;
    let (_, _, model) = selected(&request.model)?;
    let (artifact, local) = model
        .artifacts
        .iter()
        .find(|(artifact, _)| ffi::text(&artifact.identity) == request.package)
        .ok_or_else(|| invalid("package_not_found"))?;
    if !local {
        return Err(unavailable("package_not_local"));
    }
    let base = request
        .base_profile
        .as_ref()
        .map(|alias| {
            model
                .profiles
                .iter()
                .find(|profile| {
                    ffi::text(&profile.alias) == *alias
                        && ffi::text(&profile.artifact_identity) == request.package
                })
                .ok_or_else(|| invalid("base_profile_package_mismatch"))
        })
        .transpose()?;
    let binding = base
        .map(|profile| ffi::text(&profile.runtime_binding))
        .unwrap_or_default();
    let target = base
        .map(|profile| ffi::text(&profile.runtime_target))
        .unwrap_or_default();
    let backend = base
        .map(|profile| ffi::text(&profile.backend))
        .unwrap_or_default();
    let strategy = base
        .map(|profile| ffi::text(&profile.execution_strategy))
        .unwrap_or_default();
    let installation = base
        .map(|profile| ffi::text(&profile.installation))
        .unwrap_or_default();
    let deployment = match base {
        None => ffi::Deployment::Inspection,
        Some(profile) if ffi::text(&profile.profile) == "composite" => {
            if request.context.is_some() {
                return Err(invalid("composite_context_not_applicable"));
            }
            ffi::Deployment::Composite {
                root: &installation,
                target: &target,
                backend: &backend,
            }
        }
        Some(profile) if ffi::text(&profile.profile) == "single-artifact" => {
            ffi::Deployment::Single {
                binding: &binding,
                target: &target,
                backend: &backend,
                strategy: &strategy,
                context: request.context.unwrap_or(profile.context_capacity),
            }
        }
        Some(_) => return Err(unavailable("base_profile_deployment_unavailable")),
    };
    let receipt = ffi::profile_create(
        &ffi::ProfileRequest {
            path: &ffi::text(&artifact.path),
            alias: Some(&request.alias),
            family: Some(&ffi::text(&model.entry.family)),
            model: Some(&ffi::text(&model.entry.model)),
            scope: Some(if base.is_some() {
                "runtime"
            } else {
                "inspection"
            }),
            class: Some(&ffi::text(&artifact.artifact_class)),
            qprofile: None,
            calibration: None,
            support: None,
            expected_sha256: Some(&request.package),
            deployment,
        },
        None,
    )?;
    Ok(
        json!({"profile":ffi::text(&receipt.alias),"package":ffi::text(&receipt.sha256),
        "file_size":receipt.file_size,"created":true}),
    )
}
fn remove_profile(value: &Value) -> Result<Value> {
    let request: RemoveProfile = input(value)?;
    ffi::profile_remove_exact(&request.profile, Some(&request.expected_package), None)?;
    Ok(json!({"profile":request.profile,"removed":true}))
}
fn cleanup_source(value: &Value) -> Result<Value> {
    let request: Cleanup = input(value)?;
    let provenance = source(&request.source)?;
    let paths = ffi::Paths::with_models_root(None)?;
    let root = ffi::text(&paths.operator.models_root);
    acquisition::cleanup_source(
        &provenance,
        &root,
        acquisition::CleanupOptions {
            stale_locks: request.stale_locks,
            provider_cache: request.provider_cache,
            failed_partials: request.failed_partials,
            receipts: request.receipts,
            logs: request.logs,
            dry: request.dry_run,
            confirm: request.confirm,
            expected_generation: Some((request.expected_operation_id, request.expected_generation)),
        },
    )
    .map(|report| report.value)
    .map_err(domain)
}

pub(crate) fn execute(operation: &str, value: &Value) -> Result<Value> {
    validate(operation, value)?;
    match operation {
        "profile.scan" => scan_profiles(),
        "profile.create" => create_profile(value),
        "profile.verify" => verify_profile(value),
        "profile.remove" => remove_profile(value),
        "source.cleanup" => cleanup_source(value),
        "source.verify" => verify_source(value),
        "package.verify" => verify_package(value),
        "model.evict" => evict(value),
        "acquisition.start" => acquire_start(value),
        "acquisition.resume" => resume(value),
        "acquisition.cancel" => cancel(value),
        "build.start" => {
            let request: Build = input(value)?;
            bounded(&request.model, 512)?;
            if let Some(quant) = &request.quant {
                bounded(quant, 128)?;
            }
            preparation::prepare_model_expected(
                &request.model,
                ffi::preparation::Request {
                    root: None,
                    registry: None,
                    quant: request.quant.as_deref(),
                    imatrix: None,
                    dry: request.dry_run,
                },
                request.expected_plan.as_deref(),
            )
            .map_err(domain)
        }
        _ => Err(invalid("unsupported_operation")),
    }
}

fn validate_profile(operation: &str, value: &Value) -> Result<()> {
    let alias = |value: &str| {
        bounded(value, 127)?;
        if !value
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || b"-_ .:".contains(&byte))
        {
            return Err(invalid("invalid_profile_identity"));
        }
        Ok(())
    };
    let digest = |value: &str| {
        if value.len() != 64 || !value.bytes().all(|byte| byte.is_ascii_hexdigit()) {
            return Err(invalid("exact_package_digest_required"));
        }
        Ok(())
    };
    match operation {
        "profile.scan" => {
            if value.as_object().is_none_or(|value| !value.is_empty()) {
                return Err(invalid("malformed_operation_input"));
            }
        }
        "profile.verify" => {
            let request: Profile = input(value)?;
            alias(&request.profile)?;
        }
        "profile.create" => {
            let request: CreateProfile = input(value)?;
            bounded(&request.model, 512)?;
            alias(&request.alias)?;
            digest(&request.package)?;
            if let Some(base) = &request.base_profile {
                alias(base)?;
            }
            if request.context.is_some_and(|value| value == 0)
                || (request.context.is_some() && request.base_profile.is_none())
            {
                return Err(invalid("context_requires_deployment_profile"));
            }
        }
        "profile.remove" => {
            let request: RemoveProfile = input(value)?;
            alias(&request.profile)?;
            digest(&request.expected_package)?;
            if !request.confirm {
                return Err(invalid("explicit_profile_removal_confirmation_required"));
            }
        }
        _ => return Err(invalid("unsupported_operation")),
    }
    Ok(())
}

// Called before receipt/journal admission. No catalog, network or filesystem effect.
pub(crate) fn validate(operation: &str, value: &Value) -> Result<()> {
    if operation.starts_with("profile.") {
        return validate_profile(operation, value);
    }
    let text = |value: &str, cap| {
        bounded(value, cap)?;
        if value.starts_with('/')
            || value.starts_with('~')
            || value.split('/').any(|part| part == "..")
        {
            return Err(invalid("host_path_not_admitted"));
        }
        Ok(())
    };
    let generation = |identity: &str, generation: u64| {
        if identity.len() != 64
            || !identity.bytes().all(|byte| byte.is_ascii_hexdigit())
            || generation == 0
        {
            return Err(invalid("invalid_acquisition_identity"));
        }
        Ok(())
    };
    let auth = |value: Option<&str>| {
        if value.is_some_and(|value| !["anonymous", "configured"].contains(&value)) {
            return Err(invalid("unsupported_registry_authentication"));
        }
        Ok(())
    };
    let credential = |reference: Option<&str>, authentication: Option<&str>| {
        auth(authentication)?;
        if reference.is_some_and(|reference| reference != "registry:huggingface:default") {
            return Err(invalid("unknown_registry_credential_reference"));
        }
        if reference.is_some() && authentication == Some("anonymous") {
            return Err(invalid("conflicting_registry_authentication"));
        }
        Ok(())
    };
    match operation {
        "model.list" => {
            let request: List = input(value)?;
            if let Some(query) = request.query {
                text(&query, 256)?;
            }
            if !(1..=256).contains(&request.limit.unwrap_or(100)) {
                return Err(invalid("invalid_page_limit"));
            }
        }
        "model.get" => {
            let request: Model = input(value)?;
            text(&request.model, 512)?;
        }
        "model.search" => {
            let request: Search = input(value)?;
            for value in [&request.query, &request.author, &request.filter]
                .into_iter()
                .flatten()
            {
                text(value, 256)?;
            }
            if !(1..=100).contains(&request.page.unwrap_or(1))
                || !(1..=100).contains(&request.limit.unwrap_or(20))
            {
                return Err(invalid("invalid_search_window"));
            }
        }
        "model.inspect" => {
            let request: Inspect = input(value)?;
            text(&request.repository, 255)?;
            if let Some(revision) = request.revision {
                text(&revision, 127)?;
            }
        }
        "model.storage" => {
            let request: Storage = input(value)?;
            text(&request.model, 512)?;
        }
        "package.get" | "package.verify" => {
            let request: Package = input(value)?;
            text(&request.model, 512)?;
            text(&request.package, 64)?;
        }
        "acquisition.get" | "source.verify" => {
            let request: Source = input(value)?;
            text(&request.source, 512)?;
        }
        "source.cleanup" => {
            let request: Cleanup = input(value)?;
            text(&request.source, 512)?;
            generation(&request.expected_operation_id, request.expected_generation)?;
            if !request.dry_run && !request.confirm {
                return Err(invalid("explicit_cleanup_confirmation_required"));
            }
            if !(request.stale_locks
                || request.failed_partials
                || request.receipts
                || request.logs
                || request.provider_cache)
            {
                return Err(invalid("cleanup_selection_required"));
            }
        }
        "acquisition.cancel" => {
            let request: Cancel = input(value)?;
            text(&request.source, 512)?;
            generation(&request.expected_operation_id, request.expected_generation)?;
        }
        "acquisition.resume" => {
            let request: Resume = input(value)?;
            text(&request.source, 512)?;
            generation(&request.expected_operation_id, request.expected_generation)?;
            credential(
                request.credential_ref.as_deref(),
                request.authentication.as_deref(),
            )?;
        }
        "acquisition.start" => {
            let request: Start = input(value)?;
            text(&request.repository, 255)?;
            text(&request.representation, 127)?;
            if request.revision.len() != 40
                || !request
                    .revision
                    .bytes()
                    .all(|byte| byte.is_ascii_hexdigit())
            {
                return Err(invalid("immutable_revision_required"));
            }
            if let Some(name) = request.name {
                text(&name, 127)?;
                if !name
                    .bytes()
                    .all(|byte| byte.is_ascii_alphanumeric() || b"-_.".contains(&byte))
                {
                    return Err(invalid("unsafe_source_name"));
                }
            }
            credential(
                request.credential_ref.as_deref(),
                request.authentication.as_deref(),
            )?;
        }
        "build.start" => {
            let request: Build = input(value)?;
            text(&request.model, 512)?;
            if let Some(plan) = request.expected_plan
                && (plan.len() != 64 || !plan.bytes().all(|byte| byte.is_ascii_hexdigit()))
            {
                return Err(invalid("invalid_build_plan_identity"));
            }
            if let Some(quant) = request.quant {
                text(&quant, 128)?;
            }
        }
        "model.evict" => {
            let request: Evict = input(value)?;
            text(&request.model, 512)?;
            if request.representation.len() != 64
                || !request
                    .representation
                    .bytes()
                    .all(|byte| byte.is_ascii_hexdigit())
            {
                return Err(invalid("exact_representation_digest_required"));
            }
            if !["source", "package"].contains(&request.kind.as_str()) {
                return Err(invalid("invalid_representation_kind"));
            }
            if !request.dry_run && !request.confirm {
                return Err(invalid("explicit_eviction_confirmation_required"));
            }
        }
        "registry.accounts" => {
            if value.as_object().is_none_or(|value| !value.is_empty()) {
                return Err(invalid("malformed_operation_input"));
            }
        }
        _ => return Err(invalid("unsupported_operation")),
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn remote_input_never_accepts_secret_or_host_path() {
        for request in [
            json!({"repository":"org/model","revision":"a".repeat(40),"representation":"bf16","token":"secret"}),
            json!({"repository":"org/model","revision":"a".repeat(40),
                "representation":"bf16","models_root":"/tmp/other"}),
        ] {
            assert_eq!(
                execute("acquisition.start", &request).unwrap_err().message,
                "malformed_operation_input"
            );
        }
        assert_eq!(
            execute("build.start", &json!({"model":"x","out":"/tmp/payload"}))
                .unwrap_err()
                .message,
            "malformed_operation_input"
        );
    }
    #[test]
    fn credential_references_are_host_owned_and_do_not_accept_material() {
        for (reference, authentication, expected) in [
            (
                "registry:huggingface:other",
                "configured",
                "unknown_registry_credential_reference",
            ),
            (
                "registry:huggingface:default",
                "anonymous",
                "conflicting_registry_authentication",
            ),
        ] {
            let value = json!({"repository":"org/model", "revision":"a".repeat(40),
                "representation":"bf16", "credential_ref":reference, "authentication":authentication});
            assert_eq!(
                validate("acquisition.start", &value).unwrap_err().message,
                expected
            );
        }
        let value = json!({"repository":"org/model", "revision":"a".repeat(40),
            "representation":"bf16", "credential_ref":"registry:huggingface:default"});
        assert!(validate("acquisition.start", &value).is_ok());
    }
    #[test]
    fn acquisition_requires_exact_revision_before_remote_read() {
        assert_eq!(
            execute(
                "acquisition.start",
                &json!({"repository":"org/model","revision":"main","representation":"bf16"})
            )
            .unwrap_err()
            .message,
            "immutable_revision_required"
        );
    }
    #[test]
    fn bounded_catalog_and_unsupported_operations_refuse() {
        assert_eq!(
            read("model.list", &json!({"limit":0})).unwrap_err().message,
            "invalid_page_limit"
        );
        assert_eq!(
            read("model.search", &json!({"page":0}))
                .unwrap_err()
                .message,
            "invalid_search_window"
        );
        assert_eq!(
            read("training.start", &json!({})).unwrap_err().message,
            "unsupported_operation"
        );
    }
}
