/*
 * YVEX - JSON writer helpers
 *
 * File: yvex_json_writer.c
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Writes small JSON string values used by metrics, trace, profile, and CLI
 *   envelopes. This helper does not allocate and does not validate UTF-8.
 */
#include "yvex_metrics_internal.h"

int yvex_json_write_string(FILE *fp, const char *text)
{
    const unsigned char *p;

    if (!fp) {
        return YVEX_ERR_INVALID_ARG;
    }

    p = (const unsigned char *)(text ? text : "");
    fputc('"', fp);
    while (*p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', fp);
            fputc((int)*p, fp);
        } else if (*p == '\n') {
            fputs("\\n", fp);
        } else if (*p == '\r') {
            fputs("\\r", fp);
        } else if (*p == '\t') {
            fputs("\\t", fp);
        } else if (*p < 32u) {
            fprintf(fp, "\\u%04x", (unsigned int)*p);
        } else {
            fputc((int)*p, fp);
        }
        ++p;
    }
    fputc('"', fp);
    return YVEX_OK;
}
