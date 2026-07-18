/*
 * logits.c - logits diagnostic command surface.
 *
 * Owner: CLI logits command.
 * Owns: logits argv validation, dispatch, help, and compatibility rendering.
 * Does not own: logits buffer semantics, decode, sampling, or generation.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes the bounded logits API and returns process exit status.
 */
#include "src/core/operator.h"
#include "src/cli/io/out.h"
#include <yvex/api.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    yvex_logits_buffer_summary_init(summary, &options);
}

static int logits_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

int yvex_logits_command(int arg_count, char **args)
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

    if (arg_count < 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        yvex_logits_help(stdout);
        return arg_count >= 3 ? 0 : 2;
    }

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--segment") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = args[++i];
        } else if (strcmp(args[i], "--tokens") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = args[++i];
        } else if (strcmp(args[i], "--logits-count") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &logits_count) ||
                !yvex_logits_count_valid(logits_count)) {
                yvex_cli_out_writef(stderr, "yvex: --logits-count requires 1 <= N <= 256\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(args[i], "--kv-layers") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--kv-heads") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.kv_head_count)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--kv-head-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--kv-capacity") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &kv_shape.capacity)) {
                yvex_cli_out_writef(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layers") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            layer_count_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_hidden_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-hidden-dim requires a positive integer\n");
                return 2;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layer-head-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-head-dim requires a positive integer\n");
                return 2;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &layer_ffn_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --layer-ffn-dim requires a positive integer\n");
                return 2;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--chunk-size") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &chunk_size)) {
                yvex_cli_out_writef(stderr, "yvex: --chunk-size requires a positive integer\n");
                return 2;
            }
            chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(args[i], "--position-start") == 0) {
            if (i + 1 >= arg_count || !parse_ull_allow_zero(args[i + 1], &position_start)) {
                yvex_cli_out_writef(stderr, "yvex: --position-start requires a non-negative integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(args[i], "--context-length") == 0) {
            if (i + 1 >= arg_count || !parse_positive_ull(args[i + 1], &context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --context-length requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown logits option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help logits' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !backend_name || !tokens_text || !segment_name) {
        yvex_cli_out_writef(stderr, "usage: " "yvex logits --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N]\n");
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
        rc = yvex_model_context_vocab_size(model_ref.path, &vocab_size, &err);
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

    rc = yvex_graph_preflight(&model_ref,
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
        yvex_cli_graph_guard_print(&graph_guard);
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
    yvex_cli_out_writef(fp, "usage: " "yvex logits --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N]\n\nLogits creates a bounded diagnostic logits buffer from the implemented decode state. It does not run the real model output head, sample, generate, or claim DeepSeek logits.\n");
}
