/*
 * yvex_model_artifacts_cli.c - model-artifacts command adapter.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   public command symbols for models/fullmodel/attention/context/moe and
 *   tensor-collection dispatch.
 *
 * Does not own:
 *   model registry facts, model reference construction, gate execution,
 *   artifact inspection, source/native-weight inspection, output formatting,
 *   JSON/table formatting, direct stdout/stderr writing, artifact emission,
 *   runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   this file stays a thin adapter over typed input/report/render contracts and
 *   the transitional CLI surface implementation.
 *
 * Boundary:
 *   command dispatch does not imply quantization, artifact emission, runtime
 *   generation, benchmark evidence, or release readiness.
 */
#include "yvex_model_artifacts_args.h"
#include "yvex_model_artifacts_render.h"
#include "yvex_model_artifacts_surface.h"
#include "yvex_model_artifact_report.h"

#include <stdio.h>

typedef int (*model_artifacts_surface_command_fn)(int arg_count, char **args);
typedef void (*model_artifacts_surface_help_fn)(FILE *fp);

/*
 * Contract: keep the adapter compiled against the typed cell interfaces while
 * historical surfaces are decomposed. Allocates nothing, mutates no domain
 * state, performs no IO, and does not call the compatibility surface.
 */
static void model_artifacts_adapter_contract(void)
{
    (void)yvex_model_artifacts_args_parse;
    (void)yvex_model_artifact_report_build;
    (void)yvex_model_artifacts_render;
}

/*
 * Contract: dispatch to an existing CLI surface without inspecting artifacts,
 * opening backends, formatting output, or owning command facts. The callee owns
 * its current transitional behavior and exit-code semantics.
 */
static int model_artifacts_dispatch(int arg_count,
                                    char **args,
                                    model_artifacts_surface_command_fn run)
{
    model_artifacts_adapter_contract();
    return run(arg_count, args);
}

/*
 * Contract: forward help rendering to the transitional surface while renderer
 * ownership is completed. Allocates no memory and writes only through the
 * callee's existing CLI IO path.
 */
static void model_artifacts_help(FILE *fp, model_artifacts_surface_help_fn help)
{
    model_artifacts_adapter_contract();
    help(fp);
}

int yvex_models_command(int arg_count, char **args)
{
    return model_artifacts_dispatch(arg_count,
                                    args,
                                    yvex_model_artifacts_surface_models_command);
}

void yvex_models_help(FILE *fp)
{
    model_artifacts_help(fp, yvex_model_artifacts_surface_models_help);
}

int yvex_fullmodel_command(int arg_count, char **args)
{
    return model_artifacts_dispatch(arg_count,
                                    args,
                                    yvex_model_artifacts_surface_fullmodel_command);
}

void yvex_fullmodel_help(FILE *fp)
{
    model_artifacts_help(fp, yvex_model_artifacts_surface_fullmodel_help);
}

int yvex_attention_command(int arg_count, char **args)
{
    return model_artifacts_dispatch(arg_count,
                                    args,
                                    yvex_model_artifacts_surface_attention_command);
}

void yvex_attention_help(FILE *fp)
{
    model_artifacts_help(fp, yvex_model_artifacts_surface_attention_help);
}

int yvex_context_command(int arg_count, char **args)
{
    return model_artifacts_dispatch(arg_count,
                                    args,
                                    yvex_model_artifacts_surface_context_command);
}

void yvex_context_help(FILE *fp)
{
    model_artifacts_help(fp, yvex_model_artifacts_surface_context_help);
}

int yvex_moe_command(int arg_count, char **args)
{
    return model_artifacts_dispatch(arg_count,
                                    args,
                                    yvex_model_artifacts_surface_moe_command);
}

void yvex_moe_help(FILE *fp)
{
    model_artifacts_help(fp, yvex_model_artifacts_surface_moe_help);
}

int yvex_tensor_collection_command(int arg_count, char **args)
{
    return model_artifacts_dispatch(
        arg_count,
        args,
        yvex_model_artifacts_surface_tensor_collection_command);
}

void yvex_tensor_collection_help(FILE *fp)
{
    model_artifacts_help(fp, yvex_model_artifacts_surface_tensor_collection_help);
}
