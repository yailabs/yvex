/*
 * yvex_model_target_args.c - model-target argv parsing.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   model-target subcommand and option parsing into typed CLI args.
 *
 * Does not own:
 *   target catalogs, source/native tensor inspection, report building, sidecar
 *   writing, rendering, stdout/stderr writing, runtime execution, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   parsing copies scalar option values into the request and performs no
 *   filesystem/model/report work.
 *
 * Boundary:
 *   model-target input parsing does not create model support, quantization,
 *   artifact, runtime, generation, benchmark, or release capability.
 */
#include "yvex_model_target_args.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int mt_starts_option(const char *s)
{
    return s && s[0] == '-';
}

static void mt_copy(char *out, size_t cap, const char *value)
{
    if (!out || cap == 0u) {
        return;
    }
    if (!value) {
        value = "";
    }
    (void)snprintf(out, cap, "%s", value);
}

static void mt_parse_error(yvex_model_target_args *out, const char *fmt, ...)
{
    va_list ap;

    if (!out || !fmt) {
        return;
    }
    out->parse_failed = 1;
    va_start(ap, fmt);
    (void)vsnprintf(out->error_message, sizeof(out->error_message), fmt, ap);
    va_end(ap);
}

static yvex_model_target_command_kind mt_kind_from_text(const char *text)
{
    if (!text) return YVEX_MODEL_TARGET_COMMAND_UNKNOWN;
    if (strcmp(text, "help") == 0 || strcmp(text, "--help") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_HELP;
    }
    if (strcmp(text, "classes") == 0) return YVEX_MODEL_TARGET_COMMAND_CLASSES;
    if (strcmp(text, "list") == 0) return YVEX_MODEL_TARGET_COMMAND_LIST;
    if (strcmp(text, "decision") == 0) return YVEX_MODEL_TARGET_COMMAND_DECISION;
    if (strcmp(text, "candidate") == 0) return YVEX_MODEL_TARGET_COMMAND_CANDIDATE;
    if (strcmp(text, "dense-candidate") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE;
    }
    if (strcmp(text, "qwen-metal") == 0) return YVEX_MODEL_TARGET_COMMAND_QWEN_METAL;
    if (strcmp(text, "class-profile") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE;
    }
    if (strcmp(text, "tensor-collection") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION;
    }
    if (strcmp(text, "tensor-map") == 0) return YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    if (strcmp(text, "tokenizer-map") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP;
    }
    if (strcmp(text, "missing-roles") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES;
    }
    if (strcmp(text, "quant-policy") == 0) {
        return YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY;
    }
    if (strcmp(text, "inspect") == 0) return YVEX_MODEL_TARGET_COMMAND_INSPECT;
    return YVEX_MODEL_TARGET_COMMAND_UNKNOWN;
}

static const char *mt_action_name(yvex_model_target_command_kind kind)
{
    switch (kind) {
    case YVEX_MODEL_TARGET_COMMAND_CANDIDATE: return "candidate";
    case YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE: return "dense-candidate";
    case YVEX_MODEL_TARGET_COMMAND_QWEN_METAL: return "qwen-metal";
    case YVEX_MODEL_TARGET_COMMAND_DECISION: return "decision";
    case YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE: return "class-profile";
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION: return "tensor-collection";
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP: return "tensor-map";
    case YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP: return "tokenizer-map";
    case YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES: return "missing-roles";
    case YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY: return "quant-policy";
    case YVEX_MODEL_TARGET_COMMAND_LIST: return "list";
    case YVEX_MODEL_TARGET_COMMAND_INSPECT: return "inspect";
    case YVEX_MODEL_TARGET_COMMAND_CLASSES: return "classes";
    default: return "model-target";
    }
}

static int mt_mode_parse(const char *value, yvex_model_target_render_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "table") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_TABLE;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
        return 1;
    }
    if (strcmp(value, "json") == 0) {
        *mode = YVEX_MODEL_TARGET_OUTPUT_JSON;
        return 1;
    }
    return 0;
}

static int mt_requires_release(yvex_model_target_command_kind kind)
{
    return kind == YVEX_MODEL_TARGET_COMMAND_CANDIDATE ||
           kind == YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE ||
           kind == YVEX_MODEL_TARGET_COMMAND_QWEN_METAL ||
           kind == YVEX_MODEL_TARGET_COMMAND_DECISION;
}

static int mt_positional_target_kind(yvex_model_target_command_kind kind)
{
    return kind == YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE ||
           kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION ||
           kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP ||
           kind == YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP ||
           kind == YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES ||
           kind == YVEX_MODEL_TARGET_COMMAND_INSPECT;
}

/*
 * yvex_model_target_args_parse()
 *
 * Purpose:
 *   parse model-target CLI words into a typed model-target request.
 *
 * Inputs:
 *   argc/argv are borrowed CLI words; out receives copied scalar fields.
 *
 * Effects:
 *   fills request fields and parser diagnostics only; it performs no
 *   filesystem, model, source, report, or render work.
 *
 * Failure:
 *   returns invalid-arg only when the parser contract itself is invalid.
 *
 * Boundary:
 *   parsing does not build target facts, write sidecars, render output, execute
 *   runtime paths, generate, evaluate, benchmark, or mark release readiness.
 */
int yvex_model_target_args_parse(int argc,
                                 char **argv,
                                 yvex_model_target_args *out,
                                 yvex_error *err)
{
    yvex_model_target_command_kind kind;
    const char *action;
    int i;

    if (!argv || !out || argc <= 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_args",
                       "arguments and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->request.mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
    kind = argc > 2 ? mt_kind_from_text(argv[2]) : YVEX_MODEL_TARGET_COMMAND_UNKNOWN;
    out->request.kind = kind;
    action = mt_action_name(kind);
    out->help_requested = kind == YVEX_MODEL_TARGET_COMMAND_HELP;
    out->request.help_requested = out->help_requested;

    if (kind == YVEX_MODEL_TARGET_COMMAND_UNKNOWN) {
        if (argc > 2 && argv[2] && argv[2][0]) {
            mt_parse_error(out, "model-target: unknown subcommand: %s", argv[2]);
        }
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (mt_positional_target_kind(kind)) {
        if (argc > 3 && !mt_starts_option(argv[3])) {
            mt_copy(out->request.target_id, sizeof(out->request.target_id), argv[3]);
            i = 4;
        } else {
            i = 3;
        }
    } else if (kind == YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY &&
               argc > 3 && !mt_starts_option(argv[3])) {
        mt_copy(out->request.target_id, sizeof(out->request.target_id), argv[3]);
        i = 4;
    } else {
        i = 3;
    }

    for (; i < argc && !out->parse_failed; ++i) {
        const char *word = argv[i];

        if (!word) continue;
        if (strcmp(word, "--help") == 0) {
            out->help_requested = 1;
            out->request.help_requested = 1;
        } else if (strcmp(word, "--audit") == 0) {
            out->request.mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
        } else if (strcmp(word, "--json") == 0) {
            out->request.mode = YVEX_MODEL_TARGET_OUTPUT_JSON;
            out->request.output_json = 1;
        } else if (strcmp(word, "--output") == 0) {
            if (i + 1 >= argc) {
                mt_parse_error(out, "model-target %s: --output requires normal|table|audit", action);
            } else if (!mt_mode_parse(argv[++i], &out->request.mode)) {
                mt_parse_error(out, "model-target %s: unsupported output mode: %s", action, argv[i]);
            } else if (out->request.mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
                out->request.output_json = 1;
            }
        } else if (strcmp(word, "--release") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target %s: --release requires VERSION", action);
            } else {
                mt_copy(out->request.release, sizeof(out->request.release), argv[++i]);
            }
        } else if (strcmp(word, "--target") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target %s: --target requires TARGET", action);
            } else {
                mt_copy(out->request.target_id, sizeof(out->request.target_id), argv[++i]);
            }
        } else if (strcmp(word, "--candidate") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target decision: --candidate requires TARGET");
            } else {
                mt_copy(out->request.candidate_kind,
                        sizeof(out->request.candidate_kind),
                        argv[++i]);
            }
        } else if (strcmp(word, "--models-root") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target %s: models-root requires DIR", action);
            } else {
                mt_copy(out->request.models_root, sizeof(out->request.models_root), argv[++i]);
            }
        } else if (strcmp(word, "--source") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target %s: source requires DIR", action);
            } else {
                mt_copy(out->request.source_path, sizeof(out->request.source_path), argv[++i]);
            }
        } else if (strcmp(word, "--role") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target %s: role requires output-head|tokenizer|missing-roles", action);
            } else if (strcmp(argv[i + 1], "output-head") != 0 &&
                       strcmp(argv[i + 1], "tokenizer") != 0 &&
                       strcmp(argv[i + 1], "missing-roles") != 0) {
                mt_parse_error(out, "model-target %s: unsupported role: %s",
                               action, argv[i + 1]);
                i++;
            } else {
                mt_copy(out->request.role, sizeof(out->request.role), argv[++i]);
            }
        } else if (strcmp(word, "--gate") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_parse_error(out, "model-target %s: gate requires v0.1.0", action);
            } else {
                mt_copy(out->request.gate, sizeof(out->request.gate), argv[++i]);
            }
        } else if (strcmp(word, "--check-output-contract") == 0) {
            if (i + 1 >= argc || !argv[i + 1][0]) {
                mt_copy(out->request.output_contract,
                        sizeof(out->request.output_contract),
                        "missing");
            } else {
                mt_copy(out->request.output_contract,
                        sizeof(out->request.output_contract),
                        argv[++i]);
            }
        } else if (strcmp(word, "--include-candidates") == 0) {
            out->request.include_candidates = 1;
        } else if (strcmp(word, "--include-pressure-targets") == 0) {
            out->request.include_pressure_targets = 1;
        } else if (strcmp(word, "--include-blockers") == 0) {
            out->request.include_blockers = 1;
        } else if (strcmp(word, "--include-next") == 0) {
            out->request.include_next = 1;
        } else if (strcmp(word, "--include-critical-path") == 0) {
            out->request.include_critical_path = 1;
        } else if (strcmp(word, "--include-requirements") == 0) {
            out->request.include_requirements = 1;
        } else if (strcmp(word, "--include-hardware") == 0) {
            out->request.include_hardware = 1;
        } else if (strcmp(word, "--include-backend") == 0) {
            out->request.include_backend = 1;
        } else if (strcmp(word, "--include-source") == 0) {
            out->request.include_source = 1;
        } else if (strcmp(word, "--include-examples") == 0) {
            out->request.include_examples = 1;
        } else if (strcmp(word, "--paths") == 0) {
            out->request.include_paths = 1;
        } else if (strcmp(word, "--strict") == 0) {
            out->request.strict = 1;
        } else if (strcmp(word, "--role-support") == 0 ||
                   strcmp(word, "--roles") == 0) {
            out->request.include_requirements = 1;
        } else {
            if (kind == YVEX_MODEL_TARGET_COMMAND_INSPECT) {
                mt_parse_error(out, "model-target: unknown inspect option: %s", word);
            } else {
                mt_parse_error(out, "model-target %s: unknown option: %s", action, word);
            }
        }
    }

    if (!out->parse_failed && mt_requires_release(kind) &&
        !out->request.help_requested && !out->request.release[0]) {
        mt_parse_error(out, "model-target %s: --release is required", action);
    }
    if (!out->parse_failed &&
        kind == YVEX_MODEL_TARGET_COMMAND_INSPECT &&
        out->request.models_root[0] &&
        !out->request.include_paths) {
        mt_parse_error(out, "model-target inspect: --models-root requires --paths");
    }
    if (!out->parse_failed &&
        kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP &&
        out->request.gate[0] &&
        out->request.role[0]) {
        mt_parse_error(out, "model-target tensor-map: gate cannot be combined with --role");
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
