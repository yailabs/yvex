/*
 * yvex_json_writer.h - small file JSON writer helpers.
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
