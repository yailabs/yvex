/*
 * yvex_sampling_render.c - typed sampling report rendering.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal, audit, and help serialization for typed sampling reports.
 *
 * Does not own:
 *   sampling report construction, input parsing, command dispatch, sampler
 *   execution, graph guard preflight, stdout/stderr primitives, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   this renderer writes only through src/cli/io helpers and never mutates
 *   reports.
 *
 * Boundary:
 *   rendering bounded sampling facts is not runtime generation support.
 */
#include "yvex_sampling_render.h"

#include "yvex_cli_out.h"

static const char *sampling_render_str(const char *value, const char *fallback)
{
    return value ? value : fallback;
}

static const yvex_sampling_summary *
sampling_render_summary(const yvex_sampling_report *report)
{
    return report ? &report->summary : NULL;
}

int yvex_sampling_render_normal(FILE *fp,
                                const yvex_sampling_report *report)
{
    const yvex_sampling_summary *summary = sampling_render_summary(report);

    yvex_cli_out_line(fp, "sample: token");
    yvex_cli_out_line(fp, "status: sample-token");
    yvex_cli_out_kv_str(fp, "model",
                        report && report->model_arg ? report->model_arg : "");
    yvex_cli_out_kv_str(fp, "backend",
                        summary && summary->backend_name
                            ? summary->backend_name
                            : sampling_render_str(report ? report->backend_name : NULL,
                                                  "cpu"));
    yvex_cli_out_kv_str(fp, "segment",
                        sampling_render_str(report ? report->segment_name : NULL,
                                            "embedding-rmsnorm"));
    yvex_cli_out_kv_str(fp, "token_input_status",
                        sampling_render_str(report ? report->token_input_status : NULL,
                                            "fail"));
    yvex_cli_out_kv_u64(fp, "input_token_count",
                        report ? report->input_token_count : 0ull);
    yvex_cli_out_kv_bool(fp, "logits_invoked",
                         summary && summary->logits_invoked);
    yvex_cli_out_kv_bool(fp, "logits_buffer_created",
                         summary && summary->logits_buffer_created);
    yvex_cli_out_kv_str(fp, "logits_buffer_kind",
                        sampling_render_str(summary ? summary->logits_buffer_kind : NULL,
                                            "none"));
    yvex_cli_out_kv_str(fp, "logits_phase",
                        sampling_render_str(summary ? summary->logits_phase : NULL,
                                            "not-started"));
    yvex_cli_out_kv_u64(fp, "logits_count",
                        summary ? summary->logits_count : 0ull);
    yvex_cli_out_kv_u64(fp, "logits_checksum",
                        summary ? summary->logits_checksum : 0ull);
    yvex_cli_out_writef(fp, "logits_min: %.9g\n",
                        summary ? summary->logits_min : 0.0);
    yvex_cli_out_writef(fp, "logits_max: %.9g\n",
                        summary ? summary->logits_max : 0.0);
    yvex_cli_out_kv_bool(fp, "sample_created",
                         summary && summary->sample_created);
    yvex_cli_out_kv_bool(fp, "sample_executed",
                         summary && summary->sample_executed);
    yvex_cli_out_kv_str(fp, "sampler_kind",
                        sampling_render_str(summary ? summary->sampler_kind : NULL,
                                            "bounded-diagnostic"));
    yvex_cli_out_kv_str(fp, "sampling_phase",
                        sampling_render_str(summary ? summary->sampling_phase : NULL,
                                            "preflight"));
    yvex_cli_out_kv_str(fp, "sampling_strategy",
                        sampling_render_str(summary ? summary->sampling_strategy : NULL,
                                            "greedy"));
    yvex_cli_out_kv_str(fp, "sampling_source",
                        sampling_render_str(summary ? summary->sampling_source : NULL,
                                            "bounded-logits-buffer"));
    yvex_cli_out_kv_u64(fp, "candidates_considered",
                        summary ? summary->candidates_considered : 0ull);
    yvex_cli_out_kv_str(fp, "tie_break",
                        sampling_render_str(summary ? summary->tie_break : NULL,
                                            "lowest-index"));
    yvex_cli_out_kv_u64(fp, "selected_logit_index",
                        summary ? summary->selected_logit_index : 0ull);
    yvex_cli_out_writef(fp, "selected_token_id: %u\n",
                        summary ? summary->selected_token_id : 0u);
    yvex_cli_out_writef(fp, "selected_logit: %.9g\n",
                        summary ? summary->selected_logit : 0.0);
    yvex_cli_out_kv_u64(fp, "sample_checksum",
                        summary ? summary->sample_checksum : 0ull);
    yvex_cli_out_kv_bool(fp, "cleanup_attempted",
                         summary && summary->cleanup_attempted);
    yvex_cli_out_kv_str(fp, "cleanup_status",
                        sampling_render_str(summary ? summary->cleanup_status : NULL,
                                            "not-needed"));
    yvex_cli_out_kv_bool(fp, "bounded_sampling_ready",
                         summary && summary->bounded_sampling_ready);
    yvex_cli_out_kv_bool(fp, "real_vocab_sampling",
                         report && report->real_vocab_sampling);
    yvex_cli_out_kv_bool(fp, "real_model_sampling",
                         report && report->real_model_sampling);
    yvex_cli_out_kv_bool(fp, "sampling_ready",
                         report && report->sampling_ready);
    yvex_cli_out_kv_bool(fp, "generation_ready",
                         report && report->generation_ready);
    yvex_cli_out_kv_str(fp, "generation",
                        sampling_render_str(report ? report->generation : NULL,
                                            "unsupported"));
    if (report && report->graph_guard_rendered) {
        yvex_cli_out_kv_str(fp, "graph_guard_status",
                            sampling_render_str(report->graph_guard_status,
                                                "unknown"));
        yvex_cli_out_kv_str(fp, "graph_guard_phase",
                            sampling_render_str(report->graph_guard_phase,
                                                "unknown"));
    }
    yvex_cli_out_kv_str(fp, "status",
                        sampling_render_str(report ? report->status : NULL,
                                            "sample-token-fail"));
    return 0;
}

int yvex_sampling_render_audit(FILE *fp,
                               const yvex_sampling_report *report)
{
    (void)yvex_sampling_render_normal(fp, report);
    yvex_cli_out_kv_str(fp, "runtime_claim",
                        sampling_render_str(report ? report->runtime_claim : NULL,
                                            "unsupported"));
    yvex_cli_out_kv_str(fp, "benchmark_status",
                        sampling_render_str(report ? report->benchmark_status : NULL,
                                            "not-measured"));
    yvex_cli_out_line(fp,
                      "boundary: bounded diagnostic greedy sampler only; "
                      "no stochastic sampling, token append, or generation");
    return 0;
}

int yvex_sampling_render(FILE *fp,
                         yvex_sampling_report_mode mode,
                         const yvex_sampling_report *report)
{
    if (mode == YVEX_SAMPLING_REPORT_AUDIT) {
        return yvex_sampling_render_audit(fp, report);
    }
    return yvex_sampling_render_normal(fp, report);
}

int yvex_sampling_render_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex sample --model FILE_OR_ALIAS --backend cpu|cuda "
        "--segment embedding-rmsnorm --tokens IDS [--layers N "
        "[--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] "
        "[--chunk-size N] [--position-start N] [--context-length N] "
        "[--attach-kv --kv-layers N --kv-heads N --kv-head-dim N "
        "--kv-capacity N] [--logits-count N] [--strategy greedy]\n\n"
        "Sample selects one bounded diagnostic token from the implemented "
        "logits buffer using greedy selection. It does not run stochastic "
        "sampling, append tokens, generate, or claim real DeepSeek vocabulary "
        "sampling.\n");
    return 0;
}
