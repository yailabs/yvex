/*
 * YVEX - GGUF emitter reporting helpers
 */
#include "gguf_emit_internal.h"

#include <stdio.h>

const char *yvex_gguf_emit_status_name(yvex_gguf_emit_status status)
{
    switch (status) {
    case YVEX_GGUF_EMIT_STATUS_UNKNOWN: return "gguf-unknown";
    case YVEX_GGUF_EMIT_STATUS_PLANNED: return "gguf-planned";
    case YVEX_GGUF_EMIT_STATUS_WRITTEN: return "gguf-written";
    case YVEX_GGUF_EMIT_STATUS_FAILED: return "gguf-failed";
    }
    return "gguf-unknown";
}

int yvex_gguf_emit_print_summary(const yvex_gguf_emit_summary *summary)
{
    if (!summary) {
        return YVEX_ERR_INVALID_ARG;
    }
    printf("gguf emit: controlled\n");
    printf("out: %s\n", summary->out_path ? summary->out_path : "");
    printf("architecture: %s\n", summary->architecture ? summary->architecture : "");
    printf("model_name: %s\n", summary->model_name ? summary->model_name : "");
    printf("metadata_count: %llu\n", summary->metadata_count);
    printf("tensor_count: %llu\n", summary->tensor_count);
    printf("tensor_payload_bytes: %llu\n", summary->tensor_payload_bytes);
    printf("alignment: %llu\n", summary->alignment);
    printf("roundtrip_validated: %s\n", summary->roundtrip_validated ? "yes" : "no");
    printf("status: %s\n", yvex_gguf_emit_status_name(summary->status));
    return YVEX_OK;
}
