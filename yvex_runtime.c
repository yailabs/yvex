/*
 * yvex_runtime.c - Engine, session, and runtime operator coordination.
 *
 * This file owns descriptor/runtime coordination, engine/session state, shared
 * operator helpers, and runtime command surfaces that do not have a more
 * specific owner. It does not own graph proof math, provider generation, or
 * benchmark claims.
 */

#include "yvex_console_private.h"
#include <errno.h>
#include <stddef.h>
#include <yvex/yvex.h>
#include "yvex_runtime_private.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


struct yvex_session {
    const yvex_engine *engine;
    yvex_backend *backend;
    yvex_session_state state;
    unsigned long long context_length;
    unsigned long long max_tokens;
    unsigned long long position;
    unsigned long long accepted_tokens;
    unsigned long long rejected_tokens;
    int graph_partial;
    int backend_available;
    yvex_kv_cache *kv;
    yvex_logits *logits;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

char *yvex_runtime_strdup(const char *text);
void yvex_runtime_set_graph_reason(char *out, size_t cap, const yvex_graph *graph);
void yvex_runtime_set_text_reason(char *out, size_t cap, const char *text);



static void set_engine_status_from_graph(yvex_engine *engine)
{
    if (!engine->graph) {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        yvex_runtime_set_text_reason(engine->reason, sizeof(engine->reason),
                                     "graph not requested; execution not implemented in engine/session layer");
        return;
    }

    if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        engine->status = YVEX_ENGINE_STATUS_PARTIAL;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_UNSUPPORTED ||
               yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_INVALID) {
        engine->status = YVEX_ENGINE_STATUS_UNSUPPORTED;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    }
}

static int runtime_backend_kind_from_name(const char *name,
                                          yvex_backend_kind *out,
                                          yvex_error *err)
{
    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime_backend_kind_from_name",
                       "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!name || strcmp(name, "cpu") == 0) {
        *out = YVEX_BACKEND_KIND_CPU;
        return YVEX_OK;
    }
    if (strcmp(name, "cuda") == 0) {
        *out = YVEX_BACKEND_KIND_CUDA;
        return YVEX_OK;
    }

    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open",
                    "unknown backend kind: %s", name);
    return YVEX_ERR_INVALID_ARG;
}

static int attach_engine_weights(yvex_engine *engine,
                                 const yvex_engine_options *options,
                                 yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_backend_kind kind;
    const char *backend_name;
    int rc;

    if (!engine || !options || !options->attach_weights) {
        return YVEX_OK;
    }

    backend_name = options->backend_name ? options->backend_name : "cpu";
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));

    rc = runtime_backend_kind_from_name(backend_name, &kind, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    backend_options.kind = kind;
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    materialize_options.backend_name = backend_name;
    materialize_options.require_all_tensors = options->require_all_weights;
    rc = yvex_weight_table_materialize(&weights,
                                       engine->artifact,
                                       engine->gguf,
                                       engine->tensors,
                                       backend,
                                       &materialize_options,
                                       err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        return rc;
    }

    rc = yvex_weight_table_get_summary(weights, &engine->weight_summary, err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        memset(&engine->weight_summary, 0, sizeof(engine->weight_summary));
        return rc;
    }

    engine->weight_backend = backend;
    engine->weights = weights;
    return YVEX_OK;
}

int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err)
{
    yvex_engine *engine;
    yvex_artifact_options artifact_options;
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_graph_build_options graph_options;
    int rc;
    int load_tokenizer;
    int build_default_graph;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!options || !options->model_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open", "model_path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    engine = (yvex_engine *)calloc(1, sizeof(*engine));
    if (!engine) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to allocate engine");
        return YVEX_ERR_NOMEM;
    }
    engine->status = YVEX_ENGINE_STATUS_EMPTY;
    engine->model_path = yvex_runtime_strdup(options->model_path);
    if (!engine->model_path) {
        yvex_engine_close(engine);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to copy model path");
        return YVEX_ERR_NOMEM;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
    artifact_options.path = options->model_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;

    rc = yvex_artifact_open(&engine->artifact, &artifact_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_gguf_open(&engine->gguf, engine->artifact, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&engine->tensors, engine->gguf, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&engine->model, engine->gguf, engine->tensors, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(engine->artifact,
                                              engine->gguf,
                                              engine->tensors,
                                              &integrity_options,
                                              &integrity_report,
                                              err);
    }

    load_tokenizer = options->load_tokenizer != 0;
    if (rc == YVEX_OK && load_tokenizer) {
        rc = yvex_tokenizer_from_gguf(&engine->tokenizer, engine->gguf, engine->model, err);
    }

    build_default_graph = options->build_default_graph != 0;
    if (rc == YVEX_OK && build_default_graph) {
        memset(&graph_options, 0, sizeof(graph_options));
        graph_options.sequence_length = 1;
        graph_options.context_length = yvex_model_context_length(engine->model) > 0
                                           ? yvex_model_context_length(engine->model)
                                           : 1;
        graph_options.include_prefill_path = 1;
        rc = yvex_graph_build_for_model(&engine->graph, engine->model, engine->tensors, &graph_options, err);
    }
    if (rc == YVEX_OK) {
        rc = attach_engine_weights(engine, options, err);
    }

    if (rc != YVEX_OK) {
        engine->status = YVEX_ENGINE_STATUS_FAILED;
        yvex_engine_close(engine);
        return rc;
    }

    set_engine_status_from_graph(engine);
    *out = engine;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_engine_open_path(yvex_engine **out,
                          const char *model_path,
                          yvex_error *err)
{
    yvex_engine_options options;

    memset(&options, 0, sizeof(options));
    options.model_path = model_path;
    options.load_tokenizer = 1;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    return yvex_engine_open(out, &options, err);
}

void yvex_engine_close(yvex_engine *engine)
{
    if (!engine) {
        return;
    }
    yvex_weight_table_close(engine->weights);
    yvex_backend_close(engine->weight_backend);
    yvex_graph_close(engine->graph);
    yvex_tokenizer_close(engine->tokenizer);
    yvex_model_descriptor_close(engine->model);
    yvex_tensor_table_close(engine->tensors);
    yvex_gguf_close(engine->gguf);
    yvex_artifact_close(engine->artifact);
    free(engine->model_path);
    free(engine);
}

yvex_engine_status yvex_engine_status_of(const yvex_engine *engine)
{
    return engine ? engine->status : YVEX_ENGINE_STATUS_EMPTY;
}

const char *yvex_engine_status_name(yvex_engine_status status)
{
    switch (status) {
    case YVEX_ENGINE_STATUS_EMPTY: return "empty";
    case YVEX_ENGINE_STATUS_LOADED: return "loaded";
    case YVEX_ENGINE_STATUS_PARTIAL: return "partial";
    case YVEX_ENGINE_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_ENGINE_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_engine_model_path(const yvex_engine *engine)
{
    return engine && engine->model_path ? engine->model_path : "";
}

const yvex_model_descriptor *yvex_engine_model(const yvex_engine *engine)
{
    return engine ? engine->model : NULL;
}

const yvex_tensor_table *yvex_engine_tensors(const yvex_engine *engine)
{
    return engine ? engine->tensors : NULL;
}

const yvex_tokenizer *yvex_engine_tokenizer(const yvex_engine *engine)
{
    return engine ? engine->tokenizer : NULL;
}

const yvex_graph *yvex_engine_graph(const yvex_engine *engine)
{
    return engine ? engine->graph : NULL;
}

int yvex_engine_get_summary(const yvex_engine *engine,
                            yvex_engine_summary *out,
                            yvex_error *err)
{
    const yvex_gguf_header *header;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_get_summary", "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->status = engine->status;
    out->model_path = yvex_engine_model_path(engine);
    out->model_name = yvex_model_name(engine->model);
    out->architecture = yvex_arch_name(yvex_model_arch(engine->model));
    if (engine->gguf) {
        header = yvex_gguf_header_view(engine->gguf);
        if (header) {
            out->metadata_count = header->metadata_count;
            out->tensor_count = header->tensor_count;
        }
    }
    out->known_tensor_bytes = yvex_model_total_storage_bytes(engine->model);
    out->unsupported_tensor_accounting =
        yvex_model_unsupported_tensor_accounting_count(engine->model);
    out->tokenizer_model = engine->tokenizer
                               ? yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(engine->tokenizer))
                               : "absent";
    out->tokenizer_support = engine->tokenizer
                                 ? yvex_tokenizer_support_name(yvex_tokenizer_support_of(engine->tokenizer))
                                 : "absent";
    out->graph_status = engine->graph
                            ? yvex_graph_status_name(yvex_graph_status_of(engine->graph))
                            : "not-built";
    out->weights_attached = engine->weights != NULL;
    out->weights_backend = engine->weight_summary.backend_name
                               ? engine->weight_summary.backend_name
                               : "none";
    out->weight_tensor_count = engine->weight_summary.tensors_materialized;
    out->weight_total_bytes = engine->weight_summary.bytes_materialized;
    out->weight_backend_allocated_bytes = engine->weight_summary.backend_allocated_bytes;
    out->graph_execution_ready = 0;

    yvex_error_clear(err);
    return YVEX_OK;
}

const char *yvex_engine_diagnostic_reason(const yvex_engine *engine)
{
    return engine ? engine->reason : "";
}

static unsigned long long fixture_checksum_bytes(const unsigned char *data,
                                                 unsigned long long len)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; i < len; ++i) {
        hash ^= (unsigned long long)data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned int runtime_read_u16le(const unsigned char *p)
{
    return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
}

static float runtime_f16_bits_to_float(unsigned int h)
{
    unsigned int sign = (h & 0x8000u) << 16;
    unsigned int exp = (h >> 10) & 0x1fu;
    unsigned int mant = h & 0x03ffu;
    uint32_t raw;
    float out;

    if (exp == 0u) {
        if (mant == 0u) {
            raw = sign;
        } else {
            exp = 1u;
            while ((mant & 0x0400u) == 0u) {
                mant <<= 1;
                exp -= 1u;
            }
            mant &= 0x03ffu;
            raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        raw = sign | 0x7f800000u | (mant << 13);
    } else {
        raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
    }
    memcpy(&out, &raw, sizeof(out));
    return out;
}

static int build_f16_embedding_reference(const yvex_artifact *artifact,
                                         const yvex_tensor_range *range,
                                         const yvex_tensor_slice_range *slice,
                                         float *out,
                                         yvex_error *err)
{
    const unsigned char *data;
    unsigned long long hidden_size;
    unsigned long long slice_offset;
    unsigned long long i;

    if (!artifact || !range || !slice || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                       "artifact, range, slice, and reference output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!range->range_valid || !slice->range_valid) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                       "partial reference slice range is invalid");
        return YVEX_ERR_BOUNDS;
    }
    hidden_size = range->dims[0];
    slice_offset = slice->slice_absolute_offset;
    data = yvex_artifact_data(artifact);
    if (!data) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                       "artifact data is unavailable");
        return YVEX_ERR_INVALID_ARG;
    }

    for (i = 0; i < hidden_size; ++i) {
        out[i] = runtime_f16_bits_to_float(runtime_read_u16le(data + slice_offset + (i * 2ull)));
    }
    return YVEX_OK;
}

static int runtime_checked_mul_ull(unsigned long long a,
                                   unsigned long long b,
                                   unsigned long long *out)
{
    if (!out) {
        return 0;
    }
    if (a != 0ull && b > ULLONG_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static int runtime_checked_add_ull(unsigned long long a,
                                   unsigned long long b,
                                   unsigned long long *out)
{
    if (!out || a > ULLONG_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static double runtime_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

static int runtime_find_rmsnorm_epsilon(const yvex_gguf *gguf,
                                        const char **key_out,
                                        double *epsilon_out,
                                        yvex_error *err)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon",
        "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    if (!gguf || !key_out || !epsilon_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "GGUF, key output, and epsilon output are required");
        return YVEX_ERR_INVALID_ARG;
    }

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, keys[i]);
        double epsilon = 0.0;
        if (!value) {
            continue;
        }
        if (yvex_gguf_value_as_f64(value, &epsilon) != YVEX_OK || epsilon <= 0.0) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                            "rmsnorm-epsilon-invalid: %s", keys[i]);
            return YVEX_ERR_FORMAT;
        }
        *key_out = keys[i];
        *epsilon_out = epsilon;
        return YVEX_OK;
    }

    yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                   "rmsnorm-epsilon-missing");
    return YVEX_ERR_FORMAT;
}

static const yvex_tensor_info *runtime_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight",
        "blk.0.attention_norm.weight",
        "blk.0.input_layernorm.weight",
        "model.layers.0.input_layernorm.weight",
    };
    unsigned int i;
    unsigned long long count;
    unsigned long long index;

    if (!tensors) {
        return NULL;
    }
    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, preferred[i]);
        if (tensor) {
            return tensor;
        }
    }
    count = yvex_tensor_table_count(tensors);
    for (index = 0; index < count; ++index) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
        if (tensor && tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM) {
            return tensor;
        }
    }
    return NULL;
}

static int build_rmsnorm_weight_reference(const yvex_artifact *artifact,
                                          const yvex_tensor_range *range,
                                          float *out,
                                          yvex_error *err)
{
    const unsigned char *data;
    unsigned long long i;

    if (!artifact || !range || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "artifact, range, and RMSNorm reference output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!range->range_valid || range->rank != 1) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                       "RMSNorm reference tensor range is invalid");
        return YVEX_ERR_BOUNDS;
    }
    if (range->dtype != YVEX_DTYPE_F16 && range->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "rmsnorm-dtype-invalid");
        return YVEX_ERR_UNSUPPORTED;
    }
    data = yvex_artifact_data(artifact);
    if (!data) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "artifact data is unavailable");
        return YVEX_ERR_INVALID_ARG;
    }
    data += range->tensor_absolute_offset;
    for (i = 0; i < range->dims[0]; ++i) {
        if (range->dtype == YVEX_DTYPE_F16) {
            out[i] = runtime_f16_bits_to_float(runtime_read_u16le(data + (i * 2ull)));
        } else {
            memcpy(&out[i], data + (i * (unsigned long long)sizeof(float)), sizeof(float));
        }
    }
    return YVEX_OK;
}

static void build_rmsnorm_reference(const float *embedding,
                                    const float *weight,
                                    unsigned long long hidden_size,
                                    double epsilon,
                                    float *out)
{
    unsigned long long i;
    double sum_squares = 0.0;
    double inv_rms;

    for (i = 0; i < hidden_size; ++i) {
        sum_squares += (double)embedding[i] * (double)embedding[i];
    }
    inv_rms = 1.0 / runtime_sqrt_double((sum_squares / (double)hidden_size) + epsilon);
    for (i = 0; i < hidden_size; ++i) {
        out[i] = (float)((double)embedding[i] * inv_rms * (double)weight[i]);
    }
}

static int yvex_runtime_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static double max_abs_diff_f32(const float *a, const float *b, unsigned long long count)
{
    unsigned long long i;
    double max_diff = 0.0;

    for (i = 0; i < count; ++i) {
        double diff = (double)a[i] - (double)b[i];
        if (diff < 0.0) {
            diff = -diff;
        }
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
}

int yvex_engine_execute_fixture_graph(yvex_engine *engine,
                                      const yvex_fixture_graph_options *options,
                                      yvex_fixture_graph_result *out,
                                      yvex_error *err)
{
    const yvex_graph_op_info *op = NULL;
    const yvex_materialized_weight *weight;
    const yvex_device_tensor *embedding;
    yvex_backend_tensor_desc output_desc;
    yvex_device_tensor *output = NULL;
    unsigned int token_id = 0;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long i;
    float *readback = NULL;
    int rc;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_fixture_graph",
                       "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->backend_name = "none";
    out->graph_integrity_guard = "fail";
    out->graph_execution_phase = "preflight";
    out->graph_kind = "fixture-embedding";
    out->shape_status = "unchecked";
    out->range_status = "unchecked";
    out->slice_range_status = "not-needed";
    out->backend_status = "unchecked";
    out->backend_op_status = "unchecked";
    out->cleanup_status = "not-needed";
    out->op_name = "embed";
    out->weight_name = "token_embd.weight";
    out->execution_ready = 0;
    out->graph_execution_ready = 0;

    if (options) {
        token_id = options->token_id;
    }
    out->token_id = token_id;

    if (!engine->graph) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_fixture_graph",
                       "fixture graph requires a built graph");
        return YVEX_ERR_STATE;
    }
    for (i = 0; i < yvex_graph_op_count(engine->graph); ++i) {
        const yvex_graph_op_info *candidate = yvex_graph_op_at(engine->graph, i);
        if (candidate && candidate->kind == YVEX_OP_EMBED) {
            op = candidate;
            break;
        }
    }
    if (!op) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "fixture graph requires a planned embed node");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!engine->weight_backend || !engine->weights) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_fixture_graph",
                       "fixture graph requires attached weights");
        return YVEX_ERR_STATE;
    }
    out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    out->backend_status = "ready";

    weight = yvex_weight_table_find(engine->weights, "token_embd.weight");
    if (!weight) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (yvex_weight_dtype(weight) != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "fixture graph embed execution requires F32 token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    embedding = yvex_weight_device_tensor(weight);
    if (!embedding) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_fixture_graph",
                       "attached token embedding has no backend tensor");
        return YVEX_ERR_STATE;
    }
    if (yvex_device_tensor_rank(embedding) != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_fixture_graph",
                       "fixture graph token embedding must have rank 2");
        return YVEX_ERR_FORMAT;
    }

    hidden_size = yvex_device_tensor_dims(embedding)[0];
    vocab_size = yvex_device_tensor_dims(embedding)[1];
    if (hidden_size == 0 || vocab_size == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_fixture_graph",
                       "fixture graph token embedding dimensions must be non-zero");
        return YVEX_ERR_FORMAT;
    }
    out->shape_status = "pass";
    out->range_status = "pass";
    if ((unsigned long long)token_id >= vocab_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_fixture_graph",
                        "fixture token id %u exceeds embedding vocab size %llu",
                        token_id, vocab_size);
        return YVEX_ERR_BOUNDS;
    }
    if (hidden_size > (unsigned long long)(~(size_t)0 / sizeof(float))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_fixture_graph",
                       "fixture graph output is too large for host readback");
        return YVEX_ERR_BOUNDS;
    }

    output_count = hidden_size;
    output_bytes = output_count * (unsigned long long)sizeof(float);
    out->output_bytes_planned = output_bytes;
    memset(&output_desc, 0, sizeof(output_desc));
    output_desc.name = "fixture.embed.output";
    output_desc.dtype = YVEX_DTYPE_F32;
    output_desc.rank = 2;
    output_desc.dims[0] = 1;
    output_desc.dims[1] = hidden_size;
    output_desc.bytes = output_bytes;

    if (!yvex_backend_supports(engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        yvex_runtime_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        out->backend_op_status = "unsupported";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "fixture graph backend does not support embed op");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->backend_op_status = "supported";
    out->graph_execution_phase = "output";
    out->output_allocation_attempted = 1;
    rc = yvex_backend_tensor_alloc(engine->weight_backend, &output_desc, &output, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    out->output_bytes_allocated = output_bytes;
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_OUTPUT_ALLOC")) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_fixture_graph",
                       "test graph failure after output allocation");
        return YVEX_ERR_BACKEND;
    }
    out->graph_execution_phase = "dispatch";
    out->dispatch_attempted = 1;
    rc = yvex_backend_op_embed(engine->weight_backend, embedding, &token_id, 1, output, err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_DISPATCH")) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_fixture_graph",
                       "test graph failure after dispatch");
        return YVEX_ERR_BACKEND;
    }

    readback = (float *)malloc((size_t)output_bytes);
    if (!readback) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_execute_fixture_graph",
                       "failed to allocate fixture output readback");
        return YVEX_ERR_NOMEM;
    }
    rc = yvex_backend_tensor_read(engine->weight_backend, output, readback, output_bytes, err);
    if (rc != YVEX_OK) {
        free(readback);
        yvex_backend_tensor_free(engine->weight_backend, output);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }

    out->executed = 1;
    out->node_count = 1;
    out->output_count = output_count;
    out->output_bytes = output_bytes;
    out->output_checksum = fixture_checksum_bytes((const unsigned char *)readback, output_bytes);
    out->output_value_count = output_count > YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES
                                  ? YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES
                                  : output_count;
    for (i = 0; i < out->output_value_count; ++i) {
        out->output_values[i] = readback[i];
    }

    free(readback);
    yvex_backend_tensor_free(engine->weight_backend, output);
    out->graph_integrity_guard = "pass";
    out->graph_execution_phase = "complete";
    out->cleanup_status = "not-needed";
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_engine_execute_partial_graph(yvex_engine *engine,
                                      const yvex_partial_graph_options *options,
                                      yvex_partial_graph_result *out,
                                      yvex_error *err)
{
    const yvex_graph_op_info *op = NULL;
    const yvex_materialized_weight *weight;
    const yvex_tensor_info *tensor;
    const yvex_device_tensor *embedding;
    yvex_backend_tensor_desc output_desc;
    yvex_device_tensor *output = NULL;
    unsigned int token_id = 0;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long i;
    yvex_selected_embedding_shape embedding_shape;
    yvex_tensor_range tensor_range;
    yvex_tensor_slice_range slice_range;
    float *readback = NULL;
    float *reference = NULL;
    int rc;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                       "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->backend_name = "none";
    out->graph_integrity_guard = "fail";
    out->graph_execution_phase = "preflight";
    out->graph_kind = "selected-embedding-partial";
    out->shape_status = "unchecked";
    out->range_status = "unchecked";
    out->slice_range_status = "unchecked";
    out->backend_status = "unchecked";
    out->backend_op_status = "unchecked";
    out->cleanup_status = "not-needed";
    out->segment_name = "token-embedding";
    out->weight_name = "token_embd.weight";
    out->weight_dtype = "F16";
    out->output_dtype = "F32";
    out->execution_ready = 0;
    out->graph_execution_ready = 0;

    if (options) {
        token_id = options->token_id;
    }
    out->token_id = token_id;

    if (!engine->graph) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_partial_graph",
                       "real partial graph requires a built graph");
        return YVEX_ERR_STATE;
    }
    if (!engine->weight_backend || !engine->weights) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_partial_graph",
                       "real partial graph requires engine-attached weights");
        return YVEX_ERR_STATE;
    }
    out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    out->backend_status = "ready";

    weight = yvex_weight_table_find(engine->weights, "token_embd.weight");
    if (!weight) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_partial_graph",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (yvex_weight_dtype(weight) != YVEX_DTYPE_F16) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_partial_graph",
                       "real partial embedding segment requires F16 token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    tensor = yvex_tensor_table_find(engine->tensors, "token_embd.weight");
    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_partial_graph",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    embedding = yvex_weight_device_tensor(weight);
    if (!embedding) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_partial_graph",
                       "attached token embedding has no backend tensor");
        return YVEX_ERR_STATE;
    }
    for (i = 0; i < yvex_graph_op_count(engine->graph); ++i) {
        const yvex_graph_op_info *candidate = yvex_graph_op_at(engine->graph, i);
        if (candidate && candidate->kind == YVEX_OP_EMBED) {
            op = candidate;
            break;
        }
    }
    if (!op) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_partial_graph",
                       "real partial graph requires a planned embed node");
        return YVEX_ERR_UNSUPPORTED;
    }
    memset(&embedding_shape, 0, sizeof(embedding_shape));
    rc = yvex_selected_embedding_shape_validate(tensor, token_id, &embedding_shape, err);
    if (rc != YVEX_OK) {
        out->shape_status = "fail";
        out->slice_range_status = "fail";
        if (strstr(yvex_error_message(err), "token-out-of-range")) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                            "partial token out of range: %u >= %llu",
                            token_id, embedding_shape.vocab_size);
        }
        return rc;
    }
    out->shape_status = "pass";
    if (yvex_device_tensor_rank(embedding) != 2 || tensor->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_partial_graph",
                       "real partial token embedding must have rank 2");
        return YVEX_ERR_FORMAT;
    }

    hidden_size = embedding_shape.hidden_size;
    vocab_size = embedding_shape.vocab_size;
    if (hidden_size == 0 || vocab_size == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_partial_graph",
                       "real partial token embedding dimensions must be non-zero");
        return YVEX_ERR_FORMAT;
    }
    if (yvex_device_tensor_dims(embedding)[0] != hidden_size ||
        yvex_device_tensor_dims(embedding)[1] != vocab_size ||
        tensor->dims[0] != hidden_size || tensor->dims[1] != vocab_size ||
        tensor->storage_bytes != yvex_weight_bytes(weight)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_partial_graph",
                       "attached token embedding shape does not match tensor descriptor");
        return YVEX_ERR_FORMAT;
    }
    memset(&tensor_range, 0, sizeof(tensor_range));
    memset(&slice_range, 0, sizeof(slice_range));
    rc = yvex_tensor_range_validate(engine->artifact, engine->gguf, tensor, &tensor_range, err);
    if (rc != YVEX_OK) {
        out->range_status = "fail";
        return rc;
    }
    out->range_status = "pass";
    if (tensor_range.tensor_bytes != yvex_weight_bytes(weight)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_partial_graph",
                       "attached token embedding byte count does not match validated tensor range");
        return YVEX_ERR_FORMAT;
    }
    rc = yvex_tensor_embedding_slice_range_validate(&tensor_range,
                                                    token_id,
                                                    &slice_range,
                                                    err);
    if (rc != YVEX_OK) {
        out->slice_range_status = "fail";
        if ((unsigned long long)token_id >= vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                            "partial token out of range: %u >= %llu", token_id, vocab_size);
        }
        return rc;
    }
    out->slice_range_status = "pass";
    if (embedding_shape.output_bytes > (unsigned long long)(~(size_t)0)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                       "partial graph output is too large for host readback");
        return YVEX_ERR_BOUNDS;
    }

    output_count = embedding_shape.output_count;
    output_bytes = embedding_shape.output_bytes;
    out->output_bytes_planned = output_bytes;
    out->reference_bytes_planned = slice_range.slice_bytes;
    memset(&output_desc, 0, sizeof(output_desc));
    output_desc.name = "partial.token_embedding.output";
    output_desc.dtype = YVEX_DTYPE_F32;
    output_desc.rank = 2;
    output_desc.dims[0] = 1;
    output_desc.dims[1] = hidden_size;
    output_desc.bytes = output_bytes;

    if (!yvex_backend_supports(engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        yvex_runtime_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        out->backend_op_status = "unsupported";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_partial_graph",
                       "real partial graph backend does not support embed op");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->backend_op_status = "supported";
    readback = (float *)malloc((size_t)output_bytes);
    reference = (float *)malloc((size_t)output_bytes);
    if (!readback || !reference) {
        free(readback);
        free(reference);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_execute_partial_graph",
                       "failed to allocate partial graph readback buffers");
        return YVEX_ERR_NOMEM;
    }

    out->graph_execution_phase = "reference";
    out->reference_read_attempted = 1;
    rc = build_f16_embedding_reference(engine->artifact, &tensor_range, &slice_range,
                                       reference, err);
    if (rc != YVEX_OK) {
        free(readback);
        free(reference);
        return rc;
    }
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_REFERENCE_READ")) {
        free(readback);
        free(reference);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_partial_graph",
                       "test graph failure after reference read");
        return YVEX_ERR_BACKEND;
    }

    out->graph_execution_phase = "output";
    out->output_allocation_attempted = 1;
    rc = yvex_backend_tensor_alloc(engine->weight_backend, &output_desc, &output, err);
    if (rc != YVEX_OK) {
        free(readback);
        free(reference);
        return rc;
    }
    out->output_bytes_allocated = output_bytes;
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_OUTPUT_ALLOC")) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        free(readback);
        free(reference);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_partial_graph",
                       "test graph failure after output allocation");
        return YVEX_ERR_BACKEND;
    }
    out->graph_execution_phase = "dispatch";
    out->dispatch_attempted = 1;
    rc = yvex_backend_op_embed(engine->weight_backend, embedding, &token_id, 1, output, err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        free(readback);
        free(reference);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_DISPATCH")) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        free(readback);
        free(reference);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_partial_graph",
                       "test graph failure after dispatch");
        return YVEX_ERR_BACKEND;
    }
    rc = yvex_backend_tensor_read(engine->weight_backend, output, readback, output_bytes, err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        free(readback);
        free(reference);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }

    out->executed = 1;
    out->node_count = 1;
    out->output_count = output_count;
    out->output_bytes = output_bytes;
    out->output_checksum = fixture_checksum_bytes((const unsigned char *)readback, output_bytes);
    out->reference_checksum = fixture_checksum_bytes((const unsigned char *)reference, output_bytes);
    out->max_abs_diff = max_abs_diff_f32(readback, reference, output_count);
    if (out->max_abs_diff != 0.0) {
        yvex_backend_tensor_free(engine->weight_backend, output);
        free(readback);
        free(reference);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_engine_execute_partial_graph",
                        "partial graph reference comparison failed: max_abs_diff %.9g",
                        out->max_abs_diff);
        return YVEX_ERR_FORMAT;
    }
    out->output_value_count = output_count > YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES
                                  ? YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES
                                  : output_count;
    for (i = 0; i < out->output_value_count; ++i) {
        out->output_values[i] = readback[i];
    }

    yvex_backend_tensor_free(engine->weight_backend, output);
    free(readback);
    free(reference);
    out->graph_integrity_guard = "pass";
    out->graph_execution_phase = "complete";
    out->cleanup_status = "not-needed";
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_engine_execute_segment_graph(yvex_engine *engine,
                                      const yvex_segment_graph_options *options,
                                      yvex_segment_graph_result *out,
                                      yvex_error *err)
{
    const yvex_graph_op_info *embed_op = NULL;
    const yvex_materialized_weight *token_weight;
    const yvex_materialized_weight *rmsnorm_weight;
    const yvex_tensor_info *token_tensor;
    const yvex_tensor_info *rmsnorm_tensor;
    const yvex_device_tensor *embedding;
    const yvex_device_tensor *rmsnorm_weight_tensor;
    yvex_backend_tensor_desc embed_desc;
    yvex_backend_tensor_desc output_desc;
    yvex_device_tensor *embed_output = NULL;
    yvex_device_tensor *segment_output = NULL;
    yvex_selected_embedding_shape embedding_shape;
    yvex_tensor_range token_range;
    yvex_tensor_range rmsnorm_range;
    yvex_tensor_slice_range slice_range;
    const char *epsilon_key = NULL;
    double epsilon = 0.0;
    unsigned int token_id = 0;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_bytes;
    unsigned long long planned_alloc_bytes;
    unsigned long long i;
    float *embedding_reference = NULL;
    float *rmsnorm_weight_reference = NULL;
    float *segment_reference = NULL;
    float *readback = NULL;
    int rc;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->backend_name = "none";
    out->graph_integrity_guard = "fail";
    out->graph_execution_phase = "preflight";
    out->graph_kind = "selected-embedding-rmsnorm";
    out->shape_status = "unchecked";
    out->range_status = "unchecked";
    out->slice_range_status = "unchecked";
    out->backend_status = "unchecked";
    out->backend_op_status = "unchecked";
    out->cleanup_status = "not-needed";
    out->segment_name = "embedding-rmsnorm";
    out->token_tensor_name = "token_embd.weight";
    out->token_tensor_dtype = "F16";
    out->rmsnorm_tensor_name = "";
    out->rmsnorm_tensor_dtype = "";
    out->rmsnorm_epsilon_key = "";
    out->execution_ready = 0;
    out->graph_execution_ready = 0;

    if (options) {
        token_id = options->token_id;
        if (options->segment_name && strcmp(options->segment_name, "embedding-rmsnorm") != 0) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                           "unsupported segment; expected embedding-rmsnorm");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    out->token_id = token_id;

    if (!engine->graph) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_segment_graph",
                       "real segment graph requires a built graph");
        return YVEX_ERR_STATE;
    }
    if (!engine->weight_backend || !engine->weights) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_segment_graph",
                       "real segment graph requires engine-attached weights");
        return YVEX_ERR_STATE;
    }
    out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    out->backend_status = "ready";

    for (i = 0; i < yvex_graph_op_count(engine->graph); ++i) {
        const yvex_graph_op_info *candidate = yvex_graph_op_at(engine->graph, i);
        if (candidate && candidate->kind == YVEX_OP_EMBED) {
            embed_op = candidate;
            break;
        }
    }
    if (!embed_op) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "real segment graph requires a planned embed node");
        return YVEX_ERR_UNSUPPORTED;
    }

    token_weight = yvex_weight_table_find(engine->weights, "token_embd.weight");
    if (!token_weight) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "required-tensor-missing: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (yvex_weight_dtype(token_weight) != YVEX_DTYPE_F16) {
        out->shape_status = "fail";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "real segment embedding requires F16 token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    token_tensor = yvex_tensor_table_find(engine->tensors, "token_embd.weight");
    if (!token_tensor) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "required-tensor-missing: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    embedding = yvex_weight_device_tensor(token_weight);
    if (!embedding) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_segment_graph",
                       "attached token embedding has no backend tensor");
        return YVEX_ERR_STATE;
    }

    memset(&embedding_shape, 0, sizeof(embedding_shape));
    rc = yvex_selected_embedding_shape_validate(token_tensor, token_id, &embedding_shape, err);
    if (rc != YVEX_OK) {
        out->shape_status = strstr(yvex_error_message(err), "token-out-of-range") ? "pass" : "fail";
        out->slice_range_status = "fail";
        if (strstr(yvex_error_message(err), "token-out-of-range")) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                            "partial token out of range: %u >= %llu",
                            token_id, embedding_shape.vocab_size);
        }
        return rc;
    }
    hidden_size = embedding_shape.hidden_size;
    vocab_size = embedding_shape.vocab_size;
    if (yvex_device_tensor_rank(embedding) != 2 ||
        yvex_device_tensor_dims(embedding)[0] != hidden_size ||
        yvex_device_tensor_dims(embedding)[1] != vocab_size ||
        yvex_weight_bytes(token_weight) != token_tensor->storage_bytes) {
        out->shape_status = "fail";
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                       "attached token embedding shape does not match tensor descriptor");
        return YVEX_ERR_FORMAT;
    }

    rmsnorm_tensor = runtime_find_first_rmsnorm_tensor(engine->tensors);
    if (!rmsnorm_tensor) {
        out->shape_status = "fail";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "rmsnorm-tensor-missing");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->rmsnorm_tensor_name = rmsnorm_tensor->name;
    out->rmsnorm_tensor_dtype = yvex_dtype_name(rmsnorm_tensor->dtype);
    rmsnorm_weight = yvex_weight_table_find(engine->weights, rmsnorm_tensor->name);
    if (!rmsnorm_weight) {
        out->shape_status = "fail";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "rmsnorm-tensor-missing");
        return YVEX_ERR_UNSUPPORTED;
    }
    rmsnorm_weight_tensor = yvex_weight_device_tensor(rmsnorm_weight);
    if (!rmsnorm_weight_tensor) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_segment_graph",
                       "attached RMSNorm weight has no backend tensor");
        return YVEX_ERR_STATE;
    }
    if (rmsnorm_tensor->rank != 1 ||
        rmsnorm_tensor->dims[0] != hidden_size ||
        yvex_device_tensor_rank(rmsnorm_weight_tensor) != 1 ||
        yvex_device_tensor_dims(rmsnorm_weight_tensor)[0] != hidden_size) {
        out->shape_status = "fail";
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                       "rmsnorm-shape-invalid");
        return YVEX_ERR_FORMAT;
    }
    if (rmsnorm_tensor->dtype != YVEX_DTYPE_F16 && rmsnorm_tensor->dtype != YVEX_DTYPE_F32) {
        out->shape_status = "fail";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "rmsnorm-dtype-invalid");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->shape_status = "pass";

    memset(&token_range, 0, sizeof(token_range));
    memset(&rmsnorm_range, 0, sizeof(rmsnorm_range));
    memset(&slice_range, 0, sizeof(slice_range));
    rc = yvex_tensor_range_validate(engine->artifact, engine->gguf, token_tensor, &token_range, err);
    if (rc != YVEX_OK) {
        out->range_status = "fail";
        return rc;
    }
    rc = yvex_tensor_range_validate(engine->artifact, engine->gguf, rmsnorm_tensor, &rmsnorm_range, err);
    if (rc != YVEX_OK) {
        out->range_status = "fail";
        return rc;
    }
    if (token_range.tensor_bytes != yvex_weight_bytes(token_weight) ||
        rmsnorm_range.tensor_bytes != yvex_weight_bytes(rmsnorm_weight)) {
        out->range_status = "fail";
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                       "attached tensor byte count does not match validated range");
        return YVEX_ERR_FORMAT;
    }
    out->range_status = "pass";
    rc = yvex_tensor_embedding_slice_range_validate(&token_range, token_id, &slice_range, err);
    if (rc != YVEX_OK) {
        out->slice_range_status = "fail";
        if ((unsigned long long)token_id >= vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                            "partial token out of range: %u >= %llu", token_id, vocab_size);
        }
        return rc;
    }
    out->slice_range_status = "pass";

    rc = runtime_find_rmsnorm_epsilon(engine->gguf, &epsilon_key, &epsilon, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    out->rmsnorm_epsilon_key = epsilon_key;
    out->rmsnorm_epsilon = epsilon;

    if (yvex_runtime_test_env_enabled("YVEX_TEST_SEGMENT_MEMORY_PLAN_OVERFLOW") ||
        !runtime_checked_mul_ull(hidden_size, (unsigned long long)sizeof(float), &output_bytes) ||
        output_bytes > (unsigned long long)(~(size_t)0) ||
        !runtime_checked_add_ull(output_bytes, output_bytes, &planned_alloc_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                       "segment-memory-plan-overflow");
        return YVEX_ERR_BOUNDS;
    }
    out->hidden_size = hidden_size;
    out->vocab_size = vocab_size;
    out->segment_ops = 2;
    out->segment_intermediate_count = 1;
    out->segment_intermediate_bytes = output_bytes;
    out->segment_output_count = hidden_size;
    out->segment_output_bytes = output_bytes;
    out->segment_scratch_bytes = output_bytes;
    out->segment_reference_bytes = output_bytes;
    out->output_bytes_planned = planned_alloc_bytes;
    out->reference_bytes_planned = output_bytes;

    if (!yvex_backend_supports(engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        !yvex_backend_supports(engine->weight_backend, YVEX_BACKEND_CAP_OP_RMS_NORM) ||
        yvex_runtime_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        out->backend_op_status = "unsupported";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "backend-op-unsupported");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->backend_op_status = "supported";

    embedding_reference = (float *)malloc((size_t)output_bytes);
    rmsnorm_weight_reference = (float *)malloc((size_t)output_bytes);
    segment_reference = (float *)malloc((size_t)output_bytes);
    readback = (float *)malloc((size_t)output_bytes);
    if (!embedding_reference || !rmsnorm_weight_reference || !segment_reference || !readback) {
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_execute_segment_graph",
                       "failed to allocate segment reference/readback buffers");
        return YVEX_ERR_NOMEM;
    }

    out->graph_execution_phase = "reference";
    out->reference_read_attempted = 1;
    rc = build_f16_embedding_reference(engine->artifact, &token_range, &slice_range,
                                       embedding_reference, err);
    if (rc == YVEX_OK) {
        rc = build_rmsnorm_weight_reference(engine->artifact, &rmsnorm_range,
                                            rmsnorm_weight_reference, err);
    }
    if (rc != YVEX_OK) {
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        return rc;
    }
    build_rmsnorm_reference(embedding_reference,
                            rmsnorm_weight_reference,
                            hidden_size,
                            epsilon,
                            segment_reference);
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_REFERENCE_READ")) {
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after reference read");
        return YVEX_ERR_BACKEND;
    }

    memset(&embed_desc, 0, sizeof(embed_desc));
    embed_desc.name = "segment.embedding.output";
    embed_desc.dtype = YVEX_DTYPE_F32;
    embed_desc.rank = 2;
    embed_desc.dims[0] = 1;
    embed_desc.dims[1] = hidden_size;
    embed_desc.bytes = output_bytes;
    memset(&output_desc, 0, sizeof(output_desc));
    output_desc.name = "segment.rmsnorm.output";
    output_desc.dtype = YVEX_DTYPE_F32;
    output_desc.rank = 2;
    output_desc.dims[0] = 1;
    output_desc.dims[1] = hidden_size;
    output_desc.bytes = output_bytes;

    out->graph_execution_phase = "output";
    out->output_allocation_attempted = 1;
    rc = yvex_backend_tensor_alloc(engine->weight_backend, &embed_desc, &embed_output, err);
    if (rc != YVEX_OK) {
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        return rc;
    }
    out->output_bytes_allocated = output_bytes;
    rc = yvex_backend_tensor_alloc(engine->weight_backend, &output_desc, &segment_output, err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }
    out->output_bytes_allocated = planned_alloc_bytes;
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_OUTPUT_ALLOC")) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after output allocation");
        return YVEX_ERR_BACKEND;
    }

    out->graph_execution_phase = "dispatch";
    out->dispatch_attempted = 1;
    rc = yvex_backend_op_embed(engine->weight_backend, embedding, &token_id, 1, embed_output, err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_OP0") ||
        yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_SEGMENT_AFTER_OP0")) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after segment op 0");
        return YVEX_ERR_BACKEND;
    }
    rc = yvex_backend_op_rms_norm(engine->weight_backend,
                                  embed_output,
                                  rmsnorm_weight_tensor,
                                  (float)epsilon,
                                  segment_output,
                                  err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }
    if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_GRAPH_AFTER_DISPATCH")) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after dispatch");
        return YVEX_ERR_BACKEND;
    }

    rc = yvex_backend_tensor_read(engine->weight_backend, segment_output, readback, output_bytes, err);
    if (rc != YVEX_OK) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }

    out->executed = 1;
    out->node_count = 2;
    out->output_checksum = fixture_checksum_bytes((const unsigned char *)readback, output_bytes);
    out->reference_checksum = fixture_checksum_bytes((const unsigned char *)segment_reference, output_bytes);
    out->max_abs_diff = max_abs_diff_f32(readback, segment_reference, hidden_size);
    if (out->max_abs_diff > 0.0001) {
        yvex_backend_tensor_free(engine->weight_backend, segment_output);
        yvex_backend_tensor_free(engine->weight_backend, embed_output);
        free(embedding_reference);
        free(rmsnorm_weight_reference);
        free(segment_reference);
        free(readback);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                        "segment graph reference comparison failed: max_abs_diff %.9g",
                        out->max_abs_diff);
        return YVEX_ERR_FORMAT;
    }
    out->output_value_count = hidden_size > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES
                                  ? YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES
                                  : hidden_size;
    for (i = 0; i < out->output_value_count; ++i) {
        out->output_values[i] = readback[i];
    }

    yvex_backend_tensor_free(engine->weight_backend, segment_output);
    yvex_backend_tensor_free(engine->weight_backend, embed_output);
    free(embedding_reference);
    free(rmsnorm_weight_reference);
    free(segment_reference);
    free(readback);
    out->graph_integrity_guard = "pass";
    out->graph_execution_phase = "complete";
    out->cleanup_status = "not-needed";
    yvex_error_clear(err);
    return YVEX_OK;
}



char *yvex_runtime_strdup(const char *text)
{
    char *copy;
    size_t len;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

void yvex_runtime_set_text_reason(char *out, size_t cap, const char *text)
{
    if (!out || cap == 0) {
        return;
    }
    snprintf(out, cap, "%s", text ? text : "");
}

void yvex_runtime_set_graph_reason(char *out, size_t cap, const yvex_graph *graph)
{
    unsigned long long missing_count;
    int has_output_norm = 0;
    int has_output_head = 0;
    int has_token_embedding = 0;
    unsigned long long i;

    if (!out || cap == 0) {
        return;
    }
    out[0] = '\0';

    if (!graph) {
        snprintf(out, cap, "graph not built");
        return;
    }

    missing_count = yvex_graph_missing_required_count(graph);
    for (i = 0; i < missing_count; ++i) {
        const yvex_graph_missing_required *missing = yvex_graph_missing_required_at(graph, i);
        if (!missing) {
            continue;
        }
        if (missing->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING) {
            has_token_embedding = 1;
        } else if (missing->role == YVEX_TENSOR_ROLE_OUTPUT_NORM) {
            has_output_norm = 1;
        } else if (missing->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD) {
            has_output_head = 1;
        }
    }

    if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        if (has_output_norm && has_output_head) {
            snprintf(out, cap, "graph partial; missing output_norm, output_head");
        } else if (has_output_norm) {
            snprintf(out, cap, "graph partial; missing output_norm");
        } else if (has_output_head) {
            snprintf(out, cap, "graph partial; missing output_head");
        } else if (missing_count > 0) {
            snprintf(out, cap, "graph partial; missing required tensor roles");
        } else {
            snprintf(out, cap, "graph partial");
        }
    } else if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_UNSUPPORTED) {
        if (has_token_embedding) {
            snprintf(out, cap, "graph unsupported; missing token_embedding");
        } else {
            snprintf(out, cap, "graph unsupported");
        }
    } else if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_BUILT) {
        snprintf(out, cap, "decode runtime not implemented in engine/session layer");
    } else {
        snprintf(out, cap, "graph status: %s", yvex_graph_status_name(yvex_graph_status_of(graph)));
    }
}



static void set_session_reason_from_graph(yvex_session *session)
{
    const yvex_graph *graph = yvex_engine_graph(session->engine);

    yvex_runtime_set_graph_reason(session->reason, sizeof(session->reason), graph);
}

static yvex_session_state reset_state_for_graph(const yvex_session *session)
{
    return session->graph_partial ? YVEX_SESSION_STATE_PARTIAL : YVEX_SESSION_STATE_READY;
}

int yvex_session_create(yvex_session **out,
                        const yvex_engine *engine,
                        yvex_backend *backend,
                        const yvex_session_options *options,
                        yvex_error *err)
{
    yvex_session *session;
    unsigned long long context_length;
    int allow_partial = 1;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!engine || !backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_create", "engine and backend are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_backend_status_of(backend) != YVEX_BACKEND_STATUS_READY) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_create", "backend is not ready for engine/session layer sessions");
        return YVEX_ERR_UNSUPPORTED;
    }

    context_length = yvex_model_context_length(yvex_engine_model(engine));
    if (options && options->context_length > 0) {
        context_length = options->context_length;
    }
    if (context_length == 0) {
        context_length = 1;
    }
    if (options) {
        allow_partial = options->allow_partial_graph;
    }

    session = (yvex_session *)calloc(1, sizeof(*session));
    if (!session) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_session_create", "failed to allocate session");
        return YVEX_ERR_NOMEM;
    }

    session->engine = engine;
    session->backend = backend;
    session->state = YVEX_SESSION_STATE_CREATED;
    session->context_length = context_length;
    session->max_tokens = options ? options->max_tokens : 0;
    session->backend_available = 1;
    session->graph_partial = yvex_engine_status_of(engine) == YVEX_ENGINE_STATUS_PARTIAL ||
                             (yvex_engine_graph(engine) &&
                              yvex_graph_status_of(yvex_engine_graph(engine)) == YVEX_GRAPH_STATUS_PARTIAL);

    if (session->graph_partial && !allow_partial) {
        yvex_session_close(session);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_create",
                       "graph is partial; allow_partial_graph is required in engine/session layer");
        return YVEX_ERR_UNSUPPORTED;
    }

    if (options && options->create_kv) {
        rc = yvex_kv_cache_create_shape(&session->kv, &options->kv_shape, err);
    } else {
        rc = yvex_kv_cache_create(&session->kv, yvex_engine_model(engine), context_length, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_logits_create(&session->logits, yvex_engine_model(engine), err);
    }
    if (rc != YVEX_OK) {
        yvex_session_close(session);
        return rc;
    }

    set_session_reason_from_graph(session);
    session->state = reset_state_for_graph(session);
    *out = session;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_session_close(yvex_session *session)
{
    if (!session) {
        return;
    }
    session->state = YVEX_SESSION_STATE_CLOSED;
    yvex_logits_close(session->logits);
    yvex_kv_cache_close(session->kv);
    free(session);
}

yvex_session_state yvex_session_state_of(const yvex_session *session)
{
    return session ? session->state : YVEX_SESSION_STATE_CLOSED;
}

unsigned long long yvex_session_position(const yvex_session *session)
{
    return session ? session->position : 0;
}

unsigned long long yvex_session_context_length(const yvex_session *session)
{
    return session ? session->context_length : 0;
}

int yvex_session_get_summary(const yvex_session *session,
                             yvex_session_summary *out,
                             yvex_error *err)
{
    yvex_kv_summary kv_summary;
    yvex_logits_summary logits_summary;
    yvex_engine_summary engine_summary;

    if (!session || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_get_summary", "session and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->state = session->state;
    out->engine_status = yvex_engine_status_name(yvex_engine_status_of(session->engine));
    out->backend_kind = yvex_backend_kind_name(yvex_backend_kind_of(session->backend));
    out->backend_status = yvex_backend_status_name(yvex_backend_status_of(session->backend));
    out->context_length = session->context_length;
    out->position = session->position;
    out->accepted_tokens = session->accepted_tokens;
    out->rejected_tokens = session->rejected_tokens;
    out->graph_partial = session->graph_partial;
    out->backend_available = session->backend_available;
    out->execution_ready = 0;
    out->graph_execution_ready = 0;

    if (yvex_engine_get_summary(session->engine, &engine_summary, err) == YVEX_OK) {
        out->weights_attached = engine_summary.weights_attached;
        out->weights_backend = engine_summary.weights_backend;
        out->weight_tensor_count = engine_summary.weight_tensor_count;
        out->weight_total_bytes = engine_summary.weight_total_bytes;
    }

    if (session->kv && yvex_kv_cache_get_summary(session->kv, &kv_summary, err) == YVEX_OK) {
        out->kv_status = yvex_kv_status_name(kv_summary.status);
        out->kv_bytes = kv_summary.bytes;
        out->kv_owner = kv_summary.owner;
        out->kv_dtype = kv_summary.dtype;
        out->kv_layers = kv_summary.layer_count;
        out->kv_heads = kv_summary.kv_head_count;
        out->kv_head_dim = kv_summary.head_dim;
        out->kv_capacity = kv_summary.context_length;
        out->kv_bytes_per_position = kv_summary.bytes_per_position;
        out->kv_allocated_bytes = kv_summary.allocated_bytes;
        out->kv_written_positions = kv_summary.written_positions;
        out->kv_append_count = kv_summary.append_count;
        out->kv_read_count = kv_summary.read_count;
        out->kv_overflow_status = kv_summary.overflow_status;
        out->kv_cleanup_status = kv_summary.cleanup_status;
        out->kv_session_owned = kv_summary.session_owned;
    } else {
        out->kv_status = "empty";
        out->kv_owner = "none";
        out->kv_dtype = "none";
        out->kv_overflow_status = "not-checked";
        out->kv_cleanup_status = "not-needed";
    }
    if (session->logits && yvex_logits_get_summary(session->logits, &logits_summary, err) == YVEX_OK) {
        out->logits_status = yvex_logits_status_name(logits_summary.status);
        out->logits_capacity = logits_summary.vocab_size;
    } else {
        out->logits_status = "empty";
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

const char *yvex_session_diagnostic_reason(const yvex_session *session)
{
    return session ? session->reason : "";
}

int yvex_session_accept_tokens(yvex_session *session,
                               const yvex_tokens *tokens,
                               yvex_error *err)
{
    if (!session || !tokens) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_accept_tokens",
                       "session and tokens are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_accept_tokens", "session is closed");
        return YVEX_ERR_STATE;
    }
    if (tokens->len > session->context_length ||
        session->position > session->context_length - tokens->len) {
        session->rejected_tokens += tokens->len;
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_session_accept_tokens",
                        "accepting %llu tokens would exceed context length %llu",
                        tokens->len, session->context_length);
        return YVEX_ERR_BOUNDS;
    }

    session->position += tokens->len;
    session->accepted_tokens += tokens->len;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_session_prefill(yvex_session *session,
                         const yvex_tokens *tokens,
                         yvex_error *err)
{
    (void)tokens;

    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_prefill", "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->graph_partial) {
        session->state = YVEX_SESSION_STATE_PARTIAL;
        set_session_reason_from_graph(session);
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_session_prefill",
                        "prefill is not executable in engine/session layer because %s", session->reason);
        return YVEX_ERR_UNSUPPORTED;
    }

    yvex_runtime_set_text_reason(session->reason, sizeof(session->reason),
                                 "prefill runtime not implemented in engine/session layer");
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_prefill",
                   "prefill runtime is not implemented in engine/session layer");
    return YVEX_ERR_UNSUPPORTED;
}

int yvex_session_decode_next(yvex_session *session,
                             unsigned int *out_token,
                             yvex_error *err)
{
    if (!session || !out_token) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_decode_next",
                       "session and out_token are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out_token = 0;
    yvex_runtime_set_text_reason(session->reason, sizeof(session->reason),
                                 "decode runtime not implemented in engine/session layer");
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_session_decode_next",
                   "decode runtime is not implemented in engine/session layer");
    return YVEX_ERR_UNSUPPORTED;
}

int yvex_session_cancel(yvex_session *session,
                        yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_cancel", "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_cancel", "session is closed");
        return YVEX_ERR_STATE;
    }
    session->state = YVEX_SESSION_STATE_CANCELLED;
    yvex_runtime_set_text_reason(session->reason, sizeof(session->reason), "session cancelled");
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_session_reset(yvex_session *session,
                       yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_reset", "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_reset", "session is closed");
        return YVEX_ERR_STATE;
    }

    session->position = 0;
    session->accepted_tokens = 0;
    session->rejected_tokens = 0;
    (void)yvex_kv_cache_clear(session->kv, err);
    session->state = reset_state_for_graph(session);
    set_session_reason_from_graph(session);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_session_kv_append_position_f32(yvex_session *session,
                                        const float *values,
                                        unsigned long long value_count,
                                        unsigned long long *out_position,
                                        yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_kv_append_position_f32",
                       "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_kv_append_position_f32",
                       "session is closed");
        return YVEX_ERR_STATE;
    }
    return yvex_kv_cache_append_position_f32(session->kv, values, value_count, out_position, err);
}

int yvex_session_kv_read_position_f32(yvex_session *session,
                                      unsigned long long position,
                                      float *out_values,
                                      unsigned long long value_count,
                                      yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_kv_read_position_f32",
                       "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_kv_read_position_f32",
                       "session is closed");
        return YVEX_ERR_STATE;
    }
    return yvex_kv_cache_read_position_f32(session->kv, position, out_values, value_count, err);
}

int yvex_session_kv_clear(yvex_session *session,
                          yvex_error *err)
{
    if (!session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_session_kv_clear",
                       "session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->state == YVEX_SESSION_STATE_CLOSED) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_session_kv_clear", "session is closed");
        return YVEX_ERR_STATE;
    }
    return yvex_kv_cache_clear(session->kv, err);
}


const char *yvex_session_state_name(yvex_session_state state)
{
    switch (state) {
    case YVEX_SESSION_STATE_CREATED: return "created";
    case YVEX_SESSION_STATE_READY: return "ready";
    case YVEX_SESSION_STATE_PREFILLING: return "prefilling";
    case YVEX_SESSION_STATE_DECODING: return "decoding";
    case YVEX_SESSION_STATE_PARTIAL: return "partial";
    case YVEX_SESSION_STATE_CANCELLED: return "cancelled";
    case YVEX_SESSION_STATE_FAILED: return "failed";
    case YVEX_SESSION_STATE_CLOSED: return "closed";
    }
    return "unknown";
}

/* Shared operator command helpers. */

int print_yvex_error(const yvex_error *err, int exit_code)
{
    fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

int exit_for_status(int status)
{
    switch (status) {
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
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

void print_quoted_bytes(const char *data, unsigned long long len)
{
    unsigned long long i;

    putchar('"');
    for (i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar((int)ch);
        } else if (ch == '\n') {
            printf("\\n");
        } else if (ch == '\r') {
            printf("\\r");
        } else if (ch == '\t') {
            printf("\\t");
        } else if (ch < 32 || ch > 126) {
            printf("\\x%02x", (unsigned int)ch);
        } else {
            putchar((int)ch);
        }
    }
    putchar('"');
}

int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;
    yvex_model_ref ref;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));

    rc = yvex_model_ref_resolve(&ref, path, NULL, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    options.path = ref.path;
    options.readonly = 1;
    options.map = 1;

    rc = yvex_artifact_open(artifact, &options, err);
    yvex_model_ref_clear(&ref);
    return rc;
}

void close_tokenizer_context(yvex_cli_tokenizer_context *ctx)
{
    if (!ctx) {
        return;
    }
    yvex_tokenizer_close(ctx->tokenizer);
    ctx->tokenizer = NULL;
    yvex_model_descriptor_close(ctx->model);
    yvex_tensor_table_close(ctx->table);
    yvex_gguf_close(ctx->gguf);
    yvex_artifact_close(ctx->artifact);
    memset(ctx, 0, sizeof(*ctx));
}

void close_model_context(yvex_cli_tokenizer_context *ctx)
{
    if (!ctx) {
        return;
    }
    yvex_model_descriptor_close(ctx->model);
    yvex_tensor_table_close(ctx->table);
    yvex_gguf_close(ctx->gguf);
    yvex_artifact_close(ctx->artifact);
    memset(ctx, 0, sizeof(*ctx));
}

int open_model_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err)
{
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    int rc;

    memset(ctx, 0, sizeof(*ctx));
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
    rc = open_artifact_for_gguf(path, &ctx->artifact, err);
    if (rc == YVEX_OK) {
        rc = yvex_gguf_open(&ctx->gguf, ctx->artifact, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&ctx->table, ctx->gguf, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&ctx->model, ctx->gguf, ctx->table, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(ctx->artifact,
                                              ctx->gguf,
                                              ctx->table,
                                              &integrity_options,
                                              &integrity_report,
                                              err);
    }
    if (rc != YVEX_OK) {
        close_model_context(ctx);
    }
    return rc;
}

int open_tokenizer_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err)
{
    int rc = open_model_context(path, ctx, err);

    if (rc == YVEX_OK) {
        rc = yvex_tokenizer_from_gguf(&ctx->tokenizer, ctx->gguf, ctx->model, err);
    }
    if (rc != YVEX_OK) {
        close_tokenizer_context(ctx);
    }
    return rc;
}

void print_tensor_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int d;

    printf("[");
    for (d = 0; d < rank; ++d) {
        if (d > 0) {
            printf(",");
        }
        printf("%llu", dims[d]);
    }
    printf("]");
}

void print_native_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    printf("[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            printf(",");
        }
        printf("%llu", dims[i]);
    }
    printf("]");
}

void print_token_ids(const yvex_tokens *tokens)
{
    unsigned long long i;

    printf("ids:");
    for (i = 0; i < tokens->len; ++i) {
        printf(" %u", tokens->ids[i]);
    }
    printf("\n");
}

int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len)
{
    unsigned int *ids = NULL;
    unsigned long long len = 0;
    unsigned long long cap = 0;
    const char *p = text;

    *out_ids = NULL;
    *out_len = 0;

    while (*p) {
        char *end = NULL;
        unsigned long value;
        unsigned int *next;

        value = strtoul(p, &end, 10);
        if (end == p || value > 0xfffffffful) {
            free(ids);
            return 0;
        }
        if (len == cap) {
            unsigned long long next_cap = cap == 0 ? 8 : cap * 2u;
            if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*ids))) {
                free(ids);
                return 0;
            }
            next = (unsigned int *)realloc(ids, (size_t)next_cap * sizeof(*ids));
            if (!next) {
                free(ids);
                return 0;
            }
            ids = next;
            cap = next_cap;
        }
        ids[len++] = (unsigned int)value;
        if (*end == ',') {
            p = end + 1;
        } else if (*end == '\0') {
            p = end;
        } else {
            free(ids);
            return 0;
        }
    }

    *out_ids = ids;
    *out_len = len;
    return len > 0;
}

int parse_positive_ull(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

int parse_ull_allow_zero(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

int parse_uint_allow_zero(const char *text, unsigned int *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 0xffffffffull) {
        return 0;
    }
    *out = (unsigned int)value;
    return 1;
}

int parse_dims_csv(const char *text,
                          unsigned int rank,
                          unsigned long long dims[4])
{
    const char *p = text;
    char *end = NULL;
    unsigned int i;

    if (!text || !dims || rank == 0 || rank > 4u) return 0;
    for (i = 0; i < 4u; ++i) dims[i] = 0;
    for (i = 0; i < rank; ++i) {
        unsigned long long value;
        errno = 0;
        value = strtoull(p, &end, 10);
        if (errno != 0 || end == p || value == 0) return 0;
        dims[i] = value;
        if (i + 1u < rank) {
            if (*end != ',') return 0;
            p = end + 1;
        } else if (*end != '\0') {
            return 0;
        }
    }
    return 1;
}

/* Runtime-owned command surface. Domain-specific commands live with their owners. */

static void print_token_input_tokens(const yvex_token_input *input)
{
    unsigned long long i;

    for (i = 0; input && i < input->token_count; ++i) {
        printf("token_%llu: %u\n", i, input->tokens[i]);
    }
}

void print_token_input_summary(const yvex_token_input *input,
                                      const char *status,
                                      const char *bounds_status,
                                      unsigned long long selected_index,
                                      unsigned int selected_token,
                                      int has_selected)
{
    printf("token_input_status: %s\n", status ? status : "fail");
    printf("token_input_kind: %s\n",
           input ? yvex_token_input_kind_name(input->kind) : "unknown");
    printf("token_count: %llu\n", input ? input->token_count : 0ull);
    if (input) {
        printf("selected_token_index: %llu\n", selected_index);
    }
    if (has_selected) {
        printf("selected_token_id: %u\n", selected_token);
    } else if (input) {
        printf("selected_token_id: unavailable\n");
    }
    printf("token_bounds_status: %s\n", bounds_status ? bounds_status : "not-checked");
}

int cli_token_input_vocab_from_model(const char *path,
                                            unsigned long long *out_vocab_size,
                                            yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *tensor;
    yvex_tokenizer *tokenizer = NULL;
    int rc;

    if (!path || !out_vocab_size) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cli_token_input_vocab_from_model",
                       "path and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out_vocab_size = 0ull;
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (tensor && tensor->rank == 2 && tensor->dims[1] > 0ull) {
        *out_vocab_size = tensor->dims[1];
        close_model_context(&ctx);
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_tokenizer_from_gguf(&tokenizer, ctx.gguf, ctx.model, err);
    if (rc == YVEX_OK && yvex_tokenizer_vocab_size(tokenizer) > 0ull) {
        *out_vocab_size = yvex_tokenizer_vocab_size(tokenizer);
        yvex_tokenizer_close(tokenizer);
        close_model_context(&ctx);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    yvex_tokenizer_close(tokenizer);
    close_model_context(&ctx);
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cli_token_input_vocab_from_model",
                   "tokenizer-metadata-missing");
    return YVEX_ERR_UNSUPPORTED;
}

static int command_input(int argc, char **argv)
{
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_token_input input;
    yvex_tokens tokens;
    yvex_error err;
    const char *subcommand;
    const char *model_arg = NULL;
    const char *tokens_text = NULL;
    const char *prompt_text = NULL;
    unsigned long long vocab_size = 0ull;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&input, 0, sizeof(input));
    memset(&tokens, 0, sizeof(tokens));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_input_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    subcommand = argv[2];
    if (strcmp(subcommand, "tokens") != 0 && strcmp(subcommand, "prompt") != 0) {
        fprintf(stderr, "yvex: input requires tokens or prompt\n");
        fprintf(stderr, "usage: yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0 && strcmp(subcommand, "tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--text") == 0 && strcmp(subcommand, "prompt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires TEXT\n");
                return 2;
            }
            prompt_text = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown input option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help input' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: input requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (strcmp(subcommand, "tokens") == 0 && !tokens_text) {
        fprintf(stderr, "yvex: input tokens requires --tokens IDS\n");
        return 2;
    }
    if (strcmp(subcommand, "prompt") == 0 && !prompt_text) {
        fprintf(stderr, "yvex: input prompt requires --text TEXT\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&ref, "input");
    if (rc != YVEX_OK) {
        printf("token_input: %s\n", subcommand);
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("identity_status: fail\n");
        print_token_input_summary(NULL, "fail", "not-checked", 0ull, 0u, 0);
        printf("prefill_ready: false\n");
        printf("status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    if (strcmp(subcommand, "tokens") == 0) {
        rc = yvex_token_input_parse_explicit(tokens_text, &input, &err);
        if (rc == YVEX_OK) {
            rc = cli_token_input_vocab_from_model(ref.path, &vocab_size, &err);
        }
        if (rc == YVEX_OK) {
            rc = yvex_token_input_validate_bounds(&input, vocab_size, &err);
        }

        printf("token_input: tokens\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("identity_status: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered");
        print_token_input_summary(&input,
                                  rc == YVEX_OK ? "pass" : "fail",
                                  rc == YVEX_OK ? "pass" :
                                  input.token_bounds_checked ? "fail" : "not-checked",
                                  0ull,
                                  input.token_count > 0ull ? input.tokens[0] : 0u,
                                  input.token_count > 0ull);
        printf("vocab_size: %llu\n", vocab_size);
        print_token_input_tokens(&input);
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");
        yvex_model_ref_clear(&ref);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        return 0;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: not-checked\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenizer_from_gguf(&ctx.tokenizer, ctx.gguf, ctx.model, &err);
    if (rc != YVEX_OK) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: missing\n");
        printf("reason: tokenizer-metadata-missing\n");
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }
    if (yvex_tokenizer_support_of(ctx.tokenizer) != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: unsupported\n");
        printf("tokenizer_support: %s\n",
               yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
        printf("reason: tokenizer-metadata-missing\n");
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        close_tokenizer_context(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }

    rc = yvex_tokenize_text(ctx.tokenizer, prompt_text, &tokens, &err);
    if (rc == YVEX_OK) {
        rc = yvex_token_input_from_ids(YVEX_TOKEN_INPUT_PROMPT_TEXT,
                                       tokens.ids,
                                       tokens.len,
                                       &input,
                                       &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&input,
                                              yvex_tokenizer_vocab_size(ctx.tokenizer),
                                              &err);
    }

    printf("token_input: prompt\n");
    printf("model: %s\n", model_arg);
    printf("resolved_path: %s\n", ref.path ? ref.path : "");
    printf("model_input_kind: %s\n",
           ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    printf("tokenizer_status: present\n");
    printf("tokenizer_support: %s\n",
           yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    print_token_input_summary(&input,
                              rc == YVEX_OK ? "pass" : "fail",
                              rc == YVEX_OK ? "pass" :
                              input.token_bounds_checked ? "fail" : "not-checked",
                              0ull,
                              input.token_count > 0ull ? input.tokens[0] : 0u,
                              input.token_count > 0ull);
    printf("vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    print_token_input_tokens(&input);
    printf("prefill_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");

    yvex_tokens_free(&tokens);
    close_tokenizer_context(&ctx);
    yvex_model_ref_clear(&ref);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_engine(int argc, char **argv)
{
    yvex_engine *engine = NULL;
    yvex_model_ref model_ref;
    yvex_engine_options options;
    yvex_engine_summary summary;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&options, 0, sizeof(options));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_engine_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a file or alias\n");
                return 2;
            }
            if (model_arg) {
                fprintf(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu or cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "yvex: unknown engine option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help engine' for usage.\n");
            return 2;
        } else {
            if (model_arg) {
                fprintf(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = argv[i];
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: engine requires FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n");
        return 2;
    }
    if (backend_name && strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (backend_name) {
        rc = enforce_registered_identity_cli(&model_ref, "engine");
        if (rc != YVEX_OK) {
            yvex_model_ref_clear(&model_ref);
            return exit_for_status(rc);
        }
    }

    options.model_path = model_ref.path;
    options.load_tokenizer = backend_name ? 0 : 1;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    options.attach_weights = backend_name != NULL;
    options.backend_name = backend_name;
    options.require_all_weights = 1;

    rc = yvex_engine_open(&engine, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: engine-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(engine, &summary, &err);
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("engine status: %s\n", yvex_engine_status_name(summary.status));
    printf("format: gguf\n");
    printf("architecture: %s\n", summary.architecture);
    printf("model_name: %s\n", summary.model_name);
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("known_tensor_bytes: %llu\n", summary.known_tensor_bytes);
    printf("unsupported_tensor_accounting: %llu\n", summary.unsupported_tensor_accounting);
    printf("tokenizer_model: %s\n", summary.tokenizer_model);
    printf("tokenizer_support: %s\n", summary.tokenizer_support);
    printf("graph_status: %s\n", summary.graph_status);
    printf("weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    printf("weights_backend: %s\n", summary.weights_backend);
    printf("weight_tensor_count: %llu\n", summary.weight_tensor_count);
    printf("weight_total_bytes: %llu\n", summary.weight_total_bytes);
    printf("weight_backend_allocated_bytes: %llu\n", summary.weight_backend_allocated_bytes);
    if (summary.weights_attached) {
        const yvex_tensor_table *table = yvex_engine_tensors(engine);
        unsigned long long count = yvex_tensor_table_count(table);
        unsigned long long j;

        for (j = 0; j < count; ++j) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(table, j);
            unsigned int d;

            if (!tensor) {
                continue;
            }
            printf("attached_weight_%llu: %s role=%s rank=%u dims=[",
                   j,
                   tensor->name ? tensor->name : "",
                   yvex_tensor_role_name(tensor->role),
                   tensor->rank);
            for (d = 0; d < tensor->rank; ++d) {
                if (d > 0) {
                    printf(",");
                }
                printf("%llu", tensor->dims[d]);
            }
            printf("] dtype=%s bytes=%llu\n",
                   yvex_dtype_name(tensor->dtype),
                   tensor->storage_bytes);
        }
    }
    printf("execution_ready: false\n");
    printf("graph_execution_ready: false\n");
    printf("reason: %s\n", yvex_engine_diagnostic_reason(engine));
    printf("status: %s\n", summary.weights_attached ? "engine-weights-attached" : "engine-descriptor");

    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static int command_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("name: YVEX\n");
    printf("version: %s\n", yvex_version_string());
    printf("language: C\n");
    printf("interface: CLI-only\n");
    printf("status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, standalone RoPE, attention, matmul, and MLP ops, explicit token input boundary, prefill state foundation, minimal KV binding, and minimal KV ownership\n");
    printf("library: libyvex.a\n");
    printf("filesystem: implemented\n");
    printf("artifact: open/read implemented\n");
    printf("gguf: metadata/tensor directory parsing implemented\n");
    printf("model: descriptor-only implemented\n");
    printf("tokenizer: fixture encode/decode implemented\n");
    printf("token_input: explicit token boundary implemented\n");
    printf("prefill_state: segment-summary foundation and minimal KV binding implemented\n");
    printf("prompt: default renderer implemented\n");
    printf("graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 attention primitive, standalone F32 matmul/projection primitive, and standalone F32 MLP/feed-forward primitive implemented\n");
    printf("planner: estimate-only implemented\n");
    printf("backend: CPU reference implemented\n");
    printf("backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention primitive, F32 matmul/projection primitive, and F32 MLP/feed-forward primitive implemented when CUDA is available\n");
    printf("weights: selected tensor materialization implemented\n");
    printf("engine: descriptor open and selected-weight attachment implemented\n");
    printf("session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented\n");
    printf("run: accepted-only runtime shell implemented\n");
    printf("chat: accepted-only REPL shell implemented\n");
    printf("metrics: runtime collector implemented\n");
    printf("trace: JSONL writer implemented\n");
    printf("profile: JSON writer implemented\n");
    printf("run_artifacts: metrics/trace/profile files implemented\n");
    printf("source_manifest: provenance JSON writer implemented\n");
    printf("native_weights: safetensors header inventory implemented\n");
    printf("gguf_template: contract validator implemented\n");
    printf("gguf_emit: controlled GGUF writer implemented\n");
    printf("conversion: open-weight selected tensor bridge implemented\n");
    printf("model_ref: alias-or-path resolver implemented\n");
    printf("model_registry: local model alias registry implemented\n");
    printf("quant_job: external quantization job manifest implemented\n");
    printf("qtype_support: conversion support matrix implemented\n");
    printf("weight_mapping: tensor adapter contract implemented\n");
    printf("quant_policy: manifest validator implemented\n");
    printf("imatrix: calibration artifact manifest implemented\n");
    printf("server_binary: yvexd shell implemented\n");
    printf("server_endpoints: health/metrics/models status implemented\n");
    printf("server_generation: not implemented\n");
    printf("kv: minimal session-owned append/read boundary implemented\n");
    printf("logits: unavailable skeleton implemented\n");
    printf("generation: unsupported\n");
    printf("inference: not implemented\n");
    printf("cuda: available when local driver/device probe succeeds\n");
    printf("server: yvexd status shell implemented\n");
    return 0;
}

static void print_prefill_state_summary(const yvex_prefill_state_summary *summary,
                                        const char *model_arg,
                                        const char *backend_name,
                                        const char *token_input_status,
                                        const char *status)
{
    unsigned long long i;

    printf("prefill: state\n");
    printf("prefill_state_created: %s\n",
           summary && summary->prefill_state_created ? "true" : "false");
    printf("prefill_state_kind: %s\n",
           summary && summary->prefill_state_kind ? summary->prefill_state_kind : "segment-summary");
    printf("sequence_execution_mode: %s\n",
           summary && summary->sequence_execution_mode
               ? summary->sequence_execution_mode
               : "independent-token-segments");
    printf("prefill_phase: %s\n",
           summary && summary->prefill_phase ? summary->prefill_phase : "preflight");
    printf("model: %s\n", model_arg ? model_arg : "");
    printf("backend: %s\n",
           summary && summary->backend_name && strcmp(summary->backend_name, "none") != 0
               ? summary->backend_name
               : (backend_name ? backend_name : "cpu"));
    printf("segment: %s\n",
           summary && summary->segment_name ? summary->segment_name : "embedding-rmsnorm");
    printf("token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    printf("token_count: %llu\n", summary ? summary->token_count : 0ull);
    printf("tokens_processed: %llu\n", summary ? summary->tokens_processed : 0ull);
    printf("position_start: %llu\n", summary ? summary->position_start : 0ull);
    printf("position_end: %llu\n", summary ? summary->position_end : 0ull);
    printf("failed_token_index: %llu\n", summary ? summary->failed_token_index : 0ull);
    printf("segment_graph_executions: %llu\n", summary ? summary->segment_graph_executions : 0ull);
    printf("segment_output_count: %llu\n", summary ? summary->segment_output_count : 0ull);
    printf("segment_output_bytes: %llu\n", summary ? summary->segment_output_bytes : 0ull);
    printf("prefill_aggregate_checksum: %llu\n", summary ? summary->aggregate_checksum : 0ull);
    printf("prefill_final_token_checksum: %llu\n", summary ? summary->final_token_checksum : 0ull);
    printf("prefill_total_output_bytes: %llu\n", summary ? summary->total_output_bytes : 0ull);
    printf("prefill_scratch_bytes: %llu\n", summary ? summary->scratch_bytes : 0ull);
    printf("prefill_max_abs_diff: %.9g\n", summary ? summary->max_abs_diff : 0.0);
    printf("cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    if (summary && summary->cuda_parity) {
        printf("prefill_cuda_parity: pass\n");
    }
    printf("kv_ready: %s\n", summary && summary->kv_ready ? "true" : "false");
    printf("session_kv_owned: %s\n",
           summary && summary->session_kv_owned ? "true" : "false");
    printf("kv_bound_to_prefill: %s\n",
           summary && summary->kv_bound_to_prefill ? "true" : "false");
    printf("kv_binding_kind: %s\n",
           summary && summary->kv_binding_kind ? summary->kv_binding_kind : "none");
    printf("kv_status: %s\n",
           summary && summary->kv_status ? summary->kv_status : "not-requested");
    printf("kv_owner: %s\n",
           summary && summary->kv_owner ? summary->kv_owner : "none");
    printf("kv_dtype: %s\n",
           summary && summary->kv_dtype ? summary->kv_dtype : "none");
    printf("kv_layers: %llu\n", summary ? summary->kv_layers : 0ull);
    printf("kv_heads: %llu\n", summary ? summary->kv_heads : 0ull);
    printf("kv_head_dim: %llu\n", summary ? summary->kv_head_dim : 0ull);
    printf("kv_capacity: %llu\n", summary ? summary->kv_capacity : 0ull);
    printf("kv_values_per_position: %llu\n", summary ? summary->kv_values_per_position : 0ull);
    printf("kv_bytes_per_position: %llu\n", summary ? summary->kv_bytes_per_position : 0ull);
    printf("kv_planned_bytes: %llu\n", summary ? summary->kv_planned_bytes : 0ull);
    printf("kv_allocated_bytes: %llu\n", summary ? summary->kv_allocated_bytes : 0ull);
    printf("kv_positions_written: %llu\n", summary ? summary->kv_positions_written : 0ull);
    printf("kv_append_count: %llu\n", summary ? summary->kv_append_count : 0ull);
    printf("kv_read_count: %llu\n", summary ? summary->kv_read_count : 0ull);
    printf("kv_read_position: %llu\n", summary ? summary->kv_read_position : 0ull);
    printf("kv_read_value_count: %llu\n", summary ? summary->kv_read_value_count : 0ull);
    printf("kv_read_checksum: %llu\n", summary ? summary->kv_read_checksum : 0ull);
    printf("kv_read_sample_values:");
    if (summary && summary->kv_read_sample_count > 0ull) {
        for (i = 0; i < summary->kv_read_sample_count; ++i) {
            printf("%s%.9g", i == 0 ? " " : ",", (double)summary->kv_read_sample_values[i]);
        }
    }
    printf("\n");
    printf("kv_overflow: %s\n",
           summary && summary->kv_overflow_status ? summary->kv_overflow_status : "not-checked");
    printf("kv_cleanup_status: %s\n",
           summary && summary->kv_cleanup_status ? summary->kv_cleanup_status : "not-needed");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", status ? status : "prefill-state-fail");
}

static void init_prefill_summary_cli_defaults(yvex_prefill_state_summary *summary,
                                              const char *segment_name,
                                              int attach_kv,
                                              const yvex_kv_shape *shape)
{
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->prefill_state_kind = "segment-summary";
    summary->sequence_execution_mode = "independent-token-segments";
    summary->prefill_phase = "preflight";
    summary->segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    summary->cleanup_status = "not-needed";
    summary->generation_status = "unsupported";
    summary->kv_binding_kind = attach_kv ? "minimal-diagnostic" : "none";
    summary->kv_status = attach_kv ? "planned" : "not-requested";
    summary->kv_owner = "none";
    summary->kv_dtype = "none";
    summary->kv_overflow_status = "not-checked";
    summary->kv_cleanup_status = "not-needed";
    if (shape) {
        summary->kv_layers = shape->layer_count;
        summary->kv_heads = shape->kv_head_count;
        summary->kv_head_dim = shape->head_dim;
        summary->kv_capacity = shape->capacity;
    }
}

static int command_prefill(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_prefill_state_options prefill_options;
    yvex_prefill_state_summary prefill_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = "cpu";
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    yvex_kv_shape kv_shape;
    unsigned long long vocab_size = 0ull;
    int attach_kv = 0;
    int kv_shape_seen = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_prefill_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.layer_count)) {
                fprintf(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.kv_head_count)) {
                fprintf(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.head_dim)) {
                fprintf(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.capacity)) {
                fprintf(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (!model_arg) {
            model_arg = argv[i];
        } else {
            fprintf(stderr, "yvex: unknown prefill option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help prefill' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !tokens_text || !segment_name) {
        fprintf(stderr, "usage: yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n");
        return 2;
    }
    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        fprintf(stderr, "yvex: unsupported prefill segment: %s\n", segment_name);
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        fprintf(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        fprintf(stderr, "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "prefill");
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
    if (rc == YVEX_OK) {
        rc = cli_token_input_vocab_from_model(model_ref.path, &vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, vocab_size, &err);
    }
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = preflight_graph_guard(&model_ref,
                               backend_name,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        print_graph_guard_report(&graph_guard);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    prefill_options.token_input = &token_input;
    prefill_options.segment_name = segment_name;
    prefill_options.position_start = 0ull;
    prefill_options.attach_kv = attach_kv;
    prefill_options.kv_shape = kv_shape;
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, &err);
    if (rc != YVEX_OK) {
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static int command_plan(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_plan *plan = NULL;
    yvex_plan_options options;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1;
    options.backend_name = "cpu";

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_plan_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.sequence_length)) {
                fprintf(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            options.backend_name = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown plan option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help plan' for usage.\n");
            return 2;
        }
    }

    rc = open_model_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_plan_create(&plan, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_plan_dump(plan, stdout, &err);
    }

    yvex_plan_close(plan);
    close_model_context(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static void trim_line(char *line)
{
    size_t len;

    if (!line) {
        return;
    }
    len = strlen(line);
    while (len > 0 && (line[len - 1u] == '\n' || line[len - 1u] == '\r')) {
        line[len - 1u] = '\0';
        len -= 1u;
    }
}

static void cli_copy_text(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    snprintf(dst, cap, "%s", src ? src : "");
    dst[cap - 1u] = '\0';
}

static void cli_set_result_artifacts(yvex_chat_accept_result *result,
                                     const yvex_run_artifacts *artifacts)
{
    if (!result || !artifacts) {
        return;
    }
    cli_copy_text(result->run_id, sizeof(result->run_id), artifacts->run_id);
    cli_copy_text(result->run_dir, sizeof(result->run_dir), artifacts->run_dir);
    if (artifacts->has_metrics) {
        cli_copy_text(result->metrics_out, sizeof(result->metrics_out), artifacts->metrics_path);
    }
    if (artifacts->has_trace) {
        cli_copy_text(result->trace_out, sizeof(result->trace_out), artifacts->trace_path);
    }
    if (artifacts->has_profile) {
        cli_copy_text(result->profile_out, sizeof(result->profile_out), artifacts->profile_path);
    }
}

static int cli_write_observability_files(const char *command_name,
                                         const char *model_name,
                                         const char *backend_name,
                                         const char *status,
                                         const yvex_run_artifacts *artifacts,
                                         const yvex_metrics *metrics,
                                         int argc,
                                         char **argv,
                                         yvex_error *err)
{
    yvex_profile_summary profile;
    yvex_metric_counters counters;
    int rc;

    if (!artifacts || !metrics) {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_run_artifacts_write_command(artifacts, argc, argv, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (artifacts->has_metrics) {
        rc = yvex_metrics_write_json(artifacts->metrics_path, metrics, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (artifacts->has_profile) {
        rc = yvex_metrics_get_counters(metrics, &counters, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        memset(&profile, 0, sizeof(profile));
        profile.run_id = artifacts->run_id;
        profile.command = command_name;
        profile.model_name = model_name;
        profile.backend_name = backend_name;
        profile.status = status;
        profile.execution_ready = 0;
        profile.counters = counters;
        rc = yvex_profile_write_json(artifacts->profile_path, &profile, metrics, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

static int command_run(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_chat_accept_result result;
    yvex_engine_summary engine_summary;
    yvex_model_ref model_ref;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
    yvex_trace_options trace_options;
    yvex_run_artifacts artifacts;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    const char *prompt_text = NULL;
    const char *system_text = NULL;
    const char *output = "plain";
    const char *status_line = "off";
    const char *metrics_out = NULL;
    const char *trace_out = NULL;
    const char *profile_out = NULL;
    const char *run_dir = NULL;
    unsigned long long context_length = 0;
    unsigned long long phase_token = 0;
    unsigned long long total_token = 0;
    int save_run = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc == 2 || (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0))) {
        yvex_run_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--prompt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --prompt requires a value\n");
                return 2;
            }
            prompt_text = argv[++i];
        } else if (strcmp(argv[i], "--system") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --system requires a value\n");
                return 2;
            }
            system_text = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --output requires plain or json\n");
                return 2;
            }
            output = argv[++i];
            if (strcmp(output, "plain") != 0 && strcmp(output, "json") != 0) {
                fprintf(stderr, "yvex: --output must be plain or json\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--status-line") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
            status_line = argv[++i];
            if (strcmp(status_line, "auto") != 0 && strcmp(status_line, "off") != 0 &&
                strcmp(status_line, "always") != 0) {
                fprintf(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--metrics-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = argv[++i];
        } else if (strcmp(argv[i], "--trace-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = argv[++i];
        } else if (strcmp(argv[i], "--profile-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = argv[++i];
        } else if (strcmp(argv[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(argv[i], "--run-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown run option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help run' for usage.\n");
            return 2;
        }
    }

    if (!model_path) {
        fprintf(stderr, "yvex: --model is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!prompt_text) {
        fprintf(stderr, "yvex: --prompt is required for yvex run in diagnostic runtime\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "run", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("run status: backend-unsupported\n");
        printf("backend: cuda\n");
        printf("execution_ready: false\n");
        printf("reason: %s\n", yvex_error_message(&err));
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "backend-unsupported",
                              yvex_error_message(&err), &err);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    if (yvex_engine_get_summary(runtime.engine, &engine_summary, &err) == YVEX_OK) {
        (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                           engine_summary.unsupported_tensor_accounting, &err);
    }

    rc = yvex_chat_runtime_accept_user_text(&runtime, system_text, prompt_text, &result, &err);
    if (rc != YVEX_OK) {
        yvex_chat_runtime_close(&runtime);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "accepted-only",
                          "decode runtime is not implemented in diagnostic runtime", &err);
    cli_set_result_artifacts(&result, &artifacts);

    if (strcmp(status_line, "always") == 0) {
        (void)yvex_status_line_print(stderr, "accept", result.prompt_tokens, result.position);
    }

    if (strcmp(output, "json") == 0) {
        (void)yvex_run_command_json(stdout, &result);
    } else {
        (void)yvex_run_command_plain(stdout, &result);
    }

    yvex_trace_close(trace);
    rc = cli_write_observability_files("run", result.model_name, result.backend_name,
                                       "accepted-only", &artifacts, metrics, argc, argv, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_close(&runtime);
    yvex_metrics_close(metrics);
    yvex_model_ref_clear(&model_ref);
    if (final_rc != 0) {
        return final_rc;
    }
    return 0;
}

static int command_session(int argc, char **argv)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_session *session = NULL;
    yvex_model_ref model_ref;
    yvex_engine_options engine_options;
    yvex_session_options session_options;
    yvex_session_summary summary;
    yvex_backend_options backend_options;
    yvex_tokens tokens;
    yvex_error err;
    const char *backend_name = "cpu";
    const char *text = NULL;
    int accept_tokens = 0;
    int tokenized = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&session_options, 0, sizeof(session_options));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&tokens, 0, sizeof(tokens));
    memset(&model_ref, 0, sizeof(model_ref));
    session_options.allow_partial_graph = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_session_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &session_options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--text") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = argv[++i];
        } else if (strcmp(argv[i], "--accept-tokens") == 0) {
            accept_tokens = 1;
        } else {
            fprintf(stderr, "yvex: unknown session option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help session' for usage.\n");
            return 2;
        }
    }

    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, argv[2], NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "session");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = text ? 1 : 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;

    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: session-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        yvex_engine_close(engine);
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: session-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_session_create(&session, engine, backend, &session_options, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (text) {
        rc = yvex_tokenize_text(yvex_engine_tokenizer(engine), text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_session_close(session);
            yvex_backend_close(backend);
            yvex_engine_close(engine);
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        tokenized = 1;
        if (accept_tokens) {
            rc = yvex_session_accept_tokens(session, &tokens, &err);
            if (rc != YVEX_OK) {
                yvex_tokens_free(&tokens);
                yvex_session_close(session);
                yvex_backend_close(backend);
                yvex_engine_close(engine);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
    }

    rc = yvex_session_get_summary(session, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_tokens_free(&tokens);
        yvex_session_close(session);
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("engine_status: %s\n", summary.engine_status);
    printf("backend: %s\n", summary.backend_kind);
    printf("backend_status: %s\n", summary.backend_status);
    printf("session_state: %s\n", yvex_session_state_name(summary.state));
    printf("context_length: %llu\n", summary.context_length);
    printf("position: %llu\n", summary.position);
    printf("accepted_tokens: %llu\n", summary.accepted_tokens);
    printf("kv_status: %s\n", summary.kv_status);
    printf("kv_bytes: %llu\n", summary.kv_bytes);
    printf("logits_status: %s\n", summary.logits_status);
    printf("weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    printf("weights_backend: %s\n", summary.weights_backend);
    printf("weight_tensor_count: %llu\n", summary.weight_tensor_count);
    printf("weight_total_bytes: %llu\n", summary.weight_total_bytes);
    printf("execution_ready: false\n");
    printf("graph_execution_ready: false\n");
    printf("reason: %s\n", yvex_session_diagnostic_reason(session));
    if (tokenized) {
        printf("tokens: %llu\n", tokens.len);
    }
    printf("status: %s\n", accept_tokens ? "session-token-accepted" : "session-created");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static void print_chat_slash_help(FILE *fp)
{
    fprintf(fp, "commands:\n");
    fprintf(fp, "  /help\n");
    fprintf(fp, "  /status\n");
    fprintf(fp, "  /model\n");
    fprintf(fp, "  /backend\n");
    fprintf(fp, "  /tokens\n");
    fprintf(fp, "  /reset\n");
    fprintf(fp, "  /quit\n");
}

static void print_chat_model(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_engine_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_engine_get_summary(runtime->engine, &summary, &err) == YVEX_OK) {
        fprintf(fp, "model: %s\n", summary.model_name);
        fprintf(fp, "architecture: %s\n", summary.architecture);
        fprintf(fp, "tokenizer: %s\n", summary.tokenizer_model);
    }
}

static void print_chat_backend(FILE *fp, const yvex_chat_runtime *runtime)
{
    fprintf(fp, "backend: %s\n", yvex_backend_kind_name(yvex_backend_kind_of(runtime->backend)));
    fprintf(fp, "status: %s\n", yvex_backend_status_name(yvex_backend_status_of(runtime->backend)));
    fprintf(fp, "capabilities:\n");
    fprintf(fp, "  tensor_alloc: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) ? "yes" : "no");
    fprintf(fp, "  tensor_read_write: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE) ? "yes" : "no");
    fprintf(fp, "  op_embed: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_EMBED) ? "yes" : "no");
    fprintf(fp, "  op_mlp: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_MLP) ? "yes" : "no");
    fprintf(fp, "  op_rope: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_ROPE) ? "yes" : "no");
}

static void print_chat_tokens(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_session_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_chat_runtime_get_summary(runtime, &summary, &err) == YVEX_OK) {
        fprintf(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
        fprintf(fp, "position: %llu\n", summary.position);
    }
}

static int handle_chat_slash(yvex_chat_runtime *runtime,
                             yvex_slash_command command,
                             const char *line,
                             int *done,
                             yvex_error *err)
{
    int rc;

    switch (command) {
    case YVEX_SLASH_HELP:
        print_chat_slash_help(stdout);
        return YVEX_OK;
    case YVEX_SLASH_STATUS:
        return yvex_chat_runtime_print_status(stdout, runtime, err);
    case YVEX_SLASH_MODEL:
        print_chat_model(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_BACKEND:
        print_chat_backend(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_TOKENS:
        print_chat_tokens(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_RESET:
        rc = yvex_chat_runtime_reset(runtime, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        printf("session reset\n");
        printf("position: 0\n");
        return YVEX_OK;
    case YVEX_SLASH_QUIT:
        printf("bye\n");
        *done = 1;
        return YVEX_OK;
    case YVEX_SLASH_UNKNOWN:
        printf("unknown slash command: %s\n", line ? line : "");
        printf("type /help\n");
        return YVEX_OK;
    case YVEX_SLASH_NOT_COMMAND:
        break;
    }
    return YVEX_OK;
}

static int resolve_chat_model_ref(yvex_model_ref *out,
                                  const char *explicit_model,
                                  yvex_error *err)
{
    yvex_model_registry *registry = NULL;
    const yvex_model_registry_entry *selected;
    char alias[256];
    int n;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat", "model reference output is required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (explicit_model && explicit_model[0]) {
        return yvex_model_ref_resolve(out, explicit_model, NULL, err);
    }

    rc = models_registry_open(&registry, NULL, 0, err);
    if (rc != YVEX_OK) {
        if (rc == YVEX_ERR_IO) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat",
                           "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
            return YVEX_ERR_INVALID_ARG;
        }
        return rc;
    }

    selected = yvex_model_registry_selected(registry);
    if (!selected || !selected->alias || !selected->alias[0]) {
        yvex_model_registry_close(registry);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat",
                       "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
        return YVEX_ERR_INVALID_ARG;
    }

    n = snprintf(alias, sizeof(alias), "%s", selected->alias);
    yvex_model_registry_close(registry);
    if (n < 0 || (size_t)n >= sizeof(alias)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "chat", "selected model alias is too long");
        return YVEX_ERR_BOUNDS;
    }

    return yvex_model_ref_resolve(out, alias, NULL, err);
}

static int command_chat(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_engine_summary engine_summary;
    yvex_session_summary session_summary;
    yvex_model_ref model_ref;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
    yvex_trace_options trace_options;
    yvex_run_artifacts artifacts;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    const char *metrics_out = NULL;
    const char *trace_out = NULL;
    const char *profile_out = NULL;
    const char *run_dir = NULL;
    unsigned long long context_length = 0;
    unsigned long long phase_token = 0;
    unsigned long long total_token = 0;
    char line[4096];
    int save_run = 0;
    int done = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc == 2 || (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0))) {
        yvex_chat_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--metrics-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = argv[++i];
        } else if (strcmp(argv[i], "--trace-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = argv[++i];
        } else if (strcmp(argv[i], "--profile-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = argv[++i];
        } else if (strcmp(argv[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(argv[i], "--run-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown chat option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help chat' for usage.\n");
            return 2;
        }
    }

    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex chat in diagnostic runtime\n");
        return 2;
    }

    rc = resolve_chat_model_ref(&model_ref, model_path, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "chat", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: chat-backend-unsupported\n");
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "backend-unsupported",
                              yvex_error_message(&err), &err);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    (void)yvex_engine_get_summary(runtime.engine, &engine_summary, &err);
    (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                       engine_summary.unsupported_tensor_accounting, &err);
    (void)yvex_chat_runtime_get_summary(&runtime, &session_summary, &err);
    printf("YVEX chat runtime\n");
    printf("model: %s\n", engine_summary.model_name);
    printf("backend: %s\n", backend_name);
    printf("session_state: %s\n", yvex_session_state_name(session_summary.state));
    printf("generation: unsupported in diagnostic runtime\n");
    printf("type /help for commands\n");

    while (!done) {
        printf("yvex> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        if (line[0] == '/') {
            rc = handle_chat_slash(&runtime, yvex_slash_parse(line), line, &done, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            continue;
        }

        {
            yvex_chat_accept_result result;
            rc = yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_CHAT_TURN, &phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            rc = yvex_chat_runtime_accept_user_text(&runtime, NULL, line, &result, &err);
            (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_CHAT_TURN, phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            (void)yvex_metrics_add_chat_turn(metrics, &err);
            (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_CHAT_TURN, "chat_turn", "accepted",
                                  "user prompt accepted", &err);
            printf("accepted tokens: %llu\n", result.prompt_tokens);
            printf("position: %llu\n", result.position);
            printf("assistant: [generation unsupported in diagnostic runtime]\n");
        }
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "accepted-only",
                          "chat runtime exited without generation", &err);
    yvex_trace_close(trace);
    rc = cli_write_observability_files("chat", engine_summary.model_name, backend_name,
                                       "accepted-only", &artifacts, metrics, argc, argv, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_close(&runtime);
    yvex_metrics_close(metrics);
    yvex_model_ref_clear(&model_ref);
    if (final_rc != 0) {
        return final_rc;
    }
    return 0;
}

int yvex_chat_command(int argc, char **argv)
{
    return command_chat(argc, argv);
}

int yvex_engine_command(int argc, char **argv)
{
    return command_engine(argc, argv);
}

int yvex_runtime_info_command(int argc, char **argv)
{
    return command_info(argc, argv);
}

int yvex_input_command(int argc, char **argv)
{
    return command_input(argc, argv);
}

int yvex_plan_command(int argc, char **argv)
{
    return command_plan(argc, argv);
}

int yvex_prefill_command(int argc, char **argv)
{
    return command_prefill(argc, argv);
}

int yvex_run_command(int argc, char **argv)
{
    return command_run(argc, argv);
}

int yvex_session_command(int argc, char **argv)
{
    return command_session(argc, argv);
}

void yvex_chat_help(FILE *fp)
{
    fprintf(fp, "usage: yvex chat [--model FILE_OR_ALIAS] --backend cpu|cuda [--ctx N] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]\n\nChat opens diagnostic engine/backend/session state and accepts user text without generating output. If --model is omitted, chat uses the current model selected with yvex models use ALIAS.\n");
}

void yvex_engine_help(FILE *fp)
{
    fprintf(fp, "usage: yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n\nEngine opens descriptor/tokenizer/graph state and can observe selected materialized residency. It does not execute prefill, decode, or generation.\n");
}

void yvex_runtime_info_help(FILE *fp)
{
    fprintf(fp, "usage: yvex info\n\nPrints the implemented build and runtime boundary status.\n");
}

void yvex_input_help(FILE *fp)
{
    fprintf(fp, "usage: yvex input tokens --model FILE_OR_ALIAS --tokens IDS\n");
    fprintf(fp, "       yvex input prompt --model FILE_OR_ALIAS --text TEXT\n");
    fprintf(fp, "\nInput parses explicit tokens or tokenizer-backed prompt text into validated token input.\n");
}

void yvex_plan_help(FILE *fp)
{
    fprintf(fp, "usage: yvex plan <path> [--backend cpu|cuda] [--seq N] [--ctx N]\n\nPlan builds graph and memory estimates. Execution remains disabled.\n");
}

void yvex_prefill_help(FILE *fp)
{
    fprintf(fp, "usage: yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n\nPrefill records a segment-summary prefill foundation from validated token input and can bind processed positions to minimal KV ownership. It does not run attention, decode, logits, sampling, or generation.\n");
}

void yvex_run_help(FILE *fp)
{
    fprintf(fp, "usage: yvex run --model FILE --backend cpu|cuda --prompt TEXT [--system TEXT] [--output plain|json] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]\n\nRun accepts one prompt through the diagnostic runtime path and reports accepted-only diagnostics.\n");
}

void yvex_session_help(FILE *fp)
{
    fprintf(fp, "usage: yvex session FILE_OR_ALIAS [--backend cpu|cuda] [--ctx N] [--text TEXT] [--accept-tokens]\n\nSession creates a lifecycle diagnostic session over engine/backend state. It does not run prefill, decode, or generation.\n");
}
