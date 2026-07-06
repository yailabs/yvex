/*
 * yvex_logits.c - Logits runtime boundary.
 *
 * This file owns the session logits skeleton and a bounded diagnostic logits
 * buffer over decode state. It does not run the real model output head, sample,
 * generate, or benchmark.
 */

#include <yvex/logits.h>
#include "yvex_console_private.h"
#include "yvex_cli_out.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_logits {
    yvex_logits_summary summary;
};

static int logits_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long logits_mix_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long logits_mix_float(unsigned long long hash, float value)
{
    uint32_t bits = 0u;

    memcpy(&bits, &value, sizeof(bits));
    return logits_mix_u64(hash, (unsigned long long)bits);
}

static int logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

static void logits_buffer_defaults(yvex_logits_buffer_summary *out,
                                   const yvex_logits_buffer_options *options)
{
    const yvex_decode_step_options *decode_options;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    decode_options = options ? options->decode_options : NULL;
    out->logits_buffer_kind = "bounded-diagnostic";
    out->logits_phase = "preflight";
    out->logits_source = "decode-state";
    out->backend_name = decode_options && decode_options->backend_name
                            ? decode_options->backend_name
                            : "cpu";
    out->decode_step_kind = "none";
    out->decode_phase = "not-started";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
}

static void logits_copy_decode_summary(yvex_logits_buffer_summary *out,
                                       const yvex_decode_step_summary *decode)
{
    if (!out || !decode) {
        return;
    }
    out->backend_name = decode->backend_name ? decode->backend_name : out->backend_name;
    out->decode_state_created = decode->decode_state_created;
    out->decode_step_executed = decode->decode_step_executed;
    out->decode_step_kind = decode->decode_step_kind ? decode->decode_step_kind
                                                     : "bounded-diagnostic";
    out->decode_phase = decode->decode_phase ? decode->decode_phase : "unknown";
    out->decode_position = decode->decode_position;
    out->decode_state_checksum = decode->decode_state_checksum;
}

static float logits_value_from_decode(const yvex_decode_step_summary *decode,
                                      unsigned long long index,
                                      unsigned long long seed)
{
    unsigned long long local;
    unsigned long long word;
    double basis = 0.0;
    double signed_offset;
    double value;

    local = logits_mix_u64(seed, index);
    local = logits_mix_u64(local, decode->decode_position);
    if (decode->decode_state_value_count > 0ull) {
        basis = (double)decode->decode_state_values[index % decode->decode_state_value_count];
    }
    word = local & 0xffffull;
    signed_offset = ((double)word / 65535.0 - 0.5) * 0.125;
    value = basis + signed_offset - (double)index * 0.0001;
    return (float)value;
}

static void logits_accumulate_summary(yvex_logits_buffer_summary *out,
                                      const float *values,
                                      unsigned long long count)
{
    unsigned long long i;
    unsigned long long checksum = 1469598103934665603ull;

    if (!out || !values || count == 0ull) {
        return;
    }
    out->logits_min = (double)values[0];
    out->logits_max = (double)values[0];
    out->logits_sum = 0.0;
    for (i = 0; i < count; ++i) {
        double value = (double)values[i];
        checksum = logits_mix_float(checksum, values[i]);
        checksum = logits_mix_u64(checksum, i);
        if (value < out->logits_min) {
            out->logits_min = value;
        }
        if (value > out->logits_max) {
            out->logits_max = value;
        }
        out->logits_sum += value;
        if (i < YVEX_LOGITS_MAX_SAMPLE_VALUES) {
            out->logits_sample_values[i] = values[i];
            out->logits_sample_count = i + 1ull;
        }
    }
    out->logits_checksum = checksum;
}

int yvex_engine_create_logits_buffer(yvex_engine *engine,
                                     const yvex_logits_buffer_options *options,
                                     yvex_logits_buffer_summary *out,
                                     yvex_error *err)
{
    yvex_decode_step_summary decode_summary;
    float *values = NULL;
    unsigned long long i;
    unsigned long long count;
    unsigned long long seed;
    int rc;

    if (!engine || !options || !options->decode_options || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_logits_buffer",
                       "engine, options, decode options, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    logits_buffer_defaults(out, options);
    count = options->logits_count;
    if (!logits_count_valid(count)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_logits_buffer",
                       "logits count must be between 1 and 256");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&decode_summary, 0, sizeof(decode_summary));
    out->decode_invoked = 1;
    out->logits_phase = "decode";
    rc = yvex_engine_decode_step(engine, options->decode_options, &decode_summary, err);
    logits_copy_decode_summary(out, &decode_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!decode_summary.decode_state_created) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_create_logits_buffer",
                       "decode state was not created");
        return YVEX_ERR_STATE;
    }

    if (logits_test_env_enabled("YVEX_TEST_FAIL_LOGITS_AFTER_DECODE")) {
        out->logits_phase = "after-decode";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_logits_buffer",
                       "test logits failure after decode");
        return YVEX_ERR_BACKEND;
    }

    out->logits_phase = "allocation";
    if (logits_test_env_enabled("YVEX_TEST_FAIL_LOGITS_AFTER_ALLOC")) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_create_logits_buffer",
                       "test logits allocation failure");
        return YVEX_ERR_NOMEM;
    }
    if (count > ULLONG_MAX / (unsigned long long)sizeof(*values)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_logits_buffer",
                       "logits byte count overflow");
        return YVEX_ERR_BOUNDS;
    }
    values = (float *)calloc((size_t)count, sizeof(*values));
    if (!values) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_create_logits_buffer",
                       "failed to allocate bounded logits buffer");
        return YVEX_ERR_NOMEM;
    }

    out->logits_phase = "fill";
    seed = decode_summary.decode_state_checksum;
    for (i = 0; i < count; ++i) {
        values[i] = logits_value_from_decode(&decode_summary, i, seed);
    }
    if (logits_test_env_enabled("YVEX_TEST_FAIL_LOGITS_AFTER_FILL")) {
        free(values);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_logits_buffer",
                       "test logits failure after fill");
        return YVEX_ERR_BACKEND;
    }

    out->logits_count = count;
    out->logits_bytes = count * (unsigned long long)sizeof(*values);
    logits_accumulate_summary(out, values, count);
    free(values);
    out->cleanup_attempted = 1;
    out->cleanup_status = "pass";
    out->logits_buffer_created = 1;
    out->bounded_logits_ready = 1;
    out->logits_phase = "complete";
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

static void logits_print_summary(const yvex_logits_buffer_summary *summary,
                                 const char *model_arg,
                                 const char *backend_name,
                                 const char *segment_name,
                                 const char *token_input_status,
                                 unsigned long long input_token_count,
                                 const char *status)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "logits: buffer\n");
    yvex_cli_out_writef(stdout, "status: logits-buffer\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg ? model_arg : "");
    yvex_cli_out_writef(stdout, "backend: %s\n",
           summary && summary->backend_name ? summary->backend_name
                                            : (backend_name ? backend_name : "cpu"));
    yvex_cli_out_writef(stdout, "segment: %s\n", segment_name ? segment_name : "embedding-rmsnorm");
    yvex_cli_out_writef(stdout, "token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    yvex_cli_out_writef(stdout, "input_token_count: %llu\n", input_token_count);
    yvex_cli_out_writef(stdout, "decode_invoked: %s\n", summary && summary->decode_invoked ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_state_created: %s\n",
           summary && summary->decode_state_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_step_executed: %s\n",
           summary && summary->decode_step_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_step_kind: %s\n",
           summary && summary->decode_step_kind ? summary->decode_step_kind : "none");
    yvex_cli_out_writef(stdout, "decode_phase: %s\n",
           summary && summary->decode_phase ? summary->decode_phase : "not-started");
    yvex_cli_out_writef(stdout, "decode_position: %llu\n", summary ? summary->decode_position : 0ull);
    yvex_cli_out_writef(stdout, "decode_state_checksum: %llu\n",
           summary ? summary->decode_state_checksum : 0ull);
    yvex_cli_out_writef(stdout, "logits_buffer_created: %s\n",
           summary && summary->logits_buffer_created ? "true" : "false");
    yvex_cli_out_writef(stdout, "logits_buffer_kind: %s\n",
           summary && summary->logits_buffer_kind ? summary->logits_buffer_kind
                                                  : "bounded-diagnostic");
    yvex_cli_out_writef(stdout, "logits_phase: %s\n",
           summary && summary->logits_phase ? summary->logits_phase : "preflight");
    yvex_cli_out_writef(stdout, "logits_source: %s\n",
           summary && summary->logits_source ? summary->logits_source : "decode-state");
    yvex_cli_out_writef(stdout, "logits_count: %llu\n", summary ? summary->logits_count : 0ull);
    yvex_cli_out_writef(stdout, "logits_bytes: %llu\n", summary ? summary->logits_bytes : 0ull);
    yvex_cli_out_writef(stdout, "logits_checksum: %llu\n", summary ? summary->logits_checksum : 0ull);
    yvex_cli_out_writef(stdout, "logits_min: %.9g\n", summary ? summary->logits_min : 0.0);
    yvex_cli_out_writef(stdout, "logits_max: %.9g\n", summary ? summary->logits_max : 0.0);
    yvex_cli_out_writef(stdout, "logits_sum: %.9g\n", summary ? summary->logits_sum : 0.0);
    yvex_cli_out_writef(stdout, "logits_sample_count: %llu\n",
           summary ? summary->logits_sample_count : 0ull);
    if (summary) {
        for (i = 0; i < summary->logits_sample_count; ++i) {
            yvex_cli_out_writef(stdout, "logit_%llu: %.9g\n", i, (double)summary->logits_sample_values[i]);
        }
    }
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "bounded_logits_ready: %s\n",
           summary && summary->bounded_logits_ready ? "true" : "false");
    yvex_cli_out_writef(stdout, "real_model_logits: false\n");
    yvex_cli_out_writef(stdout, "real_model_output_head: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "logits-buffer-fail");
}

static void logits_cli_defaults(yvex_logits_buffer_summary *summary,
                                const char *backend_name)
{
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options options;

    memset(&decode_options, 0, sizeof(decode_options));
    memset(&options, 0, sizeof(options));
    decode_options.backend_name = backend_name ? backend_name : "cpu";
    decode_options.segment_name = "embedding-rmsnorm";
    options.decode_options = &decode_options;
    options.logits_count = 16ull;
    logits_buffer_defaults(summary, &options);
}

static int logits_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

int yvex_logits_command(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_logits_buffer_summary logits_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    yvex_kv_shape kv_shape;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    unsigned long long vocab_size = 0ull;
    unsigned long long layer_count = 0ull;
    unsigned long long layer_hidden_dim = 0ull;
    unsigned long long layer_head_dim = 0ull;
    unsigned long long layer_ffn_dim = 0ull;
    unsigned long long chunk_size = 0ull;
    unsigned long long position_start = 0ull;
    unsigned long long context_length = 0ull;
    unsigned long long logits_count = 16ull;
    int attach_kv = 0;
    int kv_shape_seen = 0;
    int layer_count_seen = 0;
    int layer_hidden_seen = 0;
    int layer_head_seen = 0;
    int layer_ffn_seen = 0;
    int chunk_size_seen = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&decode_options, 0, sizeof(decode_options));
    memset(&logits_options, 0, sizeof(logits_options));
    memset(&logits_summary, 0, sizeof(logits_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_logits_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &logits_count) ||
                !logits_count_valid(logits_count)) {
                yvex_cli_out_writef(stderr, "yvex: --logits-count requires 1 <= N <= 256\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.kv_head_count)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.capacity)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            layer_count_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_hidden_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-hidden-dim requires a positive integer\n");
                return 2;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-head-dim requires a positive integer\n");
                return 2;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_ffn_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-ffn-dim requires a positive integer\n");
                return 2;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &chunk_size)) {
                yvex_cli_out_writef(stderr, "yvex: --chunk-size requires a positive integer\n");
                return 2;
            }
            chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position-start") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &position_start)) {
                yvex_cli_out_writef(stderr, "yvex: --position-start requires a non-negative integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --context-length requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown logits option: %s\n", argv[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help logits' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !backend_name || !tokens_text || !segment_name) {
        yvex_cli_out_writef(stderr, "usage: yvex logits --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N]\n");
        return 2;
    }
    if (!logits_backend_name_valid(backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unsupported logits segment: %s\n", segment_name);
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        yvex_cli_out_writef(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        yvex_cli_out_writef(stderr, "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }
    if (!layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "yvex: --layer-* options require --layers N\n");
        return 2;
    }
    if (layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "yvex: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim\n");
        return 2;
    }
    if (layer_count_seen && (layer_count == 0ull || layer_count > 16ull)) {
        yvex_cli_out_writef(stderr, "yvex: --layers requires 1 <= N <= 16\n");
        return 2;
    }
    if (layer_count_seen && !layer_hidden_seen) {
        layer_hidden_dim = 8ull;
        layer_head_dim = 8ull;
        layer_ffn_dim = 16ull;
    }
    if (layer_count_seen && layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_cli_out_writef(stderr, "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff\n",
                (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "logits");
    if (rc != YVEX_OK) {
        logits_cli_defaults(&logits_summary, backend_name);
        logits_print_summary(&logits_summary, model_arg, backend_name, segment_name, "fail", 0ull,
                             "logits-buffer-fail");
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
        logits_cli_defaults(&logits_summary, backend_name);
        logits_print_summary(&logits_summary, model_arg, backend_name, segment_name, "fail",
                             token_input.token_count, "logits-buffer-fail");
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
        logits_cli_defaults(&logits_summary, backend_name);
        logits_print_summary(&logits_summary, model_arg, backend_name, segment_name, "pass",
                             token_input.token_count, "logits-buffer-fail");
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
        logits_cli_defaults(&logits_summary, backend_name);
        logits_print_summary(&logits_summary, model_arg, backend_name, segment_name, "pass",
                             token_input.token_count, "logits-buffer-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    decode_options.token_input = &token_input;
    decode_options.segment_name = segment_name;
    decode_options.backend_name = backend_name;
    decode_options.position_start = position_start;
    decode_options.chunk_size = chunk_size_seen ? chunk_size : 0ull;
    decode_options.context_length = context_length;
    decode_options.attach_kv = attach_kv;
    decode_options.kv_shape = kv_shape;
    decode_options.layer_count = layer_count_seen ? layer_count : 0ull;
    decode_options.layer_hidden_dim = layer_hidden_dim;
    decode_options.layer_head_dim = layer_head_dim;
    decode_options.layer_ffn_dim = layer_ffn_dim;
    logits_options.decode_options = &decode_options;
    logits_options.logits_count = logits_count;

    rc = yvex_engine_create_logits_buffer(engine, &logits_options, &logits_summary, &err);
    if (rc != YVEX_OK) {
        const char *status = "logits-buffer-fail";
        if (logits_summary.logits_phase &&
            strcmp(logits_summary.logits_phase, "fill") == 0 &&
            logits_summary.cleanup_attempted &&
            logits_summary.cleanup_status &&
            strcmp(logits_summary.cleanup_status, "pass") == 0) {
            status = "logits-buffer-failed-cleaned";
        }
        logits_print_summary(&logits_summary, model_arg, backend_name, segment_name, "pass",
                             token_input.token_count, status);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    logits_print_summary(&logits_summary, model_arg, backend_name, segment_name, "pass",
                         token_input.token_count, "logits-buffer-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

void yvex_logits_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex logits --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N]\n\nLogits creates a bounded diagnostic logits buffer from the implemented decode state. It does not run the real model output head, sample, generate, or claim DeepSeek logits.\n");
}
