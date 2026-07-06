/*
 * yvex_cli_log.c - CLI log writer implementation.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   approved direct log-line output for CLI-owned surfaces.
 *
 * Does not own:
 *   metrics, traces, profile documents, runtime behavior, generation, eval, or
 *   benchmark evidence.
 *
 * Invariants:
 *   log helpers print only caller-provided lines.
 *
 * Boundary:
 *   log writing is not runtime support or benchmark evidence.
 */
#include "yvex_cli_log.h"

#include "yvex_cli_out.h"

void yvex_cli_log_line(FILE *fp, const char *line)
{
    yvex_cli_out_line(fp, line);
}
