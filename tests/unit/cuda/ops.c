/*
 * YVEX - CUDA op tests
 *
 * File: tests/test_cuda_ops.c
 * Layer: test
 *
 * Purpose:
 *   Proves that CUDA backend CUDA supports the same minimal F32 embedding op as the backend layer
 *   CPU reference backend. Returns 77 when CUDA is unavailable.
 */
#include <stdio.h>
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

static int open_cuda_or_skip(yvex_backend **out)
{
    yvex_backend_options options;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(out, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        fprintf(stderr, "SKIP: CUDA unavailable: %s\n", yvex_error_message(&err));
        return 77;
    }
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open cuda backend");
    return 0;
}

int yvex_cuda_test_ops(void)
{
    yvex_backend *cuda_backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *rms_input = NULL;
    yvex_device_tensor *rms_weight = NULL;
    yvex_device_tensor *rms_out = NULL;
    yvex_device_tensor *rope_input = NULL;
    yvex_device_tensor *rope_out = NULL;
    yvex_device_tensor *attention_query = NULL;
    yvex_device_tensor *attention_keys = NULL;
    yvex_device_tensor *attention_values = NULL;
    yvex_device_tensor *attention_scores = NULL;
    yvex_device_tensor *attention_probabilities = NULL;
    yvex_device_tensor *attention_out = NULL;
    yvex_device_tensor *matmul_input = NULL;
    yvex_device_tensor *matmul_weight = NULL;
    yvex_device_tensor *matmul_out = NULL;
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
    unsigned int bad_ids[1] = {9};
    float rms_input_data[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    unsigned char rms_weight_data[8];
    float rms_out_data[4];
    float rms_expected[4] = {0.9999995f, 1.9999990f, 2.9999985f, 3.9999980f};
    float rope_input_data[8] = {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    float rope_out_data[8];
    float attention_query_data[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float attention_key_data[12] = {
        0.2f, 0.1f, 0.0f, 0.3f,
        0.4f, 0.3f, 0.2f, 0.1f,
        0.1f, 0.5f, 0.3f, 0.2f,
    };
    float attention_value_data[12] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        10.0f, 20.0f, 30.0f, 40.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
    };
    float attention_out_data[4];
    float attention_score_data[3];
    float attention_probability_data[3];
    float matmul_input_data[8] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
    };
    float matmul_weight_data[12] = {
        0.5f, 1.0f, 1.5f,
        2.0f, 2.5f, 3.0f,
        3.5f, 4.0f, 4.5f,
        5.0f, 5.5f, 6.0f,
    };
    float matmul_out_data[6];
    float matmul_expected[6];
    unsigned int rms_half_values[4] = {0x3c00u, 0x4000u, 0x4200u, 0x4400u};
    unsigned int i;
    unsigned int row;
    unsigned int col;
    int rc;

    for (i = 0; i < 4u; ++i) {
        write_u16le(rms_weight_data + (i * 2u), rms_half_values[i]);
    }
    for (row = 0; row < 2u; ++row) {
        for (col = 0; col < 3u; ++col) {
            double sum = 0.0;
            for (i = 0; i < 4u; ++i) {
                sum += (double)matmul_input_data[(row * 4u) + i] *
                       (double)matmul_weight_data[(i * 3u) + col];
            }
            matmul_expected[(row * 3u) + col] = (float)sum;
        }
    }

    rc = open_cuda_or_skip(&cuda_backend);
    if (rc == 77) return 77;

    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_TENSOR_ALLOC),
                     "cuda tensor alloc supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE),
                     "cuda tensor rw supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_EMBED),
                     "cuda embed supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_RMS_NORM),
                     "cuda rmsnorm supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_ROPE),
                     "cuda rope supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_ATTENTION),
                     "cuda attention supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_MATMUL),
                     "cuda matmul supported");

    make_desc(&desc, "embedding", YVEX_DTYPE_F32, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda embedding");
    rc = yvex_backend_tensor_write(cuda_backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda embedding");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 2, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda output");
    rc = yvex_backend_op_embed(cuda_backend, embedding, token_ids, 2, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda embed succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda embed output");
    YVEX_TEST_ASSERT(memcmp(out_data, expected, sizeof(expected)) == 0,
                     "cuda embed output matches expected rows");

    rc = yvex_backend_op_embed(cuda_backend, embedding, bad_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad token id fails");

    make_desc(&desc, "rms_input", YVEX_DTYPE_F32, 2, 1, 4, sizeof(rms_input_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &rms_input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda rms input");
    rc = yvex_backend_tensor_write(cuda_backend, rms_input, rms_input_data, sizeof(rms_input_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda rms input");

    make_desc(&desc, "rms_weight", YVEX_DTYPE_F16, 1, 4, 0, sizeof(rms_weight_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &rms_weight, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda rms weight");
    rc = yvex_backend_tensor_write(cuda_backend, rms_weight, rms_weight_data, sizeof(rms_weight_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda rms weight");

    make_desc(&desc, "rms_out", YVEX_DTYPE_F32, 2, 1, 4, sizeof(rms_out_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &rms_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda rms output");
    rc = yvex_backend_op_rms_norm(cuda_backend, rms_input, rms_weight, 0.000001f, rms_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda rms norm succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, rms_out, rms_out_data, sizeof(rms_out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda rms output");
    for (i = 0; i < 4u; ++i) {
        YVEX_TEST_ASSERT(float_close(rms_out_data[i], rms_expected[i], 0.0001f),
                         "cuda rms output matches expected");
    }

    make_desc(&desc, "rope_input", YVEX_DTYPE_F32, 1, 8, 0, sizeof(rope_input_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &rope_input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda rope input");
    rc = yvex_backend_tensor_write(cuda_backend, rope_input, rope_input_data, sizeof(rope_input_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda rope input");

    make_desc(&desc, "rope_out", YVEX_DTYPE_F32, 1, 8, 0, sizeof(rope_out_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &rope_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda rope output");
    rc = yvex_backend_op_rope(cuda_backend, rope_input, 0, 10000.0f, rope_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda rope position zero succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, rope_out, rope_out_data, sizeof(rope_out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda rope zero output");
    for (i = 0; i < 8u; ++i) {
        YVEX_TEST_ASSERT(float_close(rope_out_data[i], rope_input_data[i], 0.0001f),
                         "cuda rope position zero is identity");
    }

    rc = yvex_backend_op_rope(cuda_backend, rope_input, 7, 10000.0f, rope_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda rope position seven succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, rope_out, rope_out_data, sizeof(rope_out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda rope output");
    YVEX_TEST_ASSERT(!float_close(rope_out_data[0], rope_input_data[0], 0.0001f),
                     "cuda rope non-zero position changes first pair");

    make_desc(&desc, "attention_query", YVEX_DTYPE_F32, 1, 4, 0, sizeof(attention_query_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &attention_query, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda attention query");
    rc = yvex_backend_tensor_write(cuda_backend, attention_query,
                                   attention_query_data, sizeof(attention_query_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda attention query");

    make_desc(&desc, "attention_keys", YVEX_DTYPE_F32, 2, 3, 4, sizeof(attention_key_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &attention_keys, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda attention keys");
    rc = yvex_backend_tensor_write(cuda_backend, attention_keys,
                                   attention_key_data, sizeof(attention_key_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda attention keys");

    make_desc(&desc, "attention_values", YVEX_DTYPE_F32, 2, 3, 4, sizeof(attention_value_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &attention_values, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda attention values");
    rc = yvex_backend_tensor_write(cuda_backend, attention_values,
                                   attention_value_data, sizeof(attention_value_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda attention values");

    make_desc(&desc, "attention_scores", YVEX_DTYPE_F32, 1, 3, 0, sizeof(attention_score_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &attention_scores, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda attention scores");
    make_desc(&desc, "attention_probabilities", YVEX_DTYPE_F32, 1, 3, 0, sizeof(attention_probability_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &attention_probabilities, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda attention probabilities");
    make_desc(&desc, "attention_out", YVEX_DTYPE_F32, 1, 4, 0, sizeof(attention_out_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &attention_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda attention output");

    rc = yvex_backend_op_attention(cuda_backend, attention_query, attention_keys,
                                   attention_values, 3, 0, 0.5f, 1,
                                   attention_scores, attention_probabilities,
                                   attention_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda attention position zero succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, attention_out,
                                  attention_out_data, sizeof(attention_out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda attention output");
    for (i = 0; i < 4u; ++i) {
        YVEX_TEST_ASSERT(float_close(attention_out_data[i], attention_value_data[i], 0.0001f),
                         "cuda attention position zero selects first value");
    }

    rc = yvex_backend_op_attention(cuda_backend, attention_query, attention_keys,
                                   attention_values, 3, 2, 0.5f, 1,
                                   attention_scores, attention_probabilities,
                                   attention_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda attention full prefix succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, attention_out,
                                  attention_out_data, sizeof(attention_out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda attention prefix output");
    YVEX_TEST_ASSERT(!float_close(attention_out_data[0], attention_value_data[0], 0.0001f),
                     "cuda attention full prefix mixes more than first value");

    rc = yvex_backend_op_attention(cuda_backend, attention_query, attention_keys,
                                   attention_values, 3, 3, 0.5f, 1,
                                   attention_scores, attention_probabilities,
                                   attention_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "cuda attention rejects out-of-range position");

    make_desc(&desc, "matmul_input", YVEX_DTYPE_F32, 2, 2, 4, sizeof(matmul_input_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &matmul_input, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda matmul input");
    rc = yvex_backend_tensor_write(cuda_backend, matmul_input,
                                   matmul_input_data, sizeof(matmul_input_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda matmul input");
    make_desc(&desc, "matmul_weight", YVEX_DTYPE_F32, 2, 4, 3, sizeof(matmul_weight_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &matmul_weight, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda matmul weight");
    rc = yvex_backend_tensor_write(cuda_backend, matmul_weight,
                                   matmul_weight_data, sizeof(matmul_weight_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda matmul weight");
    make_desc(&desc, "matmul_out", YVEX_DTYPE_F32, 2, 2, 3, sizeof(matmul_out_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &matmul_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda matmul output");
    rc = yvex_backend_op_matmul(cuda_backend, matmul_input, matmul_weight, matmul_out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda matmul succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, matmul_out,
                                  matmul_out_data, sizeof(matmul_out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda matmul output");
    for (i = 0; i < 6u; ++i) {
        YVEX_TEST_ASSERT(float_close(matmul_out_data[i], matmul_expected[i], 0.0001f),
                         "cuda matmul output matches reference");
    }

    yvex_backend_tensor_free(cuda_backend, matmul_out);
    yvex_backend_tensor_free(cuda_backend, matmul_weight);
    yvex_backend_tensor_free(cuda_backend, matmul_input);
    yvex_backend_tensor_free(cuda_backend, attention_out);
    yvex_backend_tensor_free(cuda_backend, attention_probabilities);
    yvex_backend_tensor_free(cuda_backend, attention_scores);
    yvex_backend_tensor_free(cuda_backend, attention_values);
    yvex_backend_tensor_free(cuda_backend, attention_keys);
    yvex_backend_tensor_free(cuda_backend, attention_query);
    yvex_backend_tensor_free(cuda_backend, rope_out);
    yvex_backend_tensor_free(cuda_backend, rope_input);
    yvex_backend_tensor_free(cuda_backend, rms_out);
    yvex_backend_tensor_free(cuda_backend, rms_weight);
    yvex_backend_tensor_free(cuda_backend, rms_input);
    yvex_backend_tensor_free(cuda_backend, out);
    yvex_backend_tensor_free(cuda_backend, embedding);
    yvex_backend_close(cuda_backend);
    return 0;
}
