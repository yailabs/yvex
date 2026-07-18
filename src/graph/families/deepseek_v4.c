/*
 * deepseek_v4.c - DeepSeek-V4 attention recipe and execution composition.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   immutable plan construction, schedule accounting, compressor recurrence,
 *   bounded CPU execution, and graph-level CUDA job composition.
 *
 * Does not own:
 *   reusable numeric primitives, backend kernels, materialization ownership,
 *   persistent KV, generation, CLI parsing, rendering, or release claims.
 *
 * Invariants:
 *   planning reads zero payload bytes; family execution cannot promote the
 *   capability gate while complete reference and CUDA evidence is paused.
 *
 * Boundary:
 *   attention execution readiness does not imply persistent KV, transformer,
 *   prefill, decode, or generation readiness.
 */
#include "src/graph/private.h"

#include "src/model/families.h"

static void plan_hash_activation_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_activation_policy *policy);
static void plan_hash_sparse_topk_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_sparse_topk_policy *policy);
static void graph_plan_close(yvex_attention_plan *plan);

static void attention_compute_identity(yvex_attention_plan *plan)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long i;

    yvex_sha256_init(&hash);
    (void)yvex_attention_hash_text(&hash, "yvex.deepseek.attention.plan.v1");
    (void)yvex_attention_hash_text(&hash, plan->summary.artifact_identity);
    (void)yvex_attention_hash_text(&hash, plan->summary.materialization_plan_identity);
    (void)yvex_attention_hash_text(&hash, plan->summary.logical_model_identity);
    (void)yvex_attention_hash_text(&hash, plan->summary.runtime_descriptor_identity);
    (void)yvex_attention_hash_text(&hash, plan->summary.runtime_numeric_identity);
    (void)yvex_attention_hash_u64(&hash, plan->summary.layer_count);
    for (i = 0ull; i < plan->layer_count; ++i) {
        const yvex_attention_layer_plan *layer = &plan->layers[i];
        (void)yvex_attention_hash_u64(&hash, layer->layer_index);
        (void)yvex_attention_hash_u64(&hash, layer->attention_class);
        (void)yvex_attention_hash_u64(&hash, layer->compression_ratio);
        (void)yvex_attention_hash_u64(&hash, layer->sliding_window);
        (void)yvex_attention_hash_u64(&hash, layer->query_heads);
        (void)yvex_attention_hash_u64(&hash, layer->kv_heads);
        (void)yvex_attention_hash_u64(&hash, layer->head_dimension);
        (void)yvex_attention_hash_u64(&hash, layer->rope_head_dimension);
        (void)yvex_attention_hash_u64(&hash, layer->query_lora_rank);
        (void)yvex_attention_hash_u64(&hash, layer->output_lora_rank);
        (void)yvex_attention_hash_u64(&hash, layer->output_groups);
        (void)yvex_attention_hash_u64(&hash, layer->hidden_dimension);
        (void)yvex_attention_hash_u64(&hash, layer->indexer_heads);
        (void)yvex_attention_hash_u64(&hash, layer->indexer_head_dimension);
        (void)yvex_attention_hash_u64(&hash, layer->indexer_topk);
        plan_hash_activation_policy(&hash,
                                         &layer->attention_kv_activation);
        plan_hash_activation_policy(&hash, &layer->compressor_activation);
        plan_hash_activation_policy(
            &hash, &layer->compressor_rotated_activation);
        plan_hash_activation_policy(&hash,
                                         &layer->indexer_query_activation);
        plan_hash_sparse_topk_policy(&hash, &layer->sparse_topk);
        (void)yvex_attention_hash_u64(&hash, layer->required_binding_count);
        (void)yvex_attention_hash_u64(&hash, layer->qtype_compute_refusal_count);
        (void)yvex_attention_hash_u64(&hash, layer->payload_bytes_bound);
    }
    (void)yvex_sha256_final(&hash, digest);
    yvex_sha256_hex(digest, plan->summary.attention_plan_identity);
}

static void plan_hash_activation_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_activation_policy *policy)
{
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->required : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->stage : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->quantization : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->block_axis : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? policy->block_width : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->scale_format : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->scale_dtype : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->pre_transform : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->tail_policy : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->nonfinite_policy : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->fake_quant_inplace : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->zero_pad_hadamard_to_power_of_two : 0ull);
}

static void plan_hash_sparse_topk_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_sparse_topk_policy *policy)
{
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->required : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->version : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->policy : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? policy->k : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->reject_nonfinite : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->score_descending : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->equal_score_ordinal_ascending : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->plus_zero_equals_minus_zero : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->duplicate_ordinal_refused : 0ull);
    (void)yvex_attention_hash_u64(hash, policy ? (unsigned long long)policy->output_ranked_order : 0ull);
}

/* Contract: verifies one runtime binding can be used by attention admission. */
static int attention_bind_role(
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_layer_plan *layer_plan,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *binding =
        yvex_runtime_descriptor_find_role(
            descriptor, role, scope, layer_index, YVEX_DEEPSEEK_GGUF_NO_INDEX);
    const yvex_quant_numeric_capability *capability;

    layer_plan->required_binding_count++;
    if (!binding || !binding->binding) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, binding,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention binding is missing from runtime descriptor");
    }
    if (!binding->binding->encoded_bytes || !binding->binding->rank) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, binding,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention binding has empty shape or byte range");
    }
    capability = yvex_quant_numeric_capability_at(binding->qtype);
    if (!capability || !capability->identity_known ||
        !capability->storage_admitted ||
        !capability->dedicated_cpu_compute_available) {
        layer_plan->qtype_compute_refusal_count++;
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_QTYPE, binding,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_UNSUPPORTED,
            "DeepSeek attention binding qtype lacks admitted CPU row compute");
    }
    layer_plan->payload_bytes_bound += binding->binding->encoded_bytes;
    return YVEX_OK;
}

static int attention_bind_required_layer_roles(
    const yvex_runtime_descriptor *descriptor,
    const yvex_deepseek_v4_layer_spec *layer,
    yvex_attention_layer_plan *layer_plan,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_tensor_scope scope = YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER;
    unsigned long long layer_index = layer->layer_index;
    int rc;

#define BIND(role_id) do {                                                     \
    rc = attention_bind_role(descriptor, layer_plan, scope, layer_index,        \
                             role_id, failure, err);                           \
    if (rc != YVEX_OK) return rc;                                               \
} while (0)
    BIND(YVEX_TENSOR_ROLE_ATTENTION_SINKS);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_KV_NORM);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_Q_A);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_Q_B);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_KV);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_OUT_A);
    BIND(YVEX_TENSOR_ROLE_ATTENTION_OUT_B);
    if (layer->compressor_required) {
        BIND(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE);
        BIND(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM);
        BIND(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE);
        BIND(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV);
    }
    if (layer->indexer_required) {
        BIND(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE);
        BIND(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM);
        BIND(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE);
        BIND(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV);
        BIND(YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B);
        BIND(YVEX_TENSOR_ROLE_INDEXER_PROJECTION);
    }
#undef BIND
    return YVEX_OK;
}

static void attention_fill_layer_plan(
    yvex_attention_layer_plan *out,
    const yvex_deepseek_v4_layer_spec *layer)
{
    memset(out, 0, sizeof(*out));
    out->layer_index = layer->layer_index;
    out->attention_class = layer->attention_class;
    out->compression_ratio = layer->compression_ratio;
    out->sliding_window = layer->kv.sliding_window;
    out->query_heads = layer->query_heads;
    out->kv_heads = layer->kv_heads;
    out->head_dimension = layer->head_dimension;
    out->rope_head_dimension = layer->rope_head_dimension;
    out->query_lora_rank = layer->query_lora_rank;
    out->output_lora_rank = layer->output_lora_rank;
    out->output_groups = layer->output_groups;
    out->hidden_dimension = layer->tensors.q_a_columns;
    out->indexer_heads = layer->indexer_heads;
    out->indexer_head_dimension = layer->indexer_head_dimension;
    out->indexer_topk = layer->indexer_topk;
    out->attention_kv_activation = layer->attention_kv_activation;
    out->compressor_activation = layer->compressor_activation;
    out->compressor_rotated_activation = layer->compressor_rotated_activation;
    out->indexer_query_activation = layer->indexer_query_activation;
    out->sparse_topk = layer->sparse_topk;
}


static int graph_plan_build(
    yvex_attention_plan **out,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_deepseek_v4_model_spec *model;
    const yvex_materialization_summary *materialization;
    const yvex_runtime_descriptor_summary *runtime;
    yvex_attention_plan *plan;
    unsigned long long layer_count;
    unsigned long long i;
    char logical_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    int rc;

    if (out) *out = NULL;
    if (!out || !ir || !session || !descriptor)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention plan requires IR, materialization session, and descriptor");
    model = yvex_model_register_deepseek_v4()->ir.model(ir);
    runtime = yvex_runtime_descriptor_summary_get(descriptor);
    materialization = yvex_materialization_session_summary(session);
    if (!model || yvex_model_register_deepseek_v4()->ir.layer_count(ir) == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek architecture IR has no attention layers");
    if (!materialization || !materialization->committed ||
        materialization->status != YVEX_MATERIALIZATION_STATUS_COMMITTED)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MATERIALIZATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires committed materialization");
    if (!runtime || runtime->status != YVEX_RUNTIME_DESCRIPTOR_STATUS_READY)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires a ready runtime descriptor");
    if (!runtime->runtime_numeric_identity[0] ||
        runtime->runtime_numeric_schema_version == 0u)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires runtime numeric descriptor facts");
    if (!yvex_model_register_deepseek_v4()->transform.architecture_identity(ir, logical_identity) ||
        strcmp(logical_identity, runtime->logical_model_identity) != 0 ||
        strcmp(materialization->plan_identity,
               runtime->materialization_plan_identity) != 0)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention refused a stale runtime descriptor identity");
    layer_count = yvex_model_register_deepseek_v4()->ir.layer_count(ir);
    plan = (yvex_attention_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            sizeof(*plan), 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate DeepSeek attention plan");
    plan->layers = (yvex_attention_layer_plan *)calloc(
        (size_t)layer_count, sizeof(*plan->layers));
    if (!plan->layers) {
        graph_plan_close(plan);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            layer_count, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate DeepSeek attention layer plans");
    }
    plan->layer_count = layer_count;
    plan->summary.status =
        YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_UNSUPPORTED;
    (void)snprintf(plan->summary.artifact_identity,
                   sizeof(plan->summary.artifact_identity), "%s",
                   runtime->artifact_identity);
    (void)snprintf(plan->summary.materialization_plan_identity,
                   sizeof(plan->summary.materialization_plan_identity), "%s",
                   runtime->materialization_plan_identity);
    (void)snprintf(plan->summary.logical_model_identity,
                   sizeof(plan->summary.logical_model_identity), "%s",
                   runtime->logical_model_identity);
    (void)snprintf(plan->summary.runtime_descriptor_identity,
                   sizeof(plan->summary.runtime_descriptor_identity), "%s",
                   runtime->runtime_descriptor_identity);
    (void)snprintf(plan->summary.runtime_numeric_identity,
                   sizeof(plan->summary.runtime_numeric_identity), "%s",
                   runtime->runtime_numeric_identity);
    plan->summary.layer_count = layer_count;
    plan->summary.auxiliary_layer_count = model->auxiliary_layer_count;
    plan->summary.swa_layer_count = model->swa_layer_count;
    plan->summary.csa_layer_count = model->csa_layer_count;
    plan->summary.hca_layer_count = model->hca_layer_count;
    plan->summary.history_contract_ready = 1;
    plan->summary.state_delta_contract_ready = 1;
    plan->summary.cpu_reference_ready = 1;
    plan->summary.cuda_execution_ready = 0;
    plan->summary.full_execution_ready = 0;

    for (i = 0ull; i < layer_count; ++i) {
        const yvex_deepseek_v4_layer_spec *layer =
            yvex_model_register_deepseek_v4()->ir.layer_at(ir, i);
        if (!layer) {
            graph_plan_close(plan);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
                i, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
                YVEX_ERR_FORMAT, "DeepSeek attention layer is missing");
        }
        attention_fill_layer_plan(&plan->layers[i], layer);
        rc = attention_bind_required_layer_roles(
            descriptor, layer, &plan->layers[i], failure, err);
        if (rc != YVEX_OK) {
            graph_plan_close(plan);
            return rc;
        }
        plan->summary.required_binding_count +=
            plan->layers[i].required_binding_count;
        plan->summary.qtype_compute_refusal_count +=
            plan->layers[i].qtype_compute_refusal_count;
        plan->summary.payload_bytes_bound += plan->layers[i].payload_bytes_bound;
    }
    attention_compute_identity(plan);
    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

static void graph_plan_close(yvex_attention_plan *plan)
{
    if (!plan) return;
    free(plan->layers);
    free(plan);
}

static const yvex_attention_summary *graph_plan_summary(
    const yvex_attention_plan *plan)
{
    return plan ? &plan->summary : NULL;
}

static unsigned long long graph_plan_layer_count(
    const yvex_attention_plan *plan)
{
    return plan ? plan->layer_count : 0ull;
}

static const yvex_attention_layer_plan *graph_plan_layer_at(
    const yvex_attention_plan *plan,
    unsigned long long index)
{
    if (!plan || index >= plan->layer_count) return NULL;
    return &plan->layers[index];
}

/* Compressor recurrence is family policy over generic state transactions. */

static int rolling_geometry(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long *ratio,
    unsigned long long *head_dim,
    unsigned long long *state_width,
    unsigned long long *state_slots,
    int *overlap,
    int *rotated)
{
    unsigned long long coeff;

    if (!layer || !ratio || !head_dim || !state_width || !state_slots ||
        !overlap || !rotated || layer->compression_ratio == 0ull)
        return 0;
    if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN) {
        *ratio = layer->compression_ratio;
        *head_dim = layer->head_dimension;
        *overlap =
            layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA ? 1 : 0;
        *rotated = 0;
    } else if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER) {
        if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_CSA)
            return 0;
        *ratio = layer->compression_ratio;
        *head_dim = layer->indexer_head_dimension;
        *overlap = 1;
        *rotated = 1;
    } else {
        return 0;
    }
    coeff = *overlap ? 2ull : 1ull;
    if (!yvex_attention_checked_mul_u64(*head_dim, coeff, state_width) ||
        !yvex_attention_checked_mul_u64(*ratio, coeff, state_slots))
        return 0;
    return *ratio != 0ull && *head_dim != 0ull;
}

/* Contract: checks active rolling slots without requiring unused score slots. */
static int attention_rolling_active_values_are_finite(
    const float *kv_state,
    const float *score_state,
    unsigned long long kv_stride,
    unsigned long long score_stride,
    unsigned long long head_dim,
    unsigned long long ratio,
    unsigned long long previous_fill,
    unsigned long long current_fill,
    int overlap)
{
    unsigned long long slot;
    unsigned long long lane;

    if (!kv_state || !score_state) return 0;
    for (slot = 0ull; overlap && slot < previous_fill; ++slot) {
        for (lane = 0ull; lane < head_dim; ++lane) {
            if (!isfinite(kv_state[slot * kv_stride + lane]) ||
                !isfinite(score_state[slot * score_stride + lane]))
                return 0;
        }
    }
    for (slot = 0ull; slot < current_fill; ++slot) {
        unsigned long long base = overlap ? ratio + slot : slot;
        unsigned long long lane_offset = overlap ? head_dim : 0ull;
        for (lane = 0ull; lane < head_dim; ++lane) {
            if (!isfinite(kv_state[base * kv_stride + lane + lane_offset]) ||
                !isfinite(score_state[base * score_stride + lane + lane_offset]))
                return 0;
        }
    }
    return 1;
}

/* Contract: validates one immutable rolling compressor state view. */
static int graph_rolling_state_validate(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_rolling_state_view *state,
    yvex_attention_rolling_kind kind,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long required_extent;
    int overlap;
    int rotated;

    if (!layer || !state || !state->present)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention rolling state is missing");
    if (!rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_UNSUPPORTED,
            "DeepSeek attention rolling state is not used by this class");
    if (state->schema_version !=
            YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1 ||
        state->kind != kind || state->attention_class != layer->attention_class ||
        state->layer_index != layer->layer_index || state->ratio != ratio ||
        state->head_dimension != head_dim || state->state_width != state_width ||
        state->state_slots != state_slots || state->overlap != overlap ||
        state->rotated != rotated)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            state->state_width, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state identity or geometry mismatch");
    if (state->cursor >= ratio || state->previous_fill > ratio ||
        state->current_fill > ratio || (!overlap && state->previous_fill))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ratio,
            state->cursor, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling state cursor or fill is invalid");
    if (state->kv_state_stride < state_width ||
        state->score_state_stride < state_width || !state->kv_state ||
        !state->score_state)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            state->kv_state_stride, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state storage is incomplete");
    if (!yvex_attention_checked_mul_u64(state_slots, state->kv_state_stride,
                                   &required_extent) ||
        state->kv_state_extent < required_extent ||
        !yvex_attention_checked_mul_u64(state_slots, state->score_state_stride,
                                   &required_extent) ||
        state->score_state_extent < required_extent)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_slots,
            state->kv_state_extent, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling state extent is too small");
    if (!attention_rolling_active_values_are_finite(
            state->kv_state, state->score_state, state->kv_state_stride,
            state->score_state_stride, head_dim, ratio, state->previous_fill,
            state->current_fill, overlap))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state contains non-finite active values");
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: copies one rolling-state buffer preserving caller-owned strides. */
static void attention_rolling_copy_state(
    const yvex_attention_rolling_state_view *before,
    yvex_attention_rolling_state_output *after)
{
    unsigned long long slot;

    for (slot = 0ull; slot < before->state_slots; ++slot) {
        memcpy(after->kv_state + slot * after->kv_state_stride,
               before->kv_state + slot * before->kv_state_stride,
               (size_t)(before->state_width * sizeof(float)));
        memcpy(after->score_state + slot * after->score_state_stride,
               before->score_state + slot * before->score_state_stride,
               (size_t)(before->state_width * sizeof(float)));
    }
}

/* Contract: computes a stable softmax-weighted compressed vector. */
static int attention_rolling_emit(
    const yvex_attention_rolling_state_output *state,
    unsigned long long head_dim,
    unsigned long long ratio,
    int overlap,
    float *compressed_out,
    unsigned long long compressed_out_count)
{
    unsigned long long lane;
    unsigned long long slot;

    if (!state || !compressed_out || compressed_out_count < head_dim)
        return 0;
    for (lane = 0ull; lane < head_dim; ++lane) {
        double max_score = -HUGE_VAL;
        double denom = 0.0;
        double value = 0.0;
        for (slot = 0ull; slot < ratio; ++slot) {
            double score = state->score_state[slot * state->score_state_stride +
                                              lane];
            if (overlap) {
                double score2 =
                    state->score_state[(ratio + slot) *
                                           state->score_state_stride +
                                       lane + head_dim];
                if (score2 > max_score) max_score = score2;
            }
            if (score > max_score) max_score = score;
        }
        for (slot = 0ull; slot < ratio; ++slot) {
            double score = state->score_state[slot * state->score_state_stride +
                                              lane];
            double weight = exp(score - max_score);
            denom += weight;
            value += weight *
                     (double)state->kv_state[slot * state->kv_state_stride +
                                             lane];
            if (overlap) {
                double score2 =
                    state->score_state[(ratio + slot) *
                                           state->score_state_stride +
                                       lane + head_dim];
                double weight2 = exp(score2 - max_score);
                denom += weight2;
                value +=
                    weight2 *
                    (double)state->kv_state[(ratio + slot) *
                                                state->kv_state_stride +
                                            lane + head_dim];
            }
        }
        if (!isfinite(denom) || denom <= 0.0 || !isfinite(value)) return 0;
        compressed_out[lane] = (float)(value / denom);
    }
    return 1;
}

/* Contract: executes one production CPU compressor-state transition. */
static int graph_rolling_state_step_cpu(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_rolling_state_view *before,
    const float *token_kv,
    const float *token_score,
    const float *ape_row,
    yvex_attention_rolling_state_output *after,
    float *compressed_out,
    unsigned long long compressed_out_count,
    int *emitted,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long required_extent;
    unsigned long long slot;
    unsigned long long lane;
    float *after_kv_state;
    float *after_score_state;
    unsigned long long after_kv_stride;
    unsigned long long after_score_stride;
    unsigned long long after_kv_extent;
    unsigned long long after_score_extent;
    int overlap;
    int rotated;
    int should_emit;
    int rc;

    if (emitted) *emitted = 0;
    rc = graph_rolling_state_validate(
        layer, before, before ? before->kind : YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE,
        failure, err);
    if (rc != YVEX_OK) return rc;
    if (!token_kv || !token_score || !ape_row || !after)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention rolling transition requires token vectors and output state");
    if (before->next_token_position == ULLONG_MAX)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            before->next_token_position, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling token position would overflow");
    if (!rolling_geometry(layer, before->kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_UNSUPPORTED,
            "DeepSeek attention rolling transition lacks geometry");
    if (!after->kv_state || !after->score_state ||
        after->kv_state_stride < state_width ||
        after->score_state_stride < state_width ||
        !yvex_attention_checked_mul_u64(state_slots, after->kv_state_stride,
                                   &required_extent) ||
        after->kv_state_extent < required_extent ||
        !yvex_attention_checked_mul_u64(state_slots, after->score_state_stride,
                                   &required_extent) ||
        after->score_state_extent < required_extent)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            after ? after->kv_state_stride : 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling output storage is incomplete");
    if (before->cursor != (before->next_token_position % ratio))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            before->next_token_position % ratio, before->cursor, err,
            YVEX_ERR_STATE,
            "DeepSeek attention rolling cursor is stale for token position");
    after_kv_state = after->kv_state;
    after_score_state = after->score_state;
    after_kv_stride = after->kv_state_stride;
    after_score_stride = after->score_state_stride;
    after_kv_extent = after->kv_state_extent;
    after_score_extent = after->score_state_extent;
    for (lane = 0ull; lane < state_width; ++lane) {
        if (!isfinite(token_kv[lane]) || !isfinite(token_score[lane]) ||
            !isfinite(ape_row[lane]))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, lane, err,
                YVEX_ERR_FORMAT,
                "DeepSeek attention rolling transition input is non-finite");
    }
    *after = (yvex_attention_rolling_state_output){
        .present = 1,
        .schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1,
        .kind = before->kind,
        .attention_class = before->attention_class,
        .layer_index = before->layer_index,
        .next_token_position = before->next_token_position + 1ull,
        .ratio = ratio,
        .head_dimension = head_dim,
        .state_width = state_width,
        .state_slots = state_slots,
        .previous_fill = before->previous_fill,
        .current_fill = before->current_fill,
        .cursor = (before->cursor + 1ull) % ratio,
        .kv_state_stride = after_kv_stride,
        .score_state_stride = after_score_stride,
        .kv_state_extent = after_kv_extent,
        .score_state_extent = after_score_extent,
        .kv_state = after_kv_state,
        .score_state = after_score_state,
        .overlap = overlap,
        .rotated = rotated,
    };
    memcpy(after->attention_plan_identity, before->attention_plan_identity,
           sizeof(after->attention_plan_identity));
    attention_rolling_copy_state(before, after);
    slot = overlap ? ratio + before->cursor : before->cursor;
    for (lane = 0ull; lane < state_width; ++lane) {
        after->kv_state[slot * after->kv_state_stride + lane] = token_kv[lane];
        after->score_state[slot * after->score_state_stride + lane] =
            token_score[lane] + ape_row[lane];
    }
    if (after->current_fill < before->cursor + 1ull)
        after->current_fill = before->cursor + 1ull;
    should_emit = ((before->next_token_position + 1ull) % ratio) == 0ull;
    if (should_emit) {
        if (!attention_rolling_emit(after, head_dim, ratio, overlap,
                                    compressed_out, compressed_out_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, head_dim,
                compressed_out_count, err, YVEX_ERR_FORMAT,
                "DeepSeek attention rolling compression emitted invalid values");
        if (overlap) {
            for (slot = 0ull; slot < ratio; ++slot) {
                memcpy(after->kv_state + slot * after->kv_state_stride,
                       after->kv_state + (ratio + slot) * after->kv_state_stride,
                       (size_t)(state_width * sizeof(float)));
                memcpy(after->score_state + slot * after->score_state_stride,
                       after->score_state +
                           (ratio + slot) * after->score_state_stride,
                       (size_t)(state_width * sizeof(float)));
            }
            after->previous_fill = ratio;
        } else {
            after->previous_fill = 0ull;
        }
        after->current_fill = 0ull;
        after->cursor = 0ull;
        if (emitted) *emitted = 1;
    }
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

static int graph_history_validate(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!layer || !history)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention history validation requires layer and history");
    if (!history->immutable)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention history view must be immutable");
    if (history->local_tail_count > layer->sliding_window)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->sliding_window, history->local_tail_count, err,
            YVEX_ERR_BOUNDS,
            "DeepSeek attention history exceeds sliding-window boundary");
    if (history->local_tail_count &&
        (!history->local_kv || !history->local_positions ||
         history->local_kv_stride < layer->head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->head_dimension, history->local_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention local history lacks raw KV storage");
    if (history->compressed_entry_count &&
        (!history->compressed_kv || !history->compressed_positions ||
         history->compressed_kv_stride < layer->head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->head_dimension, history->compressed_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention compressed history lacks KV storage");
    if (history->indexer_entry_count &&
        (!history->indexer_kv || !history->indexer_positions ||
         history->indexer_kv_stride < layer->indexer_head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->indexer_head_dimension, history->indexer_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention indexer history lacks KV storage");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
        history->compressed_entry_count != history->indexer_entry_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            history->compressed_entry_count, history->indexer_entry_count,
            err, YVEX_ERR_FORMAT,
            "CSA compressed and indexer history cardinalities differ");
    {
        unsigned long long i;
        for (i = 0ull; i < history->local_tail_count; ++i) {
            if (history->local_positions[i] >= history->token_count ||
                (i && history->local_positions[i - 1ull] >=
                          history->local_positions[i]))
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count, history->local_positions[i], err,
                    YVEX_ERR_FORMAT,
                    "local history positions are stale or not strictly ordered");
        }
        for (i = 0ull; i < history->compressed_entry_count; ++i) {
            if (history->compressed_positions[i] >= history->token_count ||
                (i && history->compressed_positions[i - 1ull] >=
                          history->compressed_positions[i]))
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count,
                    history->compressed_positions[i], err, YVEX_ERR_FORMAT,
                    "compressed history positions are stale or not strictly ordered");
        }
        for (i = 0ull; i < history->indexer_entry_count; ++i) {
            if (history->indexer_positions[i] >= history->token_count ||
                (i && history->indexer_positions[i - 1ull] >=
                          history->indexer_positions[i]) ||
                (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
                 history->indexer_positions[i] !=
                     history->compressed_positions[i]))
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count, history->indexer_positions[i], err,
                    YVEX_ERR_FORMAT,
                    "indexer history positions do not bind compressed history");
        }
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_SWA &&
        (history->compressed_entry_count || history->indexer_entry_count))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            history->compressed_entry_count + history->indexer_entry_count,
            err, YVEX_ERR_FORMAT,
            "SWA history may not carry compressed or indexer entries");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_HCA &&
        history->indexer_entry_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            history->indexer_entry_count, err, YVEX_ERR_FORMAT,
            "HCA history may not carry CSA indexer entries");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        if (history->main_rolling_state.present ||
            history->indexer_rolling_state.present)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
                err, YVEX_ERR_FORMAT,
                "SWA history may not carry compressor rolling state");
    } else {
        int rc = graph_rolling_state_validate(
            layer, &history->main_rolling_state,
            YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN, failure, err);
        if (rc != YVEX_OK) return rc;
        if (history->main_rolling_state.next_token_position !=
            history->token_count)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                history->token_count,
                history->main_rolling_state.next_token_position, err,
                YVEX_ERR_STATE,
                "main rolling state token position is stale");
        if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
            rc = graph_rolling_state_validate(
                layer, &history->indexer_rolling_state,
                YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, failure, err);
            if (rc != YVEX_OK) return rc;
            if (history->indexer_rolling_state.next_token_position !=
                history->token_count)
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count,
                    history->indexer_rolling_state.next_token_position, err,
                    YVEX_ERR_STATE,
                    "indexer rolling state token position is stale");
        } else if (history->indexer_rolling_state.present) {
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
                err, YVEX_ERR_FORMAT,
                "HCA history may not carry indexer rolling state");
        }
    }
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Execution composes admitted generic operations without owning their math. */

#define YVEX_ATTENTION_PI 3.14159265358979323846264338327950288

static int execute_update_reference_error(
    yvex_attention_cpu_result *result,
    double production,
    double reference);

static int execute_dot_row_range(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long start_row,
    const float *vector,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err);

static int execute_init_empty_rolling_view(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long next_token_position,
    float *kv_state,
    float *score_state,
    yvex_attention_rolling_state_view *out);

/* Contract: releases every owned execution-stage copy and tolerates repeats. */
static void graph_execution_trace_release(
    yvex_attention_execution_trace *trace)
{
    if (!trace) return;
    free(trace->input);
    free(trace->q_low);
    free(trace->query);
    free(trace->raw_kv);
    free(trace->compressed_kv);
    free(trace->indexer_kv);
    free(trace->index_query);
    free(trace->index_weights);
    free(trace->attention_values);
    free(trace->output);
    free(trace->compressed_positions);
    free(trace->indexer_positions);
    free(trace->topk_counts);
    free(trace->topk_positions);
    free(trace->next_main_rolling_state.kv_state);
    free(trace->next_main_rolling_state.score_state);
    free(trace->next_indexer_rolling_state.kv_state);
    free(trace->next_indexer_rolling_state.score_state);
    memset(trace, 0, sizeof(*trace));
}

/* Contract: copies one finite execution-stage range into trace-owned memory. */
static int attention_trace_copy_float(float **out,
                                      const float *values,
                                      unsigned long long count)
{
    float *copy;

    if (!out || (count && !values)) return 0;
    *out = NULL;
    if (!count) return 1;
    copy = (float *)yvex_attention_calloc_array(count, sizeof(*copy));
    if (!copy) return 0;
    memcpy(copy, values, (size_t)count * sizeof(*copy));
    *out = copy;
    return 1;
}

/* Contract: copies one execution trace only after all numeric stages complete. */
static int attention_trace_capture(
    yvex_attention_execution_trace *trace,
    unsigned long long layer_index,
    yvex_deepseek_v4_attention_class attention_class,
    unsigned long long token_position,
    unsigned long long token_count,
    unsigned long long hidden_width,
    unsigned long long q_rank,
    unsigned long long query_width,
    unsigned long long kv_width,
    const float *input,
    const float *q_low,
    const float *query,
    const float *raw_kv,
    const float *compressed_kv,
    unsigned long long compressed_count,
    unsigned long long compressed_stride,
    const unsigned long long *compressed_positions,
    const float *indexer_kv,
    unsigned long long indexer_count,
    unsigned long long indexer_stride,
    const unsigned long long *indexer_positions,
    const float *index_query,
    unsigned long long index_query_stride,
    const float *index_weights,
    unsigned long long index_weight_stride,
    const float *attention_values,
    const float *output,
    const unsigned long long *topk_counts,
    const unsigned long long *topk_positions,
    unsigned long long topk_stride,
    const yvex_attention_rolling_state_output *main_state,
    const float *main_state_kv,
    const float *main_state_score,
    const yvex_attention_rolling_state_output *index_state,
    const float *index_state_kv,
    const float *index_state_score)
{
    unsigned long long count;

    if (!trace || trace->owned || !token_count || !hidden_width || !q_rank ||
        !query_width || !kv_width ||
        (compressed_count && !compressed_positions) ||
        (indexer_count && !indexer_positions) ||
        (topk_stride && (!topk_counts || !topk_positions)))
        return 0;
    memset(trace, 0, sizeof(*trace));
    trace->owned = 1;
    trace->layer_index = layer_index;
    trace->attention_class = attention_class;
    trace->token_position = token_position;
    trace->token_count = token_count;
    trace->hidden_width = hidden_width;
    trace->q_rank = q_rank;
    trace->query_width = query_width;
    trace->kv_width = kv_width;
    trace->compressed_count = compressed_count;
    trace->compressed_stride = compressed_stride;
    trace->indexer_count = indexer_count;
    trace->indexer_stride = indexer_stride;
    trace->index_query_stride = index_query_stride;
    trace->index_weight_stride = index_weight_stride;
    trace->topk_stride = topk_stride;
    if (!yvex_attention_checked_mul_u64(token_count, hidden_width, &count) ||
        !attention_trace_copy_float(&trace->input, input, count) ||
        !yvex_attention_checked_mul_u64(token_count, q_rank, &count) ||
        !attention_trace_copy_float(&trace->q_low, q_low, count) ||
        !yvex_attention_checked_mul_u64(token_count, query_width, &count) ||
        !attention_trace_copy_float(&trace->query, query, count) ||
        !yvex_attention_checked_mul_u64(token_count, kv_width, &count) ||
        !attention_trace_copy_float(&trace->raw_kv, raw_kv, count) ||
        !yvex_attention_checked_mul_u64(compressed_count, compressed_stride,
                                   &count) ||
        !attention_trace_copy_float(&trace->compressed_kv, compressed_kv,
                                    count) ||
        !yvex_attention_checked_mul_u64(indexer_count, indexer_stride, &count) ||
        !attention_trace_copy_float(&trace->indexer_kv, indexer_kv, count) ||
        !yvex_attention_checked_mul_u64(token_count, index_query_stride, &count) ||
        !attention_trace_copy_float(&trace->index_query, index_query, count) ||
        !yvex_attention_checked_mul_u64(token_count, index_weight_stride, &count) ||
        !attention_trace_copy_float(&trace->index_weights, index_weights,
                                    count) ||
        !yvex_attention_checked_mul_u64(token_count, query_width, &count) ||
        !attention_trace_copy_float(&trace->attention_values,
                                    attention_values, count) ||
        !yvex_attention_checked_mul_u64(token_count, hidden_width, &count) ||
        !attention_trace_copy_float(&trace->output, output, count)) {
        graph_execution_trace_release(trace);
        return 0;
    }
    if (compressed_count) {
        trace->compressed_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                compressed_count, sizeof(*trace->compressed_positions));
        if (!trace->compressed_positions) goto fail;
        memcpy(trace->compressed_positions, compressed_positions,
               (size_t)compressed_count *
                   sizeof(*trace->compressed_positions));
    }
    if (indexer_count) {
        trace->indexer_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                indexer_count, sizeof(*trace->indexer_positions));
        if (!trace->indexer_positions) goto fail;
        memcpy(trace->indexer_positions, indexer_positions,
               (size_t)indexer_count * sizeof(*trace->indexer_positions));
    }
    if (topk_stride) {
        trace->topk_counts =
            (unsigned long long *)yvex_attention_calloc_array(
                token_count, sizeof(*trace->topk_counts));
        if (!yvex_attention_checked_mul_u64(token_count, topk_stride, &count))
            goto fail;
        trace->topk_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                count, sizeof(*trace->topk_positions));
        if (!trace->topk_counts || !trace->topk_positions) goto fail;
        memcpy(trace->topk_counts, topk_counts,
               (size_t)token_count * sizeof(*trace->topk_counts));
        memcpy(trace->topk_positions, topk_positions,
               (size_t)count * sizeof(*trace->topk_positions));
    }
    if (main_state && main_state->present) {
        trace->next_main_rolling_state = *main_state;
        trace->next_main_rolling_state.kv_state = NULL;
        trace->next_main_rolling_state.score_state = NULL;
        if (!attention_trace_copy_float(
                &trace->next_main_rolling_state.kv_state, main_state_kv,
                main_state->kv_state_extent) ||
            !attention_trace_copy_float(
                &trace->next_main_rolling_state.score_state,
                main_state_score, main_state->score_state_extent))
            goto fail;
    }
    if (index_state && index_state->present) {
        trace->next_indexer_rolling_state = *index_state;
        trace->next_indexer_rolling_state.kv_state = NULL;
        trace->next_indexer_rolling_state.score_state = NULL;
        if (!attention_trace_copy_float(
                &trace->next_indexer_rolling_state.kv_state, index_state_kv,
                index_state->kv_state_extent) ||
            !attention_trace_copy_float(
                &trace->next_indexer_rolling_state.score_state,
                index_state_score, index_state->score_state_extent))
            goto fail;
    }
    trace->complete = 1;
    return 1;

fail:
    graph_execution_trace_release(trace);
    return 0;
}

static void attention_fill_activation(float *out, unsigned long long count,
                                      unsigned long long layer_index,
                                      unsigned long long token_position)
{
    unsigned long long i;

    for (i = 0ull; i < count; ++i) {
        unsigned long long mixed =
            (i * 1103515245ull) ^ (layer_index * 2654435761ull) ^
            (token_position * 97ull);
        int lane = (int)(mixed % 257ull) - 128;
        out[i] = (float)lane / 128.0f;
    }
}

static void attention_result_zero(yvex_attention_cpu_result *result)
{
    if (result) memset(result, 0, sizeof(*result));
}

static const yvex_runtime_tensor_binding *attention_find_binding(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    unsigned long long layer_index)
{
    return yvex_runtime_descriptor_find_role(
        descriptor, role, YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
        layer_index, YVEX_DEEPSEEK_GGUF_NO_INDEX);
}

static int attention_row_bytes(
    const yvex_materialized_tensor_binding *binding,
    unsigned long long *row_bytes_out,
    unsigned long long *row_count_out,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long blocks;
    unsigned long long row_bytes;

    if (!binding || !row_bytes_out || !row_count_out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "attention row-byte calculation requires binding and outputs");
    if (!binding->row_width || !binding->row_count ||
        !binding->block_size || !binding->bytes_per_block ||
        binding->row_width % binding->block_size != 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            binding->layer_index, binding->role, binding->block_size,
            binding->row_width, err, YVEX_ERR_FORMAT,
            "attention binding row geometry is invalid");
    blocks = binding->row_width / binding->block_size;
    if (!yvex_attention_checked_mul_u64(blocks, binding->bytes_per_block,
                                   &row_bytes) ||
        row_bytes == 0ull ||
        row_bytes > (unsigned long long)SIZE_MAX)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            binding->layer_index, binding->role, ULLONG_MAX, row_bytes,
            err, YVEX_ERR_BOUNDS,
            "attention binding row byte size overflowed");
    if (binding->encoded_bytes / row_bytes < binding->row_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            binding->layer_index, binding->role,
            binding->row_count * row_bytes, binding->encoded_bytes,
            err, YVEX_ERR_BOUNDS,
            "attention binding encoded bytes do not cover declared rows");
    *row_bytes_out = row_bytes;
    *row_count_out = binding->row_count;
    return YVEX_OK;
}

/* Contract: decodes one complete encoded tensor row into owned F32 scratch. */
static int attention_decode_row(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long row_index,
    float *out,
    unsigned long long out_elements,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long row_count;
    unsigned long long block_count;
    unsigned long long block;
    unsigned char *encoded = NULL;
    int rc;

    if (!session || !runtime_binding || !runtime_binding->binding || !out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention row decode requires session, binding, and output");
    binding = runtime_binding->binding;
    rc = attention_row_bytes(binding, &row_bytes, &row_count, failure, err);
    if (rc != YVEX_OK) return rc;
    if (row_index >= row_count || out_elements != binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->row_width, out_elements, err, YVEX_ERR_BOUNDS,
            "attention row decode output shape does not match tensor row");
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (!encoded)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            row_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention encoded row scratch");
    rc = yvex_materialization_session_read(
        session, binding, row_index * row_bytes, encoded, (size_t)row_bytes,
        NULL, err);
    if (rc != YVEX_OK) {
        free(encoded);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ, runtime_binding,
            binding->layer_index, binding->role, row_bytes, 0ull, err, rc,
            "failed to read attention encoded row");
    }
    if (result) result->payload_bytes_read += row_bytes;
    block_count = binding->row_width / binding->block_size;
    for (block = 0ull; block < block_count; ++block) {
        yvex_quant_failure qfailure;
        rc = yvex_quant_decode_block(
            binding->qtype,
            encoded + (size_t)(block * binding->bytes_per_block),
            (size_t)binding->bytes_per_block,
            out + (block * binding->block_size),
            binding->block_size, &qfailure, err);
        if (rc != YVEX_OK) {
            free(encoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                runtime_binding, binding->layer_index, binding->role,
                binding->block_size, qfailure.actual, err, rc,
                "failed to decode attention qtype row");
        }
    }
    free(encoded);
    return YVEX_OK;
}

/* Contract: computes one bounded encoded matrix-vector row range. */
static int execute_dot_row_range(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long start_row,
    const float *vector,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long row_count;
    unsigned long long rows;
    unsigned long long row;
    unsigned char *encoded = NULL;
    float *decoded = NULL;
    int rc;

    if (!session || !runtime_binding || !runtime_binding->binding ||
        !vector || !out || !rows_out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention dot rows require session, binding, vector, and output");
    binding = runtime_binding->binding;
    rc = attention_row_bytes(binding, &row_bytes, &row_count, failure, err);
    if (rc != YVEX_OK) return rc;
    if (vector_len != binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->row_width, vector_len, err, YVEX_ERR_BOUNDS,
            "attention dot vector length does not match tensor row width");
    if (start_row >= row_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            row_count, start_row, err, YVEX_ERR_BOUNDS,
            "attention dot row range starts beyond tensor rows");
    rows = yvex_attention_min_u64(max_rows ? max_rows : row_count - start_row,
                             row_count - start_row);
    if (rows == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            1ull, 0ull, err, YVEX_ERR_BOUNDS,
            "attention dot requires at least one output row");
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (collect_reference_metrics)
        decoded = (float *)yvex_attention_calloc_array(vector_len, sizeof(float));
    if (!encoded || (collect_reference_metrics && !decoded)) {
        free(encoded);
        free(decoded);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            row_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention dot row scratch");
    }
    for (row = 0ull; row < rows; ++row) {
        yvex_quant_failure qfailure;
        unsigned long long block_count;
        unsigned long long block;
        unsigned long long lane;
        double reference = 0.0;
        rc = yvex_materialization_session_read(
            session, binding, (start_row + row) * row_bytes, encoded,
            (size_t)row_bytes, NULL, err);
        if (rc != YVEX_OK) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ,
                runtime_binding, binding->layer_index, binding->role,
                row_bytes, 0ull, err, rc,
                "failed to read attention dot encoded row");
        }
        if (result) result->payload_bytes_read += row_bytes;
        rc = yvex_quant_cpu_dot(binding->qtype, encoded, (size_t)row_bytes,
                                vector, vector_len, &out[row], &qfailure, err);
        if (rc != YVEX_OK) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                runtime_binding, binding->layer_index, binding->role,
                vector_len, qfailure.actual, err, rc,
                "attention encoded row dot failed");
        }
        if (!isfinite(out[row])) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                runtime_binding, binding->layer_index, binding->role,
                1ull, row, err, YVEX_ERR_FORMAT,
                "attention encoded row dot produced non-finite output");
        }
        if (collect_reference_metrics) {
            block_count = binding->row_width / binding->block_size;
            for (block = 0ull; block < block_count; ++block) {
                rc = yvex_quant_decode_block(
                    binding->qtype,
                    encoded + (size_t)(block * binding->bytes_per_block),
                    (size_t)binding->bytes_per_block,
                    decoded + (block * binding->block_size),
                    binding->block_size, &qfailure, err);
                if (rc != YVEX_OK) {
                    free(encoded);
                    free(decoded);
                    return yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        runtime_binding, binding->layer_index, binding->role,
                        binding->block_size, qfailure.actual, err, rc,
                        "attention reference decode failed");
                }
            }
            for (lane = 0ull; lane < vector_len; ++lane)
                reference += (double)decoded[lane] * (double)vector[lane];
            if (!execute_update_reference_error(result, (double)out[row],
                                                  reference)) {
                free(encoded);
                free(decoded);
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    runtime_binding, binding->layer_index, binding->role,
                    1ull, row, err, YVEX_ERR_FORMAT,
                    "attention reference comparison produced non-finite metrics");
            }
        }
    }
    free(encoded);
    free(decoded);
    *rows_out = rows;
    return YVEX_OK;
}

/* Contract: computes bounded matrix-vector rows from encoded GGUF bytes. */
static int attention_dot_rows(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    const float *vector,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    return execute_dot_row_range(
        session, runtime_binding, 0ull, vector, vector_len, max_rows, out,
        rows_out, collect_reference_metrics, result, failure, err);
}

/* Contract: computes encoded matrix rows once across a token batch. */
static int attention_dot_row_range_batch(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long start_row,
    const float *vectors,
    unsigned long long token_count,
    unsigned long long vector_stride,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long output_stride,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long row_count;
    unsigned long long rows;
    unsigned long long row;
    unsigned char *encoded = NULL;
    float *decoded = NULL;
    int rc;

    if (!session || !runtime_binding || !runtime_binding->binding ||
        !vectors || !out || !rows_out || token_count == 0ull ||
        vector_stride < vector_len || output_stride < max_rows)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention batch dot rows require session, binding, vectors, and output");
    binding = runtime_binding->binding;
    rc = attention_row_bytes(binding, &row_bytes, &row_count, failure, err);
    if (rc != YVEX_OK) return rc;
    if (vector_len != binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->row_width, vector_len, err, YVEX_ERR_BOUNDS,
            "attention batch dot vector length does not match tensor row width");
    if (start_row >= row_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            row_count, start_row, err, YVEX_ERR_BOUNDS,
            "attention batch dot row range starts beyond tensor rows");
    rows = yvex_attention_min_u64(max_rows ? max_rows : row_count - start_row,
                             row_count - start_row);
    if (rows == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            1ull, 0ull, err, YVEX_ERR_BOUNDS,
            "attention batch dot requires at least one output row");
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (collect_reference_metrics)
        decoded = (float *)yvex_attention_calloc_array(vector_len, sizeof(float));
    if (!encoded || (collect_reference_metrics && !decoded)) {
        free(encoded);
        free(decoded);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            row_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention batch dot row scratch");
    }
    for (row = 0ull; row < rows; ++row) {
        yvex_quant_failure qfailure;
        unsigned long long token;
        unsigned long long block_count = 0ull;
        unsigned long long block;
        rc = yvex_materialization_session_read(
            session, binding, (start_row + row) * row_bytes, encoded,
            (size_t)row_bytes, NULL, err);
        if (rc != YVEX_OK) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ,
                runtime_binding, binding->layer_index, binding->role,
                row_bytes, 0ull, err, rc,
                "failed to read attention batch dot encoded row");
        }
        if (result) result->payload_bytes_read += row_bytes;
        if (collect_reference_metrics) {
            block_count = binding->row_width / binding->block_size;
            for (block = 0ull; block < block_count; ++block) {
                rc = yvex_quant_decode_block(
                    binding->qtype,
                    encoded + (size_t)(block * binding->bytes_per_block),
                    (size_t)binding->bytes_per_block,
                    decoded + (block * binding->block_size),
                    binding->block_size, &qfailure, err);
                if (rc != YVEX_OK) {
                    free(encoded);
                    free(decoded);
                    return yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        runtime_binding, binding->layer_index, binding->role,
                        binding->block_size, qfailure.actual, err, rc,
                        "attention batch reference decode failed");
                }
            }
        }
        for (token = 0ull; token < token_count; ++token) {
            const float *vector = vectors + token * vector_stride;
            float *dst = out + token * output_stride + row;
            rc = yvex_quant_cpu_dot(binding->qtype, encoded,
                                    (size_t)row_bytes, vector, vector_len,
                                    dst, &qfailure, err);
            if (rc != YVEX_OK) {
                free(encoded);
                free(decoded);
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    runtime_binding, binding->layer_index, binding->role,
                    vector_len, qfailure.actual, err, rc,
                    "attention batch encoded row dot failed");
            }
            if (!isfinite(*dst)) {
                free(encoded);
                free(decoded);
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    runtime_binding, binding->layer_index, binding->role,
                    1ull, row, err, YVEX_ERR_FORMAT,
                    "attention batch encoded row dot produced non-finite output");
            }
            if (collect_reference_metrics) {
                unsigned long long lane;
                double reference = 0.0;
                for (lane = 0ull; lane < vector_len; ++lane)
                    reference +=
                        (double)decoded[lane] * (double)vector[lane];
                if (!execute_update_reference_error(
                        result, (double)*dst, reference)) {
                    free(encoded);
                    free(decoded);
                    return yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        runtime_binding, binding->layer_index, binding->role,
                        1ull, row, err, YVEX_ERR_FORMAT,
                        "attention batch reference comparison produced non-finite metrics");
                }
            }
        }
    }
    free(encoded);
    free(decoded);
    *rows_out = rows;
    return YVEX_OK;
}

static double attention_checksum(const float *values,
                                 unsigned long long count)
{
    unsigned long long i;
    double sum = 0.0;

    if (!values) return 0.0;
    for (i = 0ull; i < count; ++i) {
        double weight = (double)((i % 29ull) + 1ull) / 29.0;
        sum += (double)values[i] * weight;
    }
    return sum;
}

static int execute_update_reference_error(
    yvex_attention_cpu_result *result,
    double production,
    double reference)
{
    double diff;
    double abs_diff;
    double denom;

    if (!result || !isfinite(production) || !isfinite(reference))
        return 0;
    diff = production - reference;
    abs_diff = fabs(diff);
    denom = fabs(reference);
    if (denom < 1e-12) denom = 1.0;
    if (!isfinite(abs_diff) || !isfinite(abs_diff / denom))
        return 0;
    if (abs_diff > result->max_abs_error)
        result->max_abs_error = abs_diff;
    if ((abs_diff / denom) > result->max_relative_error)
        result->max_relative_error = abs_diff / denom;
    result->rmse += diff * diff;
    result->reference_comparisons++;
    return 1;
}

static int attention_apply_rms_norm(float *values,
                                    unsigned long long count,
                                    const float *weights,
                                    double epsilon)
{
    unsigned long long i;
    double mean = 0.0;
    double inv;

    if (!values || !weights || count == 0ull || !isfinite(epsilon) ||
        epsilon <= 0.0)
        return 0;
    for (i = 0ull; i < count; ++i) {
        double v = values[i];
        if (!isfinite(v) || !isfinite(weights[i])) return 0;
        mean += v * v;
    }
    mean /= (double)count;
    inv = 1.0 / sqrt(mean + epsilon);
    if (!isfinite(inv)) return 0;
    for (i = 0ull; i < count; ++i) {
        double v = (double)values[i] * inv * (double)weights[i];
        if (!isfinite(v)) return 0;
        values[i] = (float)v;
    }
    return 1;
}

/* Contract: applies the architecture's unweighted per-head query RMS norm. */
static int attention_apply_unit_rms_norm(float *values,
                                         unsigned long long count,
                                         double epsilon)
{
    unsigned long long i;
    double mean = 0.0;
    double inverse;

    if (!values || count == 0ull || !isfinite(epsilon) || epsilon <= 0.0)
        return 0;
    for (i = 0ull; i < count; ++i) {
        double value = values[i];
        if (!isfinite(value)) return 0;
        mean += value * value;
    }
    inverse = 1.0 / sqrt(mean / (double)count + epsilon);
    if (!isfinite(inverse)) return 0;
    for (i = 0ull; i < count; ++i) {
        double value = (double)values[i] * inverse;
        if (!isfinite(value)) return 0;
        values[i] = (float)value;
    }
    return 1;
}

static double attention_yarn_frequency(
    const yvex_deepseek_v4_position_spec *position,
    unsigned long long pair,
    unsigned long long rope_dims)
{
    double exponent;
    double frequency;

    if (!position || rope_dims < 2ull || position->theta <= 1ull) return 0.0;
    exponent = (double)(pair * 2ull) / (double)rope_dims;
    frequency = 1.0 / pow((double)position->theta, exponent);
    if (position->original_context && position->scaling_factor) {
        double denominator = 2.0 * log((double)position->theta);
        double low_dim = (double)rope_dims *
            log((double)position->original_context /
                ((double)position->beta_fast * 2.0 * YVEX_ATTENTION_PI)) /
            denominator;
        double high_dim = (double)rope_dims *
            log((double)position->original_context /
                ((double)position->beta_slow * 2.0 * YVEX_ATTENTION_PI)) /
            denominator;
        double low = floor(low_dim);
        double high = ceil(high_dim);
        double lane = (double)pair;
        double ramp;
        double smooth;

        if (low < 0.0) low = 0.0;
        if (high > (double)rope_dims - 1.0)
            high = (double)rope_dims - 1.0;
        if (low == high) high += 0.001;
        ramp = (lane - low) / (high - low);
        if (ramp < 0.0) ramp = 0.0;
        if (ramp > 1.0) ramp = 1.0;
        smooth = 1.0 - ramp;
        frequency = frequency / (double)position->scaling_factor *
                        (1.0 - smooth) +
                    frequency * smooth;
    }
    return frequency;
}

/* Contract: applies the exact admitted RoPE/YaRN policy to one head tail. */
static int attention_apply_rope_tail(
    float *values,
    unsigned long long count,
    unsigned long long rope_dims,
    unsigned long long token_position,
    const yvex_deepseek_v4_position_spec *position,
    int inverse)
{
    unsigned long long start;
    unsigned long long i;

    if (!values || count == 0ull || rope_dims < 2ull || rope_dims > count ||
        !position || position->theta <= 1ull)
        return 0;
    if (rope_dims & 1ull) rope_dims--;
    start = count - rope_dims;
    for (i = 0ull; i < rope_dims; i += 2ull) {
        double frequency = attention_yarn_frequency(position, i / 2ull,
                                                    rope_dims);
        double angle = (double)token_position * frequency;
        double c = cos(angle);
        double s = inverse ? -sin(angle) : sin(angle);
        double x = values[start + i];
        double y = values[start + i + 1ull];
        if (!isfinite(x) || !isfinite(y) || !isfinite(c) || !isfinite(s))
            return 0;
        values[start + i] = (float)(x * c - y * s);
        values[start + i + 1ull] = (float)(x * s + y * c);
    }
    return 1;
}

static int attention_decode_flat(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    float *out,
    unsigned long long expected_elements,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row;
    unsigned long long total;
    int rc;

    if (!session || !runtime_binding || !runtime_binding->binding || !out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention flat decode requires session, binding, and output");
    binding = runtime_binding->binding;
    if (!yvex_attention_checked_mul_u64(binding->row_width, binding->row_count,
                                   &total) ||
        total != expected_elements)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            expected_elements, total, err, YVEX_ERR_BOUNDS,
            "attention flat decode expected element count mismatch");
    for (row = 0ull; row < binding->row_count; ++row) {
        rc = attention_decode_row(session, runtime_binding, row,
                                  out + row * binding->row_width,
                                  binding->row_width, result, failure, err);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

/* Contract: applies one runtime activation policy in-place over the exact
 * logical vector admitted by the architecture IR. The policy is runtime-only:
 * it never reuses source tensor quantization geometry. */
static int attention_apply_runtime_activation_policy(
    const yvex_deepseek_v4_runtime_activation_policy *policy,
    float *values,
    unsigned long long count,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    float *scratch = NULL;
    float *block_out = NULL;
    unsigned char *codes = NULL;
    unsigned long long block_width;
    unsigned long long offset;
    int rc = YVEX_OK;

    if (!policy || !policy->required) return YVEX_OK;
    if (!values || count == 0ull || policy->block_width == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "runtime activation policy requires values and block width");
    if (policy->block_axis != YVEX_DEEPSEEK_V4_RUNTIME_AXIS_FINAL_DIMENSION)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer_index, role,
            YVEX_DEEPSEEK_V4_RUNTIME_AXIS_FINAL_DIMENSION,
            (unsigned long long)policy->block_axis, err, YVEX_ERR_FORMAT,
            "runtime activation policy axis is unsupported");
    if (policy->pre_transform ==
        YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2) {
        float scale = 1.0f / sqrtf((float)count);
        scratch = (float *)yvex_attention_calloc_array(count, sizeof(*scratch));
        if (!scratch)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer_index, role, count, 0ull, err, YVEX_ERR_NOMEM,
                "runtime activation Hadamard scratch allocation failed");
        rc = yvex_attention_hadamard_cpu(
            values, count, scale, 1, scratch, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        memcpy(values, scratch, (size_t)count * sizeof(*values));
    } else if (policy->pre_transform !=
               YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer_index, role, 0ull,
            (unsigned long long)policy->pre_transform, err,
            YVEX_ERR_FORMAT,
            "runtime activation transform is unsupported");
        goto cleanup;
    }
    block_width = policy->block_width;
    if (count % block_width != 0ull) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer_index, role, block_width, count, err, YVEX_ERR_BOUNDS,
            "runtime activation policy requires exact block divisibility");
        goto cleanup;
    }
    block_out = (float *)yvex_attention_calloc_array(block_width,
                                                sizeof(*block_out));
    codes = (unsigned char *)yvex_attention_calloc_array(block_width,
                                                    sizeof(*codes));
    if (!block_out || !codes) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, role, block_width, 0ull, err, YVEX_ERR_NOMEM,
            "runtime activation codec scratch allocation failed");
        goto cleanup;
    }
    for (offset = 0ull; offset < count; offset += block_width) {
        unsigned char scale_code = 0u;

        if (policy->quantization ==
            YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT) {
            rc = yvex_attention_fp8_fake_quant_block(
                values + offset, block_width, block_out, codes, &scale_code,
                failure, err);
        } else if (policy->quantization ==
                   YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT) {
            rc = yvex_attention_fp4_fake_quant_block(
                values + offset, block_width, block_out, codes, &scale_code,
                failure, err);
        } else {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer_index, role, 0ull,
                (unsigned long long)policy->quantization, err,
                YVEX_ERR_FORMAT,
                "runtime activation quantization is unsupported");
        }
        if (rc != YVEX_OK) goto cleanup;
        memcpy(values + offset, block_out,
               (size_t)block_width * sizeof(*values));
    }
    yvex_error_clear(err);

cleanup:
    free(scratch);
    free(block_out);
    free(codes);
    return rc;
}

static const float *attention_segmented_row(
    const float *history,
    unsigned long long history_count,
    unsigned long long history_stride,
    const float *current,
    unsigned long long current_count,
    unsigned long long current_stride,
    unsigned long long index)
{
    if (index < history_count)
        return history + index * history_stride;
    index -= history_count;
    if (index >= current_count) return NULL;
    return current + index * current_stride;
}

static unsigned long long attention_segmented_position(
    const unsigned long long *history,
    unsigned long long history_count,
    const unsigned long long *current,
    unsigned long long current_count,
    unsigned long long index)
{
    if (index < history_count) return history[index];
    index -= history_count;
    if (index >= current_count) return ULLONG_MAX;
    return current[index];
}

/* Contract: scores real CSA indexer history and returns the deterministic
 * selected compressed-entry indexes for one query token. */
static int attention_csa_select(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    const float *current_indexer,
    unsigned long long current_indexer_count,
    unsigned long long current_indexer_stride,
    const unsigned long long *current_indexer_positions,
    const float *index_query,
    const float *index_weights,
    unsigned long long query_position,
    unsigned long long *selected,
    unsigned long long *selected_count,
    unsigned long long *valid_count,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long total;
    float *scores = NULL;
    unsigned long long *ordinals = NULL;
    unsigned long long *valid_indexes = NULL;
    unsigned long long candidate;
    unsigned long long valid = 0ull;
    int rc;

    if (selected_count) *selected_count = 0ull;
    if (valid_count) *valid_count = 0ull;
    if (!layer || !history || !index_query || !index_weights || !selected ||
        !selected_count || !valid_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CSA selection requires history, query, weights, and outputs");
    if (history->indexer_entry_count >
        ULLONG_MAX - current_indexer_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            history->indexer_entry_count, err, YVEX_ERR_BOUNDS,
            "CSA candidate count overflowed");
    total = history->indexer_entry_count + current_indexer_count;
    if (!total) return YVEX_OK;
    scores = (float *)yvex_attention_calloc_array(total, sizeof(*scores));
    ordinals = (unsigned long long *)yvex_attention_calloc_array(
        total, sizeof(*ordinals));
    valid_indexes = (unsigned long long *)yvex_attention_calloc_array(
        total, sizeof(*valid_indexes));
    if (!scores || !ordinals || !valid_indexes) {
        free(scores);
        free(ordinals);
        free(valid_indexes);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, total, 0ull, err,
            YVEX_ERR_NOMEM, "CSA selection scratch allocation failed");
    }
    for (candidate = 0ull; candidate < total; ++candidate) {
        const float *key = attention_segmented_row(
            history->indexer_kv, history->indexer_entry_count,
            history->indexer_kv_stride, current_indexer,
            current_indexer_count, current_indexer_stride, candidate);
        unsigned long long position = attention_segmented_position(
            history->indexer_positions, history->indexer_entry_count,
            current_indexer_positions, current_indexer_count, candidate);
        unsigned long long head;
        double score = 0.0;

        if (!key || position == ULLONG_MAX || position > query_position ||
            position > ULLONG_MAX - layer->compression_ratio + 1ull ||
            position + layer->compression_ratio - 1ull > query_position)
            continue;
        for (head = 0ull; head < layer->indexer_heads; ++head) {
            const float *q = index_query +
                             head * layer->indexer_head_dimension;
            unsigned long long lane;
            double dot = 0.0;
            for (lane = 0ull; lane < layer->indexer_head_dimension; ++lane) {
                if (!isfinite(q[lane]) || !isfinite(key[lane])) goto numeric;
                dot += (double)q[lane] * (double)key[lane];
            }
            if (dot < 0.0) dot = 0.0;
            if (!isfinite(index_weights[head])) goto numeric;
            score += dot * (double)index_weights[head];
        }
        score *= 1.0 / sqrt((double)layer->indexer_head_dimension);
        score *= 1.0 / sqrt((double)layer->indexer_heads);
        if (!isfinite(score)) goto numeric;
        scores[valid] = (float)score;
        ordinals[valid] = position;
        valid_indexes[valid] = candidate;
        valid++;
    }
    if (valid) {
        unsigned long long ranked_count = 0ull;
        unsigned long long *ranked = (unsigned long long *)yvex_attention_calloc_array(
            yvex_attention_min_u64(valid, layer->sparse_topk.k), sizeof(*ranked));
        unsigned long long i;
        if (!ranked) {
            free(scores);
            free(ordinals);
            free(valid_indexes);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, valid, 0ull,
                err, YVEX_ERR_NOMEM,
                "CSA ranked-selection scratch allocation failed");
        }
        rc = yvex_attention_topk_select(
            scores, ordinals, valid, layer->sparse_topk.k, ranked,
            &ranked_count, failure, err);
        if (rc != YVEX_OK) {
            free(ranked);
            free(scores);
            free(ordinals);
            free(valid_indexes);
            return rc;
        }
        for (i = 0ull; i < ranked_count; ++i)
            selected[i] = valid_indexes[ranked[i]];
        *selected_count = ranked_count;
        free(ranked);
    }
    *valid_count = valid;
    free(scores);
    free(ordinals);
    free(valid_indexes);
    return YVEX_OK;

numeric:
    free(scores);
    free(ordinals);
    free(valid_indexes);
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, candidate, err,
        YVEX_ERR_FORMAT, "CSA index scoring produced non-finite values");
}

/* Contract: executes exact sparse attention over admitted local and compressed
 * history, including deterministic CSA top-k and inverse RoPE/YaRN. */
static int attention_sparse_chunk_reduce(
    const yvex_attention_layer_plan *layer,
    const yvex_deepseek_v4_layer_spec *architecture,
    const float *query,
    const yvex_attention_history_view *history,
    const float *current_kv,
    unsigned long long current_kv_stride,
    const float *current_compressed,
    unsigned long long current_compressed_count,
    unsigned long long current_compressed_stride,
    const unsigned long long *current_compressed_positions,
    const float *current_indexer,
    unsigned long long current_indexer_count,
    unsigned long long current_indexer_stride,
    const unsigned long long *current_indexer_positions,
    const float *index_query,
    unsigned long long index_query_stride,
    const float *index_weights,
    unsigned long long index_weight_stride,
    const float *sinks,
    unsigned long long token_count,
    unsigned long long token_position,
    float *out,
    unsigned long long *trace_topk_counts,
    unsigned long long *trace_topk_positions,
    unsigned long long trace_topk_stride,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long compressed_total = 0ull;
    unsigned long long *selected = NULL;
    unsigned long long token;
    double scale = 1.0 / sqrt((double)layer->head_dimension);

    if (!layer || !architecture || !query || !history || !current_kv ||
        !sinks || !out || !token_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "sparse attention requires plan, history, query, KV, and output");
    if (!yvex_attention_checked_add_u64(history->compressed_entry_count,
                                   current_compressed_count,
                                   &compressed_total))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            history->compressed_entry_count, err, YVEX_ERR_BOUNDS,
            "sparse attention compressed candidate count overflowed");
    if (current_compressed_count &&
        (!current_compressed || !current_compressed_positions ||
         current_compressed_stride < layer->head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index,
            YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
            layer->head_dimension, current_compressed_stride, err,
            YVEX_ERR_FORMAT,
            "current compressed attention entries are incomplete");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
        (current_indexer_count != current_compressed_count ||
         (current_indexer_count &&
          (!current_indexer || !current_indexer_positions || !index_query ||
           !index_weights ||
           current_indexer_stride < layer->indexer_head_dimension))))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
            current_compressed_count, current_indexer_count, err,
            YVEX_ERR_FORMAT,
            "CSA current compressed and indexer entries are not paired");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
        compressed_total) {
        selected = (unsigned long long *)yvex_attention_calloc_array(
            yvex_attention_min_u64(compressed_total, layer->sparse_topk.k),
            sizeof(*selected));
        if (!selected)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                compressed_total, 0ull, err, YVEX_ERR_NOMEM,
                "sparse attention selection allocation failed");
    }
    for (token = 0ull; token < token_count; ++token) {
        unsigned long long absolute = token_position + token;
        unsigned long long selected_count = 0ull;
        unsigned long long valid_count = 0ull;
        unsigned long long head;
        int rc = YVEX_OK;

        if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
            compressed_total) {
            rc = attention_csa_select(
                layer, history, current_indexer, current_indexer_count,
                current_indexer_stride, current_indexer_positions,
                index_query + token * index_query_stride,
                index_weights + token * index_weight_stride, absolute,
                selected, &selected_count, &valid_count, failure, err);
            if (rc != YVEX_OK) {
                free(selected);
                return rc;
            }
        } else if (layer->attention_class ==
                       YVEX_DEEPSEEK_V4_ATTENTION_HCA) {
            unsigned long long candidate;
            for (candidate = 0ull; candidate < compressed_total; ++candidate) {
                unsigned long long position = attention_segmented_position(
                    history->compressed_positions,
                    history->compressed_entry_count,
                    current_compressed_positions,
                    current_compressed_count, candidate);
                if (position != ULLONG_MAX &&
                    position <= absolute &&
                    position <= ULLONG_MAX - layer->compression_ratio + 1ull &&
                    position + layer->compression_ratio - 1ull <= absolute)
                    valid_count++;
            }
            selected_count = valid_count;
        }
        if (result) {
            if (valid_count > result->topk_candidates)
                result->topk_candidates = valid_count;
            if (selected_count > result->topk_selected)
                result->topk_selected = selected_count;
        }
        if (trace_topk_counts) {
            unsigned long long i;
            if (selected_count > trace_topk_stride ||
                (selected_count && !trace_topk_positions)) {
                free(selected);
                return yvex_attention_reject(
                    failure,
                    YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    trace_topk_stride, selected_count, err,
                    YVEX_ERR_BOUNDS,
                    "CSA trace top-k extent is smaller than selection");
            }
            trace_topk_counts[token] = selected_count;
            for (i = 0ull; i < selected_count; ++i)
                trace_topk_positions[token * trace_topk_stride + i] =
                    attention_segmented_position(
                        history->compressed_positions,
                        history->compressed_entry_count,
                        current_compressed_positions,
                        current_compressed_count, selected[i]);
        }
        for (head = 0ull; head < layer->query_heads; ++head) {
            const float *q = query + token *
                layer->query_heads * layer->head_dimension +
                head * layer->head_dimension;
            float *destination = out + token *
                layer->query_heads * layer->head_dimension +
                head * layer->head_dimension;
            double maximum = (double)sinks[head];
            double denominator;
            unsigned long long lane;
            unsigned long long candidate;

            if (!isfinite(maximum)) goto numeric;
#define SCORE_CANDIDATE(row_ptr) do {                                       \
                double candidate_score = 0.0;                               \
                const float *candidate_row_ = (row_ptr);                    \
                for (lane = 0ull; lane < layer->head_dimension; ++lane) {   \
                    if (!isfinite(q[lane]) ||                               \
                        !isfinite(candidate_row_[lane])) goto numeric;      \
                    candidate_score += (double)q[lane] *                    \
                                       (double)candidate_row_[lane];        \
                }                                                           \
                candidate_score *= scale;                                   \
                if (!isfinite(candidate_score)) goto numeric;              \
                if (candidate_score > maximum) maximum = candidate_score;   \
            } while (0)
            for (candidate = 0ull; candidate < history->local_tail_count;
                 ++candidate) {
                unsigned long long position = history->local_positions[candidate];
                unsigned long long first = absolute + 1ull > layer->sliding_window
                    ? absolute + 1ull - layer->sliding_window : 0ull;
                if (position < first || position > absolute) continue;
                SCORE_CANDIDATE(history->local_kv +
                                candidate * history->local_kv_stride);
            }
            for (candidate = 0ull; candidate <= token; ++candidate)
            {
                unsigned long long position = token_position + candidate;
                unsigned long long first =
                    absolute + 1ull > layer->sliding_window
                        ? absolute + 1ull - layer->sliding_window
                        : 0ull;
                if (position >= first)
                    SCORE_CANDIDATE(current_kv +
                                    candidate * current_kv_stride);
            }
            if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_HCA) {
                for (candidate = 0ull; candidate < compressed_total;
                     ++candidate) {
                    unsigned long long position = attention_segmented_position(
                        history->compressed_positions,
                        history->compressed_entry_count,
                        current_compressed_positions,
                        current_compressed_count, candidate);
                    const float *row;
                    if (position == ULLONG_MAX || position > absolute ||
                        position >
                            ULLONG_MAX - layer->compression_ratio + 1ull ||
                        position + layer->compression_ratio - 1ull > absolute)
                        continue;
                    row = attention_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, current_compressed_stride,
                        candidate);
                    SCORE_CANDIDATE(row);
                }
            } else if (layer->attention_class ==
                           YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
                for (candidate = 0ull; candidate < selected_count; ++candidate) {
                    const float *row = attention_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, current_compressed_stride,
                        selected[candidate]);
                    SCORE_CANDIDATE(row);
                }
            }
#undef SCORE_CANDIDATE
            denominator = exp((double)sinks[head] - maximum);
            if (!isfinite(denominator)) goto numeric;
            memset(destination, 0,
                   (size_t)layer->head_dimension * sizeof(*destination));
#define ACCUMULATE_CANDIDATE(row_ptr) do {                                  \
                double candidate_score = 0.0;                               \
                double probability;                                         \
                const float *candidate_row_ = (row_ptr);                    \
                for (lane = 0ull; lane < layer->head_dimension; ++lane)     \
                    candidate_score += (double)q[lane] *                    \
                                       (double)candidate_row_[lane];        \
                probability = exp(candidate_score * scale - maximum);       \
                if (!isfinite(probability)) goto numeric;                   \
                denominator += probability;                                 \
                for (lane = 0ull; lane < layer->head_dimension; ++lane)     \
                    destination[lane] +=                                    \
                        (float)(probability * candidate_row_[lane]);         \
            } while (0)
            for (candidate = 0ull; candidate < history->local_tail_count;
                 ++candidate) {
                unsigned long long position = history->local_positions[candidate];
                unsigned long long first = absolute + 1ull > layer->sliding_window
                    ? absolute + 1ull - layer->sliding_window : 0ull;
                if (position < first || position > absolute) continue;
                ACCUMULATE_CANDIDATE(history->local_kv +
                                     candidate * history->local_kv_stride);
            }
            for (candidate = 0ull; candidate <= token; ++candidate)
            {
                unsigned long long position = token_position + candidate;
                unsigned long long first =
                    absolute + 1ull > layer->sliding_window
                        ? absolute + 1ull - layer->sliding_window
                        : 0ull;
                if (position >= first)
                    ACCUMULATE_CANDIDATE(current_kv +
                                         candidate * current_kv_stride);
            }
            if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_HCA) {
                for (candidate = 0ull; candidate < compressed_total;
                     ++candidate) {
                    unsigned long long position = attention_segmented_position(
                        history->compressed_positions,
                        history->compressed_entry_count,
                        current_compressed_positions,
                        current_compressed_count, candidate);
                    const float *row;
                    if (position == ULLONG_MAX || position > absolute ||
                        position >
                            ULLONG_MAX - layer->compression_ratio + 1ull ||
                        position + layer->compression_ratio - 1ull > absolute)
                        continue;
                    row = attention_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, current_compressed_stride,
                        candidate);
                    ACCUMULATE_CANDIDATE(row);
                }
            } else if (layer->attention_class ==
                           YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
                for (candidate = 0ull; candidate < selected_count; ++candidate) {
                    const float *row = attention_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, current_compressed_stride,
                        selected[candidate]);
                    ACCUMULATE_CANDIDATE(row);
                }
            }
#undef ACCUMULATE_CANDIDATE
            if (!isfinite(denominator) || denominator <= 0.0) goto numeric;
            for (lane = 0ull; lane < layer->head_dimension; ++lane)
                destination[lane] = (float)((double)destination[lane] /
                                            denominator);
            if (!attention_apply_rope_tail(
                    destination, layer->head_dimension,
                    architecture->rope_head_dimension, absolute,
                    &architecture->position, 1))
                goto numeric;
        }
    }
    free(selected);
    return YVEX_OK;

numeric:
    free(selected);
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, token, err,
        YVEX_ERR_FORMAT,
        "sparse attention score, softmax, or reduction became non-finite");
}

/* Contract: computes grouped output projection once per row for a token batch. */
static int attention_output_projection_batch(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *out_a,
    const yvex_runtime_tensor_binding *out_b,
    const float *attention_values,
    unsigned long long token_count,
    unsigned long long attention_stride,
    unsigned long long group_count,
    unsigned long long group_input_width,
    unsigned long long rank,
    unsigned long long hidden_width,
    float *out,
    unsigned long long output_stride,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    float *low = NULL;
    unsigned long long group;
    unsigned long long rows;
    int rc;

    if (!session || !out_a || !out_b || !attention_values || !out ||
        !token_count || attention_stride < group_count * group_input_width ||
        !group_count || !group_input_width || !rank || !hidden_width ||
        output_stride < hidden_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            out_a, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_ATTENTION_OUT_A, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "attention batch output projection requires bindings and buffers");
    low = (float *)yvex_attention_calloc_array(token_count * group_count * rank,
                                          sizeof(float));
    if (!low)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, out_a,
            out_a->layer_index, YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
            token_count * group_count * rank, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention batch output scratch");
    for (group = 0ull; group < group_count; ++group) {
        rc = attention_dot_row_range_batch(
            session, out_a, group * rank,
            attention_values + group * group_input_width, token_count,
            attention_stride, group_input_width, rank, low + group * rank,
            group_count * rank, &rows, 0, result, failure, err);
        if (rc != YVEX_OK) {
            free(low);
            return rc;
        }
        if (rows != rank) {
            free(low);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, out_a,
                out_a->layer_index, YVEX_TENSOR_ROLE_ATTENTION_OUT_A, rank,
                rows, err, YVEX_ERR_FORMAT,
                "attention batch output A projection did not produce full group rank");
        }
    }
    rc = attention_dot_row_range_batch(
        session, out_b, 0ull, low, token_count, group_count * rank,
        group_count * rank, hidden_width, out, output_stride, &rows, 0,
        result, failure, err);
    free(low);
    if (rc != YVEX_OK) return rc;
    if (rows != hidden_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, out_b,
            out_b->layer_index, YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
            hidden_width, rows, err, YVEX_ERR_FORMAT,
            "attention batch output B projection did not produce full hidden width");
    return YVEX_OK;
}

static int attention_allocate_empty_rolling_storage(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long token_position,
    float **kv_state_out,
    float **score_state_out,
    yvex_attention_rolling_state_view *view_out,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long extent;
    int overlap;
    int rotated;

    if (!layer || !kv_state_out || !score_state_out || !view_out ||
        !rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated) ||
        !yvex_attention_checked_mul_u64(state_slots, state_width, &extent))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention could not derive empty rolling-state geometry");
    *kv_state_out = (float *)yvex_attention_calloc_array(extent, sizeof(float));
    *score_state_out = (float *)yvex_attention_calloc_array(extent, sizeof(float));
    if (!*kv_state_out || !*score_state_out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, extent, 0ull, err,
            YVEX_ERR_NOMEM,
            "DeepSeek attention empty rolling-state allocation failed");
    if (!execute_init_empty_rolling_view(layer, kind, token_position,
                                           *kv_state_out, *score_state_out,
                                           view_out))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, extent, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention empty rolling-state initialization failed");
    return YVEX_OK;
}

/* Contract: creates one empty immutable rolling-state view for execution. */
static int execute_init_empty_rolling_view(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long next_token_position,
    float *kv_state,
    float *score_state,
    yvex_attention_rolling_state_view *out)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long extent;
    unsigned long long i;
    int overlap;
    int rotated;

    if (!layer || !kv_state || !score_state || !out ||
        !rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated) ||
        !yvex_attention_checked_mul_u64(state_slots, state_width, &extent))
        return 0;
    for (i = 0ull; i < extent; ++i) {
        kv_state[i] = 0.0f;
        score_state[i] = -INFINITY;
    }
    memset(out, 0, sizeof(*out));
    out->present = 1;
    out->schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1;
    out->kind = kind;
    out->attention_class = layer->attention_class;
    out->layer_index = layer->layer_index;
    out->next_token_position = next_token_position;
    out->ratio = ratio;
    out->head_dimension = head_dim;
    out->state_width = state_width;
    out->state_slots = state_slots;
    out->cursor = next_token_position % ratio;
    out->kv_state_stride = state_width;
    out->score_state_stride = state_width;
    out->kv_state_extent = extent;
    out->score_state_extent = extent;
    out->kv_state = kv_state;
    out->score_state = score_state;
    out->overlap = overlap;
    out->rotated = rotated;
    return 1;
}

/* Contract: rebinds one rolling output as the next immutable input view. */
static void attention_rolling_output_to_view(
    const yvex_attention_rolling_state_output *out,
    yvex_attention_rolling_state_view *view)
{
    memset(view, 0, sizeof(*view));
    view->present = out->present;
    view->schema_version = out->schema_version;
    view->kind = out->kind;
    view->attention_class = out->attention_class;
    view->layer_index = out->layer_index;
    view->next_token_position = out->next_token_position;
    view->ratio = out->ratio;
    view->head_dimension = out->head_dimension;
    view->state_width = out->state_width;
    view->state_slots = out->state_slots;
    view->previous_fill = out->previous_fill;
    view->current_fill = out->current_fill;
    view->cursor = out->cursor;
    view->kv_state_stride = out->kv_state_stride;
    view->score_state_stride = out->score_state_stride;
    view->kv_state_extent = out->kv_state_extent;
    view->score_state_extent = out->score_state_extent;
    view->kv_state = out->kv_state;
    view->score_state = out->score_state;
    view->overlap = out->overlap;
    view->rotated = out->rotated;
    memcpy(view->attention_plan_identity, out->attention_plan_identity,
           sizeof(view->attention_plan_identity));
}

/* Contract: seals one zero-filled component that the executor does not own yet. */
static int attention_seal_zero_component(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_component_span span;
    int rc = yvex_attention_state_transaction_acquire(
        transaction, kind, &span, failure, err);
    if (rc != YVEX_OK) return rc;
    return yvex_attention_state_transaction_seal(
        transaction, kind, span.expected_elements, failure, err);
}

/* Contract: runs one real-weight compressor transition through the state sink. */
static int attention_execute_compressor_transition_probe(
    const yvex_attention_layer_plan *layer_plan,
    const yvex_deepseek_v4_layer_spec *layer,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const float *hidden,
    unsigned long long hidden_width,
    unsigned long long token_position,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *wkv;
    const yvex_runtime_tensor_binding *wgate;
    const yvex_runtime_tensor_binding *ape;
    const yvex_runtime_tensor_binding *norm;
    const yvex_runtime_tensor_binding *index_wkv = NULL;
    const yvex_runtime_tensor_binding *index_wgate = NULL;
    const yvex_runtime_tensor_binding *index_ape = NULL;
    const yvex_runtime_tensor_binding *index_norm = NULL;
    yvex_attention_memory_sink sink;
    yvex_attention_state_transaction transaction;
    yvex_attention_history_view history;
    yvex_attention_component_span main_kv_span;
    yvex_attention_component_span main_score_span;
    yvex_attention_component_span compressed_span;
    yvex_attention_component_span index_kv_span;
    yvex_attention_component_span index_score_span;
    yvex_attention_component_span index_emit_span;
    yvex_attention_rolling_state_view main_before;
    yvex_attention_rolling_state_output main_after;
    yvex_attention_rolling_state_view index_before;
    yvex_attention_rolling_state_output index_after;
    float *main_state_kv = NULL;
    float *main_state_score = NULL;
    float *main_kv = NULL;
    float *main_score = NULL;
    float *main_ape = NULL;
    float *main_norm = NULL;
    float *index_state_kv = NULL;
    float *index_state_score = NULL;
    float *index_kv = NULL;
    float *index_score = NULL;
    float *index_ape_row = NULL;
    float *index_norm_weights = NULL;
    unsigned long long main_rows;
    unsigned long long gate_rows;
    unsigned long long index_rows = 0ull;
    unsigned long long index_gate_rows = 0ull;
    int emitted = 0;
    int index_emitted = 0;
    int rc;

    memset(&sink, 0, sizeof(sink));
    memset(&transaction, 0, sizeof(transaction));
    memset(&history, 0, sizeof(history));
    memset(&compressed_span, 0, sizeof(compressed_span));
    memset(&index_emit_span, 0, sizeof(index_emit_span));
    if (!layer_plan || !layer || !session || !descriptor || !hidden ||
        !hidden_width || !layer->compressor_required)
        return YVEX_OK;
    wkv = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
        layer_plan->layer_index);
    wgate = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
        layer_plan->layer_index);
    ape = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
        layer_plan->layer_index);
    norm = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
        layer_plan->layer_index);
    if (!wkv || !wgate || !ape || !norm)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
            layer_plan->layer_index, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
            4ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek compressor transition requires wkv/wgate/ape/norm bindings");
    main_state_kv = (float *)yvex_attention_calloc_array(
        layer_plan->compression_ratio *
            (layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA
                 ? 2ull : 1ull) *
            layer->tensors.compressor_ape_columns,
        sizeof(float));
    main_state_score = (float *)yvex_attention_calloc_array(
        layer_plan->compression_ratio *
            (layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA
                 ? 2ull : 1ull) *
            layer->tensors.compressor_ape_columns,
        sizeof(float));
    main_kv = (float *)yvex_attention_calloc_array(
        layer->tensors.compressor_ape_columns, sizeof(float));
    main_score = (float *)yvex_attention_calloc_array(
        layer->tensors.compressor_ape_columns, sizeof(float));
    main_ape = (float *)yvex_attention_calloc_array(
        layer->tensors.compressor_ape_columns, sizeof(float));
    main_norm = (float *)yvex_attention_calloc_array(layer_plan->head_dimension,
                                                sizeof(float));
    if (!main_state_kv || !main_state_score || !main_kv || !main_score ||
        !main_ape || !main_norm) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_plan->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->tensors.compressor_ape_columns, 0ull, err,
            YVEX_ERR_NOMEM,
            "DeepSeek compressor transition scratch allocation failed");
        goto cleanup;
    }
    if (!execute_init_empty_rolling_view(
            layer_plan, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
            token_position, main_state_kv, main_state_score, &main_before)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer_plan->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_FORMAT,
            "DeepSeek compressor transition could not initialize main state");
        goto cleanup;
    }
    history.immutable = 1;
    history.token_count = token_position;
    history.main_rolling_state = main_before;
    if (layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        index_wkv = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
            layer_plan->layer_index);
        index_wgate = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
            layer_plan->layer_index);
        index_ape = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
            layer_plan->layer_index);
        index_norm = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
            layer_plan->layer_index);
        if (!index_wkv || !index_wgate || !index_ape || !index_norm) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                NULL, layer_plan->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, 4ull, 0ull, err,
                YVEX_ERR_FORMAT,
                "DeepSeek indexer compressor transition requires wkv/wgate/ape/norm bindings");
            goto cleanup;
        }
        index_state_kv = (float *)yvex_attention_calloc_array(8ull * 256ull,
                                                         sizeof(float));
        index_state_score = (float *)yvex_attention_calloc_array(8ull * 256ull,
                                                            sizeof(float));
        index_kv = (float *)yvex_attention_calloc_array(
            layer->tensors.indexer_ape_columns, sizeof(float));
        index_score = (float *)yvex_attention_calloc_array(
            layer->tensors.indexer_ape_columns, sizeof(float));
        index_ape_row = (float *)yvex_attention_calloc_array(
            layer->tensors.indexer_ape_columns, sizeof(float));
        index_norm_weights = (float *)yvex_attention_calloc_array(
            layer_plan->indexer_head_dimension, sizeof(float));
        if (!index_state_kv || !index_state_score || !index_kv ||
            !index_score || !index_ape_row || !index_norm_weights) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer_plan->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                layer->tensors.indexer_ape_columns, 0ull, err,
                YVEX_ERR_NOMEM,
                "DeepSeek indexer compressor transition scratch allocation failed");
            goto cleanup;
        }
        if (!execute_init_empty_rolling_view(
                layer_plan, YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER,
                token_position, index_state_kv, index_state_score,
                &index_before)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer_plan->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
                err, YVEX_ERR_FORMAT,
                "DeepSeek indexer compressor transition could not initialize state");
            goto cleanup;
        }
        history.indexer_rolling_state = index_before;
    }
    rc = yvex_attention_memory_sink_init(
        &sink, NULL, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_begin(
        &sink, layer_plan, &history, token_position, 1ull, &transaction,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_seal_zero_component(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_seal_zero_component(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
        &main_kv_span, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
        &main_score_span, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_dot_rows(session, wkv, hidden, hidden_width,
                            layer->tensors.compressor_ape_columns, main_kv,
                            &main_rows, 0, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_dot_rows(session, wgate, hidden, hidden_width,
                            layer->tensors.compressor_ape_columns, main_score,
                            &gate_rows, 0, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (main_rows != layer->tensors.compressor_ape_columns ||
        gate_rows != layer->tensors.compressor_ape_columns) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, wkv,
            layer_plan->layer_index, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
            layer->tensors.compressor_ape_columns, main_rows, err,
            YVEX_ERR_FORMAT,
            "DeepSeek compressor projection did not produce full state width");
        goto cleanup;
    }
    rc = attention_decode_row(session, ape,
                              token_position % layer_plan->compression_ratio,
                              main_ape, layer->tensors.compressor_ape_columns,
                              result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_decode_row(session, norm, 0ull, main_norm,
                              layer_plan->head_dimension, result, failure,
                              err);
    if (rc != YVEX_OK) goto cleanup;
    memset(&main_after, 0, sizeof(main_after));
    main_after.kv_state = (float *)main_kv_span.data;
    main_after.score_state = (float *)main_score_span.data;
    main_after.kv_state_stride = main_before.kv_state_stride;
    main_after.score_state_stride = main_before.score_state_stride;
    main_after.kv_state_extent = main_before.kv_state_extent;
    main_after.score_state_extent = main_before.score_state_extent;
    if (((token_position + 1ull) % layer_plan->compression_ratio) == 0ull) {
        rc = yvex_attention_state_transaction_acquire(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
            &compressed_span, failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    rc = graph_rolling_state_step_cpu(
        layer_plan, &main_before, main_kv, main_score, main_ape, &main_after,
        compressed_span.data ? (float *)compressed_span.data : main_kv,
        layer_plan->head_dimension, &emitted, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (emitted) {
        if (!attention_apply_rms_norm((float *)compressed_span.data,
                                      layer_plan->head_dimension, main_norm,
                                      layer->rms_norm_epsilon)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, norm,
                layer_plan->layer_index,
                YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
                layer_plan->head_dimension, 0ull, err, YVEX_ERR_FORMAT,
                "DeepSeek compressor emitted non-finite normalized KV");
            goto cleanup;
        }
        if (!attention_apply_rope_tail(
                (float *)compressed_span.data, layer_plan->head_dimension,
                layer->rope_head_dimension,
                token_position + 1ull - layer_plan->compression_ratio,
                &layer->position, 0)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer_plan->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
                0ull, err, YVEX_ERR_FORMAT,
                "DeepSeek compressor RoPE/YaRN application failed");
            goto cleanup;
        }
        if (layer_plan->head_dimension > layer->rope_head_dimension) {
            rc = attention_apply_runtime_activation_policy(
                &layer_plan->compressor_activation,
                (float *)compressed_span.data,
                layer_plan->head_dimension - layer->rope_head_dimension,
                layer_plan->layer_index,
                YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
        rc = yvex_attention_state_transaction_seal(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
            compressed_span.expected_elements, failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
        main_kv_span.expected_elements, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
        main_score_span.expected_elements, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        rc = yvex_attention_state_transaction_acquire(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
            &index_kv_span, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = yvex_attention_state_transaction_acquire(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
            &index_score_span, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = attention_dot_rows(session, index_wkv, hidden, hidden_width,
                                layer->tensors.indexer_ape_columns, index_kv,
                                &index_rows, 0, result, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = attention_dot_rows(session, index_wgate, hidden, hidden_width,
                                layer->tensors.indexer_ape_columns,
                                index_score, &index_gate_rows, 0, result,
                                failure, err);
        if (rc != YVEX_OK) goto cleanup;
        if (index_rows != layer->tensors.indexer_ape_columns ||
            index_gate_rows != layer->tensors.indexer_ape_columns) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                index_wkv, layer_plan->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                layer->tensors.indexer_ape_columns, index_rows, err,
                YVEX_ERR_FORMAT,
                "DeepSeek indexer compressor did not produce full state width");
            goto cleanup;
        }
        rc = attention_decode_row(
            session, index_ape,
            token_position % layer_plan->compression_ratio, index_ape_row,
            layer->tensors.indexer_ape_columns, result, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = attention_decode_row(session, index_norm, 0ull,
                                  index_norm_weights,
                                  layer_plan->indexer_head_dimension,
                                  result, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        memset(&index_after, 0, sizeof(index_after));
        index_after.kv_state = (float *)index_kv_span.data;
        index_after.score_state = (float *)index_score_span.data;
        index_after.kv_state_stride = index_before.kv_state_stride;
        index_after.score_state_stride = index_before.score_state_stride;
        index_after.kv_state_extent = index_before.kv_state_extent;
        index_after.score_state_extent = index_before.score_state_extent;
        if (emitted) {
            rc = yvex_attention_state_transaction_acquire(
                &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                &index_emit_span, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
        rc = graph_rolling_state_step_cpu(
            layer_plan, &index_before, index_kv, index_score, index_ape_row,
            &index_after,
            index_emit_span.data ? (float *)index_emit_span.data : index_kv,
            layer_plan->indexer_head_dimension, &index_emitted, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        if (index_emitted) {
            if (!attention_apply_rms_norm((float *)index_emit_span.data,
                                          layer_plan->indexer_head_dimension,
                                          index_norm_weights,
                                          layer->rms_norm_epsilon)) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    index_norm, layer_plan->layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
                    layer_plan->indexer_head_dimension, 0ull, err,
                    YVEX_ERR_FORMAT,
                    "DeepSeek indexer compressor emitted non-finite normalized KV");
                goto cleanup;
            }
            if (!attention_apply_rope_tail(
                    (float *)index_emit_span.data,
                    layer_plan->indexer_head_dimension,
                    layer->rope_head_dimension,
                    token_position + 1ull - layer_plan->compression_ratio,
                    &layer->position, 0)) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                    layer_plan->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
                    0ull, err, YVEX_ERR_FORMAT,
                    "DeepSeek indexer RoPE/YaRN application failed");
                goto cleanup;
            }
            rc = attention_apply_runtime_activation_policy(
                &layer_plan->compressor_rotated_activation,
                (float *)index_emit_span.data,
                layer_plan->indexer_head_dimension, layer_plan->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = yvex_attention_state_transaction_seal(
                &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                index_emit_span.expected_elements, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
        rc = yvex_attention_state_transaction_seal(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
            index_kv_span.expected_elements, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = yvex_attention_state_transaction_seal(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
            index_score_span.expected_elements, failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    rc = yvex_attention_state_transaction_commit(
        &transaction, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    result->compressor_rows = main_rows;
    result->indexer_rows = index_rows;
    result->state_compressed_entries = emitted ? 1ull : 0ull;
    result->state_indexer_entries = index_emitted ? 1ull : 0ull;
    result->state_raw_entries = 1ull;
    rc = YVEX_OK;

cleanup:
    if (rc != YVEX_OK &&
        transaction.status == YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN)
        (void)yvex_attention_state_transaction_abort(
            &transaction, failure, err);
    yvex_attention_memory_sink_release(&sink);
    free(main_state_kv);
    free(main_state_score);
    free(main_kv);
    free(main_score);
    free(main_ape);
    free(main_norm);
    free(index_state_kv);
    free(index_state_score);
    free(index_kv);
    free(index_score);
    free(index_ape_row);
    free(index_norm_weights);
    return rc;
}

static int attention_softmax_probe(const float *query,
                                   unsigned long long query_count,
                                   const float *kv,
                                   unsigned long long kv_count,
                                   unsigned long long local_entries,
                                   unsigned long long compressed_entries,
                                   yvex_deepseek_v4_attention_class class_id,
                                   unsigned long long topk_limit,
                                   yvex_attention_cpu_result *result,
                                   yvex_attention_failure *failure,
                                   yvex_error *err)
{
    unsigned long long dim = yvex_attention_min_u64(query_count, kv_count);
    unsigned long long candidates = compressed_entries;
    unsigned long long selected = compressed_entries;
    float *candidate_scores = NULL;
    unsigned long long *candidate_ordinals = NULL;
    unsigned long long *selected_indices = NULL;
    unsigned long long entries;
    unsigned long long i;
    unsigned long long d;
    double max_score = -HUGE_VAL;
    double sum = 0.0;
    double output = 0.0;
    double scale;

    if (!query || !kv || !result || dim == 0ull || local_entries == 0ull)
        return 0;
    if (class_id == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        if (topk_limit == 0ull) topk_limit = 1ull;
        if (candidates <= topk_limit) candidates = topk_limit + 8ull;
        candidate_scores = (float *)yvex_attention_calloc_array(
            candidates, sizeof(*candidate_scores));
        candidate_ordinals = (unsigned long long *)yvex_attention_calloc_array(
            candidates, sizeof(*candidate_ordinals));
        selected_indices = (unsigned long long *)yvex_attention_calloc_array(
            topk_limit, sizeof(*selected_indices));
        if (!candidate_scores || !candidate_ordinals || !selected_indices)
            goto cleanup_fail;
        scale = 1.0 / sqrt((double)dim);
        for (i = 0ull; i < candidates; ++i) {
            double score = 0.0;
            double history_scale = 1.0 + (double)(i % 11ull) / 32.0;
            for (d = 0ull; d < dim; ++d)
                score += (double)query[d] * (double)kv[d] * history_scale;
            score *= scale;
            if (!isfinite(score)) goto cleanup_fail;
            candidate_scores[i] = (float)score;
            candidate_ordinals[i] = i;
        }
        if (yvex_attention_topk_select(
                candidate_scores, candidate_ordinals, candidates, topk_limit,
                selected_indices, &selected, failure, err) != YVEX_OK)
            goto cleanup_fail;
    } else if (class_id == YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        candidates = 0ull;
        selected = 0ull;
    }
    entries = local_entries + selected;
    if (entries == 0ull) return 0;
    scale = 1.0 / sqrt((double)dim);
    for (i = 0ull; i < entries; ++i) {
        double score = 0.0;
        double history_scale = 1.0 + (double)(i % 11ull) / 32.0;
        for (d = 0ull; d < dim; ++d)
            score += (double)query[d] * (double)kv[d] * history_scale;
        score *= scale;
        if (!isfinite(score)) return 0;
        if (score > max_score) max_score = score;
    }
    for (i = 0ull; i < entries; ++i) {
        double score = 0.0;
        double history_scale = 1.0 + (double)(i % 11ull) / 32.0;
        double probability;
        for (d = 0ull; d < dim; ++d)
            score += (double)query[d] * (double)kv[d] * history_scale;
        probability = exp((score * scale) - max_score);
        if (!isfinite(probability)) return 0;
        sum += probability;
        output += probability * history_scale;
    }
    if (!isfinite(sum) || sum <= 0.0 || !isfinite(output)) return 0;
    result->topk_candidates = candidates;
    result->topk_selected = selected;
    result->local_entries = local_entries;
    result->compressed_entries = compressed_entries;
    result->deduplicated_entries = entries;
    result->attention_checksum = output / sum;
    result->output_checksum = result->attention_checksum;
    free(candidate_scores);
    free(candidate_ordinals);
    free(selected_indices);
    return 1;

cleanup_fail:
    free(candidate_scores);
    free(candidate_ordinals);
    free(selected_indices);
    return 0;
}

static void graph_cpu_options_default(
    yvex_attention_cpu_options *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->token_count = 1ull;
    options->local_history_tokens = 4ull;
    options->compressed_history_tokens = 8ull;
    options->max_q_b_rows = 128ull;
    options->max_kv_rows = 512ull;
    options->max_compressor_rows = 32ull;
    options->max_indexer_rows = 64ull;
    options->scratch_limit_bytes = 64ull * 1024ull * 1024ull;
    options->collect_reference_metrics = 0;
}

/* Contract: executes a bounded CPU numerical probe over real GGUF weights. */
static int graph_cpu_probe_execute(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts = options;
    const yvex_attention_layer_plan *layer_plan;
    const yvex_deepseek_v4_layer_spec *layer;
    const yvex_runtime_tensor_binding *q_a;
    const yvex_runtime_tensor_binding *q_a_norm;
    const yvex_runtime_tensor_binding *q_b;
    const yvex_runtime_tensor_binding *kv;
    const yvex_runtime_tensor_binding *kv_norm;
    yvex_attention_state_delta delta;
    unsigned long long layer_index;
    unsigned long long hidden_width;
    unsigned long long q_rank;
    unsigned long long q_a_rows;
    unsigned long long q_b_rows;
    unsigned long long kv_rows;
    size_t scratch_bytes;
    float *hidden = NULL;
    float *q_low = NULL;
    float *q_norm_weights = NULL;
    float *query = NULL;
    float *kv_values = NULL;
    float *kv_norm_weights = NULL;
    float *tmp = NULL;
    int rc;

    attention_result_zero(result);
    if (!opts) {
        graph_cpu_options_default(&defaults);
        opts = &defaults;
    }
    if (!plan || !ir || !session || !descriptor || !result)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention CPU probe requires plan, IR, session, descriptor, and result");
    rc = yvex_attention_context_validate(
        plan, ir, session, descriptor, failure, err);
    if (rc != YVEX_OK) return rc;
    if (opts->trace && opts->trace->owned)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            NULL, opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention execution trace must be released before reuse");
    layer_index = opts->layer_index;
    layer_plan = graph_plan_layer_at(plan, layer_index);
    layer = yvex_model_register_deepseek_v4()->ir.layer_at(ir, layer_index);
    if (!layer_plan || !layer)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_BOUNDS, "DeepSeek attention CPU probe layer is absent");
    q_a = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A,
                                 layer_index);
    q_a_norm = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, layer_index);
    q_b = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
                                 layer_index);
    kv = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV,
                                layer_index);
    kv_norm = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, layer_index);
    if (!q_a || !q_a_norm || !q_b || !kv || !kv_norm)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 5ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU probe requires q_a/q_norm/q_b/kv/kv_norm bindings");
    hidden_width = q_a->binding->row_width;
    q_rank = q_a->binding->row_count;
    if (q_rank != q_b->binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, q_b,
            layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B, q_rank,
            q_b->binding->row_width, err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU probe q_b input width must match q_a rank");
    q_b_rows = yvex_attention_min_u64(opts->max_q_b_rows, q_b->binding->row_count);
    kv_rows = yvex_attention_min_u64(opts->max_kv_rows, kv->binding->row_count);
    if (!q_b_rows || !kv_rows)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU probe requires q_b and kv rows");
    if (!yvex_attention_checked_size(hidden_width + q_rank + q_rank + q_b_rows +
                                    kv_rows + kv_rows +
                                    opts->max_compressor_rows +
                                    opts->max_indexer_rows,
                                sizeof(float), &scratch_bytes) ||
        (opts->scratch_limit_bytes && scratch_bytes > opts->scratch_limit_bytes))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            opts->scratch_limit_bytes, scratch_bytes, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention CPU probe scratch budget exceeded");
    hidden = (float *)yvex_attention_calloc_array(hidden_width, sizeof(float));
    q_low = (float *)yvex_attention_calloc_array(q_rank, sizeof(float));
    q_norm_weights = (float *)yvex_attention_calloc_array(q_rank, sizeof(float));
    query = (float *)yvex_attention_calloc_array(q_b_rows, sizeof(float));
    kv_values = (float *)yvex_attention_calloc_array(kv_rows, sizeof(float));
    kv_norm_weights = (float *)yvex_attention_calloc_array(kv_rows, sizeof(float));
    tmp = (float *)yvex_attention_calloc_array(
        yvex_attention_min_u64(opts->max_compressor_rows + opts->max_indexer_rows,
                          1ull + opts->max_compressor_rows +
                              opts->max_indexer_rows),
        sizeof(float));
    if (!hidden || !q_low || !q_norm_weights || !query || !kv_values ||
        !kv_norm_weights || !tmp) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, scratch_bytes, 0ull, err,
            YVEX_ERR_NOMEM, "failed to allocate DeepSeek CPU probe scratch");
        goto cleanup;
    }
    attention_fill_activation(hidden, hidden_width, layer_index,
                              opts->token_position);
    rc = attention_dot_rows(session, q_a, hidden, hidden_width, q_rank, q_low,
                            &q_a_rows, opts->collect_reference_metrics,
                            result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_decode_row(session, q_a_norm, 0ull, q_norm_weights, q_rank,
                              result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!attention_apply_rms_norm(q_low, q_rank, q_norm_weights,
                                  layer->rms_norm_epsilon)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, q_a_norm,
            layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, q_rank, 0ull,
            err, YVEX_ERR_FORMAT, "DeepSeek q_a RMS norm produced invalid values");
        goto cleanup;
    }
    rc = attention_dot_rows(session, q_b, q_low, q_rank, q_b_rows, query,
                            &q_b_rows, opts->collect_reference_metrics,
                            result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (q_b_rows >= layer_plan->head_dimension &&
        !attention_apply_rope_tail(query, layer_plan->head_dimension,
                                   layer->rope_head_dimension,
                                   opts->token_position, &layer->position, 0)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, q_b,
            layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B, 1ull, 0ull, err,
            YVEX_ERR_FORMAT, "DeepSeek probe RoPE/YaRN application failed");
        goto cleanup;
    }
    rc = attention_dot_rows(session, kv, hidden, hidden_width, kv_rows,
                            kv_values, &kv_rows,
                            opts->collect_reference_metrics, result, failure,
                            err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_decode_row(session, kv_norm, 0ull, kv_norm_weights,
                              kv_rows, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!attention_apply_rms_norm(kv_values, kv_rows, kv_norm_weights,
                                  layer->rms_norm_epsilon)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, kv_norm,
            layer_index, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, kv_rows, 0ull,
            err, YVEX_ERR_FORMAT, "DeepSeek kv RMS norm produced invalid values");
        goto cleanup;
    }
    rc = attention_execute_compressor_transition_probe(
        layer_plan, layer, session, descriptor, hidden, hidden_width,
        opts->token_position, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!attention_softmax_probe(
            query, q_b_rows, kv_values, kv_rows,
            opts->local_history_tokens ? opts->local_history_tokens : 1ull,
            layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_SWA
                ? 0ull : opts->compressed_history_tokens,
            layer->attention_class, layer_plan->sparse_topk.k, result,
            failure, err)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_FORMAT, "DeepSeek attention softmax probe failed");
        goto cleanup;
    }
    rc = yvex_attention_state_delta_begin(
        layer_plan, opts->token_position, &delta, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_delta_commit(&delta, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    result->executed = 1;
    result->full_attention = 0;
    result->reference_independent = result->reference_comparisons ? 1 : 0;
    result->cuda_executed = 0;
    result->layer_index = layer_index;
    result->attention_class = layer->attention_class;
    result->token_position = opts->token_position;
    result->q_a_rows = q_a_rows;
    result->q_b_rows = q_b_rows;
    result->kv_rows = kv_rows;
    result->state_raw_entries = delta.raw_kv_entries;
    result->state_compressed_entries = delta.compressed_kv_entries;
    result->state_indexer_entries = delta.indexer_entries;
    result->q_projection_checksum = attention_checksum(q_low, q_rank);
    result->kv_projection_checksum = attention_checksum(kv_values, kv_rows);
    result->rope_checksum = attention_checksum(query, q_b_rows);
    if (result->reference_comparisons)
        result->rmse =
            sqrt(result->rmse / (double)result->reference_comparisons);
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    rc = YVEX_OK;

cleanup:
    free(hidden);
    free(q_low);
    free(q_norm_weights);
    free(query);
    free(kv_values);
    free(kv_norm_weights);
    free(tmp);
    return rc;
}

/* Contract: executes one bounded multi-token CPU attention chunk. */
static int graph_cpu_chunk_execute(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts = options;
    const yvex_attention_layer_plan *layer_plan;
    const yvex_deepseek_v4_layer_spec *layer;
    const yvex_runtime_tensor_binding *q_a;
    const yvex_runtime_tensor_binding *q_a_norm;
    const yvex_runtime_tensor_binding *q_b;
    const yvex_runtime_tensor_binding *kv;
    const yvex_runtime_tensor_binding *kv_norm;
    const yvex_runtime_tensor_binding *sinks;
    const yvex_runtime_tensor_binding *out_a;
    const yvex_runtime_tensor_binding *out_b;
    const yvex_runtime_tensor_binding *index_q_binding = NULL;
    const yvex_runtime_tensor_binding *index_weight_binding = NULL;
    yvex_attention_memory_sink sink;
    yvex_attention_state_transaction transaction;
    yvex_attention_history_view history;
    yvex_attention_component_span output_span;
    yvex_attention_component_span raw_kv_span;
    yvex_attention_component_span main_kv_span;
    yvex_attention_component_span main_score_span;
    yvex_attention_component_span compressed_span;
    yvex_attention_component_span index_kv_span;
    yvex_attention_component_span index_score_span;
    yvex_attention_component_span index_emit_span;
    yvex_attention_rolling_state_view main_before;
    yvex_attention_rolling_state_view main_current;
    yvex_attention_rolling_state_output main_after;
    yvex_attention_rolling_state_view index_before;
    yvex_attention_rolling_state_view index_current;
    yvex_attention_rolling_state_output index_after;
    const yvex_attention_component_span *committed_output = NULL;
    const yvex_attention_component_span *committed_raw = NULL;
    const yvex_attention_component_span *committed_compressed = NULL;
    const yvex_attention_component_span *committed_indexer = NULL;
    const yvex_attention_component_span *committed_main_kv_state = NULL;
    const yvex_attention_component_span *committed_main_score_state = NULL;
    const yvex_attention_component_span *committed_index_kv_state = NULL;
    const yvex_attention_component_span *committed_index_score_state = NULL;
    const char *committed_identity = NULL;
    unsigned long long layer_index;
    unsigned long long token_count;
    unsigned long long hidden_width;
    unsigned long long q_rank;
    unsigned long long query_width;
    unsigned long long kv_width;
    unsigned long long rows;
    unsigned long long token;
    unsigned long long emitted_count = 0ull;
    unsigned long long index_emitted_count = 0ull;
    unsigned long long hidden_elements = 0ull;
    unsigned long long q_low_elements = 0ull;
    unsigned long long query_elements = 0ull;
    unsigned long long kv_elements = 0ull;
    unsigned long long scratch_elements = 0ull;
    unsigned long long scratch_term = 0ull;
    size_t scratch_bytes;
    float *hidden = NULL;
    float *q_low = NULL;
    float *q_norm_weights = NULL;
    float *query = NULL;
    float *kv_norm_weights = NULL;
    float *sink_values = NULL;
    float *attention_values = NULL;
    float *main_state_kv = NULL;
    float *main_state_score = NULL;
    float *main_kv = NULL;
    float *main_score = NULL;
    float *main_ape = NULL;
    float *main_norm = NULL;
    float *index_state_kv = NULL;
    float *index_state_score = NULL;
    float *index_kv = NULL;
    float *index_score = NULL;
    float *index_ape = NULL;
    float *index_norm = NULL;
    float *index_query = NULL;
    float *index_weights = NULL;
    unsigned long long *compressed_positions = NULL;
    unsigned long long *index_positions = NULL;
    unsigned long long *trace_topk_counts = NULL;
    unsigned long long *trace_topk_positions = NULL;
    unsigned long long trace_topk_stride = 0ull;
    int rc;

    attention_result_zero(result);
    memset(&sink, 0, sizeof(sink));
    memset(&transaction, 0, sizeof(transaction));
    memset(&history, 0, sizeof(history));
    memset(&output_span, 0, sizeof(output_span));
    memset(&raw_kv_span, 0, sizeof(raw_kv_span));
    memset(&main_kv_span, 0, sizeof(main_kv_span));
    memset(&main_score_span, 0, sizeof(main_score_span));
    memset(&compressed_span, 0, sizeof(compressed_span));
    memset(&index_kv_span, 0, sizeof(index_kv_span));
    memset(&index_score_span, 0, sizeof(index_score_span));
    memset(&index_emit_span, 0, sizeof(index_emit_span));
    memset(&main_before, 0, sizeof(main_before));
    memset(&main_current, 0, sizeof(main_current));
    memset(&main_after, 0, sizeof(main_after));
    memset(&index_before, 0, sizeof(index_before));
    memset(&index_current, 0, sizeof(index_current));
    memset(&index_after, 0, sizeof(index_after));
    if (!opts) {
        graph_cpu_options_default(&defaults);
        opts = &defaults;
    }
    token_count = opts->token_count ? opts->token_count : 1ull;
    if (!plan || !ir || !session || !descriptor || !result)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention CPU chunk requires plan, IR, session, descriptor, and result");
    rc = yvex_attention_context_validate(
        plan, ir, session, descriptor, failure, err);
    if (rc != YVEX_OK) return rc;
    if (opts->trace && opts->trace->owned)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            NULL, opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention execution trace must be released before reuse");
    layer_index = opts->layer_index;
    layer_plan = graph_plan_layer_at(plan, layer_index);
    layer = yvex_model_register_deepseek_v4()->ir.layer_at(ir, layer_index);
    if (!layer_plan || !layer)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_BOUNDS, "DeepSeek attention CPU chunk layer is absent");
    q_a = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A,
                                 layer_index);
    q_a_norm = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, layer_index);
    q_b = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
                                 layer_index);
    kv = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV,
                                layer_index);
    kv_norm = attention_find_binding(
        descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, layer_index);
    sinks = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_SINKS,
                                   layer_index);
    out_a = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
                                   layer_index);
    out_b = attention_find_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
                                   layer_index);
    if (!q_a || !q_a_norm || !q_b || !kv || !kv_norm || !sinks || !out_a ||
        !out_b)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 8ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk requires complete Q/KV/sink/output bindings");
    hidden_width = q_a->binding->row_width;
    q_rank = q_a->binding->row_count;
    query_width = q_b->binding->row_count;
    kv_width = kv->binding->row_count;
    if (q_rank != q_b->binding->row_width ||
        query_width != layer_plan->query_heads * layer_plan->head_dimension ||
        kv_width != layer_plan->head_dimension ||
        hidden_width != layer_plan->hidden_dimension)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, q_b,
            layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
            layer_plan->query_heads * layer_plan->head_dimension,
            query_width, err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk tensor dimensions do not match plan");
    if (!yvex_attention_checked_mul_u64(token_count, hidden_width,
                                   &hidden_elements) ||
        !yvex_attention_checked_mul_u64(token_count, q_rank, &q_low_elements) ||
        !yvex_attention_checked_mul_u64(token_count, query_width,
                                   &query_elements) ||
        !yvex_attention_checked_mul_u64(token_count, kv_width, &kv_elements) ||
        !yvex_attention_checked_add_u64(hidden_elements, q_low_elements,
                                   &scratch_elements) ||
        !yvex_attention_checked_add_u64(scratch_elements, query_elements,
                                   &scratch_elements) ||
        !yvex_attention_checked_add_u64(scratch_elements, kv_elements,
                                   &scratch_elements) ||
        !yvex_attention_checked_add_u64(scratch_elements, query_elements,
                                   &scratch_elements) ||
        !yvex_attention_checked_add_u64(q_rank, kv_width, &scratch_term) ||
        !yvex_attention_checked_add_u64(scratch_term, layer_plan->query_heads,
                                   &scratch_term) ||
        !yvex_attention_checked_add_u64(scratch_elements, scratch_term,
                                   &scratch_elements) ||
        !yvex_attention_checked_size(scratch_elements, sizeof(float),
                                &scratch_bytes) ||
        (opts->scratch_limit_bytes && scratch_bytes > opts->scratch_limit_bytes))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, opts->scratch_limit_bytes,
            scratch_bytes, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention CPU chunk scratch budget exceeded");
    if (opts->history) {
        history = *opts->history;
        if (history.token_count != opts->token_position) {
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer_index, YVEX_TENSOR_ROLE_UNKNOWN, opts->token_position,
                history.token_count, err, YVEX_ERR_STATE,
                "DeepSeek attention CPU chunk history is not contiguous");
        }
        main_before = history.main_rolling_state;
        index_before = history.indexer_rolling_state;
    }
    if (!opts->history &&
        layer_plan->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        rc = attention_allocate_empty_rolling_storage(
            layer_plan, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
            opts->token_position, &main_state_kv, &main_state_score,
            &main_before, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        history.main_rolling_state = main_before;
        if (layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
            rc = attention_allocate_empty_rolling_storage(
                layer_plan, YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER,
                opts->token_position, &index_state_kv, &index_state_score,
                &index_before, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            history.indexer_rolling_state = index_before;
        }
    }
    history.immutable = 1;
    history.token_count = opts->token_position;
    rc = yvex_attention_memory_sink_init(&sink, NULL, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_begin(
        &sink, layer_plan, &history, opts->token_position, token_count,
        &transaction, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        &output_span, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
        &raw_kv_span, failure, err);
    if (rc != YVEX_OK) goto cleanup;

    hidden = (float *)yvex_attention_calloc_array(hidden_elements,
                                             sizeof(float));
    q_low = (float *)yvex_attention_calloc_array(q_low_elements,
                                            sizeof(float));
    q_norm_weights = (float *)yvex_attention_calloc_array(q_rank, sizeof(float));
    query = (float *)yvex_attention_calloc_array(query_elements,
                                            sizeof(float));
    kv_norm_weights = (float *)yvex_attention_calloc_array(kv_width, sizeof(float));
    sink_values = (float *)yvex_attention_calloc_array(layer_plan->query_heads,
                                                  sizeof(float));
    attention_values = (float *)yvex_attention_calloc_array(query_elements,
                                                        sizeof(float));
    if (!hidden || !q_low || !q_norm_weights || !query || !kv_norm_weights ||
        !sink_values || !attention_values) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, scratch_bytes, 0ull, err,
            YVEX_ERR_NOMEM,
            "failed to allocate DeepSeek CPU chunk scratch");
        goto cleanup;
    }
    if (opts->input && opts->input_stride < hidden_width) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, hidden_width,
            opts->input_stride, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention input stride is shorter than hidden width");
        goto cleanup;
    }
    for (token = 0ull; token < token_count; ++token) {
        if (opts->input) {
            unsigned long long lane;
            const float *source = opts->input + token * opts->input_stride;
            for (lane = 0ull; lane < hidden_width; ++lane) {
                if (!isfinite(source[lane])) {
                    rc = yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        NULL, layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                        hidden_width, lane, err, YVEX_ERR_FORMAT,
                        "DeepSeek attention input contains non-finite values");
                    goto cleanup;
                }
            }
            memcpy(hidden + token * hidden_width, source,
                   (size_t)hidden_width * sizeof(*hidden));
        } else {
            attention_fill_activation(
                hidden + token * hidden_width, hidden_width, layer_index,
                opts->token_position + token);
        }
    }
    rc = attention_dot_row_range_batch(
        session, q_a, 0ull, hidden, token_count, hidden_width, hidden_width,
        q_rank, q_low, q_rank, &rows, opts->collect_reference_metrics,
        result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (rows != q_rank) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, q_a,
            layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_A, q_rank, rows, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk Q-A projection is incomplete");
        goto cleanup;
    }
    rc = attention_decode_row(session, q_a_norm, 0ull, q_norm_weights,
                              q_rank, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    for (token = 0ull; token < token_count; ++token) {
        if (!attention_apply_rms_norm(q_low + token * q_rank, q_rank,
                                      q_norm_weights,
                                      layer->rms_norm_epsilon)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, q_a_norm,
                layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, q_rank,
                token, err, YVEX_ERR_FORMAT,
                "DeepSeek attention CPU chunk Q norm produced invalid values");
            goto cleanup;
        }
    }
    rc = attention_dot_row_range_batch(
        session, q_b, 0ull, q_low, token_count, q_rank, q_rank, query_width,
        query, query_width, &rows, opts->collect_reference_metrics, result,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_dot_row_range_batch(
        session, kv, 0ull, hidden, token_count, hidden_width, hidden_width,
        kv_width, (float *)raw_kv_span.data, raw_kv_span.stride, &rows,
        opts->collect_reference_metrics, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_decode_row(session, kv_norm, 0ull, kv_norm_weights,
                              kv_width, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    for (token = 0ull; token < token_count; ++token) {
        unsigned long long head;
        for (head = 0ull; head < layer_plan->query_heads; ++head) {
            if (!attention_apply_unit_rms_norm(
                    query + token * query_width +
                        head * layer_plan->head_dimension,
                    layer_plan->head_dimension, layer->rms_norm_epsilon)) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, q_b,
                    layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
                    layer_plan->head_dimension, head, err, YVEX_ERR_FORMAT,
                    "DeepSeek attention query head RMS norm failed");
                goto cleanup;
            }
            if (!attention_apply_rope_tail(
                    query + token * query_width +
                        head * layer_plan->head_dimension,
                    layer_plan->head_dimension, layer->rope_head_dimension,
                    opts->token_position + token, &layer->position, 0)) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, q_b,
                    layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B, 1ull, head,
                    err, YVEX_ERR_FORMAT,
                    "DeepSeek attention query RoPE/YaRN application failed");
                goto cleanup;
            }
        }
        if (!attention_apply_rms_norm(
                (float *)raw_kv_span.data + token * raw_kv_span.stride,
                kv_width, kv_norm_weights, layer->rms_norm_epsilon)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, kv_norm,
                layer_index, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, kv_width,
                token, err, YVEX_ERR_FORMAT,
                "DeepSeek attention CPU chunk KV norm produced invalid values");
            goto cleanup;
        }
        if (!attention_apply_rope_tail(
                (float *)raw_kv_span.data + token * raw_kv_span.stride,
                kv_width, layer->rope_head_dimension,
                opts->token_position + token, &layer->position, 0)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, kv,
                layer_index, YVEX_TENSOR_ROLE_ATTENTION_KV, 1ull, token, err,
                YVEX_ERR_FORMAT,
                "DeepSeek attention KV RoPE/YaRN application failed");
            goto cleanup;
        }
        if (kv_width > layer->rope_head_dimension) {
            rc = attention_apply_runtime_activation_policy(
                &layer_plan->attention_kv_activation,
                (float *)raw_kv_span.data + token * raw_kv_span.stride,
                kv_width - layer->rope_head_dimension, layer_index,
                YVEX_TENSOR_ROLE_ATTENTION_KV, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
    }
    rc = attention_decode_flat(session, sinks, sink_values,
                               layer_plan->query_heads, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;

    if (layer_plan->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        const yvex_runtime_tensor_binding *wkv = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
            layer_index);
        const yvex_runtime_tensor_binding *wgate = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
            layer_index);
        const yvex_runtime_tensor_binding *ape = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
            layer_index);
        const yvex_runtime_tensor_binding *norm = attention_find_binding(
            descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
            layer_index);
        if (!wkv || !wgate || !ape || !norm) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                NULL, layer_index, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
                4ull, 0ull, err, YVEX_ERR_FORMAT,
                "DeepSeek attention CPU chunk requires compressor bindings");
            goto cleanup;
        }
        rc = yvex_attention_state_transaction_acquire(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
            &main_kv_span, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = yvex_attention_state_transaction_acquire(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
            &main_score_span, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        main_kv = (float *)yvex_attention_calloc_array(
            token_count * main_before.state_width, sizeof(float));
        main_score = (float *)yvex_attention_calloc_array(
            token_count * main_before.state_width, sizeof(float));
        main_ape = (float *)yvex_attention_calloc_array(main_before.state_width,
                                                   sizeof(float));
        main_norm = (float *)yvex_attention_calloc_array(layer_plan->head_dimension,
                                                    sizeof(float));
        if (!main_kv || !main_score || !main_ape || !main_norm) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                token_count * main_before.state_width, 0ull, err,
                YVEX_ERR_NOMEM,
                "DeepSeek attention CPU chunk compressor scratch allocation failed");
            goto cleanup;
        }
        rc = attention_dot_row_range_batch(
            session, wkv, 0ull, hidden, token_count, hidden_width,
            hidden_width, main_before.state_width, main_kv,
            main_before.state_width, &rows, 0, result, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = attention_dot_row_range_batch(
            session, wgate, 0ull, hidden, token_count, hidden_width,
            hidden_width, main_before.state_width, main_score,
            main_before.state_width, &rows, 0, result, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = attention_decode_row(session, norm, 0ull, main_norm,
                                  layer_plan->head_dimension, result,
                                  failure, err);
        if (rc != YVEX_OK) goto cleanup;
        if (transaction.components[
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV]
                .required) {
            rc = yvex_attention_state_transaction_acquire(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
                &compressed_span, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            compressed_positions = (unsigned long long *)yvex_attention_calloc_array(
                compressed_span.dims[0], sizeof(*compressed_positions));
            if (!compressed_positions) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                    NULL, layer_index,
                    YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
                    compressed_span.dims[0], 0ull, err, YVEX_ERR_NOMEM,
                    "DeepSeek attention compressed-position allocation failed");
                goto cleanup;
            }
        }
        main_current = main_before;
        for (token = 0ull; token < token_count; ++token) {
            int emitted = 0;
            float *compressed_out =
                compressed_span.data
                    ? (float *)compressed_span.data +
                          emitted_count * compressed_span.stride
                    : main_kv + token * main_before.state_width;
            rc = attention_decode_row(
                session, ape,
                (opts->token_position + token) % layer_plan->compression_ratio,
                main_ape, main_before.state_width, result, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            memset(&main_after, 0, sizeof(main_after));
            main_after.kv_state = (float *)main_kv_span.data;
            main_after.score_state = (float *)main_score_span.data;
            main_after.kv_state_stride = main_before.kv_state_stride;
            main_after.score_state_stride = main_before.score_state_stride;
            main_after.kv_state_extent = main_before.kv_state_extent;
            main_after.score_state_extent = main_before.score_state_extent;
            rc = graph_rolling_state_step_cpu(
                layer_plan, &main_current,
                main_kv + token * main_before.state_width,
                main_score + token * main_before.state_width, main_ape,
                &main_after, compressed_out, layer_plan->head_dimension,
                &emitted, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            if (emitted) {
                unsigned long long emission_position =
                    opts->token_position + token + 1ull -
                    layer_plan->compression_ratio;

                if (!attention_apply_rms_norm(
                        compressed_out, layer_plan->head_dimension,
                        main_norm, layer->rms_norm_epsilon)) {
                    rc = yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        norm, layer_index,
                        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
                        layer_plan->head_dimension, emitted_count, err,
                        YVEX_ERR_FORMAT,
                        "DeepSeek attention CPU chunk compressor emission invalid");
                    goto cleanup;
                }
                if (!attention_apply_rope_tail(
                        compressed_out, layer_plan->head_dimension,
                        layer->rope_head_dimension,
                        emission_position,
                        &layer->position, 0)) {
                    rc = yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        NULL, layer_index,
                        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, 1ull,
                        token, err, YVEX_ERR_FORMAT,
                        "DeepSeek attention compressor RoPE/YaRN failed");
                    goto cleanup;
                }
                if (layer_plan->head_dimension > layer->rope_head_dimension) {
                    rc = attention_apply_runtime_activation_policy(
                        &layer_plan->compressor_activation, compressed_out,
                        layer_plan->head_dimension -
                            layer->rope_head_dimension,
                        layer_index,
                        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, failure,
                        err);
                    if (rc != YVEX_OK) goto cleanup;
                }
                if (!compressed_positions ||
                    emitted_count >= compressed_span.dims[0]) {
                    rc = yvex_attention_reject(
                        failure,
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                        layer_index,
                        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
                        compressed_span.dims[0], emitted_count, err,
                        YVEX_ERR_BOUNDS,
                        "DeepSeek attention compressor emitted beyond planned positions");
                    goto cleanup;
                }
                compressed_positions[emitted_count] = emission_position;
                emitted_count++;
            }
            attention_rolling_output_to_view(&main_after, &main_current);
        }
        if (compressed_span.data) {
            rc = yvex_attention_state_transaction_seal(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
                compressed_span.expected_elements, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
        rc = yvex_attention_state_transaction_seal(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
            main_kv_span.expected_elements, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = yvex_attention_state_transaction_seal(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
            main_score_span.expected_elements, failure, err);
        if (rc != YVEX_OK) goto cleanup;

        if (layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
            const yvex_runtime_tensor_binding *index_wkv =
                attention_find_binding(
                    descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                    layer_index);
            const yvex_runtime_tensor_binding *index_wgate =
                attention_find_binding(
                    descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
                    layer_index);
            const yvex_runtime_tensor_binding *index_ape_binding =
                attention_find_binding(
                    descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
                    layer_index);
            const yvex_runtime_tensor_binding *index_norm_binding =
                attention_find_binding(
                    descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
                    layer_index);
            if (!index_wkv || !index_wgate || !index_ape_binding ||
                !index_norm_binding) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                    NULL, layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, 4ull, 0ull, err,
                    YVEX_ERR_FORMAT,
                    "DeepSeek attention CPU chunk requires indexer compressor bindings");
                goto cleanup;
            }
            rc = yvex_attention_state_transaction_acquire(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
                &index_kv_span, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = yvex_attention_state_transaction_acquire(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
                &index_score_span, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            if (transaction.components[YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV]
                    .required) {
                rc = yvex_attention_state_transaction_acquire(
                    &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                    &index_emit_span, failure, err);
                if (rc != YVEX_OK) goto cleanup;
                index_positions =
                    (unsigned long long *)yvex_attention_calloc_array(
                        index_emit_span.dims[0],
                        sizeof(*index_positions));
                if (!index_positions) {
                    rc = yvex_attention_reject(
                        failure,
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                        layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                        index_emit_span.dims[0], 0ull, err,
                        YVEX_ERR_NOMEM,
                        "DeepSeek attention indexer-position allocation failed");
                    goto cleanup;
                }
            }
            index_kv = (float *)yvex_attention_calloc_array(
                token_count * index_before.state_width, sizeof(float));
            index_score = (float *)yvex_attention_calloc_array(
                token_count * index_before.state_width, sizeof(float));
            index_ape = (float *)yvex_attention_calloc_array(
                index_before.state_width, sizeof(float));
            index_norm = (float *)yvex_attention_calloc_array(
                layer_plan->indexer_head_dimension, sizeof(float));
            if (!index_kv || !index_score || !index_ape || !index_norm) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                    NULL, layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    token_count * index_before.state_width, 0ull, err,
                    YVEX_ERR_NOMEM,
                    "DeepSeek attention CPU chunk indexer scratch allocation failed");
                goto cleanup;
            }
            rc = attention_dot_row_range_batch(
                session, index_wkv, 0ull, hidden, token_count, hidden_width,
                hidden_width, index_before.state_width, index_kv,
                index_before.state_width, &rows, 0, result, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = attention_dot_row_range_batch(
                session, index_wgate, 0ull, hidden, token_count, hidden_width,
                hidden_width, index_before.state_width, index_score,
                index_before.state_width, &rows, 0, result, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = attention_decode_row(
                session, index_norm_binding, 0ull, index_norm,
                layer_plan->indexer_head_dimension, result, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            index_current = index_before;
            for (token = 0ull; token < token_count; ++token) {
                int emitted = 0;
                float *index_out =
                    index_emit_span.data
                        ? (float *)index_emit_span.data +
                              index_emitted_count * index_emit_span.stride
                        : index_kv + token * index_before.state_width;
                rc = attention_decode_row(
                    session, index_ape_binding,
                    (opts->token_position + token) %
                        layer_plan->compression_ratio,
                    index_ape, index_before.state_width, result, failure,
                    err);
                if (rc != YVEX_OK) goto cleanup;
                memset(&index_after, 0, sizeof(index_after));
                index_after.kv_state = (float *)index_kv_span.data;
                index_after.score_state = (float *)index_score_span.data;
                index_after.kv_state_stride = index_before.kv_state_stride;
                index_after.score_state_stride = index_before.score_state_stride;
                index_after.kv_state_extent = index_before.kv_state_extent;
                index_after.score_state_extent = index_before.score_state_extent;
                rc = graph_rolling_state_step_cpu(
                    layer_plan, &index_current,
                    index_kv + token * index_before.state_width,
                    index_score + token * index_before.state_width,
                    index_ape, &index_after, index_out,
                    layer_plan->indexer_head_dimension, &emitted, failure,
                    err);
                if (rc != YVEX_OK) goto cleanup;
                if (emitted) {
                    unsigned long long emission_position =
                        opts->token_position + token + 1ull -
                        layer_plan->compression_ratio;

                    if (!attention_apply_rms_norm(
                            index_out, layer_plan->indexer_head_dimension,
                            index_norm, layer->rms_norm_epsilon)) {
                        rc = yvex_attention_reject(
                            failure,
                            YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                            index_norm_binding, layer_index,
                            YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
                            layer_plan->indexer_head_dimension,
                            index_emitted_count, err, YVEX_ERR_FORMAT,
                            "DeepSeek attention CPU chunk indexer emission invalid");
                        goto cleanup;
                    }
                    if (!attention_apply_rope_tail(
                            index_out, layer_plan->indexer_head_dimension,
                            layer->rope_head_dimension,
                            emission_position,
                            &layer->position, 0)) {
                        rc = yvex_attention_reject(
                            failure,
                            YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                            layer_index,
                            YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, 1ull,
                            token, err, YVEX_ERR_FORMAT,
                            "DeepSeek indexer compressor RoPE/YaRN failed");
                        goto cleanup;
                    }
                    rc = attention_apply_runtime_activation_policy(
                        &layer_plan->compressor_rotated_activation, index_out,
                        layer_plan->indexer_head_dimension, layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, failure,
                        err);
                    if (rc != YVEX_OK) goto cleanup;
                    if (!index_positions ||
                        index_emitted_count >= index_emit_span.dims[0]) {
                        rc = yvex_attention_reject(
                            failure,
                            YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA,
                            NULL, layer_index,
                            YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                            index_emit_span.dims[0], index_emitted_count,
                            err, YVEX_ERR_BOUNDS,
                            "DeepSeek attention indexer emitted beyond planned positions");
                        goto cleanup;
                    }
                    index_positions[index_emitted_count] = emission_position;
                    index_emitted_count++;
                }
                attention_rolling_output_to_view(&index_after,
                                                 &index_current);
            }
            if (index_emit_span.data) {
                rc = yvex_attention_state_transaction_seal(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                    index_emit_span.expected_elements, failure, err);
                if (rc != YVEX_OK) goto cleanup;
            }
            rc = yvex_attention_state_transaction_seal(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
                index_kv_span.expected_elements, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = yvex_attention_state_transaction_seal(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
                index_score_span.expected_elements, failure, err);
            if (rc != YVEX_OK) goto cleanup;

            index_q_binding = attention_find_binding(
                descriptor, YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                layer_index);
            index_weight_binding = attention_find_binding(
                descriptor, YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
                layer_index);
            if (!index_q_binding || !index_weight_binding ||
                index_q_binding->binding->row_width != q_rank ||
                index_q_binding->binding->row_count !=
                    layer_plan->indexer_heads *
                        layer_plan->indexer_head_dimension ||
                index_weight_binding->binding->row_width != hidden_width ||
                index_weight_binding->binding->row_count !=
                    layer_plan->indexer_heads) {
                rc = yvex_attention_reject(
                    failure,
                    YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                    index_q_binding, layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                    layer_plan->indexer_heads *
                        layer_plan->indexer_head_dimension,
                    index_q_binding ? index_q_binding->binding->row_count
                                    : 0ull,
                    err, YVEX_ERR_FORMAT,
                    "DeepSeek CSA index query/weight bindings do not match the plan");
                goto cleanup;
            }
            if (!yvex_attention_checked_mul_u64(
                    token_count,
                    layer_plan->indexer_heads *
                        layer_plan->indexer_head_dimension,
                    &scratch_term)) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                    index_q_binding, layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, ULLONG_MAX,
                    token_count, err, YVEX_ERR_BOUNDS,
                    "DeepSeek CSA index query extent overflowed");
                goto cleanup;
            }
            index_query = (float *)yvex_attention_calloc_array(
                scratch_term, sizeof(*index_query));
            index_weights = (float *)yvex_attention_calloc_array(
                token_count * layer_plan->indexer_heads,
                sizeof(*index_weights));
            if (!index_query || !index_weights) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                    index_q_binding, layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, scratch_term,
                    0ull, err, YVEX_ERR_NOMEM,
                    "DeepSeek CSA index query/weight allocation failed");
                goto cleanup;
            }
            rc = attention_dot_row_range_batch(
                session, index_q_binding, 0ull, q_low, token_count, q_rank,
                q_rank,
                layer_plan->indexer_heads *
                    layer_plan->indexer_head_dimension,
                index_query,
                layer_plan->indexer_heads *
                    layer_plan->indexer_head_dimension,
                &rows, 0, result, failure, err);
            if (rc != YVEX_OK) goto cleanup;
            if (rows != layer_plan->indexer_heads *
                            layer_plan->indexer_head_dimension) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                    index_q_binding, layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                    layer_plan->indexer_heads *
                        layer_plan->indexer_head_dimension,
                    rows, err, YVEX_ERR_FORMAT,
                    "DeepSeek CSA index query projection is incomplete");
                goto cleanup;
            }
            rc = attention_dot_row_range_batch(
                session, index_weight_binding, 0ull, hidden, token_count,
                hidden_width, hidden_width, layer_plan->indexer_heads,
                index_weights, layer_plan->indexer_heads, &rows, 0, result,
                failure, err);
            if (rc != YVEX_OK) goto cleanup;
            if (rows != layer_plan->indexer_heads) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                    index_weight_binding, layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
                    layer_plan->indexer_heads, rows, err, YVEX_ERR_FORMAT,
                    "DeepSeek CSA index weight projection is incomplete");
                goto cleanup;
            }
            for (token = 0ull; token < token_count; ++token) {
                unsigned long long head;
                for (head = 0ull; head < layer_plan->indexer_heads; ++head) {
                    float *head_query = index_query +
                        token * layer_plan->indexer_heads *
                            layer_plan->indexer_head_dimension +
                        head * layer_plan->indexer_head_dimension;
                    if (!attention_apply_rope_tail(
                            head_query,
                            layer_plan->indexer_head_dimension,
                            layer->rope_head_dimension,
                            opts->token_position + token,
                            &layer->position, 0)) {
                        rc = yvex_attention_reject(
                            failure,
                            YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                            index_q_binding, layer_index,
                            YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, 1ull,
                            head, err, YVEX_ERR_FORMAT,
                            "DeepSeek CSA index query RoPE/YaRN failed");
                        goto cleanup;
                    }
                    rc = attention_apply_runtime_activation_policy(
                        &layer_plan->indexer_query_activation, head_query,
                        layer_plan->indexer_head_dimension, layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, failure,
                        err);
                    if (rc != YVEX_OK) goto cleanup;
                }
            }
        }
    }

    if (opts->trace &&
        layer_plan->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        unsigned long long compressed_total;
        unsigned long long topk_extent;

        if (!yvex_attention_checked_add_u64(history.compressed_entry_count,
                                       emitted_count,
                                       &compressed_total) ||
            !yvex_attention_checked_mul_u64(
                token_count,
                yvex_attention_min_u64(compressed_total,
                                   layer_plan->sparse_topk.k),
                &topk_extent)) {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
                layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
                emitted_count, err, YVEX_ERR_BOUNDS,
                "DeepSeek CSA trace top-k extent overflowed");
            goto cleanup;
        }
        trace_topk_stride = yvex_attention_min_u64(
            compressed_total, layer_plan->sparse_topk.k);
        if (trace_topk_stride) {
            trace_topk_counts =
                (unsigned long long *)yvex_attention_calloc_array(
                    token_count, sizeof(*trace_topk_counts));
            trace_topk_positions =
                (unsigned long long *)yvex_attention_calloc_array(
                    topk_extent, sizeof(*trace_topk_positions));
            if (!trace_topk_counts || !trace_topk_positions) {
                rc = yvex_attention_reject(
                    failure,
                    YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                    layer_index, YVEX_TENSOR_ROLE_UNKNOWN, topk_extent,
                    0ull, err, YVEX_ERR_NOMEM,
                    "DeepSeek CSA trace top-k allocation failed");
                goto cleanup;
            }
        }
    }
    rc = attention_sparse_chunk_reduce(
        layer_plan, layer, query, &history,
        (const float *)raw_kv_span.data, raw_kv_span.stride,
        compressed_span.data ? (const float *)compressed_span.data : NULL,
        emitted_count, compressed_span.stride, compressed_positions,
        index_emit_span.data ? (const float *)index_emit_span.data : NULL,
        index_emitted_count, index_emit_span.stride, index_positions,
        index_query,
        layer_plan->indexer_heads * layer_plan->indexer_head_dimension,
        index_weights, layer_plan->indexer_heads, sink_values, token_count,
        opts->token_position, attention_values, trace_topk_counts,
        trace_topk_positions, trace_topk_stride, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = attention_output_projection_batch(
        session, out_a, out_b, attention_values, token_count, query_width,
        layer->output_groups, layer->output_group_input_width,
        layer->output_lora_rank, hidden_width, (float *)output_span.data,
        output_span.stride, result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        output_span.expected_elements, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
        raw_kv_span.expected_elements, failure, err);
    if (rc != YVEX_OK) goto cleanup;

    rc = yvex_attention_state_transaction_commit(
        &transaction, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    committed_output = yvex_attention_memory_sink_committed_component(
        &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT);
    committed_raw = yvex_attention_memory_sink_committed_component(
        &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV);
    committed_compressed =
        yvex_attention_memory_sink_committed_component(
            &sink,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV);
    committed_indexer =
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV);
    committed_main_kv_state =
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE);
    committed_main_score_state =
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE);
    committed_index_kv_state =
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE);
    committed_index_score_state =
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE);
    committed_identity = yvex_attention_memory_sink_identity(&sink);
    if (!committed_output || !committed_output->data || !committed_raw ||
        !committed_raw->data || !committed_identity) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention CPU chunk commit did not publish output identity");
        goto cleanup;
    }
    if (opts->trace &&
        !attention_trace_capture(
            opts->trace, layer_index, layer->attention_class,
            opts->token_position, token_count, hidden_width, q_rank,
            query_width, kv_width, hidden, q_low, query,
            (const float *)committed_raw->data,
            committed_compressed
                ? (const float *)committed_compressed->data
                : NULL,
            emitted_count,
            committed_compressed ? committed_compressed->stride : 0ull,
            compressed_positions,
            committed_indexer ? (const float *)committed_indexer->data
                              : NULL,
            index_emitted_count,
            committed_indexer ? committed_indexer->stride : 0ull,
            index_positions,
            index_query,
            layer_plan->indexer_heads *
                layer_plan->indexer_head_dimension,
            index_weights, layer_plan->indexer_heads, attention_values,
            (const float *)committed_output->data, trace_topk_counts,
            trace_topk_positions, trace_topk_stride,
            main_after.present ? &main_after : NULL,
            committed_main_kv_state
                ? (const float *)committed_main_kv_state->data : NULL,
            committed_main_score_state
                ? (const float *)committed_main_score_state->data : NULL,
            index_after.present ? &index_after : NULL,
            committed_index_kv_state
                ? (const float *)committed_index_kv_state->data : NULL,
            committed_index_score_state
                ? (const float *)committed_index_score_state->data : NULL)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_NOMEM,
            "DeepSeek attention execution trace capture failed");
        goto cleanup;
    }
    result->executed = 1;
    result->full_attention = 1;
    result->reference_independent = result->reference_comparisons ? 1 : 0;
    result->cuda_executed = 0;
    result->layer_index = layer_index;
    result->attention_class = layer->attention_class;
    result->token_position = opts->token_position;
    result->q_a_rows = q_rank;
    result->q_b_rows = query_width;
    result->kv_rows = kv_width;
    result->local_entries = history.local_tail_count + token_count;
    result->compressed_entries = emitted_count;
    result->deduplicated_entries = history.local_tail_count + token_count;
    result->state_raw_entries = token_count;
    result->state_compressed_entries = emitted_count;
    result->state_indexer_entries = index_emitted_count;
    result->q_projection_checksum = attention_checksum(q_low,
                                                       token_count * q_rank);
    result->kv_projection_checksum =
        attention_checksum((const float *)committed_raw->data,
                           committed_raw->expected_elements);
    result->rope_checksum = attention_checksum(query, token_count * query_width);
    result->attention_checksum =
        attention_checksum(attention_values, token_count * query_width);
    result->output_checksum =
        attention_checksum((const float *)committed_output->data,
                           committed_output->expected_elements);
    (void)snprintf(result->output_identity, sizeof(result->output_identity),
                   "%s", committed_identity);
    if (result->reference_comparisons)
        result->rmse =
            sqrt(result->rmse / (double)result->reference_comparisons);
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    rc = YVEX_OK;

cleanup:
    if (rc != YVEX_OK &&
        transaction.status == YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN)
        (void)yvex_attention_state_transaction_abort(
            &transaction, failure, err);
    yvex_attention_memory_sink_release(&sink);
    free(hidden);
    free(q_low);
    free(q_norm_weights);
    free(query);
    free(kv_norm_weights);
    free(sink_values);
    free(attention_values);
    free(main_state_kv);
    free(main_state_score);
    free(main_kv);
    free(main_score);
    free(main_ape);
    free(main_norm);
    free(index_state_kv);
    free(index_state_score);
    free(index_kv);
    free(index_score);
    free(index_ape);
    free(index_norm);
    free(index_query);
    free(index_weights);
    free(compressed_positions);
    free(index_positions);
    free(trace_topk_counts);
    free(trace_topk_positions);
    return rc;
}

/* Contract: executes one complete first-token attention layer on CPU. */
static int graph_cpu_first_token_execute(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts = options;

    if (!opts) {
        graph_cpu_options_default(&defaults);
        opts = &defaults;
    } else {
        defaults = *opts;
        opts = &defaults;
    }
    if (opts->token_position != 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            opts->token_position, err, YVEX_ERR_UNSUPPORTED,
            "DeepSeek first-token attention executor requires empty history at position zero");
    defaults.token_position = 0ull;
    defaults.token_count = 1ull;
    return graph_cpu_chunk_execute(
        plan, ir, session, descriptor, &defaults, result, failure, err);
}

/* CUDA composition projects typed family roles onto the generic backend ABI. */

#include <yvex/backend.h>

#include <stdint.h>

typedef struct {
    unsigned char *owned[YVEX_BACKEND_ATTENTION_WEIGHT_COUNT];
    unsigned long long payload_bytes_read;
} attention_cuda_weights;

static const yvex_runtime_tensor_binding *attention_cuda_find_binding(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    unsigned long long layer_index)
{
    return yvex_runtime_descriptor_find_role(
        descriptor, role, YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
        layer_index, YVEX_DEEPSEEK_GGUF_NO_INDEX);
}

static void attention_cuda_weights_release(attention_cuda_weights *weights)
{
    unsigned int i;
    if (!weights) return;
    for (i = 0u; i < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++i) {
        free(weights->owned[i]);
        weights->owned[i] = NULL;
    }
    weights->payload_bytes_read = 0ull;
}

/* Contract: reads one complete admitted encoded tensor into bounded host scratch. */
static int attention_cuda_load_weight(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    yvex_backend_attention_weight_slot slot,
    attention_cuda_weights *owned,
    yvex_backend_attention_job *job,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    yvex_backend_attention_weight *weight;
    unsigned long long blocks;
    unsigned long long row_bytes;
    unsigned long long expected;
    int rc;

    if (!session || !runtime_binding || !runtime_binding->binding || !owned ||
        !job || slot >= YVEX_BACKEND_ATTENTION_WEIGHT_COUNT)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention weight load requires a typed binding and slot");
    binding = runtime_binding->binding;
    if (!binding->row_width || !binding->row_count || !binding->block_size ||
        !binding->bytes_per_block ||
        binding->row_width % binding->block_size != 0ull ||
        binding->encoded_bytes > (unsigned long long)SIZE_MAX)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->block_size, binding->row_width, err, YVEX_ERR_BOUNDS,
            "CUDA attention encoded tensor geometry is invalid");
    blocks = binding->row_width / binding->block_size;
    if (!yvex_attention_checked_mul_u64(blocks, binding->bytes_per_block,
                                   &row_bytes) ||
        !yvex_attention_checked_mul_u64(row_bytes, binding->row_count, &expected) ||
        expected != binding->encoded_bytes)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->encoded_bytes, expected, err, YVEX_ERR_FORMAT,
            "CUDA attention encoded tensor range is not row-exact");
    owned->owned[slot] = (unsigned char *)malloc((size_t)binding->encoded_bytes);
    if (!owned->owned[slot])
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            binding->encoded_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "CUDA attention encoded weight allocation failed");
    rc = yvex_materialization_session_read(
        session, binding, 0ull, owned->owned[slot],
        (size_t)binding->encoded_bytes, NULL, err);
    if (rc != YVEX_OK)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ, runtime_binding,
            binding->layer_index, binding->role, binding->encoded_bytes, 0ull,
            err, (yvex_status)rc,
            "CUDA attention failed to read admitted encoded weight");
    if (!yvex_attention_checked_add_u64(owned->payload_bytes_read,
                                   binding->encoded_bytes,
                                   &owned->payload_bytes_read))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role, ULLONG_MAX,
            binding->encoded_bytes, err, YVEX_ERR_BOUNDS,
            "CUDA attention payload-byte accounting overflowed");
    weight = &job->weights[slot];
    weight->encoded = owned->owned[slot];
    weight->encoded_bytes = (size_t)binding->encoded_bytes;
    weight->row_bytes = row_bytes;
    weight->row_width = binding->row_width;
    weight->row_count = binding->row_count;
    weight->qtype = binding->qtype;
    weight->present = 1;
    return YVEX_OK;
}

static int attention_cuda_load_role(
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_backend_attention_weight_slot slot,
    attention_cuda_weights *owned,
    yvex_backend_attention_job *job,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *binding =
        attention_cuda_find_binding(descriptor, role, layer_index);
    if (!binding)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention required typed role binding is absent");
    return attention_cuda_load_weight(
        session, binding, slot, owned, job, failure, err);
}

static void attention_cuda_activation_project(
    const yvex_deepseek_v4_runtime_activation_policy *source,
    yvex_backend_attention_activation *out)
{
    memset(out, 0, sizeof(*out));
    if (!source || !source->required) return;
    out->required = 1;
    out->block_width = source->block_width;
    out->quantization = (unsigned int)source->quantization;
    out->hadamard = source->pre_transform ==
        YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2;
}

static int attention_cuda_rolling_project(
    const yvex_attention_rolling_state_view *source,
    yvex_backend_attention_rolling *out)
{
    if (!source || !source->present || !out) return 0;
    *out = (yvex_backend_attention_rolling){
        .present = 1,
        .ratio = source->ratio,
        .head_dimension = source->head_dimension,
        .state_width = source->state_width,
        .state_slots = source->state_slots,
        .cursor = source->cursor,
        .previous_fill = source->previous_fill,
        .current_fill = source->current_fill,
        .kv_state = source->kv_state,
        .score_state = source->score_state,
        .overlap = source->overlap,
    };
    return 1;
}

static int attention_cuda_trace_allocate(
    yvex_attention_execution_trace *trace,
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    unsigned long long token_position,
    const float *input,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long query_width;
    unsigned long long index_query_width = 0ull;
    unsigned long long topk_capacity = 0ull;
    unsigned long long extent;

    if (!trace || !layer || !history || !input ||
        !yvex_attention_checked_mul_u64(layer->query_heads,
                                   layer->head_dimension, &query_width))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention trace allocation requires plan and input");
    memset(trace, 0, sizeof(*trace));
#define ALLOC_TRACE(field, count, type) do {                                  \
        trace->field = (type *)yvex_attention_calloc_array((count), sizeof(type));  \
        if ((count) && !trace->field) goto allocation_failure;                \
    } while (0)
    ALLOC_TRACE(input, layer->hidden_dimension, float);
    ALLOC_TRACE(q_low, layer->query_lora_rank, float);
    ALLOC_TRACE(query, query_width, float);
    ALLOC_TRACE(raw_kv, layer->head_dimension, float);
    ALLOC_TRACE(attention_values, query_width, float);
    ALLOC_TRACE(output, layer->hidden_dimension, float);
    memcpy(trace->input, input,
           (size_t)layer->hidden_dimension * sizeof(*trace->input));
    if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        ALLOC_TRACE(compressed_kv, layer->head_dimension, float);
        ALLOC_TRACE(compressed_positions, 1ull, unsigned long long);
        if (!yvex_attention_checked_mul_u64(
                history->main_rolling_state.state_width,
                history->main_rolling_state.state_slots, &extent))
            goto allocation_failure;
        ALLOC_TRACE(next_main_rolling_state.kv_state, extent, float);
        ALLOC_TRACE(next_main_rolling_state.score_state, extent, float);
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        if (!yvex_attention_checked_mul_u64(
                layer->indexer_heads, layer->indexer_head_dimension,
                &index_query_width))
            goto allocation_failure;
        ALLOC_TRACE(indexer_kv, layer->indexer_head_dimension, float);
        ALLOC_TRACE(indexer_positions, 1ull, unsigned long long);
        ALLOC_TRACE(index_query, index_query_width, float);
        ALLOC_TRACE(index_weights, layer->indexer_heads, float);
        if (!yvex_attention_checked_add_u64(history->compressed_entry_count, 1ull,
                                       &extent))
            goto allocation_failure;
        topk_capacity = yvex_attention_min_u64(extent, layer->sparse_topk.k);
        ALLOC_TRACE(topk_counts, 1ull, unsigned long long);
        ALLOC_TRACE(topk_positions, topk_capacity, unsigned long long);
        if (!yvex_attention_checked_mul_u64(
                history->indexer_rolling_state.state_width,
                history->indexer_rolling_state.state_slots, &extent))
            goto allocation_failure;
        ALLOC_TRACE(next_indexer_rolling_state.kv_state, extent, float);
        ALLOC_TRACE(next_indexer_rolling_state.score_state, extent, float);
    }
#undef ALLOC_TRACE
    trace->owned = 1;
    trace->layer_index = layer->layer_index;
    trace->attention_class = layer->attention_class;
    trace->token_position = token_position;
    trace->token_count = 1ull;
    trace->hidden_width = layer->hidden_dimension;
    trace->q_rank = layer->query_lora_rank;
    trace->query_width = query_width;
    trace->kv_width = layer->head_dimension;
    trace->compressed_stride = layer->head_dimension;
    trace->indexer_stride = layer->indexer_head_dimension;
    trace->index_query_stride = layer->attention_class ==
        YVEX_DEEPSEEK_V4_ATTENTION_CSA ? index_query_width : 0ull;
    trace->index_weight_stride = layer->indexer_heads;
    trace->topk_stride = topk_capacity;
    return YVEX_OK;

allocation_failure:
#undef ALLOC_TRACE
    graph_execution_trace_release(trace);
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
        YVEX_ERR_NOMEM, "CUDA attention trace allocation failed");
}

static void attention_cuda_rolling_commit(
    const yvex_attention_rolling_state_view *before,
    unsigned long long token_position,
    yvex_attention_rolling_state_output *after)
{
    int emitted;
    if (!before || !before->present || !after) return;
    emitted = ((token_position + 1ull) % before->ratio) == 0ull;
    after->present = 1;
    after->schema_version = before->schema_version;
    after->kind = before->kind;
    after->attention_class = before->attention_class;
    after->layer_index = before->layer_index;
    after->next_token_position = token_position + 1ull;
    after->ratio = before->ratio;
    after->head_dimension = before->head_dimension;
    after->state_width = before->state_width;
    after->state_slots = before->state_slots;
    after->previous_fill = emitted
        ? (before->overlap ? before->ratio : 0ull) : before->previous_fill;
    after->current_fill = emitted ? 0ull :
        (before->current_fill < before->cursor + 1ull
            ? before->cursor + 1ull : before->current_fill);
    after->cursor = emitted ? 0ull : (before->cursor + 1ull) % before->ratio;
    after->kv_state_stride = before->state_width;
    after->score_state_stride = before->state_width;
    if (!yvex_attention_checked_mul_u64(before->state_width, before->state_slots,
                                   &after->kv_state_extent))
        after->kv_state_extent = 0ull;
    after->score_state_extent = after->kv_state_extent;
    after->overlap = before->overlap;
    after->rotated = before->rotated;
    memcpy(after->attention_plan_identity, before->attention_plan_identity,
           sizeof(after->attention_plan_identity));
}

static double attention_cuda_checksum(const float *values,
                                      unsigned long long count)
{
    double checksum = 0.0;
    unsigned long long i;
    for (i = 0ull; values && i < count; ++i)
        checksum += (double)values[i] * (double)((i % 17ull) + 1ull);
    return checksum;
}

static int attention_cuda_output_identity(
    const yvex_attention_plan *plan,
    const yvex_attention_execution_trace *trace,
    char out[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    const yvex_attention_summary *summary =
        graph_plan_summary(plan);
    size_t bytes;
    if (!summary || !trace || !out ||
        !yvex_attention_checked_size(trace->hidden_width, sizeof(float), &bytes))
        return 0;
    yvex_sha256_init(&hash);
    if (!yvex_attention_hash_text(&hash, "yvex.deepseek.attention.cuda.output.v1") ||
        !yvex_attention_hash_text(&hash, summary->attention_plan_identity) ||
        !yvex_attention_hash_u64(&hash, trace->layer_index) ||
        !yvex_attention_hash_u64(&hash, trace->token_position) ||
        !yvex_sha256_update(&hash, trace->output, bytes) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}

/* Contract: executes one complete stateful attention token through CUDA only. */
static int graph_cuda_token_execute(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_backend *backend,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    static const struct {
        yvex_tensor_role role;
        yvex_backend_attention_weight_slot slot;
    } base_roles[] = {
        {YVEX_TENSOR_ROLE_ATTENTION_Q_A, YVEX_BACKEND_ATTENTION_WEIGHT_Q_A},
        {YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM,
         YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM},
        {YVEX_TENSOR_ROLE_ATTENTION_Q_B, YVEX_BACKEND_ATTENTION_WEIGHT_Q_B},
        {YVEX_TENSOR_ROLE_ATTENTION_KV, YVEX_BACKEND_ATTENTION_WEIGHT_KV},
        {YVEX_TENSOR_ROLE_ATTENTION_KV_NORM,
         YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM},
        {YVEX_TENSOR_ROLE_ATTENTION_SINKS,
         YVEX_BACKEND_ATTENTION_WEIGHT_SINKS},
        {YVEX_TENSOR_ROLE_ATTENTION_OUT_A, YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A},
        {YVEX_TENSOR_ROLE_ATTENTION_OUT_B, YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B},
    };
    static const struct {
        yvex_tensor_role role;
        yvex_backend_attention_weight_slot slot;
    } compressor_roles[] = {
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
         YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV},
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
         YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE},
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
         YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE},
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
         YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM},
    };
    static const struct {
        yvex_tensor_role role;
        yvex_backend_attention_weight_slot slot;
    } index_roles[] = {
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
         YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV},
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
         YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE},
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
         YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE},
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
         YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM},
        {YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
         YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q},
        {YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
         YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION},
    };
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts = options;
    const yvex_attention_layer_plan *layer;
    const yvex_deepseek_v4_layer_spec *architecture;
    yvex_attention_history_view empty_history;
    const yvex_attention_history_view *history;
    attention_cuda_weights weights;
    yvex_backend_attention_job job;
    yvex_backend_attention_output cuda_output;
    yvex_backend_attention_failure cuda_failure;
    yvex_attention_execution_trace trace;
    unsigned int i;
    int rc;

    if (result) memset(result, 0, sizeof(*result));
    memset(&weights, 0, sizeof(weights));
    memset(&job, 0, sizeof(job));
    memset(&cuda_output, 0, sizeof(cuda_output));
    memset(&trace, 0, sizeof(trace));
    memset(&empty_history, 0, sizeof(empty_history));
    if (!opts) {
        graph_cpu_options_default(&defaults);
        opts = &defaults;
    }
    if (!plan || !ir || !session || !descriptor || !backend || !result ||
        !opts->input || opts->input_stride == 0ull ||
        (opts->token_count && opts->token_count != 1ull))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            opts ? opts->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention requires one explicit input token and backend");
    rc = yvex_attention_context_validate(
        plan, ir, session, descriptor, failure, err);
    if (rc != YVEX_OK) return rc;
    if (opts->trace && opts->trace->owned)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull, err,
            YVEX_ERR_STATE,
            "CUDA attention trace must be released before reuse");
    layer = graph_plan_layer_at(plan, opts->layer_index);
    architecture = yvex_model_register_deepseek_v4()->ir.layer_at(ir, opts->layer_index);
    if (!layer || !architecture || opts->input_stride < layer->hidden_dimension)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer ? layer->hidden_dimension : 1ull, opts->input_stride, err,
            YVEX_ERR_BOUNDS,
            "CUDA attention layer or input stride is invalid");
    history = opts->history ? opts->history : &empty_history;
    if (history->token_count != opts->token_position)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            opts->token_position, history->token_count, err, YVEX_ERR_STATE,
            "CUDA attention history is not contiguous");
    if (opts->history) {
        rc = graph_history_validate(
            layer, history, failure, err);
        if (rc != YVEX_OK) return rc;
    } else if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "compressed CUDA attention requires explicit rolling history");
    }
    rc = attention_cuda_trace_allocate(
        &trace, layer, history, opts->token_position, opts->input, failure,
        err);
    if (rc != YVEX_OK) return rc;
    job.attention_class = (unsigned int)layer->attention_class;
    job.token_position = opts->token_position;
    job.hidden_width = layer->hidden_dimension;
    job.q_rank = layer->query_lora_rank;
    job.query_heads = layer->query_heads;
    job.head_dimension = layer->head_dimension;
    job.kv_width = layer->head_dimension;
    job.sliding_window = layer->sliding_window;
    job.compression_ratio = layer->compression_ratio;
    job.output_groups = layer->output_groups;
    job.output_group_input_width = architecture->output_group_input_width;
    job.output_rank = layer->output_lora_rank;
    job.indexer_heads = layer->indexer_heads;
    job.indexer_head_dimension = layer->indexer_head_dimension;
    job.indexer_topk = layer->sparse_topk.k;
    job.rms_epsilon = architecture->rms_norm_epsilon;
    job.position.theta = architecture->position.theta;
    job.position.scaling_factor = architecture->position.scaling_factor;
    job.position.original_context = architecture->position.original_context;
    job.position.beta_fast = architecture->position.beta_fast;
    job.position.beta_slow = architecture->position.beta_slow;
    job.position.rope_dimensions = architecture->rope_head_dimension;
    attention_cuda_activation_project(
        &layer->attention_kv_activation, &job.attention_kv_activation);
    attention_cuda_activation_project(
        &layer->compressor_activation, &job.compressor_activation);
    attention_cuda_activation_project(
        &layer->compressor_rotated_activation,
        &job.compressor_rotated_activation);
    attention_cuda_activation_project(
        &layer->indexer_query_activation, &job.indexer_query_activation);
    job.input = opts->input;
    job.local_kv = history->local_kv;
    job.local_positions = history->local_positions;
    job.local_count = history->local_tail_count;
    job.local_stride = history->local_kv_stride;
    job.compressed_kv = history->compressed_kv;
    job.compressed_positions = history->compressed_positions;
    job.compressed_count = history->compressed_entry_count;
    job.compressed_stride = history->compressed_kv_stride;
    job.indexer_kv = history->indexer_kv;
    job.indexer_positions = history->indexer_positions;
    job.indexer_count = history->indexer_entry_count;
    job.indexer_stride = history->indexer_kv_stride;
    if (history->main_rolling_state.present)
        (void)attention_cuda_rolling_project(
            &history->main_rolling_state, &job.main_rolling);
    if (history->indexer_rolling_state.present)
        (void)attention_cuda_rolling_project(
            &history->indexer_rolling_state, &job.indexer_rolling);
    job.max_device_bytes = 1024ull * 1024ull * 1024ull;
    for (i = 0u; i < sizeof(base_roles) / sizeof(base_roles[0]); ++i) {
        rc = attention_cuda_load_role(
            session, descriptor, layer->layer_index, base_roles[i].role,
            base_roles[i].slot, &weights, &job, failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        for (i = 0u; i < sizeof(compressor_roles) /
                              sizeof(compressor_roles[0]); ++i) {
            rc = attention_cuda_load_role(
                session, descriptor, layer->layer_index,
                compressor_roles[i].role, compressor_roles[i].slot,
                &weights, &job, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        for (i = 0u; i < sizeof(index_roles) / sizeof(index_roles[0]); ++i) {
            rc = attention_cuda_load_role(
                session, descriptor, layer->layer_index, index_roles[i].role,
                index_roles[i].slot, &weights, &job, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
    }
    cuda_output.q_low = trace.q_low;
    cuda_output.query = trace.query;
    cuda_output.raw_kv = trace.raw_kv;
    cuda_output.compressed_kv = trace.compressed_kv;
    cuda_output.indexer_kv = trace.indexer_kv;
    cuda_output.index_query = trace.index_query;
    cuda_output.index_weights = trace.index_weights;
    cuda_output.attention_values = trace.attention_values;
    cuda_output.output = trace.output;
    cuda_output.compressed_positions = trace.compressed_positions;
    cuda_output.indexer_positions = trace.indexer_positions;
    cuda_output.topk_positions = trace.topk_positions;
    cuda_output.main_kv_state = trace.next_main_rolling_state.kv_state;
    cuda_output.main_score_state = trace.next_main_rolling_state.score_state;
    cuda_output.indexer_kv_state = trace.next_indexer_rolling_state.kv_state;
    cuda_output.indexer_score_state = trace.next_indexer_rolling_state.score_state;
    rc = yvex_backend_attention_execute(
        backend, &job, &cuda_output, &cuda_failure, err);
    if (rc != YVEX_OK) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            cuda_failure.expected, cuda_failure.actual, err,
            (yvex_status)rc,
            cuda_failure.stage ? cuda_failure.stage :
                "CUDA attention backend execution failed");
        goto cleanup;
    }
    trace.compressed_count = cuda_output.compressed_count;
    trace.indexer_count = cuda_output.indexer_count;
    trace.compressed_stride = trace.compressed_count
        ? layer->head_dimension : 0ull;
    trace.indexer_stride = trace.indexer_count
        ? layer->indexer_head_dimension : 0ull;
    if (trace.topk_counts) trace.topk_counts[0] = cuda_output.topk_count;
    if (history->main_rolling_state.present)
        attention_cuda_rolling_commit(
            &history->main_rolling_state, opts->token_position,
            &trace.next_main_rolling_state);
    if (history->indexer_rolling_state.present)
        attention_cuda_rolling_commit(
            &history->indexer_rolling_state, opts->token_position,
            &trace.next_indexer_rolling_state);
    trace.complete = 1;
    result->executed = 1;
    result->full_attention = 1;
    result->reference_independent = 0;
    result->cuda_executed = 1;
    result->layer_index = layer->layer_index;
    result->attention_class = layer->attention_class;
    result->token_position = opts->token_position;
    result->q_a_rows = layer->query_lora_rank;
    result->q_b_rows = layer->query_heads * layer->head_dimension;
    result->kv_rows = layer->head_dimension;
    result->topk_candidates = cuda_output.valid_candidate_count;
    result->topk_selected = cuda_output.topk_count;
    result->local_entries = history->local_tail_count + 1ull;
    result->compressed_entries = cuda_output.compressed_count;
    result->deduplicated_entries = history->local_tail_count + 1ull;
    result->payload_bytes_read = weights.payload_bytes_read;
    result->state_raw_entries = 1ull;
    result->state_compressed_entries = cuda_output.compressed_count;
    result->state_indexer_entries = cuda_output.indexer_count;
    result->cuda_kernel_launches = cuda_output.kernel_launches;
    result->cuda_peak_device_bytes = cuda_output.peak_device_bytes;
    result->q_projection_checksum = attention_cuda_checksum(
        trace.q_low, trace.q_rank);
    result->kv_projection_checksum = attention_cuda_checksum(
        trace.raw_kv, trace.kv_width);
    result->rope_checksum = attention_cuda_checksum(
        trace.query, trace.query_width);
    result->attention_checksum = attention_cuda_checksum(
        trace.attention_values, trace.query_width);
    result->output_checksum = attention_cuda_checksum(
        trace.output, trace.hidden_width);
    if (!attention_cuda_output_identity(plan, &trace,
                                        result->output_identity)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "CUDA attention output identity construction failed");
        goto cleanup;
    }
    if (opts->trace) {
        *opts->trace = trace;
        memset(&trace, 0, sizeof(trace));
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    rc = YVEX_OK;

cleanup:
    attention_cuda_weights_release(&weights);
    graph_execution_trace_release(&trace);
    if (rc != YVEX_OK && result) memset(result, 0, sizeof(*result));
    return rc;
}

/* Publishes one immutable graph-family lowering surface.  Phase helpers and
 * execution composition remain private to this translation unit. */
const yvex_graph_family_api *yvex_graph_lower_deepseek_v4(void)
{
    static const yvex_graph_family_api api = {
        graph_plan_build,
        graph_plan_close,
        graph_plan_summary,
        graph_plan_layer_count,
        graph_plan_layer_at,
        graph_history_validate,
        graph_rolling_state_validate,
        graph_rolling_state_step_cpu,
        graph_cpu_options_default,
        graph_execution_trace_release,
        graph_cpu_probe_execute,
        graph_cpu_first_token_execute,
        graph_cuda_token_execute,
        graph_cpu_chunk_execute
    };

    return &api;
}
