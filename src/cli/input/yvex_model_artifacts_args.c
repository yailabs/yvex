/*
 * yvex_model_artifacts_args.c - model artifact CLI argument parser.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   argc/argv parsing into typed model artifact report request fields.
 *
 * Does not own:
 *   registry lookup, model gate checks, report building, rendering,
 *   stdout/stderr, artifact emission, runtime generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   parser performs no artifact IO and calls no report builders.
 *
 * Boundary:
 *   argument parsing is not artifact emission or runtime support.
 */
#include "yvex_model_artifacts_args.h"

#include <string.h>

int yvex_model_artifacts_args_parse(int argc,
                                    char **argv,
                                    yvex_model_artifacts_args *out,
                                    yvex_error *err)
{
    int i;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifacts_args",
                       "output args are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->request.kind = YVEX_MODEL_ARTIFACT_REPORT_STATUS;
    out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_NORMAL;

    for (i = 0; i < argc; ++i) {
        const char *arg = argv ? argv[i] : NULL;
        if (!arg) continue;
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "help") == 0) {
            out->help_requested = 1;
        } else if (strcmp(arg, "list") == 0) {
            out->request.kind = YVEX_MODEL_ARTIFACT_REPORT_LIST;
        } else if (strcmp(arg, "check") == 0) {
            out->request.kind = YVEX_MODEL_ARTIFACT_REPORT_CHECK;
        } else if (strcmp(arg, "--audit") == 0) {
            out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_AUDIT;
        } else if (strcmp(arg, "--output") == 0 && i + 1 < argc) {
            const char *mode = argv[++i];
            if (strcmp(mode, "table") == 0) out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_TABLE;
            else if (strcmp(mode, "audit") == 0) out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_AUDIT;
            else out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_NORMAL;
        } else if (strcmp(arg, "--registry") == 0 && i + 1 < argc) {
            out->request.registry_path = argv[++i];
        } else if (strcmp(arg, "--model") == 0 && i + 1 < argc) {
            out->request.model_ref = argv[++i];
        } else if (strcmp(arg, "--path") == 0 && i + 1 < argc) {
            out->request.artifact_path = argv[++i];
        }
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
