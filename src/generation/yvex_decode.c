/*
 * yvex_decode.c - Decode-step runtime boundary.
 *
 * This file owns one bounded diagnostic decode-state step over prefill/KV
 * state summaries. It does not produce logits, sample, generate, benchmark, or
 * claim full model decode.
 */

#include <yvex/decode.h>
#include "yvex_console_private.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int decode_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long decode_mix_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int decode_add_ull(unsigned long long a,
                          unsigned long long b,
                          unsigned long long *out)
{
    if (!out || a > ULLONG_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static void decode_summary_defaults(yvex_decode_step_summary *out,
                                    const yvex_decode_step_options *options)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->decode_step_kind = "bounded-diagnostic";
    out->decode_phase = "preflight";
    out->decode_execution_mode = "prefill-summary-advance";
    out->backend_name = options && options->backend_name ? options->backend_name : "cpu";
    out->segment_name = options && options->segment_name ? options->segment_name : "embedding-rmsnorm";
    out->context_boundary_status = "unchecked";
    out->prefill_state_kind = "none";
    out->prefill_phase = "not-started";
    out->kv_binding_source = "none";
    out->kv_status = "not-requested";
    out->decode_state_source = "prefill-aggregate";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
}

static void decode_copy_prefill_summary(yvex_decode_step_summary *out,
                                        const yvex_prefill_state_summary *prefill)
{
    if (!out || !prefill) {
        return;
    }
    out->input_token_count = prefill->token_count;
    out->prefill_tokens_processed = prefill->tokens_processed;
    out->prefill_position_start = prefill->position_start;
    out->prefill_position_end = prefill->position_end;
    out->context_length = prefill->context_length;
    out->context_boundary_status = prefill->context_boundary_status
                                       ? prefill->context_boundary_status
                                       : "unchecked";
    out->prefill_state_created = prefill->prefill_state_created;
    out->prefill_state_kind = prefill->prefill_state_kind
                                  ? prefill->prefill_state_kind
                                  : "segment-summary";
    out->prefill_phase = prefill->prefill_phase ? prefill->prefill_phase : "unknown";
    out->prefill_aggregate_checksum = prefill->aggregate_checksum;
    out->prefill_final_token_checksum = prefill->final_token_checksum;
    out->kv_bound_to_prefill = prefill->kv_bound_to_prefill;
    out->kv_binding_source = prefill->kv_bound_to_prefill && prefill->kv_binding_source
                                 ? prefill->kv_binding_source
                                 : (prefill->kv_binding_source ? prefill->kv_binding_source : "none");
    out->kv_status = prefill->kv_status ? prefill->kv_status : "not-requested";
    out->kv_positions_written = prefill->kv_positions_written;
    out->kv_read_checksum = prefill->kv_read_checksum;
    out->cleanup_attempted = prefill->cleanup_attempted;
    out->cleanup_status = prefill->cleanup_status ? prefill->cleanup_status : "not-needed";
}

static unsigned long long decode_state_checksum(const yvex_prefill_state_summary *prefill,
                                                unsigned long long decode_position)
{
    unsigned long long hash = 1469598103934665603ull;

    hash = decode_mix_u64(hash, prefill->aggregate_checksum);
    hash = decode_mix_u64(hash, prefill->final_token_checksum);
    hash = decode_mix_u64(hash, prefill->layer_final_checksum);
    hash = decode_mix_u64(hash, prefill->kv_read_checksum);
    hash = decode_mix_u64(hash, decode_position);
    hash = decode_mix_u64(hash, prefill->token_count);
    hash = decode_mix_u64(hash, prefill->context_length);
    return hash;
}

static void decode_fill_state_values(yvex_decode_step_summary *out)
{
    unsigned long long hash;
    unsigned long long i;

    if (!out) {
        return;
    }
    hash = out->decode_state_checksum;
    out->decode_state_value_count = YVEX_DECODE_STATE_MAX_VALUES;
    for (i = 0; i < out->decode_state_value_count; ++i) {
        unsigned long long word;
        hash = decode_mix_u64(hash, out->decode_position + i);
        word = (hash >> ((i % 4ull) * 12ull)) & 0xffffull;
        out->decode_state_values[i] =
            (float)((double)word / 65536.0 +
                    (double)out->decode_position * 0.0001 +
                    (double)i * 0.00001);
    }
}

int yvex_engine_decode_step(yvex_engine *engine,
                            const yvex_decode_step_options *options,
                            yvex_decode_step_summary *out,
                            yvex_error *err)
{
    yvex_prefill_state_options prefill_options;
    yvex_prefill_state_summary prefill_summary;
    unsigned long long default_context_length = 0ull;
    unsigned long long decode_position = 0ull;
    int rc;

    if (!engine || !options || !options->token_input || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_decode_step",
                       "engine, options, token input, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    decode_summary_defaults(out, options);
    out->input_token_count = options->token_input->token_count;
    if (options->token_input->token_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_decode_step",
                       "decode requires at least one input token");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    prefill_options.token_input = options->token_input;
    prefill_options.segment_name = options->segment_name;
    prefill_options.position_start = options->position_start;
    prefill_options.chunk_size = options->chunk_size;
    prefill_options.context_length = options->context_length;
    if (prefill_options.context_length == 0ull) {
        if (!decode_add_ull(options->position_start,
                            options->token_input->token_count,
                            &default_context_length) ||
            !decode_add_ull(default_context_length, 1ull, &default_context_length)) {
            out->decode_phase = "context-boundary";
            out->context_boundary_status = "fail";
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_decode_step",
                           "decode context length overflow");
            return YVEX_ERR_BOUNDS;
        }
        prefill_options.context_length = default_context_length;
    }
    prefill_options.attach_kv = options->attach_kv;
    prefill_options.kv_shape = options->kv_shape;
    prefill_options.layer_count = options->layer_count;
    prefill_options.layer_hidden_dim = options->layer_hidden_dim;
    prefill_options.layer_head_dim = options->layer_head_dim;
    prefill_options.layer_ffn_dim = options->layer_ffn_dim;

    out->prefill_invoked = 1;
    out->decode_phase = "prefill";
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, err);
    decode_copy_prefill_summary(out, &prefill_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!prefill_summary.prefill_state_created) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_decode_step",
                       "prefill state was not created");
        return YVEX_ERR_STATE;
    }

    if (!decode_add_ull(prefill_summary.position_end, 1ull, &decode_position)) {
        out->decode_phase = "context-boundary";
        out->context_boundary_status = "fail";
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_decode_step",
                       "decode position overflow");
        return YVEX_ERR_BOUNDS;
    }
    out->decode_position = decode_position;
    if (decode_position >= prefill_summary.context_length) {
        out->decode_phase = "context-boundary";
        out->context_boundary_status = "fail";
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_decode_step",
                       "decode position exceeds context length");
        return YVEX_ERR_BOUNDS;
    }
    out->context_boundary_status = "pass";

    if (decode_test_env_enabled("YVEX_TEST_FAIL_DECODE_AFTER_PREFILL")) {
        out->decode_phase = "after-prefill";
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_decode_step",
                       "test decode failure after prefill");
        return YVEX_ERR_BACKEND;
    }

    out->decode_phase = "state-allocation";
    if (decode_test_env_enabled("YVEX_TEST_FAIL_DECODE_AFTER_STATE_ALLOC")) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_decode_step",
                       "test decode state allocation failure");
        return YVEX_ERR_NOMEM;
    }

    out->decode_state_checksum = decode_state_checksum(&prefill_summary, decode_position);
    decode_fill_state_values(out);
    out->decode_state_created = 1;
    out->decode_step_executed = 1;
    out->decode_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}

static void decode_print_summary(const yvex_decode_step_summary *summary,
                                 const char *model_arg,
                                 const char *backend_name,
                                 const char *token_input_status,
                                 const char *status)
{
    unsigned long long i;

    printf("decode: step\n");
    printf("status: decode-step\n");
    printf("model: %s\n", model_arg ? model_arg : "");
    printf("backend: %s\n",
           summary && summary->backend_name ? summary->backend_name
                                            : (backend_name ? backend_name : "cpu"));
    printf("segment: %s\n",
           summary && summary->segment_name ? summary->segment_name : "embedding-rmsnorm");
    printf("token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    printf("input_token_count: %llu\n", summary ? summary->input_token_count : 0ull);
    printf("prefill_invoked: %s\n", summary && summary->prefill_invoked ? "true" : "false");
    printf("prefill_state_created: %s\n",
           summary && summary->prefill_state_created ? "true" : "false");
    printf("prefill_state_kind: %s\n",
           summary && summary->prefill_state_kind ? summary->prefill_state_kind : "none");
    printf("prefill_phase: %s\n",
           summary && summary->prefill_phase ? summary->prefill_phase : "not-started");
    printf("prefill_tokens_processed: %llu\n",
           summary ? summary->prefill_tokens_processed : 0ull);
    printf("prefill_position_start: %llu\n",
           summary ? summary->prefill_position_start : 0ull);
    printf("prefill_position_end: %llu\n",
           summary ? summary->prefill_position_end : 0ull);
    printf("decode_position: %llu\n", summary ? summary->decode_position : 0ull);
    printf("context_length: %llu\n", summary ? summary->context_length : 0ull);
    printf("context_boundary_status: %s\n",
           summary && summary->context_boundary_status
               ? summary->context_boundary_status
               : "unchecked");
    printf("decode_state_created: %s\n",
           summary && summary->decode_state_created ? "true" : "false");
    printf("decode_step_executed: %s\n",
           summary && summary->decode_step_executed ? "true" : "false");
    printf("decode_step_kind: %s\n",
           summary && summary->decode_step_kind ? summary->decode_step_kind : "bounded-diagnostic");
    printf("decode_phase: %s\n",
           summary && summary->decode_phase ? summary->decode_phase : "preflight");
    printf("decode_execution_mode: %s\n",
           summary && summary->decode_execution_mode
               ? summary->decode_execution_mode
               : "prefill-summary-advance");
    printf("decode_state_source: %s\n",
           summary && summary->decode_state_source ? summary->decode_state_source : "prefill-aggregate");
    printf("decode_state_checksum: %llu\n",
           summary ? summary->decode_state_checksum : 0ull);
    printf("decode_state_value_count: %llu\n",
           summary ? summary->decode_state_value_count : 0ull);
    if (summary) {
        for (i = 0; i < summary->decode_state_value_count; ++i) {
            printf("decode_state_value_%llu: %.9g\n",
                   i, (double)summary->decode_state_values[i]);
        }
    }
    printf("prefill_aggregate_checksum: %llu\n",
           summary ? summary->prefill_aggregate_checksum : 0ull);
    printf("prefill_final_token_checksum: %llu\n",
           summary ? summary->prefill_final_token_checksum : 0ull);
    printf("kv_bound_to_prefill: %s\n",
           summary && summary->kv_bound_to_prefill ? "true" : "false");
    printf("kv_binding_source: %s\n",
           summary && summary->kv_binding_source ? summary->kv_binding_source : "none");
    printf("kv_status: %s\n",
           summary && summary->kv_status ? summary->kv_status : "not-requested");
    printf("kv_positions_written: %llu\n",
           summary ? summary->kv_positions_written : 0ull);
    printf("kv_read_checksum: %llu\n", summary ? summary->kv_read_checksum : 0ull);
    printf("cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    printf("real_model_decode: false\n");
    printf("full_model_decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("sampling_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", status ? status : "decode-step-fail");
}

static void decode_cli_defaults(yvex_decode_step_summary *summary,
                                const char *backend_name,
                                const char *segment_name)
{
    yvex_decode_step_options options;

    memset(&options, 0, sizeof(options));
    options.backend_name = backend_name ? backend_name : "cpu";
    options.segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    decode_summary_defaults(summary, &options);
}

static int decode_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

int yvex_decode_command(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_decode_step_summary decode_summary;
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
    memset(&decode_summary, 0, sizeof(decode_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_decode_help(stdout);
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
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_count)) {
                fprintf(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            layer_count_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_hidden_dim)) {
                fprintf(stderr, "yvex: --layer-hidden-dim requires a positive integer\n");
                return 2;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_head_dim)) {
                fprintf(stderr, "yvex: --layer-head-dim requires a positive integer\n");
                return 2;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_ffn_dim)) {
                fprintf(stderr, "yvex: --layer-ffn-dim requires a positive integer\n");
                return 2;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &chunk_size)) {
                fprintf(stderr, "yvex: --chunk-size requires a positive integer\n");
                return 2;
            }
            chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position-start") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &position_start)) {
                fprintf(stderr, "yvex: --position-start requires a non-negative integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --context-length requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown decode option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help decode' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !backend_name || !tokens_text || !segment_name) {
        fprintf(stderr, "usage: yvex decode --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n");
        return 2;
    }
    if (!decode_backend_name_valid(backend_name)) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        fprintf(stderr, "yvex: unsupported decode segment: %s\n", segment_name);
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
    if (!layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        fprintf(stderr, "yvex: --layer-* options require --layers N\n");
        return 2;
    }
    if (layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        fprintf(stderr, "yvex: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim\n");
        return 2;
    }
    if (layer_count_seen && (layer_count == 0ull || layer_count > 16ull)) {
        fprintf(stderr, "yvex: --layers requires 1 <= N <= 16\n");
        return 2;
    }
    if (layer_count_seen && !layer_hidden_seen) {
        layer_hidden_dim = 8ull;
        layer_head_dim = 8ull;
        layer_ffn_dim = 16ull;
    }
    if (layer_count_seen && layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        fprintf(stderr, "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff\n",
                (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "decode");
    if (rc != YVEX_OK) {
        decode_cli_defaults(&decode_summary, backend_name, segment_name);
        decode_print_summary(&decode_summary, model_arg, backend_name, "fail", "decode-step-fail");
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
        decode_cli_defaults(&decode_summary, backend_name, segment_name);
        decode_summary.input_token_count = token_input.token_count;
        decode_print_summary(&decode_summary, model_arg, backend_name, "fail", "decode-step-fail");
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
        decode_cli_defaults(&decode_summary, backend_name, segment_name);
        decode_summary.input_token_count = token_input.token_count;
        decode_print_summary(&decode_summary, model_arg, backend_name, "pass", "decode-step-fail");
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
        decode_cli_defaults(&decode_summary, backend_name, segment_name);
        decode_summary.input_token_count = token_input.token_count;
        decode_print_summary(&decode_summary, model_arg, backend_name, "pass", "decode-step-fail");
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
    rc = yvex_engine_decode_step(engine, &decode_options, &decode_summary, &err);
    if (rc != YVEX_OK) {
        const char *status = "decode-step-fail";
        if (decode_summary.decode_phase &&
            strcmp(decode_summary.decode_phase, "after-prefill") == 0 &&
            decode_summary.cleanup_attempted &&
            decode_summary.cleanup_status &&
            strcmp(decode_summary.cleanup_status, "pass") == 0) {
            status = "decode-step-failed-cleaned";
        }
        decode_print_summary(&decode_summary, model_arg, backend_name, "pass", status);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    decode_print_summary(&decode_summary, model_arg, backend_name, "pass", "decode-step-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

void yvex_decode_help(FILE *fp)
{
    fprintf(fp, "usage: yvex decode --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n\nDecode creates one bounded diagnostic decode-state step from implemented prefill/KV state. It does not produce logits, sample, generate, or claim full DeepSeek decode.\n");
}
