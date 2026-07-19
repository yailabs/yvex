/* Owner: src/cli/io
 * Owns: approved direct table-row output for CLI renderers.
 * Does not own: table semantics, domain facts, runtime behavior, generation, eval, or benchmark evidence.
 * Invariants: table helpers print only caller-provided rows.
 * Boundary: table output is not feature support.
 * Purpose: provide approved direct table-row output for CLI renderers.
 * Inputs: typed scalar, table, or JSON fields and the selected operator stream.
 * Effects: writes only explicitly requested operator output bytes.
 * Failure: propagates stream or encoding failure without changing domain state. */
#include "src/cli/io/private.h"

/* Purpose: Compute table row for its CLI invariant (`yvex_cli_table_row`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_table_row(FILE *fp, const char *row)
{
    yvex_cli_out_line(fp, row);
}
