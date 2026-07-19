/* Owner: graph and attention planning.
 * Owns: graph plans, memory estimates, attention plan validation, canonical ordering, and identity.
 * Does not own: payload reads, family execution, reports, backend kernels, persistent KV, or generation.
 * Invariants: planning is deterministic, immutable after build, and reads zero payload bytes.
 * Boundary: plan and capability facts are not graph execution or runtime support.
 * Purpose: derive bounded executable requirements from admitted model and runtime facts.
 * Inputs: immutable model IR, tensor tables, materialization sessions, and runtime descriptors.
 * Effects: allocates independently owned plan and summary state only.
 * Failure: checked validation or allocation failure publishes no partial plan or identity. */
#include "src/graph/private.h"

#include <stdlib.h>
#include <string.h>

// Purpose: Implement the graph-local failure set semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void attention_failure_set(
    yvex_attention_failure *failure,
    yvex_attention_failure_code code,
    const yvex_runtime_tensor_binding *binding,
    unsigned long long layer_index,
    yvex_tensor_role role,
    unsigned long long expected,
    unsigned long long actual,
    const char *reason)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->code = code;
    failure->layer_index = layer_index;
    failure->role = role;
    failure->expected = expected;
    failure->actual = actual;
    failure->reason = reason;
    if (binding && binding->binding)
        (void)snprintf(failure->tensor_name, sizeof(failure->tensor_name),
                       "%s", binding->binding->name);
}

// Purpose: Return the admitted reject fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_reject(yvex_attention_failure *failure,
                            yvex_attention_failure_code code,
                            const yvex_runtime_tensor_binding *binding,
                            unsigned long long layer_index,
                            yvex_tensor_role role,
                            unsigned long long expected,
                            unsigned long long actual,
                            yvex_error *err,
                            yvex_status err_code,
                            const char *reason)
{
    attention_failure_set(failure, code, binding, layer_index, role, expected,
                          actual, reason);
    yvex_error_set(err, err_code, "yvex_deepseek_attention", reason);
    return err_code;
}

// Purpose: Return the admitted hash u64 fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_hash_u64(yvex_sha256 *hash, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;

    for (i = 0u; i < 8u; ++i)
        bytes[7u - i] = (unsigned char)((value >> (i * 8u)) & 0xffu);
    return yvex_sha256_update(hash, bytes, sizeof(bytes));
}

// Purpose: Return the admitted hash text fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_hash_text(yvex_sha256 *hash, const char *text)
{
    return yvex_sha256_update(hash, text ? text : "", text ? strlen(text) : 0u);
}

// Purpose: Return the admitted checked size fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_checked_size(unsigned long long count,
                                  unsigned long long width,
                                  size_t *out)
{
    unsigned long long bytes;

    if (!out || !yvex_core_u64_mul(count, width, &bytes) ||
        bytes > (unsigned long long)SIZE_MAX)
        return 0;
    *out = (size_t)bytes;
    return 1;
}

// Purpose: Return the admitted min u64 fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_attention_min_u64(unsigned long long a,
                                            unsigned long long b)
{
    return a < b ? a : b;
}

// Purpose: Return the admitted calloc array fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void *yvex_attention_calloc_array(unsigned long long count,
                                    unsigned long long width)
{
    size_t bytes;

    if (!yvex_attention_checked_size(count, width, &bytes)) return NULL;
    return calloc(1u, bytes);
}

// Purpose: Return the admitted context validate fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_context_validate(
    const yvex_attention_plan *plan,
    const char *logical_identity,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_attention_summary *attention;
    const yvex_runtime_descriptor_summary *runtime;
    const yvex_materialization_summary *materialization;

    if (!plan || !logical_identity || !logical_identity[0] || !session ||
        !descriptor)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "attention execution identity validation requires all owners");
    attention = yvex_attention_plan_summary(plan);
    runtime = yvex_runtime_descriptor_summary_get(descriptor);
    materialization = yvex_materialization_session_summary(session);
    if (!attention || !runtime || !materialization ||
        !materialization->committed)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "attention execution requires sealed identity-bearing owners");
    if (strcmp(logical_identity, runtime->logical_model_identity) != 0 ||
        strcmp(logical_identity, attention->logical_model_identity) != 0 ||
        strcmp(runtime->runtime_numeric_identity,
               attention->runtime_numeric_identity) != 0 ||
        strcmp(runtime->runtime_descriptor_identity,
               attention->runtime_descriptor_identity) != 0 ||
        strcmp(materialization->plan_identity,
               runtime->materialization_plan_identity) != 0 ||
        strcmp(materialization->plan_identity,
               attention->materialization_plan_identity) != 0)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "attention execution refused a stale or mismatched identity chain");
    return YVEX_OK;
}
static void plan_hash_activation_policy(
    yvex_sha256 *hash,
    const yvex_attention_activation_policy *policy);
static void plan_hash_sparse_topk_policy(
    yvex_sha256 *hash,
    const yvex_attention_topk_policy *policy);
void yvex_attention_plan_close(yvex_attention_plan *plan);

// Purpose: Derive the deterministic compute identity identity from explicit semantic fields.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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

// Purpose: Derive the deterministic plan hash activation policy identity from explicit semantic fields.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void plan_hash_activation_policy(
    yvex_sha256 *hash,
    const yvex_attention_activation_policy *policy)
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

// Purpose: Derive the deterministic plan hash sparse topk policy identity from explicit semantic fields.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void plan_hash_sparse_topk_policy(
    yvex_sha256 *hash,
    const yvex_attention_topk_policy *policy)
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

// Purpose: Implement the graph-local bind role semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

static int attention_bind_role(
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_layer_plan *layer_plan,
    yvex_tensor_scope scope,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *binding =
        yvex_runtime_descriptor_find_role(
            descriptor, role, scope, layer_index, YVEX_ATTENTION_NO_TENSOR_INDEX);
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

// Purpose: Implement the graph-local bind required layer roles semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_bind_required_layer_roles(
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_layer_plan *layer,
    yvex_attention_layer_plan *layer_plan,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_tensor_scope scope = YVEX_TENSOR_SCOPE_MAIN_LAYER;
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

// Purpose: Return the admitted plan build fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_plan_build(
    yvex_attention_plan **out,
    const yvex_attention_recipe *recipe,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialization_summary *materialization;
    const yvex_runtime_descriptor_summary *runtime;
    yvex_attention_plan *plan;
    unsigned long long layer_count;
    unsigned long long i;
    char logical_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    int rc;

    if (out) *out = NULL;
    if (!out || !recipe || !recipe->context || !recipe->identity ||
        !recipe->layer || !recipe->layer_count || !session || !descriptor)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention plan requires IR, materialization session, and descriptor");
    runtime = yvex_runtime_descriptor_summary_get(descriptor);
    materialization = yvex_materialization_session_summary(session);
    if (!materialization || !materialization->committed ||
        materialization->status != YVEX_MATERIALIZATION_STATUS_COMMITTED)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MATERIALIZATION, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires committed materialization");
    if (!runtime || runtime->status != YVEX_RUNTIME_DESCRIPTOR_STATUS_READY)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires a ready runtime descriptor");
    if (!runtime->runtime_numeric_identity[0] ||
        runtime->runtime_numeric_schema_version == 0u)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention requires runtime numeric descriptor facts");
    if (!recipe->identity(recipe->context, logical_identity) ||
        strcmp(logical_identity, runtime->logical_model_identity) != 0 ||
        strcmp(materialization->plan_identity,
               runtime->materialization_plan_identity) != 0)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "DeepSeek attention refused a stale runtime descriptor identity");
    layer_count = recipe->layer_count;
    plan = (yvex_attention_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            sizeof(*plan), 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate DeepSeek attention plan");
    plan->layers = (yvex_attention_layer_plan *)calloc(
        (size_t)layer_count, sizeof(*plan->layers));
    if (!plan->layers) {
        yvex_attention_plan_close(plan);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
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
    plan->summary.auxiliary_layer_count = recipe->auxiliary_layer_count;
    plan->summary.swa_layer_count = recipe->swa_layer_count;
    plan->summary.csa_layer_count = recipe->csa_layer_count;
    plan->summary.hca_layer_count = recipe->hca_layer_count;
    plan->summary.history_contract_ready = 1;
    plan->summary.state_delta_contract_ready = 1;
    plan->summary.cpu_reference_ready = 1;
    plan->summary.cuda_execution_ready = 0;
    plan->summary.full_execution_ready = 0;

    for (i = 0ull; i < layer_count; ++i) {
        yvex_attention_layer_plan *layer = &plan->layers[i];
        if (!recipe->layer(recipe->context, i, layer)) {
            yvex_attention_plan_close(plan);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
                i, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
                YVEX_ERR_FORMAT, "DeepSeek attention layer is missing");
        }
        rc = attention_bind_required_layer_roles(
            descriptor, layer, layer, failure, err);
        if (rc != YVEX_OK) {
            yvex_attention_plan_close(plan);
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

// Purpose: Return the admitted plan close fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_plan_close(yvex_attention_plan *plan)
{
    if (!plan) return;
    free(plan->layers);
    free(plan);
}

// Purpose: Return the admitted plan summary fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_attention_summary *yvex_attention_plan_summary(
    const yvex_attention_plan *plan)
{
    return plan ? &plan->summary : NULL;
}

// Purpose: Return the admitted plan layer count fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_attention_plan_layer_count(
    const yvex_attention_plan *plan)
{
    return plan ? plan->layer_count : 0ull;
}

// Purpose: Return the admitted plan layer at fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_attention_layer_plan *yvex_attention_plan_layer_at(
    const yvex_attention_plan *plan,
    unsigned long long index)
{
    if (!plan || index >= plan->layer_count) return NULL;
    return &plan->layers[index];
}

// Purpose: Implement the graph-local backend allowed semantic operation.
static int backend_allowed(const char *name)
{
    return strcmp(name, "cpu") == 0 || strcmp(name, "none") == 0 || strcmp(name, "cuda") == 0;
}

// Purpose: Project the stable textual ABI label for backend kind from name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
yvex_backend_kind yvex_graph_backend_kind_from_name(const char *name)
{
    return strcmp(name, "cuda") == 0 ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU;
}

// Purpose: Implement the graph-local plan variant supported semantic operation.

static int plan_variant_supported(const yvex_backend *backend,
                                  yvex_backend_operation_variant variant)
{
    yvex_backend_capability_result result;
    yvex_error err;

    yvex_error_clear(&err);
    return yvex_backend_query_capability(backend, variant, &result, &err) ==
               YVEX_OK &&
           result.state == YVEX_BACKEND_CAPABILITY_SUPPORTED;
}

// Purpose: Implement the graph-local fill backend status semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int fill_backend_status(yvex_plan *plan, const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options backend_options;
    int rc;

    if (strcmp(backend_name, "cpu") == 0 || strcmp(backend_name, "cuda") == 0) {
        memset(&backend_options, 0, sizeof(backend_options));
        backend_options.kind = yvex_graph_backend_kind_from_name(backend_name);
        rc = yvex_backend_open(&backend, &backend_options, err);
        if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
            plan->backend_status = yvex_core_strdup("unavailable");
            yvex_error_clear(err);
            return plan->backend_status ? YVEX_OK : YVEX_ERR_NOMEM;
        }
        if (rc != YVEX_OK) {
            return rc;
        }
        plan->backend_status = yvex_core_strdup(
            yvex_backend_status_of(backend) == YVEX_BACKEND_STATUS_CONTEXT_READY
                ? "context-available" : "available");
        plan->backend_tensor_alloc =
            plan_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_ALLOC) &&
            plan_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_ZERO);
        plan->backend_tensor_read_write =
            plan_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_WRITE) &&
            plan_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_READ) &&
            plan_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_COPY);
        plan->backend_op_embed = plan_variant_supported(
            backend, YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32);
        plan->backend_op_matmul = plan_variant_supported(
            backend, YVEX_BACKEND_VARIANT_MATMUL_F32);
        plan->backend_op_mlp = plan_variant_supported(
            backend, YVEX_BACKEND_VARIANT_MLP_DENSE_F32);
        plan->backend_op_rms_norm = plan_variant_supported(
            backend, YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32);
        plan->backend_op_rope = plan_variant_supported(
            backend, YVEX_BACKEND_VARIANT_ROPE_F32);
        plan->backend_op_attention = plan_variant_supported(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32);
        yvex_backend_close(backend);
    } else {
        plan->backend_status = yvex_core_strdup("not-selected");
    }

    if (!plan->backend_status) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to copy backend status");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

// Purpose: Construct plan create with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_plan_create(yvex_plan **out,
                     const yvex_model_descriptor *model,
                     const yvex_tensor_table *tensors,
                     const yvex_plan_options *options,
                     yvex_error *err)
{
    yvex_plan *plan;
    yvex_graph_build_options graph_options;
    const char *backend_name = "cpu";
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_create", "model and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&graph_options, 0, sizeof(graph_options));
    graph_options.sequence_length = 1;
    graph_options.context_length = yvex_model_context_length(model) > 0
                                       ? yvex_model_context_length(model)
                                       : 1;
    graph_options.include_prefill_path = 1;

    if (options) {
        if (options->sequence_length > 0) {
            graph_options.sequence_length = options->sequence_length;
        }
        if (options->context_length > 0) {
            graph_options.context_length = options->context_length;
        }
        if (options->backend_name) {
            backend_name = options->backend_name;
        }
    }

    if (!backend_allowed(backend_name)) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_plan_create",
                        "backend label unsupported in graph planner: %s", backend_name);
        return YVEX_ERR_UNSUPPORTED;
    }

    plan = (yvex_plan *)calloc(1, sizeof(*plan));
    if (!plan) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to allocate plan");
        return YVEX_ERR_NOMEM;
    }
    plan->backend_name = yvex_core_strdup(backend_name);
    if (!plan->backend_name) {
        yvex_plan_close(plan);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to copy backend label");
        return YVEX_ERR_NOMEM;
    }
    rc = fill_backend_status(plan, backend_name, err);
    if (rc != YVEX_OK) {
        yvex_plan_close(plan);
        return rc;
    }

    rc = yvex_graph_build_for_model(&plan->graph, model, tensors, &graph_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_memory_plan_from_graph(&plan->memory, plan->graph, tensors, err);
    }
    if (rc != YVEX_OK) {
        yvex_plan_close(plan);
        return rc;
    }

    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

// Purpose: Release graph-owned resources held by plan close.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_plan_close(yvex_plan *plan)
{
    if (!plan) {
        return;
    }
    free(plan->backend_name);
    free(plan->backend_status);
    yvex_memory_plan_close(plan->memory);
    yvex_graph_close(plan->graph);
    free(plan);
}

// Purpose: Implement the graph-local plan graph semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_graph *yvex_plan_graph(const yvex_plan *plan)
{
    return plan ? plan->graph : NULL;
}

// Purpose: Implement the graph-local plan memory semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_memory_plan *yvex_plan_memory(const yvex_plan *plan)
{
    return plan ? plan->memory : NULL;
}

// Purpose: Apply the checked graph-local add checked invariant.
static int add_checked(unsigned long long a,
                       unsigned long long b,
                       unsigned long long *out,
                       yvex_error *err)
{
    if (a > ULLONG_MAX - b) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_memory_plan_from_graph", "memory byte total overflow");
        return YVEX_ERR_BOUNDS;
    }
    *out = a + b;
    return YVEX_OK;
}

// Purpose: Implement the graph-local compute activation peak semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int compute_activation_peak(const yvex_graph *graph,
                                   unsigned long long *out,
                                   yvex_error *err)
{
    unsigned long long peak = 0;
    unsigned long long i;

    for (i = 0; i < yvex_graph_value_count(graph); ++i) {
        const yvex_graph_value_info *value = yvex_graph_value_at(graph, i);
        unsigned long long bytes;
        int rc;

        if (!value || value->kind != YVEX_VALUE_ACTIVATION) {
            continue;
        }
        rc = yvex_dtype_tensor_storage_bytes(value->dtype,
                                             value->dims,
                                             value->rank,
                                             &bytes,
                                             err);
        if (rc == YVEX_ERR_UNSUPPORTED) {
            yvex_error_clear(err);
            continue;
        }
        if (rc != YVEX_OK) {
            return rc;
        }
        if (bytes > peak) {
            peak = bytes;
        }
    }

    *out = peak;
    return YVEX_OK;
}

// Purpose: Implement the graph-local memory plan from graph semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_memory_plan_from_graph(yvex_memory_plan **out,
                                const yvex_graph *graph,
                                const yvex_tensor_table *tensors,
                                yvex_error *err)
{
    yvex_memory_plan *plan;
    unsigned long long i;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_from_graph", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!graph || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_from_graph",
                       "graph and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    plan = (yvex_memory_plan *)calloc(1, sizeof(*plan));
    if (!plan) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_memory_plan_from_graph",
                       "failed to allocate memory plan");
        return YVEX_ERR_NOMEM;
    }

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        if (!tensor) {
            continue;
        }
        if (tensor->storage_bytes == 0) {
            plan->summary.model_tensor_bytes_unknown_count += 1u;
        } else {
            rc = add_checked(plan->summary.model_tensor_bytes_known,
                             tensor->storage_bytes,
                             &plan->summary.model_tensor_bytes_known,
                             err);
            if (rc != YVEX_OK) {
                yvex_memory_plan_close(plan);
                return rc;
            }
        }
    }

    rc = compute_activation_peak(graph, &plan->summary.activation_peak_bytes, err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }

    rc = add_checked(plan->summary.model_tensor_bytes_known,
                     plan->summary.activation_peak_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }
    rc = add_checked(plan->summary.total_known_bytes,
                     plan->summary.kv_cache_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }
    rc = add_checked(plan->summary.total_known_bytes,
                     plan->summary.scratch_peak_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }

    switch (yvex_graph_status_of(graph)) {
    case YVEX_GRAPH_STATUS_BUILT:
        plan->status = YVEX_MEMORY_PLAN_ESTIMATED;
        break;
    case YVEX_GRAPH_STATUS_PARTIAL:
        plan->status = YVEX_MEMORY_PLAN_PARTIAL;
        break;
    case YVEX_GRAPH_STATUS_UNSUPPORTED:
    case YVEX_GRAPH_STATUS_INVALID:
        plan->status = YVEX_MEMORY_PLAN_UNSUPPORTED;
        break;
    case YVEX_GRAPH_STATUS_EMPTY:
    default:
        plan->status = YVEX_MEMORY_PLAN_EMPTY;
        break;
    }

    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

// Purpose: Release graph-owned resources held by memory plan close.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_memory_plan_close(yvex_memory_plan *plan)
{
    free(plan);
}

// Purpose: Return the admitted memory plan status of fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
yvex_memory_plan_status yvex_memory_plan_status_of(const yvex_memory_plan *plan)
{
    return plan ? plan->status : YVEX_MEMORY_PLAN_EMPTY;
}

// Purpose: Project the stable textual ABI label for memory plan status name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const char *yvex_memory_plan_status_name(yvex_memory_plan_status status)
{
    switch (status) {
    case YVEX_MEMORY_PLAN_EMPTY: return "empty";
    case YVEX_MEMORY_PLAN_ESTIMATED: return "estimated";
    case YVEX_MEMORY_PLAN_PARTIAL: return "partial";
    case YVEX_MEMORY_PLAN_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

// Purpose: Return the admitted memory plan get summary fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_memory_plan_get_summary(const yvex_memory_plan *plan,
                                 yvex_memory_plan_summary *out,
                                 yvex_error *err)
{
    if (!plan || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_get_summary",
                       "plan and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = plan->summary;
    yvex_error_clear(err);
    return YVEX_OK;
}
