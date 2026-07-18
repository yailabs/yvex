/*
 * yvex_deepseek_attention_internal.c - DeepSeek-V4 attention shared-private helper owner.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   typed refusal construction, SHA-256 identity primitives, checked arithmetic, and bounded allocation helpers used by the attention modules.
 *
 * Does not own:
 *   source parsing, GGUF mapping, materialization ownership, persistent KV,
 *   prefill, decode, logits, sampling, generation, eval, benchmark, release
 *   claims, CLI parsing, or rendering.
 *
 * Invariants:
 *   helpers publish no capability state and never own CLI or persistent runtime storage.
 *
 * Boundary:
 *   shared helpers do not execute attention.
 */
#include "yvex_deepseek_attention_internal.h"

#include "src/model/compilation/yvex_deepseek_transform_ir.h"

static void attention_failure_set(
    yvex_deepseek_attention_failure *failure,
    yvex_deepseek_attention_failure_code code,
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

/* Contract: records one typed refusal and leaves caller-owned outputs unset. */
int attention_reject(yvex_deepseek_attention_failure *failure,
                            yvex_deepseek_attention_failure_code code,
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

int attention_hash_u64(yvex_sha256 *hash, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;

    for (i = 0u; i < 8u; ++i)
        bytes[7u - i] = (unsigned char)((value >> (i * 8u)) & 0xffu);
    return yvex_sha256_update(hash, bytes, sizeof(bytes));
}

int attention_hash_text(yvex_sha256 *hash, const char *text)
{
    return yvex_sha256_update(hash, text ? text : "", text ? strlen(text) : 0u);
}


int attention_checked_mul_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out)
{
    if (!out || (left != 0ull && right > ULLONG_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

/* Contract: adds two unsigned geometry values and refuses overflow. */
int attention_checked_add_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out)
{
    if (!out || left > ULLONG_MAX - right) return 0;
    *out = left + right;
    return 1;
}

int attention_checked_size(unsigned long long count,
                                  unsigned long long width,
                                  size_t *out)
{
    unsigned long long bytes;

    if (!out || !attention_checked_mul_u64(count, width, &bytes) ||
        bytes > (unsigned long long)SIZE_MAX)
        return 0;
    *out = (size_t)bytes;
    return 1;
}

unsigned long long attention_min_u64(unsigned long long a,
                                            unsigned long long b)
{
    return a < b ? a : b;
}

void *attention_calloc_array(unsigned long long count,
                                    unsigned long long width)
{
    size_t bytes;

    if (!attention_checked_size(count, width, &bytes)) return NULL;
    return calloc(1u, bytes);
}

/* Contract: binds one execution call to the exact sealed logical, runtime,
 * materialization, and attention-plan identities before any payload read. */
int attention_execution_context_validate(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    const yvex_deepseek_attention_summary *attention;
    const yvex_runtime_descriptor_summary *runtime;
    const yvex_materialization_summary *materialization;
    char logical_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];

    if (!plan || !ir || !session || !descriptor)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "attention execution identity validation requires all owners");
    attention = yvex_deepseek_attention_plan_summary(plan);
    runtime = yvex_runtime_descriptor_summary_get(descriptor);
    materialization = yvex_materialization_session_summary(session);
    if (!attention || !runtime || !materialization ||
        !materialization->committed ||
        !yvex_deepseek_transform_architecture_identity(ir, logical_identity))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
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
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "attention execution refused a stale or mismatched identity chain");
    return YVEX_OK;
}
