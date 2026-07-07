/*
 * yvex_source_render.h - typed source report renderer.
 */
#ifndef YVEX_SOURCE_RENDER_H
#define YVEX_SOURCE_RENDER_H

#include <stdio.h>
#include "yvex_source_report.h"

typedef enum {
    YVEX_SOURCE_RENDER_NORMAL = 0,
    YVEX_SOURCE_RENDER_TABLE,
    YVEX_SOURCE_RENDER_AUDIT
} yvex_source_render_mode;

int yvex_source_render(FILE *fp,
                       yvex_source_render_mode mode,
                       const yvex_source_report *report);
int yvex_source_render_normal(FILE *fp, const yvex_source_report *report);
int yvex_source_render_table(FILE *fp, const yvex_source_report *report);
int yvex_source_render_audit(FILE *fp, const yvex_source_report *report);
void yvex_source_render_help(FILE *fp);

#endif
