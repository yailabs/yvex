/* Owner: src/generation
 * Owns: trace-section selection and counter accounting for generation reports.
 * Does not own: trace rendering, CLI output, command grammar, runtime model support, provider generation, eval,
 *   benchmark, or release decisions.
 * Invariants: trace accounting is complete before renderers serialize reports; renderers must not mutate trace
 *   counters.
 * Boundary: trace accounting is diagnostic evidence only and not generation support.
 * Purpose: Account deterministic diagnostic generation trace sections and counters.
 * Inputs: A trace level, section, and caller-owned trace summary.
 * Effects: Mutates only explicit trace counters.
 * Failure: Invalid levels or sections remain typed unknown values without hidden state. */
#include <yvex/internal/generation.h>

#include <string.h>

typedef struct {
    yvex_generation_trace_level level;
    size_t counter_offset;
    unsigned long long fixed_count;
    unsigned long long step_multiplier;
} trace_rule;

#define TRACE_RULE(level_, field_, fixed_, multiplier_)                                 \
    {level_, offsetof(yvex_generation_report, field_), fixed_, multiplier_}

static const trace_rule generation_trace_rules[] = {
    TRACE_RULE(YVEX_GENERATION_TRACE_TOKENS, trace_tokens, 7ull, 0ull),
    TRACE_RULE(YVEX_GENERATION_TRACE_STEPS, trace_steps, 0ull, 11ull),
    TRACE_RULE(YVEX_GENERATION_TRACE_LOGITS, trace_logits, 0ull, 5ull),
    TRACE_RULE(YVEX_GENERATION_TRACE_SAMPLING, trace_sampling, 0ull, 6ull),
};

#undef TRACE_RULE

/* Purpose: test one trace section against the exact section-or-full admission rule. */
static int generation_trace_wants(yvex_generation_trace_level level,
                                  yvex_generation_trace_level section)
{
    return level == section || level == YVEX_GENERATION_TRACE_FULL;
}

/* Purpose: Implement the canonical trace wants tokens mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_generation_trace_wants_tokens(yvex_generation_trace_level level)
{
    return generation_trace_wants(level, YVEX_GENERATION_TRACE_TOKENS);
}

/* Purpose: Implement the canonical trace wants steps mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_generation_trace_wants_steps(yvex_generation_trace_level level)
{
    return generation_trace_wants(level, YVEX_GENERATION_TRACE_STEPS);
}

/* Purpose: Implement the canonical trace wants kv mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_generation_trace_wants_kv(yvex_generation_trace_level level)
{
    return generation_trace_wants(level, YVEX_GENERATION_TRACE_KV);
}

/* Purpose: Implement the canonical trace wants logits mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_generation_trace_wants_logits(yvex_generation_trace_level level)
{
    return generation_trace_wants(level, YVEX_GENERATION_TRACE_LOGITS);
}

/* Purpose: Implement the canonical trace wants sampling mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_generation_trace_wants_sampling(yvex_generation_trace_level level)
{
    return generation_trace_wants(level, YVEX_GENERATION_TRACE_SAMPLING);
}

/* Purpose: Implement the canonical trace attempted steps mechanism owned by the generation boundary. */
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

/* Purpose: Implement the canonical trace add mechanism owned by the generation boundary. */
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

/* Purpose: derive trace section and total record counters from an immutable diagnostic
 * generation report state before CLI rendering.
 * Inputs: report is borrowed mutable accounting state already filled by the diagnostic generation loop.
 * Effects: resets and fills trace counters/status only; it does not print, execute model
 * work, load tensor payloads, or mutate runtime objects.
 * Failure: no failure path; missing reports are ignored.
 * Boundary: trace counters are diagnostic evidence only and not generation support. */
void yvex_generation_trace_account(yvex_generation_report *report)
{
    unsigned long long attempted_steps;
    size_t rule_index;
    int failed;

    if (!report) {
        return;
    }

    memset(&report->trace_records, 0,
           offsetof(yvex_generation_report, trace_step_records) -
               offsetof(yvex_generation_report, trace_records));

    if (!report->trace_enabled) {
        report->trace_status = "disabled";
        return;
    }

    attempted_steps = generation_trace_attempted_steps(report);
    failed = report->phase && strcmp(report->phase, "failed") == 0;

    for (rule_index = 0;
         rule_index < sizeof(generation_trace_rules) / sizeof(generation_trace_rules[0]);
         ++rule_index) {
        const trace_rule *rule = &generation_trace_rules[rule_index];
        unsigned long long count = rule->fixed_count + attempted_steps * rule->step_multiplier;
        unsigned long long *counter =
            (unsigned long long *)((unsigned char *)report + rule->counter_offset);

        if (generation_trace_wants(report->trace_level, rule->level)) {
            generation_trace_add(report, counter, count);
        }
    }
    if (yvex_generation_trace_wants_kv(report->trace_level)) {
        generation_trace_add(report, &report->trace_kv,
                             report->trace_kv_requested ? 9ull : 4ull);
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
