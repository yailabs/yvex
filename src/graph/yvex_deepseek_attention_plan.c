/*
 * yvex_deepseek_attention_plan.c - DeepSeek-V4 attention plan owner.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   immutable plan construction, runtime binding admission, schedule accounting, and attention-plan identity.
 *
 * Does not own:
 *   source parsing, GGUF mapping, materialization ownership, persistent KV,
 *   prefill, decode, logits, sampling, generation, eval, benchmark, release
 *   claims, CLI parsing, or rendering.
 *
 * Invariants:
 *   plan construction reads zero tensor payload bytes and refuses missing or non-computable bindings before execution.
 *
 * Boundary:
 *   attention execution readiness does not imply persistent KV, transformer,
 *   prefill, decode, or generation readiness.
 */
#include "yvex_deepseek_attention_internal.h"

#include "src/model/compilation/yvex_deepseek_transform_ir.h"

static void attention_hash_activation_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_activation_policy *policy);
static void attention_hash_sparse_topk_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_sparse_topk_policy *policy);

static void attention_compute_identity(yvex_deepseek_attention_plan *plan)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long i;

    yvex_sha256_init(&hash);
    (void)attention_hash_text(&hash, "yvex.deepseek.attention.plan.v1");
    (void)attention_hash_text(&hash, plan->summary.artifact_identity);
    (void)attention_hash_text(&hash, plan->summary.materialization_plan_identity);
    (void)attention_hash_text(&hash, plan->summary.logical_model_identity);
    (void)attention_hash_text(&hash, plan->summary.runtime_descriptor_identity);
    (void)attention_hash_text(&hash, plan->summary.runtime_numeric_identity);
    (void)attention_hash_u64(&hash, plan->summary.layer_count);
    for (i = 0ull; i < plan->layer_count; ++i) {
        const yvex_deepseek_attention_layer_plan *layer = &plan->layers[i];
        (void)attention_hash_u64(&hash, layer->layer_index);
        (void)attention_hash_u64(&hash, layer->attention_class);
        (void)attention_hash_u64(&hash, layer->compression_ratio);
        (void)attention_hash_u64(&hash, layer->sliding_window);
        (void)attention_hash_u64(&hash, layer->query_heads);
        (void)attention_hash_u64(&hash, layer->kv_heads);
        (void)attention_hash_u64(&hash, layer->head_dimension);
        (void)attention_hash_u64(&hash, layer->rope_head_dimension);
        (void)attention_hash_u64(&hash, layer->query_lora_rank);
        (void)attention_hash_u64(&hash, layer->output_lora_rank);
        (void)attention_hash_u64(&hash, layer->output_groups);
        (void)attention_hash_u64(&hash, layer->hidden_dimension);
        (void)attention_hash_u64(&hash, layer->indexer_heads);
        (void)attention_hash_u64(&hash, layer->indexer_head_dimension);
        (void)attention_hash_u64(&hash, layer->indexer_topk);
        attention_hash_activation_policy(&hash,
                                         &layer->attention_kv_activation);
        attention_hash_activation_policy(&hash, &layer->compressor_activation);
        attention_hash_activation_policy(
            &hash, &layer->compressor_rotated_activation);
        attention_hash_activation_policy(&hash,
                                         &layer->indexer_query_activation);
        attention_hash_sparse_topk_policy(&hash, &layer->sparse_topk);
        (void)attention_hash_u64(&hash, layer->required_binding_count);
        (void)attention_hash_u64(&hash, layer->qtype_compute_refusal_count);
        (void)attention_hash_u64(&hash, layer->payload_bytes_bound);
    }
    (void)yvex_sha256_final(&hash, digest);
    yvex_sha256_hex(digest, plan->summary.attention_plan_identity);
}

static void attention_hash_activation_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_activation_policy *policy)
{
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->required : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->stage : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->quantization : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->block_axis : 0ull);
    (void)attention_hash_u64(hash, policy ? policy->block_width : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->scale_format : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->scale_dtype : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->pre_transform : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->tail_policy : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->nonfinite_policy : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->fake_quant_inplace : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->zero_pad_hadamard_to_power_of_two : 0ull);
}

static void attention_hash_sparse_topk_policy(
    yvex_sha256 *hash,
    const yvex_deepseek_v4_runtime_sparse_topk_policy *policy)
{
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->required : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->version : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->policy : 0ull);
    (void)attention_hash_u64(hash, policy ? policy->k : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->reject_nonfinite : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->score_descending : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->equal_score_ordinal_ascending : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->plus_zero_equals_minus_zero : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->duplicate_ordinal_refused : 0ull);
    (void)attention_hash_u64(hash, policy ? (unsigned long long)policy->output_ranked_order : 0ull);
}

/* Contract: verifies one runtime binding can be used by attention admission. */
static int attention_bind_role(
    const yvex_runtime_descriptor *descriptor,
    yvex_deepseek_attention_layer_plan *layer_plan,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *binding =
        yvex_runtime_descriptor_find_role(
            descriptor, role, scope, layer_index, YVEX_DEEPSEEK_GGUF_NO_INDEX);
    const yvex_quant_numeric_capability *capability;

    layer_plan->required_binding_count++;
    if (!binding || !binding->binding) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, binding,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention binding is missing from runtime descriptor");
    }
    if (!binding->binding->encoded_bytes || !binding->binding->rank) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, binding,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention binding has empty shape or byte range");
    }
    capability = yvex_quant_numeric_capability_at(binding->qtype);
    if (!capability || !capability->identity_known ||
        !capability->storage_admitted ||
        !capability->dedicated_cpu_compute_available) {
        layer_plan->qtype_compute_refusal_count++;
        return attention_reject(
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
    yvex_deepseek_attention_layer_plan *layer_plan,
    yvex_deepseek_attention_failure *failure,
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
    yvex_deepseek_attention_layer_plan *out,
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


int yvex_deepseek_attention_plan_build(
    yvex_deepseek_attention_plan **out,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    const yvex_deepseek_v4_model_spec *model;
    const yvex_materialization_summary *materialization;
    const yvex_runtime_descriptor_summary *runtime;
    yvex_deepseek_attention_plan *plan;
    unsigned long long layer_count;
    unsigned long long i;
    char logical_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    int rc;

    if (out) *out = NULL;
    if (!out || !ir || !session || !descriptor)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention plan requires IR, materialization session, and descriptor");
    model = yvex_deepseek_v4_ir_model(ir);
    runtime = yvex_runtime_descriptor_summary_get(descriptor);
    materialization = yvex_materialization_session_summary(session);
    if (!model || yvex_deepseek_v4_ir_layer_count(ir) == 0ull)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek architecture IR has no attention layers");
    if (!materialization || !materialization->committed ||
        materialization->status != YVEX_MATERIALIZATION_STATUS_COMMITTED)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MATERIALIZATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires committed materialization");
    if (!runtime || runtime->status != YVEX_RUNTIME_DESCRIPTOR_STATUS_READY)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires a ready runtime descriptor");
    if (!runtime->runtime_numeric_identity[0] ||
        runtime->runtime_numeric_schema_version == 0u)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires runtime numeric descriptor facts");
    if (!yvex_deepseek_transform_architecture_identity(ir, logical_identity) ||
        strcmp(logical_identity, runtime->logical_model_identity) != 0 ||
        strcmp(materialization->plan_identity,
               runtime->materialization_plan_identity) != 0)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention refused a stale runtime descriptor identity");
    layer_count = yvex_deepseek_v4_ir_layer_count(ir);
    plan = (yvex_deepseek_attention_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            sizeof(*plan), 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate DeepSeek attention plan");
    plan->layers = (yvex_deepseek_attention_layer_plan *)calloc(
        (size_t)layer_count, sizeof(*plan->layers));
    if (!plan->layers) {
        yvex_deepseek_attention_plan_close(plan);
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            layer_count, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate DeepSeek attention layer plans");
    }
    plan->layer_count = layer_count;
    plan->summary.status = YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_READY;
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
#ifdef YVEX_HAVE_CUDA_KERNEL_PTX
    plan->summary.cuda_execution_ready = 1;
#else
    plan->summary.cuda_execution_ready = 0;
#endif
    plan->summary.full_execution_ready = 1;

    for (i = 0ull; i < layer_count; ++i) {
        const yvex_deepseek_v4_layer_spec *layer =
            yvex_deepseek_v4_ir_layer_at(ir, i);
        if (!layer) {
            yvex_deepseek_attention_plan_close(plan);
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
                i, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
                YVEX_ERR_FORMAT, "DeepSeek attention layer is missing");
        }
        attention_fill_layer_plan(&plan->layers[i], layer);
        rc = attention_bind_required_layer_roles(
            descriptor, layer, &plan->layers[i], failure, err);
        if (rc != YVEX_OK) {
            yvex_deepseek_attention_plan_close(plan);
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

void yvex_deepseek_attention_plan_close(yvex_deepseek_attention_plan *plan)
{
    if (!plan) return;
    free(plan->layers);
    free(plan);
}

const yvex_deepseek_attention_summary *yvex_deepseek_attention_plan_summary(
    const yvex_deepseek_attention_plan *plan)
{
    return plan ? &plan->summary : NULL;
}

unsigned long long yvex_deepseek_attention_plan_layer_count(
    const yvex_deepseek_attention_plan *plan)
{
    return plan ? plan->layer_count : 0ull;
}

const yvex_deepseek_attention_layer_plan *yvex_deepseek_attention_plan_layer_at(
    const yvex_deepseek_attention_plan *plan,
    unsigned long long index)
{
    if (!plan || index >= plan->layer_count) return NULL;
    return &plan->layers[index];
}
