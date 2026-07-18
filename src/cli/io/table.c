/*
 * table.c - CLI table writer implementation.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   approved direct table-row output for CLI renderers.
 *
 * Does not own:
 *   table semantics, domain facts, runtime behavior, generation, eval, or
 *   benchmark evidence.
 *
 * Invariants:
 *   table helpers print only caller-provided rows.
 *
 * Boundary:
 *   table output is not feature support.
 */
#include "table.h"

#include "out.h"

void yvex_cli_table_row(FILE *fp, const char *row)
{
    yvex_cli_out_line(fp, row);
}
