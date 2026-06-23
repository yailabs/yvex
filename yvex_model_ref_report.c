/*
 * YVEX - Model reference names
 */
#include "yvex_model_ref_internal.h"

const char *yvex_model_ref_kind_name(yvex_model_ref_kind kind)
{
    switch (kind) {
    case YVEX_MODEL_REF_PATH:
        return "path";
    case YVEX_MODEL_REF_ALIAS:
        return "alias";
    case YVEX_MODEL_REF_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *yvex_model_ref_status_name(yvex_model_ref_status status)
{
    switch (status) {
    case YVEX_MODEL_REF_STATUS_RESOLVED:
        return "resolved";
    case YVEX_MODEL_REF_STATUS_NOT_FOUND:
        return "not-found";
    case YVEX_MODEL_REF_STATUS_ALIAS_PATH_MISSING:
        return "alias-path-missing";
    case YVEX_MODEL_REF_STATUS_REGISTRY_UNAVAILABLE:
        return "registry-unavailable";
    case YVEX_MODEL_REF_STATUS_INVALID:
        return "invalid";
    case YVEX_MODEL_REF_STATUS_UNKNOWN:
    default:
        return "unknown";
    }
}
