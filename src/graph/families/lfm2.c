/* Source-faithful Liquid parameter compilation. Topology and tensor semantics
 * are supplied by the source family; common owners execute and publish bytes. */
#include "src/graph/private.h"
#include <yvex/internal/families/lfm2.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/compiler_source.h>
#include <yvex/internal/artifact_lowering.h>
#include <yvex/internal/compilation.h>
#include <yvex/internal/gguf.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/operator_graph.h>
#include <yvex/internal/tokenizer.h>
#include <stdlib.h>
#include <string.h>

#define LFM_TRANSFORM "a161c69911d1e270bd0c53f85de6fb773367ec186f352d2d0b8fb82e3dbfe6c8"
#define LFM_PRESET "lfm2.5-24b-a2b-source-faithful-v1"

static int lfm_role(const yvex_lfm2_architecture *a, const yvex_lfm2_tensor_binding *b,
    yvex_tensor_role *role, yvex_tensor_collection *collection)
{
    static const yvex_tensor_role roles[YVEX_LFM2_ROLE_COUNT] = {
        [YVEX_LFM2_ROLE_EMBEDDING] = YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
        [YVEX_LFM2_ROLE_FINAL_NORM] = YVEX_TENSOR_ROLE_OUTPUT_NORM,
        [YVEX_LFM2_ROLE_OPERATOR_NORM] = YVEX_TENSOR_ROLE_ATTENTION_NORM,
        [YVEX_LFM2_ROLE_FFN_NORM] = YVEX_TENSOR_ROLE_FFN_NORM,
        [YVEX_LFM2_ROLE_CONV_WEIGHT] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_CONVOLUTION,
        [YVEX_LFM2_ROLE_CONV_IN] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_INPUT_PROJECTION,
        [YVEX_LFM2_ROLE_CONV_OUT] = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_OUTPUT,
        [YVEX_LFM2_ROLE_QUERY] = YVEX_TENSOR_ROLE_ATTENTION_Q,
        [YVEX_LFM2_ROLE_KEY] = YVEX_TENSOR_ROLE_ATTENTION_K,
        [YVEX_LFM2_ROLE_VALUE] = YVEX_TENSOR_ROLE_ATTENTION_V,
        [YVEX_LFM2_ROLE_ATTENTION_OUT] = YVEX_TENSOR_ROLE_ATTENTION_OUT,
        [YVEX_LFM2_ROLE_QUERY_NORM] = YVEX_TENSOR_ROLE_ATTENTION_Q_NORM,
        [YVEX_LFM2_ROLE_KEY_NORM] = YVEX_TENSOR_ROLE_ATTENTION_K_NORM,
        [YVEX_LFM2_ROLE_DENSE_GATE] = YVEX_TENSOR_ROLE_FFN_GATE,
        [YVEX_LFM2_ROLE_DENSE_UP] = YVEX_TENSOR_ROLE_FFN_UP,
        [YVEX_LFM2_ROLE_DENSE_DOWN] = YVEX_TENSOR_ROLE_FFN_DOWN,
        [YVEX_LFM2_ROLE_ROUTER] = YVEX_TENSOR_ROLE_MOE_ROUTER,
        [YVEX_LFM2_ROLE_ROUTER_BIAS] = YVEX_TENSOR_ROLE_MOE_ROUTER_BIAS,
        [YVEX_LFM2_ROLE_EXPERT_GATE] = YVEX_TENSOR_ROLE_MOE_EXPERT_GATE,
        [YVEX_LFM2_ROLE_EXPERT_UP] = YVEX_TENSOR_ROLE_MOE_EXPERT_UP,
        [YVEX_LFM2_ROLE_EXPERT_DOWN] = YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN};
    if (!a || !b || b->role <= YVEX_LFM2_ROLE_UNKNOWN || b->role >= YVEX_LFM2_ROLE_COUNT)
        return 0;
    *role = roles[b->role];
    if (b->role == YVEX_LFM2_ROLE_OPERATOR_NORM && a->mixer[b->layer] == YVEX_LFM2_MIXER_CONV)
        *role = YVEX_TENSOR_ROLE_SEQUENCE_MIXER_BLOCK_NORM;
    if (b->role == YVEX_LFM2_ROLE_EMBEDDING) *collection = YVEX_TENSOR_COLLECTION_GLOBAL;
    else if (b->role <= YVEX_LFM2_ROLE_FFN_NORM) *collection = YVEX_TENSOR_COLLECTION_NORM;
    else if (b->role <= YVEX_LFM2_ROLE_CONV_OUT) *collection = YVEX_TENSOR_COLLECTION_SEQUENCE_MIXER;
    else if (b->role <= YVEX_LFM2_ROLE_KEY_NORM) *collection = YVEX_TENSOR_COLLECTION_ATTENTION;
    else if (b->role <= YVEX_LFM2_ROLE_DENSE_DOWN) *collection = YVEX_TENSOR_COLLECTION_DENSE_FFN;
    else if (b->role <= YVEX_LFM2_ROLE_ROUTER_BIAS) *collection = YVEX_TENSOR_COLLECTION_ROUTER;
    else *collection = YVEX_TENSOR_COLLECTION_ROUTED_EXPERT;
    return *role != YVEX_TENSOR_ROLE_UNKNOWN;
}

typedef struct {
    const yvex_lfm2_architecture *architecture;
    yvex_source_tensor_snapshot *snapshot;
    unsigned long long count, terminals;
    unsigned long long collection[YVEX_TENSOR_COLLECTION_COUNT], standard, extension;
    /* Source ordinals, not inferred file ordering, bind each expert operand. */
    unsigned long long experts[YVEX_LFM2_LAYER_CAP][3][YVEX_LFM2_EXPERT_CAP];
} lfm_projection;

static int lfm_account(lfm_projection *p, yvex_tensor_role role,
    yvex_tensor_collection collection, unsigned long long layer, yvex_error *err)
{
    char name[256]; const char *reason = NULL;
    yvex_gguf_name_provenance provenance;
    if (!yvex_gguf_name_map_resolve(role, 0, layer, YVEX_TRANSFORM_IR_NO_ID,
        name, sizeof(name), &provenance, &reason)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "lfm2.parameters", reason);
        return YVEX_ERR_UNSUPPORTED;
    }
    p->collection[collection]++; p->terminals++;
    p->standard += provenance == YVEX_GGUF_NAME_PINNED_STANDARD;
    p->extension += provenance == YVEX_GGUF_NAME_YVEX_EXTENSION;
    return YVEX_OK;
}

static int lfm_bank(lfm_projection *p, yvex_transform_recipe_sink *sink,
    unsigned long long layer, unsigned int projection, yvex_transform_failure *failure, yvex_error *err)
{
    const yvex_lfm2_architecture *a = p->architecture;
    yvex_transform_source_spec operands[YVEX_LFM2_EXPERT_CAP];
    yvex_tensor_role role = projection == 0u ? YVEX_TENSOR_ROLE_MOE_EXPERT_GATE :
        projection == 1u ? YVEX_TENSOR_ROLE_MOE_EXPERT_UP : YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
    unsigned long long rows = projection == 2u ? a->hidden : a->expert_intermediate;
    unsigned long long cols = projection == 2u ? a->expert_intermediate : a->hidden;
    for (unsigned long long e = 0u; e < a->experts; ++e) {
        unsigned long long index = p->experts[layer][projection][e];
        const yvex_native_weight_info *tensor = yvex_source_tensor_snapshot_at(p->snapshot, index);
        if (!tensor) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "lfm2.parameters", "expert operand missing from audited source");
            return YVEX_ERR_FORMAT;
        }
        yvex_transform_source_requirement r = {.source_name = tensor->name, .requirement_index = index,
            .source_dtype = YVEX_NATIVE_DTYPE_BF16, .value_dtype = YVEX_TRANSFORM_DTYPE_BF16,
            .shape = {.rank = 2u, .dims = {rows, cols}}, .scope = YVEX_TRANSFORM_SCOPE_MAIN_LAYER,
            .subsystem = YVEX_TRANSFORM_SUBSYSTEM_ROUTED_EXPERT, .role_hint = role,
            .layer_index = layer, .auxiliary_index = YVEX_TRANSFORM_IR_NO_ID,
            .expert_index = e, .required_uses = 1u};
        int rc = yvex_transform_recipe_sink_resolve_source(sink, &r, &operands[e], failure, err);
        if (rc != YVEX_OK) return rc;
    }
    yvex_transform_shape shape = {.rank = 3u, .dims = {a->experts, rows, cols}};
    yvex_transform_precision_constraint precision = {.flags = YVEX_TRANSFORM_PRECISION_EXACT,
        .allowed_physical_classes = YVEX_TRANSFORM_PHYSICAL_BF16};
    yvex_transform_node_spec node = {.kind = YVEX_TRANSFORM_OP_STACK, .axis = 0u,
        .numeric = YVEX_TRANSFORM_NUMERIC_EXACT, .ordering = YVEX_TRANSFORM_ORDER_AXIS,
        .payload_execution_required = 1};
    int rc = yvex_transform_recipe_add_terminal(sink, role, YVEX_TENSOR_COLLECTION_ROUTED_EXPERT,
        YVEX_TENSOR_SCOPE_MAIN_LAYER, layer, YVEX_TRANSFORM_IR_NO_ID, operands, a->experts,
        &shape, YVEX_TRANSFORM_DTYPE_BF16, &precision, &node, failure, err);
    return rc == YVEX_OK ? lfm_account(p, role, YVEX_TENSOR_COLLECTION_ROUTED_EXPERT, layer, err) : rc;
}

static int lfm_transform(void *context, yvex_transform_recipe_sink *sink,
    yvex_transform_failure *failure, yvex_error *err)
{
    lfm_projection *p = context;
    memset(p->experts, 0xff, sizeof(p->experts));
    for (unsigned long long i = 0u; i < p->count; ++i) {
        const yvex_native_weight_info *tensor = yvex_source_tensor_snapshot_at(p->snapshot, i);
        yvex_lfm2_tensor_binding binding;
        yvex_tensor_role role; yvex_tensor_collection collection;
        int rc = yvex_model_register_lfm2()->tensor_classify(p->architecture, tensor, &binding, err);
        if (rc != YVEX_OK) return rc;
        if (!lfm_role(p->architecture, &binding, &role, &collection)) return YVEX_ERR_FORMAT;
        if (collection == YVEX_TENSOR_COLLECTION_ROUTED_EXPERT) {
            unsigned int projection = binding.role == YVEX_LFM2_ROLE_EXPERT_GATE ? 0u :
                binding.role == YVEX_LFM2_ROLE_EXPERT_UP ? 1u : 2u;
            if (p->experts[binding.layer][projection][binding.expert] != ~0ull) return YVEX_ERR_FORMAT;
            p->experts[binding.layer][projection][binding.expert] = i;
            continue;
        }
        yvex_transform_direct_recipe recipe = {.source_name = tensor->name, .role = role,
            .collection = collection, .scope = binding.layer == ~0ull ?
                YVEX_TENSOR_SCOPE_GLOBAL : YVEX_TENSOR_SCOPE_MAIN_LAYER,
            .layer = binding.layer, .auxiliary = YVEX_TRANSFORM_IR_NO_ID,
            .expert = YVEX_TRANSFORM_IR_NO_ID, .requirement_index = i, .source_dtype = tensor->dtype,
            .shape = {.rank = tensor->rank}};
        for (unsigned int d = 0u; d < tensor->rank; ++d) recipe.shape.dims[d] = tensor->dims[d];
        rc = yvex_transform_recipe_add_direct(sink, &recipe, failure, err);
        if (rc == YVEX_OK) rc = lfm_account(p, role, collection, binding.layer, err);
        if (rc != YVEX_OK) return rc;
    }
    for (unsigned long long l = p->architecture->dense_layers; l < p->architecture->layers; ++l)
        for (unsigned int projection = 0u; projection < 3u; ++projection) {
            int rc = lfm_bank(p, sink, l, projection, failure, err);
            if (rc != YVEX_OK) return rc;
        }
    return YVEX_OK;
}

static int lfm_lowering(yvex_artifact_lowering_map **out, const yvex_transform_ir *transform,
    const lfm_projection *p, yvex_error *err)
{
    const yvex_lfm2_architecture *a = p->architecture;
    yvex_artifact_lowering_metadata metadata[] = {
        {.key = "general.architecture", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING, .string_value = "lfm2_moe"},
        {.key = "general.name", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING, .string_value = "LFM2.5-24B-A2B"},
        {.key = "general.source.repository", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING,
            .string_value = YVEX_LFM2_REPOSITORY},
        {.key = "general.source.revision", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING,
            .string_value = YVEX_LFM2_REVISION},
        {.key = "general.license", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING,
            .string_value = "lfm-open-license-v1.0"},
        {.key = "lfm2_moe.block_count", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64, .u64_value = a->layers},
        {.key = "lfm2_moe.embedding_length", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64, .u64_value = a->hidden},
        {.key = "lfm2_moe.vocabulary_size", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64, .u64_value = a->vocabulary},
        {.key = "lfm2_moe.context_length", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64, .u64_value = a->context},
        {.key = "lfm2_moe.attention.head_count", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64, .u64_value = a->heads},
        {.key = "lfm2_moe.attention.head_count_kv", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64,
            .u64_value = a->kv_heads},
        {.key = "lfm2_moe.expert_count", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64, .u64_value = a->experts},
        {.key = "lfm2_moe.expert_used_count", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64,
            .u64_value = a->selected_experts},
        {.key = "lfm2_moe.conv_kernel", .type = YVEX_ARTIFACT_LOWERING_METADATA_U64,
            .u64_value = a->convolution_kernel},
        {.key = "yvex.lfm2.architecture", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING},
        {.key = "yvex.lfm2.numerical_policy", .type = YVEX_ARTIFACT_LOWERING_METADATA_STRING,
            .string_value = "bf16-rounded-norm-rope-unclamped-sigmoid-moe-uncached-conv-v1"}};
    yvex_core_text_copy(metadata[14].string_value, sizeof(metadata[14].string_value), a->identity);
    yvex_artifact_lowering_policy policy = {.schema_version = YVEX_ARTIFACT_LOWERING_POLICY_SCHEMA_V1,
        .source_contribution_count = p->count, .descriptor_count = p->terminals,
        .trunk_descriptor_count = p->terminals, .pinned_standard_count = p->standard,
        .extension_count = p->extension, .metadata = metadata,
        .metadata_count = sizeof(metadata) / sizeof(metadata[0])};
    memcpy(policy.trunk_collection_counts, p->collection, sizeof(p->collection));
    yvex_artifact_lowering_failure failure;
    return yvex_artifact_lowering_operations.build(out, transform, &policy, &failure, err);
}

int yvex_lfm2_source_lower(yvex_transform_ir **transform, yvex_artifact_lowering_map **lowering,
    const yvex_source_verification *verification, yvex_source_tensor_snapshot *snapshot,
    yvex_compilation_source_failure *failure, yvex_error *err)
{
    yvex_lfm2_architecture a;
    yvex_lfm2_inventory inventory;
    yvex_source_tensor_snapshot_facts facts;
    yvex_transform_failure tf;
    if (transform) *transform = NULL;
    if (lowering) *lowering = NULL;
    if (failure) memset(failure, 0, sizeof(*failure));
    if (!transform || !lowering || !verification || !snapshot ||
        !verification->manifest_payload_trusted ||
        !yvex_sha256_hex_valid(verification->manifest_payload_identity)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "lfm2.parameters", "authenticated source payload required");
        return YVEX_ERR_FORMAT;
    }
    int rc = yvex_model_register_lfm2()->open(verification, &a, err);
    if (rc == YVEX_OK) rc = yvex_model_register_lfm2()->snapshot_audit(&a, snapshot, &inventory, err);
    if (rc == YVEX_OK) rc = yvex_source_tensor_snapshot_facts_get(snapshot, &facts, err);
    if (rc != YVEX_OK) return rc;
    if (facts.identity != verification->source_snapshot_identity || facts.tensor_count != inventory.tensors) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "lfm2.parameters", "source snapshot identity mismatch");
        return YVEX_ERR_FORMAT;
    }
    lfm_projection *p = calloc(1u, sizeof(*p));
    if (!p) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "lfm2.parameters", "recipe allocation failed");
        return YVEX_ERR_NOMEM;
    }
    p->architecture = &a; p->snapshot = snapshot; p->count = inventory.tensors;
    unsigned long long terminals = inventory.tensors - (a.layers - a.dense_layers) * 3u * (a.experts - 1u);
    yvex_transform_header header = {.schema_version = YVEX_TRANSFORM_IR_SPECIALIZATION_SCHEMA_VERSION,
        .logical_model_identity = a.identity, .source_snapshot_identity = facts.identity,
        .required_payload_identity = verification->manifest_payload_identity,
        .payload_trust_class = verification->manifest_payload_trust_class,
        .architecture_identity = a.identity, .role_map_identity = inventory.identity,
        .source_population_count = facts.tensor_count, .expected_source_count = facts.tensor_count,
        .expected_terminal_count = terminals, .header_scan_count = facts.header_scan_count};
    yvex_transform_builder_options options = {.source_snapshot = snapshot};
    yvex_transform_budget_default(&options.budget);
    rc = yvex_transform_recipe_compile(transform, &header, lfm_transform, p, &options, &tf, err);
    if (rc == YVEX_OK) rc = lfm_lowering(lowering, *transform, p, err);
    if (rc != YVEX_OK) {
        if (failure) failure->code = *transform ? YVEX_COMPILATION_SOURCE_FAILURE_LOWERING :
            YVEX_COMPILATION_SOURCE_FAILURE_TRANSFORM_IR;
        yvex_artifact_lowering_operations.close(*lowering); *lowering = NULL;
        yvex_transform_ir_release(transform);
    }
    free(p);
    return rc;
}

static const void *lfm_source_identity(void) { return yvex_source_target_identity_find(YVEX_LFM2_TARGET); }
static const yvex_compilation_source_projection lfm_source_projection = {
    .schema_version = YVEX_COMPILATION_SOURCE_PROJECTION_SCHEMA_V1,
    .expected_mapping_identity = 11773153470211518808ull,
    .required_contribution_mask = YVEX_COMPILATION_SOURCE_REQUIRE_DIRECT |
        YVEX_COMPILATION_SOURCE_REQUIRE_GLOBAL | YVEX_COMPILATION_SOURCE_REQUIRE_NORM,
    .source_identity = lfm_source_identity, .lower = yvex_lfm2_source_lower,
    .lowering = &yvex_artifact_lowering_operations};

static int lfm_source_open(yvex_family_compilation_source *out,
    const yvex_compilation_runtime_binding_request *request, yvex_error *err)
{
    yvex_compilation_source_session *source = NULL;
    yvex_compilation_source_failure failure;
    yvex_compilation_source_options options = {0};
    if (out) memset(out, 0, sizeof(*out));
    if (!out || !request || !request->source_path || !request->models_root ||
        request->source_stream_count > 64u) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "lfm2.source", "exact source and bounded worker count required");
        return YVEX_ERR_INVALID_ARG;
    }
    options.source_path = request->source_path; options.models_root = request->models_root;
    options.manifest_path = request->source_manifest_path;
    yvex_source_payload_budget_default(&options.budget);
    if (request->source_stream_count) {
        options.budget.maximum_streams = request->source_stream_count;
        if (options.budget.maximum_open_handles < request->source_stream_count)
            options.budget.maximum_open_handles = request->source_stream_count;
        options.budget.maximum_inflight_host_bytes = options.budget.chunk_bytes * request->source_stream_count;
    }
    options.chunk_bytes = options.budget.chunk_bytes; options.page_bytes = options.budget.page_bytes;
    int rc = yvex_compilation_source_operations.open(&source, &options, &lfm_source_projection, &failure, err);
    if (rc != YVEX_OK) { yvex_compilation_source_operations.close(source); return rc; }
    out->owner = source; out->verification = yvex_compilation_source_operations.verification(source);
    out->transform_ir = yvex_compilation_source_operations.transform(source);
    out->transform_binding = yvex_compilation_source_operations.binding(source);
    out->artifact_lowering = yvex_compilation_source_operations.lowering(source);
    out->source_summary = yvex_compilation_source_operations.summary(source);
    out->lowering_context = out->artifact_lowering;
    out->tokenizer_vocabulary_size = out->verification->tokenizer_effective_vocab_size;
    if (!out->source_summary || !out->source_summary->complete || out->tokenizer_vocabulary_size != 125017u) {
        yvex_compilation_source_operations.close(source); memset(out, 0, sizeof(*out));
        yvex_error_set(err, YVEX_ERR_FORMAT, "lfm2.source", "complete pinned source/tokenizer population required");
        return YVEX_ERR_FORMAT;
    }
    return YVEX_OK;
}

static void lfm_source_close(void *source) { yvex_compilation_source_operations.close(source); }

static int lfm_attention_layer(const void *context, unsigned long long ordinal,
    yvex_semantic_attention_layer *out)
{
    const yvex_ir_module *m = context;
    unsigned long long layer = 0u, attention = 0u;
    for (size_t i = 0u; i < yvex_ir_operation_count(m); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(m, (yvex_ir_id)i);
        if (!strcmp(op->definition->name, "sequence.double_gated_conv")) { layer++; continue; }
        if (strcmp(op->definition->name, "attention.causal")) continue;
        if (attention++ != ordinal) { layer++; continue; }
        if (!out) return 0;
        memset(out, 0, sizeof(*out));
        out->ordinal = ordinal; out->layer_index = layer;
        out->predictor_index = YVEX_ATTENTION_NO_TENSOR_INDEX; out->tensor_scope = YVEX_TENSOR_SCOPE_MAIN_LAYER;
        out->attention_class = YVEX_ATTENTION_CLASS_SWA;
        out->compute_contract = YVEX_ATTENTION_COMPUTE_BF16_EAGER_V1;
#define LFM_ATTR(name) yvex_ir_attribute_get(m, (yvex_ir_id)i, name)->value.integer
        out->sliding_window = LFM_ATTR("maximum_context");
        out->query_heads = LFM_ATTR("query_heads"); out->kv_heads = LFM_ATTR("kv_heads");
        out->head_dimension = LFM_ATTR("head_dimension"); out->rope_head_dimension = LFM_ATTR("rotary_dimension");
        out->hidden_dimension = out->query_heads * out->head_dimension;
        out->position.rope_dimension = out->rope_head_dimension;
        out->position.theta = LFM_ATTR("theta"); out->position.scaling_factor = 1u;
        out->position.original_context = out->position.maximum_context = out->sliding_window;
        out->position.partial_rope = 1;
#undef LFM_ATTR
        /* Input and Q/K normalization are explicit program operations; the
         * attention plan must not normalize them a second time. */
        return 1;
    }
    return 0;
}

static int lfm_semantic(yvex_semantic_model_ir **out, const yvex_source_verification *verification,
    unsigned int attention_compute, unsigned int mhc_projection, unsigned int matrix_projection, yvex_error *err)
{
    yvex_lfm2_architecture a;
    yvex_ir_module *program = NULL;
    yvex_model_execution_descriptor descriptor;
    yvex_semantic_numeric_contract numeric = {.schema_version = YVEX_SEMANTIC_NUMERIC_CONTRACT_SCHEMA_V1,
        .numeric_schema_version = 1u, .compute_policy_count = 1u, .activation_policy_count = 1u,
        .sparse_topk_policy_count = 1u, .algorithm_revision = "lfm-bf16-uncached-conv-sigmoid-moe-v1"};
    if (out) *out = NULL;
    if (!out || !verification || attention_compute || mhc_projection || matrix_projection) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "lfm2.semantic", "source numerical realization required");
        return YVEX_ERR_UNSUPPORTED;
    }
    int rc = yvex_model_register_lfm2()->open(verification, &a, err);
    if (rc == YVEX_OK) rc = yvex_model_register_lfm2()->program_build(
        &program, &a, verification->manifest_payload_identity, err);
    if (rc != YVEX_OK) return rc;
    yvex_sha256 hash; unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, numeric.algorithm_revision) ||
        !yvex_sha256_update_text(&hash, yvex_ir_identity(program)) || !yvex_sha256_final(&hash, digest))
        rc = YVEX_ERR_STATE;
    else yvex_sha256_hex(digest, numeric.identity);
    yvex_model_execution_descriptor_request d = {.schema_version = YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V3,
        .logical_model_identity = a.identity, .source_model_identity = verification->manifest_payload_identity,
        .attention_schedule_identity = yvex_ir_identity(program),
        .persistent_state_identity = yvex_ir_identity(program),
        .maximum_context = a.context, .original_context = a.context, .rope_scaling = YVEX_MODEL_ROPE_SCALING_NONE,
        .rope_theta = (unsigned long long)a.rope_theta, .rope_scaling_factor = 1u,
        .layer_count = a.layers, .hidden_width = a.hidden, .vocabulary_size = a.vocabulary,
        .attention_heads = a.heads, .kv_heads = a.kv_heads, .head_width = a.head_width,
        .swa_layers = a.attention_layers, .sequence_mixer_layers = a.convolution_layers,
        .sliding_window = a.context, .normalization_epsilon = a.norm_epsilon,
        .routed_experts = a.experts, .experts_per_row = a.selected_experts, .routed_ffn_width = a.expert_intermediate,
        .dense_ffn_width = a.intermediate, .routed_scaling_factor = a.routed_scale,
        .output_input_width = a.hidden, .output_vocabulary_size = a.vocabulary,
        .persistent_state_class_mask = YVEX_MODEL_STATE_CLASS_BIT(YVEX_MODEL_STATE_SWA_RING) |
            YVEX_MODEL_STATE_CLASS_BIT(YVEX_MODEL_STATE_RECURRENT_SEQUENCE),
        .bos_token_id = a.bos, .eos_token_id = a.eos};
    if (rc == YVEX_OK) rc = yvex_model_execution_descriptor_seal(&d, &descriptor, err);
    yvex_semantic_reference_request references[] = {
        {"source-revision", YVEX_LFM2_REVISION},
        {"numerical-reference", "transformers@ddb849abe009d1089e6c691bfc897f27211c663c"},
        {"convolution-realization", "uncached-conv1d-bf16-f32-fma-v1"}};
    yvex_semantic_model_ir_request request = {.schema_version = YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2,
        .family_adapter_id = YVEX_LFM2_ADAPTER_ID, .family_adapter_version = YVEX_LFM2_ADAPTER_VERSION,
        .target_id = YVEX_LFM2_TARGET, .source_model_identity = verification->manifest_payload_identity,
        .logical_model_identity = a.identity, .semantic_payload_identity = descriptor.identity,
        .execution_descriptor = &descriptor, .numeric_contract = &numeric,
        .attention_context = program, .attention_layer = lfm_attention_layer,
        .attention_layer_count = a.attention_layers,
        .references = references, .reference_count = sizeof(references) / sizeof(references[0]), .program = program};
    if (rc == YVEX_OK) rc = yvex_semantic_model_ir_seal(out, &request, err);
    yvex_ir_module_close(&program);
    return rc;
}

static const yvex_quant_artifact_lowering_rule lfm_quant_rules[] = {
    {YVEX_ARTIFACT_LOWERING_TRANSFORM_DIRECT, YVEX_TRANSFORM_OP_IDENTITY,
        YVEX_GGUF_QTYPE_BF16, YVEX_GGUF_QTYPE_BF16, 0, YVEX_NATIVE_DTYPE_BF16},
    {YVEX_ARTIFACT_LOWERING_TRANSFORM_DIRECT, YVEX_TRANSFORM_OP_IDENTITY,
        YVEX_GGUF_QTYPE_F32, YVEX_GGUF_QTYPE_F32, 0, YVEX_NATIVE_DTYPE_F32},
    {YVEX_ARTIFACT_LOWERING_TRANSFORM_SCALAR_STACK, YVEX_TRANSFORM_OP_STACK,
        YVEX_GGUF_QTYPE_BF16, YVEX_GGUF_QTYPE_BF16, 0, YVEX_NATIVE_DTYPE_BF16}};
static const yvex_quant_artifact_lowering_policy lfm_quant_lowering = {
    LFM_PRESET, LFM_PRESET, lfm_quant_rules, sizeof(lfm_quant_rules) / sizeof(lfm_quant_rules[0])};
static int lfm_quant_default(yvex_quant_plan **out, const yvex_transform_ir *transform,
    const yvex_transform_binding *binding, const void *lowering, yvex_error *err)
{
    yvex_quant_failure failure;
    return yvex_quant_plan_build_artifact_lowering_profile(out, transform, binding, lowering,
        &lfm_quant_lowering, YVEX_QUANT_PROFILE_SOURCE_FAITHFUL, NULL, &failure, err);
}
static int lfm_quant_policy(yvex_quant_plan **out, const yvex_transform_ir *transform,
    const yvex_transform_binding *binding, const void *lowering, const yvex_quant_policy *policy,
    const char *imatrix, yvex_error *err)
{
    yvex_quant_failure failure;
    return yvex_quant_plan_build_artifact_lowering_policy(out, transform, binding, lowering,
        &lfm_quant_lowering, policy, imatrix, NULL, &failure, err);
}

static unsigned long long lfm_preset_count(void) { return 1u; }
static const char *lfm_preset_name(unsigned long long index) { return index ? NULL : LFM_PRESET; }
static int lfm_preset_open(yvex_quant_policy **out, const char *name, yvex_error *err)
{
    if (out) *out = NULL;
    if (!out || !name || strcmp(name, LFM_PRESET)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "lfm2.policy", "only source-faithful scalar policy is admitted");
        return YVEX_ERR_UNSUPPORTED;
    }
    yvex_quant_policy_rule rule = {.schema_version = YVEX_QUANT_POLICY_SCHEMA_VERSION,
        .match_mask = YVEX_QUANT_MATCH_PHYSICAL_CLASS, .operation = YVEX_QUANT_POLICY_OPERATION_ANY,
        .scope = YVEX_TENSOR_SCOPE_GLOBAL, .physical_class = YVEX_QUANT_POLICY_PHYSICAL_QUANTIZABLE,
        .qtype = YVEX_QUANT_QTYPE_SOURCE, .requires_cpu_compute = 1, .priority = 10u,
        .label = "preserve original Liquid BF16/F32 parameters"};
    yvex_quant_policy_definition definition = {LFM_PRESET, YVEX_LFM2_TARGET, "built-in-preset", &rule, 1u};
    return yvex_quant_policy_create_definition(out, &definition, err);
}
static const yvex_quant_preset_catalog *lfm_presets(void)
{
    static const yvex_quant_preset_catalog catalog = {YVEX_QUANT_PRESET_CATALOG_SCHEMA_V1,
        YVEX_LFM2_TARGET, lfm_preset_count, lfm_preset_name, lfm_preset_open};
    return &catalog;
}

static int lfm_tokenizer(yvex_tokenizer_family_policy *out, yvex_error *err)
{
    static const yvex_tokenizer_direct_policy policy = {
        .family_adapter_id = YVEX_LFM2_ADAPTER_ID, .family_adapter_version = YVEX_LFM2_ADAPTER_VERSION,
        .tokenizer_kind = YVEX_TOKENIZER_KIND_GGML_GPT2, .model_policy = YVEX_TOKENIZER_MODEL_BPE_BYTELEVEL,
        .prompt_policy = YVEX_TOKENIZER_PROMPT_VERBATIM, .vocabulary_size = 125017u,
        .base_vocabulary_size = 124893u, .merge_count = 293320u, .added_token_count = 124u,
        .special_token_count = 120u, .bos_present = 1, .eos_present = 1, .pad_present = 1,
        .bos_token_id = 124894u, .eos_token_id = 124900u, .pad_token_id = 124893u,
        .architecture = "lfm2_moe", .tokenizer_model = "gpt2", .tokenizer_pre = "llama-bpe",
        .tokenizer_json_identity = "695be7802a0e4b8a81048f0ff5ebb7fc811a0ba5a6be63dbb24deb5a81096f41",
        .tokenizer_config_identity = "11f1de897317b489dd09199284528382eeb17f566398b16e2039bb2266d26b09",
        .prompt_name = "verbatim-lfm2-v1"};
    return yvex_tokenizer_family_policy_compile_direct(out, &policy, err) == YVEX_OK;
}

static int lfm_artifact_admit(const yvex_artifact *artifact, yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure, yvex_error *err)
{
    /* Exact source-preserving production, independently read on 2026-10-20.
     * This catalog authenticates bytes and structure, not model quality or
     * backend execution. Admission rehashes the opened artifact. */
    static const yvex_complete_artifact_admission catalog = {
        .artifact_class = YVEX_ARTIFACT_CLASS_COMPLETE_YVEX,
        .metadata_count = 43u, .tensor_count = 428u,
        .payload_bytes = 47943180288ull, .file_bytes = 47969264224ull,
        .source_snapshot_identity = 7724516236606848092ull,
        .mapping_identity = 11773153470211518808ull,
        .payload_identity = "3dcd02062afc89181c88e308d16d68ec421271ef57f47b7ba7199ebeb99e02d7",
        .transform_identity = LFM_TRANSFORM,
        .profile_identity = "de49ca4964821baf15b88dc03535081d23e53a7c8da87cf0f86ee4c21ea6f90e",
        .profile_name = LFM_PRESET,
        .quant_execution_identity = "7944fc6249a203ee00e3c99ce8fb3d62578f73043960d0e605ac21304c07c65c",
        .payload_plan_identity = "8cc298498265775c452797758bdb107778e54e410f27e8cd59ea4880919207fc",
        .payload_byte_identity = "4a4a1dd8971824144593310f7bf02937e5689b2da513b5ea91953276eda69c29",
        .writer_plan_identity = "79b5132ade37a3601c2cb127b6b2cdea17d6143be1071606a4578ded60a30fbe",
        .artifact_identity = "ffd820b1614f994c4e146c49f5e29278029623ef86a90f67efee95f7bcd7612c",
        .official_reader_revision = YVEX_GGUF_OFFICIAL_READER_REVISION,
        .tokenizer_complete = 1, .native_reader_accepted = 1,
        .official_reader_accepted = 1, .payload_integrity_accepted = 1,
        .materialization_input_ready = 1};
    const yvex_artifact_catalog_contract contract = {.catalog = &catalog};
    return yvex_artifact_admit_catalog(artifact, NULL, NULL, &contract, out, failure, err);
}

static int lfm_runtime(yvex_runtime_descriptor **out, const yvex_complete_artifact_admission *admission,
    yvex_materialization_session *materialization, const void *lowering,
    const yvex_semantic_model_ir *semantic_model, yvex_error *err)
{
    const yvex_semantic_model_ir_summary *semantic = yvex_semantic_model_ir_summary_get(semantic_model);
    yvex_runtime_descriptor_failure failure;
    yvex_materialization_projection projection;
    if (out) *out = NULL;
    if (!semantic || semantic->execution_descriptor.schema_version != YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V3 ||
        semantic->decoder_layer_count || !yvex_semantic_model_ir_program(semantic_model)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "lfm2.runtime", "sealed typed hybrid model required");
        return YVEX_ERR_FORMAT;
    }
    const yvex_semantic_numeric_contract *numeric = &semantic->numeric_contract;
    const yvex_model_execution_descriptor *d = &semantic->execution_descriptor;
    yvex_runtime_descriptor_family_facts facts = {.logical_model_identity = d->logical_model_identity,
        .runtime_numeric_identity = numeric->identity, .runtime_hadamard_revision = numeric->algorithm_revision,
        .runtime_numeric_schema_version = numeric->numeric_schema_version,
        .runtime_compute_policy_count = numeric->compute_policy_count,
        .runtime_activation_policy_count = numeric->activation_policy_count,
        .runtime_sparse_topk_policy_count = numeric->sparse_topk_policy_count,
        .layer_count = d->layer_count, .vocabulary_size = d->vocabulary_size, .model_execution = d};
    int rc = yvex_materialization_project_artifact_lowering(lowering, &projection, err);
    return rc == YVEX_OK ? yvex_runtime_descriptor_build_projected(out, admission,
        materialization, &facts, &projection, &failure, err) : rc;
}

static int lfm_attention_plan(yvex_attention_plan **out,
    const yvex_semantic_model_ir *semantic, const yvex_materialization_session *materialization,
    const yvex_runtime_descriptor *descriptor, yvex_attention_failure *failure, yvex_error *err)
{
    return yvex_attention_plan_build_semantic(out, semantic, YVEX_TENSOR_SCOPE_MAIN_LAYER,
        materialization, descriptor, failure, err);
}

static const yvex_graph_compiler_api *lfm_graph(void)
{
    static const yvex_graph_compiler_api api = {.plan_build = lfm_attention_plan};
    return &api;
}
static int lfm_capabilities(yvex_runtime_capabilities *out)
{ if (!out) return 0; memset(out, 0, sizeof(*out)); return yvex_runtime_capabilities_contract_valid(out); }
static int lfm_transformer(const yvex_runtime_descriptor_summary *r, yvex_transformer_family_policy *out)
{ (void)r; if (!out) return 0; memset(out, 0, sizeof(*out)); return 1; }
static int lfm_logits(yvex_logits_family_policy *out)
{
    if (!out) return 0;
    *out = (yvex_logits_family_policy){.schema_version = YVEX_RUNTIME_LOGITS_SCHEMA_V1, .tied_output_head = 1};
    return 1;
}
static int lfm_speculation(const yvex_runtime_descriptor_summary *r, yvex_speculation_family_policy *out)
{ (void)r; if (!out) return 0; memset(out, 0, sizeof(*out)); return 1; }

static const yvex_family_binding_pipeline lfm_pipeline = {
    .schema_version = YVEX_FAMILY_BINDING_PIPELINE_SCHEMA_V2, .source_open = lfm_source_open,
    .source_close = lfm_source_close, .artifact_admit = lfm_artifact_admit, .semantic_model_build = lfm_semantic,
    .runtime_descriptor_build = lfm_runtime, .quant_plan_default = lfm_quant_default,
    .quant_plan_policy = lfm_quant_policy,
    .tokenizer_architecture = "lfm2_moe", .tokenizer_model = "gpt2", .tokenizer_pre = "llama-bpe"};
static const yvex_family_compiler_adapter lfm_compiler = {
    .schema_version = YVEX_FAMILY_COMPILER_SCHEMA_V3, .adapter_id = YVEX_LFM2_ADAPTER_ID,
    .adapter_version = YVEX_LFM2_ADAPTER_VERSION, .target_id = YVEX_LFM2_TARGET, .family = "lfm2",
    .logical_transform_identity = LFM_TRANSFORM, .graph = lfm_graph,
    .operator_graph_build = yvex_operator_graph_ir_build_program, .execution_capabilities = lfm_capabilities,
    .transformer_policy = lfm_transformer, .logits_policy = lfm_logits, .speculation_policy = lfm_speculation,
    .tokenizer_policy = lfm_tokenizer, .physical_variant = yvex_graph_physical_variant_api_get,
    .binding_pipeline = &lfm_pipeline, .binding_compile = yvex_family_binding_compile};
static const yvex_model_deployment_defaults lfm_defaults = {
    .schema_version = YVEX_MODEL_DEPLOYMENT_DEFAULTS_SCHEMA_CURRENT, .logical_family = "lfm2",
    .logical_model = YVEX_LFM2_TARGET, .quant_preset = LFM_PRESET,
    .backend = "cpu", .engine_kind = "text", .execution_strategy = "target-only"};
static const yvex_graph_execution_binding *lfm_execution(void)
{
    static const yvex_graph_execution_binding binding = {
        .schema_version = YVEX_GRAPH_EXECUTION_BINDING_SCHEMA_V1, .adapter_id = YVEX_LFM2_ADAPTER_ID,
        .adapter_version = YVEX_LFM2_ADAPTER_VERSION, .target_id = YVEX_LFM2_TARGET,
        .family_name = "lfm2", .logical_transform_identity = LFM_TRANSFORM, .operator_family_key = "lfm2",
        .operator_artifact_filename = "lfm2.5-24b-a2b-source-faithful.gguf",
        .source_manifest_filename = "lfm2.5-24b-a2b.source-manifest.json",
        .deployment_defaults = &lfm_defaults, .compiler = &lfm_compiler};
    return &binding;
}
static const yvex_family_descriptor lfm_descriptor = {
    .schema_version = YVEX_FAMILY_DESCRIPTOR_SCHEMA_V1, .target_id = YVEX_LFM2_TARGET,
    .family = "lfm2", .tokenizer_architecture = "lfm2_moe", .tokenizer_pre = "llama-bpe",
    .execution = lfm_execution, .quant_presets = lfm_presets};
static const yvex_family_descriptor *const lfm_targets[] = {&lfm_descriptor};
const yvex_family_target_catalog yvex_graph_family_catalog_lfm2 = {
    .schema_version = YVEX_FAMILY_TARGET_CATALOG_SCHEMA_V1, .family = "lfm2",
    .targets = lfm_targets, .target_count = 1u};
