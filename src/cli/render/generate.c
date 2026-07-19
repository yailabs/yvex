/* Owner: src/cli/render
 * Owns: normal, audit, and help text serialization for typed generation reports.
 * Does not own: generation report construction, argv parsing, command dispatch, runtime execution, eval, benchmark,
 *   or release decisions.
 * Invariants: renderers serialize facts only, use src/cli/io writers, and do not mutate trace counters or
 *   generation state.
 * Boundary: rendering diagnostic generation facts is not generation support.
 * Purpose: provide normal, audit, and help text serialization for typed generation reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"

static const yvex_render_field_spec generation_identity_fields[] = {
    {"status", YVEX_RENDER_FIELD_TEXT, offsetof(yvex_generation_report, status),
     "generation-loop-fail"},
    {"model", YVEX_RENDER_FIELD_TEXT, offsetof(yvex_generation_report, model_arg), ""},
    {"backend", YVEX_RENDER_FIELD_TEXT, offsetof(yvex_generation_report, backend_name), "cpu"},
    {"segment", YVEX_RENDER_FIELD_TEXT, offsetof(yvex_generation_report, segment_name),
     "embedding-rmsnorm"},
    {"state_id", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, state.state_id), NULL},
    {"lifecycle_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, state.lifecycle_status), "unknown"},
    {"generation_state", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, state.generation_state), "unknown"},
    {"state_dirty", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.state_dirty), NULL},
};

static const yvex_render_field_spec generation_cancel_fields[] = {
    {"cancel_supported", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.cancel_supported), NULL},
    {"cancel_requested", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.cancel_requested), NULL},
    {"cancel_reason", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, state.cancel_reason), "none"},
};

static const yvex_render_field_spec generation_input_fields[] = {
    {"cancel_timing", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, state.cancel_timing), "none"},
    {"cancel_safe_point", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, state.cancel_safe_point), "none"},
    {"partial_output_available", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.partial_output_available), NULL},
    {"token_input_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, token_input_status), "fail"},
    {"prompt_token_count", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, prompt_token_count), NULL},
    {"prefill_token_count", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, prefill_token_count), NULL},
    {"max_new_tokens", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, max_new_tokens), NULL},
    {"context_length", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, context_length), NULL},
    {"generated_token_count", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, generated_token_count), NULL},
    {"accepted_token_count", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, accepted_token_count), NULL},
    {"partial_generated_token_count", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, partial_generated_token_count), NULL},
    {"total_token_count", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, total_token_count), NULL},
    {"position_start", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, position_start), NULL},
    {"prefill_position_end", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, prefill_position_end), NULL},
    {"current_decode_position", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, current_decode_position), NULL},
    {"last_successful_position", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, last_successful_position), NULL},
};

static const char *const generation_mode_lines[] = { "generation_loop_kind: bounded-diagnostic",
    "generation_mode: diagnostic-runtime",
    "decode_mode: bounded-diagnostic",
    "logits_mode: bounded-diagnostic",
    "sampling_strategy: greedy"};

static const char *const generation_boundary_lines[] = { "full_model_generation: false",
    "real_deepseek_generation: false"};

static const yvex_render_field_spec generation_execution_fields[] = {
    {"prefill_invoked", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, prefill_invoked), NULL},
    {"decode_steps", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, decode_steps), NULL},
    {"logits_steps", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, logits_steps), NULL},
    {"sample_steps", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, sample_steps), NULL},
    {"append_steps", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, append_steps), NULL},
};

static const yvex_render_field_spec generation_selection_fields[] = {
    {"last_selected_token_id", YVEX_RENDER_FIELD_U32,
     offsetof(yvex_generation_report, last_selected_token_id), NULL},
    {"last_selected_logit", YVEX_RENDER_FIELD_DOUBLE,
     offsetof(yvex_generation_report, last_selected_logit), NULL},
};

static const yvex_render_field_spec generation_result_fields[] = {
    {"append_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, append_status), "not-started"},
    {"append_failure", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, append_failure), "none"},
    {"stop_policy", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, stop_policy), "bounded-diagnostic"},
    {"stop_requested", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, stop_requested), NULL},
    {"stop_reason", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, stop_reason), "internal-error"},
    {"stop_phase", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, stop_phase), "preflight"},
    {"stop_step", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, stop_step), NULL},
    {"stop_timing", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, stop_timing), "preflight"},
    {"stop_after_append", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, stop_after_append), NULL},
    {"stop_before_append", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, stop_before_append), NULL},
    {"failure_stop", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, failure_stop), NULL},
    {"unsupported_stop_feature", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, unsupported_stop_feature), NULL},
    {"eos_policy", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, eos_policy), "unsupported"},
    {"stop_token_policy", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, stop_token_policy), "unsupported"},
    {"trace_level", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, trace_level_name), "none"},
    {"trace_enabled", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, trace_enabled), NULL},
    {"trace_records", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_records), NULL},
    {"trace_tokens", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_tokens), NULL},
    {"trace_steps", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_steps), NULL},
    {"trace_kv", YVEX_RENDER_FIELD_U64, offsetof(yvex_generation_report, trace_kv), NULL},
    {"trace_logits", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_logits), NULL},
    {"trace_sampling", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_sampling), NULL},
    {"trace_append", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_append), NULL},
    {"trace_stop", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_stop), NULL},
    {"trace_cancel", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_cancel), NULL},
    {"trace_cleanup", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_cleanup), NULL},
    {"trace_failures", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, trace_failures), NULL},
    {"trace_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, trace_status), "disabled"},
    {"generation_checksum", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, generation_checksum), NULL},
    {"sequence_checksum", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, sequence_checksum), NULL},
};

static const yvex_render_field_spec generation_cleanup_fields[] = {
    {"cleanup_attempted", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, cleanup_attempted), NULL},
    {"cleanup_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, cleanup_status), "not-needed"},
    {"cleanup_idempotent", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.cleanup_idempotent), NULL},
    {"cleanup_repeated", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.cleanup_repeated), NULL},
    {"cleanup_owned_state_released", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.cleanup_owned_state_released), NULL},
    {"failure_preserved", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.failure_preserved), NULL},
    {"partial_output_preserved", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_generation_report, state.partial_output_preserved), NULL},
};

static const char *const generation_cleanup_boundary_lines[] = { "generation_ready: false",
    "generation: unsupported-full-model",
    "benchmark_status: not-measured"};

static const yvex_render_field_spec generation_failure_fields[] = {
    {"failed_phase", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_generation_report, failed_phase), "none"},
    {"failed_step", YVEX_RENDER_FIELD_U64,
     offsetof(yvex_generation_report, failed_step), NULL},
};

static const char *const literal_lines_0[] = {
    "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens "
        "IDS --max-new-tokens N [options]\n",
    "Bounded diagnostic generation loop over the existing prefill, decode, bounded logits, greedy sample, "
        "token append, stop, trace, cancel, and cleanup path.\n",
    "Model-backed DeepSeek generation is unsupported. This command has no product-generation example until "
        "the full runtime path exists.\n",
    "Required arguments:",
    "  --model FILE_OR_ALIAS      selected segment artifact path or registry alias",
    "  --backend cpu|cuda         backend used by the bounded diagnostic path",
    "  --segment embedding-rmsnorm",
    "  --tokens IDS               comma-separated diagnostic token IDs",
    "  --max-new-tokens N         positive bounded diagnostic token budget\n",
    "Diagnostic options:",
    "  --strategy greedy          only greedy is accepted",
    "  --context-length N         stop before append when the bounded context is full",
    "  --logits-count N           bounded diagnostic logits count, 1 <= N <= 256",
    "  --layers N                 optional controlled layer fixture scheduling",
    "  --chunk-size N             optional bounded prefill chunking",
    "  --attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N\n",
    "Trace options:",
    "  --trace-level none|tokens|steps|kv|logits|sampling|full",
    "  Trace records are diagnostic text records only; they do not dump raw tensors.\n",
    "Cancellation options:",
    "  --cancel-after-steps N     N=0 cancels before first decode; N>0 cancels after N appended diagnostic tokens\n",
    "Stop behavior:",
    "  max-new-tokens stops after append; context-length stops before append; decode/logits/sample/append "
        "failures preserve partial diagnostic output.",
    "  Partial diagnostic output is preserved on failure, cancellation, and context stops.",
    "  EOS and stop-token text matching are unsupported for this bounded path.\n",
    "Output policy:",
    "  --output normal|audit     normal is compact; audit preserves full diagnostic state fields",
    "  --audit                   shortcut for --output audit",
    "  Text output is the stable operator contract. Normal output is compact by default; audit/trace "
        "output carries diagnostic state, counters, trace, cancel, cleanup, and boundary records.",
    "  The command emits no ANSI color by default.\n",
    "Boundaries:",
    "  full model generation: unsupported",
    "  real DeepSeek generation: unsupported",
    "  real output-head logits: unsupported",
    "  real vocabulary sampling: unsupported",
    "  tokenizer-quality text generation: unsupported",
    "  provider/server/streaming generation: unsupported",
    "  evaluation: unsupported",
    "  benchmark_status: not-measured",
    "  throughput: not-measured"};

/* Purpose: Render generate print optional ull from typed facts (`generate_print_optional_ull`). */
static void generate_print_optional_ull(FILE *fp,
                                        const char *label,
                                        int seen,
                                        unsigned long long value)
{
    yvex_cli_out_writef(fp, "%s: ", label ? label : "value");
    if (seen) {
        yvex_cli_out_writef(fp, "%llu", value);
    } else {
        yvex_cli_out_puts(fp, "none");
    }
    yvex_cli_out_char(fp, '\n');
}

/* Purpose: Render generate print token list from typed facts (`generate_print_token_list`). */
static void generate_print_token_list(FILE *fp,
                                      const char *label,
                                      const unsigned int *tokens,
                                      unsigned long long count)
{
    unsigned long long i;

    yvex_cli_out_writef(fp, "%s: ", label ? label : "tokens");
    for (i = 0ull; tokens && i < count; ++i) {
        if (i > 0ull) {
            yvex_cli_out_char(fp, ',');
        }
        yvex_cli_out_writef(fp, "%u", tokens[i]);
    }
    yvex_cli_out_char(fp, '\n');
}

/* Purpose: Render generate print runtime sequence from typed facts (`generate_print_runtime_sequence`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_print_runtime_sequence(
    FILE *fp,
    const yvex_generation_report *report)
{
    unsigned long long i;
    unsigned long long emitted = 0ull;

    yvex_cli_out_puts(fp, "runtime_token_sequence: ");
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

/* Purpose: Render generate print cleanup from typed facts (`generate_print_cleanup`). */
static void generate_print_cleanup(FILE *fp,
                                   const yvex_generation_report *report)
{
    yvex_generation_report empty = {0};
    const yvex_generation_report *facts = report ? report : &empty;

    render_object_fields(fp, facts, generation_cleanup_fields,
                         sizeof(generation_cleanup_fields) /
                             sizeof(generation_cleanup_fields[0]));
    render_lines(fp, generation_cleanup_boundary_lines,
                 sizeof(generation_cleanup_boundary_lines) /
                     sizeof(generation_cleanup_boundary_lines[0]));
    render_object_fields(fp, facts, generation_failure_fields,
                         sizeof(generation_failure_fields) /
                             sizeof(generation_failure_fields[0]));
}

/* Purpose: Render generate print summary from typed facts (`generate_print_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void generate_print_summary(FILE *fp,
                                   const yvex_generation_report *report)
{
    yvex_generation_report empty = {0};
    const yvex_generation_report *facts = report ? report : &empty;

    yvex_cli_out_line(fp, "generate: loop");
    render_object_fields(fp, facts, generation_identity_fields,
                         sizeof(generation_identity_fields) /
                             sizeof(generation_identity_fields[0]));
    generate_print_optional_ull(fp, "active_step",
                                facts->state.active_step_seen,
                                facts->state.active_step);
    generate_print_optional_ull(fp, "last_completed_step",
                                facts->state.last_completed_step_seen,
                                facts->state.last_completed_step);
    render_object_fields(fp, facts, generation_cancel_fields,
                         sizeof(generation_cancel_fields) /
                             sizeof(generation_cancel_fields[0]));
    generate_print_optional_ull(fp, "cancel_step",
                                facts->state.cancel_step_seen,
                                facts->state.cancel_step);
    render_object_fields(fp, facts, generation_input_fields,
                         sizeof(generation_input_fields) /
                             sizeof(generation_input_fields[0]));
    render_lines(fp, generation_mode_lines,
                 sizeof(generation_mode_lines) / sizeof(generation_mode_lines[0]));
    yvex_cli_out_kv_bool(fp, "bounded_generation", facts->loop_executed);
    render_lines(fp, generation_boundary_lines,
                 sizeof(generation_boundary_lines) /
                     sizeof(generation_boundary_lines[0]));
    render_object_fields(fp, facts, generation_execution_fields,
                         sizeof(generation_execution_fields) /
                             sizeof(generation_execution_fields[0]));
    yvex_cli_out_kv_u32(fp, "candidate_token_id",
                        facts->candidate_token_seen ? facts->candidate_token_id : 0u);
    yvex_cli_out_kv_double(fp, "candidate_logit",
                           facts->candidate_token_seen ? facts->candidate_logit : 0.0);
    render_object_fields(fp, facts, generation_selection_fields,
                         sizeof(generation_selection_fields) /
                             sizeof(generation_selection_fields[0]));
    yvex_cli_out_kv_u32(fp, "last_appended_token_id",
                        facts->last_appended_token_seen
                            ? facts->last_appended_token_id : 0u);
    render_object_fields(fp, facts, generation_result_fields,
                         sizeof(generation_result_fields) /
                             sizeof(generation_result_fields[0]));
    generate_print_token_list(fp, "generated_token_ids",
                              facts->generated_tokens,
                              facts->generated_token_count);
    generate_print_runtime_sequence(fp, facts);
    generate_print_cleanup(fp, facts);
}

/* Purpose: Render generate print trace and summary from typed facts (`generate_print_trace_and_summary`). */
static void generate_print_trace_and_summary(
    FILE *fp,
    const yvex_generation_report *report)
{
    yvex_generate_render_trace(fp, report);
    generate_print_summary(fp, report);
}

/* Purpose: Render generate print normal summary from typed facts (`generate_print_normal_summary`). */
static void generate_print_normal_summary(
    FILE *fp,
    const yvex_generation_report *report)
{
    yvex_cli_out_line(fp, "status: diagnostic-generation");
    yvex_cli_out_kv_str(fp, "model",
                        report && report->model_arg ? report->model_arg : "");
    yvex_cli_out_kv_str(fp, "backend",
                        report && report->backend_name ?
                            report->backend_name : "cpu");
    yvex_cli_out_writef(fp, "tokens: %llu -> %llu diagnostic\n",
                        report ? report->prompt_token_count : 0ull,
                        report ? report->generated_token_count : 0ull);
    yvex_cli_out_kv_str(fp, "stop",
                        report && report->stop_reason ?
                            report->stop_reason : "internal-error");
    yvex_cli_out_line(fp, "boundary: full-model generation unsupported");
    yvex_cli_out_line(fp, "benchmark_status: not-measured");
    yvex_cli_out_line(fp, "hint: use --audit or --trace-level full for diagnostic internals");
}

/* Purpose: Render generate print output from typed facts (`generate_print_output`). */
static void generate_print_output(FILE *fp,
                                  yvex_generate_render_mode mode,
                                  const yvex_generation_report *report)
{
    if (mode == YVEX_GENERATE_RENDER_AUDIT ||
        (report && report->trace_level != YVEX_GENERATION_TRACE_NONE)) {
        generate_print_trace_and_summary(fp, report);
        return;
    }
    generate_print_normal_summary(fp, report);
}

/* Purpose: Render generate render from typed facts (`yvex_generate_render`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_render(FILE *fp,
                         yvex_generate_render_mode mode,
                         const yvex_generation_report *report)
{
    generate_print_output(fp, mode, report);
    return 0;
}

/* Purpose: Render generate render normal from typed facts (`yvex_generate_render_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_render_normal(FILE *fp,
                                const yvex_generation_report *report)
{
    generate_print_normal_summary(fp, report);
    return 0;
}

/* Purpose: Render generate render audit from typed facts (`yvex_generate_render_audit`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_render_audit(FILE *fp,
                               const yvex_generation_report *report)
{
    generate_print_trace_and_summary(fp, report);
    return 0;
}

/* Purpose: Render generate render help from typed facts (`yvex_generate_render_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_generate_render_help(FILE *fp)
{
    yvex_cli_out_lines(fp, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    return 0;
}
