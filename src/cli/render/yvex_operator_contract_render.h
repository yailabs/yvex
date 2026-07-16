/*
 * yvex_operator_contract_render.h - typed Operator identity rendering.
 *
 * Owner: src/cli/render.
 * Owns: JSON and help formatting from an immutable protocol identity report.
 * Does not own: identity construction, argv parsing, capability discovery, or
 * direct stdout/stderr implementation.
 * Invariants: rendering consumes typed facts and emits no capability claims.
 * Boundary: JSON identity compatibility is not backend or runtime readiness.
 */
#ifndef YVEX_OPERATOR_CONTRACT_RENDER_H
#define YVEX_OPERATOR_CONTRACT_RENDER_H

#include <stdio.h>

#include <yvex/version.h>

int yvex_operator_contract_render(
    FILE *fp,
    const yvex_operator_contract_report *report);
int yvex_operator_contract_render_help(FILE *fp);

#endif
