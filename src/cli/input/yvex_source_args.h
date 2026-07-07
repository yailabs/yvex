/*
 * yvex_source_args.h - source CLI argument parser.
 */
#ifndef YVEX_SOURCE_ARGS_H
#define YVEX_SOURCE_ARGS_H

#include <yvex/error.h>
#include "yvex_source_render.h"

typedef struct {
    const char *family;
    const char *release;
    const char *models_root;
    const char *source;
    const char *target;
    int include_files;
    int include_config;
    int include_blockers;
    int include_next;
    int include_tensors;
    unsigned long long tensor_limit;
    yvex_source_render_mode render_mode;
    int help;
} yvex_source_args;

int yvex_source_args_parse(int argc,
                           char **argv,
                           yvex_source_args *out,
                           yvex_error *err);
void yvex_source_report_request_from_parsed(yvex_source_report_request *request,
                                            const yvex_source_args *args);

#endif
