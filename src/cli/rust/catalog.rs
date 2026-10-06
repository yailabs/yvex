// Catalog projection joins copied source/package/profile facts with exact live
// engine identities. Admission remains in the C catalog and runtime owners.
use crate::{
    client,
    ffi::{self, Error, Library, ModelSnapshot, raw},
    presentation,
    registry::Invocation,
};
use serde_json::{Value, json};

fn failure(reason: &str) -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "model.selector".into(),
        message: reason.into(),
    }
}

fn unavailable_representation(reason: &str) -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "model.push".into(),
        message: reason.into(),
    }
}

pub(crate) fn selector(model: &ModelSnapshot) -> String {
    [
        &model.entry.model[..],
        &model.entry.display_name[..],
        &model.entry.identity[..],
    ]
    .into_iter()
    .map(ffi::text)
    .find(|value| !value.is_empty())
    .unwrap_or_default()
}

fn precision(value: &str) -> String {
    match value.to_ascii_lowercase().as_str() {
        "iq2xxs-q2k-mxfp4" => "IQ2_XXS/Q2_K/MXFP4",
        "iq2xxs" | "iq2_xxs" => "IQ2_XXS",
        "q2k" | "q2_k" => "Q2_K",
        "mxfp4" => "MXFP4",
        "bf16" => "BF16",
        "f16" | "fp16" => "FP16",
        "f32" | "fp32" => "FP32",
        "" => "not recorded",
        _ => value,
    }
    .into()
}

fn same_choice(
    left: &raw::yvex_model_runtime_profile_fact,
    right: &raw::yvex_model_runtime_profile_fact,
) -> bool {
    if ffi::text(&left.artifact_identity) != ffi::text(&right.artifact_identity)
        || ffi::text(&left.backend) != ffi::text(&right.backend)
        || ffi::text(&left.engine_kind) != ffi::text(&right.engine_kind)
        || ffi::text(&left.execution_strategy) != ffi::text(&right.execution_strategy)
    {
        return false;
    }
    !ffi::text(&left.artifact_identity).is_empty()
        || (ffi::text(&left.profile) == ffi::text(&right.profile)
            && ffi::text(&left.installation) == ffi::text(&right.installation)
            && ffi::text(&left.runtime_target) == ffi::text(&right.runtime_target))
}

// Registry order is the retained deployment revision order, not numerical support.
fn choices(model: &ModelSnapshot) -> Vec<usize> {
    let mut choices: Vec<usize> = Vec::new();
    for (index, profile) in model
        .profiles
        .iter()
        .enumerate()
        .filter(|(_, profile)| profile.launchable != 0)
    {
        if let Some(previous) = choices
            .iter_mut()
            .find(|previous| same_choice(&model.profiles[**previous], profile))
        {
            *previous = index;
        } else {
            choices.push(index);
        }
    }
    choices
}

fn model_state(model: &ModelSnapshot) -> &'static str {
    let entry = &model.entry;
    if entry.profile_launchable != 0 {
        return "READY";
    }
    if entry.profile_count != 0 {
        return "BLOCKED";
    }
    if entry.artifact_count != 0 {
        return "PREPARING";
    }
    let (mut verified, mut external, mut acquired, mut remote, mut blocked) =
        (false, false, false, false, false);
    for source in &model.sources {
        let state = ffi::text(&source.acquisition_state);
        let storage = ffi::text(&source.storage_kind);
        if state == "source-remote" || (state == "source-missing" && storage != "external") {
            remote = true;
        } else if state == "source-partial" || (state == "source-missing" && storage == "external")
        {
            blocked = true;
        } else if storage == "external" {
            external = true;
        } else if storage == "remote" {
            remote = true;
        } else if ffi::text(&source.verification_state) == "payload-verified" {
            verified = true;
        } else if !ffi::text(&source.path).is_empty() {
            acquired = true;
        }
    }
    if verified || acquired || external {
        if matches!(ffi::text(&entry.family).as_str(), "" | "unknown") {
            return "UNSUPPORTED";
        }
        if entry.identity_kind
            == raw::yvex_model_identity_kind_YVEX_MODEL_IDENTITY_PROVIDER_REPOSITORY_REVISION
        {
            return "UNBOUND";
        }
    }
    if verified {
        "VERIFIED"
    } else if external {
        "EXTERNAL"
    } else if acquired {
        "SOURCE"
    } else if remote || entry.remote_available != 0 {
        "REMOTE"
    } else if blocked {
        "BLOCKED"
    } else {
        "UNBOUND"
    }
}

fn source_location(source: &raw::yvex_local_source_record) -> String {
    let origin = ffi::text(&source.origin_uri);
    let path = ffi::text(&source.path);
    if ffi::text(&source.acquisition_state) == "source-missing" && !origin.is_empty() {
        return origin;
    }
    if !path.is_empty() {
        return path;
    }
    if !origin.is_empty() {
        return origin;
    }
    let repository = ffi::text(&source.repository);
    let revision = ffi::text(&source.revision);
    if repository.is_empty() {
        String::new()
    } else {
        format!(
            "hf://{repository}{}",
            if revision.is_empty() {
                revision
            } else {
                format!("@{revision}")
            }
        )
    }
}

fn source_json(source: &raw::yvex_local_source_record) -> Value {
    json!({ "provider": ffi::text(&source.provider), "repository": ffi::text(&source.repository),
        "revision": ffi::text(&source.revision), "origin_uri": ffi::text(&source.origin_uri),
        "storage": ffi::text(&source.storage_kind), "path": ffi::text(&source.path),
        "format": ffi::text(&source.format), "precision": ffi::text(&source.precision),
        "verification": ffi::text(&source.verification_state), "state": ffi::text(&source.acquisition_state),
        "digest": ffi::text(&source.digest), "size_bytes": source.size_bytes, "size_known": source.size_known != 0 })
}

pub(crate) fn profile_json(profile: &raw::yvex_model_runtime_profile_fact) -> Value {
    json!({ "identity": ffi::text(&profile.alias), "artifact_identity": ffi::text(&profile.artifact_identity),
        "backend": ffi::text(&profile.backend), "engine_kind": ffi::text(&profile.engine_kind),
        "strategy": ffi::text(&profile.execution_strategy), "context": profile.context_capacity,
        "launchable": profile.launchable != 0, "blocker": ffi::text(&profile.blocker),
        "readiness": ffi::text(&profile.readiness), "compatibility": ffi::text(&profile.compatibility),
        "capabilities": { "input_mask": profile.capabilities.input_kinds,
            "output_mask": profile.capabilities.output_kinds, "properties": profile.capabilities.execution_properties,
            "maximum_input_parts": profile.capabilities.maximum_input_parts } })
}

fn publication_json(publication: &raw::yvex_model_publication) -> Value {
    json!({ "provider": ffi::text(&publication.provider), "repository": ffi::text(&publication.repository),
        "revision": ffi::text(&publication.revision), "filename": ffi::text(&publication.filename),
        "sha256": ffi::text(&publication.remote_sha256), "manifest_filename": ffi::text(&publication.manifest_filename),
        "manifest_sha256": ffi::text(&publication.manifest_sha256), "size_bytes": publication.size_bytes,
        "state": "PUBLISHED" })
}

pub(crate) fn artifact_json(
    model: &ModelSnapshot,
    artifact: &raw::yvex_model_artifact_fact,
    local: bool,
) -> Value {
    let identity = ffi::text(&artifact.identity);
    json!({ "identity": identity, "path": ffi::text(&artifact.path), "format": ffi::text(&artifact.format),
        "quant_precision": if artifact.physical_variant[0] != 0 { ffi::text(&artifact.physical_variant) } else {
            ffi::text(&artifact.artifact_class) },
        "size_bytes": artifact.file_size, "tensor_count": artifact.tensor_count, "local_available": local,
        "remote_locations": model.publications.iter().filter(|publication| ffi::text(&publication.artifact_identity)
            == identity).map(publication_json).collect::<Vec<_>>() })
}

fn components(
    profile: Option<&raw::yvex_model_runtime_profile_fact>,
    inspect_precision: bool,
) -> Vec<Value> {
    let Some(profile) = profile.filter(|profile| {
        ffi::text(&profile.profile) == "composite"
            && profile.installation[0] != 0
            && profile.runtime_target[0] != 0
    }) else {
        return Vec::new();
    };
    let Ok(paths) = ffi::media_component_paths(&ffi::text(&profile.runtime_target)) else {
        return Vec::new();
    };
    let root = std::path::PathBuf::from(ffi::text(&profile.installation));
    ["text encoder", "transformer", "video VAE", "audio VAE"]
        .into_iter()
        .zip(paths)
        .map(|(role, suffix)| {
            let path = root.join(&suffix);
            let metadata = if suffix.is_empty() {
                None
            } else {
                path.metadata().ok().filter(|metadata| metadata.is_file())
            };
            let present = metadata.is_some();
            let path = path.to_string_lossy();
            let quant = if !present {
                "--".into()
            } else if inspect_precision {
                ffi::Gguf::open(&path)
                    .and_then(|gguf| gguf.precision())
                    .unwrap_or_else(|_| "metadata unavailable".into())
            } else {
                "metadata-defined".into()
            };
            json!({ "role": role, "path": path, "format": "GGUF", "quant_precision": quant,
                "state": if present { "PRESENT" } else { "MISSING" },
                "size_bytes": metadata.map_or(0, |metadata| metadata.len()), "present": present })
        })
        .collect()
}

pub(crate) struct ModelView {
    pub contract: Value,
    size: Option<u64>,
}

pub(crate) fn size(value: Option<u64>) -> String {
    let Some(value) = value else {
        return "not recorded".into();
    };
    let mut scaled = value as f64;
    let mut unit = 0;
    let units = ["B", "KiB", "MiB", "GiB", "TiB"];
    while scaled >= 1024.0 && unit + 1 < units.len() {
        scaled /= 1024.0;
        unit += 1;
    }
    if unit == 0 {
        format!("{value} B")
    } else {
        format!("{scaled:.2} {}", units[unit])
    }
}

pub(crate) struct RuntimeChoice {
    pub alias: String,
    pub contract: Value,
}

fn runtime_choice(
    model: &ModelSnapshot,
    profile: &raw::yvex_model_runtime_profile_fact,
) -> RuntimeChoice {
    let artifact = model
        .artifacts
        .iter()
        .map(|(artifact, _)| artifact)
        .find(|artifact| ffi::text(&artifact.identity) == ffi::text(&profile.artifact_identity));
    let kind = artifact
        .and_then(|artifact| {
            [
                ffi::text(&artifact.physical_variant),
                ffi::text(&artifact.artifact_class),
            ]
            .into_iter()
            .find(|value| !value.is_empty())
        })
        .or_else(|| {
            [
                ffi::text(&profile.artifact_class),
                ffi::text(&profile.profile),
            ]
            .into_iter()
            .find(|value| !value.is_empty())
        })
        .unwrap_or_else(|| "default".into());
    let identity = artifact
        .map(|artifact| ffi::text(&artifact.identity))
        .unwrap_or_default();
    let suffix = if identity.is_empty() {
        ffi::text(&profile.runtime_binding)
            .rsplit('/')
            .next()
            .unwrap_or_default()
            .to_string()
    } else {
        identity.clone()
    };
    let variant = if suffix.is_empty() {
        kind.clone()
    } else {
        format!(
            "{}@{}",
            kind.chars().take(96).collect::<String>(),
            suffix.chars().take(8).collect::<String>()
        )
    };
    let composite = ffi::text(&profile.profile) == "composite";
    RuntimeChoice {
        alias: ffi::text(&profile.alias),
        contract: json!({ "model": selector(model), "name": ffi::text(&model.entry.display_name),
            "variant": variant, "format": if composite { "composite".into() } else {
                artifact.map(|artifact| ffi::text(&artifact.format)).unwrap_or_else(|| "package".into()) },
            "quant_precision": if composite { "component-defined".into() } else { precision(&kind) },
            "backend": ffi::text(&profile.backend), "profile_identity": ffi::text(&profile.alias),
            "artifact_identity": identity }),
    }
}

fn choice_matches(
    model: &ModelSnapshot,
    profile: &raw::yvex_model_runtime_profile_fact,
    choice: &RuntimeChoice,
    variant: &str,
) -> bool {
    choice.contract["variant"] == variant
        || ffi::text(&profile.artifact_identity) == variant
        || ffi::text(&profile.artifact_class) == variant
        || model.artifacts.iter().any(|(artifact, _)| {
            ffi::text(&artifact.identity) == ffi::text(&profile.artifact_identity)
                && ffi::text(&artifact.physical_variant) == variant
        })
}

pub(crate) fn interactive_choice(
    labels: &[String],
    title: &str,
    width: usize,
    styled: bool,
) -> Result<usize, Error> {
    use std::io::{self, IsTerminal, Write};
    if !io::stdin().is_terminal() || !io::stdout().is_terminal() {
        return Err(failure(
            "explicit model/variant required when input is not a terminal",
        ));
    }
    let mut lines = vec![title.into()];
    lines.extend(
        labels
            .iter()
            .enumerate()
            .map(|(index, label)| format!("{}  {label}", index + 1)),
    );
    let mut stdout = io::stdout().lock();
    stdout
        .write_all(
            presentation::lines(&lines, width, styled)
                .map_err(|error| failure(&error.to_string()))?
                .as_bytes(),
        )
        .map_err(|error| failure(&error.to_string()))?;
    loop {
        stdout
            .write_all(b"Choice [number, q to cancel] > ")
            .and_then(|()| stdout.flush())
            .map_err(|error| failure(&error.to_string()))?;
        let mut input = String::new();
        if io::stdin()
            .read_line(&mut input)
            .map_err(|error| failure(&error.to_string()))?
            == 0
            || matches!(input.trim(), "" | "q" | "Q")
        {
            return Err(failure("selection cancelled; no state changed"));
        }
        if let Some(index) = input
            .trim()
            .parse::<usize>()
            .ok()
            .filter(|value| *value > 0 && *value <= labels.len())
        {
            return Ok(index - 1);
        }
    }
}

pub(crate) fn select_runtime(
    invocation: &Invocation<'_>,
    requested: Option<&str>,
    loaded: Option<&[raw::yvex_server_engine_summary]>,
    width: usize,
    styled: bool,
) -> Result<RuntimeChoice, Error> {
    let library = Library::open(
        invocation.value("--models-root"),
        invocation.value("--registry"),
    )?;
    let mut candidates = Vec::new();
    for index in 0..library.count() {
        if let Some(name) = requested
            && !library.matches(index, name)?
        {
            continue;
        }
        let model = library.snapshot(index)?;
        let profiles: Vec<_> = choices(&model)
            .into_iter()
            .filter(|index| {
                loaded.is_none_or(|engines| {
                    engines.iter().any(|engine| {
                        ffi::text(&engine.alias) == ffi::text(&model.profiles[*index].alias)
                            && engine.state
                                == raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADED
                            && engine.execution_ready != 0
                    })
                })
            })
            .collect();
        if requested.is_some() || !profiles.is_empty() {
            candidates.push((model, profiles));
        }
    }
    if candidates.is_empty() {
        return Err(Error {
            code: if requested.is_some() {
                raw::yvex_status_YVEX_ERR_INVALID_ARG
            } else {
                raw::yvex_status_YVEX_ERR_STATE
            },
            owner: "model.selection".into(),
            message: if let Some(name) = requested {
                format!("model not found: {name}")
            } else {
                "no launchable models are known locally".into()
            },
        });
    }
    let (model, profiles) = if requested.is_some() {
        if candidates.len() != 1 {
            return Err(failure("ambiguous model; use an exact identity"));
        }
        candidates.pop().unwrap()
    } else {
        let labels: Vec<_> = candidates
            .iter()
            .map(|(model, _)| selector(model))
            .collect();
        let index = interactive_choice(&labels, "Select model", width, styled)?;
        candidates.swap_remove(index)
    };
    if profiles.is_empty() && loaded.is_none() {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "model.selection".into(),
            message: format!("model is not launchable: {}", selector(&model)),
        });
    }
    let mut available: Vec<_> = profiles
        .into_iter()
        .map(|index| (index, runtime_choice(&model, &model.profiles[index])))
        .filter(|(index, choice)| {
            invocation.value("--variant").is_none_or(|variant| {
                choice_matches(&model, &model.profiles[*index], choice, variant)
            })
        })
        .collect();
    if available.is_empty() {
        // Explicit unload retains server-owned idempotence after retirement.
        // Resolve the still-admitted deployment, never invent a generation or
        // relax the loaded-only population for an interactive selection.
        if invocation.operation.operation_id == "model.unload"
            && requested.is_some()
            && loaded.is_some()
        {
            return select_runtime(invocation, requested, None, width, styled);
        }
        return Err(failure("model variant is not launchable or not loaded"));
    }
    let index = if available.len() == 1 {
        0
    } else if invocation.value("--variant").is_some() {
        return Err(failure("ambiguous model variant; use exact identity"));
    } else if requested.is_some() && loaded.is_none() {
        available
            .iter()
            .enumerate()
            .max_by_key(|(_, (revision, _))| *revision)
            .unwrap()
            .0
    } else {
        let labels = available
            .iter()
            .map(|(_, choice)| {
                format!(
                    "{} · {}",
                    choice.contract["variant"].as_str().unwrap(),
                    choice.contract["backend"].as_str().unwrap()
                )
            })
            .collect::<Vec<_>>();
        interactive_choice(&labels, "Select deployment", width, styled)?
    };
    Ok(available.swap_remove(index).1)
}

fn loaded_json(model: &ModelSnapshot, engine: &raw::yvex_server_engine_summary) -> Value {
    let registered = model
        .profiles
        .iter()
        .find(|profile| ffi::text(&profile.alias) == ffi::text(&engine.alias));
    let variant = registered
        .and_then(|profile| {
            model.artifacts.iter().find(|(artifact, _)| {
                ffi::text(&artifact.identity) == ffi::text(&profile.artifact_identity)
            })
        })
        .map(|(artifact, _)| {
            format!(
                "{}@{}",
                if artifact.physical_variant[0] != 0 {
                    ffi::text(&artifact.physical_variant)
                } else {
                    ffi::text(&artifact.artifact_class)
                },
                &ffi::text(&artifact.identity)[..8]
            )
        })
        .unwrap_or_else(|| {
            ffi::text(&engine.artifact_identity)
                .chars()
                .take(16)
                .collect()
        });
    json!({ "profile_identity": ffi::text(&engine.alias), "variant": variant,
        "generation": engine.generation, "sessions": engine.session_count })
}

pub(crate) fn facts(
    model: &ModelSnapshot,
    engines: &[raw::yvex_server_engine_summary],
) -> ModelView {
    let selector = selector(model);
    let loaded: Vec<_> = engines
        .iter()
        .filter(|engine| {
            engine.execution_ready != 0
                && engine.state == raw::yvex_server_engine_state_YVEX_SERVER_ENGINE_LOADED
                && (model
                    .profiles
                    .iter()
                    .any(|profile| ffi::text(&profile.alias) == ffi::text(&engine.alias))
                    || ffi::text(&engine.target_id) == selector)
        })
        .collect();
    let source = model
        .sources
        .iter()
        .find(|source| {
            source.path[0] != 0 && ffi::text(&source.acquisition_state) != "source-missing"
        })
        .or(model.sources.first());
    let choices = choices(model);
    let preferred = choices.iter().max().map(|index| &model.profiles[*index]);
    let mut artifact = loaded
        .first()
        .and_then(|engine| {
            model.artifacts.iter().find(|(artifact, _)| {
                ffi::text(&artifact.identity) == ffi::text(&engine.artifact_identity)
            })
        })
        .map(|(artifact, _)| artifact);
    let mut profile = None;
    if artifact.is_none() && loaded.is_empty() && model.entry.profile_launchable != 0 {
        profile = preferred;
        artifact = profile
            .and_then(|profile| {
                model.artifacts.iter().find(|(artifact, _)| {
                    ffi::text(&artifact.identity) == ffi::text(&profile.artifact_identity)
                })
            })
            .map(|(artifact, _)| artifact);
    }
    if artifact.is_none() && model.artifacts.len() == 1 {
        artifact = Some(&model.artifacts[0].0);
    }
    if profile.is_none() {
        let candidates: Vec<_> = model
            .profiles
            .iter()
            .filter(|profile| {
                profile.launchable != 0
                    && artifact.is_none_or(|artifact| {
                        ffi::text(&profile.artifact_identity) == ffi::text(&artifact.identity)
                    })
            })
            .collect();
        if candidates.len() == 1 {
            profile = Some(candidates[0]);
        }
    }
    if profile.is_none()
        && let Some(artifact) = artifact
    {
        profile = choices
            .iter()
            .map(|index| &model.profiles[*index])
            .find(|profile| ffi::text(&profile.artifact_identity) == ffi::text(&artifact.identity));
    }
    let provider = source
        .map(|source| ffi::text(&source.provider))
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| ffi::text(&model.entry.provider));
    let origin = match provider.as_str() {
        "huggingface" => "Hugging Face".into(),
        "" if source.is_some_and(|source| ffi::text(&source.storage_kind) == "external") => {
            "external".into()
        }
        "" => if model.entry.artifact_count != 0 {
            "local"
        } else {
            "unknown"
        }
        .into(),
        _ => provider,
    };
    let state = if loaded.is_empty() {
        model_state(model)
    } else {
        "LOADED"
    };
    let execution = if let Some(engine) = loaded.first() {
        ffi::backend_name(engine.backend)
    } else if let Some(profile) = profile {
        ffi::text(&profile.backend)
    } else if model.entry.profile_launchable != 0 {
        "select variant".into()
    } else if model.entry.identity_kind
        == raw::yvex_model_identity_kind_YVEX_MODEL_IDENTITY_PROVIDER_REPOSITORY_REVISION
    {
        "unbound source".into()
    } else {
        match state {
            "REMOTE" => "not acquired",
            "UNBOUND" => "not linked",
            "UNSUPPORTED" => "unsupported",
            _ if model.entry.profile_count != 0 => "not current",
            _ => "not prepared",
        }
        .into()
    };
    let composite = profile.is_some_and(|profile| ffi::text(&profile.profile) == "composite");
    let (format, quant, location) = if composite {
        (
            "composite".into(),
            "mixed components".into(),
            profile
                .map(|profile| ffi::text(&profile.installation))
                .unwrap(),
        )
    } else if let Some(artifact) = artifact {
        (
            if artifact.format[0] != 0 {
                ffi::text(&artifact.format)
            } else {
                "package".into()
            },
            precision(&if artifact.physical_variant[0] != 0 {
                ffi::text(&artifact.physical_variant)
            } else {
                ffi::text(&artifact.artifact_class)
            }),
            ffi::text(&artifact.path),
        )
    } else if model.artifacts.len() > 1 {
        (
            "multiple".into(),
            "select variant".into(),
            source
                .map(source_location)
                .unwrap_or_else(|| format!("{} local representations", model.artifacts.len())),
        )
    } else if model.sources.len() > 1 {
        (
            "multiple".into(),
            "select variant".into(),
            format!("{} local source representations", model.sources.len()),
        )
    } else if let Some(source) = source {
        (
            if source.format[0] != 0 {
                ffi::text(&source.format)
            } else {
                "source".into()
            },
            precision(&ffi::text(&source.precision)),
            source_location(source),
        )
    } else {
        ("--".into(), "--".into(), String::new())
    };
    let components = components(profile, true);
    let size = if composite {
        if components.len() == 4
            && components
                .iter()
                .all(|component| component["present"] == true)
        {
            components.iter().try_fold(0u64, |sum, component| {
                sum.checked_add(component["size_bytes"].as_u64()?)
            })
        } else {
            None
        }
    } else if let Some(artifact) = artifact {
        Some(artifact.file_size)
    } else if model.artifacts.len() > 1 || model.sources.len() > 1 {
        None
    } else {
        source
            .filter(|source| source.size_known != 0)
            .map(|source| source.size_bytes)
    };
    let contract = json!({ "selector": selector, "identity": ffi::text(&model.entry.identity),
        "working_set": model.working_set,
        "name": ffi::text(&model.entry.display_name), "family": ffi::text(&model.entry.family), "origin": origin,
        "format": format, "quant_precision": quant, "state": state, "execution": execution, "location": location,
        "selected_profile": profile.map(|profile| ffi::text(&profile.alias)), "recommendation": Value::Null,
        "representation_count": if model.entry.artifact_count != 0 { model.entry.artifact_count } else {
            model.entry.source_count }, "sources": model.sources.iter().map(source_json).collect::<Vec<_>>(),
        "representations": model.artifacts.iter().map(|(artifact, local)| artifact_json(model, artifact, *local))
            .collect::<Vec<_>>(), "profiles": model.profiles.iter().map(profile_json).collect::<Vec<_>>(),
        "components": components,
        "loaded_engines": loaded.iter().map(|engine| loaded_json(model, engine)).collect::<Vec<_>>() });
    ModelView { contract, size }
}

fn detail_record(
    title: &str,
    value: &Value,
    fields: &[(&str, &str)],
    width: usize,
    styled: bool,
) -> Result<String, Error> {
    let values: Vec<_> = fields
        .iter()
        .filter_map(|(label, key)| {
            let value = &value[*key];
            let text = match value {
                Value::String(text) if matches!(*key, "precision" | "quant_precision") => {
                    precision(text)
                }
                Value::String(text) => text.clone(),
                Value::Number(_) | Value::Bool(_) => value.to_string(),
                _ => return None,
            };
            (!text.is_empty()).then_some((*label, text))
        })
        .collect();
    let borrowed: Vec<_> = values
        .iter()
        .map(|(key, value)| (*key, value.as_str()))
        .collect();
    presentation::record(title, &borrowed, width, styled)
        .map_err(|error| failure(&error.to_string()))
}

// Detail is a deeper projection of the same copied native facts as JSON. It
// must not turn an acquired source or emitted representation into a runnable
// deployment, nor discard historical blocked profiles to simplify the display.
fn model_detail(facts: &Value, width: usize, styled: bool) -> Result<String, Error> {
    let mut output = detail_record(
        "MODEL",
        facts,
        &[
            ("identity", "identity"),
            ("family", "family"),
            ("origin", "origin"),
            ("location", "location"),
            ("precision", "quant_precision"),
        ],
        width,
        styled,
    )?;
    for source in facts["sources"].as_array().into_iter().flatten() {
        output.push_str(&detail_record(
            "SOURCE",
            source,
            &[
                ("storage", "storage"),
                ("format", "format"),
                ("precision", "precision"),
                ("state", "state"),
                ("verification", "verification"),
                ("provider", "provider"),
                ("repository", "repository"),
                ("revision", "revision"),
                ("path", "path"),
                ("origin", "origin_uri"),
                ("digest", "digest"),
            ],
            width,
            styled,
        )?);
    }
    for artifact in facts["representations"].as_array().into_iter().flatten() {
        output.push_str(&detail_record(
            "REPRESENTATION",
            artifact,
            &[
                ("identity", "identity"),
                ("format", "format"),
                ("precision", "quant_precision"),
                ("path", "path"),
                ("bytes", "size_bytes"),
                ("tensors", "tensor_count"),
                ("local", "local_available"),
            ],
            width,
            styled,
        )?);
        for publication in artifact["remote_locations"]
            .as_array()
            .into_iter()
            .flatten()
        {
            output.push_str(&detail_record(
                "PUBLICATION",
                publication,
                &[
                    ("provider", "provider"),
                    ("repository", "repository"),
                    ("revision", "revision"),
                    ("filename", "filename"),
                    ("sha256", "sha256"),
                    ("state", "state"),
                ],
                width,
                styled,
            )?);
        }
    }
    let profiles = facts["profiles"]
        .as_array()
        .map(Vec::as_slice)
        .unwrap_or(&[]);
    if profiles.is_empty() {
        output.push_str(
            &presentation::record(
                "RUNTIME",
                &[("readiness", "not launchable · no admitted runtime profile")],
                width,
                styled,
            )
            .map_err(|error| failure(&error.to_string()))?,
        );
    }
    for profile in profiles {
        output.push_str(&detail_record(
            "RUNTIME",
            profile,
            &[
                ("profile", "identity"),
                ("artifact", "artifact_identity"),
                ("backend", "backend"),
                ("kind", "engine_kind"),
                ("strategy", "strategy"),
                ("context", "context"),
                ("blocker", "blocker"),
            ],
            width,
            styled,
        )?);
        output.push_str(
            &presentation::record(
                "",
                &[(
                    "readiness",
                    if profile["launchable"] == true {
                        "launchable"
                    } else {
                        "not launchable"
                    },
                )],
                width,
                styled,
            )
            .map_err(|error| failure(&error.to_string()))?,
        );
    }
    for engine in facts["loaded_engines"].as_array().into_iter().flatten() {
        output.push_str(&detail_record(
            "ENGINE",
            engine,
            &[
                ("profile", "profile_identity"),
                ("variant", "variant"),
                ("generation", "generation"),
                ("sessions", "sessions"),
            ],
            width,
            styled,
        )?);
    }
    for component in facts["components"].as_array().into_iter().flatten() {
        output.push_str(&detail_record(
            "COMPONENT",
            component,
            &[
                ("role", "role"),
                ("path", "path"),
                ("precision", "precision"),
                ("present", "present"),
                ("bytes", "size_bytes"),
            ],
            width,
            styled,
        )?);
    }
    Ok(output)
}

pub(crate) fn dispatch(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String, Error> {
    let library = Library::open(
        invocation.value("--models-root"),
        invocation.value("--registry"),
    )?;
    let mut models = Vec::new();
    let search = invocation.operation.operation_id == "model.search";
    let query = invocation
        .positionals
        .first()
        .map(String::as_str)
        .unwrap_or("");
    let selector = if search {
        None
    } else {
        invocation.positionals.first()
    };
    for index in 0..library.count() {
        if selector.is_none() || library.matches(index, selector.unwrap())? {
            let model = library.snapshot(index)?;
            if !search || query_matches(&model.entry, query) {
                models.push(model);
            }
        }
    }
    if search {
        let (page, limit) = search_window(invocation)?;
        models = models
            .into_iter()
            .skip(((page - 1) * limit) as usize)
            .take(limit as usize)
            .collect();
    }
    if selector.is_some() && models.len() != 1 {
        return Err(failure(if models.is_empty() {
            "model selector not found"
        } else {
            "ambiguous; use the exact identity"
        }));
    }
    let engines = client::engines(None).unwrap_or_default();
    let json_mode = invocation.has("--json") || invocation.value("--output") == Some("json");
    if json_mode {
        let value = if search {
            json!({ "schema": "yvex.model.search.v1", "provider": "local", "query": query,
                "models": models.iter().map(|model| facts(model, &engines).contract).collect::<Vec<_>>() })
        } else if selector.is_some() {
            json!({ "schema": "yvex.model.v4", "model": facts(&models[0], &engines).contract })
        } else {
            json!({ "schema": "yvex.model.list.v4", "models": models.iter().map(|model| facts(model, &engines).contract)
                .collect::<Vec<_>>() })
        };
        return Ok(format!("{value}\n"));
    }
    let mut output = if search {
        format!("LOCAL MODELS · {query:?}\n\n")
    } else {
        String::new()
    };
    // Visibility follows the native catalog, just as the JSON projection does.
    // Working-set selection controls deployment intent, not catalog membership.
    for model in &models {
        let view = facts(model, &engines);
        let facts = view.contract;
        let state = facts["state"].as_str().unwrap();
        let state_role = match state {
            "LOADED" | "READY" => replai::Role::Success,
            "BLOCKED" | "UNSUPPORTED" | "UNBOUND" => replai::Role::Warning,
            _ => replai::Role::Dim,
        };
        let metadata = format!(
            " · {} · {} · {} · {} representations",
            facts["execution"].as_str().unwrap(),
            facts["format"].as_str().unwrap(),
            size(view.size),
            facts["representation_count"]
        );
        output.push_str(
            &presentation::spans(
                &[
                    (replai::Role::Accent, facts["selector"].as_str().unwrap()),
                    (replai::Role::Default, "  "),
                    (state_role, state),
                    (replai::Role::Dim, &metadata),
                ],
                width,
                styled,
            )
            .map_err(|error| failure(&error.to_string()))?,
        );
        if invocation.has("--wide") {
            output.push_str(
                &presentation::record(
                    "CATALOG",
                    &[
                        ("family", facts["family"].as_str().unwrap()),
                        ("origin", facts["origin"].as_str().unwrap()),
                        ("location", facts["location"].as_str().unwrap()),
                    ],
                    width,
                    styled,
                )
                .map_err(|error| failure(&error.to_string()))?,
            );
        }
        if selector.is_some()
            || invocation.has("--audit")
            || invocation.value("--output") == Some("audit")
        {
            output.push_str(&model_detail(&facts, width, styled)?);
        }
        for profile in model
            .profiles
            .iter()
            .filter(|profile| profile.launchable == 0 && profile.blocker[0] != 0)
        {
            if model.entry.profile_launchable == 0 || selector.is_some() {
                output.push_str(
                    &presentation::spans(
                        &[
                            (replai::Role::Warning, "  BLOCKER  "),
                            (replai::Role::Default, &ffi::text(&profile.blocker)),
                        ],
                        width,
                        styled,
                    )
                    .map_err(|error| failure(&error.to_string()))?,
                );
            }
        }
    }
    if output.is_empty() {
        output.push_str("no models in the catalog\n");
    }
    Ok(output)
}

pub(crate) fn query_matches(entry: &raw::yvex_model_library_entry, query: &str) -> bool {
    let query = query.to_ascii_lowercase();
    [
        &entry.display_name[..],
        &entry.model[..],
        &entry.family[..],
        &entry.runtime_target[..],
        &entry.identity[..],
        &entry.repository[..],
    ]
    .into_iter()
    .any(|field| ffi::text(field).to_ascii_lowercase().contains(&query))
}

fn search_window(invocation: &Invocation<'_>) -> Result<(u32, u32), Error> {
    let parse = |flag: &str, default: u32, maximum: u32| {
        let value = invocation
            .value(flag)
            .map(str::parse::<u32>)
            .transpose()
            .map_err(|_| failure("search paging must be an integer"))?
            .unwrap_or(default);
        if value == 0 || value > maximum {
            return Err(failure("search paging is outside its bounded range"));
        }
        Ok(value)
    };
    Ok((
        parse("--page", 1, 20)?,
        parse("--limit", if invocation.has("--all") { 50 } else { 8 }, 50)?,
    ))
}

pub(crate) fn selected_model(
    invocation: &Invocation<'_>,
) -> Result<(Library, u64, ModelSnapshot), Error> {
    let library = Library::open(
        invocation.value("--models-root"),
        invocation.value("--registry"),
    )?;
    let mut selected = None;
    for index in 0..library.count() {
        if library.matches(index, &invocation.positionals[0])? {
            if selected.is_some() {
                return Err(failure("model selector is ambiguous"));
            }
            selected = Some(index);
        }
    }
    let index = selected.ok_or_else(|| failure("model selector not found"))?;
    let snapshot = library.snapshot(index)?;
    Ok((library, index, snapshot))
}

pub(crate) fn storage_projection(facts: &ffi::catalog::StorageFacts) -> Value {
    let rows = facts
        .rows
        .iter()
        .map(|row| {
            json!({ "path": ffi::text(&row.path), "role": ffi::text(&row.role),
        "exists": row.exists != 0, "attributable_to_model": row.attributable != 0,
        "logical_bytes": row.logical_bytes, "allocated_bytes": row.allocated_bytes,
        "files": row.files, "shared_files": row.shared_files })
        })
        .collect::<Vec<_>>();
    json!({ "schema": "yvex.model.storage.v1", "rows": rows, "logical_bytes": facts.logical_bytes,
        "allocated_bytes": facts.allocated_bytes, "allocation_method": "st_blocks; device/inode deduplicated",
        "reflink_shared_extents": Value::Null, "historical_peak_bytes": Value::Null })
}

fn model_storage(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String, Box<dyn std::error::Error>> {
    let (library, index, model) = selected_model(invocation)?;
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let facts = library.storage(
        index,
        &ffi::text(&paths.operator.models_root),
        invocation.has("--include-caches"),
    )?;
    if invocation.has("--json") {
        return Ok(format!("{}\n", storage_projection(&facts)));
    }
    let mut output = presentation::lines(
        &[format!(
            "STORAGE  {} · logical {} · allocated {}",
            selector(&model),
            size(Some(facts.logical_bytes)),
            size(Some(facts.allocated_bytes))
        )],
        width,
        styled,
    )?;
    for row in &facts.rows {
        let state = if row.exists == 0 {
            "absent"
        } else if row.attributable != 0 {
            "model-owned reference"
        } else {
            "shared cache scope; model attribution unknown"
        };
        output.push_str(&presentation::record(
            &ffi::text(&row.role),
            &[
                ("path", &ffi::text(&row.path)),
                ("state", state),
                ("logical", &size(Some(row.logical_bytes))),
                ("allocated", &size(Some(row.allocated_bytes))),
                ("files", &row.files.to_string()),
                ("shared_files", &row.shared_files.to_string()),
            ],
            width,
            styled,
        )?);
    }
    output.push_str(&presentation::lines(
        &[
            "Inodes counted once; cache rows are not a per-model disk charge.".into(),
            "Reflink sharing and historical peak are unknown.".into(),
        ],
        width,
        styled,
    )?);
    Ok(output)
}

fn model_evict(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String, Box<dyn std::error::Error>> {
    let (library, index, model) = selected_model(invocation)?;
    let source = invocation.value("--representation") == Some("source");
    let variant = invocation.value("--variant");
    let selected = if source {
        model
            .sources
            .iter()
            .enumerate()
            .filter(|(_, row)| {
                ffi::text(&row.provider) == "huggingface"
                    && variant.is_none_or(|value| value == ffi::text(&row.digest))
            })
            .map(|(index, _)| index)
            .collect::<Vec<_>>()
    } else {
        model
            .artifacts
            .iter()
            .enumerate()
            .filter(|(_, (row, _))| {
                variant.is_none_or(|value| {
                    value == ffi::text(&row.identity) || value == ffi::text(&row.physical_variant)
                })
            })
            .map(|(index, _)| index)
            .collect::<Vec<_>>()
    };
    if selected.len() != 1 {
        return Err(Box::new(failure(
            "select one representation with --variant SHA256",
        )));
    }
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let result = library.evict(
        index,
        selected[0] as u64,
        source,
        &ffi::text(&paths.operator.models_root),
        invocation.has("--dry-run"),
    )?;
    if invocation.has("--json") {
        return Ok(format!(
            "{}\n",
            json!({ "schema": "yvex.model.evict.v1",
        "changed": result.changed != 0, "local": result.local != 0, "logical_bytes": result.logical_bytes,
        "allocated_bytes": result.allocated_bytes, "path": ffi::text(&result.path) })
        ));
    }
    Ok(presentation::record(
        &format!("MODEL  {}", selector(&model)),
        &[
            ("location", &ffi::text(&result.path)),
            (
                "action",
                if invocation.has("--dry-run") {
                    "dry-run; no bytes removed"
                } else if result.changed != 0 {
                    "local payload evicted"
                } else {
                    "none; already remote-only"
                },
            ),
            ("remote", "exact catalog identity retained"),
        ],
        width,
        styled,
    )?)
}

fn push_selection(
    model: &ModelSnapshot,
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<(String, String, String), Error> {
    let artifacts =
        !model.artifacts.is_empty() && invocation.value("--representation") != Some("source");
    if artifacts {
        let variant = invocation.value("--variant");
        let choices = model
            .artifacts
            .iter()
            .map(|(row, _)| row)
            .filter(|row| {
                variant.is_none_or(|value| {
                    value == ffi::text(&row.identity)
                        || value.eq_ignore_ascii_case(&ffi::text(&row.physical_variant))
                        || value.eq_ignore_ascii_case(&ffi::text(&row.artifact_class))
                })
            })
            .collect::<Vec<_>>();
        let chosen = match choices.len() {
            0 => return Err(failure("no publishable representation matches --variant")),
            1 => 0,
            _ if variant.is_none() && !invocation.has("--json") => interactive_choice(
                &choices
                    .iter()
                    .map(|row| {
                        format!(
                            "{} · {} · {}",
                            ffi::text(&row.format),
                            ffi::text(&row.physical_variant),
                            ffi::text(&row.identity)
                        )
                    })
                    .collect::<Vec<_>>(),
                "PUBLISH REPRESENTATION",
                width,
                styled,
            )?,
            _ => {
                return Err(failure(
                    "multiple publishable representations exist; pass an exact --variant",
                ));
            }
        };
        let row = choices[chosen];
        Ok((
            ffi::text(&row.path),
            ffi::text(&row.identity),
            ffi::text(&row.format),
        ))
    } else {
        if invocation.value("--representation") == Some("artifact") {
            return Err(unavailable_representation(
                "model has no local artifact to publish",
            ));
        }
        let row = model
            .sources
            .first()
            .filter(|row| row.path[0] != 0)
            .ok_or_else(|| {
                unavailable_representation("model has no local publishable representation")
            })?;
        Ok((
            ffi::text(&row.path),
            ffi::text(&row.digest),
            ffi::text(&row.format),
        ))
    }
}

fn model_push(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<crate::Output, Box<dyn std::error::Error>> {
    let destination = &invocation.positionals[1];
    if let Some((scheme, _)) = destination.split_once("://")
        && scheme != "file"
    {
        return Ok(crate::Output {
            diagnostic: true,
            exit: 3,
            text: presentation::record(
                "PUBLISH  unavailable",
                &[
                    ("transport", scheme),
                    ("reason", "provider write unavailable; no state changed"),
                ],
                width,
                styled,
            )?,
        });
    }
    let (_library, _, model) = selected_model(invocation)?;
    let (path, digest, format) = push_selection(&model, invocation, width, styled)?;
    let result = ffi::catalog::export(&path, destination, &digest)?;
    let output = if invocation.has("--json") {
        format!(
            "{}\n",
            json!({ "schema": "yvex.model.push.v1", "model": selector(&model),
            "representation_identity": ffi::text(&result.digest), "destination": ffi::text(&result.destination_path),
            "bytes": result.bytes })
        )
    } else {
        presentation::record(
            &format!("MODEL  {}", selector(&model)),
            &[
                ("representation", &format),
                ("bytes", &result.bytes.to_string()),
                ("digest", &ffi::text(&result.digest)),
                ("destination", &ffi::text(&result.destination_path)),
                ("state", "published"),
            ],
            width,
            styled,
        )?
    };
    Ok(crate::Output::standard(output, 0))
}

pub(crate) fn lifecycle(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<crate::Output, Box<dyn std::error::Error>> {
    match invocation.operation.operation_id.as_str() {
        "model.storage" => {
            model_storage(invocation, width, styled).map(|text| crate::Output::standard(text, 0))
        }
        "model.evict" => {
            model_evict(invocation, width, styled).map(|text| crate::Output::standard(text, 0))
        }
        "model.push" => model_push(invocation, width, styled),
        _ => unreachable!("registry-selected model lifecycle operation"),
    }
}

#[derive(Default)]
struct RemoteLocal {
    source_revision: String,
    package_revision: String,
    source_format: String,
    package_format: String,
    source: bool,
    package: bool,
    related: bool,
}
fn remote_local(
    snapshot: &ffi::catalog::RemoteSnapshot,
    model: &raw::yvex_remote_model,
) -> RemoteLocal {
    let repository = ffi::text(&model.repository);
    let revision = ffi::text(&model.resolved_revision);
    let mut local = RemoteLocal::default();
    for source in &snapshot.sources {
        if ffi::text(&source.repository) == repository && !repository.is_empty() {
            local.source_revision = ffi::text(&source.revision);
            local.source_format = ffi::text(&source.representation);
            if revision.is_empty()
                || local.source_revision.is_empty()
                || local.source_revision != revision
            {
                local.related = true;
            } else {
                local.source = true;
            }
        }
    }
    for package in &snapshot.packages {
        if ffi::text(&package.repository) == repository && !repository.is_empty() {
            local.package_revision = ffi::text(&package.revision);
            local.package_format = ffi::text(&package.representation);
            if revision.is_empty()
                || local.package_revision.is_empty()
                || local.package_revision != revision
            {
                local.related = true;
            } else {
                local.package = true;
            }
        }
    }
    local
}
fn remote_representation_local(
    local: &RemoteLocal,
    kind: raw::yvex_model_representation_kind,
) -> bool {
    let format = match kind {
        raw::yvex_model_representation_kind_YVEX_MODEL_REPRESENTATION_SAFETENSORS => "safetensors",
        raw::yvex_model_representation_kind_YVEX_MODEL_REPRESENTATION_GGUF => "gguf",
        _ => return false,
    };
    (local.source && local.source_format.contains(format))
        || (local.package && local.package_format.contains(format))
}
fn remote_product_status(model: &raw::yvex_remote_model) -> &'static str {
    match model.kind {
        raw::yvex_remote_model_kind_YVEX_REMOTE_MODEL_ADAPTER
        | raw::yvex_remote_model_kind_YVEX_REMOTE_MODEL_COMPONENT
        | raw::yvex_remote_model_kind_YVEX_REMOTE_MODEL_DELTA
        | raw::yvex_remote_model_kind_YVEX_REMOTE_MODEL_DERIVATIVE => return "related",
        _ => (),
    }
    match model.support_stage {
        raw::yvex_model_support_stage_YVEX_MODEL_SUPPORT_PACKAGE_PREPARATION => "supported",
        raw::yvex_model_support_stage_YVEX_MODEL_SUPPORT_PHYSICAL_INSPECTION => "inspect",
        raw::yvex_model_support_stage_YVEX_MODEL_SUPPORT_SOURCE_INGEST => "acquirable",
        raw::yvex_model_support_stage_YVEX_MODEL_SUPPORT_ARCHITECTURE_RECOGNIZED => "recognized",
        _ => "unknown",
    }
}
pub(crate) fn remote_json(
    snapshot: &ffi::catalog::RemoteSnapshot,
    model: &ffi::catalog::RemoteModel,
) -> Result<Value, Error> {
    let native = &model.facts;
    let local = remote_local(snapshot, native);
    let representations = model.representations.iter().map(|representation| {
        json!({ "identity": ffi::text(&representation.identity), "format": ffi::text(&representation.format),
            "precision": ffi::text(&representation.precision),
            "precision_evidence": ffi::text(&representation.precision_evidence),
            "file_pattern": ffi::text(&representation.file_pattern),
            "compatibility": ffi::text(&representation.compatibility),
            "file_count": representation.file_count, "size_bytes": representation.size_bytes,
            "size_known": representation.size_known != 0, "provisional": representation.provisional != 0,
            "local": remote_representation_local(&local, representation.kind) })
    }).collect::<Vec<_>>();
    let files = model.files.iter().map(|file| {
        Ok(json!({ "path": ffi::text(&file.path), "kind": ffi::catalog::remote_file_kind(file.kind)?,
            "representation": ffi::text(&file.representation),
            "size_bytes": file.size_bytes, "size_known": file.size_known != 0 }))
    }).collect::<Result<Vec<Value>, Error>>()?;
    Ok(
        json!({ "provider": ffi::text(&native.provider), "repository": ffi::text(&native.repository),
        "requested_revision": ffi::text(&native.revision_reference),
        "resolved_revision": ffi::text(&native.resolved_revision),
        "kind": ffi::catalog::remote_kind(native.kind)?, "kind_evidence": ffi::text(&native.kind_evidence),
        "model_identity": ffi::text(&native.model_identity), "family_affinity": ffi::text(&native.family),
        "family_evidence": ffi::text(&native.family_evidence), "architecture": ffi::text(&native.architecture),
        "base_model": ffi::text(&native.base_model),
        "support_stage": ffi::catalog::support_stage(native.support_stage)?,
        "product_status": remote_product_status(native), "engine_state": "not-observed",
        "local_source_revision": local.source_revision, "local_package_revision": local.package_revision,
        "local_source": local.source, "local_package": local.package, "local_related_revision": local.related,
        "kind_provisional": native.kind_provisional != 0, "canonical": native.canonical != 0,
        "ranking_score": native.ranking_score, "provider_rank": native.provider_rank,
        "gated_known": native.gated_known != 0, "gated": native.gated != 0,
        "parameter_count_known": native.parameter_count_known != 0, "parameter_count": native.parameter_count,
        "representations": representations, "files": files }),
    )
}

fn remote_human(
    model: &Value,
    inspect: bool,
    audit: bool,
    width: usize,
    styled: bool,
) -> Result<String, Error> {
    let string = |key: &str| model[key].as_str().unwrap_or("");
    let label = format!(
        "{} · {} · {}",
        string("repository"),
        string("kind"),
        string("product_status")
    );
    let mut output = presentation::lines(&[label], width, styled)
        .map_err(|error| failure(&error.to_string()))?;
    if !inspect && !audit {
        let representation = model["representations"]
            .as_array()
            .unwrap()
            .iter()
            .map(|row| row["format"].as_str().unwrap_or("unknown"))
            .collect::<Vec<_>>();
        let location = format!(
            "hf://{}@{}",
            string("repository"),
            string("resolved_revision")
        );
        output.push_str(
            &presentation::lines(
                &[format!("  {} · {}", representation.join(" / "), location)],
                width,
                styled,
            )
            .map_err(|error| failure(&error.to_string()))?,
        );
        return Ok(output);
    }
    let fields = model
        .as_object()
        .unwrap()
        .iter()
        .filter(|(_, value)| !value.is_array())
        .map(|(key, value)| {
            (
                if key == "family_affinity" {
                    "family"
                } else {
                    key.as_str()
                },
                if key == "family_affinity" && value.as_str() == Some("") {
                    "unknown".into()
                } else {
                    value
                        .as_str()
                        .map_or_else(|| value.to_string(), str::to_owned)
                },
            )
        })
        .collect::<Vec<_>>();
    let pairs = fields
        .iter()
        .map(|(key, value)| (*key, value.as_str()))
        .collect::<Vec<_>>();
    output.push_str(
        &presentation::record("PROVENANCE", &pairs, width, styled)
            .map_err(|error| failure(&error.to_string()))?,
    );
    for (category, title) in [("representations", "REPRESENTATION"), ("files", "FILE")] {
        for row in model[category].as_array().unwrap() {
            let fields = row
                .as_object()
                .unwrap()
                .iter()
                .map(|(key, value)| {
                    (
                        key.as_str(),
                        value
                            .as_str()
                            .map_or_else(|| value.to_string(), str::to_owned),
                    )
                })
                .collect::<Vec<_>>();
            let pairs = fields
                .iter()
                .map(|(key, value)| (*key, value.as_str()))
                .collect::<Vec<_>>();
            output.push_str(
                &presentation::record(title, &pairs, width, styled)
                    .map_err(|error| failure(&error.to_string()))?,
            );
        }
    }
    Ok(output)
}

pub(crate) fn discover(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<String, Box<dyn std::error::Error>> {
    let provider = invocation.value("--provider").unwrap_or("huggingface");
    let inspect = matches!(
        invocation.operation.operation_id.as_str(),
        "source.inspect" | "model.show"
    );
    if provider == "local" && !inspect {
        if invocation.has("--author") || invocation.has("--filter") {
            return Err(Box::new(crate::registry::Refusal {
                reason: "local model search does not admit --author/--filter".into(),
                hint: None,
            }));
        }
        return Ok(dispatch(invocation, width, styled)?);
    }
    let locator = if invocation.operation.operation_id == "model.show" {
        Some(ffi::catalog::locator(&invocation.positionals[0])?)
    } else {
        None
    };
    let repository = locator
        .as_ref()
        .map(|locator| ffi::text(&locator.repository));
    let revision = locator.as_ref().map(|locator| ffi::text(&locator.revision));
    let request = if inspect {
        ffi::RemoteRequest::Inspect {
            repository: repository.as_deref().unwrap_or(&invocation.positionals[0]),
            revision: revision
                .as_deref()
                .filter(|revision| !revision.is_empty())
                .or_else(|| invocation.value("--revision")),
        }
    } else {
        let (page, limit) = search_window(invocation)?;
        ffi::RemoteRequest::Search {
            query: invocation.positionals.first().map(String::as_str),
            author: invocation.value("--author"),
            filter: invocation.value("--filter"),
            page,
            limit,
        }
    };
    let snapshot = ffi::remote_catalog(request, invocation.value("--models-root"))?;
    let models = snapshot
        .models
        .iter()
        .map(|model| remote_json(&snapshot, model))
        .collect::<Result<Vec<_>, _>>()?;
    if invocation.has("--json") || invocation.value("--output") == Some("json") {
        return Ok(format!(
            "{}\n",
            json!({ "schema": "yvex.model-catalog-projection.v1",
            "authorities": ["remote-provider", "local-catalog"], "query": snapshot.query,
            "provider_result_count": snapshot.provider_count, "models": models })
        ));
    }
    let mut output = format!("REMOTE MODELS · {:?}\n\n", snapshot.query);
    for model in &models {
        output.push_str(&remote_human(
            model,
            inspect,
            invocation.has("--audit") || invocation.value("--output") == Some("audit"),
            width,
            styled,
        )?);
    }
    if models.is_empty() {
        output.push_str("no provider matches\n");
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn source_metadata_is_not_execution_support() {
        let mut model = ModelSnapshot {
            entry: Default::default(),
            working_set: false,
            sources: Vec::new(),
            artifacts: Vec::new(),
            profiles: Vec::new(),
            publications: Vec::new(),
        };
        ffi::put_text(&mut model.entry.family, "unknown").unwrap();
        let mut source = raw::yvex_local_source_record::default();
        ffi::put_text(&mut source.path, "/metadata-only").unwrap();
        ffi::put_text(&mut source.acquisition_state, "source-acquired").unwrap();
        model.sources.push(source);
        assert_eq!(model_state(&model), "UNSUPPORTED");
        assert!(facts(&model, &[]).contract["selected_profile"].is_null());
    }

    #[test]
    fn runtime_choice_retains_exact_revisions_and_variant_identity() {
        let mut model = ModelSnapshot {
            entry: Default::default(),
            working_set: true,
            sources: Vec::new(),
            artifacts: Vec::new(),
            profiles: Vec::new(),
            publications: Vec::new(),
        };
        ffi::put_text(&mut model.entry.model, "control").unwrap();
        let mut artifact = raw::yvex_model_artifact_fact::default();
        ffi::put_text(&mut artifact.identity, &"a".repeat(64)).unwrap();
        ffi::put_text(&mut artifact.physical_variant, "BF16").unwrap();
        ffi::put_text(&mut artifact.format, "gguf").unwrap();
        model.artifacts.push((artifact, true));
        let mut profile = raw::yvex_model_runtime_profile_fact {
            launchable: 1,
            ..Default::default()
        };
        ffi::put_text(&mut profile.alias, "control-cpu-1").unwrap();
        ffi::put_text(&mut profile.artifact_identity, &"a".repeat(64)).unwrap();
        ffi::put_text(&mut profile.backend, "cpu").unwrap();
        ffi::put_text(&mut profile.engine_kind, "text").unwrap();
        ffi::put_text(&mut profile.execution_strategy, "autoregressive").unwrap();
        model.profiles.push(profile);
        let mut revision = profile;
        ffi::put_text(&mut revision.alias, "control-cpu-2").unwrap();
        model.profiles.push(revision);
        let mut cuda = profile;
        ffi::put_text(&mut cuda.backend, "cuda").unwrap();
        model.profiles.push(cuda);
        assert_eq!(choices(&model), vec![1, 2]);
        let choice = runtime_choice(&model, &revision);
        assert_eq!(choice.alias, "control-cpu-2");
        assert_eq!(choice.contract["variant"], "BF16@aaaaaaaa");
        assert_eq!(choice.contract["artifact_identity"], "a".repeat(64));
        assert!(choice_matches(&model, &revision, &choice, "BF16"));
        assert!(choice_matches(&model, &revision, &choice, "BF16@aaaaaaaa"));
        assert!(choice_matches(&model, &revision, &choice, &"a".repeat(64)));
        assert!(!choice_matches(&model, &revision, &choice, "BF16@bbbbbbbb"));
    }
}
