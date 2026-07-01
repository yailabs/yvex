/*
 * yvex_prefill.c - Prefill state runtime boundary.
 *
 * This file owns the implemented segment-summary prefill foundation, bounded
 * layer-backed diagnostic state path, and minimal KV binding. It does not
 * claim full transformer prefill, decode, logits, sampling, or generation.
 */

#include <yvex/engine.h>
#include "yvex_console_private.h"
#include "yvex_runtime_private.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int prefill_checked_add_ull(unsigned long long a,
                                   unsigned long long b,
                                   unsigned long long *out)
{
    if (!out || a > ULLONG_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static int prefill_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long prefill_mix_checksum_u64(unsigned long long hash,
                                                   unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long prefill_checksum_f32_values(const float *values,
                                                      unsigned long long count)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; values && i < count; ++i) {
        uint32_t raw = 0u;
        memcpy(&raw, &values[i], sizeof(raw));
        hash = prefill_mix_checksum_u64(hash, (unsigned long long)raw);
        hash = prefill_mix_checksum_u64(hash, i);
    }
    return hash;
}

static void fill_prefill_kv_values(float *values,
                                   unsigned long long value_count,
                                   const float *source_values,
                                   unsigned long long source_count,
                                   unsigned int token_id,
                                   unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        float base = 0.0f;
        if (source_values && source_count > 0ull) {
            base = source_values[i % source_count];
        }
        values[i] = base +
                    (float)(position * 0.001) +
                    (float)(token_id * 0.000001) +
                    (float)(i * 0.0000001);
    }
}

static void prefill_summary_apply_kv(const yvex_kv_summary *kv,
                                     yvex_prefill_state_summary *out)
{
    if (!kv || !out) {
        return;
    }
    out->kv_status = yvex_kv_status_name(kv->status);
    out->kv_owner = kv->owner;
    out->kv_dtype = kv->dtype;
    out->kv_layers = kv->layer_count;
    out->kv_heads = kv->kv_head_count;
    out->kv_head_dim = kv->head_dim;
    out->kv_capacity = kv->context_length;
    out->kv_values_per_position = kv->values_per_position;
    out->kv_bytes_per_position = kv->bytes_per_position;
    out->kv_planned_bytes = kv->bytes;
    out->kv_allocated_bytes = kv->allocated_bytes;
    out->kv_positions_written = kv->written_positions;
    out->kv_append_count = kv->append_count;
    out->kv_read_count = kv->read_count;
    out->kv_overflow_status = kv->overflow_status;
    out->session_kv_owned = kv->session_owned;
}

static int prefill_cleanup_kv(yvex_kv_cache *kv,
                              yvex_prefill_state_summary *out)
{
    yvex_error cleanup_err;

    if (!kv || !out) {
        return 0;
    }
    yvex_error_clear(&cleanup_err);
    out->cleanup_attempted = 1;
    if (yvex_kv_cache_clear(kv, &cleanup_err) == YVEX_OK) {
        out->cleanup_status = "pass";
        out->kv_cleanup_status = "pass";
        return 1;
    }
    out->cleanup_status = "fail";
    out->kv_cleanup_status = "fail";
    return 0;
}

int yvex_engine_create_prefill_state(yvex_engine *engine,
                                     const yvex_prefill_state_options *options,
                                     yvex_prefill_state_summary *out,
                                     yvex_error *err)
{
    yvex_segment_graph_options segment_options;
    yvex_segment_graph_result segment_result;
    yvex_cli_layer_fixture_options layer_options;
    yvex_cli_layer_fixture_result layer_result;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary kv_summary;
    const yvex_token_input *input;
    const char *segment_name = "embedding-rmsnorm";
    const float *kv_source_values = NULL;
    unsigned long long kv_source_count = 0ull;
    float layer_seed_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    float *kv_values = NULL;
    float *kv_read_values = NULL;
    unsigned long long aggregate = 1469598103934665603ull;
    unsigned long long kv_value_count = 0ull;
    unsigned long long kv_position = 0ull;
    unsigned long long kv_sample_count = 0ull;
    unsigned long long i;
    unsigned long long j;
    int layer_prefill_requested;
    int rc;

    if (!engine || !options || !options->token_input || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
                       "engine, options, token input, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    layer_prefill_requested = options->layer_count > 0ull;
    memset(out, 0, sizeof(*out));
    out->prefill_state_kind = layer_prefill_requested
                                  ? "layer-backed-segment-summary"
                                  : "segment-summary";
    out->sequence_execution_mode = layer_prefill_requested
                                       ? "segment-then-controlled-layer-fixture"
                                       : "independent-token-segments";
    out->prefill_phase = "preflight";
    out->backend_name = "none";
    out->segment_name = "embedding-rmsnorm";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
    out->kv_ready = 0;
    out->session_kv_owned = 0;
    out->kv_bound_to_prefill = 0;
    out->kv_binding_kind = options->attach_kv ? "minimal-diagnostic" : "none";
    out->kv_binding_source = layer_prefill_requested
                                 ? "layer-final-sample"
                                 : "segment-output-sample";
    out->kv_status = options->attach_kv ? "planned" : "not-requested";
    out->kv_owner = "none";
    out->kv_dtype = "none";
    out->kv_overflow_status = "not-checked";
    out->kv_cleanup_status = "not-needed";
    out->full_transformer_prefill_ready = 0;
    out->decode_ready = 0;
    out->logits_ready = 0;
    out->generation_ready = 0;
    out->layer_prefill_requested = layer_prefill_requested;
    out->layer_execution_kind = layer_prefill_requested
                                    ? "controlled-layer-fixture"
                                    : "none";
    out->layer_input_projection = layer_prefill_requested
                                      ? "segment-sample-prefix"
                                      : "none";
    out->layer_handoff = layer_prefill_requested
                             ? "selected-position-row"
                             : "none";
    out->layer_sequence_rebuild = layer_prefill_requested
                                      ? "deterministic-with-previous-position-row"
                                      : "none";
    out->model_layer_execution = 0;
    out->layer_count = options->layer_count;

    input = options->token_input;
    if (options->segment_name) {
        segment_name = options->segment_name;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
                       "unsupported prefill segment; expected embedding-rmsnorm");
        return YVEX_ERR_INVALID_ARG;
    }
    if (layer_prefill_requested) {
        if (options->layer_count > 16ull ||
            options->layer_hidden_dim == 0ull ||
            options->layer_head_dim == 0ull ||
            options->layer_ffn_dim == 0ull) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
                           "layer-backed prefill requires 1 <= layers <= 16 and positive layer dimensions");
            return YVEX_ERR_INVALID_ARG;
        }
        if (options->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                           "layer-backed prefill hidden dimension exceeds segment sample capacity");
            return YVEX_ERR_BOUNDS;
        }
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

    if (options->attach_kv) {
        out->prefill_phase = "kv-preflight";
        out->kv_layers = options->kv_shape.layer_count;
        out->kv_heads = options->kv_shape.kv_head_count;
        out->kv_head_dim = options->kv_shape.head_dim;
        out->kv_capacity = options->kv_shape.capacity;
        if (options->kv_shape.capacity < input->token_count) {
            out->kv_overflow_status = "capacity-too-small";
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                           "KV capacity is smaller than token count");
            return YVEX_ERR_BOUNDS;
        }
        if (prefill_test_env_enabled("YVEX_TEST_FAIL_PREFILL_KV_ALLOC")) {
            out->prefill_phase = "kv-allocation";
            out->kv_status = "fail";
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_create_prefill_state",
                           "test prefill KV allocation failure");
            return YVEX_ERR_NOMEM;
        }
        out->prefill_phase = "kv-allocation";
        rc = yvex_kv_cache_create_shape(&kv, &options->kv_shape, err);
        if (rc != YVEX_OK) {
            out->kv_status = "fail";
            return rc;
        }
        rc = yvex_kv_cache_get_summary(kv, &kv_summary, err);
        if (rc != YVEX_OK) {
            yvex_kv_cache_close(kv);
            return rc;
        }
        prefill_summary_apply_kv(&kv_summary, out);
        kv_value_count = yvex_kv_cache_position_value_count(kv);
        kv_values = (float *)calloc((size_t)kv_value_count, sizeof(float));
        kv_read_values = (float *)calloc((size_t)kv_value_count, sizeof(float));
        if (!kv_values || !kv_read_values) {
            (void)prefill_cleanup_kv(kv, out);
            free(kv_values);
            free(kv_read_values);
            yvex_kv_cache_close(kv);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_create_prefill_state",
                           "failed to allocate prefill KV diagnostic buffers");
            return YVEX_ERR_NOMEM;
        }
    }

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
            if (kv) {
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
            } else {
                out->cleanup_attempted = segment_result.cleanup_attempted;
                out->cleanup_status = segment_result.cleanup_status
                                          ? segment_result.cleanup_status
                                          : (segment_result.cleanup_attempted ? "pass" : "not-needed");
            }
            return rc;
        }

        out->segment_graph_executions += 1ull;
        out->tokens_processed += 1ull;
        out->segment_output_count = segment_result.segment_output_count;
        out->segment_output_bytes = segment_result.segment_output_bytes;
        if (!prefill_checked_add_ull(out->total_output_bytes,
                                     segment_result.segment_output_bytes,
                                     &out->total_output_bytes) ||
            !prefill_checked_add_ull(out->scratch_bytes,
                                     segment_result.segment_scratch_bytes,
                                     &out->scratch_bytes)) {
            out->failed_token_index = i;
            if (kv) {
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
            } else {
                out->cleanup_attempted = 1;
                out->cleanup_status = "pass";
            }
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                           "prefill byte accounting overflow");
            return YVEX_ERR_BOUNDS;
        }
        out->final_token_checksum = segment_result.output_checksum;
        aggregate = prefill_mix_checksum_u64(aggregate, (unsigned long long)input->tokens[i]);
        aggregate = prefill_mix_checksum_u64(aggregate, segment_result.output_checksum);
        aggregate = prefill_mix_checksum_u64(aggregate, segment_result.reference_checksum);
        if (segment_result.max_abs_diff > out->max_abs_diff) {
            out->max_abs_diff = segment_result.max_abs_diff;
        }
        kv_source_values = segment_result.output_values;
        kv_source_count = segment_result.output_value_count;

        if (layer_prefill_requested) {
            if (segment_result.output_value_count < options->layer_hidden_dim) {
                out->failed_token_index = i;
                if (kv) {
                    (void)prefill_cleanup_kv(kv, out);
                    free(kv_values);
                    free(kv_read_values);
                    yvex_kv_cache_close(kv);
                } else {
                    out->cleanup_attempted = 1;
                    out->cleanup_status = "pass";
                }
                yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                               "segment output sample is too small for layer-backed prefill");
                return YVEX_ERR_BOUNDS;
            }
            for (j = 0; j < options->layer_hidden_dim; ++j) {
                layer_seed_values[j] = segment_result.output_values[j];
            }

            memset(&layer_options, 0, sizeof(layer_options));
            memset(&layer_result, 0, sizeof(layer_result));
            layer_options.backend_name = out->backend_name;
            layer_options.layers = options->layer_count;
            layer_options.seq_len = input->token_count;
            layer_options.position = i;
            layer_options.hidden_dim = options->layer_hidden_dim;
            layer_options.head_dim = options->layer_head_dim;
            layer_options.ffn_dim = options->layer_ffn_dim;
            layer_options.initial_position_values = layer_seed_values;
            layer_options.initial_position_value_count = options->layer_hidden_dim;

            out->prefill_phase = "layer-execution";
            rc = yvex_cli_graph_execute_layer_fixture(&layer_options, &layer_result, err);
            out->layer_graph_executions += 1ull;
            out->layer_block_executions += layer_result.layers;
            out->layer_total_op_count += layer_result.total_op_count;
            out->layer_output_count = layer_result.output_value_count;
            out->layer_output_bytes = layer_result.output_bytes;
            out->layer_final_checksum = layer_result.final_output_checksum;
            out->layer_final_reference_checksum = layer_result.final_reference_checksum;
            out->layer_max_abs_diff = layer_result.final_max_abs_diff;
            out->layer_output_sample_count = layer_result.output_value_count;
            for (j = 0; j < layer_result.output_value_count; ++j) {
                out->layer_output_sample_values[j] = layer_result.output_values[j];
            }
            if (!prefill_checked_add_ull(out->layer_total_output_bytes,
                                         layer_result.output_bytes,
                                         &out->layer_total_output_bytes) ||
                !prefill_checked_add_ull(out->layer_total_scratch_bytes,
                                         layer_result.scratch_bytes,
                                         &out->layer_total_scratch_bytes) ||
                !prefill_checked_add_ull(out->total_output_bytes,
                                         layer_result.output_bytes,
                                         &out->total_output_bytes) ||
                !prefill_checked_add_ull(out->scratch_bytes,
                                         layer_result.scratch_bytes,
                                         &out->scratch_bytes)) {
                out->failed_token_index = i;
                if (kv) {
                    (void)prefill_cleanup_kv(kv, out);
                    free(kv_values);
                    free(kv_read_values);
                    yvex_kv_cache_close(kv);
                } else {
                    out->cleanup_attempted = 1;
                    out->cleanup_status = "pass";
                }
                yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_prefill_state",
                               "layer-backed prefill byte accounting overflow");
                return YVEX_ERR_BOUNDS;
            }
            if (rc != 0) {
                yvex_status layer_status = yvex_error_code(err);
                out->failed_token_index = i;
                if (kv) {
                    (void)prefill_cleanup_kv(kv, out);
                    free(kv_values);
                    free(kv_read_values);
                    yvex_kv_cache_close(kv);
                } else {
                    out->cleanup_attempted = layer_result.cleanup_attempted;
                    out->cleanup_status = layer_result.cleanup_status
                                              ? layer_result.cleanup_status
                                              : (layer_result.cleanup_attempted ? "pass" : "not-needed");
                }
                if (layer_status == YVEX_OK) {
                    yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_create_prefill_state",
                                   "layer-backed prefill fixture execution failed");
                    layer_status = YVEX_ERR_STATE;
                }
                return layer_status;
            }
            aggregate = prefill_mix_checksum_u64(aggregate, layer_result.final_output_checksum);
            aggregate = prefill_mix_checksum_u64(aggregate, layer_result.final_reference_checksum);
            out->final_token_checksum = layer_result.final_output_checksum;
            if (layer_result.final_max_abs_diff > out->max_abs_diff) {
                out->max_abs_diff = layer_result.final_max_abs_diff;
            }
            kv_source_values = layer_result.output_values;
            kv_source_count = layer_result.output_value_count;

            if (prefill_test_env_enabled("YVEX_TEST_FAIL_PREFILL_LAYERS_AFTER_LAYER_EXECUTION")) {
                out->failed_token_index = i;
                out->prefill_phase = "layer-execution-complete";
                if (kv) {
                    (void)prefill_cleanup_kv(kv, out);
                    free(kv_values);
                    free(kv_read_values);
                    yvex_kv_cache_close(kv);
                } else {
                    out->cleanup_attempted = layer_result.cleanup_attempted;
                    out->cleanup_status = layer_result.cleanup_status
                                              ? layer_result.cleanup_status
                                              : "pass";
                }
                yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_prefill_state",
                               "test prefill layer failure after layer execution");
                return YVEX_ERR_BACKEND;
            }
        }

        if (kv) {
            out->prefill_phase = "kv-append";
            fill_prefill_kv_values(kv_values,
                                   kv_value_count,
                                   kv_source_values,
                                   kv_source_count,
                                   input->tokens[i],
                                   options->position_start + i);
            rc = yvex_kv_cache_append_position_f32(kv,
                                                   kv_values,
                                                   kv_value_count,
                                                   &kv_position,
                                                   err);
            if (rc != YVEX_OK) {
                out->failed_token_index = i;
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
                return rc;
            }
            if (kv_position != i) {
                out->failed_token_index = i;
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
                yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_create_prefill_state",
                               "KV append position did not match token index");
                return YVEX_ERR_STATE;
            }
            rc = yvex_kv_cache_get_summary(kv, &kv_summary, err);
            if (rc != YVEX_OK) {
                out->failed_token_index = i;
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
                return rc;
            }
            prefill_summary_apply_kv(&kv_summary, out);
            if (prefill_test_env_enabled("YVEX_TEST_FAIL_PREFILL_KV_AFTER_APPEND_0") &&
                i == 0ull) {
                out->failed_token_index = i + 1ull;
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
                yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_prefill_state",
                               "test prefill KV failure after append 0");
                return YVEX_ERR_BACKEND;
            }
        }

        if (layer_prefill_requested &&
            prefill_test_env_enabled("YVEX_TEST_FAIL_PREFILL_LAYERS_AFTER_TOKEN_0") &&
            i == 0ull) {
            out->failed_token_index = i + 1ull;
            out->prefill_phase = "layer-token-complete";
            if (kv) {
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
            } else {
                out->cleanup_attempted = 1;
                out->cleanup_status = "pass";
            }
            yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_prefill_state",
                           "test prefill layer failure after token 0");
            return YVEX_ERR_BACKEND;
        }

        if (prefill_test_env_enabled("YVEX_TEST_FAIL_PREFILL_AFTER_TOKEN_0") && i == 0ull) {
            out->failed_token_index = i + 1ull;
            if (kv) {
                (void)prefill_cleanup_kv(kv, out);
                free(kv_values);
                free(kv_read_values);
                yvex_kv_cache_close(kv);
            } else {
                out->cleanup_attempted = 1;
                out->cleanup_status = "pass";
            }
            yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_prefill_state",
                           "test prefill failure after token 0");
            return YVEX_ERR_BACKEND;
        }
    }

    if (kv) {
        out->prefill_phase = "kv-readback";
        rc = yvex_kv_cache_read_position_f32(kv, 0ull, kv_read_values, kv_value_count, err);
        if (rc != YVEX_OK) {
            (void)prefill_cleanup_kv(kv, out);
            free(kv_values);
            free(kv_read_values);
            yvex_kv_cache_close(kv);
            return rc;
        }
        out->kv_read_position = 0ull;
        out->kv_read_value_count = kv_value_count;
        out->kv_read_checksum = prefill_checksum_f32_values(kv_read_values, kv_value_count);
        kv_sample_count = kv_value_count < YVEX_PREFILL_KV_MAX_SAMPLE_VALUES
                              ? kv_value_count
                              : YVEX_PREFILL_KV_MAX_SAMPLE_VALUES;
        out->kv_read_sample_count = kv_sample_count;
        for (i = 0; i < kv_sample_count; ++i) {
            out->kv_read_sample_values[i] = kv_read_values[i];
        }
        rc = yvex_kv_cache_get_summary(kv, &kv_summary, err);
        if (rc != YVEX_OK) {
            (void)prefill_cleanup_kv(kv, out);
            free(kv_values);
            free(kv_read_values);
            yvex_kv_cache_close(kv);
            return rc;
        }
        prefill_summary_apply_kv(&kv_summary, out);
        out->kv_ready = 1;
        out->kv_bound_to_prefill = 1;
        out->kv_cleanup_status = "pass";
        yvex_kv_cache_close(kv);
        kv = NULL;
        free(kv_values);
        free(kv_read_values);
        kv_values = NULL;
        kv_read_values = NULL;
    }

    out->aggregate_checksum = aggregate;
    out->prefill_state_created = 1;
    out->prefill_phase = "complete";
    out->cleanup_status = "not-needed";
    out->cuda_parity = out->backend_name && strcmp(out->backend_name, "cuda") == 0;
    yvex_error_clear(err);
    return YVEX_OK;
}
