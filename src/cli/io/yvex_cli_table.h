/*
 * yvex_cli_table.h - CLI table writer ownership marker.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   shared table writer entry points for migrated CLI renderers.
 *
 * Does not own:
 *   domain facts, command parsing, runtime behavior, generation, eval, or
 *   benchmark evidence.
 *
 * Invariants:
 *   table helpers serialize caller-provided rows only.
 *
 * Boundary:
 *   table formatting is not capability support.
 */
#ifndef YVEX_CLI_TABLE_H
#define YVEX_CLI_TABLE_H

#include <stdio.h>

void yvex_cli_table_row(FILE *fp, const char *row);

#endif
