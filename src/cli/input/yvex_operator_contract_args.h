/*
 * yvex_operator_contract_args.h - typed Operator contract command input.
 *
 * Owner: src/cli/input.
 * Owns: the parsed help/JSON selection for `yvex operator-contract`.
 * Does not own: product identity, protocol facts, rendering, or process IO.
 * Invariants: parsing borrows argv and has no allocation or side effects.
 * Boundary: accepting the plumbing grammar does not prove native capability.
 */
#ifndef YVEX_OPERATOR_CONTRACT_ARGS_H
#define YVEX_OPERATOR_CONTRACT_ARGS_H

#include <yvex/error.h>

typedef struct {
    int help;
} yvex_operator_contract_args;

int yvex_operator_contract_args_parse(
    int argc,
    char **argv,
    yvex_operator_contract_args *out,
    yvex_error *err);

#endif
