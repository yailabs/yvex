/* Owner: src/cli/render
 * Owns: trace text serialization for typed generation reports.
 * Does not own: trace accounting, generation loop execution, argv parsing, command dispatch, eval, benchmark, or
 *   release decisions.
 * Invariants: trace rendering is read-only over reports and uses src/cli/io writers.
 * Boundary: trace rendering is diagnostic evidence only and not generation support.
 * Purpose: provide trace text serialization for typed generation reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"
#include <yvex/internal/generation.h>

#include <string.h>

/* Purpose: Compute generate trace token list for its CLI invariant (`generate_trace_token_list`). */
static void generate_trace_token_list(FILE *fp,
                                      const char *label,
                                      const unsigned int *tokens,
                                      unsigned long long count)
{
    yvex_cli_out_token_list(fp, label, tokens, count);
}

/* Purpose: Compute generate trace runtime sequence for its CLI invariant (`generate_trace_runtime_sequence`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_trace_runtime_sequence(
    FILE *fp,
    const yvex_generation_report *report)
{
    unsigned long long i;
    unsigned long long emitted = 0ull;

    yvex_cli_out_writef(fp, "trace.tokens.runtime_sequence: ");
    if (report) {
        for (i = 0ull;
             i < report->prompt_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS;
             ++i) {
            yvex_cli_out_writef(fp, "%s%u",
                                emitted > 0ull ? "," : "",
                                report->prompt_tokens[i]);
            emitted += 1ull;
        }
        for (i = 0ull;
             i < report->generated_token_count &&
                 i < YVEX_TOKEN_INPUT_MAX_TOKENS;
             ++i) {
            yvex_cli_out_writef(fp, "%s%u",
                                emitted > 0ull ? "," : "",
                                report->generated_tokens[i]);
            emitted += 1ull;
        }
    }
    yvex_cli_out_char(fp, '\n');
}

/* Purpose: Compute generate emit token trace for its CLI invariant (`generate_emit_token_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_token_trace(FILE *fp,
                                      const yvex_generation_report *report)
{
    if (!report) {
        return;
    }
    generate_trace_token_list(fp, "trace.tokens.prompt",
                              report->prompt_tokens,
                              report->prompt_token_count);
    generate_trace_token_list(fp, "trace.tokens.generated",
                              report->generated_tokens,
                              report->generated_token_count);
    generate_trace_runtime_sequence(fp, report);
    yvex_cli_out_kv_u64(fp, "trace.tokens.prompt_count",
                        report->prompt_token_count);
    yvex_cli_out_kv_u64(fp, "trace.tokens.generated_count",
                        report->generated_token_count);
    yvex_cli_out_kv_u64(fp, "trace.tokens.total_count",
                        report->total_token_count);
    yvex_cli_out_kv_str(fp, "trace.tokens.stop_reason",
                        report->stop_reason ? report->stop_reason : "none");
}

/* Purpose: Compute generate emit step trace for its CLI invariant (`generate_emit_step_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_step_trace(FILE *fp,
                                     const yvex_generation_report *report)
{
    unsigned long long i;

    if (!report) {
        return;
    }
    for (i = 0ull; i < report->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &report->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        yvex_cli_out_writef(fp, "trace.step.%llu.index: %llu\n",
                            i, record->index);
        yvex_cli_out_writef(fp, "trace.step.%llu.decode_position: %llu\n",
                            i, record->decode_position);
        yvex_cli_out_writef(fp, "trace.step.%llu.decode_status: %s\n",
                            i, record->decode_status ? record->decode_status : "skipped");
        yvex_cli_out_writef(fp, "trace.step.%llu.logits_status: %s\n",
                            i, record->logits_status ? record->logits_status : "skipped");
        yvex_cli_out_writef(fp, "trace.step.%llu.sample_status: %s\n",
                            i, record->sample_status ? record->sample_status : "skipped");
        yvex_cli_out_writef(fp, "trace.step.%llu.append_status: %s\n",
                            i, record->append_status ? record->append_status : "not-started");
        yvex_cli_out_writef(fp, "trace.step.%llu.selected_token_id: %u\n",
                            i, record->selected_token_id);
        yvex_cli_out_writef(fp, "trace.step.%llu.appended_token_id: %u\n",
                            i, record->appended_token_id);
        yvex_cli_out_writef(fp, "trace.step.%llu.position_after_append: %llu\n",
                            i, record->position_after_append);
        yvex_cli_out_writef(fp, "trace.step.%llu.stop_reason: %s\n",
                            i, record->stop_reason ? record->stop_reason : "none");
        yvex_cli_out_writef(fp, "trace.step.%llu.stop_timing: %s\n",
                            i, record->stop_timing ? record->stop_timing : "none");
    }
}

/* Purpose: Compute generate emit kv trace for its CLI invariant (`generate_emit_kv_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_kv_trace(FILE *fp,
                                   const yvex_generation_report *report)
{
    if (!report) {
        return;
    }
    yvex_cli_out_kv_str(fp, "trace.kv.status",
                        report->trace_kv_requested ? "requested" : "unavailable");
    yvex_cli_out_kv_str(fp, "trace.kv.mode", "diagnostic");
    yvex_cli_out_kv_str(fp, "trace.kv.real_attention_kv", "false");
    yvex_cli_out_kv_str(fp, "trace.kv.full_model_kv", "false");
    if (report->trace_kv_requested) {
        yvex_cli_out_kv_u64(fp, "trace.kv.layers",
                            report->trace_kv_shape.layer_count);
        yvex_cli_out_kv_u64(fp, "trace.kv.heads",
                            report->trace_kv_shape.kv_head_count);
        yvex_cli_out_kv_u64(fp, "trace.kv.head_dim",
                            report->trace_kv_shape.head_dim);
        yvex_cli_out_kv_u64(fp, "trace.kv.capacity",
                            report->trace_kv_shape.capacity);
        yvex_cli_out_kv_str(fp, "trace.kv.binding_source",
                            "generate-decode-options");
    }
}

/* Purpose: Compute generate emit logits trace for its CLI invariant (`generate_emit_logits_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_logits_trace(FILE *fp,
                                       const yvex_generation_report *report)
{
    unsigned long long i;

    if (!report) {
        return;
    }
    for (i = 0ull; i < report->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &report->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        yvex_cli_out_writef(fp, "trace.step.%llu.logits_mode: bounded-diagnostic\n", i);
        yvex_cli_out_writef(fp, "trace.step.%llu.logits_checksum: %llu\n",
                            i, record->logits_checksum);
        yvex_cli_out_writef(fp, "trace.step.%llu.logits_min: %.9g\n",
                            i, record->logits_min);
        yvex_cli_out_writef(fp, "trace.step.%llu.logits_max: %.9g\n",
                            i, record->logits_max);
        yvex_cli_out_writef(fp, "trace.step.%llu.real_output_head_logits: false\n", i);
    }
}

/* Purpose: Compute generate emit sampling trace for its CLI invariant (`generate_emit_sampling_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_sampling_trace(FILE *fp,
                                         const yvex_generation_report *report)
{
    unsigned long long i;

    if (!report) {
        return;
    }
    for (i = 0ull; i < report->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &report->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        yvex_cli_out_writef(fp, "trace.step.%llu.sampling_strategy: greedy\n", i);
        yvex_cli_out_writef(fp, "trace.step.%llu.candidate_token_id: %u\n",
                            i, record->candidate_token_id);
        yvex_cli_out_writef(fp, "trace.step.%llu.candidate_logit: %.9g\n",
                            i, record->candidate_logit);
        yvex_cli_out_writef(fp, "trace.step.%llu.sample_checksum: %llu\n",
                            i, record->sample_checksum);
        yvex_cli_out_writef(fp, "trace.step.%llu.real_vocab_sampling: false\n", i);
        yvex_cli_out_writef(fp, "trace.step.%llu.stochastic_sampling: false\n", i);
    }
}

/* Purpose: Compute generate emit append trace for its CLI invariant (`generate_emit_append_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_append_trace(FILE *fp,
                                       const yvex_generation_report *report)
{
    unsigned long long i;

    if (!report) {
        return;
    }
    for (i = 0ull; i < report->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &report->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        yvex_cli_out_writef(fp, "trace.append.%llu.append_status: %s\n",
                            i, record->append_status ? record->append_status : "not-started");
        yvex_cli_out_writef(fp, "trace.append.%llu.candidate_token_id: %u\n",
                            i, record->candidate_token_id);
        yvex_cli_out_writef(fp, "trace.append.%llu.appended_token_id: %u\n",
                            i, record->appended_token_id);
        yvex_cli_out_writef(fp, "trace.append.%llu.accepted_token_count: %llu\n",
                            i, record->accepted_token_count);
        yvex_cli_out_writef(fp, "trace.append.%llu.generated_token_count: %llu\n",
                            i, record->generated_token_count);
        yvex_cli_out_writef(fp, "trace.append.%llu.total_token_count: %llu\n",
                            i, record->total_token_count);
        yvex_cli_out_writef(fp, "trace.append.%llu.sequence_checksum: %llu\n",
                            i, record->sequence_checksum);
    }
}

/* Purpose: Compute generate emit stop trace for its CLI invariant (`generate_emit_stop_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_stop_trace(FILE *fp,
                                     const yvex_generation_report *report)
{
    if (!report) {
        return;
    }
    yvex_cli_out_kv_str(fp, "trace.stop.policy",
                        report->stop_policy ? report->stop_policy : "bounded-diagnostic");
    yvex_cli_out_kv_bool(fp, "trace.stop.requested", report->stop_requested);
    yvex_cli_out_kv_str(fp, "trace.stop.reason",
                        report->stop_reason ? report->stop_reason : "internal-error");
    yvex_cli_out_kv_str(fp, "trace.stop.phase",
                        report->stop_phase ? report->stop_phase : "preflight");
    yvex_cli_out_kv_u64(fp, "trace.stop.step", report->stop_step);
    yvex_cli_out_kv_str(fp, "trace.stop.timing",
                        report->stop_timing ? report->stop_timing : "preflight");
    yvex_cli_out_kv_bool(fp, "trace.stop.before_append",
                         report->stop_before_append);
    yvex_cli_out_kv_bool(fp, "trace.stop.after_append",
                         report->stop_after_append);
    yvex_cli_out_kv_bool(fp, "trace.stop.failure_stop",
                         report->failure_stop);
    yvex_cli_out_kv_bool(fp, "trace.stop.unsupported_stop_feature",
                         report->unsupported_stop_feature);
    yvex_cli_out_kv_str(fp, "trace.stop.eos_policy",
                        report->eos_policy ? report->eos_policy : "unsupported");
    yvex_cli_out_kv_str(fp, "trace.stop.stop_token_policy",
                        report->stop_token_policy ? report->stop_token_policy : "unsupported");
}

/* Purpose: Compute generate emit cancel trace for its CLI invariant (`generate_emit_cancel_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_cancel_trace(FILE *fp,
                                       const yvex_generation_report *report)
{
    if (!report || !report->state.cancel_requested) {
        return;
    }
    yvex_cli_out_kv_str(fp, "trace.cancel.requested", "true");
    yvex_cli_out_kv_str(fp, "trace.cancel.reason",
                        report->state.cancel_reason ?
                            report->state.cancel_reason : "interrupted");
    if (report->state.cancel_step_seen) {
        yvex_cli_out_kv_u64(fp, "trace.cancel.step",
                            report->state.cancel_step);
    } else {
        yvex_cli_out_kv_str(fp, "trace.cancel.step", "none");
    }
    yvex_cli_out_kv_str(fp, "trace.cancel.timing",
                        report->state.cancel_timing ?
                            report->state.cancel_timing : "none");
    yvex_cli_out_kv_str(fp, "trace.cancel.safe_point",
                        report->state.cancel_safe_point ?
                            report->state.cancel_safe_point : "none");
    yvex_cli_out_kv_u64(fp, "trace.cancel.partial_generated_token_count",
                        report->partial_generated_token_count);
}

/* Purpose: Compute generate emit failure trace for its CLI invariant (`generate_emit_failure_trace`). */
static void generate_emit_failure_trace(FILE *fp,
                                        const yvex_generation_report *report)
{
    if (!report || !report->phase || strcmp(report->phase, "failed") != 0) {
        return;
    }
    yvex_cli_out_kv_str(fp, "trace.failure.phase",
                        report->failed_phase ? report->failed_phase : "internal-error");
    yvex_cli_out_kv_u64(fp, "trace.failure.step", report->failed_step);
    yvex_cli_out_kv_str(fp, "trace.failure.stop_reason",
                        report->stop_reason ? report->stop_reason : "internal-error");
    yvex_cli_out_kv_u64(fp, "trace.failure.partial_generated_token_count",
                        report->partial_generated_token_count);
    yvex_cli_out_kv_u64(fp, "trace.failure.last_successful_position",
                        report->last_successful_position);
    yvex_cli_out_kv_bool(fp, "trace.failure.cleanup_attempted",
                         report->cleanup_attempted);
}

/* Purpose: Release or reset owned generate emit cleanup trace state (`generate_emit_cleanup_trace`). */
static void generate_emit_cleanup_trace(FILE *fp,
                                        const yvex_generation_report *report)
{
    if (!report) {
        return;
    }
    yvex_cli_out_kv_bool(fp, "trace.cleanup.attempted",
                         report->cleanup_attempted);
    yvex_cli_out_kv_str(fp, "trace.cleanup.status",
                        report->cleanup_status ? report->cleanup_status : "not-needed");
}

/* Purpose: render requested trace sections from a finished diagnostic generation report.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_emit_trace(FILE *fp,
                                const yvex_generation_report *report)
{
    int failed;

    if (!report || !report->trace_enabled) {
        return;
    }
    failed = report->phase && strcmp(report->phase, "failed") == 0;
    if (yvex_generation_trace_wants_tokens(report->trace_level)) {
        generate_emit_token_trace(fp, report);
    }
    if (yvex_generation_trace_wants_steps(report->trace_level)) {
        generate_emit_step_trace(fp, report);
    }
    if (yvex_generation_trace_wants_kv(report->trace_level)) {
        generate_emit_kv_trace(fp, report);
    }
    if (yvex_generation_trace_wants_logits(report->trace_level)) {
        generate_emit_logits_trace(fp, report);
    }
    if (yvex_generation_trace_wants_sampling(report->trace_level)) {
        generate_emit_sampling_trace(fp, report);
    }
    if (report->trace_level == YVEX_GENERATION_TRACE_FULL) {
        generate_emit_append_trace(fp, report);
        generate_emit_stop_trace(fp, report);
    }
    if (report->state.cancel_requested) {
        generate_emit_cancel_trace(fp, report);
    }
    if (failed) {
        generate_emit_failure_trace(fp, report);
    }
    if (report->trace_level == YVEX_GENERATION_TRACE_FULL || failed) {
        generate_emit_cleanup_trace(fp, report);
    }
}

/* Purpose: Render generate render trace from typed facts (`yvex_generate_render_trace`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_render_trace(FILE *fp,
                               const yvex_generation_report *report)
{
    generate_emit_trace(fp, report);
    return 0;
}
