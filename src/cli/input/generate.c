/*
 * generate.c - generate command argument parser.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   CLI grammar and option validation for the generate command.
 *
 * Does not own:
 *   generation execution, report rendering, command dispatch, runtime
 *   support, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   parser output is typed; invalid values fail before entering the
 *   generation domain.
 *
 * Boundary:
 *   parsing arguments is not diagnostic generation execution or support.
 */
#include "generate.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void generate_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "generate", message);
}

static void generate_arg_errorf(yvex_error *err,
                                const char *fmt,
                                const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "generate", fmt,
                    value ? value : "");
}

static int generate_parse_trace_level(const char *text,
                                      yvex_generation_trace_level *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "none") == 0) {
        *out = YVEX_GENERATION_TRACE_NONE;
    } else if (strcmp(text, "tokens") == 0) {
        *out = YVEX_GENERATION_TRACE_TOKENS;
    } else if (strcmp(text, "steps") == 0) {
        *out = YVEX_GENERATION_TRACE_STEPS;
    } else if (strcmp(text, "kv") == 0) {
        *out = YVEX_GENERATION_TRACE_KV;
    } else if (strcmp(text, "logits") == 0) {
        *out = YVEX_GENERATION_TRACE_LOGITS;
    } else if (strcmp(text, "sampling") == 0) {
        *out = YVEX_GENERATION_TRACE_SAMPLING;
    } else if (strcmp(text, "full") == 0) {
        *out = YVEX_GENERATION_TRACE_FULL;
    } else {
        return 0;
    }
    return 1;
}

static int generate_parse_output_mode(const char *text,
                                      yvex_generate_render_mode *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "normal") == 0) {
        *out = YVEX_GENERATE_RENDER_NORMAL;
        return 1;
    }
    if (strcmp(text, "audit") == 0) {
        *out = YVEX_GENERATE_RENDER_AUDIT;
        return 1;
    }
    return 0;
}

static int generate_parse_strategy(const char *text)
{
    return text && strcmp(text, "greedy") == 0;
}

static int generate_parse_ull_raw(const char *text,
                                  unsigned long long *out,
                                  int allow_zero)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out || text[0] == '\0' || text[0] == '-') {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        return 0;
    }
    if (!allow_zero && value == 0ull) {
        return 0;
    }
    *out = value;
    return 1;
}

static int generate_parse_positive_ull_cli(const char *text,
                                           unsigned long long *out)
{
    return generate_parse_ull_raw(text, out, 0);
}

static int generate_parse_ull_allow_zero_cli(const char *text,
                                             unsigned long long *out)
{
    return generate_parse_ull_raw(text, out, 1);
}

static int generate_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

static int generate_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

/*
 * yvex_generate_args_parse()
 *
 * Purpose:
 *   parse generate command CLI arguments into a typed generation request.
 *
 * Inputs:
 *   argc/argv are borrowed command arguments from yvex generate.
 *
 * Effects:
 *   fills out with parsed options only; it does not resolve models, open
 *   engines, render output, or execute generation.
 *
 * Failure:
 *   returns invalid-arg with exact parser text for missing/unsupported options
 *   and malformed numeric values.
 *
 * Boundary:
 *   argument parsing is not generation execution or support.
 */
int yvex_generate_args_parse(int argc,
                             char **argv,
                             yvex_generate_args *out,
                             yvex_error *err)
{
    yvex_generation_request *request;
    int max_new_tokens_seen = 0;
    int kv_shape_seen = 0;
    int layer_hidden_seen = 0;
    int layer_head_seen = 0;
    int layer_ffn_seen = 0;
    int i;

    if (!out) {
        generate_arg_error(err, "error: internal parser output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->render_mode = YVEX_GENERATE_RENDER_NORMAL;
    request = &out->request;
    request->logits_count = 16ull;
    request->trace_level = YVEX_GENERATION_TRACE_NONE;

    if (argc >= 3 &&
        (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        out->help_requested = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (argc < 3) {
        generate_arg_error(
            err,
            "error: generate requires --model FILE_OR_ALIAS\n"
            "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda "
            "--segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\n"
            "Try 'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --model requires FILE_OR_ALIAS");
                return YVEX_ERR_INVALID_ARG;
            }
            request->model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --backend requires cpu|cuda");
                return YVEX_ERR_INVALID_ARG;
            }
            request->backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --segment requires embedding-rmsnorm");
                return YVEX_ERR_INVALID_ARG;
            }
            request->segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --tokens requires IDS");
                return YVEX_ERR_INVALID_ARG;
            }
            request->tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--max-new-tokens") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->max_new_tokens)) {
                generate_arg_error(err, "error: --max-new-tokens must be an integer greater than 0");
                return YVEX_ERR_INVALID_ARG;
            }
            max_new_tokens_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc || !generate_parse_strategy(argv[i + 1])) {
                generate_arg_error(err, "error: --strategy currently supports greedy only");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--trace-level") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_trace_level(argv[i + 1],
                                            &request->trace_level)) {
                generate_arg_error(err, "error: --trace-level requires none|tokens|steps|kv|logits|sampling|full");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            out->render_mode = YVEX_GENERATE_RENDER_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                generate_arg_error(err, "error: --output requires normal|audit");
                return YVEX_ERR_INVALID_ARG;
            }
            if (!generate_parse_output_mode(argv[++i], &out->render_mode)) {
                generate_arg_errorf(err, "error: unsupported output mode: %s", argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(argv[i], "--json") == 0) {
            generate_arg_error(err, "error: JSON output is unsupported for generate; use --output normal|audit");
            return YVEX_ERR_INVALID_ARG;
        } else if (strcmp(argv[i], "--cancel-after-steps") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_ull_allow_zero_cli(argv[i + 1],
                                                   &request->cancel_after_steps)) {
                generate_arg_error(err, "error: --cancel-after-steps must be a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            request->cancel_after_steps_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->logits_count) ||
                !generate_logits_count_valid(request->logits_count)) {
                generate_arg_error(err, "error: --logits-count requires 1 <= N <= 256");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            request->attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->kv_shape.layer_count)) {
                generate_arg_error(err, "error: --kv-layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->kv_shape.kv_head_count)) {
                generate_arg_error(err, "error: --kv-heads requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->kv_shape.head_dim)) {
                generate_arg_error(err, "error: --kv-head-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->kv_shape.capacity)) {
                generate_arg_error(err, "error: --kv-capacity requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->layer_count)) {
                generate_arg_error(err, "error: --layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            request->layer_count_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->layer_hidden_dim)) {
                generate_arg_error(err, "error: --layer-hidden-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-head-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->layer_head_dim)) {
                generate_arg_error(err, "error: --layer-head-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->layer_ffn_dim)) {
                generate_arg_error(err, "error: --layer-ffn-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->chunk_size)) {
                generate_arg_error(err, "error: --chunk-size requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            request->chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position-start") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_ull_allow_zero_cli(argv[i + 1],
                                                   &request->position_start)) {
                generate_arg_error(err, "error: --position-start requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1],
                                                 &request->context_length)) {
                generate_arg_error(err, "error: --context-length requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            request->context_length_seen = 1;
            i += 1;
        } else {
            generate_arg_errorf(err, "error: unknown generate option: %s\nTry 'yvex help generate' for usage.", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
    }

    if (!request->model_arg) {
        generate_arg_error(err, "error: generate requires --model FILE_OR_ALIAS\nusage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\nTry 'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!request->backend_name) {
        generate_arg_error(err, "error: generate requires --backend cpu|cuda\nusage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\nTry 'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!request->segment_name) {
        generate_arg_error(err, "error: generate requires --segment embedding-rmsnorm\nusage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\nTry 'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!request->tokens_text) {
        generate_arg_error(err, "error: generate requires --tokens IDS\nusage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\nTry 'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!max_new_tokens_seen) {
        generate_arg_error(err, "error: generate requires --max-new-tokens N\nusage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\nTry 'yvex help generate' for examples and boundaries.");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!generate_backend_name_valid(request->backend_name)) {
        generate_arg_error(err, "error: --backend supports cpu|cuda for bounded diagnostics");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(request->segment_name, "embedding-rmsnorm") != 0) {
        generate_arg_error(err, "error: generate segment currently supports embedding-rmsnorm for bounded diagnostics");
        return YVEX_ERR_INVALID_ARG;
    }
    if (kv_shape_seen && !request->attach_kv) {
        generate_arg_error(err, "error: --kv-* options require --attach-kv");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->attach_kv &&
        (request->kv_shape.layer_count == 0ull ||
         request->kv_shape.kv_head_count == 0ull ||
         request->kv_shape.head_dim == 0ull ||
         request->kv_shape.capacity == 0ull)) {
        generate_arg_error(err, "error: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!request->layer_count_seen &&
        (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        generate_arg_error(err, "error: --layer-* options require --layers N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->layer_count_seen &&
        (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        generate_arg_error(err, "error: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->layer_count_seen &&
        (request->layer_count == 0ull || request->layer_count > 16ull)) {
        generate_arg_error(err, "error: --layers requires 1 <= N <= 16");
        return YVEX_ERR_INVALID_ARG;
    }
    if (request->layer_count_seen && !layer_hidden_seen) {
        request->layer_hidden_dim = 8ull;
        request->layer_head_dim = 8ull;
        request->layer_ffn_dim = 16ull;
    }
    if (request->layer_count_seen &&
        request->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "generate",
                        "error: --layer-hidden-dim cannot exceed %u for sampled segment handoff",
                        (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
