/*
 * YVEX - Conversion plan builder
 */
#include "conversion_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yvex_conversion_plan_write_json(const yvex_conversion_options *options,
                                    const char *plan_out_path,
                                    yvex_conversion_summary *summary_out,
                                    yvex_error *err)
{
    yvex_native_weight_options no;
    yvex_native_weight_table *native = NULL;
    yvex_conversion_tensor_plan *plans = NULL;
    unsigned long long native_count;
    unsigned long long plan_count = 0;
    unsigned long long i;
    FILE *fp;
    int rc;

    if (!options || !summary_out || !options->architecture ||
        !options->native_source_dir || !plan_out_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_plan", "arch, native-source and out-plan are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(summary_out, 0, sizeof(*summary_out));
    summary_out->architecture = options->architecture;
    summary_out->out_path = plan_out_path;
    summary_out->execution_ready = 0;
    memset(&no, 0, sizeof(no));
    no.source_dir = options->native_source_dir;
    no.recursive = 1;
    rc = yvex_native_weight_table_open(&native, &no, err);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    native_count = yvex_native_weight_table_count(native);
    summary_out->native_tensor_count = native_count;
    plans = (yvex_conversion_tensor_plan *)calloc((size_t)(native_count ? native_count : 1), sizeof(*plans));
    if (!plans) {
        yvex_native_weight_table_close(native);
        yvex_error_set(err, YVEX_ERR_NOMEM, "conversion_plan", "plan allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (i = 0; i < native_count; ++i) {
        const yvex_native_weight_info *info = yvex_native_weight_table_at(native, i);
        yvex_conversion_tensor_plan plan;
        const char *qtype = NULL;
        if (options->tensor_name && strcmp(options->tensor_name, info->name) != 0) continue;
        if (options->limit_tensors && plan_count >= options->limit_tensors) break;
        rc = yvex_conversion_map_tensor(options->architecture, info, qtype, &plan, err);
        if (rc != YVEX_OK) break;
        plans[plan_count++] = plan;
        if (plan.status == YVEX_CONVERT_TENSOR_STATUS_READY) summary_out->planned_tensor_count++;
        else if (plan.status == YVEX_CONVERT_TENSOR_STATUS_UNMAPPED) summary_out->unmapped_tensor_count++;
        else if (plan.status == YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE) summary_out->unsupported_qtype_count++;
    }
    if (rc == YVEX_OK) {
        fp = fopen(plan_out_path, "wb");
        if (!fp) {
            yvex_error_setf(err, YVEX_ERR_IO, "conversion_plan", "cannot open plan output: %s", strerror(errno));
            rc = YVEX_ERR_IO;
        } else {
            rc = yvex_conversion_report_plan_json(fp, options, summary_out, plans, plan_count, err);
            if (fclose(fp) != 0 && rc == YVEX_OK) rc = YVEX_ERR_IO;
        }
    }
    free(plans);
    yvex_native_weight_table_close(native);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    summary_out->status = summary_out->unsupported_qtype_count || summary_out->unmapped_tensor_count
        ? YVEX_CONVERSION_STATUS_PARTIAL
        : YVEX_CONVERSION_STATUS_PLANNED;
    return YVEX_OK;
}
