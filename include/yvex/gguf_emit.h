/*
 * YVEX - GGUF emitter
 *
 * File: include/yvex/gguf_emit.h
 * Layer: public tool API
 *
 * Purpose:
 *   Defines the first intentionally small GGUF emission surface. OWI.7 emits
 *   one controlled F32 tensor with controlled metadata and validates it through
 *   the existing YVEX parser and materialization stack.
 *
 * Does not own:
 *   - generic model conversion
 *   - DeepSeek conversion
 *   - quantization
 *   - inference readiness
 */
#ifndef YVEX_GGUF_EMIT_H
#define YVEX_GGUF_EMIT_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_gguf_emit_plan yvex_gguf_emit_plan;

typedef enum {
    YVEX_GGUF_EMIT_STATUS_UNKNOWN = 0,
    YVEX_GGUF_EMIT_STATUS_PLANNED,
    YVEX_GGUF_EMIT_STATUS_WRITTEN,
    YVEX_GGUF_EMIT_STATUS_FAILED
} yvex_gguf_emit_status;

typedef struct {
    const char *out_path;
    const char *template_path;
    const char *native_source_dir;
    const char *tensor_name;
    const char *target_name;
    const char *model_name;
    const char *architecture;
    int transpose_2d;
    int overwrite;
} yvex_gguf_emit_options;

typedef struct {
    yvex_gguf_emit_status status;
    const char *out_path;
    const char *template_path;
    const char *model_name;
    const char *architecture;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long bytes_written;
    unsigned long long tensor_payload_bytes;
    unsigned long long alignment;
    int roundtrip_validated;
} yvex_gguf_emit_summary;

int yvex_gguf_emit_controlled(const yvex_gguf_emit_options *options,
                              yvex_gguf_emit_summary *summary_out,
                              yvex_error *err);

const char *yvex_gguf_emit_status_name(yvex_gguf_emit_status status);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GGUF_EMIT_H */
