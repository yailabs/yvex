/*
 * yvex_operator_contract_cli.c - Operator identity command adapter.
 *
 * Owner: src/cli/commands.
 * Owns: parse/report/render dispatch and stable CLI exit classification.
 * Does not own: product/version facts, JSON formatting, argv grammar, process
 * execution, capability discovery, backend state, or runtime state.
 * Invariants: one accepted request builds one typed immutable report and calls
 * one typed renderer; no browser- or environment-controlled command is run.
 * Boundary: a successful handshake admits Operator transport only.
 */
#include "yvex_operator_contract_args.h"
#include "yvex_operator_contract_render.h"

#include "yvex_cli_out.h"

/* Contract: maps the bounded identity command statuses to stable CLI exits. */
static int operator_contract_exit_for_status(yvex_status status)
{
    return status == YVEX_OK ? 0 : status == YVEX_ERR_INVALID_ARG ? 2 : 1;
}

/* Contract: emits one typed parser/report failure through CLI-owned IO. */
static int operator_contract_error(const yvex_error *err, yvex_status status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err), yvex_error_message(err));
    return operator_contract_exit_for_status(status);
}

/*
 * Contract: parses borrowed argv, builds immutable core identity, renders JSON,
 * and returns a stable exit without allocation or non-output IO.
 */
int yvex_operator_contract_command(int argc, char **argv)
{
    yvex_operator_contract_args args;
    yvex_operator_contract_report report;
    yvex_error err;
    yvex_status status;

    yvex_error_clear(&err);
    status = (yvex_status)yvex_operator_contract_args_parse(
        argc, argv, &args, &err);
    if (status != YVEX_OK) return operator_contract_error(&err, status);
    if (args.help) {
        return operator_contract_exit_for_status(
            (yvex_status)yvex_operator_contract_render_help(
                yvex_cli_out_stdout()));
    }
    status = yvex_operator_contract_report_build(&report);
    if (status != YVEX_OK) {
        yvex_error_set(&err, status, "operator-contract",
                       "identity report construction failed");
        return operator_contract_error(&err, status);
    }
    return operator_contract_exit_for_status(
        (yvex_status)yvex_operator_contract_render(
            yvex_cli_out_stdout(), &report));
}

/* Contract: routes top-level help to the typed renderer without domain IO. */
void yvex_operator_contract_help(FILE *fp)
{
    (void)yvex_operator_contract_render_help(fp);
}
