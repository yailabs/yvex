/*
 * yvex_generation_report.c - diagnostic generation report helpers.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   small report helper APIs for diagnostic generation facts.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, graph execution, provider
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   helper names preserve the lowest true diagnostic stage and never imply
 *   full-model generation readiness.
 *
 * Boundary:
 *   report helper APIs are not runtime generation support.
 */
#include "yvex_generation_report.h"

const char *yvex_generation_trace_level_name(yvex_generation_trace_level level)
{
    switch (level) {
    case YVEX_GENERATION_TRACE_TOKENS:
        return "tokens";
    case YVEX_GENERATION_TRACE_STEPS:
        return "steps";
    case YVEX_GENERATION_TRACE_KV:
        return "kv";
    case YVEX_GENERATION_TRACE_LOGITS:
        return "logits";
    case YVEX_GENERATION_TRACE_SAMPLING:
        return "sampling";
    case YVEX_GENERATION_TRACE_FULL:
        return "full";
    case YVEX_GENERATION_TRACE_NONE:
    default:
        return "none";
    }
}
