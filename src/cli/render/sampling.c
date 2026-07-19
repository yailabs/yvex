/* Owner: src/cli/render
 * Owns: normal, audit, and help serialization for typed sampling reports.
 * Does not own: sampling report construction, input parsing, command dispatch, sampler execution, graph guard
 *   preflight, stdout/stderr primitives, generation, eval, benchmark, or release decisions.
 * Invariants: this renderer writes only through src/cli/io helpers and never mutates reports.
 * Boundary: rendering bounded sampling facts is not runtime generation support.
 * Purpose: provide normal, audit, and help serialization for typed sampling reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"

/* Purpose: Render sampling render str from typed facts (`sampling_render_str`). */
static const char *sampling_render_str(const char *value, const char *fallback)
{
    return value ? value : fallback;
}

#define SAMPLE_TEXT(type_name, member, default_text) \
    {#member, YVEX_CLI_FIELD_TEXT, offsetof(type_name, member), default_text}
#define SAMPLE_U64(type_name, member) \
    {#member, YVEX_CLI_FIELD_U64, offsetof(type_name, member), NULL}
#define SAMPLE_U32(type_name, member) \
    {#member, YVEX_CLI_FIELD_U32, offsetof(type_name, member), NULL}
#define SAMPLE_BOOL(type_name, member) \
    {#member, YVEX_CLI_FIELD_BOOL, offsetof(type_name, member), NULL}
#define SAMPLE_FLOAT(type_name, member) \
    {#member, YVEX_CLI_FIELD_FLOAT9, offsetof(type_name, member), NULL}

static const yvex_cli_field_spec sampling_input_fields[] = {
    SAMPLE_TEXT(yvex_sampling_report, model_arg, ""),
    SAMPLE_TEXT(yvex_sampling_report, segment_name, "embedding-rmsnorm"),
    SAMPLE_TEXT(yvex_sampling_report, token_input_status, "fail"),
    SAMPLE_U64(yvex_sampling_report, input_token_count),
};

static const yvex_cli_field_spec sampling_summary_fields[] = {
    SAMPLE_BOOL(yvex_sampling_summary, logits_invoked),
    SAMPLE_BOOL(yvex_sampling_summary, logits_buffer_created),
    SAMPLE_TEXT(yvex_sampling_summary, logits_buffer_kind, "none"),
    SAMPLE_TEXT(yvex_sampling_summary, logits_phase, "not-started"),
    SAMPLE_U64(yvex_sampling_summary, logits_count),
    SAMPLE_U64(yvex_sampling_summary, logits_checksum),
    SAMPLE_FLOAT(yvex_sampling_summary, logits_min),
    SAMPLE_FLOAT(yvex_sampling_summary, logits_max),
    SAMPLE_BOOL(yvex_sampling_summary, sample_created),
    SAMPLE_BOOL(yvex_sampling_summary, sample_executed),
    SAMPLE_TEXT(yvex_sampling_summary, sampler_kind, "bounded-diagnostic"),
    SAMPLE_TEXT(yvex_sampling_summary, sampling_phase, "preflight"),
    SAMPLE_TEXT(yvex_sampling_summary, sampling_strategy, "greedy"),
    SAMPLE_TEXT(yvex_sampling_summary, sampling_source, "bounded-logits-buffer"),
    SAMPLE_U64(yvex_sampling_summary, candidates_considered),
    SAMPLE_TEXT(yvex_sampling_summary, tie_break, "lowest-index"),
    SAMPLE_U64(yvex_sampling_summary, selected_logit_index),
    SAMPLE_U32(yvex_sampling_summary, selected_token_id),
    SAMPLE_FLOAT(yvex_sampling_summary, selected_logit),
    SAMPLE_U64(yvex_sampling_summary, sample_checksum),
    SAMPLE_BOOL(yvex_sampling_summary, cleanup_attempted),
    SAMPLE_TEXT(yvex_sampling_summary, cleanup_status, "not-needed"),
    SAMPLE_BOOL(yvex_sampling_summary, bounded_sampling_ready),
};

static const yvex_cli_field_spec sampling_result_fields[] = {
    SAMPLE_BOOL(yvex_sampling_report, real_vocab_sampling),
    SAMPLE_BOOL(yvex_sampling_report, real_model_sampling),
    SAMPLE_BOOL(yvex_sampling_report, sampling_ready),
    SAMPLE_BOOL(yvex_sampling_report, generation_ready),
    SAMPLE_TEXT(yvex_sampling_report, generation, "unsupported"),
};

#undef SAMPLE_FLOAT
#undef SAMPLE_BOOL
#undef SAMPLE_U32
#undef SAMPLE_U64
#undef SAMPLE_TEXT

/* Purpose: Render sampling render normal from typed facts (`yvex_sampling_render_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_sampling_render_normal(FILE *fp,
                                const yvex_sampling_report *report)
{
    const yvex_sampling_report empty = {0};
    const yvex_sampling_report *facts = report ? report : &empty;
    const yvex_sampling_summary *summary = &facts->summary;
    const char *backend = summary->backend_name ? summary->backend_name : facts->backend_name;

    yvex_cli_out_line(fp, "sample: token");
    yvex_cli_out_line(fp, "status: sample-token");
    yvex_cli_out_kv_str(fp, "backend", backend ? backend : "cpu");
    (void)yvex_cli_out_fields(fp, facts, sampling_input_fields,
                              sizeof(sampling_input_fields) / sizeof(sampling_input_fields[0]));
    (void)yvex_cli_out_fields(fp, summary, sampling_summary_fields,
                              sizeof(sampling_summary_fields) / sizeof(sampling_summary_fields[0]));
    (void)yvex_cli_out_fields(fp, facts, sampling_result_fields,
                              sizeof(sampling_result_fields) / sizeof(sampling_result_fields[0]));
    if (facts->graph_guard_rendered) {
        yvex_cli_out_kv_str(fp, "graph_guard_status",
                            sampling_render_str(facts->graph_guard_status, "unknown"));
        yvex_cli_out_kv_str(fp, "graph_guard_phase",
                            sampling_render_str(facts->graph_guard_phase, "unknown"));
    }
    yvex_cli_out_kv_str(fp, "status",
                        sampling_render_str(facts->status, "sample-token-fail"));
    return 0;
}

/* Purpose: Render sampling render audit from typed facts (`yvex_sampling_render_audit`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
                      "boundary: bounded diagnostic greedy sampler only; no stochastic sampling, token "
                          "append, or generation");
    return 0;
}

/* Purpose: Render sampling render from typed facts (`yvex_sampling_render`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_sampling_render(FILE *fp,
                         yvex_sampling_report_mode mode,
                         const yvex_sampling_report *report)
{
    if (mode == YVEX_SAMPLING_REPORT_AUDIT) {
        return yvex_sampling_render_audit(fp, report);
    }
    return yvex_sampling_render_normal(fp, report);
}

/* Purpose: Render sampling render help from typed facts (`yvex_sampling_render_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_sampling_render_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex sample --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens "
            "IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--"
            "position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --"
            "kv-capacity N] [--logits-count N] [--strategy greedy]\n\nSample selects one bounded diagnostic "
            "token from the implemented logits buffer using greedy selection. It does not run stochastic "
            "sampling, append tokens, generate, or claim real DeepSeek vocabulary sampling.\n");
    return 0;
}
