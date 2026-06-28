/*
 * yvex_runtime.c - Engine, session, KV, and logits shells.
 *
 * This file owns descriptor and lower runtime state. It implements a
 * segment-summary prefill foundation, but not attention KV, decode, sampling,
 * or generation.
 */

#include <stddef.h>
#include <yvex/yvex.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#define YVEX_RUNTIME_REASON_CAP 256u

struct yvex_engine {
    char *model_path;
    yvex_engine_status status;
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_graph *graph;
    yvex_backend *weight_backend;
    yvex_weight_table *weights;
    yvex_materialize_summary weight_summary;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

struct yvex_kv_cache {
    yvex_kv_summary summary;
};

struct yvex_logits {
    yvex_logits_summary summary;
};

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

static unsigned long long runtime_mix_checksum_u64(unsigned long long hash,
                                                   unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
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

int yvex_engine_create_prefill_state(yvex_engine *engine,
                                     const yvex_prefill_state_options *options,
                                     yvex_prefill_state_summary *out,
                                     yvex_error *err)
{
    yvex_segment_graph_options segment_options;
    yvex_segment_graph_result segment_result;
    const yvex_token_input *input;
    const char *segment_name = "embedding-rmsnorm";
    unsigned long long aggregate = 1469598103934665603ull;
    unsigned long long i;
    int rc;

    if (!engine || !options || !options->token_input || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
                       "engine, options, token input, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->prefill_state_kind = "segment-summary";
    out->sequence_execution_mode = "independent-token-segments";
    out->prefill_phase = "preflight";
    out->backend_name = "none";
    out->segment_name = "embedding-rmsnorm";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
    out->kv_ready = 0;
    out->decode_ready = 0;
    out->logits_ready = 0;

    input = options->token_input;
    if (options->segment_name) {
        segment_name = options->segment_name;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
                       "unsupported prefill segment; expected embedding-rmsnorm");
        return YVEX_ERR_INVALID_ARG;
    }
    out->segment_name = segment_name;
    out->token_count = input->token_count;
    out->position_start = options->position_start;
    out->position_end = options->position_start;

    if (input->token_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
                       "token-list-empty");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!input->token_bounds_checked || !input->token_bounds_valid) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_create_prefill_state",
                       "validated token input is required before prefill state creation");
        return YVEX_ERR_STATE;
    }
    if (input->token_count > ULLONG_MAX - options->position_start) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                       "prefill position range overflow");
        return YVEX_ERR_BOUNDS;
    }
    out->position_end = options->position_start + input->token_count - 1ull;

    if (engine->weight_backend) {
        out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    }

    for (i = 0; i < input->token_count; ++i) {
        memset(&segment_options, 0, sizeof(segment_options));
        memset(&segment_result, 0, sizeof(segment_result));
        segment_options.segment_name = segment_name;
        segment_options.token_id = input->tokens[i];

        out->prefill_phase = "token-execution";
        rc = yvex_engine_execute_segment_graph(engine, &segment_options, &segment_result, err);
        if (rc != YVEX_OK) {
            out->failed_token_index = i;
            out->cleanup_attempted = segment_result.cleanup_attempted;
            out->cleanup_status = segment_result.cleanup_status
                                      ? segment_result.cleanup_status
                                      : (segment_result.cleanup_attempted ? "pass" : "not-needed");
            return rc;
        }

        out->segment_graph_executions += 1ull;
        out->tokens_processed += 1ull;
        out->segment_output_count = segment_result.segment_output_count;
        out->segment_output_bytes = segment_result.segment_output_bytes;
        if (!runtime_checked_add_ull(out->total_output_bytes,
                                     segment_result.segment_output_bytes,
                                     &out->total_output_bytes) ||
            !runtime_checked_add_ull(out->scratch_bytes,
                                     segment_result.segment_scratch_bytes,
                                     &out->scratch_bytes)) {
            out->failed_token_index = i;
            out->cleanup_attempted = 1;
            out->cleanup_status = "pass";
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                           "prefill byte accounting overflow");
            return YVEX_ERR_BOUNDS;
        }
        out->final_token_checksum = segment_result.output_checksum;
        aggregate = runtime_mix_checksum_u64(aggregate, (unsigned long long)input->tokens[i]);
        aggregate = runtime_mix_checksum_u64(aggregate, segment_result.output_checksum);
        aggregate = runtime_mix_checksum_u64(aggregate, segment_result.reference_checksum);
        if (segment_result.max_abs_diff > out->max_abs_diff) {
            out->max_abs_diff = segment_result.max_abs_diff;
        }

        if (yvex_runtime_test_env_enabled("YVEX_TEST_FAIL_PREFILL_AFTER_TOKEN_0") && i == 0ull) {
            out->failed_token_index = i + 1ull;
            out->cleanup_attempted = 1;
            out->cleanup_status = "pass";
            yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_prefill_state",
                           "test prefill failure after token 0");
            return YVEX_ERR_BACKEND;
        }
    }

    out->aggregate_checksum = aggregate;
    out->prefill_state_created = 1;
    out->prefill_phase = "complete";
    out->cleanup_status = "not-needed";
    out->cuda_parity = out->backend_name && strcmp(out->backend_name, "cuda") == 0;
    yvex_error_clear(err);
    return YVEX_OK;
}



int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err)
{
    yvex_kv_cache *kv;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (context_length == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "context_length must be positive");
        return YVEX_ERR_INVALID_ARG;
    }

    kv = (yvex_kv_cache *)calloc(1, sizeof(*kv));
    if (!kv) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_kv_cache_create", "failed to allocate KV cache summary");
        return YVEX_ERR_NOMEM;
    }

    (void)model;
    kv->summary.status = YVEX_KV_STATUS_UNAVAILABLE;
    kv->summary.context_length = context_length;
    kv->summary.bytes = 0;

    *out = kv;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_kv_cache_close(yvex_kv_cache *kv)
{
    free(kv);
}

yvex_kv_status yvex_kv_status_of(const yvex_kv_cache *kv)
{
    return kv ? kv->summary.status : YVEX_KV_STATUS_EMPTY;
}

const char *yvex_kv_status_name(yvex_kv_status status)
{
    switch (status) {
    case YVEX_KV_STATUS_EMPTY: return "empty";
    case YVEX_KV_STATUS_UNAVAILABLE: return "unavailable";
    case YVEX_KV_STATUS_PLANNED: return "planned";
    case YVEX_KV_STATUS_ALLOCATED: return "allocated";
    }
    return "unknown";
}

int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err)
{
    if (!kv || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_get_summary", "kv and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &kv->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}



int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err)
{
    yvex_logits *logits;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_create", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }

    logits = (yvex_logits *)calloc(1, sizeof(*logits));
    if (!logits) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_logits_create", "failed to allocate logits summary");
        return YVEX_ERR_NOMEM;
    }

    logits->summary.status = YVEX_LOGITS_STATUS_UNAVAILABLE;
    logits->summary.vocab_size = 0;
    logits->summary.bytes = 0;

    *out = logits;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_logits_close(yvex_logits *logits)
{
    free(logits);
}

yvex_logits_status yvex_logits_status_of(const yvex_logits *logits)
{
    return logits ? logits->summary.status : YVEX_LOGITS_STATUS_EMPTY;
}

const char *yvex_logits_status_name(yvex_logits_status status)
{
    switch (status) {
    case YVEX_LOGITS_STATUS_EMPTY: return "empty";
    case YVEX_LOGITS_STATUS_UNAVAILABLE: return "unavailable";
    case YVEX_LOGITS_STATUS_ALLOCATED: return "allocated";
    }
    return "unknown";
}

int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err)
{
    if (!logits || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_get_summary", "logits and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &logits->summary, sizeof(*out));
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

    rc = yvex_kv_cache_create(&session->kv, yvex_engine_model(engine), context_length, err);
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
    } else {
        out->kv_status = "empty";
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
    session->state = reset_state_for_graph(session);
    set_session_reason_from_graph(session);
    yvex_error_clear(err);
    return YVEX_OK;
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
