/*
 * backend.c - backend capability command adapter.
 *
 * Owner: src/cli/commands.
 * Owns: typed backend/cuda-info parse, report, render, and exit-code dispatch.
 * Does not own: backend probing, capability policy, CUDA admission, rendering,
 * kernel execution, graph execution, or runtime support.
 * Invariants: command dispatch never derives capability from context status.
 * Boundary: capability inspection is bounded backend evidence, not runtime.
 */
#include "src/cli/input/backend.h"
#include "src/cli/render/backend.h"
#include "src/cli/io/out.h"

/* Contract: maps domain failures to stable operator exit classes. */
static int backend_cli_exit_for_status(int status)
{
    if (status == YVEX_ERR_INVALID_ARG) return 2;
    if (status == YVEX_ERR_FORMAT || status == YVEX_ERR_BOUNDS) return 4;
    if (status == YVEX_ERR_UNSUPPORTED) return 5;
    if (status == YVEX_ERR_IO || status == YVEX_ERR_NOMEM) return 3;
    return status == YVEX_OK ? 0 : 1;
}

/* Contract: renders one parser/domain error through CLI-owned IO only. */
static int backend_cli_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err), yvex_error_message(err));
    return backend_cli_exit_for_status(status);
}

/* Contract: performs the shared backend report dispatch without domain logic. */
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

void yvex_backend_help(FILE *fp)
{
    (void)yvex_backend_render_help(fp);
}

void yvex_cuda_info_help(FILE *fp)
{
    (void)yvex_cuda_info_render_help(fp);
}
