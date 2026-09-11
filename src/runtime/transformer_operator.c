/* Bounded operator entrypoint; engine/session computation stays in the runtime owner. */
#include <yvex/internal/transformer.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/core.h>
#include "src/runtime/private.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int transformer_runtime_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "runtime.transformer", reason);
    return status;
}
static int transformer_runtime_cleanup(void **opaque, yvex_error *err)
{
    return yvex_runtime_transformer_context_close(
        (yvex_runtime_transformer_context **)opaque, err);
}
static void transformer_operator_refuse(yvex_transformer_operator_result *result,
                                        const yvex_error *err)
{
    yvex_core_text_copy(result->status, sizeof(result->status), "refused");
    yvex_core_text_copy(result->reason, sizeof(result->reason),
                        err && yvex_error_is_set(err) ? yvex_error_message(err)
                                                     : "transformer execution refused");
}
int yvex_transformer_operator_execute(const yvex_transformer_operator_request *request,
                                      yvex_transformer_operator_result *result,
                                      yvex_runtime_cleanup_lease **retained_cleanup,
                                      yvex_error *err)
{
    yvex_model_engine_open_request model_request = {0};
    yvex_runtime_session_open_request session_request = {0};
    yvex_runtime_transformer_options options = {0};
    yvex_runtime_transformer_request execution_request = {0};
    yvex_runtime_transformer_output output = {0};
    yvex_transformer_input_limits limits = {0};
    yvex_model_engine_failure failure = {0};
    yvex_runtime_cleanup_lease *cleanup = NULL;
    yvex_model_engine *model = NULL;
    yvex_runtime_execution_session *session = NULL;
    yvex_runtime_transformer_context *context = NULL;
    yvex_transformer_input *input = NULL;
    const yvex_model_engine_view *model_view = NULL;
    const yvex_transformer_plan_summary *plan = NULL;
    const yvex_transformer_input_summary *input_summary = NULL;
    yvex_error primary = {0};
    unsigned long long output_count;
    int rc, cleanup_rc, adopted = 0;
    if (result) memset(result, 0, sizeof(*result));
    if (!request || !result || !retained_cleanup || *retained_cleanup ||
        !request->target || !request->artifact_path || !request->runtime_binding_path ||
        !request->input_path || !request->chunk_tokens || !request->context_capacity ||
        (request->backend != YVEX_BACKEND_KIND_CPU &&
         request->backend != YVEX_BACKEND_KIND_CUDA)) {
        rc = transformer_runtime_refuse(err, YVEX_ERR_INVALID_ARG,
                                        "complete transformer operator arguments are required");
        if (result) transformer_operator_refuse(result, err);
        return rc;
    }
    yvex_core_text_copy(result->command, sizeof(result->command),
                        "execute transformer run");
    yvex_core_text_copy(result->target, sizeof(result->target), request->target);
    yvex_core_text_copy(result->backend, sizeof(result->backend),
                        request->backend == YVEX_BACKEND_KIND_CUDA ? "cuda" : "cpu");
    yvex_core_text_copy(result->phase, sizeof(result->phase), "prefill");
    model_request.artifact_path = request->artifact_path;
    model_request.runtime_binding_path = request->runtime_binding_path;
    model_request.target_id = request->target;
    model_request.maximum_host_bytes = request->maximum_host_bytes;
    session_request.backend = request->backend;
    session_request.maximum_host_bytes = request->maximum_host_bytes;
    session_request.maximum_device_bytes = request->maximum_device_bytes;
    rc = yvex_runtime_cleanup_lease_acquire(&cleanup, &model_request, &session_request,
                                            &model, &session, &failure, err);
    limits.maximum_file_bytes = request->maximum_host_bytes
                                    ? request->maximum_host_bytes : 1ull << 30u;
    if (rc == YVEX_OK)
        rc = yvex_transformer_input_open_file(&input, request->input_path, &limits, err);
    options.maximum_host_bytes = request->maximum_host_bytes;
    options.maximum_device_bytes = request->maximum_device_bytes;
    options.context_capacity = request->context_capacity;
    options.cancel_requested = request->cancel_requested;
    options.cancel_context = request->cancel_context;
    if (rc == YVEX_OK)
        rc = yvex_runtime_transformer_context_open(
            &context, model, session, &options, NULL, err);
    if (rc == YVEX_OK) {
        rc = yvex_runtime_cleanup_lease_adopt(cleanup, context,
                                              transformer_runtime_cleanup, err);
        adopted = rc == YVEX_OK;
    }
    model_view = yvex_model_engine_view_get(model);
    plan = yvex_transformer_plan_summary_get(
        yvex_runtime_transformer_context_plan(context));
    input_summary = yvex_transformer_input_summary_get(input);
    if (rc == YVEX_OK &&
        (!model_view || !plan || !input_summary ||
         !yvex_core_u64_mul(input_summary->token_count, plan->hidden_width,
                            &output_count) || output_count > SIZE_MAX / sizeof(float)))
        rc = transformer_runtime_refuse(err, YVEX_ERR_BOUNDS,
                                        "transformer operator output extent overflowed");
    if (rc == YVEX_OK) {
        output.normalized_hidden = (float *)calloc((size_t)output_count, sizeof(float));
        output.capacity = output_count;
        if (!output.normalized_hidden)
            rc = transformer_runtime_refuse(err, YVEX_ERR_NOMEM,
                                            "transformer operator output allocation failed");
    }
    execution_request.backend = request->backend;
    execution_request.chunk_tokens = request->chunk_tokens;
    execution_request.phase = YVEX_TRANSFORMER_PHASE_PREFILL;
    if (rc == YVEX_OK)
        rc = yvex_runtime_transformer_execute(context, input, &execution_request,
                                              &output, &result->execution, err);
    if (rc == YVEX_OK) {
        yvex_core_text_copy(result->family, sizeof(result->family),
                            model_view->target_id);
        yvex_runtime_identity_copy(result->artifact_identity,
                                   model_view->binding->artifact_identity);
        yvex_runtime_identity_copy(result->runtime_binding_identity,
                                   model_view->binding->identity);
        yvex_runtime_identity_copy(result->transformer_plan_identity,
                                   plan->transformer_plan_identity);
        result->hidden_width = plan->hidden_width;
        result->expanded_width = plan->expanded_width;
        result->layer_count = plan->layer_count;
        result->embedding_ready = result->transformer_plan_ready = 1;
        result->transformer_block_ready = result->transformer_stack_ready = 1;
        result->transformer_final_head_ready = result->transformer_final_norm_ready = 1;
        result->transformer_hidden_state_ready = result->full_model_prefill_ready = 1;
        result->transformer_ready = 1;
        result->single_token_transformer_component_ready = input_summary->token_count == 1ull;
    }
    free(output.normalized_hidden);
    yvex_transformer_input_close(&input);
    primary = err ? *err : (yvex_error){0};
    if (!adopted && context) {
        cleanup_rc = yvex_runtime_transformer_context_close(&context, err);
        if (rc == YVEX_OK && cleanup_rc != YVEX_OK) rc = cleanup_rc;
    }
    cleanup_rc = yvex_runtime_cleanup_lease_close(&cleanup, err);
    if (cleanup_rc != YVEX_OK) rc = cleanup_rc;
    else if (rc != YVEX_OK && err) *err = primary;
    if (cleanup) *retained_cleanup = cleanup;
    if (rc == YVEX_OK) {
        result->completed = 1;
        yvex_core_text_copy(result->status, sizeof(result->status), "complete");
        yvex_error_clear(err);
    } else {
        transformer_operator_refuse(result, err);
    }
    return rc;
}
