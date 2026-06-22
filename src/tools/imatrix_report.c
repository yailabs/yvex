/*
 * YVEX - Imatrix report helpers
 *
 * File: src/tools/imatrix_report.c
 * Layer: tool-plane implementation
 */
#include "imatrix_internal.h"

/* Reporting is currently owned by cli/yvex_cli.c; this translation unit keeps
 * the OWI.6 tool-plane object layout explicit for future shared reports. */
int yvex_imatrix_report_translation_unit_anchor(void)
{
    return 0;
}
