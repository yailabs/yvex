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
            ("cpu_runtime_executable".into(), u8::from(cpu).to_string()),
            ("cuda_runtime_executable".into(), u8::from(cuda).to_string()),
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
        ("official_reader_admission".into(), "pending".into()),
    ];
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
    let output = match operation {
        "quant.plan" | "quant.summarize" => {
            render("PHYSICAL VARIANT  plan", &summary(&report), width, styled)?
        }
        "quant.explain" => explain(&report, invocation, width, styled)?,
        "quant.emit" => render(
            "PHYSICAL VARIANT  emitted",
            &emission(
                &report,
                &context.emit(plan, destination.unwrap())?,
                destination.unwrap(),
            ),
            width,
            styled,
        )?,
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
        if error.is::<ffi::Error>() {
            let mut output = crate::refused_error(error.as_ref(), width, styled);
            output.exit = 1; // Established engineering-command failure contract.
            Ok(output)
        } else {
            Err(error)
        }
    })
}
