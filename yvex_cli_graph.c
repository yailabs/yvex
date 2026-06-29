/*
 * yvex_cli_graph.c - Graph command and standalone graph-op proof helpers.
 */

#include "yvex_cli_private.h"

static int cli_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void init_graph_guard_report(yvex_cli_graph_guard_report *report,
                                    const char *graph_kind,
                                    int needs_slice_range,
                                    const yvex_model_ref *model_ref)
{
    memset(report, 0, sizeof(*report));
    report->guard_status = "fail";
    report->phase = "preflight";
    report->graph_kind = graph_kind ? graph_kind : "unknown";
    report->integrity_status = "unchecked";
    report->identity_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->metadata_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->shape_status = "unchecked";
    report->range_status = "unchecked";
    report->slice_range_status = needs_slice_range ? "unchecked" : "not-needed";
    report->backend_status = "not-opened";
    report->backend_op_status = "unchecked";
    report->cleanup_status = "not-needed";
}

static const yvex_tensor_info *cli_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight",
        "blk.0.attention_norm.weight",
        "blk.0.input_layernorm.weight",
        "model.layers.0.input_layernorm.weight",
    };
    unsigned int i;
    unsigned long long count;
    unsigned long long index;

    if (!tensors) {
        return NULL;
    }
    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, preferred[i]);
        if (tensor) {
            return tensor;
        }
    }
    count = yvex_tensor_table_count(tensors);
    for (index = 0; index < count; ++index) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
        if (tensor && tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM) {
            return tensor;
        }
    }
    return NULL;
}

static int cli_has_rmsnorm_epsilon(const yvex_gguf *gguf)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon",
        "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, keys[i]);
        double epsilon = 0.0;
        if (value && yvex_gguf_value_as_f64(value, &epsilon) == YVEX_OK && epsilon > 0.0) {
            return 1;
        }
    }
    return 0;
}

void print_graph_guard_report(const yvex_cli_graph_guard_report *report)
{
    printf("graph_integrity_guard: %s\n", report->guard_status ? report->guard_status : "fail");
    printf("graph_execution_phase: %s\n", report->phase ? report->phase : "preflight");
    printf("graph_kind: %s\n", report->graph_kind ? report->graph_kind : "unknown");
    printf("integrity_status: %s\n", report->integrity_status ? report->integrity_status : "unchecked");
    printf("identity_status: %s\n", report->identity_status ? report->identity_status : "unregistered");
    printf("metadata_status: %s\n", report->metadata_status ? report->metadata_status : "unregistered");
    printf("shape_status: %s\n", report->shape_status ? report->shape_status : "unchecked");
    printf("range_status: %s\n", report->range_status ? report->range_status : "unchecked");
    printf("slice_range_status: %s\n", report->slice_range_status ? report->slice_range_status : "unchecked");
    printf("backend_status: %s\n", report->backend_status ? report->backend_status : "not-opened");
    printf("backend_op_status: %s\n", report->backend_op_status ? report->backend_op_status : "unchecked");
    printf("dispatch_attempted: %s\n", report->dispatch_attempted ? "true" : "false");
    printf("reference_read_attempted: %s\n", report->reference_read_attempted ? "true" : "false");
    printf("output_allocation_attempted: %s\n", report->output_allocation_attempted ? "true" : "false");
    printf("cleanup_attempted: %s\n", report->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n", report->cleanup_status ? report->cleanup_status : "not-needed");
    printf("output_bytes_planned: %llu\n", report->output_bytes_planned);
    printf("output_bytes_allocated: %llu\n", report->output_bytes_allocated);
    printf("reference_bytes_planned: %llu\n", report->reference_bytes_planned);
}

static unsigned long long cli_checksum_bytes(const void *data, unsigned long long len)
{
    const unsigned char *bytes = (const unsigned char *)data;
    unsigned long long checksum = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; i < len; ++i) {
        checksum ^= (unsigned long long)bytes[i];
        checksum *= 1099511628211ull;
    }
    return checksum;
}

static float cli_abs_float(float x)
{
    return x < 0.0f ? -x : x;
}

static double cli_abs_double(double x)
{
    return x < 0.0 ? -x : x;
}

static double cli_sqrt_double(double x)
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

static double cli_exp_double(double x)
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

static double cli_nth_root_double(double x, unsigned long long n)
{
    double lo = 1.0;
    double hi = x > 1.0 ? x : 1.0;
    unsigned int iter;

    if (x <= 0.0 || n == 0) {
        return 0.0;
    }
    if (n == 1) {
        return x;
    }
    for (iter = 0; iter < 96u; ++iter) {
        double mid = 0.5 * (lo + hi);
        double acc = 1.0;
        unsigned long long i;

        for (i = 0; i < n; ++i) {
            acc *= mid;
            if (acc > x) {
                break;
            }
        }
        if (acc > x) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

static double cli_wrap_radians(double x)
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

static void cli_sincos_double(double x, double *sine, double *cosine)
{
    double x2;
    double s;
    double c;

    x = cli_wrap_radians(x);
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
    if (cli_abs_double(s) < 0.000000000001) {
        s = 0.0;
    }
    if (cli_abs_double(c) < 0.000000000001) {
        c = 0.0;
    }
    if (sine) {
        *sine = s;
    }
    if (cosine) {
        *cosine = c;
    }
}

static void cli_rope_fill_input(float *values, unsigned long long head_dim)
{
    unsigned long long i;

    for (i = 0; i < head_dim; ++i) {
        values[i] = (float)((double)(i + 1ull) * 0.25);
    }
}

static void cli_rope_reference(const float *input,
                               unsigned long long head_dim,
                               unsigned long long position,
                               float rope_base,
                               float *out)
{
    unsigned long long pair_count = head_dim / 2ull;
    unsigned long long pair;
    double inverse_root = 1.0 / cli_nth_root_double((double)rope_base, pair_count);
    double frequency = 1.0;

    for (pair = 0; pair < pair_count; ++pair) {
        unsigned long long even_index = pair * 2ull;
        unsigned long long odd_index = even_index + 1ull;
        double sine;
        double cosine;
        double even = (double)input[even_index];
        double odd = (double)input[odd_index];
        double angle = (double)position * frequency;

        cli_sincos_double(angle, &sine, &cosine);
        out[even_index] = (float)((even * cosine) - (odd * sine));
        out[odd_index] = (float)((even * sine) + (odd * cosine));
        frequency *= inverse_root;
    }
}

static void cli_attention_fill_inputs(float *query,
                                      float *keys,
                                      float *values,
                                      unsigned long long seq_len,
                                      unsigned long long head_dim)
{
    unsigned long long i;
    unsigned long long d;

    for (d = 0; d < head_dim; ++d) {
        query[d] = (float)(0.02 + ((double)(d + 1ull) * 0.01));
    }
    for (i = 0; i < seq_len; ++i) {
        for (d = 0; d < head_dim; ++d) {
            keys[(i * head_dim) + d] =
                (float)(0.03 + ((double)(i + 1ull) * 0.04) + ((double)(d + 1ull) * 0.002));
            values[(i * head_dim) + d] =
                (float)(0.10 + ((double)(i + 1ull) * 0.08) + ((double)(d + 1ull) * 0.01));
        }
    }
}

static void cli_attention_reference(const float *query,
                                    const float *keys,
                                    const float *values,
                                    unsigned long long seq_len,
                                    unsigned long long position,
                                    unsigned long long head_dim,
                                    float scale,
                                    int causal,
                                    float *score_scratch,
                                    float *probability_scratch,
                                    float *out)
{
    unsigned long long visible_count = causal ? position + 1ull : seq_len;
    unsigned long long i;
    unsigned long long d;
    double max_score = 0.0;
    double sum_exp = 0.0;

    for (i = 0; i < seq_len; ++i) {
        double score = 0.0;
        if (causal && i > position) {
            score_scratch[i] = 0.0f;
            probability_scratch[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += (double)query[d] * (double)keys[(i * head_dim) + d];
        }
        score *= (double)scale;
        score_scratch[i] = (float)score;
        if (i == 0ull || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        double e = cli_exp_double((double)score_scratch[i] - max_score);
        probability_scratch[i] = (float)e;
        sum_exp += e;
    }
    if (sum_exp <= 0.0) {
        for (d = 0; d < head_dim; ++d) {
            out[d] = 0.0f;
        }
        return;
    }
    for (i = 0; i < visible_count; ++i) {
        probability_scratch[i] = (float)((double)probability_scratch[i] / sum_exp);
    }
    for (d = 0; d < head_dim; ++d) {
        double value = 0.0;
        for (i = 0; i < visible_count; ++i) {
            value += (double)probability_scratch[i] * (double)values[(i * head_dim) + d];
        }
        out[d] = (float)value;
    }
}

static void cli_matmul_fill_inputs(float *input,
                                   float *weight,
                                   unsigned long long m,
                                   unsigned long long k,
                                   unsigned long long n)
{
    unsigned long long row;
    unsigned long long inner;
    unsigned long long col;

    for (row = 0; row < m; ++row) {
        for (inner = 0; inner < k; ++inner) {
            input[(row * k) + inner] =
                (float)(0.05 + ((double)(row + 1ull) * 0.02) +
                        ((double)(inner + 1ull) * 0.01));
        }
    }
    for (inner = 0; inner < k; ++inner) {
        for (col = 0; col < n; ++col) {
            weight[(inner * n) + col] =
                (float)(0.03 + ((double)(inner + 1ull) * 0.004) +
                        ((double)(col + 1ull) * 0.002));
        }
    }
}

static void cli_matmul_reference(const float *input,
                                 const float *weight,
                                 unsigned long long m,
                                 unsigned long long k,
                                 unsigned long long n,
                                 float *out)
{
    unsigned long long row;
    unsigned long long col;

    for (row = 0; row < m; ++row) {
        for (col = 0; col < n; ++col) {
            double sum = 0.0;
            unsigned long long inner;
            for (inner = 0; inner < k; ++inner) {
                sum += (double)input[(row * k) + inner] *
                       (double)weight[(inner * n) + col];
            }
            out[(row * n) + col] = (float)sum;
        }
    }
}

static double cli_silu_double(double x)
{
    return x / (1.0 + cli_exp_double(-x));
}

static void cli_mlp_fill_inputs(float *input,
                                float *gate_weight,
                                float *up_weight,
                                float *down_weight,
                                unsigned long long batch,
                                unsigned long long hidden_dim,
                                unsigned long long ffn_dim,
                                unsigned long long expert_count,
                                int routed)
{
    unsigned long long row;
    unsigned long long h;
    unsigned long long j;
    unsigned long long e;
    unsigned long long actual_experts = routed ? expert_count : 1ull;

    for (row = 0; row < batch; ++row) {
        for (h = 0; h < hidden_dim; ++h) {
            input[(row * hidden_dim) + h] =
                (float)(0.04 + ((double)(row + 1ull) * 0.03) +
                        ((double)(h + 1ull) * 0.008));
        }
    }
    for (e = 0; e < actual_experts; ++e) {
        unsigned long long up_base = e * hidden_dim * ffn_dim;
        unsigned long long down_base = e * ffn_dim * hidden_dim;
        for (h = 0; h < hidden_dim; ++h) {
            for (j = 0; j < ffn_dim; ++j) {
                gate_weight[up_base + (h * ffn_dim) + j] =
                    (float)(0.015 + ((double)(e + 1ull) * 0.004) +
                            ((double)(h + 1ull) * 0.0015) +
                            ((double)(j + 1ull) * 0.0007));
                up_weight[up_base + (h * ffn_dim) + j] =
                    (float)(0.02 + ((double)(e + 1ull) * 0.005) +
                            ((double)(h + 1ull) * 0.0012) +
                            ((double)(j + 1ull) * 0.0009));
            }
        }
        for (j = 0; j < ffn_dim; ++j) {
            for (h = 0; h < hidden_dim; ++h) {
                down_weight[down_base + (j * hidden_dim) + h] =
                    (float)(0.012 + ((double)(e + 1ull) * 0.003) +
                            ((double)(j + 1ull) * 0.0008) +
                            ((double)(h + 1ull) * 0.0011));
            }
        }
    }
}

static void cli_mlp_reference(const float *input,
                              const float *gate_weight,
                              const float *up_weight,
                              const float *down_weight,
                              unsigned long long batch,
                              unsigned long long hidden_dim,
                              unsigned long long ffn_dim,
                              unsigned long long expert_id,
                              int routed,
                              float *intermediate,
                              float *out)
{
    unsigned long long gate_offset = routed ? expert_id * hidden_dim * ffn_dim : 0ull;
    unsigned long long up_offset = gate_offset;
    unsigned long long down_offset = routed ? expert_id * ffn_dim * hidden_dim : 0ull;
    unsigned long long row;
    unsigned long long h;
    unsigned long long j;

    for (row = 0; row < batch; ++row) {
        for (j = 0; j < ffn_dim; ++j) {
            double gate_sum = 0.0;
            double up_sum = 0.0;
            for (h = 0; h < hidden_dim; ++h) {
                double x = (double)input[(row * hidden_dim) + h];
                gate_sum += x * (double)gate_weight[gate_offset + (h * ffn_dim) + j];
                up_sum += x * (double)up_weight[up_offset + (h * ffn_dim) + j];
            }
            intermediate[(row * ffn_dim) + j] =
                (float)(cli_silu_double(gate_sum) * up_sum);
        }
        for (h = 0; h < hidden_dim; ++h) {
            double sum = 0.0;
            for (j = 0; j < ffn_dim; ++j) {
                sum += (double)intermediate[(row * ffn_dim) + j] *
                       (double)down_weight[down_offset + (j * hidden_dim) + h];
            }
            out[(row * hidden_dim) + h] = (float)sum;
        }
    }
}

static float cli_max_abs_diff_f32(const float *a,
                                  const float *b,
                                  unsigned long long count)
{
    unsigned long long i;
    float max_diff = 0.0f;

    for (i = 0; i < count; ++i) {
        float diff = cli_abs_float(a[i] - b[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
}

static void cli_print_float_values(const char *name,
                                   const float *values,
                                   unsigned long long count)
{
    unsigned long long i;

    printf("%s:", name);
    for (i = 0; i < count; ++i) {
        printf("%s%.9g", i == 0 ? " " : ",", (double)values[i]);
    }
    printf("\n");
}

static void print_no_generation_readiness_fields(void)
{
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("execution_ready: false\n");
}

static void print_rope_readiness_fields(void)
{
    printf("attention_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static void print_attention_readiness_fields(int primitive_executed)
{
    printf("attention_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    printf("qkv_projection_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_prefill_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static void print_matmul_readiness_fields(int primitive_executed)
{
    printf("matmul_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    printf("qkv_projection_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_prefill_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static void print_mlp_readiness_fields(int primitive_executed)
{
    printf("mlp_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    printf("router_logits_ready: false\n");
    printf("top_k_routing_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_prefill_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static int command_graph_execute_rope_op(const char *backend_name,
                                         unsigned long long position,
                                         unsigned long long head_dim)
{
    const float rope_base = 10000.0f;
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long bytes = 0ull;
    unsigned long long sample_count;
    float max_abs_diff = 0.0f;
    float input_output_max_abs_diff = 0.0f;
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_graph_guard_report(&guard, "rope-position-op", 0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "not-needed";

    if (head_dim == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: rope-head-dim-zero\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if ((head_dim & 1ull) != 0ull) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: rope-head-dim-odd\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (cli_test_env_enabled("YVEX_TEST_ROPE_BYTE_OVERFLOW") ||
        head_dim > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        head_dim > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    bytes = head_dim * (unsigned long long)sizeof(float);
    guard.shape_status = "pass";
    guard.range_status = "not-applicable";
    guard.output_bytes_planned = bytes;
    guard.reference_bytes_planned = bytes;

    input_values = (float *)malloc((size_t)bytes);
    output_values = (float *)malloc((size_t)bytes);
    reference_values = (float *)malloc((size_t)bytes);
    if (!input_values || !output_values || !reference_values) {
        free(reference_values);
        free(output_values);
        free(input_values);
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph rope", "failed to allocate host buffers");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
    }
    cli_rope_fill_input(input_values, head_dim);
    cli_rope_reference(input_values, head_dim, position, rope_base, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: graph-op-fail\n");
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(rc);
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE)) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: backend-op-rope-unsupported\n");
        printf("status: graph-op-fail\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    guard.backend_op_status = "supported";

    desc.name = "rope.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 1;
    desc.dims[0] = head_dim;
    desc.bytes = bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc == YVEX_OK) {
        desc.name = "rope.output";
        rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = input || out ? 1 : 0;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, bytes, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (cli_test_env_enabled("YVEX_TEST_FAIL_ROPE_AFTER_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        printf("input_bytes: %llu\n", bytes);
        printf("output_bytes: %llu\n", bytes);
        printf("reference_bytes: %llu\n", bytes);
        print_rope_readiness_fields();
        printf("reason: injected-rope-after-alloc\n");
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(YVEX_ERR_STATE);
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_rope(backend, input, position, rope_base, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ROPE_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        printf("input_bytes: %llu\n", bytes);
        printf("output_bytes: %llu\n", bytes);
        printf("reference_bytes: %llu\n", bytes);
        print_rope_readiness_fields();
        printf("reason: %s\n",
               rc == YVEX_OK ? "injected-rope-after-dispatch" : yvex_error_message(&err));
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return rc == YVEX_OK ? exit_for_status(YVEX_ERR_STATE) : exit_for_status(rc);
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, bytes, &err);
    if (rc != YVEX_OK) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, head_dim);
    input_output_max_abs_diff = cli_max_abs_diff_f32(output_values, input_values, head_dim);
    guard.guard_status = max_abs_diff <= 0.0005f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = head_dim < 8ull ? head_dim : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    printf("op: rope\n");
    printf("backend: %s\n", backend_name);
    printf("position: %llu\n", position);
    printf("head_dim: %llu\n", head_dim);
    printf("rope_base: %.9g\n", (double)rope_base);
    printf("dtype: f32\n");
    printf("input_bytes: %llu\n", bytes);
    printf("output_bytes: %llu\n", bytes);
    printf("scratch_bytes: 0\n");
    printf("reference_bytes: %llu\n", bytes);
    printf("input_checksum: %llu\n", cli_checksum_bytes(input_values, bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("input_output_max_abs_diff: %.9g\n", (double)input_output_max_abs_diff);
    printf("position_zero_identity: %s\n",
           position == 0ull && input_output_max_abs_diff <= 0.0000001f ? "true" : "false");
    printf("position_dependent_output: %s\n",
           position != 0ull && input_output_max_abs_diff > 0.0001f ? "true" : "false");
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("rope_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values, sample_count);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_rope_readiness_fields();
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    free(reference_values);
    free(output_values);
    free(input_values);
    return exit_code;
}

static void print_attention_operation_fields(const char *backend_name,
                                             unsigned long long seq_len,
                                             unsigned long long position,
                                             unsigned long long head_dim,
                                             float scale,
                                             int causal,
                                             unsigned long long query_bytes,
                                             unsigned long long key_bytes,
                                             unsigned long long value_bytes,
                                             unsigned long long input_bytes,
                                             unsigned long long score_scratch_bytes,
                                             unsigned long long probability_scratch_bytes,
                                             unsigned long long output_bytes,
                                             unsigned long long reference_bytes)
{
    printf("op: attention\n");
    printf("backend: %s\n", backend_name);
    printf("dtype: f32\n");
    printf("seq_len: %llu\n", seq_len);
    printf("position: %llu\n", position);
    printf("head_dim: %llu\n", head_dim);
    printf("scale: %.9g\n", (double)scale);
    printf("mask: %s\n", causal ? "causal" : "none");
    printf("query_bytes: %llu\n", query_bytes);
    printf("key_bytes: %llu\n", key_bytes);
    printf("value_bytes: %llu\n", value_bytes);
    printf("input_bytes: %llu\n", input_bytes);
    printf("score_scratch_bytes: %llu\n", score_scratch_bytes);
    printf("probability_scratch_bytes: %llu\n", probability_scratch_bytes);
    printf("scratch_bytes: %llu\n", score_scratch_bytes + probability_scratch_bytes);
    printf("output_bytes: %llu\n", output_bytes);
    printf("reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_attention_op(const char *backend_name,
                                              unsigned long long seq_len,
                                              unsigned long long position,
                                              unsigned long long head_dim,
                                              int causal)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *query = NULL;
    yvex_device_tensor *keys = NULL;
    yvex_device_tensor *values = NULL;
    yvex_device_tensor *score_scratch = NULL;
    yvex_device_tensor *probability_scratch = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *query_values = NULL;
    float *key_values = NULL;
    float *value_values = NULL;
    float *score_values = NULL;
    float *probability_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    float *reference_scores = NULL;
    float *reference_probabilities = NULL;
    unsigned long long kv_elements = 0ull;
    unsigned long long query_bytes = 0ull;
    unsigned long long key_bytes = 0ull;
    unsigned long long value_bytes = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long score_scratch_bytes = 0ull;
    unsigned long long probability_scratch_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long visible_keys = 0ull;
    unsigned long long masked_keys = 0ull;
    unsigned long long sample_count = 0ull;
    float scale = 0.0f;
    float max_abs_diff = 0.0f;
    float probability_max_abs_diff = 0.0f;
    float masked_probability_max = 0.0f;
    float position_zero_value_diff = 0.0f;
    const char *reason = NULL;
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_graph_guard_report(&guard, "attention-primitive", 0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "unchecked";

    if (head_dim == 0 || seq_len == 0) {
        guard.shape_status = "fail";
        reason = head_dim == 0 ? "attention-head-dim-zero" : "attention-seq-len-zero";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, 0.0f,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    scale = (float)(1.0 / cli_sqrt_double((double)head_dim));
    if (position >= seq_len) {
        guard.shape_status = "fail";
        guard.slice_range_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: position-out-of-range\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (cli_test_env_enabled("YVEX_TEST_ATTENTION_BYTE_OVERFLOW") ||
        seq_len > ULLONG_MAX / head_dim ||
        head_dim > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        seq_len > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    kv_elements = seq_len * head_dim;
    if (kv_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        kv_elements > ULLONG_MAX / (unsigned long long)sizeof(float)) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    query_bytes = head_dim * (unsigned long long)sizeof(float);
    key_bytes = kv_elements * (unsigned long long)sizeof(float);
    value_bytes = key_bytes;
    score_scratch_bytes = seq_len * (unsigned long long)sizeof(float);
    probability_scratch_bytes = score_scratch_bytes;
    output_bytes = query_bytes;
    reference_bytes = output_bytes;
    if (query_bytes > ULLONG_MAX - key_bytes ||
        query_bytes + key_bytes > ULLONG_MAX - value_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes, 0,
                                         score_scratch_bytes, probability_scratch_bytes,
                                         output_bytes, reference_bytes);
        print_attention_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = query_bytes + key_bytes + value_bytes;
    visible_keys = causal ? position + 1ull : seq_len;
    masked_keys = seq_len - visible_keys;
    guard.shape_status = "pass";
    guard.slice_range_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    query_values = (float *)malloc((size_t)query_bytes);
    key_values = (float *)malloc((size_t)key_bytes);
    value_values = (float *)malloc((size_t)value_bytes);
    score_values = (float *)malloc((size_t)score_scratch_bytes);
    probability_values = (float *)malloc((size_t)probability_scratch_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    reference_scores = (float *)malloc((size_t)score_scratch_bytes);
    reference_probabilities = (float *)malloc((size_t)probability_scratch_bytes);
    if (!query_values || !key_values || !value_values || !score_values ||
        !probability_values || !output_values || !reference_values ||
        !reference_scores || !reference_probabilities) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph attention",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_attention_fill_inputs(query_values, key_values, value_values, seq_len, head_dim);
    cli_attention_reference(query_values, key_values, value_values, seq_len, position,
                            head_dim, scale, causal, reference_scores,
                            reference_probabilities, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes,
                                         input_bytes, score_scratch_bytes,
                                         probability_scratch_bytes, output_bytes,
                                         reference_bytes);
        print_attention_readiness_fields(0);
        printf("visible_keys: %llu\n", visible_keys);
        printf("masked_keys: %llu\n", masked_keys);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION)) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes,
                                         input_bytes, score_scratch_bytes,
                                         probability_scratch_bytes, output_bytes,
                                         reference_bytes);
        print_attention_readiness_fields(0);
        printf("visible_keys: %llu\n", visible_keys);
        printf("masked_keys: %llu\n", masked_keys);
        printf("reason: backend-op-attention-unsupported\n");
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "attention.query";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 1;
    desc.dims[0] = head_dim;
    desc.bytes = query_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &query, &err);
    if (rc == YVEX_OK) {
        desc.name = "attention.keys";
        desc.rank = 2;
        desc.dims[0] = seq_len;
        desc.dims[1] = head_dim;
        desc.bytes = key_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &keys, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.values";
        desc.bytes = value_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &values, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.scores";
        desc.rank = 1;
        desc.dims[0] = seq_len;
        desc.dims[1] = 0;
        desc.bytes = score_scratch_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &score_scratch, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.probabilities";
        desc.bytes = probability_scratch_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &probability_scratch, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.output";
        desc.dims[0] = head_dim;
        desc.bytes = output_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = query || keys || values || score_scratch ||
                                  probability_scratch || out;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, query, query_values, query_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, keys, key_values, key_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, values, value_values, value_bytes, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }

    if (cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-attention-after-alloc";
        goto fail_cleaned;
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_attention(backend, query, keys, values, seq_len, position,
                                   scale, causal, score_scratch,
                                   probability_scratch, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-attention-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, score_scratch, score_values, score_scratch_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, probability_scratch, probability_values,
                                      probability_scratch_bytes, &err);
    }
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_REFERENCE")) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-attention-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, head_dim);
    probability_max_abs_diff = cli_max_abs_diff_f32(probability_values,
                                                    reference_probabilities,
                                                    seq_len);
    if (causal) {
        unsigned long long mask_index;
        for (mask_index = position + 1ull; mask_index < seq_len; ++mask_index) {
            float p = cli_abs_float(probability_values[mask_index]);
            if (p > masked_probability_max) {
                masked_probability_max = p;
            }
        }
    }
    position_zero_value_diff = cli_max_abs_diff_f32(output_values,
                                                    value_values,
                                                    head_dim);
    guard.guard_status = (max_abs_diff <= 0.001f && probability_max_abs_diff <= 0.001f)
                             ? "pass"
                             : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = head_dim < 8ull ? head_dim : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                     causal, query_bytes, key_bytes, value_bytes,
                                     input_bytes, score_scratch_bytes,
                                     probability_scratch_bytes, output_bytes,
                                     reference_bytes);
    printf("visible_keys: %llu\n", visible_keys);
    printf("masked_keys: %llu\n", masked_keys);
    printf("causal_prefix_keys: %llu\n", visible_keys);
    printf("causal_mask_future_prob_zero: %s\n",
           !causal || masked_probability_max <= 0.000001f ? "true" : "false");
    printf("masked_probability_max: %.9g\n", (double)masked_probability_max);
    printf("position_zero_single_key: %s\n",
           position == 0ull && position_zero_value_diff <= 0.000001f ? "true" : "false");
    printf("last_position_full_prefix: %s\n",
           causal && position + 1ull == seq_len ? "true" : "false");
    printf("query_checksum: %llu\n", cli_checksum_bytes(query_values, query_bytes));
    printf("key_checksum: %llu\n", cli_checksum_bytes(key_values, key_bytes));
    printf("value_checksum: %llu\n", cli_checksum_bytes(value_values, value_bytes));
    printf("score_checksum: %llu\n", cli_checksum_bytes(score_values, score_scratch_bytes));
    printf("probability_checksum: %llu\n", cli_checksum_bytes(probability_values, probability_scratch_bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("softmax_max_abs_diff: %.9g\n", (double)probability_max_abs_diff);
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("attention_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("query_sample_values", query_values, sample_count);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_attention_readiness_fields(exit_code == 0);
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                     causal, query_bytes, key_bytes, value_bytes,
                                     input_bytes, score_scratch_bytes,
                                     probability_scratch_bytes, output_bytes,
                                     reference_bytes);
    printf("visible_keys: %llu\n", visible_keys);
    printf("masked_keys: %llu\n", masked_keys);
    print_attention_readiness_fields(0);
    printf("reason: %s\n", reason ? reason : "attention-op-failed");
    printf("status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, probability_scratch);
        yvex_backend_tensor_free(backend, score_scratch);
        yvex_backend_tensor_free(backend, values);
        yvex_backend_tensor_free(backend, keys);
        yvex_backend_tensor_free(backend, query);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_probabilities);
    free(reference_scores);
    free(reference_values);
    free(output_values);
    free(probability_values);
    free(score_values);
    free(value_values);
    free(key_values);
    free(query_values);
    return exit_code;
}

static void print_matmul_operation_fields(const char *backend_name,
                                          unsigned long long m,
                                          unsigned long long k,
                                          unsigned long long n,
                                          unsigned long long input_bytes,
                                          unsigned long long weight_bytes,
                                          unsigned long long output_bytes,
                                          unsigned long long reference_bytes)
{
    printf("op: matmul\n");
    printf("backend: %s\n", backend_name);
    printf("dtype: f32\n");
    printf("m: %llu\n", m);
    printf("k: %llu\n", k);
    printf("n: %llu\n", n);
    printf("projection_shape: %s\n", m == 1ull ? "true" : "false");
    printf("non_projection_shape: %s\n", m == 1ull ? "false" : "true");
    printf("input_bytes: %llu\n", input_bytes);
    printf("weight_bytes: %llu\n", weight_bytes);
    printf("output_bytes: %llu\n", output_bytes);
    printf("scratch_bytes: 0\n");
    printf("reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_matmul_op(const char *backend_name,
                                           unsigned long long m,
                                           unsigned long long k,
                                           unsigned long long n)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *weight = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *weight_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long input_elements = 0ull;
    unsigned long long weight_elements = 0ull;
    unsigned long long output_elements = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long weight_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long total_input_bytes = 0ull;
    unsigned long long sample_count = 0ull;
    float max_abs_diff = 0.0f;
    const char *reason = NULL;
    int rc = YVEX_OK;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_graph_guard_report(&guard, m == 1ull ? "matmul-projection" : "matmul-matrix",
                            0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "not-needed";

    if (m == 0 || k == 0 || n == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        printf("reason: matmul-zero-dimension\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (cli_test_env_enabled("YVEX_TEST_MATMUL_BYTE_OVERFLOW") ||
        m > ULLONG_MAX / k ||
        k > ULLONG_MAX / n ||
        m > ULLONG_MAX / n) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_elements = m * k;
    weight_elements = k * n;
    output_elements = m * n;
    if (input_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        weight_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        output_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        input_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        weight_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        output_elements > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = input_elements * (unsigned long long)sizeof(float);
    weight_bytes = weight_elements * (unsigned long long)sizeof(float);
    output_bytes = output_elements * (unsigned long long)sizeof(float);
    reference_bytes = output_bytes;
    if (input_bytes > ULLONG_MAX - weight_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    total_input_bytes = input_bytes + weight_bytes;
    guard.shape_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    input_values = (float *)malloc((size_t)input_bytes);
    weight_values = (float *)malloc((size_t)weight_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    if (!input_values || !weight_values || !output_values || !reference_values) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph matmul",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_matmul_fill_inputs(input_values, weight_values, m, k, n);
    cli_matmul_reference(input_values, weight_values, m, k, n, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL) ||
        cli_test_env_enabled("YVEX_TEST_MATMUL_BACKEND_OP_UNSUPPORTED")) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        printf("reason: backend-op-matmul-unsupported\n");
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "matmul.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 2;
    desc.dims[0] = m;
    desc.dims[1] = k;
    desc.bytes = input_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = input ? 1 : 0;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_INPUT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-matmul-after-input-alloc";
        goto fail_cleaned;
    }

    desc.name = "matmul.weight";
    desc.dims[0] = k;
    desc.dims[1] = n;
    desc.bytes = weight_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_WEIGHT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-matmul-after-weight-alloc";
        goto fail_cleaned;
    }

    desc.name = "matmul.output";
    desc.dims[0] = m;
    desc.dims[1] = n;
    desc.bytes = output_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, input_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, weight, weight_values, weight_bytes, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_OUTPUT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-matmul-after-output-alloc";
        goto fail_cleaned;
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_matmul(backend, input, weight, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-matmul-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_REFERENCE")) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-matmul-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, output_elements);
    guard.guard_status = max_abs_diff <= 0.001f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = output_elements < 8ull ? output_elements : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_matmul_operation_fields(backend_name, m, k, n,
                                  input_bytes, weight_bytes,
                                  output_bytes, reference_bytes);
    printf("input_elements: %llu\n", input_elements);
    printf("weight_elements: %llu\n", weight_elements);
    printf("output_elements: %llu\n", output_elements);
    printf("input_total_bytes: %llu\n", total_input_bytes);
    printf("input_checksum: %llu\n", cli_checksum_bytes(input_values, input_bytes));
    printf("weight_checksum: %llu\n", cli_checksum_bytes(weight_values, weight_bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("matmul_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values,
                           input_elements < 8ull ? input_elements : 8ull);
    cli_print_float_values("weight_sample_values", weight_values,
                           weight_elements < 8ull ? weight_elements : 8ull);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_matmul_readiness_fields(exit_code == 0);
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_matmul_operation_fields(backend_name, m, k, n,
                                  input_bytes, weight_bytes,
                                  output_bytes, reference_bytes);
    printf("input_elements: %llu\n", input_elements);
    printf("weight_elements: %llu\n", weight_elements);
    printf("output_elements: %llu\n", output_elements);
    printf("input_total_bytes: %llu\n", total_input_bytes);
    print_matmul_readiness_fields(0);
    printf("reason: %s\n", reason ? reason : "matmul-op-failed");
    printf("status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, weight);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_values);
    free(output_values);
    free(weight_values);
    free(input_values);
    return exit_code;
}

static void print_mlp_operation_fields(const char *backend_name,
                                       unsigned long long batch,
                                       unsigned long long hidden_dim,
                                       unsigned long long ffn_dim,
                                       const char *activation,
                                       int gated,
                                       int routed,
                                       unsigned long long expert_count,
                                       unsigned long long expert_id,
                                       unsigned long long input_bytes,
                                       unsigned long long gate_bytes,
                                       unsigned long long up_bytes,
                                       unsigned long long down_bytes,
                                       unsigned long long intermediate_bytes,
                                       unsigned long long output_bytes,
                                       unsigned long long reference_bytes)
{
    printf("op: mlp\n");
    printf("backend: %s\n", backend_name);
    printf("dtype: f32\n");
    printf("batch: %llu\n", batch);
    printf("hidden_dim: %llu\n", hidden_dim);
    printf("ffn_dim: %llu\n", ffn_dim);
    printf("activation: %s\n", activation ? activation : "unknown");
    printf("gated: %s\n", gated ? "true" : "false");
    printf("routed_expert_mode: %s\n", routed ? "true" : "false");
    printf("expert_count: %llu\n", expert_count);
    printf("expert_id: %llu\n", expert_id);
    printf("input_bytes: %llu\n", input_bytes);
    printf("gate_weight_bytes: %llu\n", gate_bytes);
    printf("up_weight_bytes: %llu\n", up_bytes);
    printf("down_weight_bytes: %llu\n", down_bytes);
    printf("intermediate_bytes: %llu\n", intermediate_bytes);
    printf("output_bytes: %llu\n", output_bytes);
    printf("reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_mlp_op(const char *backend_name,
                                        unsigned long long hidden_dim,
                                        unsigned long long ffn_dim,
                                        const char *activation,
                                        int gated,
                                        unsigned long long expert_count,
                                        unsigned long long expert_id,
                                        int routed)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *gate = NULL;
    yvex_device_tensor *up = NULL;
    yvex_device_tensor *down = NULL;
    yvex_device_tensor *intermediate = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_mlp_options options;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *gate_values = NULL;
    float *up_values = NULL;
    float *down_values = NULL;
    float *intermediate_values = NULL;
    float *reference_intermediate_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long batch = 1ull;
    unsigned long long actual_experts = routed ? expert_count : 1ull;
    unsigned long long input_elements = 0ull;
    unsigned long long gate_elements = 0ull;
    unsigned long long up_elements = 0ull;
    unsigned long long down_elements = 0ull;
    unsigned long long intermediate_elements = 0ull;
    unsigned long long output_elements = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long gate_bytes = 0ull;
    unsigned long long up_bytes = 0ull;
    unsigned long long down_bytes = 0ull;
    unsigned long long intermediate_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long total_weight_bytes = 0ull;
    unsigned long long total_input_bytes = 0ull;
    unsigned long long sample_count = 0ull;
    float max_abs_diff = 0.0f;
    const char *reason = NULL;
    int rc = YVEX_OK;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    memset(&options, 0, sizeof(options));
    init_graph_guard_report(&guard, routed ? "mlp-routed-expert" : "mlp-feed-forward",
                            0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "not-needed";

    if (hidden_dim == 0 || ffn_dim == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        printf("reason: mlp-zero-dimension\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (!gated || !activation || strcmp(activation, "silu") != 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        printf("reason: unsupported-activation\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    if (routed && (expert_count == 0 || expert_id >= expert_count)) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        printf("reason: expert-id-out-of-range\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (cli_test_env_enabled("YVEX_TEST_MLP_BYTE_OVERFLOW") ||
        hidden_dim > ULLONG_MAX / ffn_dim ||
        actual_experts > ULLONG_MAX / hidden_dim ||
        actual_experts * hidden_dim > ULLONG_MAX / ffn_dim ||
        actual_experts > ULLONG_MAX / ffn_dim ||
        actual_experts * ffn_dim > ULLONG_MAX / hidden_dim) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    input_elements = batch * hidden_dim;
    gate_elements = actual_experts * hidden_dim * ffn_dim;
    up_elements = gate_elements;
    down_elements = actual_experts * ffn_dim * hidden_dim;
    intermediate_elements = batch * ffn_dim;
    output_elements = input_elements;
    if (input_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        gate_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        up_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        down_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        intermediate_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        output_elements > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = input_elements * (unsigned long long)sizeof(float);
    gate_bytes = gate_elements * (unsigned long long)sizeof(float);
    up_bytes = up_elements * (unsigned long long)sizeof(float);
    down_bytes = down_elements * (unsigned long long)sizeof(float);
    intermediate_bytes = intermediate_elements * (unsigned long long)sizeof(float);
    output_bytes = output_elements * (unsigned long long)sizeof(float);
    reference_bytes = output_bytes;
    if (gate_bytes > ULLONG_MAX - up_bytes ||
        gate_bytes + up_bytes > ULLONG_MAX - down_bytes ||
        input_bytes > ULLONG_MAX - gate_bytes ||
        input_bytes + gate_bytes > ULLONG_MAX - up_bytes ||
        input_bytes + gate_bytes + up_bytes > ULLONG_MAX - down_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, input_bytes, gate_bytes, up_bytes,
                                   down_bytes, intermediate_bytes, output_bytes,
                                   reference_bytes);
        print_mlp_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    total_weight_bytes = gate_bytes + up_bytes + down_bytes;
    total_input_bytes = input_bytes + total_weight_bytes;
    guard.shape_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    input_values = (float *)malloc((size_t)input_bytes);
    gate_values = (float *)malloc((size_t)gate_bytes);
    up_values = (float *)malloc((size_t)up_bytes);
    down_values = (float *)malloc((size_t)down_bytes);
    intermediate_values = (float *)malloc((size_t)intermediate_bytes);
    reference_intermediate_values = (float *)malloc((size_t)intermediate_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    if (!input_values || !gate_values || !up_values || !down_values ||
        !intermediate_values || !reference_intermediate_values ||
        !output_values || !reference_values) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph mlp",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_mlp_fill_inputs(input_values, gate_values, up_values, down_values,
                        batch, hidden_dim, ffn_dim, actual_experts, routed);
    cli_mlp_reference(input_values, gate_values, up_values, down_values,
                      batch, hidden_dim, ffn_dim, expert_id, routed,
                      reference_intermediate_values, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, input_bytes, gate_bytes, up_bytes,
                                   down_bytes, intermediate_bytes, output_bytes,
                                   reference_bytes);
        print_mlp_readiness_fields(0);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MLP) ||
        cli_test_env_enabled("YVEX_TEST_MLP_BACKEND_OP_UNSUPPORTED")) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, input_bytes, gate_bytes, up_bytes,
                                   down_bytes, intermediate_bytes, output_bytes,
                                   reference_bytes);
        print_mlp_readiness_fields(0);
        printf("reason: backend-op-mlp-unsupported\n");
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "mlp.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 2;
    desc.dims[0] = batch;
    desc.dims[1] = hidden_dim;
    desc.dims[2] = 0;
    desc.bytes = input_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = input ? 1 : 0;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_INPUT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-mlp-after-input-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.gate_weight";
    desc.rank = routed ? 3 : 2;
    desc.dims[0] = routed ? actual_experts : hidden_dim;
    desc.dims[1] = routed ? hidden_dim : ffn_dim;
    desc.dims[2] = routed ? ffn_dim : 0;
    desc.bytes = gate_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &gate, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_GATE_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-mlp-after-gate-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.up_weight";
    desc.bytes = up_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &up, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_UP_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-mlp-after-up-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.down_weight";
    desc.rank = routed ? 3 : 2;
    desc.dims[0] = routed ? actual_experts : ffn_dim;
    desc.dims[1] = routed ? ffn_dim : hidden_dim;
    desc.dims[2] = routed ? hidden_dim : 0;
    desc.bytes = down_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &down, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_DOWN_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-mlp-after-down-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.intermediate";
    desc.rank = 2;
    desc.dims[0] = batch;
    desc.dims[1] = ffn_dim;
    desc.dims[2] = 0;
    desc.bytes = intermediate_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &intermediate, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_INTERMEDIATE_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-mlp-after-intermediate-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.output";
    desc.dims[0] = batch;
    desc.dims[1] = hidden_dim;
    desc.bytes = output_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, input_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, gate, gate_values, gate_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, up, up_values, up_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, down, down_values, down_bytes, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_OUTPUT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-mlp-after-output-alloc";
        goto fail_cleaned;
    }

    options.batch = batch;
    options.hidden_dim = hidden_dim;
    options.ffn_dim = ffn_dim;
    options.expert_count = routed ? expert_count : 0ull;
    options.expert_id = routed ? expert_id : 0ull;
    options.routed_expert_mode = routed;
    options.gated = gated;
    options.activation = activation;

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_mlp(backend, input, gate, up, down, &options,
                             intermediate, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-mlp-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, intermediate, intermediate_values,
                                  intermediate_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    }
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_REFERENCE")) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-mlp-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, output_elements);
    guard.guard_status = max_abs_diff <= 0.001f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = output_elements < 8ull ? output_elements : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                               activation, gated, routed, expert_count,
                               expert_id, input_bytes, gate_bytes, up_bytes,
                               down_bytes, intermediate_bytes, output_bytes,
                               reference_bytes);
    printf("input_elements: %llu\n", input_elements);
    printf("gate_weight_elements: %llu\n", gate_elements);
    printf("up_weight_elements: %llu\n", up_elements);
    printf("down_weight_elements: %llu\n", down_elements);
    printf("intermediate_elements: %llu\n", intermediate_elements);
    printf("output_elements: %llu\n", output_elements);
    printf("weight_total_bytes: %llu\n", total_weight_bytes);
    printf("input_total_bytes: %llu\n", total_input_bytes);
    printf("input_checksum: %llu\n", cli_checksum_bytes(input_values, input_bytes));
    printf("gate_weight_checksum: %llu\n", cli_checksum_bytes(gate_values, gate_bytes));
    printf("up_weight_checksum: %llu\n", cli_checksum_bytes(up_values, up_bytes));
    printf("down_weight_checksum: %llu\n", cli_checksum_bytes(down_values, down_bytes));
    printf("intermediate_checksum: %llu\n", cli_checksum_bytes(intermediate_values, intermediate_bytes));
    printf("reference_intermediate_checksum: %llu\n", cli_checksum_bytes(reference_intermediate_values, intermediate_bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("mlp_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values,
                           input_elements < 8ull ? input_elements : 8ull);
    cli_print_float_values("intermediate_sample_values", intermediate_values,
                           intermediate_elements < 8ull ? intermediate_elements : 8ull);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_mlp_readiness_fields(exit_code == 0);
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                               activation, gated, routed, expert_count,
                               expert_id, input_bytes, gate_bytes, up_bytes,
                               down_bytes, intermediate_bytes, output_bytes,
                               reference_bytes);
    printf("input_elements: %llu\n", input_elements);
    printf("gate_weight_elements: %llu\n", gate_elements);
    printf("up_weight_elements: %llu\n", up_elements);
    printf("down_weight_elements: %llu\n", down_elements);
    printf("intermediate_elements: %llu\n", intermediate_elements);
    printf("output_elements: %llu\n", output_elements);
    printf("weight_total_bytes: %llu\n", total_weight_bytes);
    printf("input_total_bytes: %llu\n", total_input_bytes);
    print_mlp_readiness_fields(0);
    printf("reason: %s\n", reason ? reason : "mlp-op-failed");
    printf("status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, intermediate);
        yvex_backend_tensor_free(backend, down);
        yvex_backend_tensor_free(backend, up);
        yvex_backend_tensor_free(backend, gate);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_values);
    free(output_values);
    free(reference_intermediate_values);
    free(intermediate_values);
    free(down_values);
    free(up_values);
    free(gate_values);
    free(input_values);
    return exit_code;
}

int preflight_graph_guard(const yvex_model_ref *model_ref,
                                 const char *backend_name,
                                 int execute_fixture,
                                 int execute_segment,
                                 unsigned int token_id,
                                 yvex_cli_graph_guard_report *report,
                                 yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_artifact_integrity_report integrity_report;
    yvex_tensor_range tensor_range;
    yvex_tensor_slice_range slice_range;
    yvex_selected_embedding_shape embedding_shape;
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    const yvex_tensor_info *tensor;
    const yvex_tensor_info *rmsnorm_tensor = NULL;
    unsigned long long hidden_size;
    unsigned long long output_bytes;
    unsigned long long planned_bytes;
    int rc;

    init_graph_guard_report(report,
                            execute_fixture ? "fixture-embedding" :
                            (execute_segment ? "selected-embedding-rmsnorm"
                                             : "selected-embedding-partial"),
                            !execute_fixture,
                            model_ref);
    memset(&ctx, 0, sizeof(ctx));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&tensor_range, 0, sizeof(tensor_range));
    memset(&slice_range, 0, sizeof(slice_range));
    memset(&embedding_shape, 0, sizeof(embedding_shape));
    memset(&backend_options, 0, sizeof(backend_options));

    rc = open_model_context(model_ref->path, &ctx, err);
    if (rc != YVEX_OK) {
        report->integrity_status = "fail";
        return rc;
    }
    rc = yvex_artifact_integrity_validate(ctx.artifact,
                                          ctx.gguf,
                                          ctx.table,
                                          NULL,
                                          &integrity_report,
                                          err);
    report->integrity_status = (rc == YVEX_OK && integrity_report.passed) ? "pass" : "fail";
    report->shape_status =
        integrity_report.tensor_shapes_invalid == 0 &&
        integrity_report.tensor_dtypes_invalid == 0 &&
        integrity_report.tensor_byte_counts_invalid == 0 ? "pass" : "fail";
    report->range_status = integrity_report.tensor_ranges_invalid == 0 ? "pass" : "fail";
    if (rc != YVEX_OK || !integrity_report.passed) {
        close_model_context(&ctx);
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "graph_integrity_preflight",
                           "artifact integrity preflight failed");
        }
        return rc == YVEX_OK ? YVEX_ERR_STATE : rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (!tensor) {
        report->shape_status = "fail";
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, tensor, &tensor_range, err);
    if (rc != YVEX_OK) {
        report->range_status = "fail";
        close_model_context(&ctx);
        return rc;
    }
    report->range_status = "pass";

    if (execute_fixture) {
        if (tensor->dtype != YVEX_DTYPE_F32) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "fixture graph embed execution requires F32 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        if (tensor->rank != 2 || tensor->dims[0] == 0 || tensor->dims[1] == 0) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                           "fixture graph token embedding must be rank 2 with non-zero dims");
            return YVEX_ERR_FORMAT;
        }
        if ((unsigned long long)token_id >= tensor->dims[1]) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                            "fixture token id %u exceeds embedding vocab size %llu",
                            token_id, tensor->dims[1]);
            return YVEX_ERR_BOUNDS;
        }
        hidden_size = tensor->dims[0];
        if (hidden_size > (unsigned long long)(~(size_t)0 / sizeof(float))) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                           "fixture graph output is too large");
            return YVEX_ERR_BOUNDS;
        }
        report->shape_status = "pass";
        report->slice_range_status = "not-needed";
        report->output_bytes_planned = hidden_size * (unsigned long long)sizeof(float);
    } else {
        if (tensor->dtype != YVEX_DTYPE_F16) {
            report->shape_status = "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "real partial embedding segment requires F16 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        rc = yvex_selected_embedding_shape_validate(tensor, token_id, &embedding_shape, err);
        if (rc != YVEX_OK) {
            const char *msg = yvex_error_message(err);
            report->shape_status = msg && strstr(msg, "token-out-of-range") ? "pass" : "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            if (msg && strstr(msg, "token-out-of-range")) {
                yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                                "partial token out of range: %u >= %llu",
                                token_id, embedding_shape.vocab_size);
            }
            return rc;
        }
        rc = yvex_tensor_embedding_slice_range_validate(&tensor_range,
                                                        token_id,
                                                        &slice_range,
                                                        err);
        if (rc != YVEX_OK) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            return rc;
        }
        report->shape_status = "pass";
        report->slice_range_status = "pass";
        report->output_bytes_planned = embedding_shape.output_bytes;
        report->reference_bytes_planned = slice_range.slice_bytes;

        if (execute_segment) {
            rmsnorm_tensor = cli_find_first_rmsnorm_tensor(ctx.table);
            if (!rmsnorm_tensor) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-tensor-missing");
                return YVEX_ERR_UNSUPPORTED;
            }
            if (rmsnorm_tensor->rank != 1 || rmsnorm_tensor->dims[0] != embedding_shape.hidden_size) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-shape-invalid");
                return YVEX_ERR_FORMAT;
            }
            if (rmsnorm_tensor->dtype != YVEX_DTYPE_F16 &&
                rmsnorm_tensor->dtype != YVEX_DTYPE_F32) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-dtype-invalid");
                return YVEX_ERR_UNSUPPORTED;
            }
            rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, rmsnorm_tensor, &tensor_range, err);
            if (rc != YVEX_OK) {
                report->range_status = "fail";
                close_model_context(&ctx);
                return rc;
            }
            if (!cli_has_rmsnorm_epsilon(ctx.gguf)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-epsilon-missing");
                return YVEX_ERR_FORMAT;
            }
            if (embedding_shape.output_bytes > ULLONG_MAX / 2ull ||
                embedding_shape.output_bytes > (unsigned long long)(~(size_t)0)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                               "segment-memory-plan-overflow");
                return YVEX_ERR_BOUNDS;
            }
            output_bytes = embedding_shape.output_bytes;
            planned_bytes = output_bytes * 2ull;
            report->output_bytes_planned = planned_bytes;
            report->reference_bytes_planned = output_bytes;
        }
    }

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        report->backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        report->backend_op_status = "unsupported";
        close_model_context(&ctx);
        return rc;
    }
    report->backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        (execute_segment && !yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM)) ||
        cli_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        report->backend_op_status = "unsupported";
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "backend-op-unsupported");
        return YVEX_ERR_UNSUPPORTED;
    }
    report->backend_op_status = "supported";
    report->guard_status = "pass";
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return YVEX_OK;
}

static int command_graph(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_graph *graph = NULL;
    yvex_graph_build_options options;
    yvex_engine_options engine_options;
    yvex_fixture_graph_options fixture_options;
    yvex_fixture_graph_result fixture_result;
    yvex_partial_graph_options partial_options;
    yvex_partial_graph_result partial_result;
    yvex_segment_graph_options segment_options;
    yvex_segment_graph_result segment_result;
    yvex_cli_graph_guard_report graph_guard;
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine *engine = NULL;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = "cpu";
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    const char *op_name = NULL;
    unsigned int token_index = 0u;
    unsigned int selected_token_id = 0u;
    unsigned long long token_vocab_size = 0ull;
    unsigned long long rope_position = 0ull;
    unsigned long long rope_head_dim = 0ull;
    unsigned long long attention_seq_len = 0ull;
    unsigned long long matmul_m = 0ull;
    unsigned long long matmul_k = 0ull;
    unsigned long long matmul_n = 0ull;
    unsigned long long mlp_hidden_dim = 0ull;
    unsigned long long mlp_ffn_dim = 0ull;
    unsigned long long mlp_experts = 0ull;
    unsigned long long mlp_expert_id = 0ull;
    int execute_fixture = 0;
    int execute_partial = 0;
    int execute_segment = 0;
    int execute_op = 0;
    int attention_causal = 0;
    int mlp_gated = 0;
    int fixture_token_provided = 0;
    int partial_token_provided = 0;
    int token_input_provided = 0;
    int token_index_provided = 0;
    int rope_position_provided = 0;
    int rope_head_dim_provided = 0;
    int attention_seq_len_provided = 0;
    int matmul_m_provided = 0;
    int matmul_k_provided = 0;
    int matmul_n_provided = 0;
    int mlp_hidden_dim_provided = 0;
    int mlp_ffn_dim_provided = 0;
    int mlp_activation_provided = 0;
    int mlp_experts_provided = 0;
    int mlp_expert_id_provided = 0;
    const char *mlp_activation = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&fixture_options, 0, sizeof(fixture_options));
    memset(&fixture_result, 0, sizeof(fixture_result));
    memset(&partial_options, 0, sizeof(partial_options));
    memset(&partial_result, 0, sizeof(partial_result));
    memset(&segment_options, 0, sizeof(segment_options));
    memset(&segment_result, 0, sizeof(segment_result));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    options.sequence_length = 1;
    options.include_prefill_path = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("graph"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.sequence_length)) {
                fprintf(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--execute-fixture") == 0) {
            execute_fixture = 1;
        } else if (strcmp(argv[i], "--fixture-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &fixture_options.token_id)) {
                fprintf(stderr, "yvex: --fixture-token requires a non-negative integer\n");
                return 2;
            }
            fixture_token_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--execute-partial") == 0) {
            execute_partial = 1;
        } else if (strcmp(argv[i], "--execute-segment") == 0) {
            execute_segment = 1;
        } else if (strcmp(argv[i], "--execute-op") == 0) {
            execute_op = 1;
        } else if (strcmp(argv[i], "--op") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --op requires rope, attention, matmul, or mlp\n");
                return 2;
            }
            op_name = argv[++i];
        } else if (strcmp(argv[i], "--m") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_m)) {
                fprintf(stderr, "yvex: --m requires a positive integer\n");
                return 2;
            }
            matmul_m_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--k") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_k)) {
                fprintf(stderr, "yvex: --k requires a positive integer\n");
                return 2;
            }
            matmul_k_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--n") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_n)) {
                fprintf(stderr, "yvex: --n requires a positive integer\n");
                return 2;
            }
            matmul_n_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--hidden-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &mlp_hidden_dim)) {
                fprintf(stderr, "yvex: --hidden-dim requires a positive integer\n");
                return 2;
            }
            mlp_hidden_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--ffn-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &mlp_ffn_dim)) {
                fprintf(stderr, "yvex: --ffn-dim requires a positive integer\n");
                return 2;
            }
            mlp_ffn_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--activation") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --activation requires silu\n");
                return 2;
            }
            mlp_activation = argv[++i];
            mlp_activation_provided = 1;
        } else if (strcmp(argv[i], "--gated") == 0) {
            mlp_gated = 1;
        } else if (strcmp(argv[i], "--experts") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &mlp_experts)) {
                fprintf(stderr, "yvex: --experts requires a positive integer\n");
                return 2;
            }
            mlp_experts_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--expert-id") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &mlp_expert_id)) {
                fprintf(stderr, "yvex: --expert-id requires a non-negative integer\n");
                return 2;
            }
            mlp_expert_id_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--seq-len") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &attention_seq_len)) {
                fprintf(stderr, "yvex: --seq-len requires a positive integer\n");
                return 2;
            }
            attention_seq_len_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &rope_position)) {
                fprintf(stderr, "yvex: --position requires a non-negative integer\n");
                return 2;
            }
            rope_position_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &rope_head_dim)) {
                fprintf(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            rope_head_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--causal") == 0) {
            attention_causal = 1;
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--partial-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &partial_options.token_id)) {
                fprintf(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            segment_options.token_id = partial_options.token_id;
            partial_token_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires comma-separated token IDs\n");
                return 2;
            }
            tokens_text = argv[++i];
            token_input_provided = 1;
        } else if (strcmp(argv[i], "--token-index") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &token_index)) {
                fprintf(stderr, "yvex: --token-index requires a non-negative integer\n");
                return 2;
            }
            token_index_provided = 1;
            i += 1;
        } else if (!model_arg) {
            model_arg = argv[i];
        } else {
            fprintf(stderr, "yvex: unknown graph option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help graph' for usage.\n");
            return 2;
        }
    }

    if (!model_arg && !execute_op) {
        fprintf(stderr, "yvex: graph requires FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda] [--execute-fixture] [--execute-partial] [--execute-segment --segment embedding-rmsnorm] [--partial-token N] [--tokens IDS --token-index N] | yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N | yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal] | yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N\n");
        return 2;
    }
    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if ((execute_fixture ? 1 : 0) + (execute_partial ? 1 : 0) +
        (execute_segment ? 1 : 0) + (execute_op ? 1 : 0) > 1) {
        fprintf(stderr, "yvex: --execute-fixture, --execute-partial, --execute-segment, and --execute-op are mutually exclusive\n");
        return 2;
    }
    if (execute_op) {
        if (model_arg) {
            fprintf(stderr, "yvex: --execute-op does not take a model artifact\n");
            return 2;
        }
        if (!op_name ||
            (strcmp(op_name, "rope") != 0 &&
             strcmp(op_name, "attention") != 0 &&
             strcmp(op_name, "matmul") != 0 &&
             strcmp(op_name, "mlp") != 0)) {
            fprintf(stderr, "yvex: --execute-op requires --op rope, --op attention, --op matmul, or --op mlp\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") == 0) {
            if (!matmul_m_provided || !matmul_k_provided || !matmul_n_provided) {
                fprintf(stderr, "yvex: --execute-op --op matmul requires --m M --k K --n N\n");
                return 2;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || attention_causal) {
                fprintf(stderr, "yvex: --position, --head-dim, --seq-len, and --causal require --op rope or --op attention\n");
                return 2;
            }
        } else if (strcmp(op_name, "mlp") == 0) {
            if (!mlp_hidden_dim_provided || !mlp_ffn_dim_provided ||
                !mlp_activation_provided || !mlp_gated) {
                fprintf(stderr, "yvex: --execute-op --op mlp requires --hidden-dim N --ffn-dim N --activation silu --gated\n");
                return 2;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || attention_causal ||
                matmul_m_provided || matmul_k_provided || matmul_n_provided) {
                fprintf(stderr, "yvex: --op mlp cannot use --position, --head-dim, --seq-len, --causal, --m, --k, or --n\n");
                return 2;
            }
            if (mlp_experts_provided != mlp_expert_id_provided) {
                fprintf(stderr, "yvex: --experts and --expert-id must be provided together\n");
                return 2;
            }
        } else if (!rope_position_provided) {
            fprintf(stderr, "yvex: --execute-op requires --position N\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") != 0 && strcmp(op_name, "mlp") != 0 &&
            !rope_head_dim_provided) {
            fprintf(stderr, "yvex: --execute-op requires --head-dim N\n");
            return 2;
        }
        if (strcmp(op_name, "attention") == 0 && !attention_seq_len_provided) {
            fprintf(stderr, "yvex: --execute-op --op attention requires --seq-len N\n");
            return 2;
        }
        if (strcmp(op_name, "rope") == 0 && (attention_seq_len_provided || attention_causal)) {
            fprintf(stderr, "yvex: --seq-len and --causal require --op attention\n");
            return 2;
        }
        if (strcmp(op_name, "rope") != 0 && strcmp(op_name, "attention") != 0 &&
            (rope_position_provided || rope_head_dim_provided)) {
            fprintf(stderr, "yvex: --position and --head-dim require --op rope or --op attention\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") != 0 &&
            (matmul_m_provided || matmul_k_provided || matmul_n_provided)) {
            fprintf(stderr, "yvex: --m, --k, and --n require --op matmul\n");
            return 2;
        }
        if (strcmp(op_name, "mlp") != 0 &&
            (mlp_hidden_dim_provided || mlp_ffn_dim_provided ||
             mlp_activation_provided || mlp_gated || mlp_experts_provided ||
             mlp_expert_id_provided)) {
            fprintf(stderr, "yvex: --hidden-dim, --ffn-dim, --activation, --gated, --experts, and --expert-id require --op mlp\n");
            return 2;
        }
        if (fixture_token_provided || partial_token_provided || token_input_provided ||
            token_index_provided || segment_name) {
            fprintf(stderr, "yvex: --execute-op cannot be combined with model graph token or segment options\n");
            return 2;
        }
        if (strcmp(op_name, "mlp") == 0) {
            return command_graph_execute_mlp_op(backend_name, mlp_hidden_dim,
                                                mlp_ffn_dim, mlp_activation,
                                                mlp_gated, mlp_experts,
                                                mlp_expert_id,
                                                mlp_experts_provided);
        }
        if (strcmp(op_name, "matmul") == 0) {
            return command_graph_execute_matmul_op(backend_name, matmul_m, matmul_k, matmul_n);
        }
        if (strcmp(op_name, "attention") == 0) {
            return command_graph_execute_attention_op(backend_name, attention_seq_len,
                                                      rope_position, rope_head_dim,
                                                      attention_causal);
        }
        return command_graph_execute_rope_op(backend_name, rope_position, rope_head_dim);
    }
    if (op_name || rope_position_provided || rope_head_dim_provided ||
        attention_seq_len_provided || attention_causal ||
        matmul_m_provided || matmul_k_provided || matmul_n_provided ||
        mlp_hidden_dim_provided || mlp_ffn_dim_provided ||
        mlp_activation_provided || mlp_gated || mlp_experts_provided ||
        mlp_expert_id_provided) {
        fprintf(stderr, "yvex: --op and standalone op options require --execute-op\n");
        return 2;
    }
    if (execute_segment) {
        if (!segment_name) {
            fprintf(stderr, "yvex: --execute-segment requires --segment embedding-rmsnorm\n");
            return 2;
        }
        if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
            fprintf(stderr, "yvex: unsupported segment: %s\n", segment_name);
            return 2;
        }
        segment_options.segment_name = segment_name;
    } else if (segment_name) {
        fprintf(stderr, "yvex: --segment requires --execute-segment\n");
        return 2;
    }
    if (token_index_provided && !token_input_provided) {
        fprintf(stderr, "yvex: --token-index requires --tokens\n");
        return 2;
    }
    if (token_input_provided && !(execute_fixture || execute_partial || execute_segment)) {
        fprintf(stderr, "yvex: --tokens is only supported with graph execution flags\n");
        return 2;
    }
    if (token_input_provided && (partial_token_provided || fixture_token_provided)) {
        fprintf(stderr, "yvex: --tokens cannot be combined with --partial-token or --fixture-token\n");
        return 2;
    }

    if (execute_fixture || execute_partial || execute_segment) {
        rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = enforce_registered_identity_cli(&model_ref,
                                             execute_fixture ? "graph-fixture" :
                                             (execute_segment ? "graph-segment" : "graph-partial"));
        if (rc != YVEX_OK) {
            init_graph_guard_report(&graph_guard,
                                    execute_fixture ? "fixture-embedding" :
                                    (execute_segment ? "selected-embedding-rmsnorm"
                                                     : "selected-embedding-partial"),
                                    !execute_fixture,
                                    &model_ref);
            graph_guard.identity_status = "fail";
            graph_guard.metadata_status = "fail";
            print_graph_guard_report(&graph_guard);
            printf("status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return exit_for_status(rc);
        }

        if (token_input_provided) {
            rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
            if (rc == YVEX_OK) {
                rc = cli_token_input_vocab_from_model(model_ref.path, &token_vocab_size, &err);
            }
            if (rc == YVEX_OK) {
                rc = yvex_token_input_validate_bounds(&token_input, token_vocab_size, &err);
            }
            if (rc == YVEX_OK) {
                rc = yvex_token_input_select(&token_input,
                                             (unsigned long long)token_index,
                                             &selected_token_id,
                                             &err);
            }
            if (rc != YVEX_OK) {
                init_graph_guard_report(&graph_guard,
                                        execute_fixture ? "fixture-embedding" :
                                        (execute_segment ? "selected-embedding-rmsnorm"
                                                         : "selected-embedding-partial"),
                                        !execute_fixture,
                                        &model_ref);
                graph_guard.slice_range_status =
                    token_input.token_bounds_checked && token_input.token_bounds_valid
                        ? "pass"
                        : "fail";
                print_token_input_summary(&token_input,
                                          "fail",
                                          token_input.token_bounds_checked
                                              ? (token_input.token_bounds_valid ? "pass" : "fail")
                                              : "not-checked",
                                          (unsigned long long)token_index,
                                          selected_token_id,
                                          0);
                printf("vocab_size: %llu\n", token_vocab_size);
                print_graph_guard_report(&graph_guard);
                printf("status: graph-integrity-fail\n");
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }

            if (execute_fixture) {
                fixture_options.token_id = selected_token_id;
            } else if (execute_segment) {
                segment_options.token_id = selected_token_id;
                partial_options.token_id = selected_token_id;
            } else {
                partial_options.token_id = selected_token_id;
                segment_options.token_id = selected_token_id;
            }
        }

        rc = preflight_graph_guard(&model_ref,
                                   backend_name,
                                   execute_fixture,
                                   execute_segment,
                                   execute_fixture ? fixture_options.token_id :
                                   (execute_segment ? segment_options.token_id : partial_options.token_id),
                                   &graph_guard,
                                   &err);
        if (rc != YVEX_OK) {
            print_graph_guard_report(&graph_guard);
            printf("status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }

        if (token_input_provided) {
            print_token_input_summary(&token_input,
                                      "pass",
                                      "pass",
                                      (unsigned long long)token_index,
                                      selected_token_id,
                                      1);
            printf("vocab_size: %llu\n", token_vocab_size);
        }

        engine_options.model_path = model_ref.path;
        engine_options.load_tokenizer = 0;
        engine_options.build_descriptor = 1;
        engine_options.build_default_graph = 1;
        engine_options.attach_weights = 1;
        engine_options.backend_name = backend_name;
        engine_options.require_all_weights = 1;

        rc = yvex_engine_open(&engine, &engine_options, &err);
        if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
            graph_guard.guard_status = "fail";
            graph_guard.phase = "preflight";
            graph_guard.backend_status = "unavailable";
            graph_guard.backend_op_status = "unsupported";
            print_graph_guard_report(&graph_guard);
            printf("%s_backend: cuda\n", execute_fixture ? "fixture" :
                   (execute_segment ? "segment" : "partial"));
            printf("%s_backend_status: unsupported\n", execute_fixture ? "fixture" :
                   (execute_segment ? "segment" : "partial"));
            printf("reason: %s\n", yvex_error_message(&err));
            printf("status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return 5;
        }
        if (rc == YVEX_OK && execute_fixture) {
            rc = yvex_engine_execute_fixture_graph(engine, &fixture_options, &fixture_result, &err);
        } else if (rc == YVEX_OK && execute_segment) {
            rc = yvex_engine_execute_segment_graph(engine, &segment_options, &segment_result, &err);
        } else if (rc == YVEX_OK) {
            rc = yvex_engine_execute_partial_graph(engine, &partial_options, &partial_result, &err);
        }
        if (rc != YVEX_OK) {
            if (execute_fixture) {
                graph_guard.phase = fixture_result.graph_execution_phase
                                        ? fixture_result.graph_execution_phase
                                        : "dispatch";
                graph_guard.shape_status = fixture_result.shape_status
                                               ? fixture_result.shape_status
                                               : graph_guard.shape_status;
                graph_guard.range_status = fixture_result.range_status
                                               ? fixture_result.range_status
                                               : graph_guard.range_status;
                graph_guard.slice_range_status = fixture_result.slice_range_status
                                                     ? fixture_result.slice_range_status
                                                     : graph_guard.slice_range_status;
                graph_guard.backend_status = fixture_result.backend_status
                                                  ? fixture_result.backend_status
                                                  : graph_guard.backend_status;
                graph_guard.backend_op_status = fixture_result.backend_op_status
                                                     ? fixture_result.backend_op_status
                                                     : graph_guard.backend_op_status;
                graph_guard.dispatch_attempted = fixture_result.dispatch_attempted;
                graph_guard.reference_read_attempted = fixture_result.reference_read_attempted;
                graph_guard.output_allocation_attempted = fixture_result.output_allocation_attempted;
                graph_guard.cleanup_attempted = fixture_result.cleanup_attempted;
                graph_guard.cleanup_status = fixture_result.cleanup_status
                                                ? fixture_result.cleanup_status
                                                : graph_guard.cleanup_status;
                graph_guard.output_bytes_planned = fixture_result.output_bytes_planned;
                graph_guard.output_bytes_allocated = fixture_result.output_bytes_allocated;
                graph_guard.reference_bytes_planned = fixture_result.reference_bytes_planned;
            } else if (execute_segment) {
                graph_guard.phase = segment_result.graph_execution_phase
                                        ? segment_result.graph_execution_phase
                                        : "dispatch";
                graph_guard.shape_status = segment_result.shape_status
                                               ? segment_result.shape_status
                                               : graph_guard.shape_status;
                graph_guard.range_status = segment_result.range_status
                                               ? segment_result.range_status
                                               : graph_guard.range_status;
                graph_guard.slice_range_status = segment_result.slice_range_status
                                                     ? segment_result.slice_range_status
                                                     : graph_guard.slice_range_status;
                graph_guard.backend_status = segment_result.backend_status
                                                  ? segment_result.backend_status
                                                  : graph_guard.backend_status;
                graph_guard.backend_op_status = segment_result.backend_op_status
                                                     ? segment_result.backend_op_status
                                                     : graph_guard.backend_op_status;
                graph_guard.dispatch_attempted = segment_result.dispatch_attempted;
                graph_guard.reference_read_attempted = segment_result.reference_read_attempted;
                graph_guard.output_allocation_attempted = segment_result.output_allocation_attempted;
                graph_guard.cleanup_attempted = segment_result.cleanup_attempted;
                graph_guard.cleanup_status = segment_result.cleanup_status
                                                ? segment_result.cleanup_status
                                                : graph_guard.cleanup_status;
                graph_guard.output_bytes_planned = segment_result.output_bytes_planned;
                graph_guard.output_bytes_allocated = segment_result.output_bytes_allocated;
                graph_guard.reference_bytes_planned = segment_result.reference_bytes_planned;
            } else {
                graph_guard.phase = partial_result.graph_execution_phase
                                        ? partial_result.graph_execution_phase
                                        : "dispatch";
                graph_guard.shape_status = partial_result.shape_status
                                               ? partial_result.shape_status
                                               : graph_guard.shape_status;
                graph_guard.range_status = partial_result.range_status
                                               ? partial_result.range_status
                                               : graph_guard.range_status;
                graph_guard.slice_range_status = partial_result.slice_range_status
                                                     ? partial_result.slice_range_status
                                                     : graph_guard.slice_range_status;
                graph_guard.backend_status = partial_result.backend_status
                                                  ? partial_result.backend_status
                                                  : graph_guard.backend_status;
                graph_guard.backend_op_status = partial_result.backend_op_status
                                                     ? partial_result.backend_op_status
                                                     : graph_guard.backend_op_status;
                graph_guard.dispatch_attempted = partial_result.dispatch_attempted;
                graph_guard.reference_read_attempted = partial_result.reference_read_attempted;
                graph_guard.output_allocation_attempted = partial_result.output_allocation_attempted;
                graph_guard.cleanup_attempted = partial_result.cleanup_attempted;
                graph_guard.cleanup_status = partial_result.cleanup_status
                                                ? partial_result.cleanup_status
                                                : graph_guard.cleanup_status;
                graph_guard.output_bytes_planned = partial_result.output_bytes_planned;
                graph_guard.output_bytes_allocated = partial_result.output_bytes_allocated;
                graph_guard.reference_bytes_planned = partial_result.reference_bytes_planned;
            }
            graph_guard.guard_status = "fail";
            print_graph_guard_report(&graph_guard);
            printf("status: %s\n",
                   graph_guard.cleanup_attempted ? "graph-failed-cleaned" : "graph-integrity-fail");
            yvex_engine_close(engine);
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }

        if (execute_fixture) {
            graph_guard.guard_status = fixture_result.graph_integrity_guard
                                           ? fixture_result.graph_integrity_guard
                                           : "pass";
            graph_guard.phase = fixture_result.graph_execution_phase
                                    ? fixture_result.graph_execution_phase
                                    : "complete";
            graph_guard.shape_status = fixture_result.shape_status ? fixture_result.shape_status : "pass";
            graph_guard.range_status = fixture_result.range_status ? fixture_result.range_status : "pass";
            graph_guard.slice_range_status = fixture_result.slice_range_status
                                                 ? fixture_result.slice_range_status
                                                 : "not-needed";
            graph_guard.backend_status = fixture_result.backend_status ? fixture_result.backend_status : "ready";
            graph_guard.backend_op_status = fixture_result.backend_op_status
                                                ? fixture_result.backend_op_status
                                                : "supported";
            graph_guard.dispatch_attempted = fixture_result.dispatch_attempted;
            graph_guard.reference_read_attempted = fixture_result.reference_read_attempted;
            graph_guard.output_allocation_attempted = fixture_result.output_allocation_attempted;
            graph_guard.cleanup_attempted = fixture_result.cleanup_attempted;
            graph_guard.cleanup_status = fixture_result.cleanup_status
                                            ? fixture_result.cleanup_status
                                            : "not-needed";
            graph_guard.output_bytes_planned = fixture_result.output_bytes_planned;
            graph_guard.output_bytes_allocated = fixture_result.output_bytes_allocated;
            graph_guard.reference_bytes_planned = fixture_result.reference_bytes_planned;
            print_graph_guard_report(&graph_guard);
            printf("fixture_graph_executed: true\n");
            printf("fixture_backend: %s\n", fixture_result.backend_name);
            printf("fixture_op: %s\n", fixture_result.op_name);
            printf("fixture_weight: %s\n", fixture_result.weight_name);
            printf("fixture_token_id: %u\n", fixture_result.token_id);
            printf("fixture_node_count: %llu\n", fixture_result.node_count);
            printf("fixture_output_count: %llu\n", fixture_result.output_count);
            printf("fixture_output_bytes: %llu\n", fixture_result.output_bytes);
            printf("fixture_output_checksum: %llu\n", fixture_result.output_checksum);
            printf("fixture_output_values:");
            for (i = 0; (unsigned long long)i < fixture_result.output_value_count; ++i) {
                printf("%s%.9g", i == 0 ? " " : ",", (double)fixture_result.output_values[i]);
            }
            printf("\n");
            printf("execution_ready: false\n");
            printf("graph_execution_ready: false\n");
            printf("status: fixture-graph-executed\n");
        } else if (execute_segment) {
            graph_guard.guard_status = segment_result.graph_integrity_guard
                                           ? segment_result.graph_integrity_guard
                                           : "pass";
            graph_guard.phase = segment_result.graph_execution_phase
                                    ? segment_result.graph_execution_phase
                                    : "complete";
            graph_guard.shape_status = segment_result.shape_status ? segment_result.shape_status : "pass";
            graph_guard.range_status = segment_result.range_status ? segment_result.range_status : "pass";
            graph_guard.slice_range_status = segment_result.slice_range_status
                                                 ? segment_result.slice_range_status
                                                 : "pass";
            graph_guard.backend_status = segment_result.backend_status ? segment_result.backend_status : "ready";
            graph_guard.backend_op_status = segment_result.backend_op_status
                                                ? segment_result.backend_op_status
                                                : "supported";
            graph_guard.dispatch_attempted = segment_result.dispatch_attempted;
            graph_guard.reference_read_attempted = segment_result.reference_read_attempted;
            graph_guard.output_allocation_attempted = segment_result.output_allocation_attempted;
            graph_guard.cleanup_attempted = segment_result.cleanup_attempted;
            graph_guard.cleanup_status = segment_result.cleanup_status
                                            ? segment_result.cleanup_status
                                            : "not-needed";
            graph_guard.output_bytes_planned = segment_result.output_bytes_planned;
            graph_guard.output_bytes_allocated = segment_result.output_bytes_allocated;
            graph_guard.reference_bytes_planned = segment_result.reference_bytes_planned;
            print_graph_guard_report(&graph_guard);
            printf("segment_graph_executed: true\n");
            printf("segment_backend: %s\n", segment_result.backend_name);
            printf("segment_name: %s\n", segment_result.segment_name);
            printf("segment_ops: %llu\n", segment_result.segment_ops);
            printf("segment_op_0: embed\n");
            printf("segment_op_1: rms_norm\n");
            printf("partial_token: %u\n", segment_result.token_id);
            printf("token_tensor: %s\n", segment_result.token_tensor_name);
            printf("token_tensor_dtype: %s\n", segment_result.token_tensor_dtype);
            printf("rmsnorm_tensor: %s\n", segment_result.rmsnorm_tensor_name);
            printf("rmsnorm_tensor_dtype: %s\n", segment_result.rmsnorm_tensor_dtype);
            printf("hidden_size: %llu\n", segment_result.hidden_size);
            printf("vocab_size: %llu\n", segment_result.vocab_size);
            printf("rmsnorm_epsilon_key: %s\n", segment_result.rmsnorm_epsilon_key);
            printf("rmsnorm_epsilon: %.9g\n", segment_result.rmsnorm_epsilon);
            printf("segment_memory_plan: explicit\n");
            printf("segment_intermediate_count: %llu\n", segment_result.segment_intermediate_count);
            printf("segment_intermediate_bytes: %llu\n", segment_result.segment_intermediate_bytes);
            printf("segment_output_count: %llu\n", segment_result.segment_output_count);
            printf("segment_output_bytes: %llu\n", segment_result.segment_output_bytes);
            printf("segment_scratch_bytes: %llu\n", segment_result.segment_scratch_bytes);
            printf("segment_reference_bytes: %llu\n", segment_result.segment_reference_bytes);
            printf("segment_output_checksum: %llu\n", segment_result.output_checksum);
            printf("segment_reference_checksum: %llu\n", segment_result.reference_checksum);
            printf("segment_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            if (strcmp(segment_result.backend_name, "cuda") == 0) {
                printf("segment_cuda_parity: pass\n");
                printf("cuda_reference_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            } else {
                printf("cpu_reference_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            }
            printf("segment_output_sample_count: %llu\n", segment_result.output_value_count);
            printf("segment_output_sample_values:");
            for (i = 0; (unsigned long long)i < segment_result.output_value_count; ++i) {
                printf("%s%.9g", i == 0 ? " " : ",", (double)segment_result.output_values[i]);
            }
            printf("\n");
            printf("execution_ready: false\n");
            printf("graph_execution_ready: false\n");
            printf("prefill_ready: false\n");
            printf("logits_ready: false\n");
            printf("generation: unsupported\n");
            printf("status: real-segment-graph-executed\n");
        } else {
            graph_guard.guard_status = partial_result.graph_integrity_guard
                                           ? partial_result.graph_integrity_guard
                                           : "pass";
            graph_guard.phase = partial_result.graph_execution_phase
                                    ? partial_result.graph_execution_phase
                                    : "complete";
            graph_guard.shape_status = partial_result.shape_status ? partial_result.shape_status : "pass";
            graph_guard.range_status = partial_result.range_status ? partial_result.range_status : "pass";
            graph_guard.slice_range_status = partial_result.slice_range_status
                                                 ? partial_result.slice_range_status
                                                 : "pass";
            graph_guard.backend_status = partial_result.backend_status ? partial_result.backend_status : "ready";
            graph_guard.backend_op_status = partial_result.backend_op_status
                                                ? partial_result.backend_op_status
                                                : "supported";
            graph_guard.dispatch_attempted = partial_result.dispatch_attempted;
            graph_guard.reference_read_attempted = partial_result.reference_read_attempted;
            graph_guard.output_allocation_attempted = partial_result.output_allocation_attempted;
            graph_guard.cleanup_attempted = partial_result.cleanup_attempted;
            graph_guard.cleanup_status = partial_result.cleanup_status
                                            ? partial_result.cleanup_status
                                            : "not-needed";
            graph_guard.output_bytes_planned = partial_result.output_bytes_planned;
            graph_guard.output_bytes_allocated = partial_result.output_bytes_allocated;
            graph_guard.reference_bytes_planned = partial_result.reference_bytes_planned;
            print_graph_guard_report(&graph_guard);
            printf("real_partial_graph_executed: true\n");
            printf("partial_graph_kind: %s\n", partial_result.segment_name);
            printf("partial_backend: %s\n", partial_result.backend_name);
            printf("partial_weight: %s\n", partial_result.weight_name);
            printf("partial_weight_dtype: %s\n", partial_result.weight_dtype);
            printf("partial_token: %u\n", partial_result.token_id);
            printf("partial_node_count: %llu\n", partial_result.node_count);
            printf("partial_output_dtype: %s\n", partial_result.output_dtype);
            printf("partial_output_count: %llu\n", partial_result.output_count);
            printf("partial_output_bytes: %llu\n", partial_result.output_bytes);
            printf("partial_output_checksum: %llu\n", partial_result.output_checksum);
            printf("partial_reference_checksum: %llu\n", partial_result.reference_checksum);
            printf("partial_max_abs_diff: %.9g\n", partial_result.max_abs_diff);
            printf("partial_output_sample_count: %llu\n", partial_result.output_value_count);
            printf("partial_output_sample_values:");
            for (i = 0; (unsigned long long)i < partial_result.output_value_count; ++i) {
                printf("%s%.9g", i == 0 ? " " : ",", (double)partial_result.output_values[i]);
            }
            printf("\n");
            printf("execution_ready: false\n");
            printf("graph_execution_ready: false\n");
            printf("prefill_ready: false\n");
            printf("logits_ready: false\n");
            printf("generation: unsupported\n");
            printf("status: real-partial-graph-executed\n");
        }

        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return 0;
    }

    rc = open_model_context(model_arg, &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_graph_build_for_model(&graph, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_graph_dump(graph, stdout, &err);
    }

    yvex_graph_close(graph);
    close_model_context(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

int yvex_cli_command_graph(int argc, char **argv)
{
    return command_graph(argc, argv);
}
