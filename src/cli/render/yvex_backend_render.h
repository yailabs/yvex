/*
 * yvex_backend_render.h - typed backend CLI rendering.
 *
 * Owner: src/cli/render.
 * Owns: backend and cuda-info normal output formatting.
 * Does not own: capability policy, backend opens, kernel execution, or claims.
 * Invariants: rendering consumes typed facts and writes only through CLI IO.
 * Boundary: rendering supported variants is not runtime support.
 */
#ifndef YVEX_BACKEND_RENDER_H
#define YVEX_BACKEND_RENDER_H

#include <stdio.h>

#include "yvex_backend_report.h"

int yvex_backend_render(FILE *fp, const yvex_backend_report *report);
int yvex_backend_render_help(FILE *fp);
int yvex_cuda_info_render_help(FILE *fp);

#endif
