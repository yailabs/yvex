/* Owner: src/cli/render
 * Owns: normal, table, audit, and help text rendering for graph reports.
 * Does not own: graph construction, report building, input parsing, command dispatch, backend primitive execution,
 *   reference comparison, stdout/stderr writer primitives, generation, eval, benchmark, or release
 *   decisions.
 * Invariants: all output goes through src/cli/io writer helpers.
 * Boundary: graph rendering is not graph runtime or generation readiness.
 * Purpose: provide normal, table, audit, and help text rendering for graph reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"
#include "src/cli/model_artifacts/private.h"

static const char *const literal_lines_0[] = {
    "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda]",
    "       yvex graph check [--suite primitives|block|layers|all] [--backend cpu|cuda]",
    "       yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N",
    "       yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal]",
    "       yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N",
    "       yvex graph --backend cpu|cuda --execute-op --op mlp --hidden-dim N --ffn-dim N --activation "
        "silu --gated [--experts N --expert-id N]",
    "       yvex graph --backend cpu|cuda --execute-block --block fixture --seq-len N --position N --"
        "hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]",
    "       yvex graph --backend cpu|cuda --execute-layers --layers N --block fixture --seq-len N --"
        "position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]",
    "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-fixture [--fixture-token N]",
    "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-partial [--partial-token N | --"
        "tokens IDS --token-index N]",
    "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-segment --segment embedding-"
        "rmsnorm [--partial-token N | --tokens IDS --token-index N]",
    "",
    "example: yvex graph check --suite primitives --backend cpu",
    "boundary: graph construction, selected primitive proof, and selected graph slice proof are not full "
        "transformer execution or generation readiness"
};

/* Purpose: Render graph render body from typed facts (`graph_render_body`). */
static const char *graph_render_body(const yvex_graph_report *report)
{
    return report && report->body ? report->body : "";
}

/* Render a graph admission result without making the graph owner write operator output. */
/* Purpose: Render graph guard print from typed facts (`yvex_cli_graph_guard_print`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_graph_guard_print(const yvex_cli_graph_guard_report *report)
{
    if (!report) return;
    yvex_cli_out_writef(stdout, "graph_integrity_guard: %s\n",
                        report->guard_status ? report->guard_status : "fail");
    yvex_cli_out_writef(stdout, "graph_execution_phase: %s\n",
                        report->phase ? report->phase : "preflight");
    yvex_cli_out_writef(stdout, "graph_kind: %s\n",
                        report->graph_kind ? report->graph_kind : "unknown");
    yvex_cli_out_writef(stdout, "integrity_status: %s\n",
                        report->integrity_status ? report->integrity_status : "unchecked");
    yvex_cli_out_writef(stdout, "identity_status: %s\n",
                        report->identity_status ? report->identity_status : "unregistered");
    yvex_cli_out_writef(stdout, "metadata_status: %s\n",
                        report->metadata_status ? report->metadata_status : "unregistered");
    yvex_cli_out_writef(stdout, "shape_status: %s\n",
                        report->shape_status ? report->shape_status : "unchecked");
    yvex_cli_out_writef(stdout, "range_status: %s\n",
                        report->range_status ? report->range_status : "unchecked");
    yvex_cli_out_writef(stdout, "slice_range_status: %s\n",
                        report->slice_range_status ? report->slice_range_status : "unchecked");
    yvex_cli_out_writef(stdout, "backend_status: %s\n",
                        report->backend_status ? report->backend_status : "not-opened");
    yvex_cli_out_writef(stdout, "backend_op_status: %s\n",
                        report->backend_op_status ? report->backend_op_status : "unchecked");
    yvex_cli_out_writef(stdout, "dispatch_attempted: %s\n",
                        report->dispatch_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "reference_read_attempted: %s\n",
                        report->reference_read_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_allocation_attempted: %s\n",
                        report->output_allocation_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n",
                        report->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n",
                        report->cleanup_status ? report->cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "output_bytes_planned: %llu\n",
                        report->output_bytes_planned);
    yvex_cli_out_writef(stdout, "output_bytes_allocated: %llu\n",
                        report->output_bytes_allocated);
    yvex_cli_out_writef(stdout, "reference_bytes_planned: %llu\n",
                        report->reference_bytes_planned);
}

/* Purpose: Render graph render normal from typed facts (`yvex_graph_render_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_render_normal(FILE *fp,
                             const yvex_graph_report *report)
{
    yvex_cli_out_writef(fp, "%s", graph_render_body(report));
    return YVEX_OK;
}

/* Purpose: Render graph render table from typed facts (`yvex_graph_render_table`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_render_table(FILE *fp,
                            const yvex_graph_report *report)
{
    yvex_cli_out_writef(fp, "%s", graph_render_body(report));
    return YVEX_OK;
}

/* Purpose: Render graph render audit from typed facts (`yvex_graph_render_audit`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_render_audit(FILE *fp,
                            const yvex_graph_report *report)
{
    yvex_cli_out_writef(fp, "%s", graph_render_body(report));
    return YVEX_OK;
}

/* Purpose: Render graph render from typed facts (`yvex_graph_render`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_render(FILE *fp,
                      yvex_graph_report_mode mode,
                      const yvex_graph_report *report)
{
    if (mode == YVEX_GRAPH_REPORT_MODE_TABLE) {
        return yvex_graph_render_table(fp, report);
    }
    if (mode == YVEX_GRAPH_REPORT_MODE_AUDIT) {
        return yvex_graph_render_audit(fp, report);
    }
    return yvex_graph_render_normal(fp, report);
}

/* Purpose: Render graph render help from typed facts (`yvex_graph_render_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_render_help(FILE *fp)
{
    yvex_cli_out_lines(fp, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    return YVEX_OK;
}

static const char *const literal_lines_1[] = { "graph_requirements_status: blocked",
    "required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,"
        "attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp-gate-"
        "up-down,activation,moe-router,expert-dispatch,expert-accumulation,final-norm,output-head-projection",
    "unsupported_graph_ops: full-transformer-attention,real-layer-scheduler,real-moe-router,real-expert-"
        "dispatch,real-output-head-projection",
    "required_backend_ops: tensor-read,rmsnorm,matmul,rope,attention,softmax,activation,residual-add,kv-read,kv-write",
    "unsupported_backend_ops: full-transformer-runtime-integration,real-attention-backed-kv,real-output-head-logits"};

static const char *const literal_lines_2[] = {
    "generation_ready: false", "generation: unsupported-full-model", "benchmark_status: not-measured"};

static const char *const literal_lines_3[] = {
    "prefill_descriptor: unsupported-full-transformer-prefill", "prefill.requires_embedding: true",
    "prefill.requires_attention_qkv: true", "prefill.requires_real_kv_writes: true",
    "prefill.requires_mlp_or_moe: true", "prefill.requires_layer_scheduler: true",
    "prefill.current_status: unsupported", "prefill.blocker: real transformer prefill not implemented",
    "decode_descriptor: unsupported-full-model-decode", "decode.mode_required: baseline-autoregressive",
    "decode.requires_prefill_state: true", "decode.requires_kv_read: true", "decode.requires_layer_execution: true",
    "decode.current_status: unsupported", "decode.blocker: full model decode not implemented",
    "logits_descriptor: unsupported-real-output-head-logits",
    "sampling_descriptor: unsupported-real-vocabulary-sampling", "residency_requirements_status: planned",
    "residency_plan: descriptor-only-no-allocation"};

static const char *const literal_lines_4[] = {
    "ssd_staged_required_bytes: planned", "kv_required_bytes: planned", "scratch_required_bytes: planned",
    "context_requirements_status: planned", "max_context: metadata-or-unknown", "requested_context: not-requested",
    "context_policy: planned", "position_policy: rope-or-family-specific-planned", "rope_policy: planned",
    "kv_requirements_status: unsupported", "kv_layout: planned", "kv_dtype: planned", "kv_layers: unknown",
    "kv_heads: unknown", "kv_head_dim: unknown", "kv_capacity_status: unsupported-full-transformer-kv",
    "kv.required: true", "kv.real_attention_writes: false", "kv.runtime_status: unsupported",
    "kv_write_ready: false", "kv_read_ready: false", "logits_requirements_status: unsupported"};

static const char *const literal_lines_5[] = {
    "logits_buffer_required: true", "real_output_head_logits: false", "logits_ready: false",
    "logits.blocker: real output-head logits runtime unsupported"};

static const char *const literal_lines_6[] = {
    "special_token_policy: planned", "eos_backed_stop: unsupported", "stop_token_text_matching: unsupported",
    "tokenizer_quality_generation: unsupported"};

static const char *const literal_lines_7[] = {
    "backend.primitive_rope: implemented-fixture", "backend.primitive_attention: implemented-fixture",
    "backend.primitive_matmul: implemented-fixture", "backend.primitive_mlp: implemented-fixture",
    "backend.full_transformer_integration: unsupported", "backend_allocation_attempted: false"};

static const char *const literal_lines_8[] = {
    "prefill_ready: false", "decode_ready: false", "sampling_ready: false", "cleanup_attempted: false",
    "cleanup_status: not-needed"};

static const char *const literal_pair_15[] = { "full_runtime_model: false", "full_model_execution: unsupported"};

static const char *const literal_pair_16[] = { "fullmodel: descriptor", "status: fullmodel-descriptor"};

static const char *const literal_pair_17[] = {
    "graph.residual_add: planned", "graph.mlp_primitive: implemented-fixture"};

static const char *const literal_pair_18[] = {
    "graph.rope_position_op: implemented-primitive", "graph.attention_primitive: implemented-fixture"};

/* Purpose: Map one admitted tensor role to its descriptor collection.
 * Inputs: Borrowed canonical role text.
 * Effects: No externally visible effect.
 * Failure: Unknown roles resolve to the explicit unknown collection.
 * Boundary: Classification is presentation-only and cannot alter model truth. */
static const char *fullmodel_descriptor_role_collection(const char *role)
{
    if (!role) return "unknown";
    if (strcmp(role, "token_embedding") == 0) return "embedding";
    if (strcmp(role, "attention_norm") == 0 ||
        strcmp(role, "post_attention_norm") == 0 ||
        strcmp(role, "final_norm") == 0) return "normalization";
    if (strcmp(role, "q_projection") == 0 ||
        strcmp(role, "k_projection") == 0 ||
        strcmp(role, "v_projection") == 0 ||
        strcmp(role, "o_projection") == 0) return "attention";
    if (strcmp(role, "mlp_gate") == 0 ||
        strcmp(role, "mlp_up") == 0 ||
        strcmp(role, "mlp_down") == 0) return "mlp";
    if (strcmp(role, "moe_router") == 0 ||
        strcmp(role, "moe_expert_gate") == 0 ||
        strcmp(role, "moe_expert_up") == 0 ||
        strcmp(role, "moe_expert_down") == 0) return "moe";
    if (strcmp(role, "output_head") == 0) return "output";
    if (strcmp(role, "tokenizer_metadata") == 0) return "tokenizer-runtime-input";
    return "unknown";
}

/* Purpose: Compute fullmodel descriptor role residency for its CLI invariant
 *   (`fullmodel_descriptor_role_residency`). */
static const char *fullmodel_descriptor_role_residency(const char *role,
                                                       const char *backend,
                                                       int present)
{
    if (!present) return "not-planned";
    if (role && strcmp(role, "tokenizer_metadata") == 0) return "host-runtime-metadata";
    return backend && strcmp(backend, "cuda") == 0 ? "cuda-resident-planned" : "cpu-resident-planned";
}

/* Purpose: Render fullmodel print descriptor role from typed facts (`fullmodel_print_descriptor_role`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_print_descriptor_role(yvex_model_context *ctx,
                                            const yvex_fullmodel_collections *collections,
                                            const char *role,
                                            const char *backend)
{
    const yvex_tensor_info *tensor = NULL;
    char dims[128];
    int present = 0;

    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        present = collections && collections->has_tokenizer_metadata;
    } else {
        tensor = fullmodel_descriptor_find_tensor(ctx, role);
        present = tensor != NULL;
    }
    dims[0] = '\0';
    if (tensor) dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
    yvex_cli_out_writef(stdout, "role.%s.status: %s\n", role ? role : "unknown", present ? "present" : "missing");
    yvex_cli_out_writef(stdout, "role.%s.tensor: %s\n", role ? role : "unknown",
           tensor && tensor->name ? tensor->name : present ? "metadata" : "none");
    yvex_cli_out_writef(stdout, "role.%s.shape: %s\n", role ? role : "unknown",
           tensor ? dims : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.dtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.qtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.bytes: %llu\n", role ? role : "unknown",
           tensor ? tensor->storage_bytes : 0ull);
    yvex_cli_out_writef(stdout, "role.%s.collection: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_collection(role));
    yvex_cli_out_writef(stdout, "role.%s.residency_expectation: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_residency(role, backend, present));
    yvex_cli_out_writef(stdout, "role.%s.runtime_consumer: %s\n", role ? role : "unknown",
           present ? "planned" : "blocked-missing-role");
}

/* Purpose: Render one descriptor collection and its exact runtime requirements.
 * Inputs: Borrowed collection identity, accounting, requirements, and blocker facts.
 * Effects: Writes ordered descriptor fields through CLI I/O.
 * Failure: CLI write failures remain owned by the output boundary.
 * Boundary: Rendering does not make the collection resident or executable. */
static void fullmodel_print_descriptor_collection(const char *name,
                                                  unsigned long long count,
                                                  unsigned long long bytes,
                                                  int required_for_prefill,
                                                  int required_for_decode,
                                                  int required_for_logits,
                                                  int required_for_generation,
                                                  const char *runtime_consumer,
                                                  const char *blocker)
{
    yvex_cli_out_writef(stdout, "collection.%s.status: %s\n", name, count > 0ull ? "present" : "missing");
    yvex_cli_out_writef(stdout, "collection.%s.tensor_count: %llu\n", name, count);
    yvex_cli_out_writef(stdout, "collection.%s.byte_count: %llu\n", name, bytes);
    yvex_cli_out_writef(stdout, "collection.%s.required_for_prefill: %s\n", name,
        required_for_prefill ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_decode: %s\n", name,
        required_for_decode ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_logits: %s\n", name,
        required_for_logits ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_generation: %s\n", name,
        required_for_generation ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.runtime_consumer: %s\n", name,
        runtime_consumer ? runtime_consumer : "planned");
    yvex_cli_out_writef(stdout, "collection.%s.blocker: %s\n", name, blocker && blocker[0] ? blocker : "none");
}

/* Purpose: Render fullmodel print descriptor phases from typed facts (`fullmodel_print_descriptor_phases`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_descriptor_phases(const char *role_status,
                                              const char *collection_status,
                                              const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-map",
        "collection-map",
        "shape-requirements",
        "residency-requirements",
        "graph-requirements",
        "prefill-requirements",
        "kv-requirements",
        "decode-requirements",
        "logits-requirements",
        "sampling-requirements",
        "tokenizer-requirements",
        "backend-requirements",
        "blocker-report",
        "descriptor-build",
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
        } else if (strcmp(phases[i], "role-map") == 0) {
            status = role_status ? role_status : "partial";
        } else if (strcmp(phases[i], "collection-map") == 0) {
            status = collection_status ? collection_status : "partial";
        } else if (strcmp(phases[i], "residency-requirements") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "backend-requirements") == 0) {
            status = "planned";
        } else if (strcmp(phases[i], "prefill-requirements") == 0 ||
                   strcmp(phases[i], "kv-requirements") == 0 ||
                   strcmp(phases[i], "decode-requirements") == 0 ||
                   strcmp(phases[i], "logits-requirements") == 0 ||
                   strcmp(phases[i], "sampling-requirements") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "skipped";
        }
        model_phase_print("descriptor_phase", i, phases[i], status, "planned");
    }
}

/* Purpose: Render fullmodel print descriptor graph requirements from typed facts
 * (`fullmodel_print_descriptor_graph_requirements`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_print_descriptor_graph_requirements(const yvex_fullmodel_collections *collections)
{
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);

    yvex_cli_out_lines(stdout, literal_lines_1, sizeof(literal_lines_1) / sizeof(literal_lines_1[0]));
    yvex_cli_out_writef(stdout, "graph.embedding_lookup: %s\n",
           collections && collections->has_token_embedding ? "planned-real-tensor" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.rmsnorm: %s\n",
           fullmodel_has_normalization_collection(collections) ? "implemented-selected-segment" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.q_projection: %s\n",
        collections && collections->has_attention_q ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.k_projection: %s\n",
        collections && collections->has_attention_k ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.v_projection: %s\n",
        collections && collections->has_attention_v ? "planned" : "missing-tensor");
    yvex_cli_out_lines(stdout, literal_pair_18, sizeof(literal_pair_18) / sizeof(literal_pair_18[0]));
    yvex_cli_out_writef(stdout, "graph.full_transformer_attention: %s\n",
        has_attention ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.o_projection: %s\n",
        collections && collections->has_attention_out ? "planned" : "missing-tensor");
    yvex_cli_out_lines(stdout, literal_pair_17, sizeof(literal_pair_17) / sizeof(literal_pair_17[0]));
    yvex_cli_out_writef(stdout, "graph.full_transformer_mlp: %s\n", has_mlp ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.moe_router: %s\n",
           collections && collections->has_moe_router ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.expert_dispatch: %s\n",
           collections && collections->has_moe_expert ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.output_head_projection: %s\n",
           collections && collections->has_output_head ? "planned" : "missing-tensor");
}

typedef enum descriptor_blocker_rule {
    DESCRIPTOR_BLOCKER_COUNT,
    DESCRIPTOR_BLOCKER_ATTENTION,
    DESCRIPTOR_BLOCKER_MLP,
    DESCRIPTOR_BLOCKER_TOKENIZER,
    DESCRIPTOR_BLOCKER_UNKNOWN,
    DESCRIPTOR_BLOCKER_FIXED
} descriptor_blocker_rule;

typedef struct descriptor_collection_spec {
    const char *name;
    size_t count_offset;
    size_t bytes_offset;
    unsigned int required_mask;
    descriptor_blocker_rule blocker_rule;
    const char *missing_blocker;
    const char *runtime_consumer;
} descriptor_collection_spec;

#define COLLECTION_OFF(member_) offsetof(yvex_fullmodel_collections, member_)
#define REQUIRED_MASK(prefill_, decode_, logits_, generation_) \
    ((unsigned int)(prefill_) | ((unsigned int)(decode_) << 1u) | \
     ((unsigned int)(logits_) << 2u) | ((unsigned int)(generation_) << 3u))

static const char *const descriptor_roles[] = {
    "token_embedding", "attention_norm", "post_attention_norm", "final_norm",
    "q_projection", "k_projection", "v_projection", "o_projection",
    "mlp_gate", "mlp_up", "mlp_down", "moe_router", "moe_expert_gate",
    "moe_expert_up", "moe_expert_down", "output_head", "tokenizer_metadata", "unknown"};

static const descriptor_collection_spec descriptor_collections[] = {
    {"embedding", COLLECTION_OFF(embedding), COLLECTION_OFF(embedding_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_COUNT,
     "embedding collection missing", "planned"},
    {"normalization", COLLECTION_OFF(normalization), COLLECTION_OFF(normalization_bytes),
     REQUIRED_MASK(1, 1, 1, 1), DESCRIPTOR_BLOCKER_COUNT,
     "normalization collection missing", "planned"},
    {"attention", COLLECTION_OFF(attention), COLLECTION_OFF(attention_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_ATTENTION,
     "attention Q/K/V/O tensors missing", "planned"},
    {"mlp", COLLECTION_OFF(mlp), COLLECTION_OFF(mlp_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_MLP, "MLP tensors missing", "planned"},
    {"moe", COLLECTION_OFF(moe), COLLECTION_OFF(moe_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_COUNT,
     "MoE tensors missing or not identified", "planned"},
    {"output", COLLECTION_OFF(output), COLLECTION_OFF(output_bytes),
     REQUIRED_MASK(0, 1, 1, 1), DESCRIPTOR_BLOCKER_COUNT, "output head missing", "planned"},
    {"tokenizer-runtime-input", COLLECTION_OFF(tokenizer), COLLECTION_OFF(tokenizer_bytes),
     REQUIRED_MASK(1, 1, 1, 1), DESCRIPTOR_BLOCKER_TOKENIZER,
     "tokenizer metadata missing", "planned"},
    {"kv-cache-runtime", (size_t)-1, (size_t)-1, REQUIRED_MASK(1, 1, 0, 1),
     DESCRIPTOR_BLOCKER_FIXED, "real attention-backed KV writes unsupported", "unsupported"},
    {"unknown", COLLECTION_OFF(unknown), COLLECTION_OFF(unknown_bytes),
     REQUIRED_MASK(0, 0, 0, 0), DESCRIPTOR_BLOCKER_UNKNOWN, "unknown tensor role", "unsupported"},
};

#undef REQUIRED_MASK
#undef COLLECTION_OFF

/* Read one count field selected by an immutable renderer table. */
/* Purpose: Compute collection value for its CLI invariant (`collection_value`). */
static unsigned long long collection_value(const yvex_fullmodel_collections *collections,
                                           size_t offset)
{
    if (!collections || offset == (size_t)-1) return 0ull;
    return *(const unsigned long long *)((const unsigned char *)collections + offset);
}

/* Resolve whether a descriptor collection satisfies its exact role contract. */
/* Purpose: Transfer bounded descriptor collection ready data (`descriptor_collection_ready`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int descriptor_collection_ready(const descriptor_collection_spec *spec,
                                       const yvex_fullmodel_collections *collections,
                                       unsigned long long count)
{
    switch (spec->blocker_rule) {
    case DESCRIPTOR_BLOCKER_ATTENTION: return fullmodel_has_attention_collection(collections);
    case DESCRIPTOR_BLOCKER_MLP: return fullmodel_has_mlp_collection(collections);
    case DESCRIPTOR_BLOCKER_TOKENIZER:
        return collections && collections->has_tokenizer_metadata;
    case DESCRIPTOR_BLOCKER_UNKNOWN: return count == 0ull;
    case DESCRIPTOR_BLOCKER_FIXED: return 0;
    case DESCRIPTOR_BLOCKER_COUNT: return count > 0ull;
    }
    return 0;
}

/* Purpose: Render fullmodel print descriptor inventory from typed facts (`fullmodel_print_descriptor_inventory`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_print_descriptor_inventory(
    yvex_model_context *ctx,
    const yvex_fullmodel_collections *collections,
    const char *backend)
{
    size_t i;

    for (i = 0; i < sizeof(descriptor_roles) / sizeof(descriptor_roles[0]); ++i) {
        fullmodel_print_descriptor_role(ctx, collections, descriptor_roles[i], backend);
    }

yvex_cli_out_writef(stdout, "embedding_descriptor: %s\n",
    collections && collections->embedding > 0ull ? "present" : "missing");
yvex_cli_out_writef(stdout, "normalization_descriptor: %s\n",
    collections && collections->normalization > 0ull ? "present" : "missing");
yvex_cli_out_writef(stdout, "attention_descriptor: %s\n",
    fullmodel_has_attention_collection(collections) ? "present" : "missing");
yvex_cli_out_writef(stdout, "mlp_descriptor: %s\n", fullmodel_has_mlp_collection(collections) ? "present" : "missing");
yvex_cli_out_writef(stdout, "moe_descriptor: %s\n",
    collections && collections->moe > 0ull ? "present" : "planned-or-missing");
yvex_cli_out_writef(stdout, "output_descriptor: %s\n",
    collections && collections->output > 0ull ? "present" : "missing");
yvex_cli_out_writef(stdout, "tokenizer_descriptor: %s\n",
    collections && collections->has_tokenizer_metadata ? "present" : "missing");
yvex_cli_out_writef(stdout, "kv_descriptor: unsupported-real-attention-backed-kv\n");

    for (i = 0; i < sizeof(descriptor_collections) / sizeof(descriptor_collections[0]); ++i) {
        const descriptor_collection_spec *spec = &descriptor_collections[i];
        unsigned long long count = collection_value(collections, spec->count_offset);
        unsigned long long bytes = collection_value(collections, spec->bytes_offset);
        int ready = descriptor_collection_ready(spec, collections, count);

        fullmodel_print_descriptor_collection(
            spec->name, count, bytes, (spec->required_mask & 1u) != 0u,
            (spec->required_mask & 2u) != 0u, (spec->required_mask & 4u) != 0u,
            (spec->required_mask & 8u) != 0u, spec->runtime_consumer,
            ready ? "none" : spec->missing_blocker);
    }
}

/* Purpose: Render fullmodel print descriptor report from typed facts (`fullmodel_print_descriptor_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_descriptor_report(const yvex_cli_fullmodel_options *options,
                                              yvex_model_ref *ref,
                                              yvex_model_context *ctx,
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
    int descriptor_complete = role_coverage &&
                              (strcmp(role_coverage, "complete") == 0 ||
                               strcmp(role_coverage, "observed") == 0);
    const char *descriptor_status = selected_target ? "partial" :
                                    (descriptor_complete ? "complete" : "partial");
    const char *materialization_plan_status = selected_target ? "partial" : "ready";
    const char *materialization_proof_status = selected_target ? "refused-selected-runtime-slice" :
                                               (descriptor_complete
                                                    ? "available-controlled-tiny-proof"
                                                    : "blocked-missing-roles");
    const char *full_materialization = selected_target ? "refused-selected-runtime-slice" :
                                      (descriptor_complete
                                           ? "controlled-tiny-proof-available"
                                           : "planned");
    unsigned long long cuda_bytes = strcmp(backend, "cuda") == 0 ? total_tensor_bytes : 0ull;
    unsigned long long cpu_bytes = strcmp(backend, "cuda") == 0 ? 0ull : total_tensor_bytes;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    yvex_cli_out_lines(stdout, literal_pair_16, sizeof(literal_pair_16) / sizeof(literal_pair_16[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "format: %s\n", options && options->format ? options->format : "text");
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\n");
    yvex_cli_out_writef(stdout, "materialization_plan_status: %s\n", materialization_plan_status);
    yvex_cli_out_writef(stdout, "materialization_proof_status: %s\n", materialization_proof_status);
    yvex_cli_out_writef(stdout, "runtime_descriptor: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_descriptor_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "runtime_descriptor_kind: fullmodel-planning\n");
    yvex_cli_out_writef(stdout, "family: %s\n", fullmodel_family_from_arch(arch));
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(arch));
    yvex_cli_out_writef(stdout, "model_class: %s\n",
        selected_target ? "selected-runtime-slice" : "descriptor-only-candidate");
    yvex_cli_out_lines(stdout, literal_pair_15, sizeof(literal_pair_15) / sizeof(literal_pair_15[0]));
    yvex_cli_out_writef(stdout, "full_model_materialization: %s\n", full_materialization);
    yvex_cli_out_lines(stdout, literal_lines_2, sizeof(literal_lines_2) / sizeof(literal_lines_2[0]));
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "tensor_role_map_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "tensor_collection_map_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n",
        descriptor_complete ? "complete" : (role_coverage ? role_coverage : "partial"));
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    yvex_cli_out_writef(stdout, "unknown_role_count: %llu\n", collections ? collections->unknown : 0ull);

    fullmodel_print_descriptor_inventory(ctx, collections, backend);

    fullmodel_print_descriptor_graph_requirements(collections);

    yvex_cli_out_lines(stdout, literal_lines_3, sizeof(literal_lines_3) / sizeof(literal_lines_3[0]));
    yvex_cli_out_writef(stdout, "cpu_resident_required_bytes: %llu\n", cpu_bytes);
    yvex_cli_out_writef(stdout, "cuda_resident_required_bytes: %llu\n", cuda_bytes);
    yvex_cli_out_writef(stdout, "host_staged_required_bytes: %llu\n", strcmp(backend,
        "cuda") == 0 ? total_tensor_bytes : 0ull);
    yvex_cli_out_lines(stdout, literal_lines_4, sizeof(literal_lines_4) / sizeof(literal_lines_4[0]));
    yvex_cli_out_writef(stdout, "output_head_present: %s\n",
        collections && collections->has_output_head ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    yvex_cli_out_writef(stdout, "output_head_dtype: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? yvex_dtype_name(fullmodel_descriptor_find_tensor(ctx, "output_head")->dtype)
               : "unknown");
    yvex_cli_out_writef(stdout, "vocab_size: %s\n",
        collections && collections->has_output_head ? "from-output-head-shape" : "unknown");
    yvex_cli_out_lines(stdout, literal_lines_5, sizeof(literal_lines_5) / sizeof(literal_lines_5[0]));

    yvex_cli_out_writef(stdout, "tokenizer_requirements_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked");
    yvex_cli_out_writef(stdout, "tokenizer_metadata_present: %s\n",
           collections && collections->has_tokenizer_metadata ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_6, sizeof(literal_lines_6) / sizeof(literal_lines_6[0]));

    yvex_cli_out_writef(stdout, "backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    yvex_cli_out_writef(stdout, "backend.cpu.available: true\n");
    yvex_cli_out_writef(stdout, "backend.cuda.context_available: %s\n",
        yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend.memory_known: %s\n", fit.memory_known ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend.required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend.fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend.fit_reason: %s\n", fit.fit_reason);
    yvex_cli_out_lines(stdout, literal_lines_7, sizeof(literal_lines_7) / sizeof(literal_lines_7[0]));

    yvex_cli_out_writef(stdout, "runtime_blockers: %s\n",
           selected_target
               ? "full runtime tensor set incomplete; attention Q/K/V/O tensors missing; MLP/MoE tensors "
                   "missing; output head missing; real transformer prefill unsupported; real attention-"
                   "backed KV writes unsupported; full model decode unsupported; real output-head logits "
                   "unsupported; real vocabulary sampling unsupported; full model execution unsupported"
               : "real transformer prefill unsupported; real attention-backed KV writes unsupported; full "
                   "model decode unsupported; real output-head logits runtime unsupported; real vocabulary "
                   "sampling unsupported; full model execution unsupported");
    yvex_cli_out_writef(stdout, "descriptor_blockers: %s\n",
           selected_target
               ? "selected-runtime-slice is partial descriptor only"
               : "runtime family adapter boundary remains planned");
    yvex_cli_out_lines(stdout, literal_lines_8, sizeof(literal_lines_8) / sizeof(literal_lines_8[0]));
    fullmodel_print_descriptor_phases(descriptor_status, descriptor_status, NULL);
}
