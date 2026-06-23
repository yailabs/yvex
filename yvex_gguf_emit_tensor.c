/*
 * YVEX - GGUF emitter tensor directory and payload writer
 */
#include "yvex_gguf_emit_internal.h"

#include <string.h>

int yvex_gguf_emit_pad_to_alignment(FILE *fp, unsigned long long alignment, yvex_error *err)
{
    long pos;
    unsigned long long rem;
    unsigned long long pad;
    unsigned char zero[32];

    pos = ftell(fp);
    if (pos < 0) {
        yvex_error_set(err, YVEX_ERR_IO, "gguf_emit.align", "failed to query file offset");
        return YVEX_ERR_IO;
    }
    rem = ((unsigned long long)pos) % alignment;
    pad = rem == 0 ? 0 : alignment - rem;
    memset(zero, 0, sizeof(zero));
    while (pad > 0) {
        size_t chunk = pad > sizeof(zero) ? sizeof(zero) : (size_t)pad;
        if (fwrite(zero, 1u, chunk, fp) != chunk) {
            yvex_error_set(err, YVEX_ERR_IO, "gguf_emit.align", "failed to write alignment padding");
            return YVEX_ERR_IO;
        }
        pad -= (unsigned long long)chunk;
    }
    return YVEX_OK;
}

int yvex_gguf_emit_write_tensor_dir(FILE *fp,
                                    const yvex_gguf_emit_plan_data *plan,
                                    yvex_error *err)
{
    if (yvex_gguf_emit_write_string(fp, plan->target_name, err, "tensor name") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, 2u, err, "tensor rank") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, 4ull, err, "tensor dim 0") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, 8ull, err, "tensor dim 1") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, 0u, err, "tensor ggml type") != YVEX_OK) return YVEX_ERR_IO;
    return yvex_gguf_emit_write_u64(fp, 0ull, err, "tensor relative offset");
}

int yvex_gguf_emit_write_tensor_payload(FILE *fp,
                                        const yvex_gguf_emit_plan_data *plan,
                                        yvex_error *err)
{
    float native[YVEX_GGUF_EMIT_NATIVE_ROWS][YVEX_GGUF_EMIT_NATIVE_COLS];
    unsigned int row;
    unsigned int col;

    for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
        for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
            native[row][col] = (float)(row * YVEX_GGUF_EMIT_NATIVE_COLS + col);
        }
    }

    if (plan->transpose_2d) {
        for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
            for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
                if (yvex_gguf_emit_write_f32(fp, native[row][col], err, "tensor payload") != YVEX_OK) {
                    return YVEX_ERR_IO;
                }
            }
        }
    } else {
        for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
            for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
                if (yvex_gguf_emit_write_f32(fp, native[row][col], err, "tensor payload") != YVEX_OK) {
                    return YVEX_ERR_IO;
                }
            }
        }
    }

    return YVEX_OK;
}
