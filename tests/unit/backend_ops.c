/*
 * YVEX - backend op tests
 *
 * File: tests/test_backend_ops.c
 * Layer: test
 *
 * Purpose:
 *   Proves that backend layer CPU backend capabilities are explicit and that the minimal
 *   F32 embedding reference op works over backend tensors.
 *
 * Covers:
 *   - yvex_backend_supports
 *   - yvex_backend_capability_name
 *   - yvex_backend_op_embed
 *   - yvex_backend_op_rope
 *   - yvex_backend_op_matmul
 *   - yvex_backend_op_attention
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_ops
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

static void make_desc(yvex_backend_tensor_desc *desc,
                      const char *name,
                      yvex_dtype dtype,
                      unsigned int rank,
                      unsigned long long d0,
                      unsigned long long d1,
                      unsigned long long bytes)
{
    memset(desc, 0, sizeof(*desc));
    desc->name = name;
    desc->dtype = dtype;
    desc->rank = rank;
    desc->dims[0] = d0;
    desc->dims[1] = d1;
    desc->bytes = bytes;
}

static void write_u16le(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
}

static int float_close(float a, float b, float tolerance)
{
    float diff = a - b;
    if (diff < 0.0f) {
        diff = -diff;
    }
    return diff <= tolerance;
}

static int test_capabilities(void)
{
    yvex_backend *backend = NULL;
    yvex_error err;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu");
    YVEX_TEST_ASSERT_STREQ(yvex_backend_capability_name(YVEX_BACKEND_CAP_TENSOR_ALLOC),
                           "tensor_alloc", "capability name");
    YVEX_TEST_ASSERT_STREQ(yvex_backend_capability_name(YVEX_BACKEND_CAP_OP_ROPE),
                           "op_rope", "rope capability name");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC),
                     "supports tensor alloc");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE),
                     "supports tensor read/write");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED),
                     "supports embed");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM),
                     "supports rms norm");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE),
                     "supports rope");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION),
                     "supports attention");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL),
                     "supports matmul");
    yvex_backend_close(backend);
    return 0;
}

static int test_embed_success(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float embedding_data[12] = {
        0.0f, 1.0f, 2.0f, 3.0f,
        10.0f, 11.0f, 12.0f, 13.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
    };
    float out_data[8];
    float expected[8] = {
        0.0f, 1.0f, 2.0f, 3.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
    };
    unsigned int token_ids[2] = {0, 2};
    int rc;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu");

    make_desc(&desc, "embedding", YVEX_DTYPE_F32, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate embedding");
    rc = yvex_backend_tensor_write(backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write embedding");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 2, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate output");

    rc = yvex_backend_op_embed(backend, embedding, token_ids, 2, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "embed succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read embed output");
    YVEX_TEST_ASSERT(memcmp(out_data, expected, sizeof(expected)) == 0, "embed output matches expected rows");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, embedding);
    yvex_backend_close(backend);
    return 0;
}

static int test_embed_f16_success(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    unsigned char embedding_data[24];
    float out_data[8];
    float expected[8] = {
        0.0f, 1.0f, 2.0f, 3.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
    };
    unsigned int token_ids[2] = {0, 2};
    unsigned int half_values[12] = {
        0x0000u, 0x3c00u, 0x4000u, 0x4200u,
        0x4900u, 0x4980u, 0x4a00u, 0x4a80u,
        0x4d00u, 0x4d40u, 0x4d80u, 0x4dc0u,
    };
    unsigned int i;
    int rc;

    for (i = 0; i < 12u; ++i) {
        write_u16le(embedding_data + (i * 2u), half_values[i]);
    }

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu f16");

    make_desc(&desc, "embedding", YVEX_DTYPE_F16, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate f16 embedding");
    rc = yvex_backend_tensor_write(backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write f16 embedding");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 2, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate f16 output");

    rc = yvex_backend_op_embed(backend, embedding, token_ids, 2, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "f16 embed succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read f16 embed output");
    YVEX_TEST_ASSERT(memcmp(out_data, expected, sizeof(expected)) == 0,
                     "f16 embed output converts expected rows");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, embedding);
    yvex_backend_close(backend);
    return 0;
}

static int test_embed_failures(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float embedding_data[12] = {0};
    unsigned int good_ids[1] = {0};
    unsigned int bad_ids[1] = {9};
    int rc;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu failures");

    make_desc(&desc, "embedding", YVEX_DTYPE_F32, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate embedding failures");
    rc = yvex_backend_tensor_write(backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write embedding failures");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 1, 4, 4u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate output failures");

    rc = yvex_backend_op_embed(backend, embedding, bad_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad token id fails");

    yvex_backend_tensor_free(backend, out);
    make_desc(&desc, "bad_out", YVEX_DTYPE_F32, 2, 4, 1, 4u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate bad output");
    rc = yvex_backend_op_embed(backend, embedding, good_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad output shape fails");
    yvex_backend_tensor_free(backend, out);
    out = NULL;

    yvex_backend_tensor_free(backend, embedding);
    make_desc(&desc, "bad_embedding", YVEX_DTYPE_BF16, 2, 4, 3, 24);
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate bf16 embedding");
    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 1, 4, 4u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate bf16 output");
    rc = yvex_backend_op_embed(backend, embedding, good_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "unsupported embedding dtype rejected");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, embedding);
    yvex_backend_close(backend);
    return 0;
}

static int test_rms_norm_success(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *weight = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float input_data[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    unsigned char weight_data[8];
    float out_data[4];
    float expected[4] = {0.9999995f, 1.9999990f, 2.9999985f, 3.9999980f};
    unsigned int half_values[4] = {0x3c00u, 0x4000u, 0x4200u, 0x4400u};
    unsigned int i;
    int rc;

    for (i = 0; i < 4u; ++i) {
        write_u16le(weight_data + (i * 2u), half_values[i]);
    }

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu rms");

    make_desc(&desc, "input", YVEX_DTYPE_F32, 2, 1, 4, sizeof(input_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate rms input");
    rc = yvex_backend_tensor_write(backend, input, input_data, sizeof(input_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write rms input");

    make_desc(&desc, "weight", YVEX_DTYPE_F16, 1, 4, 0, sizeof(weight_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate rms weight");
    rc = yvex_backend_tensor_write(backend, weight, weight_data, sizeof(weight_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write rms weight");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 1, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate rms output");

    rc = yvex_backend_op_rms_norm(backend, input, weight, 0.000001f, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "rms norm succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read rms output");
    for (i = 0; i < 4u; ++i) {
        YVEX_TEST_ASSERT(float_close(out_data[i], expected[i], 0.0001f), "rms output matches expected");
    }

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, weight);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    return 0;
}

static int test_rope_success(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float input_data[8] = {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    float out_data[8];
    unsigned int i;
    int rc;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu rope");

    make_desc(&desc, "rope_input", YVEX_DTYPE_F32, 1, 8, 0, sizeof(input_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate rope input");
    rc = yvex_backend_tensor_write(backend, input, input_data, sizeof(input_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write rope input");

    make_desc(&desc, "rope_out", YVEX_DTYPE_F32, 1, 8, 0, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate rope output");

    rc = yvex_backend_op_rope(backend, input, 0, 10000.0f, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "rope position zero succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read rope zero output");
    for (i = 0; i < 8u; ++i) {
        YVEX_TEST_ASSERT(float_close(out_data[i], input_data[i], 0.000001f),
                         "rope position zero is identity");
    }

    rc = yvex_backend_op_rope(backend, input, 7, 10000.0f, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "rope position seven succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read rope output");
    YVEX_TEST_ASSERT(!float_close(out_data[0], input_data[0], 0.0001f),
                     "rope non-zero position changes first pair");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    return 0;
}

static int test_rope_failures(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float input_data[7] = {0};
    float out_data[7] = {0};
    int rc;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu rope failures");

    make_desc(&desc, "rope_bad_input", YVEX_DTYPE_F32, 1, 7, 0, sizeof(input_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate odd rope input");
    rc = yvex_backend_tensor_write(backend, input, input_data, sizeof(input_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write odd rope input");

    make_desc(&desc, "rope_bad_out", YVEX_DTYPE_F32, 1, 7, 0, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate odd rope output");
    rc = yvex_backend_op_rope(backend, input, 7, 10000.0f, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT, "odd head dim rejected");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    return 0;
}

static int test_attention_success_and_bounds(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *query = NULL;
    yvex_device_tensor *keys = NULL;
    yvex_device_tensor *values = NULL;
    yvex_device_tensor *scores = NULL;
    yvex_device_tensor *probabilities = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float query_data[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float key_data[12] = {
        0.2f, 0.1f, 0.0f, 0.3f,
        0.4f, 0.3f, 0.2f, 0.1f,
        0.1f, 0.5f, 0.3f, 0.2f,
    };
    float value_data[12] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        10.0f, 20.0f, 30.0f, 40.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
    };
    float out_data[4];
    float score_data[3];
    float probability_data[3];
    int rc;
    unsigned int i;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu attention");

    make_desc(&desc, "attention_query", YVEX_DTYPE_F32, 1, 4, 0, sizeof(query_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &query, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate attention query");
    rc = yvex_backend_tensor_write(backend, query, query_data, sizeof(query_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write attention query");

    make_desc(&desc, "attention_keys", YVEX_DTYPE_F32, 2, 3, 4, sizeof(key_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &keys, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate attention keys");
    rc = yvex_backend_tensor_write(backend, keys, key_data, sizeof(key_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write attention keys");

    make_desc(&desc, "attention_values", YVEX_DTYPE_F32, 2, 3, 4, sizeof(value_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &values, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate attention values");
    rc = yvex_backend_tensor_write(backend, values, value_data, sizeof(value_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write attention values");

    make_desc(&desc, "attention_scores", YVEX_DTYPE_F32, 1, 3, 0, sizeof(score_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &scores, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate attention scores");
    make_desc(&desc, "attention_probabilities", YVEX_DTYPE_F32, 1, 3, 0, sizeof(probability_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &probabilities, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate attention probabilities");
    make_desc(&desc, "attention_out", YVEX_DTYPE_F32, 1, 4, 0, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate attention output");

    rc = yvex_backend_op_attention(backend, query, keys, values, 3, 0, 0.5f, 1,
                                   scores, probabilities, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "attention position zero succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read attention output");
    for (i = 0; i < 4u; ++i) {
        YVEX_TEST_ASSERT(float_close(out_data[i], value_data[i], 0.000001f),
                         "attention position zero selects first value");
    }

    rc = yvex_backend_op_attention(backend, query, keys, values, 3, 2, 0.5f, 1,
                                   scores, probabilities, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "attention full prefix succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read attention prefix output");
    YVEX_TEST_ASSERT(!float_close(out_data[0], value_data[0], 0.0001f),
                     "attention full prefix mixes more than first value");

    rc = yvex_backend_op_attention(backend, query, keys, values, 3, 3, 0.5f, 1,
                                   scores, probabilities, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "attention rejects out-of-range position");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, probabilities);
    yvex_backend_tensor_free(backend, scores);
    yvex_backend_tensor_free(backend, values);
    yvex_backend_tensor_free(backend, keys);
    yvex_backend_tensor_free(backend, query);
    yvex_backend_close(backend);
    return 0;
}

static int test_matmul_success_and_failures(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *weight = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float projection_input[8] = {0.08f, 0.09f, 0.10f, 0.11f, 0.12f, 0.13f, 0.14f, 0.15f};
    float projection_weight[64];
    float projection_out[8];
    float projection_expected[8];
    float matrix_input[8] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
    };
    float matrix_weight[12] = {
        0.5f, 1.0f, 1.5f,
        2.0f, 2.5f, 3.0f,
        3.5f, 4.0f, 4.5f,
        5.0f, 5.5f, 6.0f,
    };
    float matrix_out[6];
    float matrix_expected[6];
    unsigned int i;
    unsigned int row;
    unsigned int col;
    int rc;

    for (i = 0; i < 64u; ++i) {
        projection_weight[i] = (float)(0.01 + ((double)(i + 1u) * 0.001));
    }
    for (col = 0; col < 8u; ++col) {
        double sum = 0.0;
        for (i = 0; i < 8u; ++i) {
            sum += (double)projection_input[i] * (double)projection_weight[(i * 8u) + col];
        }
        projection_expected[col] = (float)sum;
    }
    for (row = 0; row < 2u; ++row) {
        for (col = 0; col < 3u; ++col) {
            double sum = 0.0;
            for (i = 0; i < 4u; ++i) {
                sum += (double)matrix_input[(row * 4u) + i] *
                       (double)matrix_weight[(i * 3u) + col];
            }
            matrix_expected[(row * 3u) + col] = (float)sum;
        }
    }

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu matmul");

    make_desc(&desc, "matmul_projection_input", YVEX_DTYPE_F32, 2, 1, 8, sizeof(projection_input));
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate projection input");
    rc = yvex_backend_tensor_write(backend, input, projection_input, sizeof(projection_input), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write projection input");
    make_desc(&desc, "matmul_projection_weight", YVEX_DTYPE_F32, 2, 8, 8, sizeof(projection_weight));
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate projection weight");
    rc = yvex_backend_tensor_write(backend, weight, projection_weight, sizeof(projection_weight), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write projection weight");
    make_desc(&desc, "matmul_projection_out", YVEX_DTYPE_F32, 2, 1, 8, sizeof(projection_out));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate projection output");

    rc = yvex_backend_op_matmul(backend, input, weight, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "projection matmul succeeds");
    rc = yvex_backend_tensor_read(backend, out, projection_out, sizeof(projection_out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read projection output");
    for (i = 0; i < 8u; ++i) {
        YVEX_TEST_ASSERT(float_close(projection_out[i], projection_expected[i], 0.000001f),
                         "projection matmul output matches reference");
    }

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, weight);
    yvex_backend_tensor_free(backend, input);
    input = NULL;
    weight = NULL;
    out = NULL;

    make_desc(&desc, "matmul_matrix_input", YVEX_DTYPE_F32, 2, 2, 4, sizeof(matrix_input));
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate matrix input");
    rc = yvex_backend_tensor_write(backend, input, matrix_input, sizeof(matrix_input), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write matrix input");
    make_desc(&desc, "matmul_matrix_weight", YVEX_DTYPE_F32, 2, 4, 3, sizeof(matrix_weight));
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate matrix weight");
    rc = yvex_backend_tensor_write(backend, weight, matrix_weight, sizeof(matrix_weight), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write matrix weight");
    make_desc(&desc, "matmul_matrix_out", YVEX_DTYPE_F32, 2, 2, 3, sizeof(matrix_out));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate matrix output");

    rc = yvex_backend_op_matmul(backend, input, weight, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "matrix matmul succeeds");
    rc = yvex_backend_tensor_read(backend, out, matrix_out, sizeof(matrix_out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read matrix output");
    for (i = 0; i < 6u; ++i) {
        YVEX_TEST_ASSERT(float_close(matrix_out[i], matrix_expected[i], 0.000001f),
                         "matrix matmul output matches reference");
    }

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, weight);
    yvex_backend_tensor_free(backend, input);
    input = NULL;
    weight = NULL;
    out = NULL;

    make_desc(&desc, "matmul_bad_input", YVEX_DTYPE_BF16, 2, 1, 2, 4);
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate unsupported matmul input dtype");
    make_desc(&desc, "matmul_bad_weight", YVEX_DTYPE_F32, 2, 2, 1, 2u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate matmul bad dtype weight");
    make_desc(&desc, "matmul_bad_out", YVEX_DTYPE_F32, 2, 1, 1, sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate matmul bad dtype output");
    rc = yvex_backend_op_matmul(backend, input, weight, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "matmul rejects unsupported dtype");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, weight);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    return 0;
}

int yvex_test_backend_ops(void)
{
    if (test_capabilities() != 0) return 1;
    if (test_embed_success() != 0) return 1;
    if (test_embed_f16_success() != 0) return 1;
    if (test_rms_norm_success() != 0) return 1;
    if (test_rope_success() != 0) return 1;
    if (test_rope_failures() != 0) return 1;
    if (test_attention_success_and_bounds() != 0) return 1;
    if (test_matmul_success_and_failures() != 0) return 1;
    if (test_embed_failures() != 0) return 1;
    return 0;
}
