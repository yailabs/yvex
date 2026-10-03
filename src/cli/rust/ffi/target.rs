// Copy typed native target observations. Legacy rows/tables are never read.
use super::{Error, borrowed_text, error, extent_error, raw, text};
use crate::registry::Invocation;
use serde_json::{Value, json};

pub(crate) struct Report {
    pub facts: Value,
    pub machine: Option<Value>,
    pub exit: u8,
    pub errors: Vec<String>,
}

fn store(destination: &mut [std::ffi::c_char], value: Option<&str>) -> Result<(), Error> {
    let value = value.unwrap_or("");
    if value.len() >= destination.len() || value.as_bytes().contains(&0) {
        return Err(extent_error());
    }
    for (byte, destination) in value.bytes().zip(destination) {
        *destination = std::ffi::c_char::from_ne_bytes([byte]);
    }
    Ok(())
}

fn request(invocation: &Invocation<'_>) -> Result<raw::yvex_model_target_request, Error> {
    let kind = match invocation.positionals[0].as_str() {
        "decision" => raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_DECISION,
        "candidate" => raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_CANDIDATE,
        "dense-candidate" => {
            raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE
        }
        "qwen-metal" => raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_QWEN_METAL,
        "class-profile" => {
            raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE
        }
        "tensor-collection" => {
            raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION
        }
        "tensor-map" => raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP,
        "tokenizer-map" => {
            raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP
        }
        "missing-roles" => {
            raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES
        }
        "quant-policy" => {
            raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY
        }
        "inspect" => raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_INSPECT,
        _ => return Err(extent_error()),
    };
    let mut request = raw::yvex_model_target_request {
        kind,
        ..Default::default()
    };
    // Native report owners select the compact or audit fact population. Rust
    // never reconstructs it from formatted rows. Tables arrange audit facts.
    request.mode = if invocation.has("--check-output-contract") {
        match invocation.value("--output").unwrap_or("normal") {
            "normal" => raw::yvex_model_target_render_mode_YVEX_MODEL_TARGET_OUTPUT_NORMAL,
            "table" => raw::yvex_model_target_render_mode_YVEX_MODEL_TARGET_OUTPUT_TABLE,
            _ => raw::yvex_model_target_render_mode_YVEX_MODEL_TARGET_OUTPUT_AUDIT,
        }
    } else if invocation.has("--audit")
        || matches!(invocation.value("--output"), Some("audit" | "table"))
    {
        raw::yvex_model_target_render_mode_YVEX_MODEL_TARGET_OUTPUT_AUDIT
    } else {
        raw::yvex_model_target_render_mode_YVEX_MODEL_TARGET_OUTPUT_NORMAL
    };
    request.output_json =
        i32::from(invocation.has("--json") || invocation.value("--output") == Some("json"));
    if request.output_json != 0 {
        request.mode = raw::yvex_model_target_render_mode_YVEX_MODEL_TARGET_OUTPUT_JSON;
    }
    store(
        &mut request.target_id,
        invocation
            .value("--target")
            .or_else(|| invocation.positionals.get(1).map(String::as_str)),
    )?;
    macro_rules! argument {
        ($($member:ident => $flag:literal),*) => { $(store(&mut request.$member, invocation.value($flag))?;)* };
    }
    argument!(release => "--release", models_root => "--models-root", source_path => "--source",
        role => "--role", gate => "--gate", candidate_kind => "--candidate",
        output_contract => "--check-output-contract");
    macro_rules! flag {
        ($($member:ident => $flag:literal),*) => { $(request.$member = invocation.has($flag).into();)* };
    }
    flag!(include_hardware => "--include-hardware", include_backend => "--include-backend",
        include_source => "--include-source", include_blockers => "--include-blockers",
        include_next => "--include-next", include_examples => "--include-examples",
        include_candidates => "--include-candidates", include_pressure_targets => "--include-pressure-targets",
        include_critical_path => "--include-critical-path", include_requirements => "--include-requirements",
        include_paths => "--paths", strict => "--strict");
    request.include_requirements |=
        i32::from(invocation.has("--roles") || invocation.has("--role-support"));
    Ok(request)
}

fn fields(report: &raw::yvex_model_target_report) -> Result<Value, Error> {
    let mut fields = serde_json::Map::new();
    if !report.status.is_null() {
        fields.insert(
            "status".into(),
            json!(unsafe { borrowed_text(report.status)? }),
        );
    }
    macro_rules! copy {
        ($($name:ident),*) => { $(if report.$name[0] != 0 {
            fields.insert(stringify!($name).into(), json!(text(&report.$name)));
        })* };
    }
    copy!(
        target_id,
        family,
        model,
        target_class,
        stage,
        eligibility,
        source_status,
        artifact_status,
        tensor_map_status,
        qtype_policy_status,
        runtime_status,
        generation_status,
        benchmark_status,
        next_row,
        boundary,
        reason
    );
    let count = usize::try_from(report.fact_count).map_err(|_| extent_error())?;
    let facts = report.facts.get(..count).ok_or_else(extent_error)?;
    for fact in facts {
        let value = match fact.kind {
            raw::yvex_model_target_fact_kind_YVEX_MODEL_TARGET_FACT_TEXT => json!(text(&fact.text)),
            raw::yvex_model_target_fact_kind_YVEX_MODEL_TARGET_FACT_U64 => json!(fact.number),
            _ => return Err(extent_error()),
        };
        fields.insert(text(&fact.name), value);
    }
    Ok(Value::Object(fields))
}

fn named_counts(values: &[raw::yvex_model_target_named_count], count: u32) -> Result<Value, Error> {
    let rows = values.get(..count as usize).ok_or_else(extent_error)?;
    Ok(json!(
        rows.iter()
            .map(|row| json!({"name": text(&row.name), "count": row.count}))
            .collect::<Vec<_>>()
    ))
}

fn mapping(report: &raw::yvex_model_target_report, facts: &mut Value) -> Result<Value, Error> {
    let native = unsafe { report.detail.map };
    let machine = json!({"status": facts["status"], "target_id": text(&report.target_id),
        "source_contributions": native.source_contributions, "descriptors": native.descriptors,
        "trunk_descriptors": native.trunk_descriptors, "draft_descriptors": native.draft_descriptors,
        "pinned_standard_names": native.pinned_standard_names, "extension_names": native.extension_names,
        "metadata": native.metadata, "header_scans": native.header_scans, "payload_bytes_read": native.payload_bytes,
        "mapping_identity": format!("{:016x}", native.mapping_identity), "artifact": "not-produced",
        "runtime": "unsupported", "generation": "unsupported", "next": text(&report.next_row)});
    facts
        .as_object_mut()
        .unwrap()
        .extend(machine.as_object().unwrap().clone());
    facts["source_identity"] = json!(format!("{:016x}", native.source_identity));
    facts["coverage_identity"] = json!(format!("{:016x}", native.coverage_identity));
    facts["semantic_standard_names"] = json!(native.semantic_standard_names);
    facts["collections"] = named_counts(&native.collections, native.collection_count)?;
    Ok(machine)
}

fn coverage(report: &raw::yvex_model_target_report, facts: &mut Value) -> Result<Value, Error> {
    let native = unsafe { report.detail.coverage };
    let machine = json!({"status": facts["status"], "target_id": text(&report.target_id),
        "source_tensors": native.source_tensors, "required_tensors": native.required_tensors,
        "matched_tensors": native.matched_tensors, "missing": native.missing_tensors,
        "ambiguous": native.ambiguous_tensors, "unexpected": native.unexpected_tensors,
        "header_scans": native.header_scans, "payload_bytes_read": native.payload_bytes,
        "coverage_identity": format!("{:016x}", native.coverage_identity), "mapping": "blocked",
        "runtime": "unsupported", "generation": "unsupported", "next": text(&report.next_row)});
    facts
        .as_object_mut()
        .unwrap()
        .extend(machine.as_object().unwrap().clone());
    facts["source_identity"] = json!(format!("{:016x}", native.source_identity));
    facts["source_lookups"] = json!(native.source_lookups);
    facts["source_collisions"] = json!(native.source_collisions);
    facts["source_maximum_probe"] = json!(native.source_maximum_probe);
    facts["collections"] = named_counts(&native.collections, native.collection_count)?;
    Ok(machine)
}

fn architecture(report: &raw::yvex_model_target_report, facts: &mut Value) -> Result<Value, Error> {
    let native = unsafe { report.detail.architecture };
    let machine = json!({"status": facts["status"], "target_id": text(&native.target_id),
        "repository": text(&native.repository), "revision": text(&native.revision),
        "layers": native.target_layers, "draft_layers": native.draft_layers,
        "hidden_size": native.hidden_size, "vocabulary_size": native.vocabulary_size,
        "context": native.maximum_context, "attention": {"swa": native.swa_layers,
            "csa": native.csa_layers, "hca": native.hca_layers},
        "routing": {"hash": native.hash_router_layers, "learned": native.learned_router_layers},
        "mhc_streams": native.mhc_residual_streams, "payload_bytes_read": native.source_payload_bytes,
        "runtime": "unsupported", "generation": "unsupported", "next": text(&report.next_row)});
    facts
        .as_object_mut()
        .unwrap()
        .extend(machine.as_object().unwrap().clone());
    macro_rules! numeric {
        ($($name:ident),*) => { $(facts[stringify!($name)] = json!(native.$name);)* };
    }
    numeric!(
        query_heads,
        kv_heads,
        head_dimension,
        rope_head_dimension,
        routed_experts,
        experts_per_token,
        shared_experts,
        dspark_block_size,
        dspark_noise_token_id,
        dspark_markov_rank,
        mhc_expanded_width,
        mhc_mixing_rows,
        mhc_mixing_columns,
        mhc_sinkhorn_iterations,
        tokenizer_vocabulary_size,
        tokenizer_base_vocab_entries,
        tokenizer_added_token_entries,
        bos_token_id,
        eos_token_id,
        source_quant_block_rows,
        source_quant_block_columns,
        source_header_scans,
        source_header_tensors
    );
    macro_rules! flag {
        ($($name:ident),*) => { $(facts[stringify!($name)] = json!(native.$name != 0);)* };
    }
    flag!(
        dspark_confidence_available,
        final_mhc_post_required,
        final_mhc_head_required,
        final_norm_after_mhc_head,
        output_head_required,
        output_head_tied
    );
    macro_rules! string {
        ($($name:ident),*) => { $(facts[stringify!($name)] = json!(text(&native.$name));)* };
    }
    string!(
        family,
        architecture,
        verification_stage,
        paper_revision,
        sglang_revision,
        vllm_revision,
        source_weight_dtype,
        source_expert_dtype,
        source_quantization,
        tokenizer_class,
        tokenizer_model_type
    );
    facts["feature_layers"] = json!(
        native
            .dspark_feature_layers
            .get(..native.dspark_feature_layer_count as usize)
            .ok_or_else(extent_error)?
    );
    facts["layer_programs"] = json!(
        native
            .layers
            .get(..native.layer_count as usize)
            .ok_or_else(extent_error)?
            .iter()
            .map(
                |row| json!({"index": row.index, "compression_ratio": row.compression_ratio,
        "attention": text(&row.attention), "kv": text(&row.kv), "router": text(&row.router),
        "mhc_entry": text(&row.mhc_entry)})
            )
            .collect::<Vec<_>>()
    );
    facts["draft_programs"] = json!(
        native
            .drafts
            .get(..native.draft_count as usize)
            .ok_or_else(extent_error)?
            .iter()
            .map(
                |row| json!({"predictor_index": row.predictor_index, "layer_index": row.layer_index,
        "compression_ratio": row.compression_ratio, "attention": text(&row.attention),
        "router": text(&row.router), "feature_projection": row.feature_projection != 0,
        "markov": row.markov != 0, "confidence": row.confidence != 0,
        "shared_head": row.shared_head != 0})
            )
            .collect::<Vec<_>>()
    );
    Ok(machine)
}

fn composite(report: &raw::yvex_model_target_report, facts: &mut Value) -> Result<Value, Error> {
    let native = unsafe { report.detail.composite };
    let machine = json!({"status": facts["status"], "target_id": text(&report.target_id), "family": "minimax-h3",
        "repository": text(&native.repository), "revision": text(&native.revision), "subtree": text(&native.subtree),
        "components": native.components, "weighted_components": native.weighted_components,
        "phase_edges": native.phase_edges, "shards": native.shards, "tensors": native.tensors,
        "elements": native.elements, "payload_bytes": native.payload_bytes,
        "architecture_identity": text(&native.architecture_identity),
        "phase_dag_identity": text(&native.phase_dag_identity), "role_map_identity": text(&native.role_map_identity),
        "transformation_identity": text(&native.transformation_identity), "stage": "source-to-transformation-ir",
        "artifact": "not-produced", "runtime": "unsupported", "generation": "unsupported",
        "next": text(&report.next_row)});
    facts
        .as_object_mut()
        .unwrap()
        .extend(machine.as_object().unwrap().clone());
    facts["source_snapshot_identity"] = json!(text(&native.source_snapshot_identity));
    facts["component_manifest_identity"] = json!(text(&native.component_manifest_identity));
    facts["unresolved_requirements_identity"] =
        json!(text(&native.unresolved_requirements_identity));
    facts["payload_execution_bytes"] = json!(native.payload_execution_bytes);
    facts["component_programs"] = json!(native.component.get(..native.component_count as usize)
        .ok_or_else(extent_error)?
        .iter().map(|row| json!({"id": text(&row.canonical_id), "identity": text(&row.identity), "shards": row.shards,
        "tensors": row.tensors, "phase": row.phase, "weighted": row.weighted != 0,
        "release_after_phase": row.release_after_phase != 0})).collect::<Vec<_>>());
    facts["phase_program"] = json!(
        native
            .edge
            .get(..native.edge_count as usize)
            .ok_or_else(extent_error)?
            .iter()
            .map(
                |row| json!({"source": row.source_phase, "destination": row.destination_phase,
        "data_classes": row.data_classes, "lifetime": row.lifetime})
            )
            .collect::<Vec<_>>()
    );
    Ok(machine)
}

pub(crate) fn candidates(dense: bool) -> Result<Vec<Value>, Error> {
    (0..unsafe { raw::yvex_model_target_candidate_count() })
        .map(|index| {
            let mut native = raw::yvex_model_target_candidate_projection::default();
            let mut failure = raw::yvex_error::default();
            super::execution::checked(
                unsafe {
                    raw::yvex_model_target_candidate_at(
                        index,
                        dense.into(),
                        &mut native,
                        &mut failure,
                    )
                },
                &failure,
            )?;
            let mut result = json!({});
            macro_rules! field {
            ($($name:ident),*) => { $(if !native.$name.is_null() {
                result[stringify!($name)] = json!(unsafe { borrowed_text(native.$name)? });
            })* };
        }
            field!(
                id,
                class_name,
                stage,
                eligibility,
                status,
                reason,
                next,
                blocker,
                secondary_blocker
            );
            Ok(result)
        })
        .collect()
}

pub(crate) fn report(invocation: &Invocation<'_>) -> Result<Report, Error> {
    struct Native(Box<raw::yvex_model_target_report>);
    impl Drop for Native {
        fn drop(&mut self) {
            unsafe { raw::yvex_model_target_report_close(self.0.as_mut()) };
        }
    }
    let request = request(invocation)?;
    let mut native = Native(Box::default());
    let mut failure = raw::yvex_error::default();
    let status =
        unsafe { raw::yvex_model_target_report_build(&request, native.0.as_mut(), &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    if native.0.fact_failed != 0 {
        return Err(extent_error());
    }
    let mut facts = fields(&native.0)?;
    let machine = match native.0.detail_kind {
        raw::yvex_model_target_detail_kind_YVEX_MODEL_TARGET_DETAIL_TENSOR_MAP => {
            Some(mapping(&native.0, &mut facts)?)
        }
        raw::yvex_model_target_detail_kind_YVEX_MODEL_TARGET_DETAIL_TENSOR_COVERAGE => {
            Some(coverage(&native.0, &mut facts)?)
        }
        raw::yvex_model_target_detail_kind_YVEX_MODEL_TARGET_DETAIL_MODEL_ARCHITECTURE => {
            Some(architecture(&native.0, &mut facts)?)
        }
        raw::yvex_model_target_detail_kind_YVEX_MODEL_TARGET_DETAIL_COMPOSITE_ARCHITECTURE => {
            Some(composite(&native.0, &mut facts)?)
        }
        raw::yvex_model_target_detail_kind_YVEX_MODEL_TARGET_DETAIL_NONE => {
            if native.0.kind
                == raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_DECISION
                && native.0.exit_code == 0
            {
                let mut value = facts.clone();
                value["release_qtype"] = Value::Null;
                Some(value)
            } else if facts["status"] == "architecture-ir-blocked" {
                Some(
                    json!({"status": facts["status"], "target_id": facts["target_id"],
                    "source_verification": "blocked", "reason": facts["reason"],
                    "runtime": "unsupported", "generation": "unsupported"}),
                )
            } else if facts["status"] == "missing-role-source-report" {
                Some(
                    json!({"status": facts["status"], "target_id": facts["target_id"],
                    "family": facts["family"], "evidence_basis": facts["evidence_basis"],
                    "source_roles": {"required": facts["source_roles_required"],
                        "observed": facts["source_roles_observed"], "missing": facts["source_roles_missing"],
                        "ambiguous": facts["source_roles_ambiguous"]},
                    "metadata": {"required": facts["metadata_required"], "observed": facts["metadata_observed"],
                        "missing": facts["metadata_missing"]},
                    "top_blocker": facts["top_blocker"], "next": facts["next"],
                    "runtime": facts["runtime_status"], "generation": facts["generation_status"]}),
                )
            } else if native.0.kind
                == raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP
            {
                Some(
                    json!({"status": facts["status"], "target_id": facts["target_id"],
                    "vocab_status": facts["vocab_status"], "next": facts["next"]}),
                )
            } else if native.0.kind
                == raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES
                && facts["status"] == "missing-role-report"
            {
                Some(
                    json!({"status": facts["status"], "target_id": facts["target_id"],
                    "top_blocker": facts["top_blocker"], "qwen_linear_attn": facts["qwen_linear_attn"],
                    "shared_expert": facts["shared_expert"], "tokenizer": facts["tokenizer"], "next": facts["next"]}),
                )
            } else if native.0.kind
                == raw::yvex_model_target_command_kind_YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP
                && request.gate[0] != 0
                && native.0.exit_code == 0
            {
                Some(
                    json!({"status": facts["status"], "target_id": facts["target_id"],
                    "top_blocker": facts["top_blocker"], "next": facts["next"]}),
                )
            } else {
                None
            }
        }
        _ => return Err(extent_error()),
    };
    Ok(Report {
        facts,
        machine,
        exit: u8::try_from(native.0.exit_code).map_err(|_| extent_error())?,
        errors: native
            .0
            .error_rows
            .get(..usize::try_from(native.0.error_row_count).map_err(|_| extent_error())?)
            .ok_or_else(extent_error)?
            .iter()
            .map(|row| text(&row.value))
            .collect(),
    })
}
