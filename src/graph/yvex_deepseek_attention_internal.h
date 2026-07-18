/*
 * yvex_deepseek_attention_internal.h - private DeepSeek attention graph contracts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   shared private plan object shape, checked arithmetic, failure helpers,
 *   identity helpers, and cross-module declarations for DeepSeek attention.
 *
 * Does not own:
 *   public ABI expansion, source parsing, materialization, persistent KV, full
 *   transformer execution, generation, CLI output, or release claims.
 *
 * Invariants:
 *   all modules share one failure taxonomy and one identity encoding; private
 *   helpers do not read payload bytes unless an execution owner passes a
 *   committed materialization binding.
 *
 * Boundary:
 *   private module structure is implementation ownership, not capability.
 */
#ifndef YVEX_DEEPSEEK_ATTENTION_INTERNAL_H
#define YVEX_DEEPSEEK_ATTENTION_INTERNAL_H

#include "yvex_deepseek_attention.h"

#include "src/core/yvex_sha256.h"
#include "src/gguf/yvex_quant_numeric.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_deepseek_attention_plan {
    yvex_deepseek_attention_layer_plan *layers;
    unsigned long long layer_count;
    yvex_deepseek_attention_summary summary;
};

int attention_reject(yvex_deepseek_attention_failure *failure,
                     yvex_deepseek_attention_failure_code code,
                     const yvex_runtime_tensor_binding *binding,
                     unsigned long long layer_index,
                     yvex_tensor_role role,
                     unsigned long long expected,
                     unsigned long long actual,
                     yvex_error *err,
                     yvex_status err_code,
                     const char *reason);
int attention_hash_u64(yvex_sha256 *hash, unsigned long long value);
int attention_hash_text(yvex_sha256 *hash, const char *text);
int attention_checked_mul_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out);
int attention_checked_add_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out);
int attention_checked_size(unsigned long long count,
                           unsigned long long width,
                           size_t *out);
unsigned long long attention_min_u64(unsigned long long a,
                                     unsigned long long b);
void *attention_calloc_array(unsigned long long count,
                             unsigned long long width);
int attention_rolling_geometry(
    const yvex_deepseek_attention_layer_plan *layer,
    yvex_deepseek_attention_rolling_kind kind,
    unsigned long long *ratio,
    unsigned long long *head_dim,
    unsigned long long *state_width,
    unsigned long long *state_slots,
    int *overlap,
    int *rotated);
int attention_execution_context_validate(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_hadamard_cpu(
    const float *input,
    unsigned long long length,
    float scale,
    int reject_nonfinite,
    float *output,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_topk_select(
    const float *scores,
    const unsigned long long *ordinals,
    unsigned long long candidate_count,
    unsigned long long k,
    unsigned long long *selected_indices,
    unsigned long long *selected_count,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
unsigned char yvex_deepseek_attention_ue8m0_encode_scale(float value);
float yvex_deepseek_attention_ue8m0_decode_scale(unsigned char code);
unsigned char yvex_deepseek_attention_fp4_e2m1_encode(float value);
float yvex_deepseek_attention_fp4_e2m1_decode(unsigned char code);
unsigned char yvex_deepseek_attention_fp8_e4m3fn_encode(float value);
float yvex_deepseek_attention_fp8_e4m3fn_decode(unsigned char code);
int yvex_deepseek_attention_fp8_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_fp4_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);

#endif
