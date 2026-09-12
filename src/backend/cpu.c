/*
 * Provide the independently compiled CPU implementation selected by the generic backend vtable.
 *
 * Every tensor is tied to one backend instance; checked memory accounting precedes allocation;
 * failed operations preserve output state. These primitives prove bounded CPU execution, not
 * transformer or generation support.
 */

#include <yvex/internal/backend.h>
#include "src/backend/private.h"
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/neural_operations.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int cpu_tensor_alloc(yvex_backend *, const yvex_backend_tensor_desc *,
                                 yvex_device_tensor **, yvex_error *);
static int cpu_tensor_free(yvex_backend *, yvex_device_tensor *, yvex_error *);
static int cpu_tensor_write(yvex_backend *, yvex_device_tensor *, const void *,
                                 unsigned long long, yvex_error *);
static int cpu_tensor_read(yvex_backend *, const yvex_device_tensor *, void *,
                                unsigned long long, yvex_error *);
static int cpu_tensor_zero(yvex_backend *, yvex_device_tensor *, yvex_error *);
static int cpu_tensor_copy(yvex_backend *, yvex_device_tensor *,
                                const yvex_device_tensor *, yvex_error *);
static int cpu_op_embed(yvex_backend *, const yvex_device_tensor *, const unsigned int *,
                             unsigned long long, yvex_device_tensor *, yvex_error *);
static int cpu_op_rms_norm(yvex_backend *, const yvex_device_tensor *,
                                const yvex_device_tensor *, float, yvex_device_tensor *,
                                yvex_error *);
static int cpu_op_rope(yvex_backend *, const yvex_device_tensor *, unsigned long long,
                            float, yvex_device_tensor *, yvex_error *);
static int cpu_op_matmul(yvex_backend *, const yvex_device_tensor *,
                              const yvex_device_tensor *, yvex_device_tensor *, yvex_error *);
static int cpu_op_mlp(yvex_backend *, const yvex_device_tensor *,
                           const yvex_device_tensor *, const yvex_device_tensor *,
                           const yvex_device_tensor *, const yvex_mlp_options *,
                           yvex_device_tensor *, yvex_device_tensor *, yvex_error *);
static int cpu_op_attention(yvex_backend *, const yvex_device_tensor *,
                                 const yvex_device_tensor *, const yvex_device_tensor *,
                                 unsigned long long, unsigned long long, float, int,
                                 yvex_device_tensor *, yvex_device_tensor *,
                                 yvex_device_tensor *, yvex_error *);

static int cpu_memory_stats(const yvex_backend *backend,
                            yvex_backend_memory_stats *out,
                            yvex_error *err)
{
    *out = backend->stats;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_device_info(const yvex_backend *backend,
                           yvex_backend_device_info *out,
                           yvex_error *err)
{
    *out = backend->device_info;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_sync(yvex_backend *backend, yvex_error *err)
{
    (void)backend;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_query_capability(const yvex_backend *backend,
                                yvex_backend_operation_variant variant,
                                yvex_backend_capability_result *out,
                                yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.query_capability",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (variant < 0 || variant >= YVEX_BACKEND_VARIANT_COUNT) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.query_capability",
                       "operation variant is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    out->state = YVEX_BACKEND_CAPABILITY_SUPPORTED;
    out->reason = YVEX_BACKEND_CAPABILITY_REASON_NONE;
    out->context_available = 1;
    out->function_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* One admitted numerical operation, not a model/decoder plan. Stream geometry
 * and both rounding points are supplied by the verified computational program. */
static int cpu_mhc_head(yvex_backend *backend, const yvex_device_tensor *expanded,
    const yvex_device_tensor *function, const yvex_device_tensor *base,
    const yvex_device_tensor *scale, const yvex_device_tensor *norm,
    unsigned long long rows, unsigned long long width, unsigned long long streams,
    double epsilon, double mhc_epsilon, yvex_device_tensor *pre, yvex_device_tensor *output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_device_tensor *tensors[] = {expanded, function, base, scale, norm, pre, output};
    unsigned long long sizes[7], expanded_width, row, stream, lane, i;
    const float *fn, *bias, *gain, *weights;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (output) output->is_written = 0;
    if (pre) pre->is_written = 0;
    if (!facts || !rows || !width || !streams || !isfinite(epsilon) || epsilon <= 0.0 ||
        !isfinite(mhc_epsilon) || mhc_epsilon <= 0.0 ||
        !yvex_core_u64_mul(width, streams, &expanded_width) ||
        !yvex_core_u64_mul(rows, expanded_width, &sizes[0]) ||
        !yvex_core_u64_mul(streams, expanded_width, &sizes[1]) ||
        !yvex_core_u64_mul(rows, width, &sizes[5])) goto invalid;
    sizes[2] = streams; sizes[3] = 1u; sizes[4] = width; sizes[6] = sizes[5];
    for (i = 0u; i < 7u; ++i) {
        if (i == 5u && !pre) continue;
        if (!backend_tensor_owner_is(backend, tensors[i]) || !backend_tensor_f32_elements(tensors[i], sizes[i]) ||
            (i < 5u && !tensors[i]->is_written)) goto invalid;
    }
    if (pre && pre->data == output->data) goto invalid;
    fn = (const float *)function->data; bias = (const float *)base->data;
    gain = (const float *)scale->data; weights = (const float *)norm->data;
    for (row = 0u; row < rows; ++row) {
        const float *x = (const float *)expanded->data + row * expanded_width;
        float *y = (float *)output->data + row * width;
        double squares = 0.0, inverse;
        for (i = 0u; i < expanded_width; ++i) squares += (double)x[i] * (double)x[i];
        inverse = 1.0 / sqrt(squares / (double)expanded_width + epsilon);
        if (!isfinite(inverse)) goto numeric;
        memset(y, 0, (size_t)width * sizeof(float));
        for (stream = 0u; stream < streams; ++stream) {
            double mix = 0.0, coefficient;
            for (i = 0u; i < expanded_width; ++i) mix += (double)fn[stream * expanded_width + i] * (double)x[i];
            coefficient = 1.0 / (1.0 + exp(-(mix * inverse * (double)gain[0] + (double)bias[stream])));
            coefficient += mhc_epsilon;
            for (lane = 0u; lane < width; ++lane)
                y[lane] += (float)(coefficient * (double)x[stream * width + lane]);
        }
        squares = 0.0;
        for (lane = 0u; lane < width; ++lane) {
            if (!isfinite(y[lane]) || !isfinite(weights[lane])) goto numeric;
            y[lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(y[lane]));
            squares += (double)y[lane] * (double)y[lane];
        }
        if (pre) memcpy((float *)pre->data + row * width, y, (size_t)width * sizeof(float));
        inverse = 1.0 / sqrt(squares / (double)width + epsilon);
        for (lane = 0u; lane < width; ++lane) {
            double value = (double)y[lane] * inverse * (double)weights[lane];
            if (!isfinite(value) || !isfinite((float)value)) goto numeric;
            y[lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)value));
        }
    }
    output->is_written = 1;
    if (pre) pre->is_written = 1;
    facts->active_weight_bytes = function->bytes + base->bytes + scale->bytes + norm->bytes;
    facts->activation_bytes = expanded->bytes + output->bytes + (pre ? pre->bytes : 0u);
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.mhc-head", "mHC head tensors or geometry are incompatible");
    return YVEX_ERR_FORMAT;
numeric:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.mhc-head", "mHC head produced non-finite values");
    return YVEX_ERR_FORMAT;
}

static int cpu_stream_mean(yvex_backend *backend, const yvex_device_tensor *input,
    unsigned long long rows, unsigned long long width, unsigned long long streams,
    yvex_device_tensor *output, yvex_device_tensor *resident, unsigned long long row_offset,
    unsigned long long row_stride, unsigned long long column_offset, float *host_output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long count, input_count, row, lane, stream;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (output) output->is_written = 0;
    if (!facts || !rows || !width || !streams || resident || row_offset || row_stride || column_offset ||
        host_output || !yvex_core_u64_mul(rows, width, &count) ||
        !yvex_core_u64_mul(count, streams, &input_count) || !backend_tensor_owner_is(backend, input) ||
        !backend_tensor_f32_elements(input, input_count) || !input->is_written ||
        !backend_tensor_owner_is(backend, output) || !backend_tensor_f32_elements(output, count) ||
        input->data == output->data) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.stream-mean", "incompatible stream reduction operands");
        return YVEX_ERR_FORMAT;
    }
    for (row = 0u; row < rows; ++row)
        for (lane = 0u; lane < width; ++lane) {
            double sum = 0.0;
            for (stream = 0u; stream < streams; ++stream)
                sum += ((const float *)input->data)[(row * streams + stream) * width + lane];
            float value = (float)(sum / (double)streams);
            if (!isfinite(value)) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.stream-mean", "stream mean produced a non-finite value");
                return YVEX_ERR_FORMAT;
            }
            ((float *)output->data)[row * width + lane] = value;
        }
    output->is_written = 1;
    facts->activation_bytes = input->bytes + output->bytes;
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_residual_post(yvex_backend *backend, const yvex_device_tensor *residual,
    const yvex_device_tensor *core, const yvex_device_tensor *post, const yvex_device_tensor *mix,
    unsigned long long rows, unsigned long long streams, unsigned long long width,
    yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_device_tensor *inputs[] = {residual, core, post, mix};
    unsigned long long sizes[4], total, row, target, lane, source;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (output) output->is_written = 0;
    if (!facts || !rows || !streams || !width ||
        !yvex_core_u64_mul(rows, width, &sizes[1]) || !yvex_core_u64_mul(rows, streams, &sizes[2]) ||
        !yvex_core_u64_mul(sizes[1], streams, &sizes[0]) || !yvex_core_u64_mul(sizes[2], streams, &sizes[3]) ||
        !backend_tensor_owner_is(backend, output) || !backend_tensor_f32_elements(output, sizes[0])) goto invalid;
    total = sizes[0];
    for (size_t i = 0u; i < 4u; ++i)
        if (!backend_tensor_owner_is(backend, inputs[i]) || !backend_tensor_f32_elements(inputs[i], sizes[i]) ||
            !inputs[i]->is_written || inputs[i]->data == output->data ||
            !yvex_core_u64_add(total, sizes[i], &total)) goto invalid;
    if (!yvex_core_u64_mul(total, sizeof(float), &total)) goto invalid;
    for (row = 0u; row < rows; ++row)
        for (target = 0u; target < streams; ++target)
            for (lane = 0u; lane < width; ++lane) {
                double value = (double)((const float *)post->data)[row * streams + target] *
                    ((const float *)core->data)[row * width + lane];
                for (source = 0u; source < streams; ++source)
                    value += (double)((const float *)mix->data)[(row * streams + source) * streams + target] *
                        ((const float *)residual->data)[(row * streams + source) * width + lane];
                if (!isfinite((float)value)) goto invalid;
                ((float *)output->data)[(row * streams + target) * width + lane] =
                    yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)value));
            }
    output->is_written = 1;
    facts->activation_bytes = total;
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.mhc-post", "invalid mHC post operands or non-finite result");
    return YVEX_ERR_FORMAT;
}

static const yvex_backend_transformer_operations *cpu_transformer_operations(const yvex_backend *backend)
{
    static const yvex_backend_transformer_operations operations = {
        .feature_mean = cpu_stream_mean, .final = cpu_mhc_head, .residual_post = cpu_residual_post};
    (void)backend;
    return &operations;
}

static const yvex_backend_vtable cpu_vtable = {
    .memory_stats = cpu_memory_stats,
    .device_info = cpu_device_info,
    .tensor_alloc = cpu_tensor_alloc,
    .tensor_free = cpu_tensor_free,
    .tensor_write = cpu_tensor_write,
    .tensor_read = cpu_tensor_read,
    .tensor_zero = cpu_tensor_zero,
    .tensor_copy = cpu_tensor_copy,
    .sync = cpu_sync,
    .query_capability = cpu_query_capability,
    .op_embed = cpu_op_embed,
    .op_rms_norm = cpu_op_rms_norm,
    .op_rope = cpu_op_rope,
    .op_matmul = cpu_op_matmul,
    .op_mlp = cpu_op_mlp,
    .op_attention = cpu_op_attention,
    .transformer_operations = cpu_transformer_operations,
};

int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err)
{
    yvex_backend *backend;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_open_cpu", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    backend = (yvex_backend *)calloc(1, sizeof(*backend));
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_backend_open_cpu",
                       "failed to allocate CPU backend");
        return YVEX_ERR_NOMEM;
    }
    backend->kind = YVEX_BACKEND_KIND_CPU;
    atomic_init(&backend->status, YVEX_BACKEND_STATUS_READY);
    backend->vtable = &cpu_vtable;
    backend->resource_owner = backend;
    atomic_init(&backend->lifecycle, 0ull);
    backend->device_info.kind = YVEX_BACKEND_KIND_CPU;
    backend->device_info.name = "cpu";
    backend->tensor_id_next = 1;

    *out = backend;
    yvex_error_clear(err);
    return YVEX_OK;
}

static float cpu_decode_f16(const unsigned char *bytes)
{
    unsigned short bits = (unsigned short)bytes[0] | (unsigned short)((unsigned short)bytes[1] << 8);

    return yvex_quant_f16_decode(bits);
}

static double backend_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

static double backend_abs_double(double x)
{
    return x < 0.0 ? -x : x;
}

static double backend_wrap_radians(double x)
{
    const double two_pi = 6.28318530717958647692;

    while (x > 3.14159265358979323846) {
        x -= two_pi;
    }
    while (x < -3.14159265358979323846) {
        x += two_pi;
    }
    return x;
}

static void backend_sincos_double(double x, double *sine, double *cosine)
{
    double x2;
    double s;
    double c;

    x = backend_wrap_radians(x);
    x2 = x * x;
    s = x * (1.0 -
             (x2 / 6.0) +
             ((x2 * x2) / 120.0) -
             ((x2 * x2 * x2) / 5040.0) +
             ((x2 * x2 * x2 * x2) / 362880.0));
    c = 1.0 -
        (x2 / 2.0) +
        ((x2 * x2) / 24.0) -
        ((x2 * x2 * x2) / 720.0) +
        ((x2 * x2 * x2 * x2) / 40320.0);
    if (backend_abs_double(s) < 0.000000000001) {
        s = 0.0;
    }
    if (backend_abs_double(c) < 0.000000000001) {
        c = 0.0;
    }
    if (sine) {
        *sine = s;
    }
    if (cosine) {
        *cosine = c;
    }
}

static double backend_exp_double(double x)
{
    const double ln2 = 0.69314718055994530942;
    double term;
    double sum;
    int n = 0;
    unsigned int i;

    if (x < -60.0) {
        return 0.0;
    }
    if (x > 60.0) {
        x = 60.0;
    }
    while (x > 0.5) {
        x -= ln2;
        ++n;
    }
    while (x < -0.5) {
        x += ln2;
        --n;
    }

    term = 1.0;
    sum = 1.0;
    for (i = 1u; i <= 18u; ++i) {
        term *= x / (double)i;
        sum += term;
    }
    while (n > 0) {
        sum *= 2.0;
        --n;
    }
    while (n < 0) {
        sum *= 0.5;
        ++n;
    }
    return sum;
}

static double backend_silu_double(double x)
{
    return x / (1.0 + backend_exp_double(-x));
}

static int cpu_op_embed(yvex_backend *backend,
                      const yvex_device_tensor *embedding,
                      const unsigned int *token_ids,
                      unsigned long long token_count,
                      yvex_device_tensor *out,
                      yvex_error *err)
{
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long e;
    unsigned long long t;
    const float *embedding_f32;
    const unsigned char *embedding_f16;
    float *out_data;
    int rc;

    rc = yvex_backend_validate_embed(
        backend, embedding, token_ids, token_count, out, &hidden_size, &vocab_size,
        "backend layer CPU embed supports F32 and F16 embeddings with F32 output",
        "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    embedding_f32 = (const float *)embedding->data;
    embedding_f16 = (const unsigned char *)embedding->data;
    out_data = (float *)out->data;
    for (t = 0; t < token_count; ++t) {
        unsigned int token_id = token_ids[t];
        for (e = 0; e < hidden_size; ++e) {
            unsigned long long index = ((unsigned long long)token_id * hidden_size) + e;
            if (embedding->dtype == YVEX_DTYPE_F16) {
                out_data[(t * hidden_size) + e] =
                    cpu_decode_f16(embedding_f16 + (index * 2ull));
            } else {
                out_data[(t * hidden_size) + e] = embedding_f32[index];
            }
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Compute scaled causal attention directly over admitted CPU F32 tensors. */
static int cpu_op_attention(yvex_backend *backend,
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
    const float *q_data;
    const float *k_data;
    const float *v_data;
    float *score_data;
    float *prob_data;
    float *out_data;
    unsigned long long head_dim;
    unsigned long long kv_elements;
    unsigned long long visible_count;
    unsigned long long i;
    unsigned long long d;
    double max_score = 0.0;
    double sum_exp = 0.0;
    int rc;

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

    q_data = (const float *)query->data;
    k_data = (const float *)keys->data;
    v_data = (const float *)values->data;
    score_data = (float *)score_scratch->data;
    prob_data = (float *)probability_scratch->data;
    out_data = (float *)out->data;
    visible_count = causal ? position + 1ull : seq_len;

    for (i = 0; i < seq_len; ++i) {
        double score = 0.0;
        if (causal && i > position) {
            score_data[i] = 0.0f;
            prob_data[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += (double)q_data[d] * (double)k_data[(i * head_dim) + d];
        }
        score *= (double)scale;
        score_data[i] = (float)score;
        if (i == 0 || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        double e = backend_exp_double((double)score_data[i] - max_score);
        prob_data[i] = (float)e;
        sum_exp += e;
    }
    if (sum_exp <= 0.0) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_attention",
                       "attention softmax sum is zero");
        return YVEX_ERR_STATE;
    }
    for (i = 0; i < visible_count; ++i) {
        prob_data[i] = (float)((double)prob_data[i] / sum_exp);
    }
    for (d = 0; d < head_dim; ++d) {
        double value = 0.0;
        for (i = 0; i < visible_count; ++i) {
            value += (double)prob_data[i] * (double)v_data[(i * head_dim) + d];
        }
        out_data[d] = (float)value;
    }

    score_scratch->is_written = 1;
    probability_scratch->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_op_matmul(yvex_backend *backend,
                       const yvex_device_tensor *input,
                       const yvex_device_tensor *weight,
                       yvex_device_tensor *out,
                       yvex_error *err)
{
    const float *input_data;
    const float *weight_data;
    float *out_data;
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long row;
    unsigned long long col;
    int rc;

    rc = yvex_backend_validate_matmul(backend, input, weight, out, &m, &k, &n,
                                      "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    weight_data = (const float *)weight->data;
    out_data = (float *)out->data;
    for (row = 0; row < m; ++row) {
        for (col = 0; col < n; ++col) {
            double sum = 0.0;
            unsigned long long inner;
            for (inner = 0; inner < k; ++inner) {
                sum += (double)input_data[(row * k) + inner] *
                       (double)weight_data[(inner * n) + col];
            }
            out_data[(row * n) + col] = (float)sum;
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Evaluate the gated SiLU CPU MLP primitive without temporary tensor ownership. */
static int cpu_op_mlp(yvex_backend *backend,
                    const yvex_device_tensor *input,
                    const yvex_device_tensor *gate_weight,
                    const yvex_device_tensor *up_weight,
                    const yvex_device_tensor *down_weight,
                    const yvex_mlp_options *options,
                    yvex_device_tensor *intermediate,
                    yvex_device_tensor *out,
                    yvex_error *err)
{
    const float *input_data;
    const float *gate_data;
    const float *up_data;
    const float *down_data;
    float *intermediate_data;
    float *out_data;
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long gate_offset;
    unsigned long long up_offset;
    unsigned long long down_offset;
    unsigned long long row;
    unsigned long long j;
    unsigned long long h;
    int rc;

    rc = yvex_backend_validate_mlp(
        backend, input, gate_weight, up_weight, down_weight, options,
        intermediate, out, &batch, &hidden_dim, &ffn_dim, &gate_offset,
        &up_offset, &down_offset, "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    gate_data = (const float *)gate_weight->data + gate_offset;
    up_data = (const float *)up_weight->data + up_offset;
    down_data = (const float *)down_weight->data + down_offset;
    intermediate_data = (float *)intermediate->data;
    out_data = (float *)out->data;

    for (row = 0; row < batch; ++row) {
        for (j = 0; j < ffn_dim; ++j) {
            double gate_sum = 0.0;
            double up_sum = 0.0;
            for (h = 0; h < hidden_dim; ++h) {
                double x = (double)input_data[(row * hidden_dim) + h];
                gate_sum += x * (double)gate_data[(h * ffn_dim) + j];
                up_sum += x * (double)up_data[(h * ffn_dim) + j];
            }
            intermediate_data[(row * ffn_dim) + j] =
                (float)(backend_silu_double(gate_sum) * up_sum);
        }
        for (h = 0; h < hidden_dim; ++h) {
            double sum = 0.0;
            for (j = 0; j < ffn_dim; ++j) {
                sum += (double)intermediate_data[(row * ffn_dim) + j] *
                       (double)down_data[(j * hidden_dim) + h];
            }
            out_data[(row * hidden_dim) + h] = (float)sum;
        }
    }
    intermediate->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_op_rope(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     unsigned long long position,
                     float rope_base,
                     yvex_device_tensor *out,
                     yvex_error *err)
{
    const float *input_data;
    float *out_data;
    unsigned long long head_dim;
    unsigned long long pair_count;
    unsigned long long pair;
    double inverse_root;
    double frequency = 1.0;
    int rc;

    if (!backend_tensor_owner_is(backend, input) ||
        !backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "input and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rope",
                       "CPU RoPE supports F32 input/output");
        return YVEX_ERR_UNSUPPORTED;
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
    if (!backend_tensor_f32_elements(input, head_dim) ||
        !backend_tensor_f32_elements(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rope",
                       "RoPE input/output bytes must match F32 head_dim");
        return YVEX_ERR_BOUNDS;
    }

    pair_count = head_dim / 2ull;
    inverse_root = 1.0 / yvex_backend_nth_root((double)rope_base, pair_count);
    input_data = (const float *)input->data;
    out_data = (float *)out->data;

    for (pair = 0; pair < pair_count; ++pair) {
        unsigned long long even_index = pair * 2ull;
        unsigned long long odd_index = even_index + 1ull;
        double sine;
        double cosine;
        double even = (double)input_data[even_index];
        double odd = (double)input_data[odd_index];
        double angle = (double)position * frequency;

        backend_sincos_double(angle, &sine, &cosine);
        out_data[even_index] = (float)((even * cosine) - (odd * sine));
        out_data[odd_index] = (float)((even * sine) + (odd * cosine));
        frequency *= inverse_root;
    }

    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_op_rms_norm(yvex_backend *backend,
                         const yvex_device_tensor *input,
                         const yvex_device_tensor *weight,
                         float epsilon,
                         yvex_device_tensor *out,
                         yvex_error *err)
{
    const float *input_data;
    const float *weight_f32;
    const unsigned char *weight_f16;
    float *out_data;
    unsigned long long hidden_size;
    unsigned long long row_count;
    unsigned long long row;
    unsigned long long i;
    int rc;

    rc = yvex_backend_validate_rms_norm(
        backend, input, weight, epsilon, out, &hidden_size,
        "CPU RMSNorm supports F32 input/output with F16 or F32 weight",
        "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    weight_f32 = (const float *)weight->data;
    weight_f16 = (const unsigned char *)weight->data;
    out_data = (float *)out->data;
    row_count = input->rank == 2 ? input->dims[0] : 1ull;

    for (row = 0ull; row < row_count; ++row) {
        unsigned long long offset = row * hidden_size;
        double sum_squares = 0.0;
        double rms;
        double inv_rms;

        for (i = 0ull; i < hidden_size; ++i) {
            sum_squares += (double)input_data[offset + i] *
                           (double)input_data[offset + i];
        }
        rms = backend_sqrt_double((sum_squares / (double)hidden_size) + (double)epsilon);
        inv_rms = rms > 0.0 ? 1.0 / rms : 0.0;
        for (i = 0ull; i < hidden_size; ++i) {
            float w = weight->dtype == YVEX_DTYPE_F16
                          ? cpu_decode_f16(weight_f16 + (i * 2ull))
                          : weight_f32[i];
            out_data[offset + i] =
                (float)((double)input_data[offset + i] * inv_rms * (double)w);
        }
    }

    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_alloc(yvex_backend *backend,
                          const yvex_backend_tensor_desc *desc,
                          yvex_device_tensor **out,
                          yvex_error *err)
{
    yvex_device_tensor *tensor;
    unsigned int i;
    int rc;

    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.tensor_alloc", "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = yvex_backend_memory_can_add(backend, desc->bytes, "CPU", "cpu.tensor_alloc", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = (yvex_device_tensor *)calloc(1, sizeof(*tensor));
    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor object");
        return YVEX_ERR_NOMEM;
    }
    tensor->data = (unsigned char *)calloc(1, (size_t)desc->bytes);
    if (!tensor->data) {
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor data");
        return YVEX_ERR_NOMEM;
    }
    tensor->name = yvex_core_strdup(desc->name);
    if (!tensor->name) {
        free(tensor->data);
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to copy tensor name");
        return YVEX_ERR_NOMEM;
    }
    tensor->owner = backend;
    tensor->owner_id = backend->tensor_id_next++;
    tensor->dtype = desc->dtype;
    tensor->rank = desc->rank;
    for (i = 0; i < desc->rank; ++i) {
        tensor->dims[i] = desc->dims[i];
    }
    tensor->bytes = desc->bytes;

    backend_memory_acquire(backend, desc->bytes);

    *out = tensor;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_free(yvex_backend *backend,
                         yvex_device_tensor *tensor,
                         yvex_error *err)
{
    if (!backend || !tensor || !backend_tensor_owner_is(backend, tensor)) {
        yvex_error_set(err, YVEX_ERR_STATE, "cpu.tensor_free",
                       "tensor does not belong to this backend");
        return YVEX_ERR_STATE;
    }
    backend_memory_release(backend, tensor->bytes);
    tensor->owner = NULL;
    tensor->owner_id = 0;
    free(tensor->data);
    free(tensor->name);
    free(tensor);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_write(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          const void *src,
                          unsigned long long len,
                          yvex_error *err)
{
    int rc = yvex_backend_tensor_rw_validate(
        "yvex_backend_tensor_write", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(tensor->data, src, (size_t)len);
    tensor->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_read(yvex_backend *backend,
                         const yvex_device_tensor *tensor,
                         void *dst,
                         unsigned long long len,
                         yvex_error *err)
{
    int rc = yvex_backend_tensor_rw_validate(
        "yvex_backend_tensor_read", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(dst, tensor->data, (size_t)len);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_zero(yvex_backend *backend, yvex_device_tensor *tensor,
                           yvex_error *err)
{
    if (!backend_tensor_owner_is(backend, tensor) || tensor->borrowed_host) {
        yvex_error_set(err, YVEX_ERR_STATE, "cpu.tensor.zero",
                       "one CPU-owned mutable tensor is required");
        return YVEX_ERR_STATE;
    }
    memset(tensor->data, 0, (size_t)tensor->bytes);
    tensor->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_copy(yvex_backend *backend,
                         yvex_device_tensor *dst,
                         const yvex_device_tensor *src,
                         yvex_error *err)
{
    int status = yvex_backend_tensor_copy_validate(
        backend, dst, src, "yvex_backend_tensor_copy", err);

    if (status != YVEX_OK) {
        return status;
    }
    memcpy(dst->data, src->data, (size_t)src->bytes);
    dst->is_written = src->is_written;
    yvex_error_clear(err);
    return YVEX_OK;
}
