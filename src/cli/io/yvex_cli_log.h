/*
 * yvex_cli_log.h - CLI log writer ownership marker.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   shared log text writer entry points for CLI-owned surfaces.
 *
 * Does not own:
 *   metrics, traces, profile documents, runtime behavior, generation, eval, or
 *   benchmark evidence.
 *
 * Invariants:
 *   log helpers write only caller-provided text.
 *
 * Boundary:
 *   log serialization is not observability support by itself.
 */
#ifndef YVEX_CLI_LOG_H
#define YVEX_CLI_LOG_H

#include <stdio.h>

void yvex_cli_log_line(FILE *fp, const char *line);

#endif
