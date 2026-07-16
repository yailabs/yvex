/*
 * yvex_operator_contract_args.c - Operator contract argv parsing.
 *
 * Owner: src/cli/input.
 * Owns: exact help and `--output json` grammar validation.
 * Does not own: identity facts, JSON serialization, command dispatch, or IO.
 * Invariants: only the fixed JSON plumbing mode is admitted and argv is never
 * retained or mutated.
 * Boundary: grammar admission is transport compatibility, not runtime support.
 */
#include "yvex_operator_contract_args.h"

#include <string.h>

/*
 * Contract: parses exactly `yvex operator-contract --output json` or a direct
 * help request. It allocates nothing, mutates only out/err, performs no IO, and
 * returns invalid-argument for every unsupported option or output mode.
 */
int yvex_operator_contract_args_parse(
    int argc,
    char **argv,
    yvex_operator_contract_args *out,
    yvex_error *err)
{
    if (!out || !argv) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator-contract",
                       "operator contract arguments are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (argc == 3 &&
        (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        out->help = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (argc != 4) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator-contract",
                       "usage: yvex operator-contract --output json");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(argv[2], "--output") != 0) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "operator-contract",
                        "unsupported option: %s", argv[2]);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(argv[3], "json") != 0) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "operator-contract",
                        "unsupported output mode: %s", argv[3]);
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
