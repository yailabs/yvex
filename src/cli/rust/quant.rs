// Operator projections of native conversion and physical qtype capability facts.
use crate::{
    Output,
    ffi::{self, ConversionRequest},
    presentation,
    registry::{Invocation, Refusal},
};
use replai::Alignment;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn required<'a>(invocation: &'a Invocation<'_>, name: &str) -> Result<&'a str> {
    invocation
        .value(name)
        .filter(|value| !value.is_empty())
        .ok_or_else(|| {
            Box::new(Refusal {
                reason: format!("{name} is required"),
                hint: None,
            }) as Box<dyn std::error::Error>
        })
}

fn boolean(value: bool) -> &'static str {
    if value { "yes" } else { "no" }
}

fn qtypes(width: usize, styled: bool) -> Result<Output> {
    let rows = ffi::qtype_support()?
        .into_iter()
        .map(|row| {
            vec![
                row.name,
                boolean(row.policy).into(),
                boolean(row.storage).into(),
                boolean(row.emit).into(),
                row.quantize.map(boolean).unwrap_or("n/a").into(),
                if row.compute { "partial" } else { "no" }.into(),
                row.notes,
            ]
        })
        .collect::<Vec<_>>();
    let headings = [
        "QTYPE", "POLICY", "STORAGE", "EMIT", "QUANTIZE", "COMPUTE", "NOTES",
    ]
    .map(|heading| (heading, Alignment::Left));
    let mut output = presentation::table(&headings, &rows, width, styled)?;
    output.push_str("status: qtype-support\n");
    Ok(Output::standard(output, 0))
}

fn conversion(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let planning = invocation.positionals[0] == "plan";
    let destination = required(invocation, if planning { "--out-plan" } else { "--out" })?;
    if !planning {
        required(invocation, "--tensor")?;
        required(invocation, "--target-qtype")?;
    }
    let limit = invocation
        .value("--limit")
        .map(str::parse::<u64>)
        .transpose()
        .map_err(|_| Refusal {
            reason: "conversion limit must be a non-negative u64".into(),
            hint: None,
        })?
        .unwrap_or(0);
    let request = ConversionRequest {
        architecture: required(invocation, "--arch")?,
        source: required(invocation, "--native-source")?,
        manifest: invocation.value("--source-manifest"),
        template: invocation.value("--template"),
        policy: invocation.value("--quant-policy"),
        imatrix: invocation.value("--imatrix-manifest"),
        out: invocation.value("--out"),
        plan: if planning { Some(destination) } else { None },
        tensor: invocation.value("--tensor"),
        qtype: invocation.value("--target-qtype"),
        limit,
        overwrite: invocation.has("--overwrite"),
        allow_unsupported: invocation.has("--allow-unsupported-qtype"),
        require_all: invocation.has("--require-all"),
    };
    let report = ffi::artifact_convert(request)?;
    let mut facts = serde_json::to_value(&report)?
        .as_object()
        .unwrap()
        .iter()
        .map(|(key, value)| (key.to_string(), value.to_string()))
        .collect::<Vec<_>>();
    facts.extend([
        (
            "architecture".into(),
            required(invocation, "--arch")?.into(),
        ),
        ("out".into(), destination.into()),
        (
            "status".into(),
            if planning {
                "conversion-plan-written"
            } else {
                "conversion-gguf-written"
            }
            .into(),
        ),
    ]);
    if !planning {
        facts.extend([
            (
                "source_tensor".into(),
                required(invocation, "--tensor")?.into(),
            ),
            (
                "target_qtype".into(),
                required(invocation, "--target-qtype")?.into(),
            ),
        ]);
    }
    let pairs = facts
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output::standard(
        presentation::record(
            if planning {
                "CONVERSION  plan"
            } else {
                "CONVERSION  emitted"
            },
            &pairs,
            width,
            styled,
        )?,
        0,
    ))
}

fn tensor_map(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let architecture = required(invocation, "--arch")?;
    let source = required(invocation, "--native-source")?;
    let limit = invocation
        .value("--limit")
        .unwrap_or("20")
        .parse::<u64>()
        .map_err(|_| Refusal {
            reason: "tensor mapping limit must be a non-negative u64".into(),
            hint: None,
        })?;
    let rows = ffi::tensor_mapping(ffi::MappingRequest {
        architecture,
        source,
        template: invocation.value("--template"),
        require_native: invocation.has("--require-all-native-mapped"),
        require_template: invocation.has("--require-all-template-matched"),
    })?;
    let mapped = rows.iter().filter(|row| row.mapped).count();
    let mismatch = rows.iter().filter(|row| row.shape_mismatch).count();
    let summary = serde_json::json!({
        "schema": "yvex.tensor_map.v1", "architecture": architecture, "native_source": source,
        "native_tensors": rows.len(), "mapped": mapped, "unmapped": rows.len() - mapped - mismatch,
        "shape_mismatch": mismatch,
    });
    // This existing machine contract describes the complete mapping, not human row selection.
    if invocation.has("--json") {
        return Ok(Output::standard(
            format!("{}\n", serde_json::to_string(&summary)?),
            0,
        ));
    }
    let mut facts: Vec<(String, String)> = vec![
        ("native_tensors".into(), rows.len().to_string()),
        ("mapped".into(), mapped.to_string()),
        (
            "unmapped".into(),
            (rows.len() - mapped - mismatch).to_string(),
        ),
        ("shape_mismatch".into(), mismatch.to_string()),
    ];
    if let Some(template) = invocation.value("--template") {
        facts.push(("template".into(), template.into()));
    }
    let pairs = facts
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    let mut output = presentation::record(
        &format!("TENSOR MAP  {architecture}"),
        &pairs,
        width,
        styled,
    )?;
    let selected = if let Some(name) = invocation.value("--tensor") {
        vec![
            rows.iter()
                .find(|row| row.native == name)
                .ok_or_else(|| ffi::Error {
                    code: ffi::raw::yvex_status_YVEX_ERR_FORMAT,
                    owner: "weight.mapping".into(),
                    message: format!("native tensor not found: {name}"),
                })?,
        ]
    } else {
        rows.iter()
            .take(usize::try_from(limit).unwrap_or(usize::MAX))
            .collect::<Vec<_>>()
    };
    for row in selected {
        let native_shape = serde_json::to_string(&row.native_dims)?;
        let target_shape = if row.target_dims.is_empty() {
            "unknown".into()
        } else {
            serde_json::to_string(&row.target_dims)?
        };
        output.push_str(&presentation::record(
            &row.native,
            &[
                ("role", &row.role),
                ("target", &row.target),
                ("mapping", &row.status),
                ("native_shape", &native_shape),
                ("target_shape", &target_shape),
                (
                    "transform",
                    if row.transpose { "transpose" } else { "none" },
                ),
                ("issue", &row.issue),
            ],
            width,
            styled,
        )?);
    }
    output.push_str("status: tensor-map\n");
    Ok(Output::standard(output, 0))
}

fn document_record(
    title: &str,
    summary: impl serde::Serialize,
    extra: Vec<(String, String)>,
    width: usize,
    styled: bool,
) -> Result<String> {
    let summary = serde_json::to_value(summary)?;
    let mut fields = summary
        .as_object()
        .expect("typed document projection")
        .iter()
        .map(|(key, value)| {
            let value = match value {
                serde_json::Value::String(value) => value.clone(),
                serde_json::Value::Bool(value) => boolean(*value).into(),
                _ => value.to_string(),
            };
            (
                if key == "status" {
                    "document_status".into()
                } else {
                    key.clone()
                },
                value,
            )
        })
        .collect::<Vec<_>>();
    fields.extend(extra);
    let pairs = fields
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record(title, &pairs, width, styled)?)
}

fn policy(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let action = invocation.positionals[0].as_str();
    let input = match action {
        "derive" => ffi::PolicyInput::Template {
            path: required(invocation, "--template")?,
            architecture: required(invocation, "--arch")?,
            out: required(invocation, "--out")?,
        },
        "inspect" | "validate" => ffi::PolicyInput::File {
            path: required(invocation, "--policy")?,
            validate: action == "validate",
            template: invocation.value("--template"),
        },
        _ => unreachable!("registry-owned policy action"),
    };
    let facts = ffi::policy(input)?;
    let status = if action == "derive" {
        "quant-policy-written"
    } else {
        facts.status.as_str()
    };
    let mut output = document_record(
        &format!("QUANT POLICY  {action}"),
        &facts,
        vec![("status".into(), status.into())],
        width,
        styled,
    )?;
    if action == "inspect" {
        let rows = facts
            .rows
            .iter()
            .map(|row| {
                vec![
                    format!("{}:{}", row.selector_kind, row.selector),
                    row.qtype.clone(),
                    boolean(row.requires_imatrix).into(),
                    boolean(row.storage_supported).into(),
                    boolean(row.compute_supported).into(),
                ]
            })
            .collect::<Vec<_>>();
        output.push_str(&presentation::table(
            &[
                ("SELECTOR", Alignment::Left),
                ("QTYPE", Alignment::Left),
                ("IMATRIX", Alignment::Left),
                ("STORAGE", Alignment::Left),
                ("COMPUTE", Alignment::Left),
            ],
            &rows,
            width,
            styled,
        )?);
    }
    Ok(Output::standard(output, 0))
}

fn preset(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let action = invocation
        .positionals
        .first()
        .map(String::as_str)
        .unwrap_or("list");
    let name = invocation.positionals.get(1).map(String::as_str);
    if action == "list" {
        if name.is_some() || invocation.has("--out") {
            return Err(Box::new(Refusal {
                reason: "preset list takes no name or destination".into(),
                hint: None,
            }));
        }
        return Ok(Output::standard(
            presentation::lines(&ffi::policy_presets()?, width, styled)?,
            0,
        ));
    }
    let name = name.ok_or_else(|| Refusal {
        reason: "preset show/export requires a name".into(),
        hint: None,
    })?;
    let out = if action == "export" {
        Some(required(invocation, "--out")?)
    } else {
        None
    };
    let facts = ffi::policy(ffi::PolicyInput::Preset { name, out })?;
    Ok(Output::standard(
        document_record(
            &format!("QUANT PRESET  {name}"),
            &facts,
            out.map(|out| vec![("out".into(), out.into())])
                .unwrap_or_default(),
            width,
            styled,
        )?,
        0,
    ))
}

fn imatrix(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let action = invocation.positionals[0].as_str();
    let input = if action == "create" {
        ffi::ImatrixInput::Create(ffi::ImatrixOptions {
            name: required(invocation, "--name")?,
            architecture: required(invocation, "--arch")?,
            source_manifest: invocation.value("--source-manifest"),
            policy: invocation.value("--quant-policy"),
            path: required(invocation, "--imatrix")?,
            dataset: invocation.value("--dataset"),
            command: invocation.value("--command"),
            producer: invocation.value("--producer"),
            format: required(invocation, "--format")?,
            status: required(invocation, "--status")?,
            out: required(invocation, "--out")?,
        })
    } else {
        ffi::ImatrixInput::File {
            path: required(invocation, "--manifest")?,
            validate: action == "validate",
        }
    };
    let facts = ffi::imatrix(input)?;
    let status = match action {
        "create" => "imatrix-manifest-written",
        "inspect" => "imatrix-manifest",
        "validate" => {
            if facts.issues == 0 {
                "imatrix-valid"
            } else if facts.file_exists {
                "imatrix-partial"
            } else {
                "imatrix-invalid"
            }
        }
        _ => unreachable!("registry-owned calibration action"),
    };
    Ok(Output::standard(
        document_record(
            &format!("IMATRIX  {action}"),
            &facts,
            vec![("status".into(), status.into())],
            width,
            styled,
        )?,
        0,
    ))
}

fn job(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let action = invocation.positionals[0].as_str();
    let input = if action == "create" {
        ffi::JobInput::Create(Box::new(ffi::JobOptions {
            name: required(invocation, "--name")?,
            architecture: required(invocation, "--arch")?,
            tool: invocation.value("--tool").unwrap_or("unknown"),
            tool_path: required(invocation, "--tool-path")?,
            source_manifest: invocation.value("--source-manifest"),
            source: required(invocation, "--native-source")?,
            template: required(invocation, "--template")?,
            policy: invocation.value("--quant-policy"),
            imatrix_manifest: invocation.value("--imatrix-manifest"),
            imatrix: invocation.value("--imatrix"),
            output: required(invocation, "--out-gguf")?,
            log: required(invocation, "--log")?,
            command: required(invocation, "--command")?,
            status: required(invocation, "--status")?,
            out: required(invocation, "--out")?,
        }))
    } else {
        ffi::JobInput::File(required(invocation, "--manifest")?)
    };
    let facts = ffi::quant_job(input)?;
    let status = match action {
        "create" => "quant-job-written",
        "inspect" => "quant-job-manifest",
        "validate" => "quant-job-valid",
        _ => unreachable!("registry-owned quantization job action"),
    };
    Ok(Output::standard(
        document_record(
            &format!("QUANT JOB  {action}"),
            &facts,
            vec![("status".into(), status.into())],
            width,
            styled,
        )?,
        0,
    ))
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    match invocation.operation.operation_id.as_str() {
        "quant.qtype.support" => qtypes(width, styled),
        "quant.convert" => conversion(invocation, width, styled),
        "tensor.map" => tensor_map(invocation, width, styled),
        "quant.policy" => policy(invocation, width, styled),
        "quant.preset" => preset(invocation, width, styled).or_else(|error| {
            // This established command's native failures exit 1. Preserve that
            // shell contract without replacing the actual typed native error.
            if error.is::<ffi::Error>() {
                let mut output = crate::refused_error(error.as_ref(), width, styled);
                output.exit = 1;
                Ok(output)
            } else {
                Err(error)
            }
        }),
        "quant.imatrix" => imatrix(invocation, width, styled),
        "quant.job" => job(invocation, width, styled),
        _ => unreachable!("registry dispatch owns the quantized command selection"),
    }
}
