// Exact source/artifact/deployment projections share the native model-library snapshot.
use crate::{
    ffi::{self, Library, ModelSnapshot, raw},
    presentation,
    registry::{Invocation, Refusal},
};
use serde_json::{Value, json};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn reject(reason: &str) -> Box<dyn std::error::Error> {
    Box::new(Refusal {
        reason: reason.into(),
        hint: None,
    })
}

fn deployment<'a>(invocation: &'a Invocation<'_>) -> Result<ffi::Deployment<'a>> {
    let binding = invocation.value("--runtime-binding");
    let target = invocation.value("--target");
    let backend = invocation.value("--backend");
    let strategy = invocation
        .ordered_flags
        .iter()
        .rev()
        .find(|(name, _)| name == "--execution-strategy" || name == "--generation-mode")
        .map(|(_, value)| value.as_str());
    let context = invocation.value("--ctx");
    let root = invocation.value("--installation-root");
    match invocation.value("--startup-profile") {
        Some("composite") => {
            if binding.is_some() || strategy.is_some() || context.is_some() {
                return Err(reject(
                    "composite startup profile does not accept --runtime-binding, --execution-strategy or --ctx",
                ));
            }
            Ok(ffi::Deployment::Composite {
                root: root.ok_or_else(|| {
                    reject("composite startup profile requires --installation-root")
                })?,
                target: target
                    .ok_or_else(|| reject("composite startup profile requires --target"))?,
                backend: backend
                    .ok_or_else(|| reject("composite startup profile requires --backend"))?,
            })
        }
        mode @ (None | Some("single-artifact")) => {
            if [binding, target, backend, strategy, context]
                .iter()
                .all(Option::is_none)
                && mode.is_none()
            {
                if root.is_some() {
                    return Err(reject(
                        "--installation-root requires composite startup profile",
                    ));
                }
                return Ok(ffi::Deployment::Inspection);
            }
            if root.is_some() {
                return Err(reject(
                    "single-artifact startup profile does not accept --installation-root",
                ));
            }
            let context = context
                .ok_or_else(|| {
                    reject(concat!(
                        "startup profile requires --runtime-binding, --target, --backend, ",
                        "--execution-strategy and --ctx together"
                    ))
                })?
                .parse::<u64>()
                .map_err(|_| reject("--ctx requires a positive integer"))?;
            if context == 0 {
                return Err(reject("--ctx requires a positive integer"));
            }
            Ok(ffi::Deployment::Single {
                binding: binding
                    .ok_or_else(|| reject("startup profile requires --runtime-binding"))?,
                target: target.ok_or_else(|| reject("startup profile requires --target"))?,
                backend: backend.ok_or_else(|| reject("startup profile requires --backend"))?,
                strategy: strategy
                    .ok_or_else(|| reject("startup profile requires --execution-strategy"))?,
                context,
            })
        }
        _ => Err(reject(
            "--startup-profile requires single-artifact or composite",
        )),
    }
}

pub(crate) fn profile_command(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<crate::Output> {
    if invocation.operation.operation_id == "profile.create" {
        let request = ffi::ProfileRequest {
            path: invocation
                .value("--path")
                .ok_or_else(|| reject("profile create requires --path FILE"))?,
            alias: invocation.value("--alias"),
            family: invocation.value("--family"),
            model: invocation.value("--model"),
            scope: invocation.value("--scope"),
            class: invocation.value("--class"),
            qprofile: invocation.value("--qprofile"),
            calibration: invocation.value("--calibration"),
            support: invocation.value("--support-level"),
            expected_sha256: invocation.value("--sha256"),
            deployment: deployment(invocation)?,
        };
        let receipt = ffi::profile_create(&request, invocation.value("--registry"))?;
        return Ok(crate::Output::standard(
            presentation::record(
                "PROFILE  recorded",
                &[
                    ("alias", &ffi::text(&receipt.alias)),
                    ("path", request.path),
                    ("sha256", &ffi::text(&receipt.sha256)),
                    ("file_size", &receipt.file_size.to_string()),
                    ("identity_status", "recorded"),
                    ("status", "models-added"),
                ],
                width,
                styled,
            )?,
            0,
        ));
    }
    let result = ffi::profile_verify(&invocation.positionals[0], invocation.value("--registry"))?;
    let audit = invocation.has("--audit") || invocation.value("--output") == Some("audit");
    if invocation
        .value("--output")
        .is_some_and(|mode| !["normal", "table", "audit"].contains(&mode))
    {
        return Err(reject("unsupported output mode for profile verify"));
    }
    let mut fields = vec![
        ("alias".to_string(), result.alias),
        ("identity_status".into(), result.identity_status.clone()),
        ("digest_status".into(), result.identity_status),
        ("metadata_status".into(), result.metadata_status),
        ("readiness_status".into(), result.readiness_status),
        ("reason".into(), result.reason),
        ("status".into(), result.status),
        (
            "boundary".into(),
            "identity verified, runtime generation unsupported".into(),
        ),
    ];
    if audit {
        fields.extend([
            ("path".into(), result.path),
            ("registered_sha256".into(), result.registered_sha256),
            ("current_sha256".into(), ffi::text(&result.current.sha256)),
            (
                "registered_file_size".into(),
                result.registered_size.to_string(),
            ),
            (
                "current_file_size".into(),
                result.current.file_size.to_string(),
            ),
        ]);
        for (key, before, after) in result.pairs {
            fields.push((format!("registered_{key}"), before));
            fields.push((format!("current_{key}"), after));
        }
        for (index, issue) in result.issues.iter().enumerate() {
            fields.push((
                format!("metadata_issue_{index}_code"),
                ffi::text(&issue.code),
            ));
            fields.push((
                format!("metadata_issue_{index}_registered"),
                ffi::text(&issue.registered_value),
            ));
            fields.push((
                format!("metadata_issue_{index}_current"),
                ffi::text(&issue.current_value),
            ));
        }
    }
    let borrowed: Vec<_> = fields
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect();
    // A diagnostic report on stdout is not a successful verification. Keep the
    // native state failure exit, even for the old reserved --json spelling.
    Ok(crate::Output::standard(
        presentation::record("PROFILE  verification", &borrowed, width, styled)?,
        if result.passed { 0 } else { 4 },
    ))
}

fn source(model: &ModelSnapshot, source: &raw::yvex_local_source_record) -> Value {
    let provider = ffi::text(&source.provider);
    let repository = ffi::text(&source.repository);
    let revision = ffi::text(&source.revision);
    json!({
        "identity": format!("{provider}:{repository}@{revision}"),
        "model_identity": ffi::text(&model.entry.identity),
        "name": ffi::text(&source.name), "family": ffi::text(&source.family),
        "provider": provider, "repository": repository, "revision": revision,
        "representation": ffi::text(&source.representation),
        "acquisition_state": ffi::text(&source.acquisition_state),
        "verification_state": ffi::text(&source.verification_state),
        "blocker": ffi::text(&source.blocker), "path": ffi::text(&source.path),
        "size_bytes": source.size_bytes, "size_known": source.size_known != 0,
    })
}

fn artifact(model: &ModelSnapshot, artifact: &raw::yvex_model_artifact_fact) -> Value {
    let identity = ffi::text(&artifact.identity);
    let path = ffi::text(&artifact.path);
    let profiles: Vec<_> = model
        .profiles
        .iter()
        .filter(|profile| {
            if identity.is_empty() {
                ffi::text(&profile.artifact_path) == path
            } else {
                ffi::text(&profile.artifact_identity) == identity
            }
        })
        .collect();
    json!({
        "identity": if identity.is_empty() { &path } else { &identity },
        "model_identity": ffi::text(&model.entry.identity), "path": path,
        "artifact_class": ffi::text(&artifact.artifact_class),
        "format": ffi::text(&artifact.format), "physical_variant": ffi::text(&artifact.physical_variant),
        "file_size": artifact.file_size, "tensor_count": artifact.tensor_count,
        "execution_ready": artifact.execution_ready != 0, "profile_count": profiles.len(),
        "launchable_profile_count": profiles.iter().filter(|profile| profile.launchable != 0).count(),
    })
}

fn profile(model: &ModelSnapshot, profile: &raw::yvex_model_runtime_profile_fact) -> Value {
    let deployment = [
        ffi::text(&profile.artifact_class),
        ffi::text(&profile.profile),
        "default".into(),
    ]
    .into_iter()
    .find(|value| !value.is_empty())
    .expect("default deployment label");
    json!({
        "identity": ffi::text(&profile.alias), "model_identity": ffi::text(&model.entry.identity),
        "profile_class": ffi::text(&profile.profile), "installation_root": ffi::text(&profile.installation),
        "artifact_identity": ffi::text(&profile.artifact_identity),
        "artifact_path": ffi::text(&profile.artifact_path), "runtime_binding": ffi::text(&profile.runtime_binding),
        "runtime_target": ffi::text(&profile.runtime_target), "deployment_class": deployment,
        "backend": ffi::text(&profile.backend), "engine_kind": ffi::text(&profile.engine_kind),
        "execution_strategy": ffi::text(&profile.execution_strategy), "blocker": ffi::text(&profile.blocker),
        "context_capacity": profile.context_capacity, "launchable": profile.launchable != 0,
        "capabilities": {"input_mask": profile.capabilities.input_kinds,
            "output_mask": profile.capabilities.output_kinds, "properties": profile.capabilities.execution_properties,
            "maximum_input_parts": profile.capabilities.maximum_input_parts},
    })
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    if let Some(mode) = invocation.value("--output")
        && !matches!(mode, "table" | "audit" | "json")
    {
        return Err(reject(&format!("invalid value for --output: {mode}")));
    }
    let operation = invocation.operation.operation_id.as_str();
    if operation == "profile.remove" {
        let alias = &invocation.positionals[0];
        ffi::profile_remove(alias, invocation.value("--registry"))?;
        return Ok(presentation::record(
            "PROFILE  removed",
            &[("removed", alias), ("status", "models-removed")],
            width,
            styled,
        )?);
    }
    if operation == "profile.scan" {
        return scan(invocation, width, styled);
    }
    let (domain, plural) = match operation {
        "source.list" | "source.show" => ("source", "sources"),
        "artifact.list" => ("artifact", "artifacts"),
        "profile.list" | "profile.show" => ("profile", "profiles"),
        _ => return Err("unprojected catalog operation".into()),
    };
    let library = Library::open(
        invocation.value("--models-root"),
        invocation.value("--registry"),
    )?;
    let mut records = Vec::new();
    for index in 0..library.count() {
        let model = library.snapshot(index)?;
        records.extend(match domain {
            "source" => model
                .sources
                .iter()
                .map(|item| source(&model, item))
                .collect::<Vec<_>>(),
            "artifact" => model
                .artifacts
                .iter()
                .map(|(item, _)| artifact(&model, item))
                .collect(),
            "profile" => model
                .profiles
                .iter()
                .map(|item| profile(&model, item))
                .collect(),
            _ => unreachable!("admitted catalog domain"),
        });
    }
    let detail = operation.ends_with(".show");
    if detail {
        let identity = &invocation.positionals[0];
        records.retain(|record| record["identity"] == *identity);
        if records.is_empty() {
            return Err(Box::new(Refusal {
                reason: format!("{domain} not found: {identity}"),
                hint: None,
            }));
        }
        // Preserve the canonical snapshot's first exact record, not a guessed identity join.
        records.truncate(1);
    }
    if invocation.has("--json") || invocation.value("--output") == Some("json") {
        let mut result = serde_json::Map::new();
        result.insert(
            "schema".into(),
            Value::String(if detail {
                format!("yvex.{domain}.v1")
            } else {
                format!("yvex.{domain}.list.v1")
            }),
        );
        result.insert(
            if detail { domain } else { plural }.into(),
            if detail {
                records.remove(0)
            } else {
                Value::Array(records)
            },
        );
        return Ok(format!("{}\n", Value::Object(result)));
    }
    render(
        domain,
        &records,
        detail || invocation.has("--audit") || invocation.value("--output") == Some("audit"),
        width,
        styled,
    )
}

fn scan(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let root = invocation.value("--root").ok_or_else(|| {
        Box::new(Refusal {
            reason: "profile scan requires --root DIR".into(),
            hint: None,
        })
    })?;
    let entries = ffi::profile_scan(root)?;
    let mut output = String::new();
    for entry in &entries {
        output.push_str(&presentation::record(
            "PROFILE  scan candidate",
            &[
                ("candidate", &entry.alias),
                ("family", &entry.family),
                ("model", &entry.model),
                ("scope", &entry.scope),
                ("artifact_class", &entry.class),
                ("qprofile", &entry.qprofile),
                ("calibration", &entry.calibration),
                ("path", &entry.path),
            ],
            width,
            styled,
        )?);
    }
    // --json was a reserved spelling, not a released machine schema. Do not
    // invent a registry write or generation-readiness claim from a scan.
    output.push_str(&presentation::record(
        "PROFILE  scan",
        &[
            ("root", root),
            ("candidates", &entries.len().to_string()),
            ("status", "models-scan"),
        ],
        width,
        styled,
    )?);
    Ok(output)
}

fn render(
    domain: &str,
    records: &[Value],
    detail: bool,
    width: usize,
    styled: bool,
) -> Result<String> {
    if records.is_empty() {
        return Ok(presentation::lines(
            &[format!("{}  none known locally", domain.to_uppercase())],
            width,
            styled,
        )?);
    }
    let mut output = String::new();
    for record in records {
        let title = format!(
            "{}  {}",
            domain.to_uppercase(),
            record[if domain == "source" {
                "name"
            } else {
                "identity"
            }]
            .as_str()
            .unwrap_or("")
        );
        let primary: &[&str] = match domain {
            "source" => &["acquisition_state", "verification_state", "blocker"],
            "artifact" => &[
                "artifact_class",
                "format",
                "file_size",
                "launchable_profile_count",
            ],
            "profile" => &[
                "backend",
                "engine_kind",
                "execution_strategy",
                "context_capacity",
                "launchable",
                "blocker",
            ],
            _ => unreachable!("admitted catalog domain"),
        };
        let fields: Vec<_> = record
            .as_object()
            .expect("typed record")
            .iter()
            .filter(|(key, _)| detail || primary.contains(&key.as_str()))
            .filter(|(_, value)| value.as_str() != Some(""))
            .map(|(key, value)| {
                (
                    key.as_str(),
                    value
                        .as_str()
                        .map(str::to_owned)
                        .unwrap_or_else(|| value.to_string()),
                )
            })
            .collect();
        output.push_str(&presentation::record(
            &title,
            &fields
                .iter()
                .map(|(key, value)| (*key, value.as_str()))
                .collect::<Vec<_>>(),
            width,
            styled,
        )?);
    }
    Ok(output)
}
