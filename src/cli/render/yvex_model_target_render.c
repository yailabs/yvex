/*
 * model_target CLI renderer.
 *
 * Owner: src/cli/render.
 * Owns: render functions for CLI-owned model_target output shapes.
 * Does not own: domain state, command parsing, or capability claims.
 * Boundary: renderer-only; no runtime behavior.
 */

#include <stdio.h>

void yvex_model_target_render_normal(FILE *fp, const void *report)
{
    if (!fp) {
        return;
    }
    fprintf(fp, "model_target-render: normal\n");
    fprintf(fp, "report: %s\n", report ? "present" : "not-bound");
    fprintf(fp, "boundary: renderer-only; no domain behavior or capability claim\n");
}

void yvex_model_target_render_table(FILE *fp, const void *report)
{
    if (!fp) {
        return;
    }
    fprintf(fp, "model_target-render: table\n");
    fprintf(fp, "report: %s\n", report ? "present" : "not-bound");
    fprintf(fp, "boundary: renderer-only; no domain behavior or capability claim\n");
}

void yvex_model_target_render_audit(FILE *fp, const void *report)
{
    if (!fp) {
        return;
    }
    fprintf(fp, "model_target-render: audit\n");
    fprintf(fp, "report: %s\n", report ? "present" : "not-bound");
    fprintf(fp, "boundary: renderer-only; no domain behavior or capability claim\n");
}
