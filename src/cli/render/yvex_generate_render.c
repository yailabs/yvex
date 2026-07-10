/*
 * yvex_generate_render.c - diagnostic generate report renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal, audit, and help text serialization for typed generation reports.
 *
 * Does not own:
 *   generation report construction, argv parsing, command dispatch, runtime
 *   execution, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   renderers serialize facts only, use src/cli/io writers, and do not mutate
 *   trace counters or generation state.
 *
 * Boundary:
 *   rendering diagnostic generation facts is not generation support.
 */
#include "yvex_generate_render.h"

#include "yvex_cli_out.h"
#include "yvex_generate_trace_render.h"

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

static void generate_print_summary(FILE *fp,
                                   const yvex_generation_report *report)
{
    yvex_cli_out_line(fp, "generate: loop");
    yvex_cli_out_kv_str(fp, "status",
                        report && report->status ?
                            report->status : "generation-loop-fail");
    yvex_cli_out_kv_str(fp, "model",
                        report && report->model_arg ? report->model_arg : "");
    yvex_cli_out_kv_str(fp, "backend",
                        report && report->backend_name ?
                            report->backend_name : "cpu");
    yvex_cli_out_kv_str(fp, "segment",
                        report && report->segment_name ?
                            report->segment_name : "embedding-rmsnorm");
    yvex_cli_out_kv_u64(fp, "state_id",
                        report ? report->state.state_id : 0ull);
    yvex_cli_out_kv_str(fp, "lifecycle_status",
                        report && report->state.lifecycle_status ?
                            report->state.lifecycle_status : "unknown");
    yvex_cli_out_kv_str(fp, "generation_state",
                        report && report->state.generation_state ?
                            report->state.generation_state : "unknown");
    yvex_cli_out_kv_bool(fp, "state_dirty",
                         report && report->state.state_dirty);
    generate_print_optional_ull(fp, "active_step",
                                report ? report->state.active_step_seen : 0,
                                report ? report->state.active_step : 0ull);
    generate_print_optional_ull(fp, "last_completed_step",
                                report ? report->state.last_completed_step_seen : 0,
                                report ? report->state.last_completed_step : 0ull);
    yvex_cli_out_kv_bool(fp, "cancel_supported",
                         report && report->state.cancel_supported);
    yvex_cli_out_kv_bool(fp, "cancel_requested",
                         report && report->state.cancel_requested);
    yvex_cli_out_kv_str(fp, "cancel_reason",
                        report && report->state.cancel_reason ?
                            report->state.cancel_reason : "none");
    generate_print_optional_ull(fp, "cancel_step",
                                report ? report->state.cancel_step_seen : 0,
                                report ? report->state.cancel_step : 0ull);
    yvex_cli_out_kv_str(fp, "cancel_timing",
                        report && report->state.cancel_timing ?
                            report->state.cancel_timing : "none");
    yvex_cli_out_kv_str(fp, "cancel_safe_point",
                        report && report->state.cancel_safe_point ?
                            report->state.cancel_safe_point : "none");
    yvex_cli_out_kv_bool(fp, "partial_output_available",
                         report && report->state.partial_output_available);
    yvex_cli_out_kv_str(fp, "token_input_status",
                        report && report->token_input_status ?
                            report->token_input_status : "fail");
    yvex_cli_out_kv_u64(fp, "prompt_token_count",
                        report ? report->prompt_token_count : 0ull);
    yvex_cli_out_kv_u64(fp, "prefill_token_count",
                        report ? report->prefill_token_count : 0ull);
    yvex_cli_out_kv_u64(fp, "max_new_tokens",
                        report ? report->max_new_tokens : 0ull);
    yvex_cli_out_kv_u64(fp, "context_length",
                        report ? report->context_length : 0ull);
    yvex_cli_out_kv_u64(fp, "generated_token_count",
                        report ? report->generated_token_count : 0ull);
    yvex_cli_out_kv_u64(fp, "accepted_token_count",
                        report ? report->accepted_token_count : 0ull);
    yvex_cli_out_kv_u64(fp, "partial_generated_token_count",
                        report ? report->partial_generated_token_count : 0ull);
    yvex_cli_out_kv_u64(fp, "total_token_count",
                        report ? report->total_token_count : 0ull);
    yvex_cli_out_kv_u64(fp, "position_start",
                        report ? report->position_start : 0ull);
    yvex_cli_out_kv_u64(fp, "prefill_position_end",
                        report ? report->prefill_position_end : 0ull);
    yvex_cli_out_kv_u64(fp, "current_decode_position",
                        report ? report->current_decode_position : 0ull);
    yvex_cli_out_kv_u64(fp, "last_successful_position",
                        report ? report->last_successful_position : 0ull);
    yvex_cli_out_kv_str(fp, "generation_loop_kind", "bounded-diagnostic");
    yvex_cli_out_kv_str(fp, "generation_mode", "diagnostic-runtime");
    yvex_cli_out_kv_str(fp, "decode_mode", "bounded-diagnostic");
    yvex_cli_out_kv_str(fp, "logits_mode", "bounded-diagnostic");
    yvex_cli_out_kv_str(fp, "sampling_strategy", "greedy");
    yvex_cli_out_kv_bool(fp, "bounded_generation",
                         report && report->loop_executed);
    yvex_cli_out_kv_str(fp, "full_model_generation", "false");
    yvex_cli_out_kv_str(fp, "real_deepseek_generation", "false");
    yvex_cli_out_kv_bool(fp, "prefill_invoked",
                         report && report->prefill_invoked);
    yvex_cli_out_kv_u64(fp, "decode_steps",
                        report ? report->decode_steps : 0ull);
    yvex_cli_out_kv_u64(fp, "logits_steps",
                        report ? report->logits_steps : 0ull);
    yvex_cli_out_kv_u64(fp, "sample_steps",
                        report ? report->sample_steps : 0ull);
    yvex_cli_out_kv_u64(fp, "append_steps",
                        report ? report->append_steps : 0ull);
    yvex_cli_out_kv_u32(fp, "candidate_token_id",
                        report && report->candidate_token_seen ?
                            report->candidate_token_id : 0u);
    yvex_cli_out_kv_double(fp, "candidate_logit",
                           report && report->candidate_token_seen ?
                               report->candidate_logit : 0.0);
    yvex_cli_out_kv_u32(fp, "last_selected_token_id",
                        report ? report->last_selected_token_id : 0u);
    yvex_cli_out_kv_double(fp, "last_selected_logit",
                           report ? report->last_selected_logit : 0.0);
    yvex_cli_out_kv_u32(fp, "last_appended_token_id",
                        report && report->last_appended_token_seen ?
                            report->last_appended_token_id : 0u);
    yvex_cli_out_kv_str(fp, "append_status",
                        report && report->append_status ?
                            report->append_status : "not-started");
    yvex_cli_out_kv_str(fp, "append_failure",
                        report && report->append_failure ?
                            report->append_failure : "none");
    yvex_cli_out_kv_str(fp, "stop_policy",
                        report && report->stop_policy ?
                            report->stop_policy : "bounded-diagnostic");
    yvex_cli_out_kv_bool(fp, "stop_requested",
                         report && report->stop_requested);
    yvex_cli_out_kv_str(fp, "stop_reason",
                        report && report->stop_reason ?
                            report->stop_reason : "internal-error");
    yvex_cli_out_kv_str(fp, "stop_phase",
                        report && report->stop_phase ?
                            report->stop_phase : "preflight");
    yvex_cli_out_kv_u64(fp, "stop_step",
                        report ? report->stop_step : 0ull);
    yvex_cli_out_kv_str(fp, "stop_timing",
                        report && report->stop_timing ?
                            report->stop_timing : "preflight");
    yvex_cli_out_kv_bool(fp, "stop_after_append",
                         report && report->stop_after_append);
    yvex_cli_out_kv_bool(fp, "stop_before_append",
                         report && report->stop_before_append);
    yvex_cli_out_kv_bool(fp, "failure_stop",
                         report && report->failure_stop);
    yvex_cli_out_kv_bool(fp, "unsupported_stop_feature",
                         report && report->unsupported_stop_feature);
    yvex_cli_out_kv_str(fp, "eos_policy",
                        report && report->eos_policy ?
                            report->eos_policy : "unsupported");
    yvex_cli_out_kv_str(fp, "stop_token_policy",
                        report && report->stop_token_policy ?
                            report->stop_token_policy : "unsupported");
    yvex_cli_out_kv_str(fp, "trace_level",
                        report && report->trace_level_name ?
                            report->trace_level_name : "none");
    yvex_cli_out_kv_bool(fp, "trace_enabled",
                         report && report->trace_enabled);
    yvex_cli_out_kv_u64(fp, "trace_records",
                        report ? report->trace_records : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_tokens",
                        report ? report->trace_tokens : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_steps",
                        report ? report->trace_steps : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_kv",
                        report ? report->trace_kv : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_logits",
                        report ? report->trace_logits : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_sampling",
                        report ? report->trace_sampling : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_append",
                        report ? report->trace_append : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_stop",
                        report ? report->trace_stop : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_cancel",
                        report ? report->trace_cancel : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_cleanup",
                        report ? report->trace_cleanup : 0ull);
    yvex_cli_out_kv_u64(fp, "trace_failures",
                        report ? report->trace_failures : 0ull);
    yvex_cli_out_kv_str(fp, "trace_status",
                        report && report->trace_status ?
                            report->trace_status : "disabled");
    yvex_cli_out_kv_u64(fp, "generation_checksum",
                        report ? report->generation_checksum : 0ull);
    yvex_cli_out_kv_u64(fp, "sequence_checksum",
                        report ? report->sequence_checksum : 0ull);
    generate_print_token_list(fp, "generated_token_ids",
                              report ? report->generated_tokens : 0,
                              report ? report->generated_token_count : 0ull);
    generate_print_runtime_sequence(fp, report);
    yvex_cli_out_kv_bool(fp, "cleanup_attempted",
                         report && report->cleanup_attempted);
    yvex_cli_out_kv_str(fp, "cleanup_status",
                        report && report->cleanup_status ?
                            report->cleanup_status : "not-needed");
    yvex_cli_out_kv_bool(fp, "cleanup_idempotent",
                         report && report->state.cleanup_idempotent);
    yvex_cli_out_kv_bool(fp, "cleanup_repeated",
                         report && report->state.cleanup_repeated);
    yvex_cli_out_kv_bool(fp, "cleanup_owned_state_released",
                         report && report->state.cleanup_owned_state_released);
    yvex_cli_out_kv_bool(fp, "failure_preserved",
                         report && report->state.failure_preserved);
    yvex_cli_out_kv_bool(fp, "partial_output_preserved",
                         report && report->state.partial_output_preserved);
    yvex_cli_out_kv_str(fp, "generation_ready", "false");
    yvex_cli_out_kv_str(fp, "generation", "unsupported-full-model");
    yvex_cli_out_kv_str(fp, "benchmark_status", "not-measured");
    yvex_cli_out_kv_str(fp, "failed_phase",
                        report && report->failed_phase ?
                            report->failed_phase : "none");
    yvex_cli_out_kv_u64(fp, "failed_step",
                        report ? report->failed_step : 0ull);
}

static void generate_print_trace_and_summary(
    FILE *fp,
    const yvex_generation_report *report)
{
    yvex_generate_render_trace(fp, report);
    generate_print_summary(fp, report);
}

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

int yvex_generate_render(FILE *fp,
                         yvex_generate_render_mode mode,
                         const yvex_generation_report *report)
{
    generate_print_output(fp, mode, report);
    return 0;
}

int yvex_generate_render_normal(FILE *fp,
                                const yvex_generation_report *report)
{
    generate_print_normal_summary(fp, report);
    return 0;
}

int yvex_generate_render_audit(FILE *fp,
                               const yvex_generation_report *report)
{
    generate_print_trace_and_summary(fp, report);
    return 0;
}

int yvex_generate_render_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\n\n");
    yvex_cli_out_writef(fp, "Bounded diagnostic generation loop over the existing prefill, decode, bounded logits, greedy sample, token append, stop, trace, cancel, and cleanup path.\n\n");
    yvex_cli_out_writef(fp, "Model-backed DeepSeek generation is unsupported. This command has no product-generation example until the full runtime path exists.\n\n");
    yvex_cli_out_writef(fp, "Required arguments:\n");
    yvex_cli_out_writef(fp, "  --model FILE_OR_ALIAS      selected segment artifact path or registry alias\n");
    yvex_cli_out_writef(fp, "  --backend cpu|cuda         backend used by the bounded diagnostic path\n");
    yvex_cli_out_writef(fp, "  --segment embedding-rmsnorm\n");
    yvex_cli_out_writef(fp, "  --tokens IDS               comma-separated diagnostic token IDs\n");
    yvex_cli_out_writef(fp, "  --max-new-tokens N         positive bounded diagnostic token budget\n\n");
    yvex_cli_out_writef(fp, "Diagnostic options:\n");
    yvex_cli_out_writef(fp, "  --strategy greedy          only greedy is accepted\n");
    yvex_cli_out_writef(fp, "  --context-length N         stop before append when the bounded context is full\n");
    yvex_cli_out_writef(fp, "  --logits-count N           bounded diagnostic logits count, 1 <= N <= 256\n");
    yvex_cli_out_writef(fp, "  --layers N                 optional controlled layer fixture scheduling\n");
    yvex_cli_out_writef(fp, "  --chunk-size N             optional bounded prefill chunking\n");
    yvex_cli_out_writef(fp, "  --attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N\n\n");
    yvex_cli_out_writef(fp, "Trace options:\n");
    yvex_cli_out_writef(fp, "  --trace-level none|tokens|steps|kv|logits|sampling|full\n");
    yvex_cli_out_writef(fp, "  Trace records are diagnostic text records only; they do not dump raw tensors.\n\n");
    yvex_cli_out_writef(fp, "Cancellation options:\n");
    yvex_cli_out_writef(fp, "  --cancel-after-steps N     N=0 cancels before first decode; N>0 cancels after N appended diagnostic tokens\n\n");
    yvex_cli_out_writef(fp, "Stop behavior:\n");
    yvex_cli_out_writef(fp, "  max-new-tokens stops after append; context-length stops before append; decode/logits/sample/append failures preserve partial diagnostic output.\n");
    yvex_cli_out_writef(fp, "  Partial diagnostic output is preserved on failure, cancellation, and context stops.\n");
    yvex_cli_out_writef(fp, "  EOS and stop-token text matching are unsupported for this bounded path.\n\n");
    yvex_cli_out_writef(fp, "Output policy:\n");
    yvex_cli_out_writef(fp, "  --output normal|audit     normal is compact; audit preserves full diagnostic state fields\n");
    yvex_cli_out_writef(fp, "  --audit                   shortcut for --output audit\n");
    yvex_cli_out_writef(fp, "  Text output is the stable operator contract. Normal output is compact by default; audit/trace output carries diagnostic state, counters, trace, cancel, cleanup, and boundary records.\n");
    yvex_cli_out_writef(fp, "  The command emits no ANSI color by default.\n\n");
    yvex_cli_out_writef(fp, "Boundaries:\n");
    yvex_cli_out_writef(fp, "  full model generation: unsupported\n");
    yvex_cli_out_writef(fp, "  real DeepSeek generation: unsupported\n");
    yvex_cli_out_writef(fp, "  real output-head logits: unsupported\n");
    yvex_cli_out_writef(fp, "  real vocabulary sampling: unsupported\n");
    yvex_cli_out_writef(fp, "  tokenizer-quality text generation: unsupported\n");
    yvex_cli_out_writef(fp, "  provider/server/streaming generation: unsupported\n");
    yvex_cli_out_writef(fp, "  evaluation: unsupported\n");
    yvex_cli_out_writef(fp, "  benchmark_status: not-measured\n");
    yvex_cli_out_writef(fp, "  throughput: not-measured\n");
    return 0;
}
