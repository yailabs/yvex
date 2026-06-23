/*
 * YVEX - GGUF emitter internals
 */
#ifndef YVEX_GGUF_EMIT_INTERNAL_H
#define YVEX_GGUF_EMIT_INTERNAL_H

#include <stdio.h>

#include <yvex/gguf_emit.h>

#define YVEX_GGUF_EMIT_ALIGNMENT 32ull
#define YVEX_GGUF_EMIT_METADATA_COUNT 12ull
#define YVEX_GGUF_EMIT_TENSOR_COUNT 1ull
#define YVEX_GGUF_EMIT_NATIVE_ROWS 8u
#define YVEX_GGUF_EMIT_NATIVE_COLS 4u
#define YVEX_GGUF_EMIT_TENSOR_FLOATS 32u
#define YVEX_GGUF_EMIT_PAYLOAD_BYTES 128ull

typedef struct {
    const char *out_path;
    const char *template_path;
    const char *model_name;
    const char *architecture;
    const char *tensor_name;
    const char *target_name;
    int transpose_2d;
    int overwrite;
} yvex_gguf_emit_plan_data;

int yvex_gguf_emit_write_metadata(FILE *fp,
                                  const yvex_gguf_emit_plan_data *plan,
                                  yvex_error *err);
int yvex_gguf_emit_write_tensor_dir(FILE *fp,
                                    const yvex_gguf_emit_plan_data *plan,
                                    yvex_error *err);
int yvex_gguf_emit_write_tensor_payload(FILE *fp,
                                        const yvex_gguf_emit_plan_data *plan,
                                        yvex_error *err);
int yvex_gguf_emit_print_summary(const yvex_gguf_emit_summary *summary);

int yvex_gguf_emit_write_u32(FILE *fp, unsigned int value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_u64(FILE *fp, unsigned long long value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_i32(FILE *fp, int value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_f32(FILE *fp, float value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_string(FILE *fp, const char *value, yvex_error *err, const char *field);
int yvex_gguf_emit_pad_to_alignment(FILE *fp, unsigned long long alignment, yvex_error *err);

#endif /* YVEX_GGUF_EMIT_INTERNAL_H */
