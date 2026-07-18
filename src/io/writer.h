/*
 * Owner: io.writer (io).
 * Owns: the private-interface boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: private-interface; moving this contract requires an ownership-manifest change.
 *
 * writer.h - small file JSON writer helpers.
 */
#ifndef YVEX_JSON_WRITER_H
#define YVEX_JSON_WRITER_H

#include <stdio.h>

void yvex_file_json_write_string(FILE *fp, const char *s);
void yvex_file_json_write_field(FILE *fp,
                                const char *indent,
                                const char *name,
                                const char *value,
                                int comma);

#endif
