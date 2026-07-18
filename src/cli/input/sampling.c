/*
 * sampling.c - sampling command input parser.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   CLI grammar and value validation for the sampling command.
 *
 * Does not own:
 *   model reference resolution, graph guard preflight, engine open, sampler
 *   execution, report construction, rendering, stdout/stderr output,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   the parser only translates borrowed argv into a typed request.
 *
 * Boundary:
 *   input parsing is not bounded sampling execution.
 */
#include "sampling.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void sampling_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "sample", message);
}

static void sampling_arg_errorf(yvex_error *err,
                                const char *fmt,
                                const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "sample", fmt,
                    value ? value : "");
}

static int sampling_parse_ull_raw(const char *text,
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

static int sampling_parse_positive_ull(const char *text,
                                       unsigned long long *out)
{
    return sampling_parse_ull_raw(text, out, 0);
}

static int sampling_parse_ull_allow_zero(const char *text,
                                         unsigned long long *out)
{
    return sampling_parse_ull_raw(text, out, 1);
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

static void sampling_args_defaults(yvex_sampling_args *out)
{
    memset(out, 0, sizeof(*out));
    out->request.strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
    out->request.logits_count = 16ull;
    out->render_mode = YVEX_SAMPLING_REPORT_NORMAL;
    out->help_exit_code = 0;
}

static int sampling_args_require_value(int argc,
                                       char **argv,
                                       int index,
                                       const char *message,
                                       yvex_error *err)
{
    (void)argv;
    if (index + 1 >= argc) {
        sampling_arg_error(err, message);
        return 0;
    }
    return 1;
}

static int sampling_args_validate_shape(const yvex_sampling_report_request *req,
                                        int kv_shape_seen,
                                        int layer_hidden_seen,
                                        int layer_head_seen,
                                        int layer_ffn_seen,
                                        yvex_error *err)
{
    if (!req->model_arg || !req->backend_name || !req->tokens_text ||
        !req->segment_name) {
        sampling_arg_error(err,
            "usage: yvex sample --model FILE_OR_ALIAS --backend cpu|cuda "
            "--segment embedding-rmsnorm --tokens IDS [--layers N "
            "[--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] "
            "[--chunk-size N] [--position-start N] [--context-length N] "
            "[--attach-kv --kv-layers N --kv-heads N --kv-head-dim N "
            "--kv-capacity N] [--logits-count N] [--strategy greedy]");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!sample_backend_name_valid(req->backend_name)) {
        sampling_arg_errorf(err, "yvex: unknown backend kind: %s",
                            req->backend_name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(req->segment_name, "embedding-rmsnorm") != 0) {
        sampling_arg_errorf(err, "yvex: unsupported sample segment: %s",
                            req->segment_name);
        return YVEX_ERR_INVALID_ARG;
    }
    if (kv_shape_seen && !req->attach_kv) {
        sampling_arg_error(err, "yvex: --kv-* options require --attach-kv");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->attach_kv && (req->kv_shape.layer_count == 0ull ||
                           req->kv_shape.kv_head_count == 0ull ||
                           req->kv_shape.head_dim == 0ull ||
                           req->kv_shape.capacity == 0ull)) {
        sampling_arg_error(err,
            "yvex: --attach-kv requires --kv-layers, --kv-heads, "
            "--kv-head-dim, and --kv-capacity");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!req->layer_count_seen &&
        (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        sampling_arg_error(err, "yvex: --layer-* options require --layers N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layer_count_seen &&
        (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        sampling_arg_error(err,
            "yvex: custom layer dimensions require --layer-hidden-dim, "
            "--layer-head-dim, and --layer-ffn-dim");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layer_count_seen &&
        (req->layer_count == 0ull || req->layer_count > 16ull)) {
        sampling_arg_error(err, "yvex: --layers requires 1 <= N <= 16");
        return YVEX_ERR_INVALID_ARG;
    }
    if (req->layer_count_seen &&
        req->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "sample",
            "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff",
            (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

int yvex_sampling_args_parse(int argc,
                             char **argv,
                             yvex_sampling_args *out,
                             yvex_error *err)
{
    yvex_sampling_report_request *req;
    int kv_shape_seen = 0;
    int layer_hidden_seen = 0;
    int layer_head_seen = 0;
    int layer_ffn_seen = 0;
    int i;
    int rc;

    if (!out) {
        sampling_arg_error(err, "sample parser requires output");
        return YVEX_ERR_INVALID_ARG;
    }
    sampling_args_defaults(out);
    req = &out->request;

    if (argc < 3) {
        out->help_requested = 1;
        out->help_exit_code = 2;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        out->help_requested = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (!sampling_args_require_value(argc, argv, i,
                                             "yvex: --model requires FILE_OR_ALIAS",
                                             err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (!sampling_args_require_value(argc, argv, i,
                                             "yvex: --backend requires cpu|cuda",
                                             err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (!sampling_args_require_value(argc, argv, i,
                                             "yvex: --segment requires embedding-rmsnorm",
                                             err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (!sampling_args_require_value(argc, argv, i,
                                             "yvex: --tokens requires IDS",
                                             err)) {
                return YVEX_ERR_INVALID_ARG;
            }
            req->tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc ||
                !sample_parse_strategy(argv[i + 1], &req->strategy)) {
                sampling_arg_error(err, "yvex: --strategy supports only greedy");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->logits_count) ||
                !sample_logits_count_valid(req->logits_count)) {
                sampling_arg_error(err,
                                   "yvex: --logits-count requires 1 <= N <= 256");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            req->attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->kv_shape.layer_count)) {
                sampling_arg_error(err,
                                   "yvex: --kv-layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->kv_shape.kv_head_count)) {
                sampling_arg_error(err,
                                   "yvex: --kv-heads requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->kv_shape.head_dim)) {
                sampling_arg_error(err,
                                   "yvex: --kv-head-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->kv_shape.capacity)) {
                sampling_arg_error(err,
                                   "yvex: --kv-capacity requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->layer_count)) {
                sampling_arg_error(err,
                                   "yvex: --layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            req->layer_count_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->layer_hidden_dim)) {
                sampling_arg_error(err,
                    "yvex: --layer-hidden-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-head-dim") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->layer_head_dim)) {
                sampling_arg_error(err,
                    "yvex: --layer-head-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->layer_ffn_dim)) {
                sampling_arg_error(err,
                    "yvex: --layer-ffn-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1], &req->chunk_size)) {
                sampling_arg_error(err,
                                   "yvex: --chunk-size requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            req->chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position-start") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_ull_allow_zero(argv[i + 1],
                                               &req->position_start)) {
                sampling_arg_error(err,
                    "yvex: --position-start requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            if (i + 1 >= argc ||
                !sampling_parse_positive_ull(argv[i + 1],
                                             &req->context_length)) {
                sampling_arg_error(err,
                    "yvex: --context-length requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            req->context_length_seen = 1;
            i += 1;
        } else {
            yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "sample",
                            "yvex: unknown sample option: %s\n"
                            "Try 'yvex help sample' for usage.",
                            argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
    }

    if (req->layer_count_seen && !layer_hidden_seen) {
        req->layer_hidden_dim = 8ull;
        req->layer_head_dim = 8ull;
        req->layer_ffn_dim = 16ull;
    }
    rc = sampling_args_validate_shape(req,
                                      kv_shape_seen,
                                      layer_hidden_seen,
                                      layer_head_seen,
                                      layer_ffn_seen,
                                      err);
    if (rc != YVEX_OK) {
        return rc;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
