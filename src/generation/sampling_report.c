/* Owner: src/generation
 * Owns: diagnostic sampling report construction, model reference resolution, registered identity gate, token input
 *   validation, graph guard preflight, engine open/close orchestration, and sampler invocation facts.
 * Does not own: CLI input grammar, command dispatch, rendering, stdout/stderr output, stochastic sampling, token
 *   append, generation, eval, benchmark, or release decisions.
 * Invariants: report builders populate typed facts and never print operator output.
 * Boundary: this report proves only the existing bounded diagnostic greedy sampler.
 * Purpose: Build typed sampling evidence from model admission through bounded diagnostic execution.
 * Inputs: A sampling request, registries, and caller-owned report storage.
 * Effects: Owns temporary engine, token-input, and sampler resources for one report transaction.
 * Failure: Any gate or cleanup failure remains explicit and does not imply generation support. */
#include <yvex/internal/core.h>
#include <yvex/internal/generation.h>
#include <yvex/internal/runtime.h>

#include <string.h>

static const yvex_decode_step_options sampling_decode_default = {
    .backend_name = "cpu",
    .segment_name = "embedding-rmsnorm",
};

static const yvex_logits_buffer_options sampling_logits_default = {
    .logits_count = 16ull,
};

static const yvex_sampling_options sampling_options_default = {
    .strategy = YVEX_SAMPLING_STRATEGY_GREEDY,
};

#define SAMPLING_REQUEST_FIELD(field_)                                                  \
    {                                                                                   \
        offsetof(yvex_decode_step_options, field_),                                     \
        offsetof(yvex_sampling_report_request, field_),                                 \
        sizeof(((yvex_sampling_report_request *)0)->field_) +                           \
            0u * sizeof(char[sizeof(((yvex_decode_step_options *)0)->field_) ==          \
                                    sizeof(((yvex_sampling_report_request *)0)->field_)  \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection sampling_request_fields[] = {
    SAMPLING_REQUEST_FIELD(segment_name),
    SAMPLING_REQUEST_FIELD(backend_name),
    SAMPLING_REQUEST_FIELD(position_start),
    SAMPLING_REQUEST_FIELD(chunk_size),
    SAMPLING_REQUEST_FIELD(context_length),
    SAMPLING_REQUEST_FIELD(attach_kv),
    SAMPLING_REQUEST_FIELD(kv_shape),
    SAMPLING_REQUEST_FIELD(layer_count),
    SAMPLING_REQUEST_FIELD(layer_hidden_dim),
    SAMPLING_REQUEST_FIELD(layer_head_dim),
    SAMPLING_REQUEST_FIELD(layer_ffn_dim),
};

#undef SAMPLING_REQUEST_FIELD

/* Purpose: Implement the canonical sampling default summary mechanism owned by the generation boundary. */
static void sampling_default_summary(yvex_sampling_summary *summary,
                                     const char *backend_name)
{
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options options;

    decode_options = sampling_decode_default;
    logits_options = sampling_logits_default;
    options = sampling_options_default;
    decode_options.backend_name = backend_name ? backend_name : decode_options.backend_name;
    logits_options.decode_options = &decode_options;
    options.logits_options = &logits_options;
    yvex_sampling_summary_defaults(summary, &options);
}

static const yvex_sampling_report sampling_report_default = {
    .status = "sample-token-fail",
    .backend_name = "cpu",
    .segment_name = "embedding-rmsnorm",
    .token_input_status = "fail",
    .runtime_claim = "unsupported",
    .generation = "unsupported",
    .benchmark_status = "not-measured",
    .cleanup_status = "not-needed",
    .exit_code = 1
};

/* Purpose: Project immutable sampling report defaults facts into the typed reporting surface. */
static void sampling_report_defaults(const yvex_sampling_report_request *request,
                                     yvex_sampling_report *report)
{
    *report = sampling_report_default;
    report->model_arg = request ? request->model_arg : NULL;
    report->backend_name = request && request->backend_name
                               ? request->backend_name
                               : "cpu";
    report->segment_name = request && request->segment_name
                               ? request->segment_name
                               : "embedding-rmsnorm";
    sampling_default_summary(&report->summary, report->backend_name);
}

/* Purpose: Publish sampling report set rc only within its admitted destination range. */
static void sampling_report_set_rc(yvex_sampling_report *report, int status)
{
    if (!report) {
        return;
    }
    report->exit_code = yvex_generation_exit_code(status, YVEX_GENERATION_EXIT_SAMPLING);
}

/* Purpose: Copy sampling graph guard copy between compatible admitted ranges without changing semantic identity. */
static void sampling_graph_guard_copy(yvex_sampling_report *report,
                                      const yvex_generation_admission *admission)
{
    if (!report || !admission) {
        return;
    }
    report->graph_guard_rendered = 1;
    report->graph_guard_status = admission->graph_guard_status;
    report->graph_guard_phase = admission->graph_guard_phase;
}

/* Purpose: Project immutable sampling report build facts into the typed reporting surface.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_sampling_report_build(const yvex_sampling_report_request *request,
                               yvex_sampling_report *report,
                               yvex_error *err)
{
    yvex_generation_admission admission;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options sample_options;
    int rc;

    if (!request || !report) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "sampling_report",
                                      "request and report are required");
    }
    memset(report, 0, sizeof(*report));
    memset(&admission, 0, sizeof(admission));
    decode_options = sampling_decode_default;
    logits_options = sampling_logits_default;
    sample_options = sampling_options_default;
    rc = yvex_generation_admission_prepare(
        &admission, request->model_arg, request->tokens_text,
        YVEX_GENERATION_IDENTITY_SAMPLING_ALIAS, err);
    if (rc != YVEX_OK && admission.stage == YVEX_GENERATION_ADMISSION_RESOLVE) return rc;
    sampling_report_defaults(request, report);
    report->input_token_count = admission.token_input.token_count;
    if (rc != YVEX_OK) {
        sampling_report_set_rc(report, rc);
        goto done;
    }
    report->token_input_status = "pass";

    rc = yvex_generation_admission_engine_open(
        &admission, request->backend_name, err);
    if (rc != YVEX_OK) {
        if (admission.stage == YVEX_GENERATION_ADMISSION_GRAPH) {
            sampling_graph_guard_copy(report, &admission);
        }
        sampling_report_set_rc(report, rc);
        goto done;
    }

    decode_options.token_input = &admission.token_input;
    yvex_generation_project_fields(
        &decode_options, request, sampling_request_fields,
        sizeof(sampling_request_fields) / sizeof(sampling_request_fields[0]));
    if (!request->chunk_size_seen) decode_options.chunk_size = 0ull;
    if (!request->layer_count_seen) decode_options.layer_count = 0ull;
    logits_options.decode_options = &decode_options;
    logits_options.logits_count = request->logits_count;
    sample_options.logits_options = &logits_options;
    sample_options.strategy = request->strategy;

    rc = yvex_engine_sample_token(
        admission.engine, &sample_options, &report->summary, err);
    report->input_token_count = admission.token_input.token_count;
    if (rc != YVEX_OK) {
        sampling_report_set_rc(report, rc);
        goto done;
    }

    report->status = "sample-token-created";
    report->real_vocab_sampling = 0;
    report->real_model_sampling = 0;
    report->sampling_ready = 0;
    report->generation_ready = 0;
    report->exit_code = 0;
    yvex_error_clear(err);

done:
    yvex_generation_admission_close(&admission);
    return rc;
}
