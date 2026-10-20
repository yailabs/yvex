/* Project the backend-neutral exact-attention contract onto CUDA GQA execution. */
#include <yvex/internal/neural_operations.h>

#include "src/backend/cuda/private.h"
#include "src/backend/cuda/transformer_ops.h"
#include <limits.h>
#include <math.h>
#include <string.h>

enum { YVEX_CUDA_EXACT_ATTENTION_HEAD_MAX = 256u, EAGER_QUERY_TILE = 8u };

static int exact_attention_validate(
    const yvex_transformer_attention_requirement *requirement,
    const char *stage, yvex_error *err)
{
    unsigned long long query_end, query_width, key_value_width;
    if (!requirement || !requirement->query_tokens ||
        !requirement->key_value_tokens ||
        !yvex_core_u64_add(requirement->query_start,
                           requirement->query_tokens, &query_end) ||
        query_end > requirement->key_value_tokens ||
        !requirement->query_heads || !requirement->key_value_heads ||
        requirement->query_heads % requirement->key_value_heads ||
        !requirement->head_dimension ||
        !yvex_core_u64_mul(requirement->query_heads,
                           requirement->head_dimension, &query_width) ||
        !yvex_core_u64_mul(requirement->key_value_heads,
                           requirement->head_dimension, &key_value_width) ||
        (requirement->query_token_stride &&
         requirement->query_token_stride < query_width) ||
        (requirement->key_token_stride &&
         requirement->key_token_stride < key_value_width) ||
        (requirement->value_token_stride &&
         requirement->value_token_stride < key_value_width)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "complete exact-attention geometry is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (requirement->layout != YVEX_TRANSFORMER_ATTENTION_LAYOUT_TOKEN_HEAD_DIM ||
        requirement->mask < YVEX_TRANSFORMER_ATTENTION_MASK_FULL ||
        requirement->mask > YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL ||
        (requirement->numeric_contract != YVEX_TRANSFORMER_ATTENTION_NUMERIC_EXACT_F32 &&
         requirement->numeric_contract != YVEX_TRANSFORMER_ATTENTION_NUMERIC_BF16_EAGER) ||
        requirement->head_dimension > YVEX_CUDA_EXACT_ATTENTION_HEAD_MAX ||
        requirement->query_dtype != YVEX_DTYPE_F32 ||
        requirement->key_dtype != YVEX_DTYPE_F32 ||
        requirement->value_dtype != YVEX_DTYPE_F32 ||
        requirement->output_dtype != YVEX_DTYPE_F32 ||
        !requirement->deterministic) {
        yvex_error_set(
            err, YVEX_ERR_UNSUPPORTED, stage,
            "CUDA requires deterministic token/head/dimension exact F32 attention");
        return YVEX_ERR_UNSUPPORTED;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int eager_geometry(const yvex_transformer_attention_requirement *r,
    unsigned long long *scores, unsigned long long strides[3], unsigned long long extents[4])
{
    unsigned long long tile = r->query_tokens < EAGER_QUERY_TILE ? r->query_tokens : EAGER_QUERY_TILE, outputs;
    unsigned long long widths[3] = {r->query_heads * r->head_dimension,
        r->key_value_heads * r->head_dimension, r->key_value_heads * r->head_dimension};
    strides[0] = r->query_token_stride ? r->query_token_stride : widths[0];
    strides[1] = r->key_token_stride ? r->key_token_stride : widths[1];
    strides[2] = r->value_token_stride ? r->value_token_stride : widths[2];
    if (!yvex_core_u64_mul(tile, r->query_heads, scores) ||
        !yvex_core_u64_mul(*scores, r->key_value_tokens, scores) ||
        *scores > SIZE_MAX / sizeof(float) - 64u ||
        (*scores + 127u) / 128u > INT_MAX ||
        !yvex_core_u64_mul(tile, widths[0], &outputs) || outputs > (unsigned long long)INT_MAX * 128u ||
        !yvex_core_u64_mul(r->query_tokens, widths[0], &extents[3])) return 0;
    for (size_t i = 0u; i < 3u; ++i)
        if (!yvex_core_u64_mul((i ? r->key_value_tokens : r->query_tokens) - 1u, strides[i], &extents[i]) ||
            !yvex_core_u64_add(extents[i], widths[i], &extents[i]) ||
            extents[i] > SIZE_MAX / sizeof(float)) return 0;
    return extents[3] <= SIZE_MAX / sizeof(float);
}

int yvex_cuda_transformer_attention_workspace_required(
    const yvex_transformer_attention_requirement *requirement,
    unsigned long long *bytes, yvex_error *err)
{
    unsigned long long query_width, key_value_width;
    int rc;
    if (bytes) *bytes = 0ull;
    rc = exact_attention_validate(
        requirement, "cuda.transformer.attention.workspace", err);
    if (rc != YVEX_OK) return rc;
    if (!bytes) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.transformer.attention.workspace", "output required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (requirement->numeric_contract == YVEX_TRANSFORMER_ATTENTION_NUMERIC_BF16_EAGER) {
        unsigned long long scores, strides[3], extents[4];
        if (!eager_geometry(requirement, &scores, strides, extents) ||
            !yvex_core_u64_mul(scores, sizeof(float), bytes)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.transformer.attention.workspace",
                "eager score tile exceeds bounds");
            return YVEX_ERR_BOUNDS;
        }
        /* Score tile plus worst-case status alignment, unless a caller supplies
         * a prepared tile. No persistent history or weight population is copied. */
        *bytes += 256u;
        return YVEX_OK;
    }
    query_width = requirement->query_heads * requirement->head_dimension;
    key_value_width = requirement->key_value_heads * requirement->head_dimension;
    if ((requirement->query_token_stride &&
         requirement->query_token_stride != query_width) ||
        (requirement->key_token_stride &&
         requirement->key_token_stride != key_value_width) ||
        (requirement->value_token_stride &&
         requirement->value_token_stride != key_value_width)) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    return yvex_cuda_transformer_gqa_workspace_required(
        requirement->query_tokens, requirement->key_value_tokens,
        requirement->query_heads, requirement->key_value_heads,
        requirement->head_dimension, bytes, err);
}

static int eager_disjoint(const yvex_device_tensor *a, const yvex_device_tensor *b)
{
    uintptr_t x = (uintptr_t)a->data, y = (uintptr_t)b->data;
    return x <= y ? y - x >= a->bytes : x - y >= b->bytes;
}

static int eager_operands(yvex_backend *b, const yvex_transformer_attention_request *r,
    const unsigned long long extents[4], unsigned long long scores)
{
    const yvex_device_tensor *tensors[] = {r->query, r->key, r->value, r->output};
    for (size_t i = 0u; i < 4u; ++i)
        if (!backend_tensor_owner_is(b, tensors[i]) || tensors[i]->dtype != YVEX_DTYPE_F32 ||
            (i < 3u && !tensors[i]->is_written) || !backend_tensor_f32_elements(tensors[i], extents[i]))
            return 0;
    for (size_t i = 0u; i < 3u; ++i)
        if (!eager_disjoint(r->output, tensors[i])) return 0;
    if (r->workspace) {
        if (!backend_tensor_owner_is(b, r->workspace) || r->workspace->dtype != YVEX_DTYPE_F32 ||
            !backend_tensor_f32_elements(r->workspace, scores)) return 0;
        for (size_t i = 0u; i < 4u; ++i)
            if (!eager_disjoint(r->workspace, tensors[i])) return 0;
    }
    return 1;
}

static int eager_execute(yvex_backend *backend, const yvex_transformer_attention_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const char *where = "cuda.attention.eager-bf16";
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    const yvex_transformer_attention_requirement *g = &r->requirement;
    unsigned long long scores_count, strides[3], extents[4], offset = 0u, launches = 0u;
    unsigned long long cursor = backend ? backend->workspace_cursor : 0u;
    CUdeviceptr scores = 0u;
    yvex_cuda_work work = {.backend = backend, .state = state,
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    int rc, device_wide = 0, status = 0;
    memset(facts, 0, sizeof(*facts));
    if (!state || yvex_cuda_capture_active(backend) ||
        !eager_geometry(g, &scores_count, strides, extents) || !eager_operands(backend, r, extents, scores_count)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "disjoint eager attention operands outside graph capture required");
        return YVEX_ERR_FORMAT;
    }
    r->output->is_written = 0;
    rc = yvex_cuda_set_current(backend, where, err);
    if (rc == YVEX_OK && r->workspace) scores = yvex_cuda_tensor_ptr(r->workspace);
    else if (rc == YVEX_OK) rc = yvex_cuda_work_allocate(
        &work, &scores, (size_t)scores_count * sizeof(float), NULL, 0, where, NULL, err);
    if (rc == YVEX_OK) rc = yvex_cuda_work_status(&work, &work.status, where, err);
    CUdeviceptr q = yvex_cuda_tensor_ptr(r->query), k = yvex_cuda_tensor_ptr(r->key);
    CUdeviceptr v = yvex_cuda_tensor_ptr(r->value), output = yvex_cuda_tensor_ptr(r->output);
    unsigned long long heads = g->query_heads, kvheads = g->key_value_heads, dim = g->head_dimension;
    unsigned long long keys = g->key_value_tokens, start = g->query_start;
    int causal = g->mask == YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL;
    while (rc == YVEX_OK && offset < g->query_tokens) {
        unsigned long long rows = g->query_tokens - offset;
        if (rows > EAGER_QUERY_TILE) rows = EAGER_QUERY_TILE;
        void *score_args[] = {&q, &k, &scores, &heads, &kvheads, &dim, &keys, &start,
            &offset, &rows, &strides[0], &strides[1], &causal, &work.status};
        rc = yvex_cuda_launch(backend, work.variant, state->eager_score_function,
            (unsigned int)((rows * heads * keys + 127u) / 128u), 128u, 0u, score_args, where, err);
        if (rc == YVEX_OK) {
            launches++;
            void *softmax_args[] = {&scores, &heads, &keys, &start, &offset, &causal, &work.status};
            rc = yvex_cuda_launch(backend, work.variant, state->eager_softmax_function,
                (unsigned int)(rows * heads), 128u, 0u, softmax_args, where, err);
        }
        if (rc == YVEX_OK) {
            launches++;
            void *value_args[] = {&scores, &v, &output, &heads, &kvheads, &dim, &keys, &start,
                &offset, &rows, &strides[2], &causal, &work.status};
            rc = yvex_cuda_launch(backend, work.variant, state->eager_value_function,
                (unsigned int)((rows * heads * dim + 127u) / 128u), 128u, 0u, value_args, where, err);
        }
        if (rc == YVEX_OK) { launches++; offset += rows; }
    }
    /* Score scratch is no longer live after this barrier. Preserve any caller's
     * earlier arena prefix; never reset a foreign workspace to zero. */
    int sync_rc = yvex_cuda_launch_synchronize(backend, work.variant, &device_wide, where,
        rc == YVEX_OK ? err : NULL);
    if (rc == YVEX_OK) rc = sync_rc;
    if (rc == YVEX_OK) rc = yvex_cuda_status(&state->driver,
        state->driver.cuMemcpyDtoH_v2(&status, work.status, sizeof(status)), where, err);
    if (rc == YVEX_OK && status) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "invalid BF16 operand, score, probability or attention output");
        rc = YVEX_ERR_FORMAT;
    }
    yvex_error cleanup;
    int cleanup_rc = yvex_cuda_work_cleanup(&work, &cleanup);
    if (sync_rc == YVEX_OK && cleanup_rc == YVEX_OK) backend->workspace_cursor = cursor;
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    if (rc == YVEX_OK) {
        r->output->is_written = 1; facts->kernel_launches = launches;
        facts->device_synchronizations = device_wide != 0; facts->queue_synchronizations = !device_wide;
        facts->download_count = 1u; facts->d2h_bytes = sizeof(status);
        facts->temporary_bytes = scores_count * sizeof(float) + sizeof(status);
        /* Strided views overlap K/V backing. Leave compulsory traffic unavailable
         * rather than adding overlapping spans as independent physical bytes. */
        yvex_error_clear(err);
    }
    return rc;
}

int yvex_cuda_transformer_attention_execute(
    yvex_backend *backend, const yvex_transformer_attention_request *request,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc;
    if (!request || !request->query || !request->key || !request->value ||
        !request->output || !facts) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.transformer.attention",
                       "complete exact-attention tensors and result facts are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = exact_attention_validate(
        &request->requirement, "cuda.transformer.attention", err);
    if (rc != YVEX_OK) return rc;
    if (request->requirement.numeric_contract == YVEX_TRANSFORMER_ATTENTION_NUMERIC_BF16_EAGER)
        return eager_execute(backend, request, facts, err);
    return yvex_cuda_transformer_gqa_strided(
        backend, request->query, request->key, request->value, request->output,
        request->requirement.query_tokens,
        request->requirement.key_value_tokens,
        request->requirement.query_start, request->requirement.query_heads,
        request->requirement.key_value_heads,
        request->requirement.head_dimension,
        request->requirement.query_token_stride,
        request->requirement.key_token_stride,
        request->requirement.value_token_stride,
        request->requirement.mask == YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL,
        facts, err);
}
