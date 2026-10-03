// Read-only artifact presentations consume admitted C descriptors, not model names.
use crate::{
    Output,
    ffi::{self, MetadataValue, ModelView, Reference, raw},
    presentation,
    registry::{Invocation, Refusal},
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

type DiagnosticFields = Vec<(String, String)>;

const PROOF_ROLES: [&str; 12] = [
    "token_embedding",
    "attention_norm",
    "ffn_norm",
    "attention_q",
    "attention_k",
    "attention_v",
    "attention_out",
    "ffn_gate",
    "ffn_up",
    "ffn_down",
    "output_norm",
    "output_head",
];

fn inventory_json(row: &ffi::catalog::ArtifactDiscovery, status: bool) -> serde_json::Value {
    let path = if row.path.is_empty() {
        &row.expected_path
    } else {
        &row.path
    };
    if status {
        serde_json::json!({"status": "artifacts-status", "target_id": row.target_id,
            "family": row.family, "source_status": row.source_status,
            "artifact_status": row.artifact_status, "expected_artifact_path":
                if row.expected_path.is_empty() { path } else { &row.expected_path },
            "prepare_status": row.prepare_status, "top_blocker": row.top_blocker})
    } else {
        serde_json::json!({"target_id": row.target_id, "family": row.family,
            "artifact_class": row.artifact_class, "artifact_status": row.artifact_status,
            "prepare_status": row.prepare_status, "top_blocker": row.top_blocker, "path": path})
    }
}

fn inventory_record(
    row: &ffi::catalog::ArtifactDiscovery,
    audit: bool,
    width: usize,
    styled: bool,
) -> Result<String> {
    let mut fields = vec![
        ("family", row.family.clone()),
        ("class", row.artifact_class.clone()),
        ("artifact_status", row.artifact_status.clone()),
        ("prepare", row.prepare_status.clone()),
        (
            "size",
            crate::catalog::size((row.size_bytes != 0).then_some(row.size_bytes)),
        ),
    ];
    if row.top_blocker != "none" {
        fields.extend([
            ("top_blocker", row.top_blocker.clone()),
            ("next", row.next.clone()),
        ]);
    }
    if audit {
        fields.extend([
            ("source", row.source_status.clone()),
            ("path", row.path.clone()),
            ("expected_artifact_path", row.expected_path.clone()),
            ("registry_path", row.registry_path.clone()),
            ("download_report_path", row.download_report_path.clone()),
            ("source_manifest_path", row.source_manifest_path.clone()),
            ("native_inventory_path", row.native_inventory_path.clone()),
            ("tensor_map_path", row.tensor_map_path.clone()),
            ("output_head_map_path", row.output_head_map_path.clone()),
            ("tokenizer_map_path", row.tokenizer_map_path.clone()),
            ("detail", row.detail.clone()),
            ("tensor_map_status", row.tensor_map_status.clone()),
            ("output_head_map_status", row.output_head_map_status.clone()),
            ("tokenizer_map_status", row.tokenizer_map_status.clone()),
        ]);
    }
    let borrowed = fields
        .iter()
        .map(|(key, value)| (*key, value.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record(
        &format!("ARTIFACT  {}", row.target_id),
        &borrowed,
        width,
        styled,
    )?)
}

pub(crate) fn inventory(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let mode = invocation.value("--output").unwrap_or("normal");
    if !["normal", "table", "audit", "json"].contains(&mode) {
        return Err(grammar(
            "artifact inventory output requires normal|table|audit|json",
        ));
    }
    let family = invocation.value("--family");
    if family.is_some_and(|family| !["deepseek", "qwen", "gemma", "glm"].contains(&family)) {
        return Err(grammar(
            "artifact inventory family requires deepseek|qwen|gemma|glm",
        ));
    }
    let rows = ffi::catalog::artifact_inventory(invocation.value("--models-root"), family)?;
    let status = invocation.operation.operation_id == "artifact.registry.status";
    let selected = if status {
        let target = invocation
            .positionals
            .first()
            .ok_or_else(|| grammar("artifact status requires NAME"))?;
        let matches = rows
            .iter()
            .filter(|row| row.target_id == *target)
            .collect::<Vec<_>>();
        match matches.as_slice() {
            [] => {
                return Err(grammar(
                    "artifact target is not present in this discovery catalog",
                ));
            }
            [row] => vec![*row],
            _ => {
                return Err(grammar(
                    "artifact target is ambiguous; select an explicit --family",
                ));
            }
        }
    } else {
        rows.iter().collect()
    };
    if invocation.has("--json") || mode == "json" {
        let value = if status {
            inventory_json(selected[0], true)
        } else {
            serde_json::json!({"status": "artifacts-list", "artifacts":
                selected.iter().map(|row| inventory_json(row, false)).collect::<Vec<_>>()})
        };
        return Ok(Output::standard(format!("{value}\n"), 0));
    }
    let mut text = if mode == "table" && !status {
        let cells = selected
            .iter()
            .map(|row| {
                vec![
                    row.target_id.clone(),
                    row.family.clone(),
                    row.artifact_status.clone(),
                    row.top_blocker.clone(),
                ]
            })
            .collect::<Vec<_>>();
        presentation::table(
            &[
                ("ARTIFACT", replai::Alignment::Left),
                ("FAMILY", replai::Alignment::Left),
                ("STATE", replai::Alignment::Left),
                ("BLOCKER", replai::Alignment::Left),
            ],
            &cells,
            width,
            styled,
        )?
    } else {
        selected
            .iter()
            .map(|row| {
                inventory_record(
                    row,
                    mode == "audit" || invocation.has("--audit") || status,
                    width,
                    styled,
                )
            })
            .collect::<Result<Vec<_>>>()?
            .join("\n")
    };
    if selected.is_empty() {
        text.push_str("No artifact discovery records.\n");
    }
    text.push_str(
        "\nDiscovery only; bytes, digests, admission and execution are not qualified here.\n",
    );
    Ok(Output::standard(text, 0))
}

fn diagnostic_role(name: &str) -> String {
    match name {
        "post-attention-norm" => "ffn_norm",
        "final-norm" => "output_norm",
        "attention-q-projection" | "q_projection" => "attention_q",
        "attention-k-projection" | "k_projection" => "attention_k",
        "attention-v-projection" | "v_projection" => "attention_v",
        "attention-output-projection" | "o_projection" => "attention_out",
        "mlp-gate" => "ffn_gate",
        "mlp-up" => "ffn_up",
        "mlp-down" => "ffn_down",
        _ => return name.replace('-', "_"),
    }
    .into()
}

fn diagnostic_collection(name: &str, tensors: &[ffi::TensorInfo], tokenizer: bool) -> bool {
    let present = |role: &str| tensors.iter().any(|tensor| tensor.role == role);
    match name {
        "embedding" => present("token_embedding"),
        "normalization" => ["output_norm", "attention_norm", "ffn_norm"]
            .iter()
            .any(|role| present(role)),
        "attention" => ["attention_q", "attention_k", "attention_v", "attention_out"]
            .iter()
            .all(|role| present(role)),
        "mlp" => ["ffn_gate", "ffn_up", "ffn_down"]
            .iter()
            .all(|role| present(role)),
        "moe" => tensors.iter().any(|tensor| tensor.role.starts_with("moe_")),
        "output" => present("output_head"),
        "tokenizer" | "tokenizer-runtime-input" => tokenizer,
        _ => false,
    }
}

struct ProofInventory {
    indices: Vec<usize>,
    bytes: u64,
    missing: Vec<String>,
}

fn proof_inventory(invocation: &Invocation<'_>, view: &ModelView) -> Result<ProofInventory> {
    let tensors = view.tensors()?;
    let tokenizer = view
        .metadata()?
        .iter()
        .any(|entry| entry.key == "tokenizer.ggml.tokens");
    let mut missing = PROOF_ROLES
        .iter()
        .filter(|role| !tensors.iter().any(|tensor| tensor.role == **role))
        .map(|role| role.to_string())
        .collect::<Vec<_>>();
    if !tokenizer {
        missing.push("tokenizer-metadata".into());
    }
    if let Some(role) = invocation.value("--require-role") {
        let canonical = diagnostic_role(role);
        let present = if canonical == "tokenizer_metadata" {
            tokenizer
        } else {
            tensors.iter().any(|tensor| tensor.role == canonical)
        };
        if !present {
            missing.push(role.into());
        }
    }
    if let Some(collection) = invocation.value("--require-collection")
        && !diagnostic_collection(collection, &tensors, tokenizer)
    {
        missing.push(format!("collection:{collection}"));
    }
    let indices = tensors
        .iter()
        .enumerate()
        .filter(|(_, tensor)| {
            tensor.role_id != raw::yvex_tensor_role_YVEX_TENSOR_ROLE_UNKNOWN
                && PROOF_ROLES.contains(&tensor.role.as_str())
        })
        .map(|(index, _)| index)
        .collect::<Vec<_>>();
    let bytes = indices
        .iter()
        .try_fold(0_u64, |bytes, index| {
            bytes.checked_add(tensors[*index].storage_bytes)
        })
        .ok_or_else(|| grammar("allocation proof byte count overflows"))?;
    Ok(ProofInventory {
        indices,
        bytes,
        missing,
    })
}

fn proof_fault(
    invocation: &Invocation<'_>,
    fields: &mut DiagnosticFields,
    phases: &[&str],
) -> bool {
    if let Some(phase) = invocation.value("--fail-after-phase")
        && phases.contains(&phase)
    {
        fields.extend([
            ("failed_phase".into(), phase.into()),
            ("failed_reason".into(), "injected-failure".into()),
        ]);
        true
    } else {
        false
    }
}

fn diagnostic_proof(
    invocation: &Invocation<'_>,
    view: &ModelView,
) -> Result<(DiagnosticFields, u8)> {
    let inventory = proof_inventory(invocation, view)?;
    let budget = positive(
        invocation.value("--limit-bytes").unwrap_or("67108864"),
        u64::MAX,
    )?;
    let mut fields = vec![
        ("status".into(), "fullmodel-materialize-refused".into()),
        (
            "materialization_mode".into(),
            "bounded-allocation-proof; no weight transfer".into(),
        ),
        (
            "required_tensor_count".into(),
            inventory.indices.len().to_string(),
        ),
        ("required_tensor_bytes".into(), inventory.bytes.to_string()),
        (
            "missing_required_roles".into(),
            if inventory.missing.is_empty() {
                "none".into()
            } else {
                inventory.missing.join(",")
            },
        ),
        ("allocation_attempted".into(), "false".into()),
        ("payload_transferred".into(), "false".into()),
        ("execution_ready".into(), "false".into()),
        ("cleanup_status".into(), "not-needed".into()),
    ];
    if proof_fault(
        invocation,
        &mut fields,
        &[
            "preflight",
            "resolve-model",
            "artifact-identity",
            "tensor-inventory",
            "role-coverage",
        ],
    ) {
        return Ok((fields, 1));
    }
    if !inventory.missing.is_empty() {
        return Ok((fields, 0));
    }
    if proof_fault(invocation, &mut fields, &["placement-plan"]) {
        return Ok((fields, 1));
    }
    if inventory.bytes > budget {
        fields.extend([
            ("failed_phase".into(), "memory-budget".into()),
            ("failed_reason".into(), "byte-limit".into()),
        ]);
        return Ok((fields, 1));
    }
    if proof_fault(invocation, &mut fields, &["memory-budget"]) {
        return Ok((fields, 1));
    }
    if invocation.has("--dry-run") || invocation.has("--plan-only") {
        fields[0].1 = if invocation.has("--plan-only") {
            "fullmodel-materialize-plan-only"
        } else {
            "fullmodel-materialize-dry-run"
        }
        .into();
        return Ok((fields, 0));
    }
    if proof_fault(invocation, &mut fields, &["backend-preflight"]) {
        return Ok((fields, 1));
    }
    let proof = view.allocation_proof(
        invocation.value("--backend").unwrap_or("cpu"),
        &inventory.indices,
    )?;
    let exit = proof
        .failure
        .as_ref()
        .map_or(0, |failure| crate::native_exit(failure.code));
    fields[0].1 = if exit == 0 {
        "fullmodel-materialize-pass"
    } else {
        "fullmodel-materialize-fail"
    }
    .into();
    fields
        .iter_mut()
        .find(|(name, _)| name == "allocation_attempted")
        .expect("proof allocation field")
        .1 = "true".into();
    fields
        .iter_mut()
        .find(|(name, _)| name == "cleanup_status")
        .expect("proof cleanup field")
        .1 = if proof.cleanup { "pass" } else { "fail" }.into();
    fields.extend([
        (
            "materialized_tensor_count".into(),
            proof.tensors.to_string(),
        ),
        ("materialized_tensor_bytes".into(), proof.bytes.to_string()),
    ]);
    if let Some(failure) = proof.failure {
        fields.push(("reason".into(), failure.to_string()));
    }
    if proof_fault(
        invocation,
        &mut fields,
        &[
            "materialize-embedding",
            "materialize-normalization",
            "materialize-attention",
            "materialize-mlp",
            "materialize-moe",
            "materialize-output",
            "materialize-tokenizer",
            "cleanup",
        ],
    ) {
        fields[0].1 = "fullmodel-materialize-fail".into();
        return Ok((fields, 1));
    }
    Ok((fields, exit))
}

fn diagnostic_mode(invocation: &Invocation<'_>) -> Result<bool> {
    if invocation
        .value("--output")
        .is_some_and(|mode| !["normal", "table", "audit"].contains(&mode))
    {
        return Err(grammar(
            "unsupported output mode; expected normal, table or audit",
        ));
    }
    Ok(invocation.has("--audit") || invocation.value("--output") == Some("audit"))
}

fn diagnostic_context(
    invocation: &Invocation<'_>,
    info: &ffi::DescriptorInfo,
) -> Result<DiagnosticFields> {
    let requested = invocation
        .value("--context-length")
        .map(|value| positive(value, u64::MAX))
        .transpose()?;
    let chunk = invocation
        .value("--chunk-size")
        .map(|value| positive(value, u64::MAX))
        .transpose()?;
    let count = invocation
        .value("--tokens")
        .map(ffi::explicit_token_count)
        .transpose()?
        .unwrap_or(0);
    let active = requested.or((info.context_length > 0).then_some(info.context_length));
    let chunks = chunk.map_or(0, |chunk| count / chunk + u64::from(count % chunk != 0));
    let last = chunk.map_or(0, |chunk| {
        if count == 0 {
            0
        } else {
            (count - 1) % chunk + 1
        }
    });
    Ok(vec![
        ("status".into(), "context-report".into()),
        (
            "model_max_context".into(),
            if info.context_length == 0 {
                "unknown".into()
            } else {
                info.context_length.to_string()
            },
        ),
        (
            "requested_context".into(),
            requested.map_or("not-requested".into(), |v| v.to_string()),
        ),
        (
            "active_context".into(),
            active.map_or("unknown".into(), |v| v.to_string()),
        ),
        (
            "active_context_source".into(),
            if requested.is_some() {
                "operator-request"
            } else {
                "model-metadata"
            }
            .into(),
        ),
        ("token_count".into(), count.to_string()),
        ("prompt_token_count".into(), count.to_string()),
        ("prefill_token_count".into(), count.to_string()),
        ("generated_token_count".into(), "0".into()),
        (
            "chunk_size".into(),
            chunk.map_or("not-requested".into(), |v| v.to_string()),
        ),
        ("chunk_count".into(), chunks.to_string()),
        ("last_chunk_size".into(), last.to_string()),
        ("decode_start_position".into(), count.to_string()),
        (
            "context_overflow".into(),
            active.map_or("unknown".into(), |v| (count > v).to_string()),
        ),
        ("overflow_mutates_state".into(), "false".into()),
        ("prefill_context_ready".into(), "false".into()),
        ("decode_context_ready".into(), "false".into()),
        ("kv_context_ready".into(), "false".into()),
        (
            "boundary".into(),
            "report-only; no hosted state, context extension, prefill or decode executed".into(),
        ),
    ])
}

fn diagnostic_moe(invocation: &Invocation<'_>, view: &ModelView) -> Result<DiagnosticFields> {
    if invocation
        .value("--collection")
        .is_some_and(|collection| collection != "moe")
    {
        return Err(grammar(
            "tensor collection report currently accepts only --collection moe",
        ));
    }
    let roles = view.role_counts();
    let moe = roles.iter().filter(|(role, _)| role.starts_with("moe_"));
    let mut fields = vec![
        ("status".into(), "moe-inventory-report".into()),
        (
            "evidence_basis".into(),
            "admitted artifact tensor roles and metadata; no routing or execution".into(),
        ),
    ];
    fields.extend(moe.map(|(name, count)| (format!("{name}_tensor_count"), count.to_string())));
    // Source-authored metadata is displayed under its exact key. Tensor counts
    // do not become expert populations, routing policy or runtime support.
    for entry in view.metadata()? {
        if entry.key.contains("expert")
            && let MetadataValue::Unsigned(value) = entry.value
        {
            fields.push((entry.key, value.to_string()));
        }
    }
    Ok(fields)
}

fn diagnostic_model(invocation: &Invocation<'_>, view: &ModelView) -> Result<DiagnosticFields> {
    let operation = invocation.operation.operation_id.as_str();
    let status = match operation {
        "inspect.model.full.descriptor" => "fullmodel-descriptor",
        "inspect.model.full.family_runtime" => "fullmodel-family-runtime",
        "inspect.model.full.materialization_plan" => "fullmodel-materialization-plan",
        _ => "fullmodel-report",
    };
    let mut fields = vec![
        ("status".into(), status.into()),
        ("tensor_inventory_status".into(), "pass".into()),
        ("full_runtime_model".into(), "false".into()),
        ("generation_ready".into(), "false".into()),
        (
            "runtime_qualification".into(),
            "not-established-by-inventory".into(),
        ),
        ("placement_plan".into(), "report-only-no-allocation".into()),
        (
            "boundary".into(),
            "artifact inventory/descriptor only; not a hosted runtime readiness verdict".into(),
        ),
    ];
    fields.extend(
        view.role_counts()
            .into_iter()
            .filter(|(_, count)| *count > 0)
            .map(|(role, count)| (format!("role.{role}"), count.to_string())),
    );
    let limit = positive(invocation.value("--limit-tensors").unwrap_or("5"), u64::MAX)?;
    let mut tensors = view.tensors()?;
    let tokenizer = view
        .metadata()?
        .iter()
        .any(|entry| entry.key == "tokenizer.ggml.tokens");
    if let Some(role) = invocation.value("--require-role") {
        let canonical = diagnostic_role(role);
        fields.push(("required_role".into(), role.into()));
        fields.push((
            "required_role_present".into(),
            if canonical == "tokenizer_metadata" {
                tokenizer
            } else {
                tensors.iter().any(|tensor| tensor.role == canonical)
            }
            .to_string(),
        ));
    }
    if let Some(collection) = invocation.value("--require-collection") {
        fields.push(("required_collection".into(), collection.into()));
        fields.push((
            "required_collection_present".into(),
            diagnostic_collection(collection, &tensors, tokenizer).to_string(),
        ));
    }
    tensors.sort_by(|a, b| {
        b.storage_bytes
            .cmp(&a.storage_bytes)
            .then_with(|| a.name.cmp(&b.name))
    });
    for (index, tensor) in tensors
        .into_iter()
        .take(limit.min(usize::MAX as u64) as usize)
        .enumerate()
    {
        fields.push((
            format!("largest_tensor_{index}"),
            format!(
                "{} · {} · {} · {} B",
                tensor.name, tensor.role, tensor.dtype, tensor.storage_bytes
            ),
        ));
    }
    Ok(fields)
}

fn diagnostic_flags(invocation: &Invocation<'_>) -> Result<()> {
    let operation = invocation.operation.operation_id.as_str();
    if operation.starts_with("inspect.model.full.") {
        if invocation.has("--family") && operation != "inspect.model.full.family_runtime" {
            return Err(grammar(
                "--family requires inspect model full family-runtime",
            ));
        }
        for flag in [
            "--fail-after-phase",
            "--report-dir",
            "--dry-run",
            "--plan-only",
            "--limit-bytes",
        ] {
            if invocation.has(flag) {
                return Err(grammar(&format!(
                    "{flag} is only valid with artifact materialize model"
                )));
            }
        }
        if operation != "inspect.model.full.descriptor" {
            for flag in ["--require-role", "--require-collection", "--format"] {
                if invocation.has(flag) {
                    return Err(grammar(&format!(
                        "{flag} requires descriptor or materialize"
                    )));
                }
            }
        }
        if invocation
            .value("--format")
            .is_some_and(|value| value != "text")
        {
            return Err(grammar(
                "full-model descriptor currently supports --format text only",
            ));
        }
    }
    Ok(())
}

pub(crate) fn diagnostic(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let audit = diagnostic_mode(invocation)?;
    diagnostic_flags(invocation)?;
    let selector = required(invocation, "--model")?;
    if (invocation
        .operation
        .operation_id
        .starts_with("inspect.model.full.")
        || invocation.operation.operation_id == "execute.materialization.full_model")
        && let Some(target) =
            ffi::catalog::target(invocation.value("--target").unwrap_or(selector))?
        && target.target_class == "official-source-huge-model"
    {
        return source_only_diagnostic(invocation, &target, width, styled);
    }
    let reference = Reference::resolve_in(selector, invocation.value("--registry"))?;
    if reference.is_alias() {
        let (integrity, _) = reference.integrity(None, false, 0)?;
        let verification = reference.integrity_verification(&integrity)?;
        if !verification.passed {
            return Err(Box::new(ffi::Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "model.ref.diagnostic".into(),
                message: "registered artifact identity/metadata drift".into(),
            }));
        }
    }
    let view = ModelView::open(&reference.path()?)?;
    let info = view.descriptor()?;
    if invocation
        .value("--family")
        .is_some_and(|family| family != "auto" && family != info.architecture)
    {
        return Err(Box::new(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_UNSUPPORTED,
            owner: "model.diagnostic".into(),
            message: "requested family differs from admitted artifact architecture".into(),
        }));
    }
    let (mut fields, exit) = match invocation.operation.operation_id.as_str() {
        "execute.materialization.full_model" => diagnostic_proof(invocation, &view)?,
        "context.report" => (diagnostic_context(invocation, &info)?, 0),
        "evidence.moe" | "tensor.collection.report" => (diagnostic_moe(invocation, &view)?, 0),
        _ => (diagnostic_model(invocation, &view)?, 0),
    };
    if !audit {
        fields.retain(|(name, _)| {
            !name.starts_with("role.")
                && !name.starts_with("largest_tensor_")
                && !name.ends_with("_ready")
        });
    }
    fields.splice(
        0..0,
        [
            ("model".into(), selector.into()),
            ("architecture".into(), info.architecture),
            ("tensor_count".into(), info.header.tensor_count.to_string()),
            ("known_tensor_bytes".into(), info.known_bytes.to_string()),
        ],
    );
    let pairs = fields
        .iter()
        .map(|(name, value)| (name.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output::standard(
        presentation::record("MODEL  diagnostic", &pairs, width, styled)?,
        exit,
    ))
}

fn source_only_diagnostic(
    invocation: &Invocation<'_>,
    target: &ffi::catalog::Target,
    width: usize,
    styled: bool,
) -> Result<Output> {
    if invocation
        .value("--family")
        .is_some_and(|family| family != "auto" && !family.eq_ignore_ascii_case(&target.family))
    {
        return Err(grammar(
            "requested family differs from the canonical source target",
        ));
    }
    let summary = ffi::catalog::target_summary(&target.target_id)?;
    let fields = [
        ("model", required(invocation, "--model")?),
        ("target_id", target.target_id.as_str()),
        ("target_class", target.target_class.as_str()),
        ("family", target.family.as_str()),
        ("source_class", target.source_artifact_class.as_str()),
        ("source_status", summary.source_status.as_str()),
        ("artifact_class", target.target_artifact_class.as_str()),
        ("artifact_status", summary.artifact_status.as_str()),
        ("runtime_status", summary.runtime.as_str()),
        ("generation", target.generation.as_str()),
        ("allocation_attempted", "false"),
        ("payload_transferred", "false"),
        ("cleanup_status", "not-needed"),
        (
            "boundary",
            "source-target profile only; an admitted YVEX artifact is required for this operation",
        ),
    ];
    let report_only = invocation.operation.operation_id == "inspect.model.full.report";
    Ok(Output::standard(
        presentation::record("MODEL  source-only", &fields, width, styled)?,
        if report_only { 0 } else { 5 },
    ))
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

fn positive(value: &str, maximum: u64) -> Result<u64> {
    value
        .parse::<u64>()
        .ok()
        .filter(|value| *value > 0 && *value <= maximum)
        .ok_or_else(|| grammar("expected a positive integer within the declared extent"))
}

struct ExpectedTensor<'a> {
    name: &'a str,
    dtype: &'a str,
    dims: Vec<u64>,
    bytes: u64,
}

fn expected_tensor<'a>(invocation: &'a Invocation<'_>) -> Result<Option<ExpectedTensor<'a>>> {
    let names = [
        "--expect-tensor",
        "--expect-rank",
        "--expect-dims",
        "--expect-dtype",
        "--expect-bytes",
    ];
    if names.iter().all(|name| !invocation.has(name)) {
        return Ok(None);
    }
    for name in names {
        required(invocation, name)?;
    }
    let rank = positive(required(invocation, "--expect-rank")?, 4)? as usize;
    let dims = required(invocation, "--expect-dims")?
        .split(',')
        .map(|value| positive(value, u64::MAX))
        .collect::<Result<Vec<_>>>()?;
    if dims.len() != rank {
        return Err(grammar("expected dimensions must match the rank"));
    }
    Ok(Some(ExpectedTensor {
        name: required(invocation, "--expect-tensor")?,
        dtype: required(invocation, "--expect-dtype")?,
        dims,
        bytes: positive(required(invocation, "--expect-bytes")?, u64::MAX)?,
    }))
}

fn gate_value(name: &str, value: &serde_json::Value) -> String {
    if name == "expected_dims" {
        return value
            .as_array()
            .unwrap()
            .iter()
            .map(|value| value.to_string())
            .collect::<Vec<_>>()
            .join(",");
    }
    if name == "cleanup_verified" {
        return if value.as_bool().unwrap() {
            "yes"
        } else {
            "no"
        }
        .into();
    }
    value
        .as_str()
        .map(str::to_owned)
        .unwrap_or_else(|| value.to_string())
}

pub(crate) fn gate(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let materialization = invocation.operation.operation_id == "artifact.materialize.gate";
    let selector = required(invocation, "--model")?;
    let label = required(invocation, "--label")?;
    let family = required(invocation, "--family")?;
    let scope = if materialization {
        match required(invocation, "--scope")? {
            "selected-tensor" => raw::yvex_materialize_scope_YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR,
            "partial-model" => raw::yvex_materialize_scope_YVEX_MATERIALIZE_SCOPE_PARTIAL_MODEL,
            "full-model" => raw::yvex_materialize_scope_YVEX_MATERIALIZE_SCOPE_FULL_MODEL,
            _ => return Err(grammar("unknown materialization scope")),
        }
    } else {
        raw::yvex_materialize_scope_YVEX_MATERIALIZE_SCOPE_UNKNOWN
    };
    let expected = expected_tensor(invocation)?;
    if !materialization && expected.is_none() {
        return Err(grammar(
            "model gate requires one complete --expect-* tensor specification",
        ));
    }
    let repeat = positive(invocation.value("--repeat").unwrap_or("1"), 1000)? as u32;
    let reference = Reference::resolve(selector)?;
    if reference.is_alias() {
        let (integrity, _) = reference.integrity(None, false, 0)?;
        let verification = reference.integrity_verification(&integrity)?;
        if !verification.passed {
            return Err(Box::new(ffi::Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "model.ref.gate".into(),
                message: format!(
                    "registered artifact refused: identity={} metadata={} readiness={}",
                    verification.identity, verification.metadata, verification.readiness
                ),
            }));
        }
    }
    let path = reference.path()?;
    let digest = reference.registered_digest()?;
    let metadata = if reference.is_alias() {
        "pass"
    } else {
        "unregistered"
    };
    let result = ffi::artifact_gate(ffi::GateRequest {
        path: &path,
        label,
        family,
        digest: invocation.value("--sha256").or(digest.as_deref()),
        metadata,
        scope,
        expected: expected.as_ref().map(|spec| ffi::GateExpected {
            name: spec.name,
            dtype: spec.dtype,
            dims: &spec.dims,
            bytes: spec.bytes,
        }),
        cpu: invocation.value("--backend") == Some("cpu") || invocation.has("--require-cpu"),
        cuda: invocation.value("--backend") == Some("cuda") || invocation.has("--require-cuda"),
        require_cpu: invocation.has("--require-cpu"),
        require_cuda: invocation.has("--require-cuda"),
        repeat,
        cleanup: invocation.has("--check-cleanup"),
        materialization,
    })?;
    let mut fields = result
        .fields
        .iter()
        .map(|(name, value)| (name.clone(), gate_value(name, value)))
        .collect::<Vec<_>>();
    let exit = result
        .failure
        .as_ref()
        .map_or(0, |failure| crate::native_exit(failure.code));
    fields.push((
        "reason".into(),
        result.failure.map_or_else(
            || "bounded artifact gate; not generation qualification".into(),
            |failure| failure.to_string(),
        ),
    ));
    let title = if materialization {
        "MATERIALIZATION GATE"
    } else {
        "MODEL GATE"
    };
    if let Some(destination) = invocation.value("--report-out") {
        let mut report = format!("{title}\n");
        for (name, value) in &fields {
            report.push_str(&format!("{name}: {}\n", presentation::escaped_text(value)));
        }
        ffi::publish(destination, report.as_bytes(), true)?;
    }
    let pairs = fields
        .iter()
        .map(|(name, value)| (name.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output::standard(
        presentation::record(title, &pairs, width, styled)?,
        exit,
    ))
}

pub(crate) fn materialize(
    invocation: &Invocation<'_>,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let selector = invocation
        .value("--model")
        .ok_or_else(|| grammar("--model is required"))?;
    let backend = invocation
        .value("--backend")
        .ok_or_else(|| grammar("--backend is required"))?;
    let reference = Reference::resolve(selector)?;
    let mut fields = vec![
        ("model".into(), selector.into()),
        ("backend".into(), backend.into()),
    ];
    let (integrity, code) = reference.integrity(None, false, 0)?;
    let verification = reference.integrity_verification(&integrity)?;
    fields.extend([
        (
            "integrity_status".into(),
            if integrity.passed != 0 {
                "pass"
            } else {
                "fail"
            }
            .into(),
        ),
        ("identity_status".into(), verification.identity),
        ("metadata_status".into(), verification.metadata),
        ("readiness_status".into(), verification.readiness),
    ]);
    let mut exit = if code != 0 {
        crate::native_exit(code)
    } else {
        0
    };
    if exit == 0 && reference.is_alias() && !verification.passed {
        exit = crate::native_exit(raw::yvex_status_YVEX_ERR_STATE);
    }
    if exit != 0 {
        fields.extend([
            ("materialization_gate".into(), "fail".into()),
            ("materialization_phase".into(), "preflight".into()),
            ("backend_status".into(), "not-opened".into()),
            ("allocation_attempted".into(), "false".into()),
            ("transfer_attempted".into(), "false".into()),
            ("cleanup_attempted".into(), "false".into()),
            ("execution_ready".into(), "false".into()),
            ("status".into(), "materialization-integrity-fail".into()),
        ]);
        if let Some(issue) = integrity.issues[..integrity.issue_count as usize].first() {
            fields.push(("reason".into(), ffi::text(&issue.code)));
        }
    } else {
        let view = ModelView::open(&reference.path()?)?;
        fields[0].1 = view.descriptor()?.name;
        match view.materialize(
            backend,
            invocation.has("--require-all"),
            invocation.has("--allow-unsupported-dtype"),
        ) {
            Ok(result) => {
                fields.extend(
                    serde_json::to_value(&result.facts)?
                        .as_object()
                        .unwrap()
                        .iter()
                        .map(|(name, value)| {
                            (
                                name.clone(),
                                value
                                    .as_str()
                                    .map(str::to_owned)
                                    .unwrap_or_else(|| value.to_string()),
                            )
                        }),
                );
                let status = if let Some(failure) = result.failure {
                    exit = crate::native_exit(failure.code);
                    fields.push(("reason".into(), failure.to_string()));
                    if result.facts.cleanup_attempted && result.facts.cleanup_status == "pass" {
                        "materialization-failed-cleaned"
                    } else {
                        "materialization-failed"
                    }
                } else if result.facts.materialization_status == "materialized" {
                    "weights-materialized"
                } else {
                    "weights-partial"
                };
                fields.push(("status".into(), status.into()));
            }
            Err(failure) => {
                exit = crate::native_exit(failure.code);
                fields.extend([
                    ("materialization_gate".into(), "fail".into()),
                    ("materialization_phase".into(), "preflight".into()),
                    ("backend_status".into(), "unavailable".into()),
                    ("allocation_attempted".into(), "false".into()),
                    ("transfer_attempted".into(), "false".into()),
                    ("execution_ready".into(), "false".into()),
                    ("status".into(), "weights-unsupported".into()),
                    ("reason".into(), failure.to_string()),
                ]);
            }
        }
    }
    fields.push((
        "boundary".into(),
        "weight materialization; no inference admission".into(),
    ));
    let pairs = fields
        .iter()
        .map(|(name, value)| (name.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output::standard(
        presentation::record("MATERIALIZATION", &pairs, width, styled)?,
        exit,
    ))
}

pub(crate) fn construct(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let (title, fields) = if invocation.operation.operation_id == "artifact.emit.controlled" {
        let result = ffi::artifact_emit(ffi::EmitRequest {
            out: invocation
                .value("--out")
                .ok_or_else(|| grammar("--out is required"))?,
            template: invocation.value("--template"),
            source: invocation.value("--native-source"),
            tensor: invocation.value("--tensor-name").unwrap_or("embed.weight"),
            target: invocation
                .value("--target-name")
                .unwrap_or("token_embd.weight"),
            qtype: invocation.value("--target-qtype"),
            name: invocation
                .value("--model-name")
                .unwrap_or("yvex-owned-gguf-test"),
            architecture: invocation.value("--arch").unwrap_or("llama"),
            overwrite: invocation.has("--overwrite"),
        })?;
        (
            "ARTIFACT EMIT  controlled".to_string(),
            vec![
                ("out".into(), result.out),
                (
                    "boundary".into(),
                    "controlled fixture; not source conversion".into(),
                ),
                ("architecture".into(), result.architecture),
                ("model_name".into(), result.name),
                ("status".into(), result.status),
                ("metadata_count".into(), result.metadata_count.to_string()),
                ("tensor_count".into(), result.tensor_count.to_string()),
                (
                    "tensor_payload_bytes".into(),
                    result.payload_bytes.to_string(),
                ),
                ("bytes_written".into(), result.bytes_written.to_string()),
                ("alignment".into(), result.alignment.to_string()),
                (
                    "roundtrip_validated".into(),
                    if result.roundtrip_validated {
                        "yes"
                    } else {
                        "no"
                    }
                    .into(),
                ),
            ],
        )
    } else {
        template_fields(invocation)?
    };
    let pairs = fields
        .iter()
        .map(|(name, value)| (name.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record(&title, &pairs, width, styled)?)
}

fn template_fields(invocation: &Invocation<'_>) -> Result<(String, Vec<(String, String)>)> {
    let action = &invocation.positionals[0];
    let path = invocation
        .value("--template")
        .ok_or_else(|| grammar("--template is required"))?;
    let source = invocation.value("--native-source");
    if action == "compare" && source.is_none() {
        return Err(grammar("template compare requires --native-source"));
    }
    let result = ffi::artifact_template(
        path,
        source,
        action == "compare",
        invocation.has("--require-all-template-tensors-in-native"),
    )?;
    let report_status = if action == "compare" {
        result.status.replacen("template-", "template-compare-", 1)
    } else {
        result.status
    };
    let mut fields = vec![
        ("template".into(), path.into()),
        ("status".into(), report_status),
    ];
    if action == "compare" {
        fields.extend([
            ("native_source".into(), source.unwrap().into()),
            ("template_tensors".into(), result.tensor_count.to_string()),
            (
                "native_tensors".into(),
                result.native_tensor_count.to_string(),
            ),
            ("matched_exact".into(), result.matched.to_string()),
            ("missing_in_native".into(), result.missing.to_string()),
            ("shape_mismatch".into(), result.mismatched.to_string()),
        ]);
    } else if action == "inspect" {
        fields.extend([
            ("architecture".into(), result.architecture),
            ("model_name".into(), result.name),
            ("metadata_count".into(), result.metadata_count.to_string()),
            ("tensor_count".into(), result.tensor_count.to_string()),
            (
                "has_tokenizer".into(),
                if result.has_tokenizer { "yes" } else { "no" }.into(),
            ),
            ("known_roles".into(), result.known_roles.to_string()),
            ("unknown_roles".into(), result.unknown_roles.to_string()),
        ]);
    }
    if action != "inspect" {
        fields.push(("issues".into(), result.issues.len().to_string()));
        for (index, issue) in result.issues.into_iter().enumerate() {
            fields.push((
                format!("issue_{index}"),
                format!("{} · {} · {}", issue.kind, issue.tensor, issue.message),
            ));
        }
    }
    Ok((format!("ARTIFACT TEMPLATE  {action}"), fields))
}

fn integrity_fields(report: &raw::yvex_artifact_integrity_report) -> Vec<(String, String)> {
    let mut fields = Vec::new();
    for (key, value) in [
        ("format", ffi::text(&report.format)),
        ("architecture", ffi::text(&report.architecture)),
        ("sha256", present(&report.sha256, "unavailable")),
        (
            "registered_sha256",
            present(&report.registered_sha256, "absent"),
        ),
        ("expected_sha256", ffi::text(&report.expected_sha256)),
        ("digest_status", ffi::text(&report.digest_status)),
        (
            "selected_embedding_shape",
            ffi::text(&report.selected_embedding_shape),
        ),
        (
            "identity_checked",
            (report.identity_checked != 0).to_string(),
        ),
        (
            "integrity_status",
            if report.passed != 0 { "pass" } else { "fail" }.into(),
        ),
    ] {
        fields.push((key.into(), value));
    }
    if report.expected_sha256[0] != 0 {
        fields.push(("actual_sha256".into(), ffi::text(&report.sha256)));
    }
    for (key, value) in [
        ("version", u64::from(report.version)),
        ("file_size", report.file_size),
        ("tensor_count", report.tensor_count),
        ("known_tensor_bytes", report.known_tensor_bytes),
        ("tensor_ranges_checked", report.tensor_ranges_checked),
        ("tensor_ranges_valid", report.tensor_ranges_valid),
        ("tensor_ranges_invalid", report.tensor_ranges_invalid),
        ("tensor_shapes_checked", report.tensor_shapes_checked),
        ("tensor_shapes_valid", report.tensor_shapes_valid),
        ("tensor_shapes_invalid", report.tensor_shapes_invalid),
        ("tensor_dtypes_checked", report.tensor_dtypes_checked),
        ("tensor_dtypes_valid", report.tensor_dtypes_valid),
        ("tensor_dtypes_invalid", report.tensor_dtypes_invalid),
        (
            "tensor_byte_counts_checked",
            report.tensor_byte_counts_checked,
        ),
        (
            "tensor_byte_counts_invalid",
            report.tensor_byte_counts_invalid,
        ),
        (
            "selected_embedding_hidden_size",
            report.selected_embedding_hidden_size,
        ),
        (
            "selected_embedding_vocab_size",
            report.selected_embedding_vocab_size,
        ),
        (
            "selected_embedding_output_count",
            report.selected_embedding_output_count,
        ),
        (
            "selected_embedding_output_bytes",
            report.selected_embedding_output_bytes,
        ),
        (
            "selected_embedding_slice_bytes",
            report.selected_embedding_slice_bytes,
        ),
        ("integrity_errors", u64::from(report.error_count)),
        ("integrity_warnings", u64::from(report.warning_count)),
    ] {
        fields.push((key.into(), value.to_string()));
    }
    for (index, issue) in report.issues[..report.issue_count as usize]
        .iter()
        .enumerate()
    {
        let severity =
            if issue.severity == raw::yvex_integrity_severity_YVEX_INTEGRITY_SEVERITY_WARNING {
                "warning"
            } else {
                "error"
            };
        let prefix = format!("{severity}_{index}");
        fields.push((format!("{prefix}_code"), ffi::text(&issue.code)));
        fields.push((format!("{prefix}_tensor"), ffi::text(&issue.tensor)));
        fields.push((format!("{prefix}_reason"), ffi::text(&issue.reason)));
        if issue.has_range != 0 {
            for (key, value) in [
                ("relative_offset", issue.relative_offset),
                ("absolute_offset", issue.absolute_offset),
                ("tensor_bytes", issue.tensor_bytes),
                ("file_size", issue.file_size),
            ] {
                fields.push((format!("{prefix}_{key}"), value.to_string()));
            }
        }
    }
    fields
}

fn metadata_fields(
    reference: &Reference,
    report: &raw::yvex_artifact_integrity_report,
    fields: &mut Vec<(String, String)>,
    passed: &mut bool,
) -> Result<(String, String, String)> {
    let result = reference.integrity_verification(report)?;
    *passed &= result.passed;
    let identity = result.identity;
    let metadata = result.metadata;
    let readiness = result.readiness;
    fields.push((
        "model_input_kind".into(),
        if reference.is_alias() {
            "alias"
        } else {
            "path"
        }
        .into(),
    ));
    fields.push(("selected_embedding_ready".into(), "false".into()));
    if reference.is_alias() {
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
    if let Some(current) = result.current {
        fields.extend(current.pairs);
        fields.push(("support_level".into(), current.support));
        fields.push(("selected_embedding_ready".into(), current.ready.to_string()));
        fields.push((
            "selected_embedding_hidden_size".into(),
            current.hidden.to_string(),
        ));
        fields.push((
            "selected_embedding_vocab_size".into(),
            current.vocab.to_string(),
        ));
        fields.push((
            "selected_embedding_output_count".into(),
            current.output.to_string(),
        ));
        fields.push((
            "selected_embedding_output_bytes".into(),
            current
                .output
                .checked_mul(4)
                .ok_or_else(|| grammar("selected output extent overflow"))?
                .to_string(),
        ));
        fields.push((
            "selected_embedding_slice_bytes".into(),
            current.slice.to_string(),
        ));
    }
    Ok((identity, metadata, readiness))
}

fn present(value: &[std::ffi::c_char], missing: &str) -> String {
    let value = ffi::text(value);
    if value.is_empty() {
        missing.into()
    } else {
        value
    }
}

pub(crate) fn verify(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let detailed = invocation.operation.operation_id == "artifact.verify.report";
    let backend = invocation.value("--backend");
    if backend.is_some_and(|name| !["cpu", "cuda"].contains(&name)) {
        return Err(grammar("backend must be cpu or cuda"));
    }
    if !detailed
        && (backend.is_some()
            || invocation.has("--audit")
            || invocation.value("--output").is_some())
    {
        return Err(grammar(
            "backend/output options require artifact verify integrity",
        ));
    }
    let mode = invocation.value("--output").unwrap_or("normal");
    if !["normal", "table", "audit"].contains(&mode) {
        return Err(grammar(&format!(
            "unsupported output mode: {mode}; expected normal, table or audit"
        )));
    }
    let token = invocation
        .value("--partial-token")
        .map(str::parse::<u32>)
        .transpose()
        .map_err(|_| grammar("partial-token requires a non-negative u32"))?
        .unwrap_or(0);
    let reference = Reference::resolve(&invocation.positionals[0])?;
    let (report, native_status) = reference.integrity(
        invocation.value("--expect-sha256"),
        invocation.has("--require-token-embedding") || invocation.has("--partial-token"),
        token,
    )?;
    let mut passed = report.passed != 0;
    let mut fields = vec![
        ("model".into(), invocation.positionals[0].clone()),
        ("resolved_path".into(), reference.path()?),
    ];
    let mut identity = "unregistered".to_string();
    let mut metadata = "unregistered".to_string();
    if detailed {
        let (native_identity, native_metadata, mut readiness) =
            metadata_fields(&reference, &report, &mut fields, &mut passed)?;
        identity = native_identity;
        metadata = native_metadata;
        // A requested token shape supersedes metadata's unindexed readiness.
        // The native integrity owner, not this renderer, validates that shape.
        let shape = ffi::text(&report.selected_embedding_shape);
        if !shape.is_empty() {
            let ready = shape == "valid";
            fields.push(("selected_embedding_ready".into(), ready.to_string()));
            readiness = if ready { "pass" } else { "fail" }.into();
        } else if invocation.has("--require-token-embedding") || invocation.has("--partial-token") {
            fields.push(("selected_embedding_ready".into(), "false".into()));
            readiness = "fail".into();
            passed = false;
        }
        let mut preflight = "not-checked";
        let mut backend_status = "not-checked".to_string();
        if let Some(backend) = backend {
            if passed {
                match ffi::backend_preflight(backend) {
                    Ok((state, supports)) => {
                        backend_status = state;
                        preflight = if supports { "pass" } else { "fail" };
                        passed &= supports;
                    }
                    Err(error) => {
                        backend_status = if error.code == -5 {
                            "unavailable"
                        } else {
                            "fail"
                        }
                        .into();
                        preflight = "fail";
                        passed = false;
                    }
                }
            } else {
                backend_status = "not-opened".into();
                preflight = "fail";
            }
        }
        fields.extend([
            ("identity_status".into(), identity.clone()),
            ("metadata_status".into(), metadata.clone()),
            ("readiness_status".into(), readiness),
            ("backend_status".into(), backend_status),
            ("materialization_preflight".into(), preflight.into()),
            ("materialization_gate".into(), preflight.into()),
            (
                "materialization_backend".into(),
                backend.unwrap_or("not-checked").into(),
            ),
            (
                "allocation_required_bytes".into(),
                report.known_tensor_bytes.to_string(),
            ),
        ]);
    }
    let status = if detailed {
        if passed {
            "integrity-report-pass"
        } else {
            "integrity-report-fail"
        }
    } else if passed {
        "artifact-integrity-pass"
    } else {
        "artifact-integrity-fail"
    };
    if !detailed || invocation.has("--audit") || mode == "audit" {
        // Native report fields are exact facts; only layout and CLI exit projection live here.
        fields.extend(integrity_fields(&report).into_iter().filter(|(key, _)| {
            !key.starts_with("selected_embedding_") || report.selected_embedding_shape[0] != 0
        }));
        fields.push(("execution_ready".into(), "false".into()));
        fields.push(("prefill_ready".into(), "false".into()));
        fields.push(("logits_ready".into(), "false".into()));
        fields.push(("generation".into(), "unsupported".into()));
        fields.push((
            "report_status".into(),
            if passed { "pass" } else { "fail" }.into(),
        ));
    } else {
        fields.retain(|(key, _)| ["model", "materialization_preflight"].contains(&key.as_str()));
        if let Some((_, model)) = fields.iter_mut().find(|(key, _)| key == "model") {
            *model = std::path::Path::new(model)
                .file_name()
                .and_then(|name| name.to_str())
                .unwrap_or(model)
                .to_string();
        }
        fields.push((
            "integrity_status".into(),
            if passed { "pass" } else { "fail" }.into(),
        ));
        if let Some(backend) = backend {
            fields.push(("backend".into(), backend.into()));
        }
        fields.push(("digest_status".into(), ffi::text(&report.digest_status)));
        let blocker = if passed {
            "none".into()
        } else if report.issue_count > 0 {
            ffi::text(&report.issues[0].code)
        } else if identity != "pass" && identity != "unregistered" {
            "identity".into()
        } else if metadata != "pass" && metadata != "unregistered" {
            "metadata".into()
        } else {
            "integrity".into()
        };
        if !passed {
            fields.push(("top_blocker".into(), blocker));
        }
    }
    fields.push((
        "boundary".into(),
        "integrity gate only, generation unsupported".into(),
    ));
    fields.push(("status".into(), status.into()));
    // Repeated selected-embedding facts are projections of the same report; emit once.
    let mut unique = std::collections::BTreeMap::new();
    for (key, value) in fields {
        unique.insert(key, value);
    }
    let pairs = unique
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    let rendered = presentation::record(
        &format!(
            "ARTIFACT INTEGRITY  {}",
            if passed { "PASS" } else { "FAIL" }
        ),
        &pairs,
        width,
        styled,
    )?;
    let exit = if detailed {
        if passed {
            0
        } else {
            crate::native_exit(raw::yvex_status_YVEX_ERR_STATE)
        }
    } else {
        crate::native_exit(native_status)
    };
    Ok(Output::standard(rendered, exit))
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<String> {
    let view = ModelView::open(&invocation.positionals[0])?;
    let info = view.descriptor()?;
    match invocation.operation.operation_id.as_str() {
        "artifact.inspect" => Ok(presentation::record(
            "ARTIFACT  descriptor",
            &[
                ("format", "gguf"),
                ("version", &info.header.version.to_string()),
                ("metadata_count", &info.header.metadata_count.to_string()),
                ("tensor_count", &info.header.tensor_count.to_string()),
                ("tensor_data_offset", &info.data_offset.to_string()),
                ("alignment", &info.alignment.to_string()),
                ("architecture", &info.architecture),
                ("model_name", &info.name),
                ("known_tensor_bytes", &info.known_bytes.to_string()),
                (
                    "unsupported_tensor_accounting",
                    &info.unsupported_accounting.to_string(),
                ),
                ("status", "descriptor-only"),
            ],
            width,
            styled,
        )?),
        "artifact.metadata" => {
            let fields: Vec<_> = view
                .metadata()?
                .into_iter()
                .map(|entry| {
                    let value = match entry.value {
                        MetadataValue::Unsigned(value) => value.to_string(),
                        MetadataValue::Signed(value) => value.to_string(),
                        MetadataValue::Float(value) => value.to_string(),
                        MetadataValue::Boolean(value) => value.to_string(),
                        // JSON quoting is a text escape policy, not a machine output schema.
                        MetadataValue::String(value) => {
                            serde_json::to_string(&String::from_utf8_lossy(&value))
                                .expect("text serialization")
                        }
                        MetadataValue::Array {
                            element_type,
                            count,
                        } => format!("array<{element_type}>[{count}]"),
                    };
                    (entry.key, value)
                })
                .collect();
            let borrowed: Vec<_> = fields
                .iter()
                .map(|(key, value)| (key.as_str(), value.as_str()))
                .collect();
            let mut output = presentation::record(
                "ARTIFACT  metadata",
                &[
                    ("format", "gguf"),
                    ("version", &info.header.version.to_string()),
                    ("metadata_count", &info.header.metadata_count.to_string()),
                ],
                width,
                styled,
            )?;
            output.push_str(&presentation::record("METADATA", &borrowed, width, styled)?);
            Ok(output)
        }
        "artifact.tensors" => {
            let mut output = presentation::record(
                "ARTIFACT  tensors",
                &[
                    ("format", "gguf"),
                    ("version", &info.header.version.to_string()),
                    ("tensor_count", &info.header.tensor_count.to_string()),
                    ("tensor_data_offset", &info.data_offset.to_string()),
                    ("alignment", &info.alignment.to_string()),
                ],
                width,
                styled,
            )?;
            for tensor in view.tensors()? {
                let range = tensor.range.map(|range| {
                    format!(
                        "{}..{}",
                        range.tensor_absolute_offset, range.tensor_end_offset
                    )
                });
                output.push_str(&presentation::record(
                    &format!("TENSOR  {}", tensor.name),
                    &[
                        ("role", &tensor.role),
                        ("dtype", &tensor.dtype),
                        ("rank", &tensor.dimensions.len().to_string()),
                        ("dims", &format!("{:?}", tensor.dimensions)),
                        ("bytes", &tensor.storage_bytes.to_string()),
                        ("offset", &tensor.relative_offset.to_string()),
                        ("absolute", &tensor.absolute_offset.to_string()),
                        ("range", range.as_deref().unwrap_or("unavailable")),
                        (
                            "range_status",
                            if tensor.range.is_some() {
                                "valid"
                            } else {
                                "invalid"
                            },
                        ),
                        (
                            "alignment_status",
                            tensor
                                .range
                                .map(|r| if r.aligned != 0 { "valid" } else { "invalid" })
                                .unwrap_or("unknown"),
                        ),
                    ],
                    width,
                    styled,
                )?);
            }
            Ok(output)
        }
        _ => Err("unprojected artifact operation".into()),
    }
}
