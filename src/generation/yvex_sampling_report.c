/*
 * yvex_sampling_report.c - typed sampling report construction.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   diagnostic sampling report construction, model reference resolution,
 *   registered identity gate, token input validation, graph guard preflight,
 *   engine open/close orchestration, and sampler invocation facts.
 *
 * Does not own:
 *   CLI input grammar, command dispatch, rendering, stdout/stderr output,
 *   stochastic sampling, token append, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   report builders populate typed facts and never print operator output.
 *
 * Boundary:
 *   this report proves only the existing bounded diagnostic greedy sampler.
 */
#include "yvex_sampling_private.h"

#include <string.h>

static int sampling_exit_for_status(int status)
{
    switch (status) {
    case YVEX_OK:
        return 0;
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
    case YVEX_ERR_NOMEM:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

static void sampling_default_summary(yvex_sampling_summary *summary,
                                     const char *backend_name)
{
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options options;

    memset(&decode_options, 0, sizeof(decode_options));
    memset(&logits_options, 0, sizeof(logits_options));
    memset(&options, 0, sizeof(options));
    decode_options.backend_name = backend_name ? backend_name : "cpu";
    decode_options.segment_name = "embedding-rmsnorm";
    logits_options.decode_options = &decode_options;
    logits_options.logits_count = 16ull;
    options.logits_options = &logits_options;
    options.strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
    sample_summary_defaults(summary, &options);
}

static void sampling_report_defaults(const yvex_sampling_report_request *request,
                                     yvex_sampling_report *report)
{
    memset(report, 0, sizeof(*report));
    report->status = "sample-token-fail";
    report->model_arg = request ? request->model_arg : NULL;
    report->backend_name = request && request->backend_name
                               ? request->backend_name
                               : "cpu";
    report->segment_name = request && request->segment_name
                               ? request->segment_name
                               : "embedding-rmsnorm";
    report->token_input_status = "fail";
    report->runtime_claim = "unsupported";
    report->generation = "unsupported";
    report->benchmark_status = "not-measured";
    report->cleanup_status = "not-needed";
    report->exit_code = 1;
    sampling_default_summary(&report->summary, report->backend_name);
}

static void sampling_report_set_rc(yvex_sampling_report *report, int status)
{
    if (!report) {
        return;
    }
    report->exit_code = sampling_exit_for_status(status);
}

static void sampling_graph_guard_copy(yvex_sampling_report *report,
                                      const yvex_cli_graph_guard_report *guard)
{
    if (!report || !guard) {
        return;
    }
    report->graph_guard_rendered = 1;
    report->graph_guard_status = guard->guard_status;
    report->graph_guard_phase = guard->phase;
}

static int sampling_identity_gate(const yvex_model_ref *ref, yvex_error *err)
{
    yvex_artifact_file_identity identity;
    int rc;

    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) {
        return YVEX_OK;
    }
    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(ref->path, &identity, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!ref->sha256 || !ref->sha256[0] ||
        !yvex_sha256_hex_is_valid(ref->sha256)) {
        yvex_error_set(err, YVEX_ERR_STATE, "sampling_identity",
                       "registered alias lacks digest identity; re-add model");
        return YVEX_ERR_STATE;
    }
    if (strcmp(ref->sha256, identity.sha256) != 0 ||
        (ref->registered_file_size != 0ull &&
         ref->registered_file_size != identity.file_size)) {
        yvex_error_set(err, YVEX_ERR_STATE, "sampling_identity",
                       "digest mismatch for registered alias");
        return YVEX_ERR_STATE;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_sampling_report_build(const yvex_sampling_report_request *request,
                               yvex_sampling_report *report,
                               yvex_error *err)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options sample_options;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    unsigned long long vocab_size = 0ull;
    int rc;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "sampling_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&decode_options, 0, sizeof(decode_options));
    memset(&logits_options, 0, sizeof(logits_options));
    memset(&sample_options, 0, sizeof(sample_options));
    memset(&graph_guard, 0, sizeof(graph_guard));

    rc = yvex_model_ref_resolve(&model_ref, request->model_arg, NULL, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    sampling_report_defaults(request, report);

    rc = sampling_identity_gate(&model_ref, err);
    if (rc != YVEX_OK) {
        sampling_report_set_rc(report, rc);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    rc = yvex_token_input_parse_explicit(request->tokens_text, &token_input, err);
    if (rc == YVEX_OK) {
        rc = cli_token_input_vocab_from_model(model_ref.path, &vocab_size, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, vocab_size, err);
    }
    report->input_token_count = token_input.token_count;
    if (rc != YVEX_OK) {
        sampling_report_set_rc(report, rc);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }
    report->token_input_status = "pass";

    rc = preflight_graph_guard(&model_ref,
                               request->backend_name,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               err);
    if (rc != YVEX_OK) {
        sampling_graph_guard_copy(report, &graph_guard);
        sampling_report_set_rc(report, rc);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = request->backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, err);
    if (rc != YVEX_OK) {
        sampling_report_set_rc(report, rc);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    decode_options.token_input = &token_input;
    decode_options.segment_name = request->segment_name;
    decode_options.backend_name = request->backend_name;
    decode_options.position_start = request->position_start;
    decode_options.chunk_size =
        request->chunk_size_seen ? request->chunk_size : 0ull;
    decode_options.context_length = request->context_length;
    decode_options.attach_kv = request->attach_kv;
    decode_options.kv_shape = request->kv_shape;
    decode_options.layer_count =
        request->layer_count_seen ? request->layer_count : 0ull;
    decode_options.layer_hidden_dim = request->layer_hidden_dim;
    decode_options.layer_head_dim = request->layer_head_dim;
    decode_options.layer_ffn_dim = request->layer_ffn_dim;
    logits_options.decode_options = &decode_options;
    logits_options.logits_count = request->logits_count;
    sample_options.logits_options = &logits_options;
    sample_options.strategy = request->strategy;

    rc = yvex_engine_sample_token(engine, &sample_options, &report->summary, err);
    report->input_token_count = token_input.token_count;
    if (rc != YVEX_OK) {
        sampling_report_set_rc(report, rc);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    report->status = "sample-token-created";
    report->real_vocab_sampling = 0;
    report->real_model_sampling = 0;
    report->sampling_ready = 0;
    report->generation_ready = 0;
    report->exit_code = 0;
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    yvex_error_clear(err);
    return YVEX_OK;
}
