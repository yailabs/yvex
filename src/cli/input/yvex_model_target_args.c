/*
 * yvex_model_target_args.c - model-target argv parsing.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   model-target subcommand and output-mode parsing into typed CLI args.
 *
 * Does not own:
 *   target catalogs, source/native tensor inspection, report building, sidecar
 *   writing, rendering, stdout/stderr writing, runtime execution, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   parsing borrows argv and performs no filesystem/model/report work.
 *
 * Boundary:
 *   model-target input parsing does not create model support, quantization,
 *   artifact, runtime, generation, benchmark, or release capability.
 */
#include "yvex_model_target_args.h"

#include <string.h>

static yvex_model_target_command_kind model_target_kind_from_text(const char *text)
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

static yvex_model_target_output_mode model_target_mode_from_argv(int argc,
                                                                 char **argv)
{
    int i;

    for (i = 0; i < argc; ++i) {
        if (argv[i] && strcmp(argv[i], "--audit") == 0) {
            return YVEX_MODEL_TARGET_OUTPUT_AUDIT;
        }
        if (argv[i] && strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "table") == 0) {
                return YVEX_MODEL_TARGET_OUTPUT_TABLE;
            }
            if (strcmp(argv[i + 1], "audit") == 0) {
                return YVEX_MODEL_TARGET_OUTPUT_AUDIT;
            }
            if (strcmp(argv[i + 1], "json") == 0) {
                return YVEX_MODEL_TARGET_OUTPUT_JSON;
            }
            return YVEX_MODEL_TARGET_OUTPUT_NORMAL;
        }
    }
    return YVEX_MODEL_TARGET_OUTPUT_NORMAL;
}

/*
 * yvex_model_target_args_parse()
 *
 * Purpose:
 *   parse model-target argv into a typed borrowed request.
 *
 * Inputs:
 *   argc/argv are borrowed CLI argument storage.
 *
 * Effects:
 *   fills out with subcommand kind, output mode hint, and original argv; it
 *   performs no filesystem, model, source, report, or render work.
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
    if (!argv || !out || argc <= 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_args",
                       "argc, argv, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->request.argc = argc;
    out->request.argv = argv;
    out->request.mode = model_target_mode_from_argv(argc, argv);
    out->request.kind = argc > 2
                            ? model_target_kind_from_text(argv[2])
                            : YVEX_MODEL_TARGET_COMMAND_UNKNOWN;
    out->help_requested = out->request.kind == YVEX_MODEL_TARGET_COMMAND_HELP;
    yvex_error_clear(err);
    return YVEX_OK;
}
