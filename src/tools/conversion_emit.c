/*
 * YVEX - Conversion emit API
 */
#include "conversion_internal.h"

#include <stdlib.h>
#include <string.h>

int yvex_conversion_emit_gguf(const yvex_conversion_options *options,
                              yvex_conversion_summary *summary_out,
                              yvex_error *err)
{
    yvex_native_weight_options no;
    yvex_native_weight_table *native = NULL;
    const yvex_native_weight_info *info;
    yvex_conversion_tensor_plan plan;
    unsigned char *raw = NULL;
    unsigned char *converted = NULL;
    unsigned long long raw_len = 0;
    unsigned long long converted_len = 0;
    int rc;

    if (!options || !summary_out || !options->architecture ||
        !options->native_source_dir || !options->tensor_name || !options->out_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_emit", "arch, native-source, tensor and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(summary_out, 0, sizeof(*summary_out));
    summary_out->architecture = options->architecture;
    summary_out->out_path = options->out_path;
    summary_out->execution_ready = 0;

    memset(&no, 0, sizeof(no));
    no.source_dir = options->native_source_dir;
    no.recursive = 1;
    rc = yvex_native_weight_table_open(&native, &no, err);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    summary_out->native_tensor_count = yvex_native_weight_table_count(native);
    info = yvex_native_weight_table_find(native, options->tensor_name);
    if (!info) {
        yvex_native_weight_table_close(native);
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_emit", "selected tensor not found");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_conversion_map_tensor(options->architecture, info, options->target_qtype, &plan, err);
    if (rc == YVEX_OK && plan.status == YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        summary_out->unsupported_qtype_count = 1;
        yvex_native_weight_table_close(native);
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "conversion_emit",
                        "target qtype %s emit/quantize not implemented",
                        options->target_qtype ? options->target_qtype : plan.target_qtype);
        return YVEX_ERR_UNSUPPORTED;
    }
    if (rc == YVEX_OK && plan.status != YVEX_CONVERT_TENSOR_STATUS_READY) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        yvex_native_weight_table_close(native);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_emit", "selected tensor is not convertible");
        return YVEX_ERR_INVALID_ARG;
    }
    if (rc == YVEX_OK) rc = yvex_conversion_read_payload(options->native_source_dir, info, &raw, &raw_len, err);
    if (rc == YVEX_OK) rc = yvex_conversion_convert_payload(raw, raw_len, info->dtype, &plan, &converted, &converted_len, err);
    if (rc == YVEX_OK) {
        summary_out->planned_tensor_count = 1;
        summary_out->bytes_read = raw_len;
        rc = yvex_conversion_write_single_gguf(options, &plan, converted, converted_len, summary_out, err);
    }
    free(raw);
    free(converted);
    yvex_native_weight_table_close(native);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    summary_out->status = YVEX_CONVERSION_STATUS_EMITTED;
    summary_out->emitted_tensor_count = 1;
    return YVEX_OK;
}
