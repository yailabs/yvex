/*
 * yvex_source_args.c - source command argument parser.
 *
 * Owner: src/cli/input.
 * Owns: argv parsing and option validation for source-manifest report.
 * Does not own: source report building, rendering, local scanning, runtime, generation, eval, or benchmark.
 * Invariants: parsing produces a typed request and performs no domain IO.
 * Boundary: CLI parsing is not source verification or runtime readiness.
 */
#include "yvex_source_args.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int source_output_mode_parse(const char *value,
                                    yvex_source_render_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_SOURCE_RENDER_NORMAL;
        return 1;
    }
    if (strcmp(value, "table") == 0) {
        *mode = YVEX_SOURCE_RENDER_TABLE;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_SOURCE_RENDER_AUDIT;
        return 1;
    }
    if (strcmp(value, "json") == 0) {
        *mode = YVEX_SOURCE_RENDER_JSON;
        return 1;
    }
    return 0;
}

static int source_parse_positive_ull(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out || text[0] == '\0' || text[0] == '-') {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

static const char *source_path_basename(const char *path)
{
    const char *slash;

    if (!path || !path[0]) return NULL;
    slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static int source_target_matches_family_name(const char *family,
                                             const char *target)
{
    if (!family || !target) return 0;
    if (strcmp(family, "qwen") == 0) {
        return strncmp(target, "qwen", 4) == 0;
    }
    if (strcmp(family, "gemma") == 0) {
        return strncmp(target, "gemma", 5) == 0;
    }
    return 0;
}

int yvex_source_args_parse(int argc,
                           char **argv,
                           yvex_source_args *out,
                           yvex_error *err)
{
    int i;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: output args are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->render_mode = YVEX_SOURCE_RENDER_NORMAL;

    if (argc == 4 && argv && (strcmp(argv[3], "--help") == 0 || strcmp(argv[3], "-h") == 0)) {
        out->help = 1;
        return YVEX_OK;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            out->help = 1;
            return YVEX_OK;
        } else if (strcmp(argv[i], "--family") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --family requires deepseek|qwen|gemma");
                return YVEX_ERR_INVALID_ARG;
            }
            out->family = argv[++i];
        } else if (strcmp(argv[i], "--release") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --release requires VERSION");
                return YVEX_ERR_INVALID_ARG;
            }
            out->release = argv[++i];
        } else if (strcmp(argv[i], "--models-root") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --models-root requires DIR");
                return YVEX_ERR_INVALID_ARG;
            }
            out->models_root = argv[++i];
        } else if (strcmp(argv[i], "--source") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --source requires DIR");
                return YVEX_ERR_INVALID_ARG;
            }
            out->source = argv[++i];
        } else if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --target requires TARGET");
                return YVEX_ERR_INVALID_ARG;
            }
            out->target = argv[++i];
        } else if (strcmp(argv[i], "--include-files") == 0) {
            out->include_files = 1;
        } else if (strcmp(argv[i], "--include-config") == 0) {
            out->include_config = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            out->include_blockers = 1;
        } else if (strcmp(argv[i], "--include-next") == 0) {
            out->include_next = 1;
        } else if (strcmp(argv[i], "--include-tensors") == 0) {
            out->include_tensors = 1;
        } else if (strcmp(argv[i], "--strict") == 0) {
            out->strict = 1;
        } else if (strcmp(argv[i], "--tensor-limit") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --tensor-limit requires N");
                return YVEX_ERR_INVALID_ARG;
            }
            if (!source_parse_positive_ull(argv[++i], &out->tensor_limit)) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --tensor-limit requires a positive integer");
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(argv[i], "--audit") == 0) {
            out->render_mode = YVEX_SOURCE_RENDER_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --output requires normal|table|audit|json");
                return YVEX_ERR_INVALID_ARG;
            }
            if (!source_output_mode_parse(argv[++i], &out->render_mode)) {
                yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: unsupported output mode: %s", argv[i]);
                return YVEX_ERR_INVALID_ARG;
            }
        } else if (strcmp(argv[i], "--json") == 0) {
            out->render_mode = YVEX_SOURCE_RENDER_JSON;
        } else {
            yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: unknown option: %s", argv[i]);
            return YVEX_ERR_INVALID_ARG;
        }
    }

    if (!out->family || out->family[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --family is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_source_report_find_profile(out->family)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: unsupported family: %s", out->family);
        return YVEX_ERR_INVALID_ARG;
    }
    if (!out->release || out->release[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: --release is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(out->release, "v0.1.0") != 0) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "source_args_parse", "source-manifest report: unsupported release: %s", out->release);
        return YVEX_ERR_INVALID_ARG;
    }
    if (out->strict && strcmp(out->family, "deepseek") != 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_args_parse",
                       "source-manifest report: --strict is available only for the canonical DeepSeek target");
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

void yvex_source_report_request_from_parsed(yvex_source_report_request *request,
                                            const yvex_source_args *args)
{
    const char *base;

    if (!request || !args) return;
    memset(request, 0, sizeof(*request));
    request->family = args->family;
    request->release = args->release;
    request->models_root = args->models_root;
    request->source = args->source;
    request->target = args->target;
    request->include_files = args->include_files;
    request->include_config = args->include_config;
    request->include_blockers = args->include_blockers;
    request->include_next = args->include_next;
    request->include_tensors = args->include_tensors;
    request->strict = args->strict;
    request->tensor_limit = args->tensor_limit;
    request->profile = yvex_source_report_find_profile(args->family);
    if (!request->target && request->source) {
        base = source_path_basename(request->source);
        if (base && request->profile &&
            source_target_matches_family_name(request->profile->family_key, base)) {
            snprintf(request->resolved_target, sizeof(request->resolved_target), "%s", base);
            request->target = request->resolved_target;
        }
    }
    if (!request->target && request->profile) {
        request->target = request->profile->target_id;
    } else if (!request->resolved_target[0] && request->target) {
        snprintf(request->resolved_target, sizeof(request->resolved_target), "%s", request->target);
    }
}
