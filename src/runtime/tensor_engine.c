/* Own one admitted tensor source, compiled binding, physical stage and backend. */
#include <yvex/internal/tensor_engine.h>
#include <yvex/internal/tensor_parameters.h>
#include <yvex/internal/tensor_source.h>
#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct yvex_tensor_engine {
    pthread_mutex_t mutex;
    int mutex_ready, closing;
    yvex_tensor_source *source;
    yvex_tensor_binding *binding;
    yvex_tensor_parameters *parameters;
    yvex_backend *backend;
    yvex_program_stage *stage;
    yvex_tensor_engine_summary summary;
};

static int engine_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "runtime.tensor-engine", reason);
    return code;
}

static int engine_release(yvex_tensor_engine *engine, yvex_error *err)
{
    int rc = YVEX_OK;
    if (engine->stage) rc = yvex_program_stage_close(&engine->stage, err);
    if (rc == YVEX_OK && engine->backend)
        rc = yvex_backend_close_checked(&engine->backend, err);
    if (rc != YVEX_OK) return rc;
    yvex_tensor_parameters_close(&engine->parameters);
    yvex_tensor_binding_close(&engine->binding);
    yvex_tensor_source_close(&engine->source);
    if (engine->mutex_ready) pthread_mutex_destroy(&engine->mutex);
    free(engine);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_tensor_engine_open(yvex_tensor_engine **out,
    const yvex_tensor_engine_options *options, yvex_tensor_engine_summary *summary,
    yvex_error *err)
{
    if (out) *out = NULL;
    if (summary) memset(summary, 0, sizeof(*summary));
    if (!out || !summary || !options ||
        options->schema_version != YVEX_TENSOR_ENGINE_SCHEMA_V1 ||
        !options->source_path || !*options->source_path ||
        !options->binding_path || !*options->binding_path ||
        !options->generation || !options->maximum_rows ||
        !options->maximum_host_bytes)
        return engine_refuse(err, YVEX_ERR_INVALID_ARG, "bounded source, binding and generation required");
    /* The admitted F16 source reader is currently a host parameter provider.
     * Do not pretend the same binding is CUDA-resident until that owner exists. */
    if (options->backend != YVEX_BACKEND_KIND_CPU)
        return engine_refuse(err, YVEX_ERR_UNSUPPORTED, "tensor source is admitted for CPU execution only");
    yvex_tensor_engine *engine = calloc(1u, sizeof(*engine));
    if (!engine) return engine_refuse(err, YVEX_ERR_NOMEM, "engine allocation failed");
    int rc = YVEX_OK;
    if (pthread_mutex_init(&engine->mutex, NULL) != 0)
        rc = engine_refuse(err, YVEX_ERR_STATE, "engine mutex initialization failed");
    else engine->mutex_ready = 1;
    if (rc == YVEX_OK) rc = yvex_tensor_binding_open(&engine->binding, options->binding_path, err);
    const yvex_tensor_binding_summary *bound = rc == YVEX_OK ?
        yvex_tensor_binding_summary_get(engine->binding) : NULL;
    yvex_tensor_source_request source_request = {.schema_version = YVEX_TENSOR_SOURCE_SCHEMA_V1,
        .file_path = options->source_path,
        .expected_sha256 = bound ? bound->source_identity : NULL,
        .expected_tensor_count = bound ? bound->source_tensor_count : 0u};
    if (rc == YVEX_OK) rc = yvex_tensor_source_open(&engine->source, &source_request, err);
    const yvex_tensor_source_summary *source = rc == YVEX_OK ?
        yvex_tensor_source_summary_get(engine->source) : NULL;
    if (rc == YVEX_OK && (source->source_bytes != bound->source_bytes ||
        source->tensor_count != bound->source_tensor_count ||
        strcmp(source->source_identity, bound->source_identity)))
        rc = engine_refuse(err, YVEX_ERR_FORMAT, "source differs from authenticated binding");
    if (rc == YVEX_OK) rc = yvex_tensor_parameters_open(&engine->parameters, engine->source,
        yvex_tensor_binding_program(engine->binding), (size_t)bound->parameter_count,
        yvex_tensor_binding_parameter_name, engine->binding, err);
    size_t parameter_count = 0u;
    const yvex_program_kernel_parameter *parameters = rc == YVEX_OK ?
        yvex_tensor_parameters_view(engine->parameters, &parameter_count) : NULL;
    if (rc == YVEX_OK) rc = yvex_backend_open_cpu(&engine->backend, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&engine->stage,
        yvex_tensor_binding_program(engine->binding), parameters, parameter_count,
        engine->backend, options->maximum_rows, 1, options->maximum_host_bytes,
        options->maximum_device_bytes, err);
    if (rc != YVEX_OK) {
        yvex_error primary = err ? *err : (yvex_error){0};
        yvex_error cleanup = {0};
        int cleanup_rc = engine_release(engine, &cleanup);
        if (cleanup_rc != YVEX_OK) {
            *out = engine;
            if (err) *err = cleanup;
            return cleanup_rc;
        }
        if (err) *err = primary;
        return rc;
    }
    engine->summary.schema_version = YVEX_TENSOR_ENGINE_SCHEMA_V1;
    engine->summary.backend = options->backend;
    engine->summary.generation = options->generation;
    engine->summary.maximum_rows = options->maximum_rows;
    engine->summary.input_format = bound->input_format;
    engine->summary.rotary_width = bound->rotary_width;
    engine->summary.primary_theta = bound->primary_theta;
    engine->summary.secondary_theta = bound->secondary_theta;
    engine->summary.token_domain_size = bound->token_domain_size;
    engine->summary.type_domain_size = bound->type_domain_size;
    engine->summary.marker_token_id = bound->marker_token_id;
    engine->summary.source_mapped_bytes = source->source_bytes;
    engine->summary.parameter_execution_bytes = yvex_tensor_parameters_encoded_bytes(engine->parameters);
    yvex_program_stage_resources(engine->stage, &engine->summary.workspace_host_bytes,
        &engine->summary.workspace_device_bytes);
    yvex_core_text_copy(engine->summary.binding_identity, sizeof(engine->summary.binding_identity), bound->identity);
    yvex_core_text_copy(engine->summary.source_identity,
        sizeof(engine->summary.source_identity), bound->source_identity);
    yvex_core_text_copy(engine->summary.logical_model_identity,
        sizeof(engine->summary.logical_model_identity), bound->logical_model_identity);
    yvex_core_text_copy(engine->summary.tokenizer_identity,
        sizeof(engine->summary.tokenizer_identity), bound->tokenizer_identity);
    yvex_core_text_copy(engine->summary.physical_program_identity,
        sizeof(engine->summary.physical_program_identity), bound->physical_program_identity);
    *summary = engine->summary;
    *out = engine;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int tensor_engine_execute(yvex_tensor_engine *engine, unsigned long long generation,
    unsigned long long rows, const yvex_program_host_input *inputs, size_t input_count,
    float *const *outputs, size_t output_count, int (*cancel)(void *), void *cancel_context,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!engine || !engine->mutex_ready)
        return engine_refuse(err, YVEX_ERR_INVALID_ARG, "open engine required");
    if (pthread_mutex_lock(&engine->mutex) != 0)
        return engine_refuse(err, YVEX_ERR_STATE, "engine execution lock failed");
    int rc;
    if (engine->closing || generation != engine->summary.generation)
        rc = engine_refuse(err, YVEX_ERR_STATE, "stale or closing engine generation");
    else if (!rows || rows > engine->summary.maximum_rows)
        rc = engine_refuse(err, YVEX_ERR_BOUNDS, "input population exceeds admitted engine capacity");
    else rc = yvex_program_stage_host_inputs(engine->stage, rows, inputs, input_count,
        outputs, output_count, cancel, cancel_context, facts, err);
    pthread_mutex_unlock(&engine->mutex);
    return rc;
}

int yvex_tensor_engine_execute_tokens(yvex_tensor_engine *engine,
    unsigned long long generation, const unsigned int *token_ids,
    unsigned long long token_count, unsigned int type_id, float *row_scores,
    int (*cancel)(void *), void *cancel_context,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!engine || !token_ids || !row_scores || !token_count ||
        token_count > engine->summary.maximum_rows ||
        engine->summary.input_format != YVEX_TENSOR_INPUT_TOKEN_TYPE_DUAL_ROPE_V1 ||
        type_id >= engine->summary.type_domain_size ||
        engine->summary.rotary_width > SIZE_MAX / sizeof(float) / 4u / token_count)
        return engine_refuse(err, YVEX_ERR_BOUNDS, "bounded admitted token/type input required");
    for (unsigned long long i = 0u; i < token_count; ++i)
        if (token_ids[i] >= engine->summary.token_domain_size)
            return engine_refuse(err, YVEX_ERR_BOUNDS, "token exceeds authenticated input domain");
    size_t rows = (size_t)token_count;
    size_t width = (size_t)engine->summary.rotary_width;
    unsigned int *types = calloc(rows, sizeof(*types));
    float *tables = malloc(rows * width * 4u * sizeof(*tables));
    if (!types || !tables) {
        free(types); free(tables);
        return engine_refuse(err, YVEX_ERR_NOMEM, "token input lowering allocation failed");
    }
    for (size_t p = 0u; p < rows; ++p) {
        types[p] = type_id;
        for (size_t d = 0u; d < width; ++d) {
            double power = (double)(d % (width / 2u)) * 2.0 / (double)width;
            double full = (double)p / pow((double)engine->summary.primary_theta, power);
            double local = (double)p / pow((double)engine->summary.secondary_theta, power);
            tables[p * width + d] = (float)cos(full);
            tables[rows * width + p * width + d] = (float)sin(full);
            tables[2u * rows * width + p * width + d] = (float)cos(local);
            tables[3u * rows * width + p * width + d] = (float)sin(local);
        }
    }
    yvex_program_host_input inputs[] = {{.indices = token_ids}, {.indices = types},
        {.values = tables}, {.values = tables + rows * width},
        {.values = tables + 2u * rows * width}, {.values = tables + 3u * rows * width}};
    int rc = tensor_engine_execute(engine, generation, token_count, inputs, 6u,
        (float *[]){row_scores}, 1u, cancel, cancel_context, facts, err);
    free(tables);
    free(types);
    return rc;
}

int yvex_tensor_engine_close(yvex_tensor_engine **engine, yvex_error *err)
{
    if (!engine || !*engine) { yvex_error_clear(err); return YVEX_OK; }
    if (!(*engine)->mutex_ready) {
        int rc = engine_release(*engine, err);
        if (rc == YVEX_OK) *engine = NULL;
        return rc;
    }
    if (pthread_mutex_lock(&(*engine)->mutex) != 0)
        return engine_refuse(err, YVEX_ERR_STATE, "engine close lock failed");
    (*engine)->closing = 1;
    pthread_mutex_unlock(&(*engine)->mutex);
    int rc = engine_release(*engine, err);
    if (rc == YVEX_OK) *engine = NULL;
    return rc;
}
