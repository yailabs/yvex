/*
 * yvex_deepseek_attention.c - DeepSeek-V4 attention public facade.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   public status/failure names and the current hard execution-support gate.
 *
 * Does not own:
 *   plan construction, rolling compressor state, transactional sink storage,
 *   numerical execution, CUDA kernels, persistent KV, generation, CLI output,
 *   or release claims.
 *
 * Invariants:
 *   the execution gate covers only the complete attention equation; it never
 *   promotes persistent KV, transformer, prefill, decode, or generation.
 *
 * Boundary:
 *   scoped attention execution support is not persistent KV, transformer, or
 *   runtime-generation capability.
 */
#include "yvex_deepseek_attention_internal.h"

const char *yvex_deepseek_attention_status_name(
    yvex_deepseek_attention_status status)
{
    switch (status) {
    case YVEX_DEEPSEEK_ATTENTION_STATUS_REFUSED: return "refused";
    case YVEX_DEEPSEEK_ATTENTION_STATUS_PLANNED: return "planned";
    case YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_UNSUPPORTED:
        return "execution-unsupported";
    case YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_READY:
        return "execution-ready";
    default: return "unknown";
    }
}

const char *yvex_deepseek_attention_failure_name(
    yvex_deepseek_attention_failure_code code)
{
    switch (code) {
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_NONE: return "none";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT:
        return "invalid-argument";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE: return "architecture";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_MATERIALIZATION:
        return "materialization";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR: return "descriptor";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING:
        return "missing-binding";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_QTYPE: return "qtype";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION: return "dimension";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY: return "history";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_EXECUTION_UNSUPPORTED:
        return "execution-unsupported";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_READ: return "read";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC: return "numeric";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA:
        return "state-delta";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION: return "allocation";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND: return "backend";
    default: return "unknown";
    }
}

/* Contract: exposes the hard execution gate without allocating or executing. */
int yvex_deepseek_attention_execute_supported(const char **reason)
{
    if (reason) *reason = NULL;
    return 1;
}
