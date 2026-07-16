/*
 * yvex_operator_contract_render.c - Operator identity JSON renderer.
 *
 * Owner: src/cli/render.
 * Owns: stable JSON field names/order and compact command help.
 * Does not own: report facts, parser behavior, process execution, capabilities,
 * runtime state, or backend state.
 * Invariants: all JSON values originate in the typed report and bytes are
 * emitted only through approved CLI IO helpers.
 * Boundary: the rendered handshake identifies protocol compatibility only.
 */
#include "yvex_operator_contract_render.h"

#include "yvex_cli_json.h"
#include "yvex_cli_out.h"

/*
 * Contract: serializes one complete borrowed identity report as stable JSON.
 * It allocates nothing, performs only CLI output IO, and returns
 * invalid-argument without printing when fp/report is null.
 */
int yvex_operator_contract_render(
    FILE *fp,
    const yvex_operator_contract_report *report)
{
    if (!fp || !report) return YVEX_ERR_INVALID_ARG;
    yvex_cli_json_begin(fp);
    yvex_cli_json_field_str(fp, "schemaVersion", report->schema_version, 1);
    yvex_cli_json_field_str(fp, "protocolVersion", report->protocol_version, 1);
    yvex_cli_json_field_str(fp, "yvexVersion", report->yvex_version, 1);
    yvex_cli_json_field_str(fp, "product", report->product, 0);
    yvex_cli_json_end(fp);
    return YVEX_OK;
}

/*
 * Contract: prints only the fixed plumbing grammar and its capability boundary.
 * It allocates nothing and performs no domain or process IO.
 */
int yvex_operator_contract_render_help(FILE *fp)
{
    if (!fp) return YVEX_ERR_INVALID_ARG;
    yvex_cli_out_writef(
        fp,
        "usage: yvex operator-contract --output json\n\n"
        "Prints stable product, version, schema, and Operator protocol identity.\n"
        "This handshake does not report backend, runtime, or generation support.\n");
    return YVEX_OK;
}
