/*
 * yvex_generation_trace.c - diagnostic generation trace accounting.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   trace-section selection and counter accounting for generation reports.
 *
 * Does not own:
 *   trace rendering, CLI output, command grammar, runtime model support,
 *   provider generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   trace accounting is complete before renderers serialize reports; renderers
 *   must not mutate trace counters.
 *
 * Boundary:
 *   trace accounting is diagnostic evidence only and not generation support.
 */
#include "yvex_generation_trace.h"

#include <string.h>

int yvex_generation_trace_wants_tokens(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATION_TRACE_TOKENS ||
           level == YVEX_GENERATION_TRACE_FULL;
}

int yvex_generation_trace_wants_steps(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATION_TRACE_STEPS ||
           level == YVEX_GENERATION_TRACE_FULL;
}

int yvex_generation_trace_wants_kv(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATION_TRACE_KV ||
           level == YVEX_GENERATION_TRACE_FULL;
}

int yvex_generation_trace_wants_logits(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATION_TRACE_LOGITS ||
           level == YVEX_GENERATION_TRACE_FULL;
}

int yvex_generation_trace_wants_sampling(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATION_TRACE_SAMPLING ||
           level == YVEX_GENERATION_TRACE_FULL;
}

static unsigned long long generation_trace_attempted_steps(
    const yvex_generation_report *report)
{
    unsigned long long i;
    unsigned long long count = 0;

    if (!report) {
        return 0;
    }
    for (i = 0; i < report->trace_step_count; ++i) {
        if (report->trace_step_records[i].attempted) {
            count += 1ull;
        }
    }
    return count;
}

static void generation_trace_add(yvex_generation_report *report,
                                 unsigned long long *category,
                                 unsigned long long count)
{
    if (!report || count == 0ull) {
        return;
    }
    report->trace_records += count;
    if (category) {
        *category += count;
    }
}

/*
 * yvex_generation_trace_account()
 *
 * Purpose:
 *   derive trace section and total record counters from an immutable
 *   diagnostic generation report state before CLI rendering.
 *
 * Inputs:
 *   report is borrowed mutable accounting state already filled by the
 *   diagnostic generation loop.
 *
 * Effects:
 *   resets and fills trace counters/status only; it does not print, execute
 *   model work, load tensor payloads, or mutate runtime objects.
 *
 * Failure:
 *   no failure path; missing reports are ignored.
 *
 * Boundary:
 *   trace counters are diagnostic evidence only and not generation support.
 */
void yvex_generation_trace_account(yvex_generation_report *report)
{
    unsigned long long attempted_steps;
    int failed;

    if (!report) {
        return;
    }

    report->trace_records = 0ull;
    report->trace_tokens = 0ull;
    report->trace_steps = 0ull;
    report->trace_kv = 0ull;
    report->trace_logits = 0ull;
    report->trace_sampling = 0ull;
    report->trace_append = 0ull;
    report->trace_stop = 0ull;
    report->trace_cancel = 0ull;
    report->trace_cleanup = 0ull;
    report->trace_failures = 0ull;

    if (!report->trace_enabled) {
        report->trace_status = "disabled";
        return;
    }

    attempted_steps = generation_trace_attempted_steps(report);
    failed = report->phase && strcmp(report->phase, "failed") == 0;

    if (yvex_generation_trace_wants_tokens(report->trace_level)) {
        generation_trace_add(report, &report->trace_tokens, 7ull);
    }
    if (yvex_generation_trace_wants_steps(report->trace_level)) {
        generation_trace_add(report, &report->trace_steps,
                             attempted_steps * 11ull);
    }
    if (yvex_generation_trace_wants_kv(report->trace_level)) {
        generation_trace_add(report, &report->trace_kv,
                             report->trace_kv_requested ? 9ull : 4ull);
    }
    if (yvex_generation_trace_wants_logits(report->trace_level)) {
        generation_trace_add(report, &report->trace_logits,
                             attempted_steps * 5ull);
    }
    if (yvex_generation_trace_wants_sampling(report->trace_level)) {
        generation_trace_add(report, &report->trace_sampling,
                             attempted_steps * 6ull);
    }
    if (report->trace_level == YVEX_GENERATION_TRACE_FULL) {
        generation_trace_add(report, &report->trace_append,
                             attempted_steps * 7ull);
        generation_trace_add(report, &report->trace_stop, 12ull);
    }
    if (report->state.cancel_requested) {
        generation_trace_add(report, &report->trace_cancel, 6ull);
    }
    if (failed) {
        generation_trace_add(report, &report->trace_failures, 6ull);
    }
    if (report->trace_level == YVEX_GENERATION_TRACE_FULL || failed) {
        generation_trace_add(report, &report->trace_cleanup, 2ull);
    }

    report->trace_status =
        report->trace_records > 0ull ? "emitted" : "enabled";
}
