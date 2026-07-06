/*
 * yvex_sampling.c - Sampling runtime boundary.
 *
 * This file owns one bounded greedy sampler over diagnostic logits. It does
 * not perform stochastic sampling, append tokens, generate, benchmark, or claim
 * real model vocabulary sampling.
 */

#include <yvex/sampling.h>
#include "yvex_console_private.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sample_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long sample_mix_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long sample_mix_float(unsigned long long hash, double value)
{
    float narrowed = (float)value;
    uint32_t bits = 0u;

    memcpy(&bits, &narrowed, sizeof(bits));
    return sample_mix_u64(hash, (unsigned long long)bits);
}

const char *yvex_sampling_strategy_name(yvex_sampling_strategy strategy)
{
    switch (strategy) {
    case YVEX_SAMPLING_STRATEGY_GREEDY: return "greedy";
    }
    return "unknown";
}

static void sample_summary_defaults(yvex_sampling_summary *out,
                                    const yvex_sampling_options *options)
{
    const yvex_logits_buffer_options *logits_options;
    const yvex_decode_step_options *decode_options;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    logits_options = options ? options->logits_options : NULL;
    decode_options = logits_options ? logits_options->decode_options : NULL;
    out->sampler_kind = "bounded-diagnostic";
    out->sampling_phase = "preflight";
    out->sampling_strategy = options ? yvex_sampling_strategy_name(options->strategy) : "greedy";
    out->sampling_source = "bounded-logits-buffer";
    out->backend_name = decode_options && decode_options->backend_name
                            ? decode_options->backend_name
                            : "cpu";
    out->logits_buffer_kind = "none";
    out->logits_phase = "not-started";
    out->tie_break = "lowest-index";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
}

static void sample_copy_logits_summary(yvex_sampling_summary *out,
                                       const yvex_logits_buffer_summary *logits)
{
    if (!out || !logits) {
        return;
    }
    out->backend_name = logits->backend_name ? logits->backend_name : out->backend_name;
    out->logits_buffer_created = logits->logits_buffer_created;
    out->logits_buffer_kind = logits->logits_buffer_kind ? logits->logits_buffer_kind
                                                         : "bounded-diagnostic";
    out->logits_phase = logits->logits_phase ? logits->logits_phase : "unknown";
    out->logits_count = logits->logits_count;
    out->logits_checksum = logits->logits_checksum;
    out->logits_min = logits->logits_min;
    out->logits_max = logits->logits_max;
}

static unsigned long long sample_checksum(const yvex_sampling_summary *summary)
{
    unsigned long long hash = 1469598103934665603ull;

    hash = sample_mix_u64(hash, summary->logits_checksum);
    hash = sample_mix_u64(hash, summary->selected_logit_index);
    hash = sample_mix_u64(hash, (unsigned long long)summary->selected_token_id);
    hash = sample_mix_float(hash, summary->selected_logit);
    return hash;
}

int yvex_engine_sample_token(yvex_engine *engine,
                             const yvex_sampling_options *options,
                             yvex_sampling_summary *out,
                             yvex_error *err)
{
    yvex_logits_buffer_summary logits_summary;
    unsigned long long i;
    unsigned long long candidate_count;
    double best;
    int rc;

    if (!engine || !options || !options->logits_options || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_sample_token",
                       "engine, options, logits options, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    sample_summary_defaults(out, options);
    if (options->strategy != YVEX_SAMPLING_STRATEGY_GREEDY) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_sample_token",
                       "only greedy sampling is implemented");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&logits_summary, 0, sizeof(logits_summary));
    out->logits_invoked = 1;
    out->sampling_phase = "logits";
    rc = yvex_engine_create_logits_buffer(engine, options->logits_options, &logits_summary, err);
    sample_copy_logits_summary(out, &logits_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!logits_summary.logits_buffer_created) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_sample_token",
                       "logits buffer was not created");
        return YVEX_ERR_STATE;
    }

    if (sample_test_env_enabled("YVEX_TEST_FAIL_SAMPLE_AFTER_LOGITS")) {
        out->sampling_phase = "after-logits";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_sample_token",
                       "test sampling failure after logits");
        return YVEX_ERR_BACKEND;
    }

    out->sampling_phase = "select";
    candidate_count = logits_summary.logits_sample_count;
    if (candidate_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_sample_token",
                       "logits buffer has no sample values to scan");
        return YVEX_ERR_STATE;
    }
    best = (double)logits_summary.logits_sample_values[0];
    out->selected_logit_index = 0ull;
    for (i = 1ull; i < candidate_count; ++i) {
        double value = (double)logits_summary.logits_sample_values[i];
        if (value > best) {
            best = value;
            out->selected_logit_index = i;
        }
    }
    out->candidates_considered = candidate_count;
    out->selected_token_id = (unsigned int)out->selected_logit_index;
    out->selected_logit = best;
    out->sample_created = 1;
    if (sample_test_env_enabled("YVEX_TEST_FAIL_SAMPLE_AFTER_SELECT")) {
        out->sampling_phase = "after-select";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_sample_token",
                       "test sampling failure after select");
        return YVEX_ERR_BACKEND;
    }

    out->sample_checksum = sample_checksum(out);
    out->sample_executed = 1;
    out->bounded_sampling_ready = 1;
    out->sampling_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}

static void sample_print_summary(const yvex_sampling_summary *summary,
                                 const char *model_arg,
                                 const char *backend_name,
                                 const char *segment_name,
                                 const char *token_input_status,
                                 unsigned long long input_token_count,
                                 const char *status)
{
    printf("sample: token\n");
    printf("status: sample-token\n");
    printf("model: %s\n", model_arg ? model_arg : "");
    printf("backend: %s\n",
           summary && summary->backend_name ? summary->backend_name
                                            : (backend_name ? backend_name : "cpu"));
    printf("segment: %s\n", segment_name ? segment_name : "embedding-rmsnorm");
    printf("token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    printf("input_token_count: %llu\n", input_token_count);
    printf("logits_invoked: %s\n", summary && summary->logits_invoked ? "true" : "false");
    printf("logits_buffer_created: %s\n",
           summary && summary->logits_buffer_created ? "true" : "false");
    printf("logits_buffer_kind: %s\n",
           summary && summary->logits_buffer_kind ? summary->logits_buffer_kind : "none");
    printf("logits_phase: %s\n",
           summary && summary->logits_phase ? summary->logits_phase : "not-started");
    printf("logits_count: %llu\n", summary ? summary->logits_count : 0ull);
    printf("logits_checksum: %llu\n", summary ? summary->logits_checksum : 0ull);
    printf("logits_min: %.9g\n", summary ? summary->logits_min : 0.0);
    printf("logits_max: %.9g\n", summary ? summary->logits_max : 0.0);
    printf("sample_created: %s\n", summary && summary->sample_created ? "true" : "false");
    printf("sample_executed: %s\n", summary && summary->sample_executed ? "true" : "false");
    printf("sampler_kind: %s\n",
           summary && summary->sampler_kind ? summary->sampler_kind : "bounded-diagnostic");
    printf("sampling_phase: %s\n",
           summary && summary->sampling_phase ? summary->sampling_phase : "preflight");
    printf("sampling_strategy: %s\n",
           summary && summary->sampling_strategy ? summary->sampling_strategy : "greedy");
    printf("sampling_source: %s\n",
           summary && summary->sampling_source ? summary->sampling_source : "bounded-logits-buffer");
    printf("candidates_considered: %llu\n",
           summary ? summary->candidates_considered : 0ull);
    printf("tie_break: %s\n", summary && summary->tie_break ? summary->tie_break : "lowest-index");
    printf("selected_logit_index: %llu\n",
           summary ? summary->selected_logit_index : 0ull);
    printf("selected_token_id: %u\n", summary ? summary->selected_token_id : 0u);
    printf("selected_logit: %.9g\n", summary ? summary->selected_logit : 0.0);
    printf("sample_checksum: %llu\n", summary ? summary->sample_checksum : 0ull);
    printf("cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    printf("bounded_sampling_ready: %s\n",
           summary && summary->bounded_sampling_ready ? "true" : "false");
    printf("real_vocab_sampling: false\n");
    printf("real_model_sampling: false\n");
    printf("sampling_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", status ? status : "sample-token-fail");
}

static void sample_cli_defaults(yvex_sampling_summary *summary,
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

static int sample_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

static int sample_parse_strategy(const char *text,
                                 yvex_sampling_strategy *strategy)
{
    if (!text || !strategy) {
        return 0;
    }
    if (strcmp(text, "greedy") == 0) {
        *strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
        return 1;
    }
    return 0;
}

static int sample_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

int yvex_sample_command(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options sample_options;
    yvex_sampling_summary sample_summary;
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
    yvex_sampling_strategy strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
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
    memset(&sample_options, 0, sizeof(sample_options));
    memset(&sample_summary, 0, sizeof(sample_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_sample_help(stdout);
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
        } else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc || !sample_parse_strategy(argv[i + 1], &strategy)) {
                fprintf(stderr, "yvex: --strategy supports only greedy\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &logits_count) ||
                !sample_logits_count_valid(logits_count)) {
                fprintf(stderr, "yvex: --logits-count requires 1 <= N <= 256\n");
                return 2;
            }
            i += 1;
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
            fprintf(stderr, "yvex: unknown sample option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help sample' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !backend_name || !tokens_text || !segment_name) {
        fprintf(stderr, "usage: yvex sample --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N] [--strategy greedy]\n");
        return 2;
    }
    if (!sample_backend_name_valid(backend_name)) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        fprintf(stderr, "yvex: unsupported sample segment: %s\n", segment_name);
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
    rc = enforce_registered_identity_cli(&model_ref, "sample");
    if (rc != YVEX_OK) {
        sample_cli_defaults(&sample_summary, backend_name);
        sample_print_summary(&sample_summary, model_arg, backend_name, segment_name, "fail", 0ull,
                             "sample-token-fail");
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
        sample_cli_defaults(&sample_summary, backend_name);
        sample_print_summary(&sample_summary, model_arg, backend_name, segment_name, "fail",
                             token_input.token_count, "sample-token-fail");
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
        sample_cli_defaults(&sample_summary, backend_name);
        sample_print_summary(&sample_summary, model_arg, backend_name, segment_name, "pass",
                             token_input.token_count, "sample-token-fail");
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
        sample_cli_defaults(&sample_summary, backend_name);
        sample_print_summary(&sample_summary, model_arg, backend_name, segment_name, "pass",
                             token_input.token_count, "sample-token-fail");
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
    sample_options.logits_options = &logits_options;
    sample_options.strategy = strategy;

    rc = yvex_engine_sample_token(engine, &sample_options, &sample_summary, &err);
    if (rc != YVEX_OK) {
        sample_print_summary(&sample_summary, model_arg, backend_name, segment_name, "pass",
                             token_input.token_count, "sample-token-fail");
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    sample_print_summary(&sample_summary, model_arg, backend_name, segment_name, "pass",
                         token_input.token_count, "sample-token-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

void yvex_sample_help(FILE *fp)
{
    fprintf(fp, "usage: yvex sample --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N] [--strategy greedy]\n\nSample selects one bounded diagnostic token from the implemented logits buffer using greedy selection. It does not run stochastic sampling, append tokens, generate, or claim real DeepSeek vocabulary sampling.\n");
}
