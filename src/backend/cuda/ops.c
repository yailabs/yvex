/* Owner: src/backend/cuda.
 * Owns: exact dtype/shape/parameter validation, Driver API launch parameters, bounded grid arithmetic,
 *   synchronization, and output-written transitions.
 * Does not own: bundle admission, device kernel source, CPU references, graph semantics, model-family behavior, CLI
 *   output, qtype compute, or generation.
 * Invariants: every op requires an exact admitted variant; launch and final synchronization must succeed before any
 *   output is marked written.
 * Boundary: bounded primitive execution is not transformer or model runtime.
 * Purpose: Validate and launch CUDA graph primitives through admitted generated-kernel variants.
 * Inputs: Owned CUDA tensors, checked operation geometry, and immutable numeric parameters.
 * Effects: Launches work and marks output written only after successful synchronization.
 * Failure: Any admission, launch, or sync failure leaves output uncommitted. */

#include "src/backend/cuda/private.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Contract: converts a non-zero one-dimensional launch extent without truncation. */
/* Purpose: Implement the canonical grid 1d mechanism owned by the CUDA backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
static int cuda_grid_1d(unsigned long long elements,
                        unsigned int block_size,
                        unsigned int *out,
                        const char *where,
                        yvex_error *err)
{
    unsigned long long blocks;

    if (!out || block_size == 0u || elements == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where,
                       "CUDA launch extent and block size must be non-zero");
        return YVEX_ERR_INVALID_ARG;
    }
    blocks = ((elements - 1ull) / (unsigned long long)block_size) + 1ull;
    if (blocks > (unsigned long long)UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "CUDA grid dimension exceeds Driver API range");
        return YVEX_ERR_BOUNDS;
    }
    *out = (unsigned int)blocks;
    return YVEX_OK;
}

/* Purpose: Execute the typed op embed operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_op_embed(yvex_backend *backend,
                       const yvex_device_tensor *embedding,
                       const unsigned int *token_ids,
                       unsigned long long token_count,
                       yvex_device_tensor *out,
                       yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr token_ids_device = 0;
    CUdeviceptr embedding_ptr;
    CUdeviceptr out_ptr;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long total_elements;
    size_t token_bytes;
    unsigned int block_size = 128;
    unsigned int grid_size;
    void *params[6];
    yvex_backend_operation_variant variant;
    yvex_error cleanup_error;
    int cleanup_rc;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_owner_is(backend, embedding) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "embedding and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if ((embedding->dtype != YVEX_DTYPE_F32 && embedding->dtype != YVEX_DTYPE_F16) ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "CUDA backend embed supports F32 and F16 embeddings with F32 output");
        return YVEX_ERR_UNSUPPORTED;
    }
    variant = embedding->dtype == YVEX_DTYPE_F16
                  ? YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32
                  : YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_backend_validate_embed(
        backend, embedding, token_ids, token_count, out, &hidden_size, &vocab_size,
        "CUDA backend embed supports F32 and F16 embeddings with F32 output",
        "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    total_elements = token_count * hidden_size;
    if (token_count > (unsigned long long)(SIZE_MAX / sizeof(unsigned int))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "token id buffer is too large");
        return YVEX_ERR_BOUNDS;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    token_bytes = (size_t)(token_count * sizeof(unsigned int));
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemAlloc_v2(&token_ids_device, token_bytes),
                          "cuda.embed.token_alloc", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemcpyHtoD_v2(token_ids_device,
                                                         token_ids,
                                                         token_bytes),
                          "cuda.embed.token_copy", err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&cleanup_error);
        cleanup_rc = yvex_cuda_temporary_free(backend, variant, token_ids_device,
                                              "cuda.embed.token_cleanup",
                                              &cleanup_error);
        if (cleanup_rc != YVEX_OK) {
            if (err) {
                *err = cleanup_error;
            }
            return cleanup_rc;
        }
        return rc;
    }

    rc = cuda_grid_1d(total_elements, block_size, &grid_size,
                      "cuda.embed.grid", err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&cleanup_error);
        cleanup_rc = yvex_cuda_temporary_free(backend, variant, token_ids_device,
                                              "cuda.embed.token_cleanup",
                                              &cleanup_error);
        if (cleanup_rc != YVEX_OK) {
            if (err) {
                *err = cleanup_error;
            }
            return cleanup_rc;
        }
        return rc;
    }
    embedding_ptr = yvex_cuda_tensor_ptr(embedding);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &embedding_ptr;
    params[1] = &token_ids_device;
    params[2] = &out_ptr;
    params[3] = &hidden_size;
    params[4] = &vocab_size;
    params[5] = &token_count;
    rc = yvex_cuda_launch(backend, variant,
                          embedding->dtype == YVEX_DTYPE_F16
                              ? state->embed_f16_function : state->embed_function,
                          grid_size, block_size, 0, params,
                          "cuda.embed.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.embed.sync", err);
    }
    yvex_error_clear(&cleanup_error);
    cleanup_rc = yvex_cuda_temporary_free(backend, variant, token_ids_device,
                                          "cuda.embed.token_cleanup",
                                          &cleanup_error);
    if (cleanup_rc != YVEX_OK) {
        if (err) {
            *err = cleanup_error;
        }
        return cleanup_rc;
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Execute the typed op rms norm operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_op_rms_norm(yvex_backend *backend,
                          const yvex_device_tensor *input,
                          const yvex_device_tensor *weight,
                          float epsilon,
                          yvex_device_tensor *out,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr weight_ptr;
    CUdeviceptr out_ptr;
    unsigned long long hidden_size;
    unsigned int block_size = 256u;
    unsigned int shared_bytes = block_size * (unsigned int)sizeof(float);
    void *params[5];
    yvex_backend_operation_variant variant;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rms_norm",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, weight) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rms_norm",
                       "input, weight, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32 ||
        (weight->dtype != YVEX_DTYPE_F32 && weight->dtype != YVEX_DTYPE_F16)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rms_norm",
                       "CUDA RMSNorm supports F32 input/output with F16 or F32 weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    variant = weight->dtype == YVEX_DTYPE_F16
                  ? YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16
                  : YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_backend_validate_rms_norm(
        backend, input, weight, epsilon, out, &hidden_size,
        "CUDA RMSNorm supports F32 input/output with F16 or F32 weight",
        "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &weight_ptr;
    params[2] = &out_ptr;
    params[3] = &hidden_size;
    params[4] = &epsilon;

    rc = yvex_cuda_launch(backend, variant,
                          weight->dtype == YVEX_DTYPE_F16
                              ? state->rms_norm_f16_function
                              : state->rms_norm_f32_function,
                          1, block_size, shared_bytes, params,
                          "cuda.rms_norm.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.rms_norm.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Execute the typed op rope operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_op_rope(yvex_backend *backend,
                      const yvex_device_tensor *input,
                      unsigned long long position,
                      float rope_base,
                      yvex_device_tensor *out,
                      yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr out_ptr;
    unsigned long long head_dim;
    unsigned long long pair_count;
    unsigned int block_size = 128u;
    unsigned int grid_size;
    float inverse_root;
    void *params[5];
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "input and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rope",
                       "CUDA RoPE supports F32 input/output");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_ROPE_F32,
                                      "yvex_backend_op_rope", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!yvex_backend_tensor_same_shape(input, out)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!isfinite(rope_base) || rope_base <= 1.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "rope_base must be finite and greater than 1");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_backend_validate_rope(input, &head_dim, "yvex_backend_op_rope", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!yvex_backend_tensor_f32_elements(input, head_dim) ||
        !yvex_backend_tensor_f32_elements(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rope",
                       "RoPE input/output bytes must match F32 head_dim");
        return YVEX_ERR_BOUNDS;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_rope", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    pair_count = head_dim / 2ull;
    inverse_root = (float)(1.0 / yvex_backend_nth_root((double)rope_base, pair_count));
    rc = cuda_grid_1d(pair_count, block_size, &grid_size, "cuda.rope.grid", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &out_ptr;
    params[2] = &head_dim;
    params[3] = &position;
    params[4] = &inverse_root;

    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ROPE_F32,
                          state->rope_function, grid_size, block_size, 0,
                          params, "cuda.rope.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_ROPE_F32,
                                   "cuda.rope.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Execute the typed op matmul operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_op_matmul(yvex_backend *backend,
                        const yvex_device_tensor *input,
                        const yvex_device_tensor *weight,
                        yvex_device_tensor *out,
                        yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr weight_ptr;
    CUdeviceptr out_ptr;
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long output_elements;
    unsigned int block_size = 128u;
    unsigned int grid_size;
    void *params[6];
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_matmul",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    rc = yvex_backend_validate_matmul(backend, input, weight, out, &m, &k, &n,
                                      "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_MATMUL_F32,
                                      "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    output_elements = m * n;
    rc = cuda_grid_1d(output_elements, block_size, &grid_size,
                      "cuda.matmul.grid", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &weight_ptr;
    params[2] = &out_ptr;
    params[3] = &m;
    params[4] = &k;
    params[5] = &n;

    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_MATMUL_F32,
                          state->matmul_function, grid_size, block_size, 0,
                          params, "cuda.matmul.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_MATMUL_F32,
                                   "cuda.matmul.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Execute the typed op mlp operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_op_mlp(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     const yvex_device_tensor *gate_weight,
                     const yvex_device_tensor *up_weight,
                     const yvex_device_tensor *down_weight,
                     const yvex_mlp_options *options,
                     yvex_device_tensor *intermediate,
                     yvex_device_tensor *out,
                     yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr gate_ptr;
    CUdeviceptr up_ptr;
    CUdeviceptr down_ptr;
    CUdeviceptr intermediate_ptr;
    CUdeviceptr out_ptr;
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long expert_count;
    unsigned long long expert_id;
    unsigned int block_size = 128u;
    int routed;
    void *params[12];
    yvex_backend_operation_variant variant;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_mlp",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    rc = yvex_backend_validate_mlp(
        backend, input, gate_weight, up_weight, down_weight, options,
        intermediate, out, &batch, &hidden_dim, &ffn_dim, NULL, NULL, NULL,
        "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    variant = options->routed_expert_mode
                  ? YVEX_BACKEND_VARIANT_MLP_ROUTED_F32
                  : YVEX_BACKEND_VARIANT_MLP_DENSE_F32;
    intermediate->is_written = 0;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    gate_ptr = yvex_cuda_tensor_ptr(gate_weight);
    up_ptr = yvex_cuda_tensor_ptr(up_weight);
    down_ptr = yvex_cuda_tensor_ptr(down_weight);
    intermediate_ptr = yvex_cuda_tensor_ptr(intermediate);
    out_ptr = yvex_cuda_tensor_ptr(out);
    expert_count = options->expert_count;
    expert_id = options->expert_id;
    routed = options->routed_expert_mode ? 1 : 0;
    params[0] = &input_ptr;
    params[1] = &gate_ptr;
    params[2] = &up_ptr;
    params[3] = &down_ptr;
    params[4] = &intermediate_ptr;
    params[5] = &out_ptr;
    params[6] = &batch;
    params[7] = &hidden_dim;
    params[8] = &ffn_dim;
    params[9] = &expert_count;
    params[10] = &expert_id;
    params[11] = &routed;

    rc = yvex_cuda_launch(backend, variant, state->mlp_function,
                          1, block_size, 0, params, "cuda.mlp.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.mlp.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    intermediate->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Execute the typed op attention operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_op_attention(yvex_backend *backend,
                           const yvex_device_tensor *query,
                           const yvex_device_tensor *keys,
                           const yvex_device_tensor *values,
                           unsigned long long seq_len,
                           unsigned long long position,
                           float scale,
                           int causal,
                           yvex_device_tensor *score_scratch,
                           yvex_device_tensor *probability_scratch,
                           yvex_device_tensor *out,
                           yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr query_ptr;
    CUdeviceptr keys_ptr;
    CUdeviceptr values_ptr;
    CUdeviceptr score_ptr;
    CUdeviceptr probability_ptr;
    CUdeviceptr out_ptr;
    unsigned long long head_dim;
    unsigned long long kv_elements;
    unsigned int block_size = 128u;
    int causal_flag = causal ? 1 : 0;
    void *params[11];
    yvex_backend_operation_variant variant;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_attention",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!isfinite(scale) || scale <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "scale must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_backend_validate_attention(
        backend, query, keys, values, seq_len, position, score_scratch,
        probability_scratch, out, &head_dim, &kv_elements,
        "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)kv_elements;
    variant = causal ? YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32
                     : YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32;
    score_scratch->is_written = 0;
    probability_scratch->is_written = 0;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    query_ptr = yvex_cuda_tensor_ptr(query);
    keys_ptr = yvex_cuda_tensor_ptr(keys);
    values_ptr = yvex_cuda_tensor_ptr(values);
    score_ptr = yvex_cuda_tensor_ptr(score_scratch);
    probability_ptr = yvex_cuda_tensor_ptr(probability_scratch);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &query_ptr;
    params[1] = &keys_ptr;
    params[2] = &values_ptr;
    params[3] = &score_ptr;
    params[4] = &probability_ptr;
    params[5] = &out_ptr;
    params[6] = &seq_len;
    params[7] = &position;
    params[8] = &head_dim;
    params[9] = &scale;
    params[10] = &causal_flag;

    rc = yvex_cuda_launch(backend, variant, state->attention_function,
                          1, block_size, 0, params,
                          "cuda.attention.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.attention.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    score_scratch->is_written = 1;
    probability_scratch->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}
