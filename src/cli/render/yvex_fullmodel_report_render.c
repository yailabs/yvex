/*
 * yvex_fullmodel_report_surface.c - fullmodel report CLI surface.
 * Owner: src/cli/render
 * Owns: fullmodel report-family formatting for existing command behavior.
 * Does not own: runtime generation, graph execution, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing output fields.
 * Boundary: report output does not promote runtime capability.
 */
#include "yvex_fullmodel_surface.h"

static void fullmodel_print_family_runtime_phase(unsigned int index,
                                                 const char *name,
                                                 const char *status)
{
    yvex_cli_out_writef(stdout, "family_runtime_phase.%u.name: %s\n", index, name ? name : "");
    yvex_cli_out_writef(stdout, "family_runtime_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void fullmodel_print_family_runtime_phases(const char *adapter_status,
                                                  const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-descriptor",
        "family-profile",
        "role-adapter",
        "collection-adapter",
        "attention-rules",
        "position-rules",
        "kv-rules",
        "moe-rules",
        "mlp-rules",
        "output-head-rules",
        "tokenizer-rules",
        "graph-requirements",
        "runtime-phase-blockers",
        "adapter-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "skipped";
        } else if (strcmp(phases[i], "role-adapter") == 0 ||
                   strcmp(phases[i], "collection-adapter") == 0) {
            status = adapter_status ? adapter_status : "partial";
        } else if (strcmp(phases[i], "attention-rules") == 0 ||
                   strcmp(phases[i], "position-rules") == 0 ||
                   strcmp(phases[i], "kv-rules") == 0 ||
                   strcmp(phases[i], "moe-rules") == 0 ||
                   strcmp(phases[i], "mlp-rules") == 0 ||
                   strcmp(phases[i], "output-head-rules") == 0 ||
                   strcmp(phases[i], "tokenizer-rules") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "runtime-phase-blockers") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "skipped";
        }
        fullmodel_print_family_runtime_phase(i, phases[i], status);
    }
}

const char *fullmodel_detect_family(const yvex_cli_fullmodel_options *options,
                                           yvex_arch arch,
                                           const char *target_id)
{
    const char *from_arch = fullmodel_family_from_arch(arch);

    if (from_arch && strcmp(from_arch, "unknown") != 0) return from_arch;
    if (target_id && fullmodel_name_has(target_id, "deepseek")) return "deepseek";
    if (options && options->model && fullmodel_name_has(options->model, "deepseek")) return "deepseek";
    if (target_id && fullmodel_name_has(target_id, "glm")) return "glm";
    if (options && options->model && fullmodel_name_has(options->model, "glm")) return "glm";
    if (target_id && fullmodel_name_has(target_id, "qwen")) return "qwen";
    if (options && options->model && fullmodel_name_has(options->model, "qwen")) return "qwen";
    return "unknown";
}

const char *fullmodel_requested_family(const yvex_cli_fullmodel_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

int fullmodel_family_request_matches(const char *requested,
                                            const char *detected)
{
    if (!requested || !requested[0] || strcmp(requested, "auto") == 0) {
        return detected && strcmp(detected, "unknown") != 0;
    }
    return detected && strcmp(requested, detected) == 0;
}

const char *fullmodel_role_status_from_tensor(yvex_cli_tokenizer_context *ctx,
                                                     const yvex_fullmodel_collections *collections,
                                                     const char *role)
{
    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        return collections && collections->has_tokenizer_metadata ? "present" : "missing";
    }
    return fullmodel_descriptor_find_tensor(ctx, role) ? "present" : "missing";
}

static const char *fullmodel_attention_rule_status(const yvex_fullmodel_collections *collections)
{
    if (!collections ||
        !collections->has_attention_q ||
        !collections->has_attention_k ||
        !collections->has_attention_v ||
        !collections->has_attention_out) {
        return "blocked-missing-qkv";
    }
    return "blocked-full-transformer-integration";
}

int fullmodel_print_family_runtime_report(const yvex_cli_fullmodel_options *options,
                                                 yvex_model_ref *ref,
                                                 yvex_cli_tokenizer_context *ctx,
                                                 const char *target_id,
                                                 const char *target_class,
                                                 unsigned long long artifact_bytes,
                                                 yvex_arch arch,
                                                 unsigned long long tensor_count,
                                                 unsigned long long total_tensor_bytes,
                                                 const yvex_fullmodel_collections *collections,
                                                 const char *role_coverage,
                                                 const char *missing_roles,
                                                 const char *unsupported_roles,
                                                 int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = fullmodel_requested_family(options);
    const char *detected = fullmodel_detect_family(options, arch, target_id);
    const char *adapter_status = selected_target ? "partial" :
                                 (role_coverage && strcmp(role_coverage, "complete") == 0
                                      ? "complete"
                                      : "partial");
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);
    int has_output = collections && collections->has_output_head;
    int supported_family = fullmodel_family_request_matches(requested, detected) &&
                           strcmp(detected, "deepseek") == 0;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    yvex_cli_out_writef(stdout, "family_runtime: report\n");
    if (!supported_family) {
        const char *status = strcmp(detected, "unknown") == 0
                                 ? "fullmodel-family-runtime-fail"
                                 : "fullmodel-family-runtime-unsupported";
        yvex_cli_out_writef(stdout, "status: %s\n", status);
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
        yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend);
        yvex_cli_out_writef(stdout, "family: %s\n", detected);
        yvex_cli_out_writef(stdout, "family_detected: %s\n", detected);
        yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
        yvex_cli_out_writef(stdout, "family_adapter: unsupported\n");
        yvex_cli_out_writef(stdout, "family_adapter_status: unsupported\n");
        yvex_cli_out_writef(stdout, "family_runtime_stage: report-only\n");
        yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
        yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
        yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
        yvex_cli_out_writef(stdout, "descriptor_status: unavailable\n");
        yvex_cli_out_writef(stdout, "descriptor_source: fullmodel-descriptor-facts\n");
        yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
        yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
        yvex_cli_out_writef(stdout, "generation_ready: false\n");
        yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
        yvex_cli_out_writef(stdout, "runtime_blockers: unsupported or unknown runtime family adapter\n");
        yvex_cli_out_writef(stdout, "next_required_rows: FAMILY.RUNTIME.0,ATTENTION.CLASS.0,CONTEXT.CLASS.0,KV.CACHE.0,MOE.CLASS.0,family-specific-runtime-target\n");
        yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
        yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
        fullmodel_print_family_runtime_phases("unsupported", "resolve-family");
        yvex_cli_out_writef(stdout, "reason: requested family is not supported by family-runtime report\n");
        return 5;
    }

    yvex_cli_out_writef(stdout, "status: fullmodel-family-runtime\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "family: deepseek\n");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", detected);
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
    yvex_cli_out_writef(stdout, "family_adapter: deepseek-runtime-report\n");
    yvex_cli_out_writef(stdout, "family_adapter_status: %s\n", adapter_status);
    yvex_cli_out_writef(stdout, "family_runtime_stage: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");

    yvex_cli_out_writef(stdout, "descriptor_status: %s\n", adapter_status);
    yvex_cli_out_writef(stdout, "descriptor_source: fullmodel-descriptor-facts\n");
    yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));

    yvex_cli_out_writef(stdout, "role_adapter_status: %s\n", adapter_status);
    yvex_cli_out_writef(stdout, "collection_adapter_status: %s\n", adapter_status);
    yvex_cli_out_writef(stdout, "attention_rule_status: %s\n", fullmodel_attention_rule_status(collections));
    yvex_cli_out_writef(stdout, "attention_rules: %s\n", fullmodel_attention_rule_status(collections));
    yvex_cli_out_writef(stdout, "position_rule_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_rule_status: blocked\n");
    yvex_cli_out_writef(stdout, "moe_rule_status: blocked\n");
    yvex_cli_out_writef(stdout, "mlp_rule_status: %s\n", has_mlp ? "blocked-full-transformer-integration" : "blocked-missing-mlp");
    yvex_cli_out_writef(stdout, "output_head_rule_status: %s\n", has_output ? "blocked-logits-runtime" : "blocked-missing-output-head");
    yvex_cli_out_writef(stdout, "tokenizer_rule_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked-missing-tokenizer-metadata");
    yvex_cli_out_writef(stdout, "graph_requirement_status: blocked\n");
    yvex_cli_out_writef(stdout, "runtime_blocker_status: blocked\n");

    yvex_cli_out_writef(stdout, "token_embedding_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "token_embedding"));
    yvex_cli_out_writef(stdout, "attention_norm_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "attention_norm"));
    yvex_cli_out_writef(stdout, "post_attention_norm_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "post_attention_norm"));
    yvex_cli_out_writef(stdout, "final_norm_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "final_norm"));
    yvex_cli_out_writef(stdout, "q_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "q_projection"));
    yvex_cli_out_writef(stdout, "k_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "k_projection"));
    yvex_cli_out_writef(stdout, "v_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "v_projection"));
    yvex_cli_out_writef(stdout, "o_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "o_projection"));
    yvex_cli_out_writef(stdout, "mlp_gate_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "mlp_gate"));
    yvex_cli_out_writef(stdout, "mlp_up_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "mlp_up"));
    yvex_cli_out_writef(stdout, "mlp_down_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "mlp_down"));
    yvex_cli_out_writef(stdout, "moe_router_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "moe_router"));
    yvex_cli_out_writef(stdout, "moe_expert_roles: %s\n",
           collections && collections->has_moe_expert ? "present" : "missing");
    yvex_cli_out_writef(stdout, "output_head_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "output_head"));
    yvex_cli_out_writef(stdout, "tokenizer_metadata_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "tokenizer_metadata"));

    yvex_cli_out_writef(stdout, "attention_family: deepseek-family-attention-planned\n");
    yvex_cli_out_writef(stdout, "attention_type: unknown-family-specific\n");
    yvex_cli_out_writef(stdout, "attention_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "attention_q_required: true\n");
    yvex_cli_out_writef(stdout, "attention_k_required: true\n");
    yvex_cli_out_writef(stdout, "attention_v_required: true\n");
    yvex_cli_out_writef(stdout, "attention_o_required: true\n");
    yvex_cli_out_writef(stdout, "rope_required: true\n");
    yvex_cli_out_writef(stdout, "rope_status: planned\n");
    yvex_cli_out_writef(stdout, "rope_base: unknown\n");
    yvex_cli_out_writef(stdout, "rope_scaling: unknown\n");
    yvex_cli_out_writef(stdout, "mask_required: true\n");
    yvex_cli_out_writef(stdout, "mask_rule: causal-or-family-specific-planned\n");
    yvex_cli_out_writef(stdout, "context_policy: planned\n");
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");

    yvex_cli_out_writef(stdout, "kv_required: true\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported-full-transformer-kv\n");
    yvex_cli_out_writef(stdout, "kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_read_ready: false\n");

    yvex_cli_out_writef(stdout, "moe_required: family-specific-planned\n");
    yvex_cli_out_writef(stdout, "router_required: family-specific-planned\n");
    yvex_cli_out_writef(stdout, "router_present: %s\n", collections && collections->has_moe_router ? "true" : "false");
    yvex_cli_out_writef(stdout, "moe_router_present: %s\n", collections && collections->has_moe_router ? "true" : "false");
    yvex_cli_out_writef(stdout, "expert_tensors_present: %s\n", collections && collections->has_moe_expert ? "true" : "false");
    yvex_cli_out_writef(stdout, "moe_expert_count: unknown\n");
    yvex_cli_out_writef(stdout, "expert_count: unknown\n");
    yvex_cli_out_writef(stdout, "moe_active_expert_count: unknown\n");
    yvex_cli_out_writef(stdout, "active_expert_count: unknown\n");
    yvex_cli_out_writef(stdout, "moe_shared_experts: unknown\n");
    yvex_cli_out_writef(stdout, "shared_expert_status: planned\n");
    yvex_cli_out_writef(stdout, "moe_dispatch_ready: false\n");
    yvex_cli_out_writef(stdout, "moe_blockers: router logits, top-k routing, expert dispatch, and expert accumulation are not implemented\n");

    yvex_cli_out_writef(stdout, "output_head_required: true\n");
    yvex_cli_out_writef(stdout, "output_head_present: %s\n", has_output ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    yvex_cli_out_writef(stdout, "vocab_size: %s\n", has_output ? "from-output-head-shape" : "unknown");
    yvex_cli_out_writef(stdout, "logits_projection_ready: false\n");
    yvex_cli_out_writef(stdout, "real_output_head_logits: false\n");
    yvex_cli_out_writef(stdout, "logits_blockers: real output-head logits runtime unsupported\n");

    yvex_cli_out_writef(stdout, "required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp,moe-router,expert-dispatch,final-norm,output-head-projection\n");
    yvex_cli_out_writef(stdout, "implemented_graph_primitives: rope,attention-fixture,matmul,mlp-fixture,controlled-block,controlled-layers,selected-embedding,selected-rmsnorm-segment\n");
    yvex_cli_out_writef(stdout, "unsupported_graph_ops: full-attention-from-model-tensors,full-transformer-block-from-model-tensors,full-layer-stack,real-moe-router,real-expert-dispatch,real-output-head-projection\n");
    yvex_cli_out_writef(stdout, "graph.rope_primitive: implemented\n");
    yvex_cli_out_writef(stdout, "graph.attention_primitive: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "graph.matmul_primitive: implemented\n");
    yvex_cli_out_writef(stdout, "graph.mlp_primitive: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "graph.full_attention_from_model_tensors: unsupported\n");
    yvex_cli_out_writef(stdout, "graph.full_transformer_block_from_model_tensors: unsupported\n");
    yvex_cli_out_writef(stdout, "graph.full_layer_stack: unsupported\n");
    yvex_cli_out_writef(stdout, "graph.full_transformer_attention: %s\n", has_attention ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "full_transformer_graph_ready: false\n");

    yvex_cli_out_writef(stdout, "backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    yvex_cli_out_writef(stdout, "backend_available: %s\n", fit.available ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend_memory_known: %s\n", fit.memory_known ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend_required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend_fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");

    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_blockers: %s\n",
           selected_target
               ? "selected runtime slice is incomplete; attention Q/K/V/O tensors missing; output head missing; real transformer prefill unsupported; real attention-backed KV unsupported; real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported"
               : "real transformer prefill unsupported; real attention-backed KV unsupported; real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported");
    yvex_cli_out_writef(stdout, "next_required_rows: ATTENTION.CLASS.0,CONTEXT.CLASS.0,KV.CACHE.0,MOE.CLASS.0,FAMILY.RUNTIME.DeepSeek.detail,real-transformer-prefill,real-decode,real-output-head-logits,real-vocabulary-sampling,GEN.DEEPSEEK.0\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
    fullmodel_print_family_runtime_phases(adapter_status, NULL);
    return 0;
}

static void fullmodel_print_materialize_phase(unsigned int index,
                                              const char *name,
                                              const char *status)
{
    yvex_cli_out_writef(stdout, "materialize_phase.%u.name: %s\n", index, name ? name : "");
    yvex_cli_out_writef(stdout, "materialize_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void fullmodel_print_materialize_phase_set(const char *terminal_phase,
                                                  const char *failed_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-coverage",
        "placement-plan",
        "memory-budget",
        "backend-preflight",
        "materialize-embedding",
        "materialize-normalization",
        "materialize-attention",
        "materialize-mlp",
        "materialize-moe",
        "materialize-output",
        "materialize-tokenizer",
        "cleanup",
        "complete"
    };
    int failed_seen = 0;
    unsigned int i;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "planned";
        if (failed_phase && failed_phase[0] && strcmp(failed_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (!failed_seen &&
                   terminal_phase &&
                   (strcmp(terminal_phase, "complete") == 0 ||
                    strcmp(terminal_phase, phases[i]) == 0)) {
            status = "pass";
        } else if (failed_seen) {
            status = "skipped";
        }
        if (strcmp(phases[i], "materialize-moe") == 0 && !failed_seen) {
            status = "skipped";
        }
        fullmodel_print_materialize_phase(i, phases[i], status);
    }
}

void fullmodel_print_materialize_report(const yvex_fullmodel_materialize_report *report)
{
    const yvex_cli_fullmodel_options *options = report ? report->options : NULL;

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "fullmodel-materialize: %s model=%s backend=%s\n",
               report && report->status ? report->status : "fullmodel-materialize-fail",
               options->model ? options->model : "",
               options->backend ? options->backend : "cpu");
        yvex_cli_out_writef(stdout, "reason: %s\n", report && report->failed_reason ? report->failed_reason : "none");
        yvex_cli_out_writef(stdout, "bytes: materialized=%llu required=%llu\n",
               report ? report->materialized_tensor_bytes : 0ull,
               report ? report->required_tensor_bytes : 0ull);
        yvex_cli_out_writef(stdout, "cleanup: %s\n", report && report->cleanup_status ? report->cleanup_status : "not-needed");
        yvex_cli_out_writef(stdout, "boundary: bounded proof/refusal only, no full model execution\n");
        yvex_cli_out_writef(stdout, "status: %s\n", report && report->status ? report->status : "fullmodel-materialize-fail");
        return;
    }

    yvex_cli_out_writef(stdout, "fullmodel: materialize\n");
    yvex_cli_out_writef(stdout, "status: %s\n", report && report->status ? report->status : "fullmodel-materialize-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", report && report->model_resolved_path ? report->model_resolved_path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", report && report->target_id ? report->target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", report && report->target_class ? report->target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "dry_run: %s\n", options && options->dry_run ? "true" : "false");
    yvex_cli_out_writef(stdout, "plan_only: %s\n", options && options->plan_only ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_dir: %s\n", options && options->report_dir ? options->report_dir : "none");
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", report && report->artifact_identity_status ? report->artifact_identity_status : "not-checked");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: %s\n", report && report->tensor_inventory_status ? report->tensor_inventory_status : "unknown");
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n", report && report->required_role_coverage ? report->required_role_coverage : "partial");
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", report && report->missing_required_roles ? report->missing_required_roles : "unknown");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", report && report->unsupported_required_roles ? report->unsupported_required_roles : "runtime-family-adapter,real-transformer-prefill,real-attention-backed-KV,real-DeepSeek-decode,real-output-head-logits,real-vocabulary-sampling");
    yvex_cli_out_writef(stdout, "placement_plan_status: %s\n", report && report->placement_plan_status ? report->placement_plan_status : "unknown");
    yvex_cli_out_writef(stdout, "memory_budget_status: %s\n", report && report->memory_budget_status ? report->memory_budget_status : "unknown");
    yvex_cli_out_writef(stdout, "backend_preflight_status: %s\n", report && report->backend_preflight_status ? report->backend_preflight_status : "unknown");
    yvex_cli_out_writef(stdout, "materialization_mode: %s\n", report && report->materialization_mode ? report->materialization_mode : "none");
    yvex_cli_out_writef(stdout, "full_model_materialization: %s\n", report && report->full_model_materialization ? report->full_model_materialization : "failed");
    yvex_cli_out_writef(stdout, "full_model_materialization_proof: %s\n", report && report->full_model_materialization_proof ? report->full_model_materialization_proof : "fail");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "phase: %s\n", report && report->phase ? report->phase : "failed");
    yvex_cli_out_writef(stdout, "failed_phase: %s\n", report && report->failed_phase ? report->failed_phase : "none");
    yvex_cli_out_writef(stdout, "failed_reason: %s\n", report && report->failed_reason ? report->failed_reason : "none");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n", report && report->cleanup_attempted ? report->cleanup_attempted : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n", report && report->cleanup_status ? report->cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "cleanup_idempotent: %s\n", report && report->cleanup_idempotent ? report->cleanup_idempotent : "true");
    yvex_cli_out_writef(stdout, "owned_state_released: %s\n", report && report->owned_state_released ? report->owned_state_released : "true");
    yvex_cli_out_writef(stdout, "partial_materialization: %s\n", report && report->partial_materialization ? report->partial_materialization : "false");
    yvex_cli_out_writef(stdout, "materialized_tensor_count: %llu\n", report ? report->materialized_tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "materialized_tensor_bytes: %llu\n", report ? report->materialized_tensor_bytes : 0ull);
    yvex_cli_out_writef(stdout, "refused_tensor_count: %llu\n", report ? report->refused_tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "skipped_tensor_count: %llu\n", report ? report->skipped_tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "required_tensor_count: %llu\n", report ? report->required_tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "required_tensor_bytes: %llu\n", report ? report->required_tensor_bytes : 0ull);
    yvex_cli_out_writef(stdout, "peak_planned_bytes: %llu\n", report ? report->peak_planned_bytes : 0ull);
    yvex_cli_out_writef(stdout, "cpu_resident_bytes: %llu\n", report ? report->cpu_resident_bytes : 0ull);
    yvex_cli_out_writef(stdout, "cuda_resident_bytes: %llu\n", report ? report->cuda_resident_bytes : 0ull);
    yvex_cli_out_writef(stdout, "residency_plan: %s\n", report && report->residency_plan ? report->residency_plan : "not-planned");
    yvex_cli_out_writef(stdout, "runtime_blockers: %s\n", report && report->runtime_blockers ? report->runtime_blockers : "runtime family adapter not implemented");
    fullmodel_print_materialize_phase_set(report && report->phase ? report->phase : "failed",
                                          report && report->failed_phase ? report->failed_phase : NULL);
}

void fullmodel_print_report_normal(const yvex_cli_fullmodel_options *options,
                                          const char *status,
                                          const char *target_id,
                                          const char *target_class,
                                          const char *role_coverage,
                                          const char *top_blocker,
                                          const char *next)
{
    yvex_cli_out_writef(stdout, "fullmodel: report model=%s backend=%s\n",
           options && options->model ? options->model : "",
           options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "report-only");
    yvex_cli_out_writef(stdout, "target: %s class=%s\n",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n", top_blocker ? top_blocker : "missing-full-runtime-tensor-coverage");
    yvex_cli_out_writef(stdout, "next: %s\n", next ? next : "tensor/source/artifact row required");
    yvex_cli_out_writef(stdout, "boundary: report-only, no full model execution\n");
}

void fullmodel_print_plan_normal(const yvex_cli_fullmodel_options *options,
                                        const char *status,
                                        const char *target_class,
                                        const char *fit,
                                        const char *top_blocker)
{
    yvex_cli_out_writef(stdout, "materialization-plan: %s model=%s backend=%s\n",
           status ? status : "blocked",
           options && options->model ? options->model : "",
           options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "residency: %s\n", options && options->residency ? options->residency : "resident");
    yvex_cli_out_writef(stdout, "class: %s\n", target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "fit: %s\n", fit ? fit : "unknown");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n", top_blocker ? top_blocker : "full-runtime candidate artifact required");
    yvex_cli_out_writef(stdout, "boundary: plan-only, no materialization\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-materialization-plan\n");
}

void fullmodel_print_descriptor_normal(const yvex_cli_fullmodel_options *options,
                                              const char *target_id,
                                              const char *target_class,
                                              const char *role_coverage,
                                              const char *missing_roles)
{
    yvex_cli_out_writef(stdout, "report: fullmodel-descriptor\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "backend: %s target=%s class=%s\n",
           options && options->backend ? options->backend : "cpu",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n",
           missing_roles && strcmp(missing_roles, "none") == 0
               ? "runtime integration missing"
               : "missing-full-runtime-tensor-coverage");
    yvex_cli_out_writef(stdout, "boundary: descriptor report-only, no runtime execution\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-descriptor\n");
}

void fullmodel_print_family_runtime_normal(const yvex_cli_fullmodel_options *options,
                                                  const char *target_id,
                                                  const char *target_class,
                                                  const char *role_coverage,
                                                  const char *missing_roles)
{
    yvex_cli_out_writef(stdout, "report: family-runtime\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "family: %s backend=%s\n",
           options && options->family ? options->family : "auto",
           options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "target: %s class=%s\n",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "status: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n",
           missing_roles && strcmp(missing_roles, "none") == 0
               ? "runtime family adapter missing"
               : "missing family runtime tensor roles");
    yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
}

int print_fullmodel_source_only_report(const char *target,
                                              const char *backend)
{
    yvex_cli_out_writef(stdout, "fullmodel: report\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-report-unsupported\n");
    yvex_cli_out_writef(stdout, "model: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "target_class: official-source-huge-model\n");
    yvex_cli_out_writef(stdout, "source_artifact_class: official safetensors\n");
    yvex_cli_out_writef(stdout, "target_artifact_class: future YVEX-produced GGUF\n");
    yvex_cli_out_writef(stdout, "artifact_exists: false\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: 0\n");
    yvex_cli_out_writef(stdout, "artifact_identity_status: not-applicable\n");
    yvex_cli_out_writef(stdout, "tensor_count: 0\n");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "metadata_status: not-performed\n");
    yvex_cli_out_writef(stdout, "architecture: glm\n");
    yvex_cli_out_writef(stdout, "family: glm\n");
    yvex_cli_out_writef(stdout, "model_class: huge-MoE-source-target\n");
    yvex_cli_out_writef(stdout, "fullmodel_inventory: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "qtype_summary: none\n");
    yvex_cli_out_writef(stdout, "dtype_summary: none\n");
    yvex_cli_out_writef(stdout, "total_tensor_bytes: 0\n");
    yvex_cli_out_writef(stdout, "estimated_cpu_resident_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "estimated_cuda_resident_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "estimated_kv_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_scratch_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_total_runtime_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend ? backend : "cpu");
    yvex_cli_out_writef(stdout, "backend_placement_status: not-performed\n");
    yvex_cli_out_writef(stdout, "cpu_placement: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "cuda_placement: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "cuda_memory_status: unavailable\n");
    yvex_cli_out_writef(stdout, "residency_plan: future-YVEX-produced-artifact-required\n");
    yvex_cli_out_writef(stdout, "tensor_collections_status: not-performed\n");
    yvex_cli_out_writef(stdout, "collection_detected: no\n");
    yvex_cli_out_writef(stdout, "collection_supported: false\n");
    yvex_cli_out_writef(stdout, "runtime_consumer: unsupported\n");
    yvex_cli_out_writef(stdout, "embedding_tensors: 0\n");
    yvex_cli_out_writef(stdout, "normalization_tensors: 0\n");
    yvex_cli_out_writef(stdout, "attention_tensors: 0\n");
    yvex_cli_out_writef(stdout, "kv_cache_requirements: planned\n");
    yvex_cli_out_writef(stdout, "mlp_tensors: 0\n");
    yvex_cli_out_writef(stdout, "moe_tensors: 0\n");
    yvex_cli_out_writef(stdout, "output_tensors: 0\n");
    yvex_cli_out_writef(stdout, "tokenizer_tensors: 0\n");
    yvex_cli_out_writef(stdout, "required_role_coverage: none\n");
    yvex_cli_out_writef(stdout, "missing_required_roles: YVEX-produced-GGUF-artifact\n");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution\n");
    yvex_cli_out_writef(stdout, "runtime_blockers: source-only target; YVEX-produced GGUF emission planned; full model runtime unsupported\n");
    print_fullmodel_common_boundaries();
    return 0;
}

void fullmodel_print_phase(unsigned int index,
                                  const char *name,
                                  const char *status,
                                  unsigned long long tensor_count,
                                  unsigned long long tensor_bytes,
                                  const char *residency,
                                  int required,
                                  int blocked,
                                  const char *blocker);
unsigned int fullmodel_print_blocker(unsigned int index,
                                            const char *category,
                                            const char *severity,
                                            const char *message,
                                            int blocks_full_materialization,
                                            int blocks_generation);

int print_fullmodel_source_only_plan(const yvex_cli_fullmodel_options *options,
                                            const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";

    yvex_cli_out_writef(stdout, "fullmodel: materialization-plan\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-materialization-plan-unsupported\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "target_class: official-source-huge-model\n");
    yvex_cli_out_writef(stdout, "artifact_exists: false\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: 0\n");
    yvex_cli_out_writef(stdout, "artifact_identity_status: not-applicable\n");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "tensor_count: 0\n");
    yvex_cli_out_writef(stdout, "total_tensor_bytes: 0\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "residency: %s\n", residency);
    yvex_cli_out_writef(stdout, "plan_status: unsupported\n");
    yvex_cli_out_writef(stdout, "materialization_plan_ready: false\n");
    yvex_cli_out_writef(stdout, "materialization_attempted: false\n");
    yvex_cli_out_writef(stdout, "full_materialization_proof: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "plan_id: fullmodel-materialization:%s:%s:%s\n",
           target ? target : "source-only-target", backend, residency);
    yvex_cli_out_writef(stdout, "plan_kind: full-model-materialization\n");
    yvex_cli_out_writef(stdout, "plan_source: source-target-without-YVEX-GGUF\n");
    yvex_cli_out_writef(stdout, "plan_backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "plan_residency: %s\n", residency);
    yvex_cli_out_writef(stdout, "plan_tensor_count: 0\n");
    yvex_cli_out_writef(stdout, "plan_tensor_bytes: 0\n");
    yvex_cli_out_writef(stdout, "plan_collection_count: 0\n");
    yvex_cli_out_writef(stdout, "plan_phase_count: 1\n");
    yvex_cli_out_writef(stdout, "plan_blocker_count: 1\n");
    yvex_cli_out_writef(stdout, "plan_cleanup_required: false\n");
    yvex_cli_out_writef(stdout, "plan_cleanup_phases: none\n");
    yvex_cli_out_writef(stdout, "backend_available: unknown\n");
    yvex_cli_out_writef(stdout, "backend_memory_known: false\n");
    yvex_cli_out_writef(stdout, "backend_memory_total_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend_memory_available_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend_required_bytes: 0\n");
    yvex_cli_out_writef(stdout, "backend_fit_status: unsupported\n");
    yvex_cli_out_writef(stdout, "backend_fit_reason: source-only target has no YVEX-produced GGUF tensor inventory\n");
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");
    fullmodel_print_phase(0u, "preflight", "unsupported",
                          0ull, 0ull, "not-applicable", 1, 1,
                          "YVEX-produced GGUF artifact required before planning");
    (void)fullmodel_print_blocker(0u, "artifact", "fatal",
                                  "YVEX-produced GGUF artifact required before materialization planning",
                                  1, 1);
    yvex_cli_out_writef(stdout, "cleanup_plan_required: false\n");
    yvex_cli_out_writef(stdout, "cleanup_plan_phases: none\n");
    yvex_cli_out_writef(stdout, "cleanup_idempotent_required: true\n");
    yvex_cli_out_writef(stdout, "cleanup_failure_policy: preserve-failure-report\n");
    yvex_cli_out_writef(stdout, "next_required_row: FULLMODEL.2\n");
    yvex_cli_out_writef(stdout, "proof_ready_for_fullmodel_2: false\n");
    yvex_cli_out_writef(stdout, "fullmodel_2_blockers: YVEX-produced GGUF artifact missing; full tensor inventory unavailable\n");
    return 5;
}

int print_fullmodel_source_only_materialize(const yvex_cli_fullmodel_options *options,
                                                   const char *target)
{
    yvex_fullmodel_materialize_report report;

    memset(&report, 0, sizeof(report));
    report.options = options;
    report.status = "fullmodel-materialize-unsupported";
    report.model_resolved_path = "source-only-target";
    report.target_id = target ? target : "source-only-target";
    report.target_class = "official-source-huge-model";
    report.artifact_identity_status = "not-applicable";
    report.tensor_inventory_status = "not-performed-source-only-target";
    report.required_role_coverage = "none";
    report.missing_required_roles = "YVEX-produced-GGUF-artifact";
    report.unsupported_required_roles = "GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution";
    report.placement_plan_status = "unsupported";
    report.memory_budget_status = "not-performed";
    report.backend_preflight_status = "not-performed";
    report.materialization_mode = "source-only-refusal";
    report.full_model_materialization = "unsupported-source-only";
    report.full_model_materialization_proof = "unsupported";
    report.phase = "failed";
    report.failed_phase = "resolve-model";
    report.failed_reason = "YVEX-produced-GGUF-artifact-missing";
    report.cleanup_attempted = "false";
    report.cleanup_status = "not-needed";
    report.cleanup_idempotent = "true";
    report.owned_state_released = "true";
    report.partial_materialization = "false";
    report.residency_plan = "future-YVEX-produced-artifact-required";
    report.runtime_blockers = "source-only target; YVEX-produced GGUF emission planned; full model runtime unsupported";
    fullmodel_print_materialize_report(&report);
    return 5;
}

int print_fullmodel_source_only_descriptor(const yvex_cli_fullmodel_options *options,
                                                  const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";

    yvex_cli_out_writef(stdout, "fullmodel: descriptor\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-descriptor-unsupported\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "target_class: official-source-huge-model\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "format: text\n");
    yvex_cli_out_writef(stdout, "artifact_identity_status: not-applicable\n");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "materialization_plan_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "materialization_proof_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "runtime_descriptor: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_descriptor_status: unsupported\n");
    yvex_cli_out_writef(stdout, "runtime_descriptor_kind: fullmodel-planning\n");
    yvex_cli_out_writef(stdout, "family: glm\n");
    yvex_cli_out_writef(stdout, "architecture: glm\n");
    yvex_cli_out_writef(stdout, "model_class: huge-MoE-source-target\n");
    yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "full_model_materialization: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "tensor_role_map_status: not-performed\n");
    yvex_cli_out_writef(stdout, "tensor_collection_map_status: not-performed\n");
    yvex_cli_out_writef(stdout, "required_role_coverage: none\n");
    yvex_cli_out_writef(stdout, "missing_required_roles: YVEX-produced-GGUF-artifact\n");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution\n");
    yvex_cli_out_writef(stdout, "unknown_role_count: 0\n");
    yvex_cli_out_writef(stdout, "embedding_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "normalization_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "attention_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "mlp_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "moe_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "output_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "tokenizer_descriptor: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "kv_descriptor: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "prefill_descriptor: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "decode_descriptor: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "logits_descriptor: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "sampling_descriptor: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "graph_requirements_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "required_graph_ops: planned-after-YVEX-produced-GGUF\n");
    yvex_cli_out_writef(stdout, "unsupported_graph_ops: GLM-full-transformer-runtime\n");
    yvex_cli_out_writef(stdout, "required_backend_ops: planned-after-YVEX-produced-GGUF\n");
    yvex_cli_out_writef(stdout, "unsupported_backend_ops: GLM-runtime-execution\n");
    yvex_cli_out_writef(stdout, "residency_requirements_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "residency_plan: future-YVEX-produced-artifact-required\n");
    yvex_cli_out_writef(stdout, "cpu_resident_required_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "cuda_resident_required_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "host_staged_required_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "ssd_staged_required_bytes: planned\n");
    yvex_cli_out_writef(stdout, "kv_required_bytes: planned\n");
    yvex_cli_out_writef(stdout, "scratch_required_bytes: planned\n");
    yvex_cli_out_writef(stdout, "context_requirements_status: planned\n");
    yvex_cli_out_writef(stdout, "max_context: unknown\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_policy: planned\n");
    yvex_cli_out_writef(stdout, "position_policy: planned\n");
    yvex_cli_out_writef(stdout, "rope_policy: planned\n");
    yvex_cli_out_writef(stdout, "kv_requirements_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_requirements_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "output_head_present: false\n");
    yvex_cli_out_writef(stdout, "output_head_dtype: unknown\n");
    yvex_cli_out_writef(stdout, "vocab_size: unknown\n");
    yvex_cli_out_writef(stdout, "logits_buffer_required: true\n");
    yvex_cli_out_writef(stdout, "real_output_head_logits: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_blockers: source-only target; YVEX-produced GGUF emission planned; GLM runtime unsupported\n");
    yvex_cli_out_writef(stdout, "descriptor_blockers: source-only target has no YVEX-produced GGUF tensor inventory\n");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
    fullmodel_print_descriptor_phases("unsupported", "unsupported", "resolve-model");
    return 5;
}

int print_fullmodel_source_only_family_runtime(const yvex_cli_fullmodel_options *options,
                                                      const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = fullmodel_requested_family(options);

    yvex_cli_out_writef(stdout, "family_runtime: report\n");
    yvex_cli_out_writef(stdout, "status: fullmodel-family-runtime-unsupported\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "target_class: official-source-huge-model\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "family: glm\n");
    yvex_cli_out_writef(stdout, "family_detected: glm\n");
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
    yvex_cli_out_writef(stdout, "family_adapter: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "family_adapter_status: unsupported\n");
    yvex_cli_out_writef(stdout, "family_runtime_stage: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "descriptor_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "descriptor_source: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "role_adapter_status: not-performed\n");
    yvex_cli_out_writef(stdout, "collection_adapter_status: not-performed\n");
    yvex_cli_out_writef(stdout, "attention_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "position_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "kv_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "moe_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "mlp_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "output_head_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "tokenizer_rule_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "graph_requirement_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "runtime_blocker_status: blocked\n");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "token_embedding_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "attention_norm_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "q_projection_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "k_projection_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "v_projection_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "o_projection_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "output_head_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "tokenizer_metadata_role: not-performed-source-only\n");
    yvex_cli_out_writef(stdout, "attention_family: glm-family-planned\n");
    yvex_cli_out_writef(stdout, "attention_type: unknown-source-only\n");
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_required: true\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "moe_required: true\n");
    yvex_cli_out_writef(stdout, "router_required: true\n");
    yvex_cli_out_writef(stdout, "router_present: false\n");
    yvex_cli_out_writef(stdout, "expert_tensors_present: false\n");
    yvex_cli_out_writef(stdout, "moe_expert_count: unknown\n");
    yvex_cli_out_writef(stdout, "moe_active_expert_count: unknown\n");
    yvex_cli_out_writef(stdout, "moe_shared_experts: unknown\n");
    yvex_cli_out_writef(stdout, "moe_dispatch_ready: false\n");
    yvex_cli_out_writef(stdout, "output_head_required: true\n");
    yvex_cli_out_writef(stdout, "output_head_present: false\n");
    yvex_cli_out_writef(stdout, "output_head_tensor: none\n");
    yvex_cli_out_writef(stdout, "vocab_size: unknown\n");
    yvex_cli_out_writef(stdout, "logits_projection_ready: false\n");
    yvex_cli_out_writef(stdout, "real_output_head_logits: false\n");
    yvex_cli_out_writef(stdout, "required_graph_ops: planned-after-YVEX-produced-GGUF\n");
    yvex_cli_out_writef(stdout, "implemented_graph_primitives: none-for-source-only-target\n");
    yvex_cli_out_writef(stdout, "unsupported_graph_ops: GLM-full-transformer-runtime\n");
    yvex_cli_out_writef(stdout, "graph.full_attention_from_model_tensors: unsupported\n");
    yvex_cli_out_writef(stdout, "graph.full_transformer_block_from_model_tensors: unsupported\n");
    yvex_cli_out_writef(stdout, "graph.full_layer_stack: unsupported\n");
    yvex_cli_out_writef(stdout, "full_transformer_graph_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_blockers: source-only target; YVEX-produced GGUF emission planned; GLM runtime family mapping planned; GLM runtime unsupported\n");
    yvex_cli_out_writef(stdout, "next_required_rows: OWI.HUGE.0,MODEL.CLASS.3,TENSOR.COLLECTION.2,ATTENTION.CLASS.0,KV.CACHE.0,MOE.CLASS.0,GLM-YVEX-produced-GGUF,GLM-runtime-family-mapping\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
    fullmodel_print_family_runtime_phases("unsupported", "resolve-model");
    return 5;
}

static void print_fullmodel_failed_plan_fields(const yvex_cli_fullmodel_options *options,
                                               const char *phase,
                                               const char *reason,
                                               unsigned long long artifact_bytes)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";

    yvex_cli_out_writef(stdout, "residency: %s\n", residency);
    yvex_cli_out_writef(stdout, "plan_status: failed\n");
    yvex_cli_out_writef(stdout, "materialization_plan_ready: false\n");
    yvex_cli_out_writef(stdout, "materialization_attempted: false\n");
    yvex_cli_out_writef(stdout, "full_materialization_proof: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "plan_id: unavailable\n");
    yvex_cli_out_writef(stdout, "plan_kind: full-model-materialization\n");
    yvex_cli_out_writef(stdout, "plan_source: tensor-inventory\n");
    yvex_cli_out_writef(stdout, "plan_backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "plan_residency: %s\n", residency);
    yvex_cli_out_writef(stdout, "plan_tensor_count: 0\n");
    yvex_cli_out_writef(stdout, "plan_tensor_bytes: 0\n");
    yvex_cli_out_writef(stdout, "plan_collection_count: 0\n");
    yvex_cli_out_writef(stdout, "plan_phase_count: 1\n");
    yvex_cli_out_writef(stdout, "plan_blocker_count: 1\n");
    yvex_cli_out_writef(stdout, "plan_cleanup_required: false\n");
    yvex_cli_out_writef(stdout, "plan_cleanup_phases: none\n");
    yvex_cli_out_writef(stdout, "backend_available: unknown\n");
    yvex_cli_out_writef(stdout, "backend_memory_known: false\n");
    yvex_cli_out_writef(stdout, "backend_memory_total_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend_memory_available_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend_required_bytes: 0\n");
    yvex_cli_out_writef(stdout, "backend_fit_status: unknown\n");
    yvex_cli_out_writef(stdout, "backend_fit_reason: inventory failed before backend fit preflight\n");
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");
    fullmodel_print_phase(0u, phase ? phase : "preflight", "blocked",
                          0ull, artifact_bytes, "not-applicable", 1, 1,
                          reason ? reason : "inventory failed");
    (void)fullmodel_print_blocker(0u, "inventory", "fatal",
                                  reason ? reason : "inventory failed",
                                  1, 1);
    yvex_cli_out_writef(stdout, "cleanup_plan_required: false\n");
    yvex_cli_out_writef(stdout, "cleanup_plan_phases: none\n");
    yvex_cli_out_writef(stdout, "cleanup_idempotent_required: true\n");
    yvex_cli_out_writef(stdout, "cleanup_failure_policy: preserve-failure-report\n");
    yvex_cli_out_writef(stdout, "next_required_row: FULLMODEL.2\n");
    yvex_cli_out_writef(stdout, "proof_ready_for_fullmodel_2: false\n");
    yvex_cli_out_writef(stdout, "fullmodel_2_blockers: tensor inventory unavailable; materialization plan unavailable\n");
}

int print_fullmodel_missing_report(const yvex_cli_fullmodel_options *options,
                                          const char *resolved_path)
{
    int is_plan = options &&
                  options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    int is_materialize = options &&
                         options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    int is_descriptor = options &&
                        options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    int is_family_runtime = options &&
                            options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;

    if (is_materialize) {
        yvex_fullmodel_materialize_report report;
        memset(&report, 0, sizeof(report));
        report.options = options;
        report.status = "fullmodel-materialize-fail";
        report.model_resolved_path = resolved_path ? resolved_path : "";
        report.target_id = options && options->target ? options->target : "unknown";
        report.target_class = "unresolved-artifact";
        report.artifact_identity_status = "unavailable";
        report.tensor_inventory_status = "failed";
        report.required_role_coverage = "none";
        report.missing_required_roles = "artifact";
        report.unsupported_required_roles = "full-runtime-model";
        report.placement_plan_status = "failed";
        report.memory_budget_status = "not-performed";
        report.backend_preflight_status = "not-performed";
        report.materialization_mode = "none";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.phase = "failed";
        report.failed_phase = "resolve-model";
        report.failed_reason = "artifact path does not exist";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.cleanup_idempotent = "true";
        report.owned_state_released = "true";
        report.partial_materialization = "false";
        report.residency_plan = "unavailable";
        report.runtime_blockers = "artifact path missing";
        fullmodel_print_materialize_report(&report);
        yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    if (is_descriptor) {
        yvex_cli_out_writef(stdout, "fullmodel: descriptor\n");
        yvex_cli_out_writef(stdout, "status: fullmodel-descriptor-fail\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : "unknown");
        yvex_cli_out_writef(stdout, "target_class: unresolved-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_writef(stdout, "artifact_identity_status: unavailable\n");
        yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
        yvex_cli_out_writef(stdout, "materialization_plan_status: unavailable\n");
        yvex_cli_out_writef(stdout, "materialization_proof_status: unavailable\n");
        yvex_cli_out_writef(stdout, "runtime_descriptor: report-only\n");
        yvex_cli_out_writef(stdout, "runtime_descriptor_status: fail\n");
        yvex_cli_out_writef(stdout, "runtime_descriptor_kind: fullmodel-planning\n");
        yvex_cli_out_writef(stdout, "family: unknown\n");
        yvex_cli_out_writef(stdout, "architecture: unknown\n");
        yvex_cli_out_writef(stdout, "model_class: unresolved\n");
        yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
        yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
        yvex_cli_out_writef(stdout, "full_model_materialization: unavailable\n");
        yvex_cli_out_writef(stdout, "generation_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
        yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
        yvex_cli_out_writef(stdout, "tensor_role_map_status: unavailable\n");
        yvex_cli_out_writef(stdout, "tensor_collection_map_status: unavailable\n");
        yvex_cli_out_writef(stdout, "required_role_coverage: none\n");
        yvex_cli_out_writef(stdout, "missing_required_roles: artifact\n");
        yvex_cli_out_writef(stdout, "unsupported_required_roles: full-runtime-model\n");
        yvex_cli_out_writef(stdout, "runtime_blockers: artifact path missing\n");
        yvex_cli_out_writef(stdout, "descriptor_blockers: artifact path missing\n");
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "decode_ready: false\n");
        yvex_cli_out_writef(stdout, "logits_ready: false\n");
        yvex_cli_out_writef(stdout, "sampling_ready: false\n");
        yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
        yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
        fullmodel_print_descriptor_phases("fail", "fail", "resolve-model");
        yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    if (is_family_runtime) {
        yvex_cli_out_writef(stdout, "family_runtime: report\n");
        yvex_cli_out_writef(stdout, "status: fullmodel-family-runtime-fail\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : "unknown");
        yvex_cli_out_writef(stdout, "target_class: unresolved-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_writef(stdout, "family: unknown\n");
        yvex_cli_out_writef(stdout, "family_detected: unknown\n");
        yvex_cli_out_writef(stdout, "family_requested: %s\n", fullmodel_requested_family(options));
        yvex_cli_out_writef(stdout, "family_adapter: unavailable\n");
        yvex_cli_out_writef(stdout, "family_adapter_status: failed\n");
        yvex_cli_out_writef(stdout, "family_runtime_stage: report-only\n");
        yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
        yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
        yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
        yvex_cli_out_writef(stdout, "descriptor_status: fail\n");
        yvex_cli_out_writef(stdout, "descriptor_source: unavailable\n");
        yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
        yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
        yvex_cli_out_writef(stdout, "generation_ready: false\n");
        yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
        yvex_cli_out_writef(stdout, "runtime_blockers: artifact path missing\n");
        yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
        yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
        fullmodel_print_family_runtime_phases("failed", "resolve-model");
        yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    yvex_cli_out_writef(stdout, "fullmodel: %s\n", is_plan ? "materialization-plan" : "report");
    yvex_cli_out_writef(stdout, "status: %s\n", is_plan ? "fullmodel-materialization-plan-fail" : "fullmodel-report-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : "unknown");
    yvex_cli_out_writef(stdout, "target_class: unresolved-artifact\n");
    yvex_cli_out_writef(stdout, "source_artifact_class: unknown\n");
    yvex_cli_out_writef(stdout, "target_artifact_class: GGUF artifact\n");
    yvex_cli_out_writef(stdout, "artifact_exists: false\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: 0\n");
    yvex_cli_out_writef(stdout, "artifact_identity_status: unavailable\n");
    yvex_cli_out_writef(stdout, "tensor_count: 0\n");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
    yvex_cli_out_writef(stdout, "metadata_status: failed\n");
    yvex_cli_out_writef(stdout, "architecture: unknown\n");
    yvex_cli_out_writef(stdout, "family: unknown\n");
    yvex_cli_out_writef(stdout, "model_class: unresolved\n");
    yvex_cli_out_writef(stdout, "fullmodel_inventory: unavailable\n");
    yvex_cli_out_writef(stdout, "qtype_summary: none\n");
    yvex_cli_out_writef(stdout, "dtype_summary: none\n");
    yvex_cli_out_writef(stdout, "total_tensor_bytes: 0\n");
    yvex_cli_out_writef(stdout, "estimated_cpu_resident_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "estimated_cuda_resident_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "estimated_kv_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_scratch_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_total_runtime_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "backend_placement_status: failed-missing-artifact\n");
    yvex_cli_out_writef(stdout, "cpu_placement: unavailable\n");
    yvex_cli_out_writef(stdout, "cuda_placement: unavailable\n");
    yvex_cli_out_writef(stdout, "cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "cuda_memory_status: unavailable\n");
    yvex_cli_out_writef(stdout, "residency_plan: unavailable\n");
    yvex_cli_out_writef(stdout, "tensor_collections_status: unavailable\n");
    yvex_cli_out_writef(stdout, "collection_detected: no\n");
    yvex_cli_out_writef(stdout, "collection_supported: false\n");
    yvex_cli_out_writef(stdout, "runtime_consumer: unsupported\n");
    yvex_cli_out_writef(stdout, "embedding_tensors: 0\n");
    yvex_cli_out_writef(stdout, "normalization_tensors: 0\n");
    yvex_cli_out_writef(stdout, "attention_tensors: 0\n");
    yvex_cli_out_writef(stdout, "kv_cache_requirements: planned\n");
    yvex_cli_out_writef(stdout, "mlp_tensors: 0\n");
    yvex_cli_out_writef(stdout, "moe_tensors: 0\n");
    yvex_cli_out_writef(stdout, "output_tensors: 0\n");
    yvex_cli_out_writef(stdout, "tokenizer_tensors: 0\n");
    yvex_cli_out_writef(stdout, "required_role_coverage: none\n");
    yvex_cli_out_writef(stdout, "missing_required_roles: artifact\n");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: full-runtime-model\n");
    yvex_cli_out_writef(stdout, "runtime_blockers: artifact path missing\n");
    print_fullmodel_common_boundaries();
    if (is_plan) {
        print_fullmodel_failed_plan_fields(options,
                                           "preflight",
                                           "artifact path does not exist",
                                           0ull);
    }
    yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
    return exit_for_status(YVEX_ERR_IO);
}

int print_fullmodel_parse_failure_report(const yvex_cli_fullmodel_options *options,
                                                const yvex_model_ref *ref,
                                                const char *reason,
                                                int rc)
{
    unsigned long long artifact_bytes = 0ull;
    int is_plan = options &&
                  options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    int is_materialize = options &&
                         options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    int is_descriptor = options &&
                        options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    int is_family_runtime = options &&
                            options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;

    fullmodel_file_size(ref && ref->path ? ref->path : "", &artifact_bytes);
    if (is_materialize) {
        yvex_fullmodel_materialize_report report;
        memset(&report, 0, sizeof(report));
        report.options = options;
        report.status = "fullmodel-materialize-fail";
        report.model_resolved_path = ref && ref->path ? ref->path : "";
        report.target_id = options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path");
        report.target_class = "GGUF-artifact";
        report.artifact_identity_status = "not-checked";
        report.tensor_inventory_status = "failed";
        report.required_role_coverage = "none";
        report.missing_required_roles = "parseable-GGUF-tensor-directory";
        report.unsupported_required_roles = "full-runtime-model";
        report.placement_plan_status = "failed";
        report.memory_budget_status = "not-performed";
        report.backend_preflight_status = "not-performed";
        report.materialization_mode = "none";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.phase = "failed";
        report.failed_phase = "tensor-inventory";
        report.failed_reason = reason ? reason : "parse failed";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.cleanup_idempotent = "true";
        report.owned_state_released = "true";
        report.partial_materialization = "false";
        report.residency_plan = "unavailable";
        report.runtime_blockers = "GGUF metadata or tensor directory parse failed";
        fullmodel_print_materialize_report(&report);
        yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    if (is_descriptor) {
        yvex_cli_out_writef(stdout, "fullmodel: descriptor\n");
        yvex_cli_out_writef(stdout, "status: fullmodel-descriptor-fail\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
        yvex_cli_out_writef(stdout, "target_class: GGUF-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_writef(stdout, "artifact_identity_status: not-checked\n");
        yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
        yvex_cli_out_writef(stdout, "materialization_plan_status: failed\n");
        yvex_cli_out_writef(stdout, "materialization_proof_status: unavailable\n");
        yvex_cli_out_writef(stdout, "runtime_descriptor: report-only\n");
        yvex_cli_out_writef(stdout, "runtime_descriptor_status: fail\n");
        yvex_cli_out_writef(stdout, "runtime_descriptor_kind: fullmodel-planning\n");
        yvex_cli_out_writef(stdout, "family: unknown\n");
        yvex_cli_out_writef(stdout, "architecture: unknown\n");
        yvex_cli_out_writef(stdout, "model_class: parse-failed\n");
        yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
        yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
        yvex_cli_out_writef(stdout, "full_model_materialization: unavailable\n");
        yvex_cli_out_writef(stdout, "generation_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
        yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
        yvex_cli_out_writef(stdout, "tensor_role_map_status: unavailable\n");
        yvex_cli_out_writef(stdout, "tensor_collection_map_status: unavailable\n");
        yvex_cli_out_writef(stdout, "required_role_coverage: none\n");
        yvex_cli_out_writef(stdout, "missing_required_roles: parseable-GGUF-tensor-directory\n");
        yvex_cli_out_writef(stdout, "unsupported_required_roles: full-runtime-model\n");
        yvex_cli_out_writef(stdout, "runtime_blockers: GGUF metadata or tensor directory parse failed\n");
        yvex_cli_out_writef(stdout, "descriptor_blockers: GGUF metadata or tensor directory parse failed\n");
        yvex_cli_out_writef(stdout, "prefill_ready: false\n");
        yvex_cli_out_writef(stdout, "decode_ready: false\n");
        yvex_cli_out_writef(stdout, "logits_ready: false\n");
        yvex_cli_out_writef(stdout, "sampling_ready: false\n");
        yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
        yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
        fullmodel_print_descriptor_phases("fail", "fail", "tensor-inventory");
        yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    if (is_family_runtime) {
        yvex_cli_out_writef(stdout, "family_runtime: report\n");
        yvex_cli_out_writef(stdout, "status: fullmodel-family-runtime-fail\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
        yvex_cli_out_writef(stdout, "target_class: GGUF-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_writef(stdout, "family: unknown\n");
        yvex_cli_out_writef(stdout, "family_detected: unknown\n");
        yvex_cli_out_writef(stdout, "family_requested: %s\n", fullmodel_requested_family(options));
        yvex_cli_out_writef(stdout, "family_adapter: unavailable\n");
        yvex_cli_out_writef(stdout, "family_adapter_status: failed\n");
        yvex_cli_out_writef(stdout, "family_runtime_stage: report-only\n");
        yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
        yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
        yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
        yvex_cli_out_writef(stdout, "descriptor_status: fail\n");
        yvex_cli_out_writef(stdout, "descriptor_source: fullmodel-descriptor-facts\n");
        yvex_cli_out_writef(stdout, "full_runtime_model: false\n");
        yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
        yvex_cli_out_writef(stdout, "generation_ready: false\n");
        yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
        yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
        yvex_cli_out_writef(stdout, "runtime_blockers: GGUF metadata or tensor directory parse failed\n");
        yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
        yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
        fullmodel_print_family_runtime_phases("failed", "load-descriptor");
        yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    yvex_cli_out_writef(stdout, "fullmodel: %s\n", is_plan ? "materialization-plan" : "report");
    yvex_cli_out_writef(stdout, "status: %s\n", is_plan ? "fullmodel-materialization-plan-fail" : "fullmodel-report-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
    yvex_cli_out_writef(stdout, "target_class: GGUF-artifact\n");
    yvex_cli_out_writef(stdout, "source_artifact_class: unknown\n");
    yvex_cli_out_writef(stdout, "target_artifact_class: GGUF artifact\n");
    yvex_cli_out_writef(stdout, "artifact_exists: true\n");
    yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
    yvex_cli_out_writef(stdout, "artifact_identity_status: not-checked\n");
    yvex_cli_out_writef(stdout, "tensor_count: 0\n");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
    yvex_cli_out_writef(stdout, "metadata_status: failed\n");
    yvex_cli_out_writef(stdout, "architecture: unknown\n");
    yvex_cli_out_writef(stdout, "family: unknown\n");
    yvex_cli_out_writef(stdout, "model_class: parse-failed\n");
    yvex_cli_out_writef(stdout, "fullmodel_inventory: unavailable\n");
    yvex_cli_out_writef(stdout, "qtype_summary: none\n");
    yvex_cli_out_writef(stdout, "dtype_summary: none\n");
    yvex_cli_out_writef(stdout, "total_tensor_bytes: 0\n");
    yvex_cli_out_writef(stdout, "estimated_cpu_resident_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "estimated_cuda_resident_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "estimated_kv_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_scratch_bytes: planned\n");
    yvex_cli_out_writef(stdout, "estimated_total_runtime_bytes: unknown\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "backend_placement_status: failed-parse\n");
    yvex_cli_out_writef(stdout, "cpu_placement: unavailable\n");
    yvex_cli_out_writef(stdout, "cuda_placement: unavailable\n");
    yvex_cli_out_writef(stdout, "cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "cuda_memory_status: unavailable\n");
    yvex_cli_out_writef(stdout, "residency_plan: unavailable\n");
    yvex_cli_out_writef(stdout, "tensor_collections_status: failed\n");
    yvex_cli_out_writef(stdout, "collection_detected: no\n");
    yvex_cli_out_writef(stdout, "collection_supported: false\n");
    yvex_cli_out_writef(stdout, "runtime_consumer: unsupported\n");
    yvex_cli_out_writef(stdout, "embedding_tensors: 0\n");
    yvex_cli_out_writef(stdout, "normalization_tensors: 0\n");
    yvex_cli_out_writef(stdout, "attention_tensors: 0\n");
    yvex_cli_out_writef(stdout, "kv_cache_requirements: planned\n");
    yvex_cli_out_writef(stdout, "mlp_tensors: 0\n");
    yvex_cli_out_writef(stdout, "moe_tensors: 0\n");
    yvex_cli_out_writef(stdout, "output_tensors: 0\n");
    yvex_cli_out_writef(stdout, "tokenizer_tensors: 0\n");
    yvex_cli_out_writef(stdout, "required_role_coverage: none\n");
    yvex_cli_out_writef(stdout, "missing_required_roles: parseable-GGUF-tensor-directory\n");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: full-runtime-model\n");
    yvex_cli_out_writef(stdout, "runtime_blockers: GGUF metadata or tensor directory parse failed\n");
    print_fullmodel_common_boundaries();
    if (is_plan) {
        print_fullmodel_failed_plan_fields(options,
                                           "tensor-directory",
                                           reason ? reason : "parse failed",
                                           artifact_bytes);
    }
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
    return exit_for_status(rc);
}
