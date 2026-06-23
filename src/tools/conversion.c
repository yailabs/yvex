/*
 * YVEX - Conversion bridge status and mapping helpers
 */
#include "conversion_internal.h"
#include "adapters/deepseek_adapter.h"
#include "adapters/qwen_adapter.h"

#include <stdio.h>
#include <string.h>

#include <yvex/qtype_support.h>

const char *yvex_conversion_status_name(yvex_conversion_status status)
{
    switch (status) {
    case YVEX_CONVERSION_STATUS_UNKNOWN: return "conversion-unknown";
    case YVEX_CONVERSION_STATUS_PLANNED: return "conversion-planned";
    case YVEX_CONVERSION_STATUS_EMITTED: return "conversion-emitted";
    case YVEX_CONVERSION_STATUS_PARTIAL: return "conversion-partial";
    case YVEX_CONVERSION_STATUS_FAILED: return "conversion-failed";
    }
    return "conversion-unknown";
}

const char *yvex_convert_tensor_status_name(yvex_convert_tensor_status status)
{
    switch (status) {
    case YVEX_CONVERT_TENSOR_STATUS_UNKNOWN: return "unknown";
    case YVEX_CONVERT_TENSOR_STATUS_READY: return "ready";
    case YVEX_CONVERT_TENSOR_STATUS_EMITTED: return "emitted";
    case YVEX_CONVERT_TENSOR_STATUS_SKIPPED: return "skipped";
    case YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE: return "unsupported_qtype";
    case YVEX_CONVERT_TENSOR_STATUS_UNMAPPED: return "unmapped";
    case YVEX_CONVERT_TENSOR_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_convert_transform_kind_name(yvex_convert_transform_kind transform)
{
    switch (transform) {
    case YVEX_CONVERT_TRANSFORM_NONE: return "none";
    case YVEX_CONVERT_TRANSFORM_TRANSPOSE_2D: return "transpose";
    case YVEX_CONVERT_TRANSFORM_DTYPE_CAST: return "dtype_cast";
    case YVEX_CONVERT_TRANSFORM_QUANTIZE: return "quantize";
    case YVEX_CONVERT_TRANSFORM_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

const char *yvex_conversion_default_qtype_for_role(yvex_tensor_role role)
{
    switch (role) {
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
        return "F32";
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
        return "F16";
    default:
        return "Q8_0";
    }
}

static int qtype_to_ggml(const char *qtype, unsigned int *ggml, unsigned int *scalar)
{
    if (!qtype) return 0;
    if (strcmp(qtype, "F32") == 0) {
        *ggml = 0u; *scalar = 4u; return 1;
    }
    if (strcmp(qtype, "F16") == 0) {
        *ggml = 1u; *scalar = 2u; return 1;
    }
    if (strcmp(qtype, "BF16") == 0) {
        *ggml = 30u; *scalar = 2u; return 1;
    }
    return 0;
}

int yvex_conversion_map_tensor(const char *arch,
                               const yvex_native_weight_info *native,
                               const char *target_qtype,
                               yvex_conversion_tensor_plan *out,
                               yvex_error *err)
{
    yvex_weight_mapping_issue_kind issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    int mapped;
    const yvex_qtype_support_info *support;

    if (!arch || !native || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_map", "arch, native and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->native_name, sizeof(out->native_name), "%s", native->name ? native->name : "");
    out->native = native;

    if (strcmp(arch, "qwen3") == 0 || strcmp(arch, "qwen") == 0) {
        mapped = yvex_qwen_adapter_map_name(native->name, out->target_name, sizeof(out->target_name),
                                            &out->role, &issue);
    } else if (strcmp(arch, "deepseek4") == 0 || strcmp(arch, "deepseek") == 0) {
        mapped = yvex_deepseek_adapter_map_name(native->name, out->target_name, sizeof(out->target_name),
                                                &out->role, &issue);
    } else {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "conversion_map", "unsupported architecture: %s", arch);
        return YVEX_ERR_INVALID_ARG;
    }

    if (!mapped) {
        out->status = YVEX_CONVERT_TENSOR_STATUS_UNMAPPED;
        snprintf(out->target_name, sizeof(out->target_name), "%s", "unknown");
        return YVEX_OK;
    }

    out->target_qtype = target_qtype ? target_qtype : yvex_conversion_default_qtype_for_role(out->role);
    support = yvex_qtype_support_by_name(out->target_qtype);
    if (!support || !support->emit_supported) {
        out->status = YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE;
        out->transform = YVEX_CONVERT_TRANSFORM_UNSUPPORTED;
        return YVEX_OK;
    }
    if (!qtype_to_ggml(out->target_qtype, &out->ggml_type, &out->target_scalar_bytes)) {
        out->status = YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE;
        out->transform = YVEX_CONVERT_TRANSFORM_UNSUPPORTED;
        return YVEX_OK;
    }

    out->status = YVEX_CONVERT_TENSOR_STATUS_READY;
    out->transform = YVEX_CONVERT_TRANSFORM_DTYPE_CAST;
    if ((strcmp(out->target_qtype, "F16") == 0 && native->dtype == YVEX_NATIVE_DTYPE_F16) ||
        (strcmp(out->target_qtype, "BF16") == 0 && native->dtype == YVEX_NATIVE_DTYPE_BF16) ||
        (strcmp(out->target_qtype, "F32") == 0 && native->dtype == YVEX_NATIVE_DTYPE_F32)) {
        out->transform = YVEX_CONVERT_TRANSFORM_NONE;
    }
    return YVEX_OK;
}
