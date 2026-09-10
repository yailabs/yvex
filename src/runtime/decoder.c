/* Token-forward runner over compiler-owned physical work. Legacy topology is
 * normalized at binding import; no procedural layer or parameter-role execution. */
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/internal/backend.h>
#include <yvex/internal/component.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/core.h>
#include <yvex/internal/decoder_execution.h>
#include <yvex/internal/decoder_plan.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/graph_state.h>
#include <yvex/internal/sequence_mixer.h>
#include <yvex/internal/program_sequence.h>

struct yvex_runtime_decoder_execution_context {
    yvex_model_engine *model;
    yvex_runtime_execution_session *session;
    const yvex_model_engine_view *model_view;
    const yvex_runtime_session_view *session_view;
    const yvex_decoder_plan *plan;
    const yvex_decoder_plan_summary *summary;
    yvex_runtime_decoder_execution_options options;
    const yvex_program_physical *physical;
    yvex_program_device *program_device;
    yvex_program_kernels *program_kernels;
    yvex_program_sequence *program_sequence;
    yvex_program_device_argument *program_arguments;
    const yvex_runtime_decoder_execution_request *program_request;
    yvex_runtime_decoder_execution_result *program_result;
    unsigned long long kernel_host_bytes, kernel_device_bytes;
    yvex_device_tensor *output;
    yvex_device_tensor hidden_publication;
    yvex_execution_device_publication publication;
    unsigned long long host_bytes, device_bytes;
    pthread_mutex_t mutex;
    int mutex_ready, busy, invalidated;
};

typedef struct {
    yvex_runtime_decoder_execution_context *context;
    const yvex_runtime_decoder_execution_request *request;
    yvex_runtime_decoder_execution_result *result;
    yvex_device_tensor normalized;
} decoder_program_run;

static int decoder_refuse(yvex_error *err, yvex_status status, const char *where,
                   const char *reason)
{
    if (!yvex_error_is_set(err)) yvex_error_set(err, status, where, reason);
    return err && yvex_error_is_set(err) ? yvex_error_code(err) : status;
}

static int decoder_bytes_account(
    unsigned long long *total, unsigned long long bytes,
    unsigned long long maximum, const char *where, const char *reason,
    yvex_error *err)
{
    unsigned long long updated;
    if (!total || !yvex_core_u64_add(*total, bytes, &updated) ||
        (maximum && updated > maximum))
        return decoder_refuse(
            err, YVEX_ERR_BOUNDS, where, reason);
    *total = updated;
    return YVEX_OK;
}

static int decoder_buffer_open(yvex_runtime_decoder_execution_context *context,
                               unsigned long long width,
                               yvex_error *err)
{
    yvex_backend_tensor_desc descriptor = {0};
    unsigned long long elements, bytes;
    if (!width || !yvex_core_u64_mul(context->options.token_capacity, width,
                                     &elements) ||
        !yvex_core_u64_mul(elements, sizeof(float), &bytes))
        return decoder_refuse(err, YVEX_ERR_BOUNDS,
                              "runtime.decoder.buffer",
                              "decoder buffer geometry overflowed");
    descriptor.name = "decoder-workspace";
    descriptor.dtype = YVEX_DTYPE_F32;
    descriptor.rank = 2u;
    descriptor.dims[0] = context->options.token_capacity;
    descriptor.dims[1] = width;
    descriptor.bytes = bytes;
    {
        unsigned long long total;
        int rc;
        if (!yvex_core_u64_add(context->device_bytes, bytes, &total) ||
            (context->options.maximum_device_bytes && total > context->options.maximum_device_bytes))
            return decoder_refuse(err, YVEX_ERR_BOUNDS, "runtime.program.output", "output exceeds device budget");
        rc = yvex_backend_tensor_alloc(
            context->session_view->backend, &descriptor,
            &context->output, err);
        if (rc == YVEX_OK)
            rc = decoder_bytes_account(
                &context->device_bytes, bytes,
                context->options.maximum_device_bytes,
                "runtime.decoder.buffer",
                "decoder workspace exceeds the device budget", err);
        if (rc != YVEX_OK && context->output)
            (void)yvex_backend_tensor_release(
                context->session_view->backend, &context->output,
                NULL);
        return rc;
    }
}

static int decoder_physical_invoke(void *opaque, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_runtime_decoder_execution_context *c = opaque;
    const char *name = r->step->implementation;
    int delta = !strcmp(name, "gated_delta.bf16.f32state.v1");
    int attention = !strcmp(name, "gated_causal.bf16.v1");
    unsigned long long started = yvex_core_monotonic_ns();
    int rc;
    if (delta || attention) {
        yvex_program_sequence_request options = {c->program_request->input_identity,
            c->options.cancel_requested, c->options.cancel_context};
        rc = yvex_program_sequence_invoke(c->program_sequence, r, &options, facts, err);
        if (rc == YVEX_OK) {
            c->program_result->recurrent_layers += (unsigned int)delta;
            c->program_result->attention_layers += (unsigned int)attention;
            c->program_result->layers_executed++;
        }
    } else rc = yvex_program_kernels_invoke(c->program_kernels, r, facts, err);
    if (rc == YVEX_OK) {
        if (!strcmp(name, "linear.bf16.f32acc.v1")) c->program_result->linear_operations++;
        if (!strcmp(name, "embedding.bf16.v1"))
            c->program_result->embedding_nanoseconds += yvex_core_monotonic_ns() - started;
        else if (r->step->results[0] == yvex_program_physical_result_at(c->physical, 0u))
            c->program_result->final_nanoseconds += yvex_core_monotonic_ns() - started;
        else c->program_result->layer_nanoseconds += yvex_core_monotonic_ns() - started;
    }
    return rc;
}

static int decoder_physical_account(yvex_runtime_decoder_execution_context *c,
    unsigned long long host, unsigned long long device, yvex_error *err)
{
    unsigned long long h, d;
    if (!yvex_core_u64_add(c->host_bytes, host, &h) || !yvex_core_u64_add(c->device_bytes, device, &d) ||
        (c->options.maximum_host_bytes && h > c->options.maximum_host_bytes) ||
        (c->options.maximum_device_bytes && d > c->options.maximum_device_bytes))
        return decoder_refuse(err, YVEX_ERR_BOUNDS, "runtime.program.admission", "program exceeds resource budget");
    c->host_bytes = h; c->device_bytes = d;
    return YVEX_OK;
}

static int decoder_remaining(yvex_runtime_decoder_execution_context *c, unsigned long long temporary,
    unsigned long long *host, unsigned long long *device, yvex_error *err)
{
    unsigned long long used;
    if (!yvex_core_u64_add(c->host_bytes, temporary, &used) ||
        (c->options.maximum_host_bytes && used >= c->options.maximum_host_bytes) ||
        (c->options.maximum_device_bytes && c->device_bytes >= c->options.maximum_device_bytes))
        return decoder_refuse(err, YVEX_ERR_BOUNDS, "runtime.program.admission", "no resource budget remains");
    *host = c->options.maximum_host_bytes ? c->options.maximum_host_bytes - used : 0u;
    *device = c->options.maximum_device_bytes ? c->options.maximum_device_bytes - c->device_bytes : 0u;
    return YVEX_OK;
}

static int decoder_physical_open(yvex_runtime_decoder_execution_context *c, yvex_error *err)
{
    static const yvex_program_device_kernel implementations[] = {
        {"embedding.bf16.v1", decoder_physical_invoke}, {"linear.bf16.f32acc.v1", decoder_physical_invoke},
        {"rms_norm.bf16.v1", decoder_physical_invoke}, {"silu_product.bf16.v1", decoder_physical_invoke},
        {"add.bf16.v1", decoder_physical_invoke}, {"gated_delta.bf16.f32state.v1", decoder_physical_invoke},
        {"gated_causal.bf16.v1", decoder_physical_invoke}};
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(c->physical);
    const yvex_program_physical_value *output = yvex_program_physical_value_at(c->physical,
        yvex_program_physical_result_at(c->physical, 0u));
    unsigned long long host, device, temporary = 0u;
    const yvex_runtime_descriptor_summary *descriptor = yvex_runtime_descriptor_summary_get(c->model_view->descriptor);
    yvex_program_kernel_parameter *parameters = NULL;
    size_t i;
    int rc;
    if (!s || s->input_count < 2u || !output || output->type.kind != YVEX_IR_TENSOR || output->type.rank != 2u ||
        output->type.shape[1].extent != c->summary->hidden_width ||
        yvex_program_physical_value_at(c->physical, 0u)->type.scalar != YVEX_IR_INDEX ||
        yvex_program_physical_value_at(c->physical, 1u)->type.kind != YVEX_IR_SCALAR)
        return decoder_refuse(err, YVEX_ERR_FORMAT, "runtime.program.runner", "incompatible forward signature");
    rc = decoder_physical_account(c, sizeof(*c) + s->input_count * sizeof(*c->program_arguments), 0u, err);
    if (rc != YVEX_OK) return rc;
    c->program_arguments = calloc(s->input_count, sizeof(*c->program_arguments));
    if (!c->program_arguments)
        return decoder_refuse(err, YVEX_ERR_NOMEM, "runtime.program.runner", "argument allocation failed");
    for (i = 2u; i < s->input_count; ++i) {
        if (yvex_program_physical_value_at(c->physical, i)->type.kind != YVEX_IR_STATE)
            return decoder_refuse(err, YVEX_ERR_UNSUPPORTED, "runtime.program.runner", "unsupported runner input");
        c->program_arguments[i].state_handle = i;
    }
    rc = decoder_buffer_open(c, output->type.shape[1].extent, err);
    if (rc == YVEX_OK && (!descriptor ||
        !yvex_core_u64_mul(descriptor->tensor_count, sizeof(*parameters), &temporary) || temporary > SIZE_MAX))
        rc = decoder_refuse(err, YVEX_ERR_BOUNDS, "runtime.program.parameters", "binding directory exceeds host");
    if (rc == YVEX_OK) rc = decoder_remaining(c, temporary, &host, &device, err);
    if (rc == YVEX_OK) {
        parameters = calloc((size_t)descriptor->tensor_count, sizeof(*parameters));
        if (!parameters)
            rc = decoder_refuse(err, YVEX_ERR_NOMEM, "runtime.program.parameters", "directory allocation failed");
    }
    for (i = 0u; rc == YVEX_OK && i < descriptor->tensor_count; ++i) {
        const yvex_runtime_tensor_binding *row = yvex_runtime_descriptor_tensor_at(c->model_view->descriptor, i);
        const yvex_materialized_tensor_binding *b = row ? row->binding : NULL;
        const unsigned char *encoded = NULL;
        unsigned long long bytes;
        if (!b) {
            rc = decoder_refuse(err, YVEX_ERR_FORMAT, "runtime.program.parameters", "missing tensor binding");
            break;
        }
        rc = yvex_runtime_residency_binding_view(c->model_view->residency, b, &encoded, &bytes, err);
        if (rc == YVEX_OK && (!b->row_count || bytes != b->encoded_bytes || bytes % b->row_count))
            rc = decoder_refuse(err, YVEX_ERR_FORMAT, "runtime.program.parameters", "inexact parameter rows");
        if (rc == YVEX_OK) parameters[i] = (yvex_program_kernel_parameter){row->tensor_id,
            {.encoded = encoded, .encoded_bytes = bytes, .row_count = b->row_count,
             .row_width = b->row_width, .row_bytes = bytes / b->row_count, .qtype = b->qtype}};
    }
    if (rc == YVEX_OK) rc = yvex_program_kernels_open(&c->program_kernels, c->physical,
        parameters, descriptor->tensor_count, c->session_view->backend, host, device, err);
    free(parameters);
    if (rc == YVEX_OK) {
        yvex_program_kernels_resources(c->program_kernels, &c->kernel_host_bytes, &c->kernel_device_bytes);
        rc = decoder_physical_account(c, c->kernel_host_bytes, c->kernel_device_bytes, err);
    }
    if (rc == YVEX_OK) rc = decoder_remaining(c, 0u, &host, &device, err);
    if (rc == YVEX_OK) rc = yvex_program_sequence_open(&c->program_sequence, c->physical, c->model_view,
        c->session_view, c->program_kernels, c->options.token_capacity, host, device, err);
    if (rc == YVEX_OK) {
        yvex_program_sequence_resources(c->program_sequence, &host, &device);
        rc = decoder_physical_account(c, host, device, err);
    }
    if (rc == YVEX_OK) rc = decoder_remaining(c, 0u, &host, &device, err);
    if (rc == YVEX_OK) rc = yvex_program_device_open(&c->program_device, c->physical, c->session_view->backend,
        c->options.token_capacity, host, device,
        implementations, sizeof(implementations) / sizeof(implementations[0]), c, err);
    if (rc == YVEX_OK) {
        yvex_program_device_resources(c->program_device, &host, &device);
        rc = decoder_physical_account(c, host, device, err);
    }
    return rc;
}

static int decoder_program_prepare(yvex_runtime_decoder_execution_context *context,
                                    unsigned long long rows, yvex_error *err)
{
    unsigned long long host, device, remaining_host, remaining_device;
    int rc, accounted;
    context->host_bytes -= context->kernel_host_bytes;
    context->device_bytes -= context->kernel_device_bytes;
    rc = decoder_remaining(context, 0u, &remaining_host, &remaining_device, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_prepare(context->program_kernels, rows,
        remaining_host, remaining_device, err);
    /* Preparation can fail after retaining earlier admitted plans. Account
     * the actual owner on both success and failure, before a possible retry. */
    yvex_program_kernels_resources(context->program_kernels, &host, &device);
    context->kernel_host_bytes = host;
    context->kernel_device_bytes = device;
    accounted = decoder_physical_account(context, host, device, err);
    return rc == YVEX_OK ? accounted : rc;
}

int yvex_runtime_decoder_execution_context_open(
    yvex_runtime_decoder_execution_context **out, yvex_model_engine *model,
    yvex_runtime_execution_session *session,
    const yvex_runtime_decoder_execution_options *options, yvex_error *err)
{
    yvex_runtime_decoder_execution_context *context;
    int rc = YVEX_OK;
    if (out) *out = NULL;
    if (!out || !model || !session || !options || !options->context_capacity ||
        !options->token_capacity ||
        options->token_capacity > options->context_capacity ||
        !options->execution_profile)
        return decoder_refuse(err, YVEX_ERR_INVALID_ARG,
                              "runtime.decoder.open",
                              "decoder model, session, capacity, and execution profile are required");
    context = calloc(1u, sizeof(*context));
    if (!context)
        return decoder_refuse(err, YVEX_ERR_NOMEM, "runtime.decoder.open",
                              "decoder context allocation failed");
    context->model = model;
    context->session = session;
    context->model_view = yvex_model_engine_view_get(model);
    context->session_view = yvex_runtime_session_view_get(session);
    context->options = *options;
    context->plan = context->model_view ? context->model_view->decoder : NULL;
    context->summary = yvex_decoder_plan_summary_get(context->plan);
    context->physical = context->model_view ?
        yvex_compiled_model_plan_forward(context->model_view->compiled_plan) : NULL;
    if (!context->model_view || !context->session_view ||
        context->session_view->engine != model || !context->summary || !context->physical ||
        context->summary->maximum_context < options->context_capacity ||
        yvex_backend_kind_of(context->session_view->backend) !=
            YVEX_BACKEND_KIND_CUDA ||
        !context->session_view->sequence_state ||
        pthread_mutex_init(&context->mutex, NULL) != 0)
        rc = decoder_refuse(err, YVEX_ERR_STATE, "runtime.decoder.open",
                            "admitted CUDA decoder resources are unavailable");
    else
        context->mutex_ready = 1;
    if (rc == YVEX_OK) rc = decoder_physical_open(context, err);
    if (rc != YVEX_OK) {
        (void)yvex_runtime_decoder_execution_context_close(&context, NULL);
        *out = context;
        return rc;
    }
    *out = context;
    yvex_error_clear(err);
    return YVEX_OK;
}

const yvex_decoder_plan *yvex_runtime_decoder_execution_plan(
    const yvex_runtime_decoder_execution_context *context)
{
    return context ? context->plan : NULL;
}

int yvex_runtime_decoder_execution_context_close(
    yvex_runtime_decoder_execution_context **context_ptr, yvex_error *err)
{
    yvex_runtime_decoder_execution_context *context;
    yvex_backend *backend;
    int rc = YVEX_OK;
    if (!context_ptr || !*context_ptr) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    context = *context_ptr;
    if (context->mutex_ready && pthread_mutex_lock(&context->mutex) == 0) {
        if (context->busy) {
            (void)pthread_mutex_unlock(&context->mutex);
            return decoder_refuse(err, YVEX_ERR_STATE,
                                  "runtime.decoder.close",
                                  "busy decoder context cannot close");
        }
        (void)pthread_mutex_unlock(&context->mutex);
    }
    yvex_execution_device_publication_retire(&context->publication);
    backend = context->session_view ? context->session_view->backend : NULL;
    rc = yvex_program_device_close(&context->program_device, err);
    if (rc == YVEX_OK) rc = yvex_program_sequence_close(&context->program_sequence, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_close(&context->program_kernels, err);
    if (rc != YVEX_OK) return rc;
    if (context->output && yvex_backend_tensor_release(backend, &context->output, err) != YVEX_OK)
        return yvex_error_code(err);
    free(context->program_arguments);
    if (context->mutex_ready) (void)pthread_mutex_destroy(&context->mutex);
    memset(context, 0, sizeof(*context));
    free(context);
    *context_ptr = NULL;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int decoder_tensor_view(yvex_device_tensor *source, unsigned long long count,
                        unsigned long long rows, unsigned long long columns,
                        yvex_device_tensor *out)
{
    unsigned long long expected;
    if (!source || !out || !rows || !columns ||
        !yvex_core_u64_mul(rows, columns, &expected) || expected != count ||
        !yvex_backend_tensor_f32_subview(source, 0ull, count, out))
        return 0;
    out->rank = 2u;
    out->dims[0] = rows;
    out->dims[1] = columns;
    return 1;
}

static int decoder_result_facts_add(yvex_runtime_decoder_execution_result *result,
                             const yvex_backend_operation_facts *facts,
                             yvex_error *err)
{
    if (!result || !facts ||
        !yvex_core_u64_add(result->kernel_launches, facts->kernel_launches,
                           &result->kernel_launches) ||
        !yvex_core_u64_add(result->h2d_bytes, facts->h2d_bytes,
                           &result->h2d_bytes) ||
        !yvex_core_u64_add(result->d2h_bytes, facts->d2h_bytes,
                           &result->d2h_bytes) ||
        !yvex_core_u64_add(result->d2d_bytes, facts->d2d_bytes,
                           &result->d2d_bytes) ||
        !yvex_core_u64_add(result->accelerated_matrix_operations,
                           facts->accelerated_matrix_launches,
                           &result->accelerated_matrix_operations))
        return decoder_refuse(err, YVEX_ERR_BOUNDS,
                              "runtime.decoder.operation-facts",
                              "decoder execution accounting overflowed");
    return YVEX_OK;
}

static int decoder_enter(yvex_runtime_decoder_execution_context *context,
                         yvex_error *err)
{
    int rc;
    if (!context || !context->mutex_ready ||
        pthread_mutex_lock(&context->mutex) != 0)
        return decoder_refuse(err, YVEX_ERR_STATE, "runtime.decoder.enter",
                              "synchronized decoder context is required");
    if (context->busy || context->invalidated) {
        (void)pthread_mutex_unlock(&context->mutex);
        return decoder_refuse(err, YVEX_ERR_STATE, "runtime.decoder.enter",
                              "decoder context is busy or invalidated");
    }
    rc = yvex_execution_device_publication_begin(&context->publication, err);
    if (rc != YVEX_OK) {
        (void)pthread_mutex_unlock(&context->mutex);
        return rc;
    }
    context->busy = 1;
    (void)pthread_mutex_unlock(&context->mutex);
    return YVEX_OK;
}

static void decoder_leave(yvex_runtime_decoder_execution_context *context)
{
    yvex_runtime_session_summary session = {0};
    int invalidated =
        context &&
        yvex_runtime_session_summary_copy(context->session, &session, NULL) ==
            YVEX_OK &&
        session.invalidated;
    if (!context || !context->mutex_ready ||
        pthread_mutex_lock(&context->mutex) != 0)
        return;
    context->invalidated |= invalidated;
    context->busy = 0;
    (void)pthread_mutex_unlock(&context->mutex);
}

static int decoder_state_summary(
    const yvex_runtime_decoder_execution_context *context,
    yvex_graph_attention_state_summary *attention,
    yvex_sequence_state_summary *sequence, yvex_error *err)
{
    const yvex_attention_state_provider *provider =
        context && context->session_view
            ? context->session_view->attention_state_provider
            : NULL;
    if (!provider || !provider->summary ||
        provider->summary(provider->context, attention, err) != YVEX_OK ||
        yvex_sequence_state_summary_copy(context->session_view->sequence_state,
                                         sequence, err) != YVEX_OK)
        return decoder_refuse(err, YVEX_ERR_STATE, "runtime.decoder.state",
                              "mixed decoder sequence state is unavailable");
    return YVEX_OK;
}

static int decoder_prepare_attention_state(
    yvex_runtime_decoder_execution_context *context,
    const yvex_runtime_decoder_execution_request *request,
    yvex_graph_attention_state_summary *attention,
    yvex_sequence_state_summary *sequence, yvex_error *err)
{
    yvex_graph_attention_capacity_request capacity_request = {0};
    yvex_graph_attention_capacity_plan *capacity = NULL;
    yvex_attention_failure failure = {0};
    int rc;

    if (!context || !request || !attention || !sequence)
        return decoder_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decoder.state-prepare",
            "decoder state preparation requires complete execution facts");
    if (attention->prepared_layer_count) return YVEX_OK;
    if (request->token_start)
        return decoder_refuse(
            err, YVEX_ERR_STATE, "runtime.decoder.state-prepare",
            "nonzero decoder start requires committed attention state");

    capacity_request.scope = YVEX_ATTENTION_PROBE_SCOPE_FULL;
    capacity_request.token_count = context->options.context_capacity;
    capacity_request.execution_count = 1ull;
    capacity_request.use_requested_position = 1;
    rc = yvex_graph_attention_capacity_plan_build(
        &capacity, context->model_view->attention, &capacity_request, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_prepare_attention_scope_state(
            context->session, context->model, YVEX_TENSOR_SCOPE_GLOBAL,
            capacity, &failure, err);
    yvex_graph_attention_capacity_plan_close(&capacity);
    if (rc == YVEX_OK)
        rc = decoder_state_summary(context, attention, sequence, err);
    return rc;
}

static int decoder_request_validate(
    const yvex_runtime_decoder_execution_context *context,
    const yvex_runtime_decoder_execution_request *request,
    const yvex_graph_attention_state_summary *attention,
    const yvex_sequence_state_summary *sequence, yvex_error *err)
{
    unsigned long long index, end;
    if (!context || !request || !request->token_ids || !request->token_count ||
        request->token_count > context->options.token_capacity ||
        !yvex_sha256_hex_valid(request->input_identity) ||
        !yvex_core_u64_add(request->token_start, request->token_count, &end) ||
        end > context->options.context_capacity ||
        !attention || attention->transaction_active ||
        attention->prepared_layer_count != attention->layer_count ||
        !attention->position_consistent ||
        attention->next_position != request->token_start ||
        attention->capacity < end ||
        !sequence || sequence->transaction_active ||
        sequence->committed_position != request->token_start)
        return decoder_refuse(
            err, YVEX_ERR_STATE, "runtime.decoder.request",
            "decoder input must extend the exact committed mixed-state position");
    for (index = 0ull; index < request->token_count; ++index)
        if ((unsigned long long)request->token_ids[index] >=
            context->summary->vocabulary_size)
            return decoder_refuse(err, YVEX_ERR_BOUNDS,
                                  "runtime.decoder.token",
                                  "decoder token exceeds the admitted vocabulary");
    return YVEX_OK;
}

static int decoder_persistent_identity(
    const yvex_graph_attention_state_summary *attention,
    const yvex_sequence_state_summary *sequence,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!attention || !sequence || !output ||
        !yvex_sha256_hex_valid(attention->state_content_identity) ||
        !yvex_sha256_hex_valid(sequence->plan_identity) ||
        !yvex_sha256_update_text(
            &hash, "yvex.runtime.decoder.mixed-state.v1") ||
        !yvex_sha256_update_text(&hash, attention->state_content_identity) ||
        !yvex_sha256_update_text(&hash, sequence->plan_identity) ||
        !yvex_sha256_update_u64(&hash, attention->generation) ||
        !yvex_sha256_update_u64(&hash, sequence->generation) ||
        !yvex_sha256_update_u64(&hash, attention->next_position) ||
        !yvex_sha256_update_u64(&hash, sequence->committed_position) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int decoder_execution_identity(
    const yvex_runtime_decoder_execution_result *result,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!result || !output ||
        !yvex_sha256_hex_valid(result->decoder_plan_identity) ||
        !yvex_sha256_hex_valid(result->input_identity) ||
        !yvex_sha256_hex_valid(result->persistent_state_identity) ||
        !yvex_sha256_hex_valid(result->normalized_hidden_digest) ||
        !yvex_sha256_update_text(
            &hash, "yvex.runtime.decoder.execution.v1") ||
        !yvex_sha256_update_text(&hash, result->decoder_plan_identity) ||
        !yvex_sha256_update_text(&hash, result->input_identity) ||
        !yvex_sha256_update_text(&hash, result->persistent_state_identity) ||
        !yvex_sha256_update_text(&hash, result->normalized_hidden_digest) ||
        !yvex_sha256_update_u64(&hash, result->token_start) ||
        !yvex_sha256_update_u64(&hash, result->token_count) ||
        !yvex_sha256_update_u64(&hash, result->layers_executed) ||
        !yvex_sha256_update_u64(&hash, result->linear_operations) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int decoder_hidden_digest(
    const yvex_runtime_decoder_execution_result *result,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!result || !output ||
        !yvex_sha256_hex_valid(result->decoder_plan_identity) ||
        !yvex_sha256_hex_valid(result->persistent_state_identity) ||
        !yvex_sha256_update_text(
            &hash, "yvex.decoder.device-normalized-hidden.v1") ||
        !yvex_sha256_update_text(&hash, result->decoder_plan_identity) ||
        !yvex_sha256_update_text(&hash, result->persistent_state_identity) ||
        !yvex_sha256_update_u64(&hash, result->token_start) ||
        !yvex_sha256_update_u64(&hash, result->token_count) ||
        !yvex_sha256_update_u64(&hash, result->position_after) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int decoder_publish_result(
    decoder_program_run *run,
    const yvex_graph_attention_state_summary *attention,
    const yvex_sequence_state_summary *sequence, yvex_error *err)
{
    yvex_runtime_decoder_execution_context *context = run->context;
    yvex_runtime_decoder_execution_result *result = run->result;
    unsigned long long rows = run->request->token_count;
    unsigned long long width = context->summary->hidden_width;
    int rc;
    context->hidden_publication = run->normalized;
    result->schema_version = YVEX_RUNTIME_DECODER_EXECUTION_SCHEMA_V1;
    result->token_start = run->request->token_start;
    result->token_count = rows;
    result->position_after = run->request->token_start + rows;
    result->convolution_state_bytes = sequence->convolution_state_bytes;
    result->recurrent_state_bytes = sequence->recurrent_state_bytes;
    yvex_runtime_identity_copy(result->decoder_plan_identity,
                               context->summary->decoder_plan_identity);
    yvex_runtime_identity_copy(result->input_identity,
                               run->request->input_identity);
    if (!decoder_persistent_identity(attention, sequence,
                                     result->persistent_state_identity))
        return decoder_refuse(err, YVEX_ERR_STATE,
                              "runtime.decoder.result",
                              "mixed sequence-state identity derivation failed");
    if (!decoder_hidden_digest(result, result->normalized_hidden_digest))
        return decoder_refuse(
            err, YVEX_ERR_STATE, "runtime.decoder.result",
            "decoder normalized-hidden digest derivation failed");
    rc = yvex_runtime_device_view_bind(
        &result->device_hidden, YVEX_EXECUTION_DEVICE_HIDDEN, context->model,
        context->session, context->session_view->attention_state_provider,
        context->options.execution_profile, &context->hidden_publication,
        &context->publication, 0ull,
        rows, width, err);
    if (rc == YVEX_OK &&
        !decoder_execution_identity(result, result->execution_identity))
        rc = decoder_refuse(err, YVEX_ERR_STATE, "runtime.decoder.result",
                            "decoder execution identity derivation failed");
    if (rc == YVEX_OK) result->completed = 1;
    return rc;
}

static int decoder_physical_run(decoder_program_run *run, yvex_error *err)
{
    yvex_runtime_decoder_execution_context *c = run->context;
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(c->physical);
    yvex_device_tensor *output[] = {&run->normalized};
    yvex_program_device_result result = {0};
    unsigned long long rows = run->request->token_count, width = c->summary->hidden_width;
    int rc;
    if (!decoder_tensor_view(c->output, rows * width, rows, width, &run->normalized))
        return decoder_refuse(err, YVEX_ERR_BOUNDS, "runtime.program.result", "runner output exceeds admitted storage");
    c->program_arguments[0].indices = run->request->token_ids;
    c->program_arguments[1].index = run->request->token_start;
    c->program_request = run->request;
    c->program_result = run->result;
    rc = yvex_program_device_run(c->program_device, rows, c->program_arguments, s->input_count, output, 1u,
        c->options.cancel_requested, c->options.cancel_context, &result, err);
    c->program_request = NULL;
    c->program_result = NULL;
    if (rc == YVEX_OK) rc = decoder_result_facts_add(run->result, &result.backend, err);
    return rc;
}

static int decoder_execute_locked(
    yvex_runtime_decoder_execution_context *context,
    const yvex_runtime_decoder_execution_request *request,
    yvex_runtime_decoder_execution_result *result, yvex_error *err)
{
    yvex_graph_attention_state_summary attention_before = {0};
    yvex_graph_attention_state_summary attention_after = {0};
    yvex_sequence_state_summary sequence_before = {0}, sequence_after = {0};
    yvex_model_engine_failure failure = {0};
    decoder_program_run run = {0};
    int session_owned = 0, rc;

    rc = decoder_state_summary(context, &attention_before, &sequence_before,
                               err);
    if (rc == YVEX_OK)
        rc = decoder_prepare_attention_state(
            context, request, &attention_before, &sequence_before, err);
    if (rc == YVEX_OK)
        rc = decoder_request_validate(context, request, &attention_before,
                                      &sequence_before, err);
    if (rc == YVEX_OK) rc = decoder_program_prepare(context, request->token_count, err);
    if (rc == YVEX_OK) {
        rc = yvex_runtime_session_begin(context->session, &failure, err);
        session_owned = rc == YVEX_OK;
    }
    if (rc == YVEX_OK)
        rc = yvex_sequence_state_begin(context->session_view->sequence_state,
                                       request->token_start,
                                       request->token_count, err);
    run.context = context;
    run.request = request;
    run.result = result;
    if (rc == YVEX_OK) rc = decoder_physical_run(&run, err);
    if (session_owned)
        rc = yvex_runtime_session_finish_coordinated(
            context->session, rc, NULL, 0u, err);
    if (rc == YVEX_OK)
        rc = decoder_state_summary(context, &attention_after, &sequence_after,
                                   err);
    if (rc == YVEX_OK &&
        (attention_after.next_position !=
             request->token_start + request->token_count ||
         sequence_after.committed_position != attention_after.next_position ||
         result->layers_executed != context->summary->layer_count ||
         result->attention_layers != context->summary->attention_layer_count ||
         result->recurrent_layers != context->summary->recurrent_layer_count))
        rc = decoder_refuse(err, YVEX_ERR_STATE, "runtime.decoder.commit",
                            "decoder mixed-state publication is incomplete");
    if (rc == YVEX_OK)
        rc = decoder_publish_result(&run, &attention_after, &sequence_after,
                                    err);
    return rc;
}

int yvex_runtime_decoder_execution_execute(
    yvex_runtime_decoder_execution_context *context,
    const yvex_runtime_decoder_execution_request *request,
    yvex_runtime_decoder_execution_result *result, yvex_error *err)
{
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!context || !request || !result)
        return decoder_refuse(err, YVEX_ERR_INVALID_ARG,
                              "runtime.decoder.execute",
                              "decoder context, request, and result are required");
    rc = decoder_enter(context, err);
    if (rc != YVEX_OK) return rc;
    rc = decoder_execute_locked(context, request, result, err);
    decoder_leave(context);
    return rc;
}
