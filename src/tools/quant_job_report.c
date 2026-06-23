/*
 * YVEX - Quant job report helpers
 *
 * File: src/tools/quant_job_report.c
 * Layer: tool-plane implementation
 */
#include "quant_job_internal.h"

/* Reporting is currently owned by cli/yvex_cli.c; this translation unit keeps
 * the open-weight intake tool-plane layout explicit for future shared reports. */
int yvex_quant_job_report_translation_unit_anchor(void)
{
    return 0;
}
