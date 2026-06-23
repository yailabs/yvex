/*
 * YVEX - Graph operation names
 *
 * File: yvex_op.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Implements stable names for graph value kinds, operation kinds, operation
 *   statuses, and residency labels. These names are used by graph dumps,
 *   planner dumps, CLI smoke tests, and direct graph tests.
 *
 * Implements:
 *   - yvex_op_kind_name
 *   - yvex_op_status_name
 *   - yvex_value_kind_name
 *   - yvex_residency_name
 *
 * Invariants:
 *   - helpers allocate no memory
 *   - unknown enum values return explicit fallback strings
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 */
#include <yvex/op.h>

const char *yvex_op_kind_name(yvex_op_kind kind)
{
    switch (kind) {
    case YVEX_OP_EMBED: return "embed";
    case YVEX_OP_RMS_NORM: return "rms_norm";
    case YVEX_OP_MATMUL: return "matmul";
    case YVEX_OP_ROPE: return "rope";
    case YVEX_OP_ATTENTION_PREFILL: return "attention_prefill";
    case YVEX_OP_ATTENTION_DECODE: return "attention_decode";
    case YVEX_OP_KV_WRITE: return "kv_write";
    case YVEX_OP_KV_READ: return "kv_read";
    case YVEX_OP_SWIGLU: return "swiglu";
    case YVEX_OP_RESIDUAL_ADD: return "residual_add";
    case YVEX_OP_LOGITS: return "logits";
    case YVEX_OP_SAMPLER: return "sampler";
    case YVEX_OP_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

const char *yvex_op_status_name(yvex_op_status status)
{
    switch (status) {
    case YVEX_OP_STATUS_PLANNED: return "planned";
    case YVEX_OP_STATUS_MISSING_INPUT: return "missing_input";
    case YVEX_OP_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_OP_STATUS_INVALID_SHAPE: return "invalid_shape";
    }
    return "unknown";
}

const char *yvex_value_kind_name(yvex_value_kind kind)
{
    switch (kind) {
    case YVEX_VALUE_TOKEN_IDS: return "token_ids";
    case YVEX_VALUE_ACTIVATION: return "activation";
    case YVEX_VALUE_WEIGHT: return "weight";
    case YVEX_VALUE_KV_CACHE: return "kv_cache";
    case YVEX_VALUE_LOGITS: return "logits";
    case YVEX_VALUE_TEMPORARY: return "temporary";
    case YVEX_VALUE_UNKNOWN: return "unknown";
    }
    return "unknown";
}

const char *yvex_residency_name(yvex_residency residency)
{
    switch (residency) {
    case YVEX_RESIDENCY_HOST: return "host";
    case YVEX_RESIDENCY_DEVICE: return "device";
    case YVEX_RESIDENCY_BACKEND_DECIDES: return "backend_decides";
    }
    return "unknown";
}
