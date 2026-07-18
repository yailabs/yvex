/*
 * json.h - CLI JSON writer foundation.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   small plumbing JSON writer helpers for migrated CLI surfaces.
 *
 * Does not own:
 *   uniform JSON support, schemas, domain facts, command parsing, runtime
 *   behavior, generation, eval, or benchmark evidence.
 *
 * Invariants:
 *   JSON support is opt-in per command and must not imply uniform plumbing.
 *
 * Boundary:
 *   writer availability is not JSON support for every command.
 */
#ifndef YVEX_CLI_JSON_H
#define YVEX_CLI_JSON_H

#include <stdio.h>

void yvex_cli_json_begin(FILE *fp);
void yvex_cli_json_end(FILE *fp);
void yvex_cli_json_field_str(FILE *fp, const char *key, const char *value, int comma);
void yvex_cli_json_field_u64(FILE *fp, const char *key, unsigned long long value, int comma);
void yvex_cli_json_field_bool(FILE *fp, const char *key, int value, int comma);

#endif
