/*
 * yvex_kv_args.c - KV command argument parser.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   CLI grammar and option validation for KV report and ownership commands.
 *
 * Does not own:
 *   KV cache allocation, report construction, command dispatch, rendering,
 *   stdout/stderr output, attention execution, decode, logits, sampling,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   parser output is typed and domain-free; invalid values fail before report
 *   construction or KV allocation.
 *
 * Boundary:
 *   parsing arguments is not runtime KV support.
 */
#include "yvex_kv_args.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void kv_arg_error(yvex_error *err, const char *message)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "kv", message);
}

static void kv_arg_errorf(yvex_error *err,
                          const char *fmt,
                          const char *value)
{
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "kv", fmt, value ? value : "");
}

static int kv_parse_ull_raw(const char *text,
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

static int kv_parse_positive_ull(const char *text, unsigned long long *out)
{
    return kv_parse_ull_raw(text, out, 0);
}

static int kv_parse_ull_allow_zero(const char *text, unsigned long long *out)
{
    return kv_parse_ull_raw(text, out, 1);
}

static int kv_parse_report_mode(const char *text, yvex_kv_report_mode *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "normal") == 0) {
        *out = YVEX_KV_REPORT_MODE_NORMAL;
        return 1;
    }
    if (strcmp(text, "table") == 0) {
        *out = YVEX_KV_REPORT_MODE_TABLE;
        return 1;
    }
    if (strcmp(text, "audit") == 0) {
        *out = YVEX_KV_REPORT_MODE_AUDIT;
        return 1;
    }
    return 0;
}

static void kv_request_defaults(yvex_kv_report_request *request)
{
    memset(request, 0, sizeof(*request));
    request->kind = YVEX_KV_REQUEST_OWNERSHIP;
    request->family = "auto";
    request->backend = "cpu";
    request->report_mode = YVEX_KV_REPORT_MODE_NORMAL;
}

static int kv_parse_report_args(int argc,
                                char **argv,
                                yvex_kv_args *out,
                                yvex_error *err)
{
    yvex_kv_report_request *request = &out->request;
    int i;

    request->kind = YVEX_KV_REQUEST_REPORT;
    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                kv_arg_error(err, "yvex: kv report --model requires FILE_OR_ALIAS");
                return YVEX_ERR_INVALID_ARG;
            }
            request->model = argv[++i];
        } else if (strcmp(argv[i], "--family") == 0) {
            if (i + 1 >= argc) {
                kv_arg_error(err, "yvex: kv report --family requires a family name");
                return YVEX_ERR_INVALID_ARG;
            }
            request->family = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                kv_arg_error(err, "yvex: kv report --backend requires cpu or cuda");
                return YVEX_ERR_INVALID_ARG;
            }
            request->backend = argv[++i];
        } else if (strcmp(argv[i], "--registry") == 0) {
            if (i + 1 >= argc) {
                kv_arg_error(err, "yvex: kv report --registry requires FILE");
                return YVEX_ERR_INVALID_ARG;
            }
            request->registry_path = argv[++i];
        } else if (strcmp(argv[i], "--include-attention") == 0) {
            request->include_attention = 1;
        } else if (strcmp(argv[i], "--include-context") == 0) {
            request->include_context = 1;
        } else if (strcmp(argv[i], "--include-residency") == 0) {
            request->include_residency = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            request->include_blockers = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            request->report_mode = YVEX_KV_REPORT_MODE_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                kv_arg_error(err,
                             "yvex: kv report --output requires normal, table, or audit");
                return YVEX_ERR_INVALID_ARG;
            }
            if (!kv_parse_report_mode(argv[++i], &request->report_mode)) {
                kv_arg_errorf(err,
                              "yvex: kv report unsupported output mode: %s",
                              argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            out->help_requested = 1;
            yvex_error_clear(err);
            return YVEX_OK;
        } else {
            kv_arg_errorf(err, "yvex: unknown kv report option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
    }
    if (!request->model) {
        kv_arg_error(err,
                     "usage: yvex kv report --model FILE_OR_ALIAS "
                     "[--family auto|deepseek|glm|qwen|llama] [--backend cpu|cuda]");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int kv_parse_ownership_args(int argc,
                                   char **argv,
                                   yvex_kv_args *out,
                                   yvex_error *err)
{
    yvex_kv_report_request *request = &out->request;
    int i;

    request->kind = YVEX_KV_REQUEST_OWNERSHIP;
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc ||
                !kv_parse_positive_ull(argv[i + 1], &request->shape.layer_count)) {
                kv_arg_error(err, "yvex: --layers requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--heads") == 0 ||
                   strcmp(argv[i], "--lanes") == 0) {
            if (i + 1 >= argc ||
                !kv_parse_positive_ull(argv[i + 1], &request->shape.kv_head_count)) {
                kv_arg_error(err, "yvex: --heads requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc ||
                !kv_parse_positive_ull(argv[i + 1], &request->shape.head_dim)) {
                kv_arg_error(err, "yvex: --head-dim requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--capacity") == 0) {
            if (i + 1 >= argc ||
                !kv_parse_positive_ull(argv[i + 1], &request->shape.capacity)) {
                kv_arg_error(err, "yvex: --capacity requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
            i += 1;
        } else if (strcmp(argv[i], "--append-demo") == 0) {
            request->append_demo = 1;
        } else if (strcmp(argv[i], "--read-position") == 0) {
            if (i + 1 >= argc ||
                !kv_parse_ull_allow_zero(argv[i + 1], &request->read_position)) {
                kv_arg_error(err,
                             "yvex: --read-position requires a non-negative integer");
                return YVEX_ERR_INVALID_ARG;
            }
            request->read_requested = 1;
            i += 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            request->report_mode = YVEX_KV_REPORT_MODE_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                kv_arg_error(err, "yvex: kv --output requires normal, table, or audit");
                return YVEX_ERR_INVALID_ARG;
            }
            if (!kv_parse_report_mode(argv[++i], &request->report_mode)) {
                kv_arg_errorf(err, "yvex: kv unsupported output mode: %s", argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            out->help_requested = 1;
            yvex_error_clear(err);
            return YVEX_OK;
        } else {
            kv_arg_errorf(err, "yvex: unknown kv option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
    }
    if (request->shape.layer_count == 0ull ||
        request->shape.kv_head_count == 0ull ||
        request->shape.head_dim == 0ull ||
        request->shape.capacity == 0ull) {
        kv_arg_error(err,
                     "usage: yvex kv --layers N --heads N --head-dim N "
                     "--capacity N [--append-demo] [--read-position N]");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * yvex_kv_args_parse()
 *
 * Purpose:
 *   parse KV command input into a typed KV report request.
 *
 * Inputs:
 *   argc and argv are borrowed command arguments from yvex kv.
 *
 * Effects:
 *   fills out with parsed values only; it does not resolve models, allocate KV,
 *   append/read KV values, render output, or write streams.
 *
 * Failure:
 *   returns invalid-arg with parser text for missing values, malformed numbers,
 *   unsupported modes, and unknown options.
 *
 * Boundary:
 *   parsing is not KV report construction or runtime support.
 */
int yvex_kv_args_parse(int argc,
                       char **argv,
                       yvex_kv_args *out,
                       yvex_error *err)
{
    if (!out) {
        kv_arg_error(err, "yvex: internal KV parser output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    kv_request_defaults(&out->request);

    if (argc >= 3 &&
        (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        out->help_requested = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (argc < 3) {
        kv_arg_error(err,
                     "usage: yvex kv report --model FILE_OR_ALIAS or "
                     "yvex kv --layers N --heads N --head-dim N --capacity N");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(argv[2], "report") == 0) {
        return kv_parse_report_args(argc, argv, out, err);
    }
    return kv_parse_ownership_args(argc, argv, out, err);
}
