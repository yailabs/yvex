/* Owner: src/cli/commands.
 * Owns: typed backend/cuda-info parse, report, render, and exit-code dispatch.
 * Does not own: backend probing, capability policy, CUDA admission, rendering, kernel execution, graph execution,
 *   or runtime support.
 * Invariants: command dispatch never derives capability from context status.
 * Boundary: capability inspection is bounded backend evidence, not runtime.
 * Purpose: provide typed backend/cuda-info parse, report, render, and exit-code dispatch.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"
#include "src/cli/io/private.h"

/* Contract: maps domain failures to stable operator exit classes. */
/* Purpose: Compute backend exit for status for its CLI invariant (`backend_cli_exit_for_status`). */
static int backend_cli_exit_for_status(int status)
{
    if (status == YVEX_ERR_INVALID_ARG) return 2;
    if (status == YVEX_ERR_FORMAT || status == YVEX_ERR_BOUNDS) return 4;
    if (status == YVEX_ERR_UNSUPPORTED) return 5;
    if (status == YVEX_ERR_IO || status == YVEX_ERR_NOMEM) return 3;
    return status == YVEX_OK ? 0 : 1;
}

/* Contract: renders one parser/domain error through CLI-owned IO only. */
/* Purpose: Compute backend error for its CLI invariant (`backend_cli_error`). */
static int backend_cli_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err), yvex_error_message(err));
    return backend_cli_exit_for_status(status);
}

/* Contract: performs the shared backend report dispatch without domain logic. */
/* Purpose: Compute backend run for its CLI invariant (`backend_cli_run`). */
static int backend_cli_run(const yvex_backend_args *args)
{
    yvex_backend_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_backend_report_build(&args->request, &report, &err);
    if (rc != YVEX_OK) {
        return backend_cli_error(&err, rc);
    }
    (void)yvex_backend_render(yvex_cli_out_stdout(), &report);
    return report.exit_code;
}

/* Purpose: Orchestrate the typed backend command request (`yvex_backend_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_backend_command(int argc, char **argv)
{
    yvex_backend_args args;
    yvex_error err;
    int rc = yvex_backend_args_parse(argc, argv, &args, &err);

    if (rc != YVEX_OK) return backend_cli_error(&err, rc);
    if (args.help) {
        (void)yvex_backend_render_help(yvex_cli_out_stdout());
        return 0;
    }
    return backend_cli_run(&args);
}

/* Purpose: Orchestrate the typed cuda info command request (`yvex_cuda_info_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_cuda_info_command(int argc, char **argv)
{
    yvex_backend_args args;
    yvex_error err;
    int rc = yvex_cuda_info_args_parse(argc, argv, &args, &err);

    if (rc != YVEX_OK) return backend_cli_error(&err, rc);
    if (args.help) {
        (void)yvex_cuda_info_render_help(yvex_cli_out_stdout());
        return 0;
    }
    return backend_cli_run(&args);
}

/* Purpose: Render backend help from typed facts (`yvex_backend_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_backend_help(FILE *fp)
{
    (void)yvex_backend_render_help(fp);
}

/* Purpose: Render cuda info help from typed facts (`yvex_cuda_info_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cuda_info_help(FILE *fp)
{
    (void)yvex_cuda_info_render_help(fp);
}
