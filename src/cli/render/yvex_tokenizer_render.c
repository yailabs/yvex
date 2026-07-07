/*
 * tokenizer CLI renderer.
 *
 * Owner: src/cli/render.
 * Owns: render functions for CLI-owned tokenizer output shapes.
 * Does not own: domain state, command parsing, or capability claims.
 * Boundary: renderer-only; no runtime behavior.
 */

#include <stdio.h>

void yvex_tokenizer_render_normal(FILE *fp, const void *report)
{
    if (!fp) {
        return;
    }
    fprintf(fp, "tokenizer-render: normal\n");
    fprintf(fp, "report: %s\n", report ? "present" : "not-bound");
    fprintf(fp, "boundary: renderer-only; no domain behavior or capability claim\n");
}

void yvex_tokenizer_render_table(FILE *fp, const void *report)
{
    if (!fp) {
        return;
    }
    fprintf(fp, "tokenizer-render: table\n");
    fprintf(fp, "report: %s\n", report ? "present" : "not-bound");
    fprintf(fp, "boundary: renderer-only; no domain behavior or capability claim\n");
}

void yvex_tokenizer_render_audit(FILE *fp, const void *report)
{
    if (!fp) {
        return;
    }
    fprintf(fp, "tokenizer-render: audit\n");
    fprintf(fp, "report: %s\n", report ? "present" : "not-bound");
    fprintf(fp, "boundary: renderer-only; no domain behavior or capability claim\n");
}
