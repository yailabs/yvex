/*
 * Owner: cli.render.source (cli.render).
 * Owns: the private-interface boundary consumed by cli.commands,cli.model_artifacts.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: private-interface; moving this contract requires an ownership-manifest change.
 *
 * source.h - typed source report renderer.
 */
#ifndef YVEX_SOURCE_RENDER_H
#define YVEX_SOURCE_RENDER_H

#include <stdio.h>
#include "src/source/report.h"

typedef enum {
    YVEX_SOURCE_RENDER_NORMAL = 0,
    YVEX_SOURCE_RENDER_TABLE,
    YVEX_SOURCE_RENDER_AUDIT,
    YVEX_SOURCE_RENDER_JSON
} yvex_source_render_mode;

int yvex_source_render(FILE *fp,
                       yvex_source_render_mode mode,
                       const yvex_source_report *report);
int yvex_source_render_normal(FILE *fp, const yvex_source_report *report);
int yvex_source_render_table(FILE *fp, const yvex_source_report *report);
int yvex_source_render_audit(FILE *fp, const yvex_source_report *report);
int yvex_source_render_json(FILE *fp, const yvex_source_report *report);
void yvex_source_render_help(FILE *fp);

#endif
