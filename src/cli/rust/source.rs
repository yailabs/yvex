// Source provenance and trust stay native; this module owns grammar and presentation only.
use crate::{
    Output,
    ffi::{self, ManifestRequest, ReportRequest, SourceReport, raw},
    presentation,
    registry::{Invocation, Refusal},
};
use serde_json::{Value, json};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn grammar(reason: &str) -> Box<dyn std::error::Error> {
    Box::new(Refusal {
        reason: reason.into(),
        hint: None,
    })
}

fn require<'a>(invocation: &'a Invocation<'_>, name: &str) -> Result<&'a str> {
    invocation
        .value(name)
        .filter(|value| !value.is_empty())
        .ok_or_else(|| grammar(&format!("{name} is required")))
}

fn admit_flags(invocation: &Invocation<'_>, allowed: &[&str]) -> Result<()> {
    for name in invocation.flags.keys() {
        if !allowed.contains(&name.as_str()) {
            return Err(grammar(&format!(
                "{name} does not apply to this source action"
            )));
        }
    }
    Ok(())
}

fn render(
    title: &str,
    fields: &[(String, String)],
    width: usize,
    styled: bool,
    exit: u8,
) -> Result<Output> {
    let pairs = fields
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output::standard(
        presentation::record(title, &pairs, width, styled)?,
        exit,
    ))
}

fn create(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    admit_flags(
        invocation,
        &[
            "--hf-repo",
            "--revision",
            "--local-path",
            "--out",
            "--license",
            "--model-card",
            "--node",
            "--dry-run-log",
            "--download-log",
            "--pid-file",
            "--download-command",
            "--status",
        ],
    )?;
    let status = match invocation.value("--status").unwrap_or("unknown") {
        "unknown" => raw::yvex_source_status_YVEX_SOURCE_STATUS_UNKNOWN,
        "in-progress" => raw::yvex_source_status_YVEX_SOURCE_STATUS_IN_PROGRESS,
        "incomplete" => raw::yvex_source_status_YVEX_SOURCE_STATUS_INCOMPLETE,
        "failed" => raw::yvex_source_status_YVEX_SOURCE_STATUS_FAILED,
        "complete" => {
            return Err(grammar(
                "source status complete is verifier-owned; run strict exact-source verification",
            ));
        }
        _ => return Err(grammar("unknown source status")),
    };
    let request = ManifestRequest {
        repository: require(invocation, "--hf-repo")?,
        revision: require(invocation, "--revision")?,
        local: require(invocation, "--local-path")?,
        out: require(invocation, "--out")?,
        status,
        license: invocation.value("--license"),
        card: invocation.value("--model-card"),
        node: invocation.value("--node"),
        dry_run: invocation.value("--dry-run-log"),
        log: invocation.value("--download-log"),
        pid: invocation.value("--pid-file"),
        command: invocation.value("--download-command"),
    };
    let summary = ffi::source_manifest(request)?;
    render(
        "SOURCE MANIFEST  written",
        &[
            ("repo".into(), require(invocation, "--hf-repo")?.into()),
            ("revision".into(), require(invocation, "--revision")?.into()),
            (
                "local_path".into(),
                require(invocation, "--local-path")?.into(),
            ),
            ("files".into(), summary.file_count.to_string()),
            ("safetensors".into(), summary.safetensors_count.to_string()),
            (
                "total_size_bytes".into(),
                summary.total_size_bytes.to_string(),
            ),
            ("out".into(), require(invocation, "--out")?.into()),
            ("status".into(), "source-manifest-written".into()),
        ],
        width,
        styled,
        0,
    )
}

fn verify(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    admit_flags(
        invocation,
        &["--source", "--models-root", "--source-manifest", "--target"],
    )?;
    if ["--source", "--models-root", "--source-manifest"]
        .iter()
        .any(|name| invocation.value(name).is_none())
    {
        return Err(grammar(
            "--source, --models-root, and --source-manifest are required",
        ));
    }
    let (result, trust) = ffi::source_verify(
        require(invocation, "--source")?,
        require(invocation, "--models-root")?,
        require(invocation, "--source-manifest")?,
        invocation.value("--target"),
    )?;
    let native = &result.verification;
    render(
        "SOURCE VERIFY  complete",
        &[
            ("source".into(), ffi::text(&native.resolved_source_path)),
            ("revision".into(), ffi::text(&native.revision)),
            ("manifest".into(), ffi::text(&native.manifest_path)),
            ("manifest_schema".into(), ffi::text(&native.manifest_schema)),
            (
                "source_snapshot_identity".into(),
                result.payload.source_snapshot_identity.to_string(),
            ),
            (
                "payload_identity".into(),
                ffi::text(&result.payload.payload_identity),
            ),
            ("payload_trust".into(), trust),
            ("shards".into(), result.payload.shard_count.to_string()),
            ("tensors".into(), result.payload.tensor_count.to_string()),
            (
                "logical_tensor_bytes".into(),
                result.payload.logical_tensor_bytes.to_string(),
            ),
            (
                "physical_bytes_read".into(),
                result.stream.physical_bytes_read.to_string(),
            ),
            (
                "published_identity_reused".into(),
                (result.reused_published_identity != 0).to_string(),
            ),
        ],
        width,
        styled,
        0,
    )
}

fn known(value: &[std::ffi::c_char]) -> String {
    let text = ffi::text(value);
    if text.is_empty() {
        "unknown".into()
    } else {
        text
    }
}

fn contract(report: &SourceReport) -> Value {
    let native = &report.native;
    let verification = &native.verification;
    let mut result = json!({
        "status": report.status, "family": report.profile.family_key, "target_id": report.target,
        "repository": ffi::text(&native.identity_repo_id), "revision": ffi::text(&native.identity_revision),
        "source_path": ffi::text(&native.source_path), "source_kind": ffi::text(&verification.source_kind),
        "verification": report.semantics.verification_status,
        "inventory_authority": ffi::text(&verification.inventory_authority),
        "upstream_index_oid": ffi::text(&verification.upstream_index_oid),
        "upstream_index_identity_verified": verification.upstream_index_identity_verified != 0,
        "header_scan_count": verification.header_scan_count,
        "manifest_schema": ffi::text(&verification.manifest_schema),
        "manifest_state": ffi::text(&verification.manifest_status),
        "manifest_verification_stage": ffi::text(&verification.verification_stage),
        "manifest_verified": verification.manifest_verified != 0,
        "top_blocker": report.top_blocker, "next": report.next,
        "source_verification_blocker_count": verification.blocker_count, "blocker_count": report.blockers.len(),
        "source_file_count": native.source_file_count, "source_total_bytes": native.total_size_bytes,
        "shard_count": native.safetensors_count, "header_shard_count": native.native_safetensors_header_read_count,
        "header_tensor_count": native.native_tensor_count,
        "header_dtype_bf16_count": verification.dtype_bf16_count,
        "header_dtype_f32_count": verification.dtype_f32_count,
        "header_dtype_i64_count": verification.dtype_i64_count, "header_dtype_i8_count": verification.dtype_i8_count,
        "header_dtype_f8_e4m3_count": verification.dtype_f8_count,
        "header_dtype_f8_e8m0_count": verification.dtype_f8_e8m0_count,
        "config_valid": verification.config_valid != 0, "tokenizer_valid": native.semantics.tokenizer_verified != 0,
        "generation_config_valid": verification.generation_config_valid != 0,
        "shard_index_valid": verification.shard_index_valid != 0,
        "inference_config_valid": verification.inference_config_valid != 0,
        "dspark_block_size": verification.dspark_block_size,
        "dspark_noise_token_id": verification.dspark_noise_token_id,
        "dspark_target_layer_count": verification.dspark_target_layer_count,
        "dspark_target_layer_0": verification.dspark_target_layer_ids[0],
        "dspark_target_layer_1": verification.dspark_target_layer_ids[1],
        "dspark_target_layer_2": verification.dspark_target_layer_ids[2],
        "dspark_markov_rank": verification.dspark_markov_rank,
        "dspark_inference_layer_count": verification.dspark_inference_layer_count,
        "tensor_payload_loaded": false, "artifact_status": report.profile.yvex_produced_artifact_status,
        "runtime": "unsupported", "generation": "unsupported", "benchmark": "not-measured",
    });
    for (index, blocker) in report.blockers.iter().enumerate() {
        result[format!("blocker_{index}")] = blocker.as_str().into();
    }
    result
}

fn scalar_fields(value: &Value) -> Vec<(String, String)> {
    value
        .as_object()
        .expect("typed projection object")
        .iter()
        .map(|(key, value)| {
            (
                key.clone(),
                value
                    .as_str()
                    .map(String::from)
                    .unwrap_or_else(|| value.to_string()),
            )
        })
        .collect()
}

fn display_fact(value: String, fallback: &str) -> String {
    if value.is_empty() {
        fallback.into()
    } else {
        value
    }
}

// Declarative typed field projections; no pointer offsets or alternate source decisions.
macro_rules! fields {
    ($($name:literal => $value:expr),* $(,)?) => { vec![$(($name.into(), $value)),*] };
}

fn audit_configuration(report: &SourceReport) -> Vec<(String, String)> {
    let verification = &report.native.verification;
    fields! {
        "config_hidden_size" => verification.hidden_size.to_string(),
        "config_num_hidden_layers" => verification.num_hidden_layers.to_string(),
        "config_num_attention_heads" => verification.num_attention_heads.to_string(),
        "config_num_key_value_heads" => verification.num_key_value_heads.to_string(),
        "config_head_dim" => verification.head_dim.to_string(),
        "config_qk_rope_head_dim" => verification.qk_rope_head_dim.to_string(),
        "config_max_position_embeddings" => verification.max_position_embeddings.to_string(),
        "config_moe_intermediate_size" => verification.moe_intermediate_size.to_string(),
        "config_n_routed_experts" => verification.n_routed_experts.to_string(),
        "config_n_shared_experts" => verification.n_shared_experts.to_string(),
        "config_num_experts_per_tok" => verification.num_experts_per_tok.to_string(),
        "config_num_hash_layers" => verification.num_hash_layers.to_string(),
        "config_q_lora_rank" => verification.q_lora_rank.to_string(),
        "config_o_lora_rank" => verification.o_lora_rank.to_string(),
        "config_o_groups" => verification.o_groups.to_string(),
        "config_index_head_dim" => verification.index_head_dim.to_string(),
        "config_index_n_heads" => verification.index_n_heads.to_string(),
        "config_index_topk" => verification.index_topk.to_string(),
        "config_hc_mult" => verification.hc_mult.to_string(),
        "config_hc_sinkhorn_iters" => verification.hc_sinkhorn_iters.to_string(),
        "config_compress_rope_theta" => verification.compress_rope_theta.to_string(),
        "config_compress_ratio_count" => verification.compress_ratio_count.to_string(),
        "config_vocab_size" => verification.vocab_size.to_string(),
        "config_bos_token_id" => verification.bos_token_id.to_string(),
        "config_eos_token_id" => verification.eos_token_id.to_string(),
        "config_sliding_window" => verification.sliding_window.to_string(),
        "config_num_nextn_predict_layers" => verification.num_nextn_predict_layers.to_string(),
        "config_dspark_block_size" => verification.dspark_block_size.to_string(),
        "config_dspark_noise_token_id" => verification.dspark_noise_token_id.to_string(),
        "config_dspark_target_layer_count" => verification.dspark_target_layer_count.to_string(),
        "config_dspark_markov_rank" => verification.dspark_markov_rank.to_string(),
        "inference_dspark_layer_count" => verification.dspark_inference_layer_count.to_string(),
        "generation_config_bos_token_id" => verification.generation_bos_token_id.to_string(),
        "generation_config_eos_token_id" => verification.generation_eos_token_id.to_string(),
        "indexed_tensor_count" => verification.indexed_tensor_count.to_string(),
        "referenced_shard_count" => verification.referenced_shard_count.to_string(),
        "source_file_count" => verification.source_file_count.to_string(),
        "source_total_size_bytes" => verification.source_total_bytes.to_string(),
        "source_shard_count" => verification.shard_count.to_string(),
        "source_shard_bytes" => verification.shard_bytes.to_string(),
        "header_shard_count" => verification.header_shard_count.to_string(),
        "header_tensor_count" => verification.header_tensor_count.to_string(),
        "header_bytes" => verification.header_bytes.to_string(),
        "declared_tensor_bytes" => verification.declared_tensor_bytes.to_string(),
        "header_max_tensor_rank" => verification.max_tensor_rank.to_string(),
        "header_dtype_f16_count" => verification.dtype_f16_count.to_string(),
        "header_dtype_bf16_count" => verification.dtype_bf16_count.to_string(),
        "header_dtype_f32_count" => verification.dtype_f32_count.to_string(),
        "header_dtype_i64_count" => verification.dtype_i64_count.to_string(),
        "header_dtype_i8_count" => verification.dtype_i8_count.to_string(),
        "header_dtype_fp4_count" => verification.dtype_fp4_count.to_string(),
        "header_dtype_f8_count" => verification.dtype_f8_count.to_string(),
        "header_dtype_f8_e8m0_count" => verification.dtype_f8_e8m0_count.to_string(),
        "header_dtype_other_count" => verification.dtype_other_count.to_string(),
        "source_kind" => display_fact(ffi::text(&verification.source_kind), "missing"),
        "repository_verified" => (verification.repository_verified != 0).to_string(),
        "revision_verified" => (verification.revision_verified != 0).to_string(),
        "config_model_type" => display_fact(ffi::text(&verification.model_type), "missing"),
        "config_architecture" => display_fact(ffi::text(&verification.architecture), "missing"),
        "config_hc_eps" => display_fact(ffi::text(&verification.hc_eps), "missing"),
        "config_tie_word_embeddings" => (verification.tie_word_embeddings != 0).to_string(),
        "config_attention_bias" => (verification.attention_bias != 0).to_string(),
        "config_attention_dropout" => display_fact(ffi::text(&verification.attention_dropout), "missing"),
        "config_rms_norm_eps" => display_fact(ffi::text(&verification.rms_norm_eps), "missing"),
        "config_rope_theta" => verification.rope_theta.to_string(),
        "config_hidden_act" => display_fact(ffi::text(&verification.hidden_act), "missing"),
        "config_torch_dtype" => display_fact(ffi::text(&verification.torch_dtype), "missing"),
        "config_expert_dtype" => display_fact(ffi::text(&verification.expert_dtype), "missing"),
        "config_routed_scaling_factor" => display_fact(ffi::text(&verification.routed_scaling_factor), "missing"),
        "config_scoring_func" => display_fact(ffi::text(&verification.scoring_func), "missing"),
        "config_topk_method" => display_fact(ffi::text(&verification.topk_method), "missing"),
        "config_norm_topk_prob" => (verification.norm_topk_prob != 0).to_string(),
        "config_swiglu_limit" => display_fact(ffi::text(&verification.swiglu_limit), "missing"),
        "config_use_cache" => (verification.use_cache != 0).to_string(),
        "tokenizer_class" => display_fact(ffi::text(&verification.tokenizer_class), "missing"),
        "tokenizer_model_type" => display_fact(ffi::text(&verification.tokenizer_model_type), "missing"),
        "tokenizer_model_max_length" => verification.tokenizer_model_max_length.to_string(),
        "generation_config_from_model" => (verification.generation_from_model_config != 0).to_string(),
        "generation_config_do_sample" => (verification.generation_do_sample != 0).to_string(),
        "generation_config_temperature" => display_fact(ffi::text(&verification.generation_temperature), "missing"),
        "generation_config_top_p" => display_fact(ffi::text(&verification.generation_top_p), "missing"),
        "generation_config_transformers_version" =>
            display_fact(ffi::text(&verification.generation_transformers_version), "missing"),
        "header_index_match" => (verification.shard_index_headers_match != 0).to_string(),
        "upstream_index_oid" => display_fact(ffi::text(&verification.upstream_index_oid), "not-applicable"),
        "local_index_oid" => display_fact(ffi::text(&verification.local_index_oid), "not-applicable"),
        "upstream_index_identity_verified" => (verification.upstream_index_identity_verified != 0).to_string(),
        "header_scan_count" => verification.header_scan_count.to_string(),
        "manifest_schema" => display_fact(ffi::text(&verification.manifest_schema), "missing"),
        "manifest_verification_stage" => display_fact(ffi::text(&verification.verification_stage), "missing"),
        "manifest_verified" => (verification.manifest_verified != 0).to_string(),
        "manifest_published" => (verification.manifest_published != 0).to_string(),
        "manifest_reopened" => (verification.manifest_reopened != 0).to_string(),
        "source_manifest_status" => display_fact(ffi::text(&verification.manifest_status), "missing"),
        "source_manifest_path" => display_fact(ffi::text(&verification.manifest_path), "missing"),
        "source_manifest_revision" => display_fact(ffi::text(&verification.manifest_revision), "missing"),
        "source_revision" => display_fact(ffi::text(&verification.revision), "missing"),
    }
}

fn audit_inventory(report: &SourceReport) -> Vec<(String, String)> {
    let native = &report.native;
    fields! {
        "source_file_count" => native.source_file_count.to_string(),
        "source_regular_file_count" => native.source_regular_file_count.to_string(),
        "source_safetensors_count" => native.safetensors_count.to_string(),
        "source_bin_count" => native.bin_count.to_string(),
        "source_dat_count" => native.dat_count.to_string(),
        "source_json_count" => native.json_count.to_string(),
        "source_tokenizer_file_count" => native.tokenizer_file_count.to_string(),
        "source_config_file_count" => native.config_file_count.to_string(),
        "source_total_size_bytes" => native.total_size_bytes.to_string(),
        "source_safetensors_size_bytes" => native.safetensors_size_bytes.to_string(),
        "source_sidecar_size_bytes" => native.sidecar_size_bytes.to_string(),
        "source_other_size_bytes" => native.other_size_bytes.to_string(),
        "native_safetensors_count" => native.native_safetensors_count.to_string(),
        "native_safetensors_opened" => native.native_safetensors_opened.to_string(),
        "native_safetensors_header_read_count" => native.native_safetensors_header_read_count.to_string(),
        "native_safetensors_header_error_count" => native.native_safetensors_header_error_count.to_string(),
        "native_safetensors_header_bytes" => native.native_safetensors_header_bytes.to_string(),
        "native_declared_data_bytes" => native.native_declared_data_bytes.to_string(),
        "native_declared_tensor_bytes" => native.native_declared_tensor_bytes.to_string(),
        "native_max_rank" => native.native_max_rank.to_string(),
        "native_max_tensor_elements" => native.native_max_tensor_elements.to_string(),
        "native_largest_tensor_bytes" => native.native_largest_tensor_bytes.to_string(),
        "native_dtype_f16_count" => native.native_dtype_f16_count.to_string(),
        "native_dtype_bf16_count" => native.native_dtype_bf16_count.to_string(),
        "native_dtype_f32_count" => native.native_dtype_f32_count.to_string(),
        "native_dtype_i8_count" => native.native_dtype_i8_count.to_string(),
        "native_dtype_i16_count" => native.native_dtype_i16_count.to_string(),
        "native_dtype_i32_count" => native.native_dtype_i32_count.to_string(),
        "native_dtype_i64_count" => native.native_dtype_i64_count.to_string(),
        "native_dtype_u8_count" => native.native_dtype_u8_count.to_string(),
        "native_dtype_other_count" => native.native_dtype_other_count.to_string(),
        "native_invalid_file_count" => native.native_invalid_file_count.to_string(),
        "native_inventory_error_count" => native.native_inventory_error_count.to_string(),
        "source_tensor_count" => native.source_tensor_count.to_string(),
        "source_tensor_name_count" => native.source_tensor_name_count.to_string(),
        "source_tensor_file_count" => native.source_tensor_file_count.to_string(),
        "source_tensor_dtype_count" => native.source_tensor_dtype_count.to_string(),
        "source_tensor_rank_count" => native.source_tensor_rank_count.to_string(),
        "source_tensor_shape_count" => native.source_tensor_shape_count.to_string(),
        "source_tensor_declared_data_bytes" => native.source_tensor_declared_data_bytes.to_string(),
        "source_tensor_declared_tensor_bytes" => native.source_tensor_declared_tensor_bytes.to_string(),
        "source_tensor_total_elements" => native.source_tensor_total_elements.to_string(),
        "source_tensor_max_rank" => native.source_tensor_max_rank.to_string(),
        "source_tensor_max_elements" => native.source_tensor_max_elements.to_string(),
        "source_tensor_largest_elements" => native.source_tensor_largest_elements.to_string(),
        "source_tensor_largest_declared_bytes" => native.source_tensor_largest_declared_bytes.to_string(),
        "source_tensor_dtype_f16_count" => native.source_tensor_dtype_f16_count.to_string(),
        "source_tensor_dtype_bf16_count" => native.source_tensor_dtype_bf16_count.to_string(),
        "source_tensor_dtype_f32_count" => native.source_tensor_dtype_f32_count.to_string(),
        "source_tensor_dtype_i8_count" => native.source_tensor_dtype_i8_count.to_string(),
        "source_tensor_dtype_i16_count" => native.source_tensor_dtype_i16_count.to_string(),
        "source_tensor_dtype_i32_count" => native.source_tensor_dtype_i32_count.to_string(),
        "source_tensor_dtype_i64_count" => native.source_tensor_dtype_i64_count.to_string(),
        "source_tensor_dtype_u8_count" => native.source_tensor_dtype_u8_count.to_string(),
        "source_tensor_dtype_other_count" => native.source_tensor_dtype_other_count.to_string(),
        "source_tensor_rank_0_count" => native.source_tensor_rank_0_count.to_string(),
        "source_tensor_rank_1_count" => native.source_tensor_rank_1_count.to_string(),
        "source_tensor_rank_2_count" => native.source_tensor_rank_2_count.to_string(),
        "source_tensor_rank_3_count" => native.source_tensor_rank_3_count.to_string(),
        "source_tensor_rank_4_count" => native.source_tensor_rank_4_count.to_string(),
        "source_tensor_rank_other_count" => native.source_tensor_rank_other_count.to_string(),
        "source_tensor_name_embed_count" => native.source_tensor_name_embed_count.to_string(),
        "source_tensor_name_attn_count" => native.source_tensor_name_attn_count.to_string(),
        "source_tensor_name_mlp_count" => native.source_tensor_name_mlp_count.to_string(),
        "source_tensor_name_norm_count" => native.source_tensor_name_norm_count.to_string(),
        "source_tensor_name_lm_head_count" => native.source_tensor_name_lm_head_count.to_string(),
        "source_tensor_name_other_count" => native.source_tensor_name_other_count.to_string(),
        "source_tensor_metadata_error_count" => native.source_tensor_metadata_error_count.to_string(),
        "source_tensor_sample_count" => native.source_tensor_sample_count.to_string(),
        "largest_source_file_bytes" => native.largest_source_file_bytes.to_string(),
        "native_tensor_count" => native.native_tensor_count.to_string(),
        "source_tensor_largest_rank" => native.source_tensor_largest_rank.to_string(),
        "source_total_bytes" => native.total_size_bytes.to_string(),
        "shard_count" => native.safetensors_count.to_string(),
        "header_shard_count" => native.native_safetensors_header_read_count.to_string(),
        "header_tensor_count" => native.native_tensor_count.to_string(),
    }
}

fn audit_provenance(report: &SourceReport) -> Vec<(String, String)> {
    let native = &report.native;
    fields! {
        "source_artifact_status" => display_fact(report.state.clone(), "unknown"),
        "source_sidecar_status" => display_fact(report.semantics.sidecar_status.clone(), "unknown"),
        "source_tensor_payload_status" => display_fact(report.semantics.tensor_payload_status.clone(), "unknown"),
        "source_provenance_status" => display_fact(report.semantics.provenance_status.clone(), "unknown"),
        "source_origin" => display_fact(report.semantics.provenance_origin_audit.clone(), "unknown"),
        "source_authority" => display_fact(report.semantics.authority.clone(), "unknown"),
        "source_authority_status" => display_fact(report.semantics.authority_status.clone(), "unknown"),
        "source_path" => display_fact(ffi::text(&native.source_path), "unknown"),
        "source_path_source" => display_fact(ffi::text(&native.source_path_source), "unknown"),
        "source_path_status" => display_fact(report.state.clone(), "unknown"),
        "source_exists" => (native.source_exists != 0).to_string(),
        "download_registry_path" => display_fact(ffi::text(&native.download_registry_path), "unknown"),
        "download_report_path" => display_fact(ffi::text(&native.download_report_path), "unknown"),
        "download_repo_id" => display_fact(ffi::text(&native.identity_repo_id), "unknown"),
        "download_revision" => display_fact(ffi::text(&native.identity_revision), "unknown"),
        "source_footprint_class" => display_fact(report.semantics.footprint_class.clone(), "unknown"),
        "source_footprint_status" => display_fact(report.semantics.footprint_status.clone(), "unknown"),
        "largest_source_file_name" => display_fact(ffi::text(&native.largest_source_file_name), "none"),
        "source_manifest_status" => display_fact(report.semantics.manifest_status.clone(), "unknown"),
        "source_manifest_path" => display_fact(ffi::text(&native.manifest_path), "unknown"),
        "source_manifest_schema_status" => display_fact(report.semantics.manifest_schema_status.clone(), "unknown"),
        "source_manifest_schema_version" => display_fact(ffi::text(&native.manifest_schema_version), "unknown"),
        "source_manifest_source_path_status" => display_fact(report.state.clone(), "unknown"),
        "source_manifest_artifact_class_status" =>
            display_fact(report.semantics.manifest_artifact_class_status.clone(), "unknown"),
        "source_manifest_footprint_status" =>
            display_fact(report.semantics.manifest_footprint_status.clone(), "unknown"),
        "source_manifest_authority" => display_fact(report.semantics.manifest_authority.clone(), "unknown"),
        "source_manifest_provenance_status" =>
            display_fact(report.semantics.manifest_provenance_status.clone(), "unknown"),
        "source_manifest_native_inventory_status" =>
            display_fact(report.semantics.manifest_native_inventory_status.clone(), "unknown"),
        "source_manifest_tensor_metadata_status" =>
            display_fact(report.semantics.manifest_tensor_metadata_status.clone(), "unknown"),
        "source_manifest_consistency_status" =>
            display_fact(report.semantics.manifest_consistency_status.clone(), "unknown"),
        "source_manifest_hardening_status" =>
            display_fact(report.semantics.manifest_hardening_status.clone(), "unknown"),
        "source_manifest_creation_performed" => (native.semantics.manifest_creation_performed != 0).to_string(),
        "source_revision" => display_fact(ffi::text(&native.identity_revision), "unknown"),
        "source_revision_status" => display_fact(report.semantics.revision_status.clone(), "unknown"),
        "source_commit" => display_fact(ffi::text(&native.identity_revision), "unknown"),
        "source_commit_status" => display_fact(report.semantics.revision_status.clone(), "unknown"),
        "native_inventory_status" => display_fact(report.semantics.native_inventory_status.clone(), "unknown"),
        "native_inventory_source" => display_fact(report.semantics.native_inventory_source.clone(), "unknown"),
        "native_tensor_metadata_status" =>
            display_fact(report.semantics.native_tensor_metadata_status.clone(), "unknown"),
        "native_tensor_payload_status" =>
            display_fact(report.semantics.native_tensor_payload_status.clone(), "unknown"),
        "native_largest_tensor_name" => display_fact(ffi::text(&native.native_largest_tensor_name), "none"),
        "native_inventory_report_status" =>
            display_fact(report.semantics.native_inventory_report_status.clone(), "unknown"),
        "native_inventory_path" => display_fact(ffi::text(&native.native_inventory_path), "unknown"),
        "source_tensor_metadata_status" => display_fact(report.semantics.tensor_metadata_status.clone(), "unknown"),
        "source_tensor_metadata_source" => display_fact(report.semantics.tensor_metadata_source.clone(), "unknown"),
        "source_tensor_largest_name" => display_fact(ffi::text(&native.source_tensor_largest_name), "none"),
        "source_tensor_largest_file" => display_fact(ffi::text(&native.source_tensor_largest_file), "none"),
        "source_tensor_largest_dtype" => display_fact(ffi::text(&native.source_tensor_largest_dtype), "none"),
        "source_tensor_largest_shape" => display_fact(ffi::text(&native.source_tensor_largest_shape), "[]"),
        "tensor_map_path" => display_fact(ffi::text(&native.tensor_map_path), "unknown"),
        "tensor_map_status" => display_fact(report.semantics.tensor_map_report_status.clone(), "unknown"),
        "tensor_role_map_status" => display_fact(report.semantics.tensor_role_map_report_status.clone(), "unknown"),
        "output_head_map_path" => display_fact(ffi::text(&native.output_head_map_path), "unknown"),
        "output_head_map_status" => display_fact(report.semantics.output_head_map_report_status.clone(), "unknown"),
        "tokenizer_map_path" => display_fact(ffi::text(&native.tokenizer_map_path), "unknown"),
        "tokenizer_map_status" => display_fact(report.semantics.tokenizer_map_report_status.clone(), "unknown"),
        "status" => display_fact(report.status.clone(), "unknown"),
        "repository" => display_fact(ffi::text(&native.identity_repo_id), "unknown"),
        "revision" => display_fact(ffi::text(&native.identity_revision), "unknown"),
        "top_blocker" => display_fact(report.top_blocker.clone(), "unknown"),
        "next" => display_fact(report.next.clone(), "unknown"),
    }
}

fn audit_profile(report: &SourceReport) -> Vec<(String, String)> {
    fields! {
        "target_class" => display_fact(report.profile.target_class.clone(), "unknown"),
        "source_target_status" => display_fact(report.profile.source_target_status.clone(), "unknown"),
        "source_family_profile_status" => display_fact(report.profile.source_family_profile_status.clone(), "unknown"),
        "source_artifact_class" => display_fact(report.profile.source_artifact_class.clone(), "unknown"),
        "source_artifact_format" => display_fact(report.profile.source_artifact_format.clone(), "unknown"),
        "source_artifact_origin" => display_fact(report.profile.source_artifact_origin.clone(), "unknown"),
        "source_artifact_authority" => display_fact(report.profile.source_artifact_authority.clone(), "unknown"),
        "source_tensor_container" => display_fact(report.profile.source_tensor_container.clone(), "unknown"),
        "target_artifact_class" => display_fact(report.profile.target_artifact_class.clone(), "unknown"),
        "target_artifact_origin" => display_fact(report.profile.target_artifact_origin.clone(), "unknown"),
        "target_artifact_required" => display_fact(report.profile.target_artifact_required.clone(), "unknown"),
        "external_reference_status" => display_fact(report.profile.external_reference_status.clone(), "unknown"),
        "yvex_produced_artifact_status" =>
            display_fact(report.profile.yvex_produced_artifact_status.clone(), "planned"),
        "pressure_purpose" => display_fact(report.profile.pressure_purpose.clone(), "unknown"),
        "runtime_shape" => display_fact(report.profile.runtime_shape.clone(), "unknown"),
        "hardware_lane" => display_fact(report.profile.hardware_lane.clone(), "unknown"),
        "backend_lane" => display_fact(report.profile.backend_lane.clone(), "unknown"),
        "source_class" => display_fact(report.profile.source_class.clone(), "unknown"),
    }
}

fn report(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    admit_flags(
        invocation,
        &[
            "--family",
            "--release",
            "--source",
            "--models-root",
            "--target",
            "--include-files",
            "--include-config",
            "--include-blockers",
            "--include-next",
            "--include-tensors",
            "--strict",
            "--tensor-limit",
            "--audit",
            "--output",
            "--json",
        ],
    )?;
    let limit = invocation
        .value("--tensor-limit")
        .map(str::parse::<u64>)
        .transpose()
        .map_err(|_| grammar("tensor-limit requires a positive integer"))?
        .unwrap_or(0);
    if invocation.has("--tensor-limit") && limit == 0 {
        return Err(grammar("tensor-limit must be positive"));
    }
    let report = ffi::source_report(ReportRequest {
        family: require(invocation, "--family")?,
        release: require(invocation, "--release")?,
        root: invocation.value("--models-root"),
        source: invocation.value("--source"),
        target: invocation.value("--target"),
        files: invocation.has("--include-files"),
        config: invocation.has("--include-config"),
        blockers: invocation.has("--include-blockers"),
        next: invocation.has("--include-next"),
        tensors: invocation.has("--include-tensors"),
        strict: invocation.has("--strict"),
        limit,
    })?;
    let mode = invocation
        .ordered_flags
        .iter()
        .rev()
        .find_map(|(name, value)| match name.as_str() {
            "--json" => Some("json"),
            "--audit" => Some("audit"),
            "--output" => Some(value.as_str()),
            _ => None,
        })
        .unwrap_or("normal");
    if !["normal", "table", "audit", "json"].contains(&mode) {
        return Err(grammar("invalid source report output"));
    }
    let contract = contract(&report);
    let exit =
        u8::try_from(report.native.exit_code).map_err(|_| grammar("invalid native report exit"))?;
    if mode == "json" {
        return Ok(Output::standard(format!("{contract}\n"), exit));
    }
    let mut fields = scalar_fields(&contract);
    if mode == "audit" {
        fields.extend(audit_configuration(&report));
        fields.extend(audit_inventory(&report));
        fields.extend(audit_provenance(&report));
        fields.extend(audit_profile(&report));
        fields.extend(scalar_fields(&serde_json::to_value(&report.semantics)?));
        fields.extend(scalar_fields(&serde_json::to_value(&report.profile)?));
        fields.extend([
            ("target_id".into(), report.target.clone()),
            ("model".into(), report.semantics.model_name.clone()),
            ("next_required_rows".into(), report.next.clone()),
            (
                "canonical_repository".into(),
                known(&report.native.identity_repo_id),
            ),
            (
                "source_verification_status".into(),
                report.semantics.verification_status.clone(),
            ),
            (
                "source_manifest_path".into(),
                ffi::text(&report.native.manifest_path),
            ),
            ("release_qtype".into(), "unselected".into()),
            ("generation".into(), "unsupported-full-model".into()),
            (
                "config_dspark_target_layer_ids".into(),
                format!(
                    "[{},{},{}]",
                    report.native.verification.dspark_target_layer_ids[0],
                    report.native.verification.dspark_target_layer_ids[1],
                    report.native.verification.dspark_target_layer_ids[2]
                ),
            ),
            (
                "config_compress_ratios".into(),
                format!(
                    "[{}]",
                    report.native.verification.compress_ratios
                        [..report.native.verification.compress_ratio_count as usize]
                        .iter()
                        .map(u64::to_string)
                        .collect::<Vec<_>>()
                        .join(",")
                ),
            ),
        ]);
    } else {
        fields.retain(|(key, _)| {
            [
                "status",
                "family",
                "top_blocker",
                "next",
                "source_file_count",
                "header_tensor_count",
            ]
            .contains(&key.as_str())
        });
        fields.push((
            "inventory".into(),
            report.semantics.inventory_authority.clone(),
        ));
    }
    fields.push(("target".into(), report.target.clone()));
    fields.push(("source_state".into(), report.state.clone()));
    fields.push((
        "boundary".into(),
        "source report only; no artifact/runtime/generation/benchmark".into(),
    ));
    let fields = fields
        .into_iter()
        .collect::<std::collections::BTreeMap<_, _>>()
        .into_iter()
        .collect::<Vec<_>>();
    let mut output = render(
        &format!("SOURCE  {}", report.target),
        &fields,
        width,
        styled,
        exit,
    )?;
    if invocation.has("--include-tensors") {
        for sample in &report.native.source_tensor_samples
            [..report.native.source_tensor_sample_count as usize]
        {
            let name = ffi::text(&sample.name);
            output.text.push_str(&presentation::record(
                &format!("TENSOR  {name}"),
                &[
                    ("file", &ffi::text(&sample.file)),
                    ("dtype", &ffi::text(&sample.dtype)),
                    ("shape", &ffi::text(&sample.shape)),
                    ("elements", &sample.elements.to_string()),
                    ("declared_bytes", &sample.declared_bytes.to_string()),
                ],
                width,
                styled,
            )?);
        }
    }
    Ok(output)
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    if invocation.operation.operation_id == "source.native.inspect" {
        return tensors(invocation, width, styled);
    }
    if invocation.operation.operation_id == "source.payload.verify" {
        return verify(invocation, width, styled);
    }
    match invocation.positionals[0].as_str() {
        "create" => create(invocation, width, styled),
        "verify" => verify(invocation, width, styled),
        "report" => report(invocation, width, styled),
        "inspect" => Ok(Output {
            text: "source-manifest inspect is not implemented in open-weight intake\n".into(),
            exit: 5,
            diagnostic: true,
        }),
        _ => Err(grammar("unknown source-manifest action")),
    }
}

fn tensors(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let source = require(invocation, "--source")?;
    let limit = invocation
        .value("--limit")
        .map(str::parse::<u64>)
        .transpose()
        .map_err(|_| grammar("native tensor limit must be a non-negative u64"))?
        .unwrap_or(20);
    let json = invocation.has("--json");
    // The established machine contract is the whole inventory summary, not the human row filter.
    let report = ffi::native_weights(
        source,
        if json {
            None
        } else {
            invocation.value("--tensor")
        },
        if json { 0 } else { limit },
    )?;
    let summary = &report.summary;
    if json {
        let value = json!({ "schema": "yvex.native_weights.v1", "source": source, "summary": {
            "shard_count": summary.shard_count, "tensor_count": summary.tensor_count,
            "total_tensor_bytes": summary.total_tensor_bytes, "unknown_dtype_count": summary.unknown_dtype_count,
            "malformed_shard_count": summary.malformed_shard_count,
        }});
        return Ok(Output::standard(format!("{value}\n"), 0));
    }
    let mut output = render(
        "SOURCE TENSORS  safetensors",
        &[
            ("source".into(), source.into()),
            ("shards".into(), summary.shard_count.to_string()),
            ("tensors".into(), summary.tensor_count.to_string()),
            (
                "total_tensor_bytes".into(),
                summary.total_tensor_bytes.to_string(),
            ),
            (
                "unknown_dtype_count".into(),
                summary.unknown_dtype_count.to_string(),
            ),
            (
                "malformed_shard_count".into(),
                summary.malformed_shard_count.to_string(),
            ),
            (
                "status".into(),
                if summary.shard_count == 0 {
                    "native-weights-empty"
                } else {
                    "native-weights"
                }
                .into(),
            ),
        ],
        width,
        styled,
        0,
    )?;
    for tensor in report.tensors {
        output.text.push_str(&presentation::record(
            &format!("TENSOR  {}", tensor.name),
            &[
                ("shard", &tensor.shard),
                ("dtype", &tensor.dtype),
                ("rank", &tensor.dimensions.len().to_string()),
                ("shape", &format!("{:?}", tensor.dimensions)),
                ("bytes", &tensor.bytes.to_string()),
                ("offsets", &format!("[{},{}]", tensor.begin, tensor.end)),
            ],
            width,
            styled,
        )?);
    }
    Ok(output)
}
