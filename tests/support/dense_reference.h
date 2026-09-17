/* Retained source-order CPU composition: preservation, not upstream conformance. */
#ifndef TESTS_SUPPORT_DENSE_REFERENCE_H
#define TESTS_SUPPORT_DENSE_REFERENCE_H
#include "src/graph/private.h"
#include "tests/support/numeric_reference.h"
#include <math.h>
#include <string.h>

static int dense_reference_qk(float *qkv, double epsilon)
{
    for (size_t row = 0u; row < 4u; ++row)
        for (size_t head = 0u; head < 2u; ++head) {
            float *base = qkv + row * 24u + head * 12u;
            if (!yvex_attention_unit_rms_norm(base, 4u, epsilon) ||
                !yvex_attention_unit_rms_norm(base + 4u, 4u, epsilon)) return YVEX_ERR_FORMAT;
        }
    return YVEX_OK;
}

static int dense_reference_residual(float *hidden, const float *delta, const float *scale)
{
    for (size_t i = 0u; i < 32u; ++i) {
        float value = hidden[i] + delta[i] * scale[i % 8u];
        if (!isfinite(value)) return YVEX_ERR_FORMAT;
        hidden[i] = value;
    }
    return YVEX_OK;
}

static int dense_reference(const float *const weights[28], const float *input,
    const float *cosine, const float *sine, double epsilon, float output[12], yvex_error *err)
{
    float hidden[32], normalized[32], qkv[96], attention[32], projected[32];
    float fused[128], gated[64], scratch[4];
    int rc = YVEX_OK;
    memcpy(hidden, input, sizeof(hidden));
    for (size_t block = 0u; block < 2u && rc == YVEX_OK; ++block) {
        const float *const *w = weights + block * 12u;
        memcpy(normalized, hidden, sizeof(hidden));
        for (size_t row = 0u; row < 4u; ++row)
            if (!yvex_attention_rms_norm(normalized + row * 8u, 8u, w[0], epsilon)) return YVEX_ERR_FORMAT;
        rc = test_graph_linear_source_f32(normalized, 32u, 4u, 8u, w[1], 192u, w[2], 24u, 24u, qkv, 96u, err);
        if (rc == YVEX_OK) rc = dense_reference_qk(qkv, epsilon);
        for (size_t row = 0u; row < 4u; ++row)
            for (size_t head = 0u; head < 2u; ++head)
                for (size_t part = 0u; part < 2u; ++part) {
                    float *x = qkv + row * 24u + head * 12u + part * 4u;
                    float a = x[0], b = x[1], c = cosine[row * 2u], s = sine[row * 2u];
                    x[0] = a * c - b * s; x[1] = b * c + a * s;
                }
        if (rc == YVEX_OK) rc = test_graph_full_attention_f32(qkv, 4u, 2u, 4u, attention, scratch, 4u, err);
        if (rc == YVEX_OK) rc = test_graph_linear_source_f32(attention, 32u, 4u, 8u,
            w[3], 64u, w[4], 8u, 8u, projected, 32u, err);
        if (rc == YVEX_OK) rc = dense_reference_residual(hidden, projected, w[5]);
        memcpy(normalized, hidden, sizeof(hidden));
        for (size_t row = 0u; row < 4u; ++row)
            if (!yvex_attention_rms_norm(normalized + row * 8u, 8u, w[6], epsilon)) return YVEX_ERR_FORMAT;
        if (rc == YVEX_OK) rc = test_graph_linear_source_f32(normalized, 32u, 4u, 8u,
            w[7], 256u, w[8], 32u, 32u, fused, 128u, err);
        if (rc == YVEX_OK) rc = test_graph_silu_gate_f32(fused, 4u, 16u, gated, err);
        if (rc == YVEX_OK) rc = test_graph_linear_source_f32(gated, 64u, 4u, 16u,
            w[9], 128u, w[10], 8u, 8u, projected, 32u, err);
        if (rc == YVEX_OK) rc = dense_reference_residual(hidden, projected, w[11]);
    }
    if (rc == YVEX_OK) rc = test_graph_layer_norm_f32(hidden, 4u, 8u, weights[24], weights[25], epsilon, err);
    if (rc == YVEX_OK) rc = test_graph_linear_source_f32(hidden, 24u, 3u, 8u,
        weights[26], 32u, weights[27], 4u, 4u, output, 12u, err);
    return rc;
}
#endif
