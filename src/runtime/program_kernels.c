/* Bind exact parameter handles and prepare reusable numerical implementations.
 * Neither tensor roles nor model/family names participate in invocation. */
#include <yvex/internal/program_kernels.h>
#include <yvex/internal/component.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>
#include <yvex/internal/convolution.h>

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <math.h>

typedef struct {
    yvex_transformer_linear_requirement requirement;
    yvex_transformer_linear_executable *single, *multiple;
    yvex_ir_extent population;
    unsigned long long prepared_multiple_rows;
} program_kernel_linear;

struct yvex_program_kernels {
    yvex_program_physical *program;
    const yvex_program_physical_summary *summary;
    yvex_backend *backend;
    const yvex_backend_transformer_operations *ops;
    yvex_component_encoded_weight *weights;
    yvex_program_kernel_parameter *sources;
    unsigned char *source_staging;
    unsigned long long source_staging_bytes;
    yvex_device_tensor **small;
    double *offsets;
    program_kernel_linear *linears;
    size_t *linear_slots, linear_count;
    unsigned long long host_bytes, device_bytes, host_limit, device_limit, multiple_rows;
    int single_ready, multiple_ready;
    float *host_staging;
    unsigned long long host_staging_bytes;
    yvex_device_tensor *signal_workspace;
};

static int kernel_refuse(yvex_error *err, yvex_status status, const char *why)
{
    yvex_error_set(err, status, "runtime.program.kernel", why);
    return status;
}

static int kernel_account(yvex_program_kernels *c, unsigned long long host, unsigned long long device,
                           yvex_error *err)
{
    unsigned long long h, d;
    if (!yvex_core_u64_add(c->host_bytes, host, &h) || !yvex_core_u64_add(c->device_bytes, device, &d) ||
        (c->host_limit && h > c->host_limit) || (c->device_limit && d > c->device_limit))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "prepared operation resources exceed admission budget");
    c->host_bytes = h;
    c->device_bytes = d;
    return YVEX_OK;
}

static int kernel_parameters_bind(yvex_program_kernels *c, const yvex_program_kernel_parameter *parameters,
                                   size_t parameter_count, yvex_error *err)
{
    size_t i, j;
    for (i = 0u; i < c->summary->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(c->program, i);
        const yvex_component_encoded_weight *binding = NULL;
        const yvex_program_kernel_parameter *source = NULL;
        const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(v->qtype);
        unsigned long long bytes, row_bytes, elements = 1u;
        size_t matches = 0u;
        unsigned int axis;
        if (!v->parameter) continue;
        for (j = 0u; j < parameter_count; ++j) {
            if (parameters[j].tensor_id == v->tensor_id) {
                source = parameters + j; binding = &source->weight; matches++;
            }
        }
        if (matches != 1u || !binding || binding->qtype != v->qtype || !binding->row_count ||
            binding->row_width != v->type.shape[v->type.rank - 1u].extent) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "runtime.program.kernel.parameter",
                "parameter %llu: matches=%zu qtype=%u/%u row_width=%llu/%llu rows=%llu",
                v->tensor_id, matches, binding ? binding->qtype : UINT_MAX, v->qtype,
                binding ? binding->row_width : 0u, v->type.shape[v->type.rank - 1u].extent,
                binding ? binding->row_count : 0u);
            return YVEX_ERR_FORMAT;
        }
        for (axis = 0u; axis < v->type.rank; ++axis)
            if (!yvex_core_u64_mul(elements, v->type.shape[axis].extent, &elements))
                return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter shape differs from artifact storage");
        if (!geometry || !geometry->block_size || !geometry->bytes_per_block ||
            binding->row_width % geometry->block_size ||
            !yvex_core_u64_mul(binding->row_width / geometry->block_size, geometry->bytes_per_block, &row_bytes) ||
            !yvex_core_u64_mul(elements / binding->row_width, row_bytes, &bytes) || bytes != binding->encoded_bytes)
            return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter block geometry is inconsistent");
        if (!!binding->encoded == !!source->read ||
            (source->read && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) ||
            binding->row_count != elements / binding->row_width ||
            binding->row_bytes != row_bytes)
            return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter residency is incomplete");
        c->weights[i] = *binding;
        c->sources[i] = *source;
    }
    return YVEX_OK;
}

static int kernel_sources_prepare(yvex_program_kernels *c, yvex_error *err)
{
    unsigned long long maximum = 0u;
    for (size_t i = 0u; i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        unsigned long long bytes = 0u;
        for (size_t j = 0u; j < s->operand_count; ++j) {
            const yvex_program_kernel_parameter *p = c->sources + s->operands[j];
            if (p->read && !yvex_core_u64_add(bytes, p->weight.encoded_bytes, &bytes))
                return kernel_refuse(err, YVEX_ERR_BOUNDS, "parameter staging extent overflowed");
        }
        if (bytes > maximum) maximum = bytes;
    }
    if (!maximum) return YVEX_OK;
    if (maximum > SIZE_MAX || (c->host_limit && maximum > c->host_limit - c->host_bytes))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "bounded parameter staging exceeds host budget");
    c->source_staging = malloc((size_t)maximum);
    if (!c->source_staging) return kernel_refuse(err, YVEX_ERR_NOMEM, "parameter staging allocation failed");
    c->source_staging_bytes = maximum;
    return kernel_account(c, maximum, 0u, err);
}

static int kernel_source_read(yvex_program_kernels *c, yvex_ir_id id,
    unsigned long long *offset, yvex_error *err)
{
    const yvex_program_kernel_parameter *source = c->sources + id;
    if (!source->read) return YVEX_OK;
    unsigned long long bytes = source->weight.encoded_bytes;
    c->weights[id].encoded = NULL;
    if (*offset > c->source_staging_bytes || bytes > c->source_staging_bytes - *offset)
        return kernel_refuse(err, YVEX_ERR_STATE, "compiled parameter staging is unavailable");
    int rc = source->read(source->read_context, source->tensor_id, 0u,
        c->source_staging + *offset, (size_t)bytes, err);
    if (rc != YVEX_OK) return rc;
    c->weights[id].encoded = c->source_staging + *offset;
    *offset += bytes;
    return YVEX_OK;
}

static int kernel_small_prepare(yvex_program_kernels *c, yvex_ir_id slot, double offset, yvex_error *err)
{
    const yvex_component_encoded_weight *w = &c->weights[slot];
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(w->qtype);
    yvex_backend_tensor_desc d = {.name = "program-parameter", .dtype = YVEX_DTYPE_F32, .rank = 1u};
    unsigned long long i, count;
    float *host;
    int rc;
    if (c->small[slot]) return c->offsets[slot] == offset ? YVEX_OK :
        kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "parameter requires distinct prepared normalization representations");
    unsigned long long staging_offset = 0u;
    rc = kernel_source_read(c, slot, &staging_offset, err);
    if (rc != YVEX_OK) return rc;
    if (!w->encoded || !geometry || !geometry->block_size || !geometry->bytes_per_block ||
        !yvex_core_u64_mul(w->row_count, w->row_width, &count) || !count ||
        count % geometry->block_size ||
        !yvex_core_u64_mul(count, sizeof(float), &d.bytes) || d.bytes > SIZE_MAX)
        return kernel_refuse(err, YVEX_ERR_FORMAT, "small parameter has invalid encoded geometry");
    /* This bounded temporary host decoding is preparation, not a retained mirror. */
    if ((c->host_limit && d.bytes > c->host_limit - c->host_bytes) ||
        (c->device_limit && d.bytes > c->device_limit - c->device_bytes))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "small parameter preparation exceeds resource budget");
    host = malloc((size_t)d.bytes);
    if (!host) return kernel_refuse(err, YVEX_ERR_NOMEM, "small parameter preparation allocation failed");
    rc = YVEX_OK;
    for (i = 0u; rc == YVEX_OK && i < count / geometry->block_size; ++i) {
        yvex_quant_failure failure = {0};
        rc = yvex_quant_decode_block(w->qtype, w->encoded + i * geometry->bytes_per_block,
            geometry->bytes_per_block, host + i * geometry->block_size, geometry->block_size, &failure, err);
    }
    for (i = 0u; rc == YVEX_OK && offset != 0.0 && i < count; ++i) host[i] += (float)offset;
    d.dims[0] = count;
    if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(c->backend, &d, &c->small[slot], err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_write(c->backend, c->small[slot], host, d.bytes, err);
    if (rc == YVEX_OK) rc = kernel_account(c, 0u, d.bytes, err);
    if (rc == YVEX_OK) c->offsets[slot] = offset;
    free(host);
    return rc;
}

static int kernel_unit_prepare(yvex_program_kernels *c, yvex_ir_id slot,
    unsigned long long width, yvex_error *err)
{
    yvex_backend_tensor_desc d = {.name = "program-unit-scale", .dtype = YVEX_DTYPE_F32, .rank = 1u};
    if (!width || !yvex_core_u64_mul(width, sizeof(float), &d.bytes) || d.bytes > SIZE_MAX ||
        (c->host_limit && d.bytes > c->host_limit - c->host_bytes) ||
        (c->device_limit && d.bytes > c->device_limit - c->device_bytes))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "unweighted normalization preparation exceeds resource budget");
    float *host = malloc((size_t)d.bytes);
    if (!host) return kernel_refuse(err, YVEX_ERR_NOMEM, "unit scale preparation allocation failed");
    for (unsigned long long i = 0u; i < width; ++i) host[i] = 1.0f;
    d.dims[0] = width;
    int rc = yvex_backend_tensor_alloc(c->backend, &d, &c->small[slot], err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_write(c->backend, c->small[slot], host, d.bytes, err);
    if (rc == YVEX_OK) rc = kernel_account(c, 0u, d.bytes, err);
    free(host);
    return rc;
}

static yvex_transformer_attention_requirement kernel_attention_requirement(
    const yvex_program_kernels *c, const yvex_program_physical_step *s, unsigned long long rows)
{
    unsigned long long head = yvex_program_physical_attribute(s, "head_dimension")->value.integer;
    const yvex_ir_type *q = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
    const yvex_ir_type *k = &yvex_program_physical_value_at(c->program, s->operands[1])->type;
    return (yvex_transformer_attention_requirement){
        .query_tokens = q->shape[0].symbol == YVEX_IR_NONE ? q->shape[0].extent : rows,
        .key_value_tokens = k->shape[0].symbol == YVEX_IR_NONE ? k->shape[0].extent : rows,
        .query_heads = q->shape[1].extent / head, .key_value_heads = k->shape[1].extent / head,
        .head_dimension = head, .query_dtype = YVEX_DTYPE_F32, .key_dtype = YVEX_DTYPE_F32,
        .value_dtype = YVEX_DTYPE_F32, .output_dtype = YVEX_DTYPE_F32,
        .layout = YVEX_TRANSFORMER_ATTENTION_LAYOUT_TOKEN_HEAD_DIM,
        .mask = yvex_program_physical_attribute(s, "causal")->value.integer ?
            YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL : YVEX_TRANSFORMER_ATTENTION_MASK_FULL,
        .numeric_contract = YVEX_TRANSFORMER_ATTENTION_NUMERIC_EXACT_F32, .deterministic = 1};
}

static int kernel_signal_kind(const char *implementation)
{
    if (!strcmp(implementation, "conv1d.f32.v1")) return 1;
    if (!strcmp(implementation, "normalized_conv1d.f32.v1")) return 2;
    if (!strcmp(implementation, "normalized_conv1d_unbiased.f32.v1")) return 3;
    if (!strcmp(implementation, "alias_snake.f32.v1")) return 4;
    return 0;
}

static int kernel_signal_prepare(yvex_program_kernels *c, yvex_error *err)
{
    unsigned long long maximum = 0u;
    for (size_t i = 0u; i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        int kind = kernel_signal_kind(s->implementation);
        if (!kind) continue;
        if (!c->ops || (kind == 4 ? !c->ops->alias_snake : !c->ops->convolution_1d))
            return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "signal operation has no admitted backend implementation");
        unsigned long long count = 0u;
        if (kind == 4) {
            const yvex_ir_type *x = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
            count = x->shape[0].symbol == YVEX_IR_NONE ? x->shape[0].extent : c->summary->maximum_rows;
            if (!yvex_core_u64_mul(count, x->shape[1].extent, &count) ||
                !yvex_core_u64_mul(count, x->shape[2].extent, &count) || !yvex_core_u64_mul(count, 2u, &count))
                return kernel_refuse(err, YVEX_ERR_BOUNDS, "signal scratch population overflowed");
        } else if (kind != 1) {
            count = yvex_program_physical_value_at(c->program, s->operands[1])->type.shape[0].extent;
        }
        if (count > maximum) maximum = count;
    }
    if (!maximum) return YVEX_OK;
    yvex_backend_tensor_desc d = {.name = "program-signal-workspace", .dtype = YVEX_DTYPE_F32,
        .rank = 1u, .dims = {maximum}};
    if (!yvex_core_u64_mul(maximum, sizeof(float), &d.bytes) || d.bytes > SIZE_MAX ||
        (c->device_limit && d.bytes > c->device_limit - c->device_bytes))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "signal workspace exceeds preparation budget");
    int rc = yvex_backend_tensor_alloc(c->backend, &d, &c->signal_workspace, err);
    return rc == YVEX_OK ? kernel_account(c, 0u, d.bytes, err) : rc;
}

static int kernel_f32_bind(yvex_program_kernels *c, const yvex_program_physical_step *s,
    int *handled, yvex_error *err)
{
    *handled = 1;
    if (!strncmp(s->implementation, "linear_bias.cuda.", 17u)) {
        if (!c->ops || !c->ops->linear_bias_target) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "cast.exact.f32.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "cast.rne.bf16.v1")) {
        if (!c->ops || !c->ops->bf16_round) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "silu.bf16.v1") || !strcmp(s->implementation, "silu.f32.v1")) {
        if (!c->ops || !c->ops->silu) goto unavailable;
        return YVEX_OK;
    }
    if (kernel_signal_kind(s->implementation)) return YVEX_OK;
    if (!strcmp(s->implementation, "conv2d_slice.f32.v1")) {
        if (!c->ops || !c->ops->convolution_2d) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "spatial_group_norm_silu.f32.v1")) {
        if (!c->ops || !c->ops->spatial_norm_silu) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "add.f32.v1") || !strcmp(s->implementation, "mean.f32.v1")) {
        if (!c->ops || !c->ops->combine_f32) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "clamp.f32.v1")) {
        if (!c->ops || !c->ops->clamp_f32) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "clamped_swiglu.f64math.bf16.v1")) {
        if (!c->ops || !c->ops->clamped_swiglu_bf16) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "linear_bias.f32.v1")) {
        if (!c->ops || !c->ops->linear_bias_f32) goto unavailable;
        return kernel_small_prepare(c, s->operands[2], 0.0, err);
    }
    if (!strcmp(s->implementation, "slice_rows.f32.v1") ||
        !strcmp(s->implementation, "index_linearize.host.u32.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "zeros.f32.v1") ||
        !strcmp(s->implementation, "concat_rows.f32.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "parameter_copy.f32.v1"))
        return kernel_small_prepare(c, s->operands[0], 0.0, err);
    if (!strcmp(s->implementation, "rms_normalize.f32.v1")) {
        if (!c->ops || !c->ops->normalization_f32) goto unavailable;
        return kernel_unit_prepare(c, s->results[0],
            yvex_program_physical_attribute(s, "group_width")->value.integer, err);
    }
    if (!strcmp(s->implementation, "rms_norm.f32.v1")) {
        if (!c->ops || !c->ops->normalization_f32) goto unavailable;
        return kernel_small_prepare(c, s->operands[1],
            yvex_program_physical_attribute(s, "weight_offset")->value.real, err);
    }
    if (!strcmp(s->implementation, "channel_bias.f32.v1")) {
        if (!c->ops || !c->ops->channel_bias) goto unavailable;
        return kernel_small_prepare(c, s->operands[1], 0.0, err);
    }
    if (!strcmp(s->implementation, "scaled_residual.f32.v1")) {
        if (!c->ops || !c->ops->scaled_residual_f32) goto unavailable;
        return kernel_small_prepare(c, s->operands[2], 0.0, err);
    }
    if (!strcmp(s->implementation, "layer_norm.f32.v1")) {
        if (!c->ops || !c->ops->normalization_f32) goto unavailable;
        int rc = kernel_small_prepare(c, s->operands[1],
            yvex_program_physical_attribute(s, "weight_offset")->value.real, err);
        return rc == YVEX_OK ? kernel_small_prepare(c, s->operands[2], 0.0, err) : rc;
    }
    if (!strcmp(s->implementation, "rotary_half.f32.v1")) {
        if (!c->ops || !c->ops->rotary_half_f32) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "attention.full.f32.v1")) {
        if (!c->ops || !c->ops->attention_execute) goto unavailable;
        if (yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU) {
            if (!c->ops->attention_workspace_required) goto unavailable;
            yvex_transformer_attention_requirement requirement =
                kernel_attention_requirement(c, s, c->summary->maximum_rows);
            yvex_backend_tensor_desc d = {.name = "program-attention-workspace", .dtype = YVEX_DTYPE_F32,
                .rank = 1u};
            int rc = c->ops->attention_workspace_required(&requirement, &d.bytes, err);
            if (rc != YVEX_OK) return rc;
            if (!d.bytes || d.bytes % sizeof(float) ||
                (c->device_limit && d.bytes > c->device_limit - c->device_bytes))
                return kernel_refuse(err, YVEX_ERR_BOUNDS, "attention workspace exceeds preparation budget");
            d.dims[0] = d.bytes / sizeof(float);
            rc = yvex_backend_tensor_alloc(c->backend, &d, &c->small[s->results[0]], err);
            return rc == YVEX_OK ? kernel_account(c, 0u, d.bytes, err) : rc;
        }
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "split_interleaved_three.f32.v1") ||
        !strcmp(s->implementation, "split_interleaved_three.bf16.v1")) {
        if (!c->ops || !c->ops->split_interleaved_three) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "swiglu_split.f32.v1")) {
        if (!c->ops || !c->ops->swiglu_split_f32) goto unavailable;
        return YVEX_OK;
    }
    if (!strcmp(s->implementation, "swiglu_split.bf16.v1")) {
        if (!c->ops || !c->ops->swiglu_split_bf16) goto unavailable;
        return YVEX_OK;
    }
    *handled = 0;
    return YVEX_OK;
unavailable:
    return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "admitted F32 operation implementation unavailable");
}

static int kernel_instructions_bind(yvex_program_kernels *c, yvex_error *err)
{
    size_t i, j;
    int rc = YVEX_OK;
    for (i = 0u; rc == YVEX_OK && i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        c->linear_slots[i] = SIZE_MAX;
        int handled;
        rc = kernel_f32_bind(c, s, &handled, err);
        if (rc != YVEX_OK || handled) continue;
        if (!strcmp(s->implementation, "axis_rotary.f32math.bf16.v1")) continue;
        if (!strcmp(s->implementation, "observe.f32-storage.v1")) continue;
        if (!strcmp(s->implementation, "sinusoidal_embedding.f32.v1")) {
            double period = yvex_program_physical_attribute(s, "maximum_period")->value.real;
            if (!c->ops || !c->ops->sinusoidal_embedding || !isfinite((float)period) || (float)period <= 1.0f)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "sinusoidal frequency period is not executable");
            continue;
        }
        if (!strncmp(s->implementation, "indexed_rows.", 13u) ||
            !strncmp(s->implementation, "partition_rows.", 15u)) continue;
        if (!strcmp(s->implementation, "indexed_modulate.bf16.v1") ||
            !strcmp(s->implementation, "indexed_gated_residual.bf16.v1")) {
            int gated = s->operand_count == 4u;
            if (!c->ops || (gated ? !c->ops->gated_residual_bf16 : !c->ops->modulate_bf16))
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "indexed conditioning implementation unavailable");
            const yvex_program_physical_value *table =
                yvex_program_physical_value_at(c->program, s->operands[1]);
            if (table->type.shape[1].extent > UINT_MAX)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "conditioning slots exceed implementation range");
            continue;
        }
        if (!strcmp(s->implementation, "linear.row_dot.q8.v1")) {
            if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CUDA)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "Q8 row-dot target requires its CUDA implementation");
            continue;
        }
        if (!strcmp(s->implementation, "parameter.encoded.v1") ||
            !strcmp(s->implementation, "reshape.f32-storage.v1") ||
            !strcmp(s->implementation, "grid_rotary.f32.v1") ||
            !strcmp(s->implementation, "grid_bilinear.f64weights.bf16.v1") ||
            !strcmp(s->implementation, "masked_rows.host.bf16.v1") ||
            !strcmp(s->implementation, "rotary_tables.f64.bf16.v1") ||
            !strcmp(s->implementation, "embedding.encoded.f32.v1") ||
            !strcmp(s->implementation, "linear.encoded.f32.v1") ||
            !strcmp(s->implementation, "linear.row_dot.f32.v1")) continue;
        if (!strcmp(s->implementation, "linear_residual.bf16.f32add.v1")) {
            if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU && (!c->ops || !c->ops->bf16_round))
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "linear residual requires exact BF16 publication");
            continue;
        }
        if (!strcmp(s->implementation, "stream_mean.f32.f64acc.v1")) {
            if (!c->ops || !c->ops->feature_mean)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "stream mean has no admitted backend implementation");
            continue;
        }
        if (!strcmp(s->implementation, "mhc.residual_post.f64acc.bf16.v1")) {
            if (!c->ops || !c->ops->residual_post)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "mHC post has no admitted backend implementation");
            continue;
        }
        if (!strcmp(s->implementation, "mhc.head_norm.bf16.v1")) {
            if (!c->ops || !c->ops->final)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "mHC head has no admitted backend implementation");
            for (j = 1u; rc == YVEX_OK && j < 5u; ++j) rc = kernel_small_prepare(c, s->operands[j], 0.0, err);
            continue;
        }
        if (!strcmp(s->implementation, "mhc.residual_pre.bf16.v1")) {
            if (!c->ops || !c->ops->residual_pre)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "mHC ingress has no admitted backend implementation");
            for (j = 2u; rc == YVEX_OK && j < 4u; ++j) rc = kernel_small_prepare(c, s->operands[j], 0.0, err);
            continue;
        }
        if (!strcmp(s->implementation, "selective_ssd.cpu.f32state.v1")) {
            for (j = 1u; rc == YVEX_OK && j < 7u; ++j)
                rc = kernel_small_prepare(c, s->operands[j], 0.0, err);
            continue;
        }
        if (!strcmp(s->implementation, "weighted_rms.f64scale.bf16.v1")) {
            if (!c->ops || !c->ops->weighted_rms_bf16)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "weighted RMS numerical implementation unavailable");
            rc = kernel_small_prepare(c, s->operands[1], 0.0, err);
            continue;
        }
        if (!strcmp(s->implementation, "group_rms_norm.bf16.vector4.v1")) {
            if (!c->ops || !c->ops->group_rms_norm_bf16)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED,
                    "group normalization has no admitted numerical implementation");
            rc = kernel_small_prepare(c, s->operands[1], 0.0, err);
            continue;
        }
        if (!strcmp(s->implementation, "linear_bias.bf16.f32acc.v1")) {
            if (!c->ops || !c->ops->linear_bias_bf16)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "biased projection implementation unavailable");
            continue;
        }
        if (!strcmp(s->implementation, "layer_norm.f32acc.bf16.v1")) {
            if (!c->ops || !c->ops->layer_norm_f32 || !c->ops->bf16_round)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "layer normalization implementation unavailable");
            rc = kernel_small_prepare(c, s->operands[1],
                yvex_program_physical_attribute(s, "weight_offset")->value.real, err);
            if (rc == YVEX_OK) rc = kernel_small_prepare(c, s->operands[2], 0.0, err);
            continue;
        }
        if (!strcmp(s->implementation, "gelu.erf.bf16.v1") || !strcmp(s->implementation, "gelu.tanh.bf16.v1")) {
            if (!c->ops || !c->ops->gelu)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "GELU implementation unavailable");
            continue;
        }
        if (!strcmp(s->implementation, "split_three.bf16.v1")) {
            if (!c->ops || !c->ops->split_three)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "channel partition implementation unavailable");
            continue;
        }
        if (!strcmp(s->implementation, "rotary_half.f32acc.bf16.v1")) {
            if (!c->ops || !c->ops->rotary_half_f32 || !c->ops->bf16_round)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "F32-table rotary implementation unavailable");
            continue;
        }
        if (!strcmp(s->implementation, "rotary_half.bf16.products.v1")) {
            if (!c->ops || !c->ops->rotary_half_bf16)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "rotate-half BF16 implementation unavailable");
            continue;
        }
        if (!strcmp(s->implementation, "attention.full.f32acc.bf16.v1")) {
            if (!c->ops || !c->ops->attention_execute || !c->ops->bf16_round)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "full attention implementation unavailable");
            continue;
        }
        if (!c->ops || !c->ops->linear_compile || !c->ops->linear_execute || !c->ops->linear_release ||
            !c->ops->linear_summary || !c->ops->silu_product_bf16 || !c->ops->add_bf16 || !c->ops->bf16_round)
            return kernel_refuse(err, YVEX_ERR_UNSUPPORTED,
                "program requires unavailable backend numerical operations");
        if (!strcmp(s->implementation, "linear.bf16.f32acc.v1")) {
            const yvex_ir_type *a = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
            const yvex_ir_type *r = &yvex_program_physical_value_at(c->program, s->results[0])->type;
            if (r->scalar != YVEX_IR_BF16)
                return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "linear result needs a different publication contract");
            for (j = 0u; j < c->linear_count; ++j)
                if (c->linears[j].requirement.input_width == a->shape[1].extent &&
                    c->linears[j].requirement.output_width == r->shape[1].extent &&
                    c->linears[j].population.symbol == r->shape[0].symbol &&
                    c->linears[j].population.extent == r->shape[0].extent) break;
            if (j == c->linear_count) {
                c->linears[j].population = r->shape[0];
                c->linears[c->linear_count++].requirement = (yvex_transformer_linear_requirement){
                    .operation = YVEX_TRANSFORMER_LINEAR_OPERATION_PROJECTION,
                    .publication_contract = YVEX_TRANSFORMER_LINEAR_NUMERIC_BF16_F32_ACCUMULATION,
                    .source_dtype = YVEX_DTYPE_BF16, .input_dtype = YVEX_DTYPE_F32,
                    .accumulation_dtype = YVEX_DTYPE_F32, .output_dtype = YVEX_DTYPE_F32,
                    .publication_dtype = YVEX_DTYPE_BF16,
                    .input_width = a->shape[1].extent, .output_width = r->shape[1].extent};
            }
            c->linear_slots[i] = j;
        } else if (!strcmp(s->implementation, "rms_norm.bf16.v1")) {
            const yvex_ir_attribute *offset = yvex_program_physical_attribute(s, "weight_offset");
            rc = kernel_small_prepare(c, s->operands[1], offset ? offset->value.real : 0.0, err);
        } else if (!strcmp(s->implementation, "gated_delta.bf16.f32state.v1")) {
            for (j = 4u; rc == YVEX_OK && j < 8u; ++j) rc = kernel_small_prepare(c, s->operands[j], 0.0, err);
        } else if (!strcmp(s->implementation, "gated_causal.bf16.v1")) {
            rc = kernel_small_prepare(c, s->operands[3], 1.0, err);
            if (rc == YVEX_OK) rc = kernel_small_prepare(c, s->operands[4], 1.0, err);
        }
    }
    return rc;
}

int yvex_program_kernels_open(yvex_program_kernels **out, const yvex_program_physical *program,
    const yvex_program_kernel_parameter *parameters, size_t parameter_count,
    yvex_backend *backend, unsigned long long host_limit,
    unsigned long long device_limit, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(program);
    const yvex_backend_transformer_operations *ops = yvex_backend_transformer_operations_get(backend);
    yvex_program_kernels *c;
    unsigned long long host;
    int rc;
    if (out) *out = NULL;
    if (!out || !s || (parameter_count && !parameters) || !backend)
        return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "compiled parameters and admitted backend operations required");
    host = sizeof(*c) + s->value_count * (sizeof(*c->weights) + sizeof(*c->sources) +
        sizeof(*c->small) + sizeof(*c->offsets)) +
        s->step_count * (sizeof(*c->linears) + sizeof(*c->linear_slots));
    if (host_limit && host > host_limit)
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "kernel directory exceeds host budget");
    c = calloc(1u, sizeof(*c));
    if (!c) return kernel_refuse(err, YVEX_ERR_NOMEM, "kernel owner allocation failed");
    c->program = yvex_program_physical_retain(program, err);
    if (!c->program) { free(c); return yvex_error_is_set(err) ? err->code : YVEX_ERR_STATE; }
    c->summary = s; c->backend = backend; c->ops = ops;
    c->host_bytes = host; c->host_limit = host_limit; c->device_limit = device_limit;
    c->weights = calloc(s->value_count, sizeof(*c->weights));
    c->sources = calloc(s->value_count, sizeof(*c->sources));
    c->small = calloc(s->value_count, sizeof(*c->small));
    c->offsets = calloc(s->value_count, sizeof(*c->offsets));
    c->linears = calloc(s->step_count, sizeof(*c->linears));
    c->linear_slots = calloc(s->step_count, sizeof(*c->linear_slots));
    rc = c->weights && c->sources && c->small && c->offsets && c->linears && c->linear_slots ? YVEX_OK :
        kernel_refuse(err, YVEX_ERR_NOMEM, "kernel directory allocation failed");
    if (rc == YVEX_OK) rc = kernel_parameters_bind(c, parameters, parameter_count, err);
    if (rc == YVEX_OK) rc = kernel_sources_prepare(c, err);
    if (rc == YVEX_OK) rc = kernel_signal_prepare(c, err);
    if (rc == YVEX_OK) rc = kernel_instructions_bind(c, err);
    if (rc != YVEX_OK) (void)yvex_program_kernels_close(&c, NULL);
    *out = c;
    return rc;
}

static int kernel_linear_prepare(yvex_program_kernels *c, program_kernel_linear *l,
                                  unsigned long long rows, yvex_error *err)
{
    yvex_transformer_linear_executable **slot = rows == 1u ? &l->single : &l->multiple;
    yvex_transformer_linear_compile_request request = {.semantic_domain = "program.linear.bf16.f32acc.v1",
        .requirement = &l->requirement, .input_rows = rows};
    yvex_transformer_linear_executable_summary old = {0}, summary = {0};
    int rc = *slot ? c->ops->linear_summary(*slot, &old, err) : YVEX_OK;
    if (rows != 1u) l->prepared_multiple_rows = 0u;
    if (rc == YVEX_OK && (old.plan_host_bytes > c->host_bytes || old.prepared_weight_bytes > c->device_bytes))
        return kernel_refuse(err, YVEX_ERR_STATE, "prepared linear accounting is inconsistent");
    if (rc == YVEX_OK && *slot) rc = c->ops->linear_release(c->backend, slot, err);
    if (rc != YVEX_OK) return rc;
    c->host_bytes -= old.plan_host_bytes;
    c->device_bytes -= old.prepared_weight_bytes;
    rc = c->ops->linear_compile(c->backend, &request, slot, &summary, err);
    if (rc == YVEX_OK && (!summary.exact || summary.input_rows != rows || !yvex_sha256_hex_valid(summary.identity)))
        rc = kernel_refuse(err, YVEX_ERR_STATE, "backend specialization is not exact");
    if (rc == YVEX_OK) rc = kernel_account(c, summary.plan_host_bytes, summary.prepared_weight_bytes, err);
    if (rc != YVEX_OK && *slot) (void)c->ops->linear_release(c->backend, slot, NULL);
    if (rc == YVEX_OK && rows != 1u) l->prepared_multiple_rows = rows;
    return rc;
}

static int kernel_host_staging_prepare(yvex_program_kernels *c, unsigned long long rows, yvex_error *err)
{
    unsigned long long bytes = c->host_staging_bytes, total;
    for (size_t i = 0u; i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        if (!strncmp(s->implementation, "indexed_rows.", 13u) ||
            !strncmp(s->implementation, "partition_rows.", 15u) ||
            !strcmp(s->implementation, "axis_rotary.f32math.bf16.v1")) {
            unsigned long long size = 0u;
            for (size_t j = 0u; j < s->operand_count + s->result_count; ++j) {
                yvex_ir_id value = j < s->operand_count ? s->operands[j] : s->results[j - s->operand_count];
                const yvex_ir_type *t = &yvex_program_physical_value_at(c->program, value)->type;
                unsigned long long count = 1u;
                for (unsigned int axis = 0u; axis < t->rank; ++axis)
                    if (!yvex_core_u64_mul(count,
                        t->shape[axis].symbol == YVEX_IR_NONE ? t->shape[axis].extent : rows, &count))
                        return kernel_refuse(err, YVEX_ERR_BOUNDS, "indexed operation staging extent overflowed");
                if (!yvex_core_u64_add(size, count, &size))
                    return kernel_refuse(err, YVEX_ERR_BOUNDS, "indexed operation staging sum overflowed");
            }
            if (!yvex_core_u64_mul(size, sizeof(float), &size))
                return kernel_refuse(err, YVEX_ERR_BOUNDS, "indexed operation staging bytes overflowed");
            if (size > bytes) bytes = size;
            continue;
        }
        int masked = !strcmp(s->implementation, "masked_rows.host.bf16.v1");
        if (!masked && strcmp(s->implementation, "rotary_tables.f64.bf16.v1") &&
            strcmp(s->implementation, "grid_rotary.f32.v1") &&
            strcmp(s->implementation, "grid_bilinear.f64weights.bf16.v1")) continue;
        const yvex_ir_type *t = &yvex_program_physical_value_at(c->program, s->results[0])->type;
        unsigned long long size, population = yvex_program_physical_step_population(c->program, i, rows);
        if (!yvex_core_u64_mul(population, t->shape[1].extent, &size) ||
            !yvex_core_u64_mul(size, 2u, &size) ||
            (masked && !yvex_core_u64_add(size, population, &size)) ||
            !yvex_core_u64_mul(size, sizeof(float), &size))
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "operation host staging overflowed");
        if (size > bytes) bytes = size;
    }
    if (bytes == c->host_staging_bytes) return YVEX_OK;
    if (bytes > SIZE_MAX || !yvex_core_u64_add(c->host_bytes, bytes - c->host_staging_bytes, &total) ||
        (c->host_limit && total > c->host_limit))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "operation host staging exceeds host budget");
    float *grown = realloc(c->host_staging, (size_t)bytes);
    if (!grown) return kernel_refuse(err, YVEX_ERR_NOMEM, "operation host staging allocation failed");
    c->host_staging = grown; c->host_staging_bytes = bytes; c->host_bytes = total;
    return YVEX_OK;
}

/* The model's symbolic horizon is not the admitted execution population.
 * Retain scratch for the largest prepared invocation, not the context limit.
 * Growing preserves the previous buffer on refusal and accounts the temporary
 * overlap before allocating the replacement. */
static int kernel_mhc_prepare(yvex_program_kernels *c, unsigned long long rows, yvex_error *err)
{
    if (yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU) return YVEX_OK;
    for (size_t i = 0u; i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        if (strcmp(s->implementation, "mhc.residual_pre.bf16.v1")) continue;
        const yvex_ir_type *x = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        yvex_device_tensor **slot = &c->small[s->results[0]], *grown = NULL;
        unsigned long long count = x->shape[0].symbol == YVEX_IR_NONE ? x->shape[0].extent : rows;
        unsigned long long peak, old = *slot ? (*slot)->bytes : 0u;
        yvex_backend_tensor_desc d = {.name = "program-mhc-workspace", .dtype = YVEX_DTYPE_F32, .rank = 1u};
        if (!yvex_core_u64_mul(count, x->shape[1].extent, &count) ||
            !yvex_core_u64_mul(count, x->shape[2].extent, &count) ||
            !yvex_core_u64_mul(count, sizeof(float), &d.bytes))
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "mHC workspace population overflowed");
        if (d.bytes <= old) continue;
        if (old > c->device_bytes || !yvex_core_u64_add(c->device_bytes, d.bytes, &peak) ||
            (c->device_limit && peak > c->device_limit))
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "mHC workspace growth exceeds preparation budget");
        d.dims[0] = count;
        int rc = yvex_backend_tensor_alloc(c->backend, &d, &grown, err);
        if (rc != YVEX_OK) return rc;
        if (*slot && (rc = yvex_backend_tensor_release(c->backend, slot, err)) != YVEX_OK) {
            (void)yvex_backend_tensor_release(c->backend, &grown, NULL);
            return rc;
        }
        *slot = grown;
        c->device_bytes = peak - old;
    }
    return YVEX_OK;
}

int yvex_program_kernels_prepare(yvex_program_kernels *c, unsigned long long rows,
    unsigned long long host_limit, unsigned long long device_limit, yvex_error *err)
{
    size_t i;
    int rc = YVEX_OK;
    if (!c || rows < c->summary->minimum_rows || rows > c->summary->maximum_rows || rows % c->summary->row_multiple)
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "linear population is outside the compiled envelope");
    if ((host_limit && c->host_bytes > host_limit) || (device_limit && c->device_bytes > device_limit))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "prepared resources exceed the current owner budget");
    c->host_limit = host_limit;
    c->device_limit = device_limit;
    if (rows == 1u ? c->single_ready : c->multiple_ready && c->multiple_rows == rows) return YVEX_OK;
    if (rows == 1u) c->single_ready = 0; else c->multiple_ready = 0;
    rc = kernel_host_staging_prepare(c, rows, err);
    if (rc == YVEX_OK) rc = kernel_mhc_prepare(c, rows, err);
    for (i = 0u; rc == YVEX_OK && i < c->linear_count; ++i) {
        program_kernel_linear *l = &c->linears[i];
        unsigned long long population = l->population.symbol == YVEX_IR_NONE ? l->population.extent : rows;
        rc = kernel_linear_prepare(c, l, population, err);
    }
    if (rc == YVEX_OK) {
        if (rows == 1u) c->single_ready = 1;
        else { c->multiple_ready = 1; c->multiple_rows = rows; }
    }
    return rc;
}

/* The portable implementation retains the quantization owner's scalar dot
 * algorithm and per-row cancellation. This is one linear operation, not model
 * composition; dimensions and parameter handles arrive from physical work. */
static int kernel_linear_cpu(const yvex_component_encoded_weight *w,
    const yvex_program_device_invocation *r, const yvex_device_tensor *input,
    yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const float *x = (const float *)input->data;
    float *y = (float *)output->data;
    unsigned long long row, column;
    output->is_written = 0;
    for (row = 0u; row < r->rows; ++row)
        for (column = 0u; column < w->row_count; ++column) {
            yvex_quant_failure failure = {0};
            int rc;
            if (r->cancel_requested && r->cancel_requested(r->cancel_context))
                return kernel_refuse(err, YVEX_ERR_CANCELLED, "linear CPU projection cancelled");
            rc = yvex_quant_cpu_dot(w->qtype, w->encoded + column * w->row_bytes,
                (size_t)w->row_bytes, x + row * w->row_width, w->row_width,
                &y[row * w->row_count + column], &failure, err);
            if (rc != YVEX_OK) return rc;
            if (!isfinite(y[row * w->row_count + column]))
                return kernel_refuse(err, YVEX_ERR_FORMAT, "linear CPU projection produced non-finite output");
        }
    facts->active_weight_bytes = w->encoded_bytes;
    facts->activation_bytes = input->bytes + output->bytes;
    facts->compulsory_memory_facts_available = 1;
    output->is_written = 1;
    return YVEX_OK;
}

static int kernel_embedding_cpu(const yvex_component_encoded_weight *w,
    const yvex_program_device_invocation *r, const yvex_program_index_value *indices,
    yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(w->qtype);
    float *values = (float *)(void *)output->data;
    unsigned long long expected;
    if (!geometry || !geometry->block_size || !geometry->bytes_per_block || !w->encoded ||
        !indices || !indices->is_written || indices->count != r->rows || !indices->values || !output->data ||
        !yvex_core_u64_mul(r->rows, w->row_width, &expected) ||
        expected != output->bytes / sizeof(float) || w->row_width % geometry->block_size)
        return kernel_refuse(err, YVEX_ERR_FORMAT, "encoded CPU embedding geometry is incompatible");
    output->is_written = 0;
    for (unsigned long long row = 0u; row < r->rows; ++row) {
        unsigned int selected = indices->values[row];
        if (r->cancel_requested && r->cancel_requested(r->cancel_context))
            return kernel_refuse(err, YVEX_ERR_CANCELLED, "embedding CPU gather cancelled");
        if (selected >= w->row_count)
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "embedding token exceeds admitted vocabulary");
        const unsigned char *source = w->encoded + (unsigned long long)selected * w->row_bytes;
        for (unsigned long long block = 0u; block < w->row_width / geometry->block_size; ++block) {
            yvex_quant_failure failure = {0};
            int rc = yvex_quant_decode_block(w->qtype, source + block * geometry->bytes_per_block,
                geometry->bytes_per_block, values + row * w->row_width + block * geometry->block_size,
                geometry->block_size, &failure, err);
            if (rc != YVEX_OK) return rc;
        }
    }
    if (!yvex_core_u64_mul(r->rows, w->row_bytes, &facts->active_weight_bytes))
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "embedding access accounting overflowed");
    facts->activation_bytes = output->bytes;
    facts->compulsory_memory_facts_available = 1;
    output->is_written = 1;
    return YVEX_OK;
}

static int kernel_embedding(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_component_encoded_weight *w = &c->weights[s->operands[1]];
    if (yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU)
        return kernel_embedding_cpu(w, r, &r->indices[s->operands[0]], output, facts, err);
    return yvex_backend_encoded_gather(c->backend, w->encoded, w->encoded_bytes, w->qtype,
        w->row_count, w->row_width, w->row_bytes, r->indices[s->operands[0]].values,
        r->rows, output, facts, err);
}

static int kernel_linear_residual(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    const yvex_device_tensor *input, yvex_device_tensor *output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_component_encoded_weight *w = &c->weights[r->step->operands[1]];
    const yvex_device_tensor *residual = &r->values[r->step->operands[2]];
    int rc;
    if (yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU) {
        rc = kernel_linear_cpu(w, r, input, output, facts, err);
        output->is_written = 0;
        for (unsigned long long i = 0u; rc == YVEX_OK && i < output->bytes / sizeof(float); ++i) {
            float value = ((float *)output->data)[i] + ((const float *)residual->data)[i];
            if (!isfinite(value))
                return kernel_refuse(err, YVEX_ERR_FORMAT, "linear residual produced non-finite output");
            value = yvex_quant_bf16_decode(yvex_quant_bf16_encode(value));
            if (!isfinite(value)) return kernel_refuse(err, YVEX_ERR_FORMAT, "linear residual exceeds BF16 range");
            ((float *)output->data)[i] = value;
        }
        if (rc == YVEX_OK) {
            if (!yvex_core_u64_add(facts->activation_bytes, residual->bytes, &facts->activation_bytes))
                return kernel_refuse(err, YVEX_ERR_BOUNDS, "linear residual accounting overflowed");
            output->is_written = 1;
        }
        return rc;
    }
    yvex_backend_operation_facts rounding = {0};
    rc = yvex_backend_encoded_matvec(c->backend, w->encoded, w->encoded_bytes, w->qtype,
        w->row_count, w->row_width, w->row_bytes, r->rows, input, NULL, 0u, residual,
        output, YVEX_ENCODED_INPUT_BF16, YVEX_ENCODED_REDUCTION_DEFAULT, facts, err);
    if (rc == YVEX_OK) rc = c->ops->bf16_round(c->backend, output, output->bytes / sizeof(float), &rounding, err);
    if (rc != YVEX_OK) { output->is_written = 0; return rc; }
    if (!yvex_core_u64_add(facts->kernel_launches, rounding.kernel_launches, &facts->kernel_launches) ||
        !yvex_core_u64_add(facts->queue_synchronizations, rounding.queue_synchronizations,
                           &facts->queue_synchronizations) ||
        !yvex_core_u64_add(facts->device_synchronizations, rounding.device_synchronizations,
                           &facts->device_synchronizations) ||
        !yvex_core_u64_add(facts->upload_count, rounding.upload_count, &facts->upload_count) ||
        !yvex_core_u64_add(facts->download_count, rounding.download_count, &facts->download_count) ||
        !yvex_core_u64_add(facts->h2d_bytes, rounding.h2d_bytes, &facts->h2d_bytes) ||
        !yvex_core_u64_add(facts->d2h_bytes, rounding.d2h_bytes, &facts->d2h_bytes) ||
        !yvex_core_u64_add(facts->d2d_bytes, rounding.d2d_bytes, &facts->d2d_bytes)) {
        output->is_written = 0;
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "linear residual accounting overflowed");
    }
    if (rounding.temporary_bytes > facts->temporary_bytes) facts->temporary_bytes = rounding.temporary_bytes;
    facts->compulsory_memory_facts_available &= rounding.compulsory_memory_facts_available;
    return YVEX_OK;
}

static int kernel_grid(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    yvex_device_tensor *out = &r->values[s->results[0]];
    unsigned long long gh = yvex_program_physical_attribute(s, "grid_height")->value.integer;
    unsigned long long gw = yvex_program_physical_attribute(s, "grid_width")->value.integer;
    unsigned long long block = yvex_program_physical_attribute(s, "block_size")->value.integer;
    unsigned long long width = out->dims[1], count = out->bytes / sizeof(float);
    int rotary = !strcmp(s->implementation, "grid_rotary.f32.v1");
    if (out->bytes > c->host_staging_bytes / 2u)
        return kernel_refuse(err, YVEX_ERR_STATE, "grid table population was not prepared");
    for (unsigned long long row = 0u; row < r->rows; ++row) {
        if (r->cancel_requested && r->cancel_requested(r->cancel_context))
            return kernel_refuse(err, YVEX_ERR_CANCELLED, "grid table construction cancelled");
        unsigned long long h = row / (block * gw) * block + (row / block) % block;
        unsigned long long w = (row / (block * block)) % (gw / block) * block + row % block;
        if (rotary) {
            float theta = (float)yvex_program_physical_attribute(s, "theta")->value.real;
            for (unsigned long long lane = 0u; lane < width / 2u; ++lane) {
                unsigned long long axis = lane % (width / 4u), position = lane < width / 4u ? h : w;
                float frequency = powf(theta, -(float)(2u * axis) / (float)(width / 2u));
                float angle = (float)position * frequency;
                float cosine = cosf(angle), sine = sinf(angle);
                if (!isfinite(cosine) || !isfinite(sine))
                    return kernel_refuse(err, YVEX_ERR_FORMAT, "grid rotary table is not finite");
                unsigned long long i = row * width + lane;
                c->host_staging[i] = c->host_staging[i + width / 2u] = cosine;
                c->host_staging[count + i] = c->host_staging[count + i + width / 2u] = sine;
            }
        } else {
            const yvex_component_encoded_weight *weight = &c->weights[s->operands[0]];
            unsigned long long side = yvex_program_physical_attribute(s, "source_side")->value.integer;
            double hs = gh == 1u ? 0.0 : (double)h * (double)(side - 1u) / (double)(gh - 1u);
            double ws = gw == 1u ? 0.0 : (double)w * (double)(side - 1u) / (double)(gw - 1u);
            unsigned long long h0 = (unsigned long long)floor(hs), w0 = (unsigned long long)floor(ws);
            unsigned long long h1 = h0 + 1u < side ? h0 + 1u : h0, w1 = w0 + 1u < side ? w0 + 1u : w0;
            double hf = hs - floor(hs), wf = ws - floor(ws);
            for (unsigned long long column = 0u; column < width; ++column) {
                unsigned long long indices[] = {(h0 * side + w0) * width + column,
                    (h0 * side + w1) * width + column, (h1 * side + w0) * width + column,
                    (h1 * side + w1) * width + column};
                float p[4];
                for (size_t i = 0u; i < 4u; ++i) {
                    unsigned short bits;
                    memcpy(&bits, weight->encoded + indices[i] * 2u, sizeof(bits));
                    p[i] = yvex_quant_bf16_decode(bits);
                }
                float top = (float)((1.0 - wf) * p[0] + wf * p[1]);
                float bottom = (float)((1.0 - wf) * p[2] + wf * p[3]);
                float value = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)((1.0 - hf) * top + hf * bottom)));
                if (!isfinite(value)) return kernel_refuse(err, YVEX_ERR_FORMAT, "grid interpolation is not finite");
                c->host_staging[row * width + column] = value;
            }
        }
    }
    int rc = yvex_backend_tensor_write(c->backend, out, c->host_staging, out->bytes, err);
    if (rc == YVEX_OK && rotary) rc = yvex_backend_tensor_write(c->backend,
        &r->values[s->results[1]], c->host_staging + count, out->bytes, err);
    if (rc == YVEX_OK) {
        facts->compulsory_memory_facts_available = 1;
        if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) {
            facts->h2d_bytes = out->bytes * (rotary ? 2u : 1u);
            facts->upload_count = rotary ? 2u : 1u;
        }
    }
    return rc;
}

static int kernel_host_transfer(yvex_program_kernels *c, yvex_device_tensor *tensor, float *host,
    int write, yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = write ? yvex_backend_tensor_write(c->backend, tensor, host, tensor->bytes, err) :
        yvex_backend_tensor_read(c->backend, tensor, host, tensor->bytes, err);
    if (rc == YVEX_OK && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) {
        unsigned long long *bytes = write ? &facts->h2d_bytes : &facts->d2h_bytes;
        unsigned long long *count = write ? &facts->upload_count : &facts->download_count;
        if (!yvex_core_u64_add(*bytes, tensor->bytes, bytes) || !yvex_core_u64_add(*count, 1u, count))
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "host operation transfer accounting overflowed");
    }
    facts->compulsory_memory_facts_available = 1;
    return rc;
}

static int kernel_indexed_rows(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    yvex_device_tensor *out = &r->values[s->results[0]];
    unsigned long long width = out->dims[1], rows = out->dims[0], count = out->bytes / sizeof(float);
    int partition = !strncmp(s->implementation, "partition_rows.", 15u);
    float *result = c->host_staging, *scratch = result + count, *seen = scratch;
    unsigned long long offset = count, staged = 0u;
    for (size_t i = 0u; i < s->operand_count; i += 2u) {
        unsigned long long size = r->values[s->operands[i]].bytes / sizeof(float);
        if (!yvex_core_u64_add(staged, size, &staged))
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "indexed row staging overflowed");
    }
    if (!yvex_core_u64_add(offset, staged, &offset) ||
        (partition && !yvex_core_u64_add(offset, rows, &offset)) || offset > c->host_staging_bytes / sizeof(float))
        return kernel_refuse(err, YVEX_ERR_STATE, "indexed row staging was not prepared");
    seen = scratch + staged;
    if (partition) memset(seen, 0, (size_t)rows * sizeof(float));
    for (size_t i = 0u; i < s->operand_count; i += 2u) {
        yvex_device_tensor *source = &r->values[s->operands[i]];
        const unsigned int *indices = r->indices[s->operands[i + 1u]].values;
        unsigned long long population = partition ? source->dims[0] : rows;
        int rc = kernel_host_transfer(c, source, scratch, 0, facts, err);
        if (rc != YVEX_OK) return rc;
        for (unsigned long long row = 0u; row < population; ++row) {
            if (r->cancel_requested && r->cancel_requested(r->cancel_context))
                return kernel_refuse(err, YVEX_ERR_CANCELLED, "indexed row operation cancelled");
            unsigned long long selected = indices[row];
            if (selected >= (partition ? rows : source->dims[0]) || (partition && seen[selected] != 0.0f))
                return kernel_refuse(err, YVEX_ERR_BOUNDS, "row index is outside its domain or partition overlaps");
            if (partition) seen[selected] = 1.0f;
            memcpy(result + (partition ? selected : row) * width,
                scratch + (partition ? row : selected) * width, (size_t)width * sizeof(float));
        }
        scratch += source->bytes / sizeof(float);
    }
    if (partition) for (unsigned long long row = 0u; row < rows; ++row)
        if (seen[row] != 1.0f) return kernel_refuse(err, YVEX_ERR_FORMAT, "row partition is incomplete");
    return kernel_host_transfer(c, out, result, 1, facts, err);
}

static int kernel_axis_rotary(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    yvex_device_tensor *positions = &r->values[s->operands[0]], *frequencies = &r->values[s->operands[1]];
    yvex_device_tensor *cosine = &r->values[s->results[0]], *sine = &r->values[s->results[1]];
    unsigned long long count = cosine->bytes / sizeof(float), axes = positions->dims[1];
    unsigned long long n = frequencies->bytes / sizeof(float), required;
    if (!yvex_core_u64_add(positions->bytes, frequencies->bytes, &required) ||
        !yvex_core_u64_add(required, cosine->bytes, &required) ||
        !yvex_core_u64_add(required, sine->bytes, &required) || required > c->host_staging_bytes)
        return kernel_refuse(err, YVEX_ERR_STATE, "axis frequency staging was not prepared");
    float *x = c->host_staging, *freq = x + positions->bytes / sizeof(float);
    float *co = freq + n, *si = co + count;
    int rc = kernel_host_transfer(c, positions, x, 0, facts, err);
    if (rc == YVEX_OK) rc = kernel_host_transfer(c, frequencies, freq, 0, facts, err);
    for (unsigned long long row = 0u; rc == YVEX_OK && row < positions->dims[0]; ++row) {
        if (r->cancel_requested && r->cancel_requested(r->cancel_context))
            return kernel_refuse(err, YVEX_ERR_CANCELLED, "axis frequency construction cancelled");
        for (unsigned long long column = 0u; column < axes * n * 2u; ++column) {
            unsigned long long pair = column % (axes * n), i = row * axes * n * 2u + column;
            float angle = x[row * axes + pair / n] * freq[pair % n];
            if (!isfinite(angle)) return kernel_refuse(err, YVEX_ERR_FORMAT, "axis frequency product is not finite");
            co[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(cosf(angle)));
            si[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(sinf(angle)));
        }
    }
    if (rc == YVEX_OK) rc = kernel_host_transfer(c, cosine, co, 1, facts, err);
    if (rc == YVEX_OK) rc = kernel_host_transfer(c, sine, si, 1, facts, err);
    return rc;
}

static int kernel_rotary_tables(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    yvex_device_tensor *cosine = &r->values[s->results[0]], *sine = &r->values[s->results[1]];
    unsigned long long width = cosine->dims[1], count = cosine->bytes / sizeof(float);
    unsigned long long sy = yvex_program_physical_attribute(s, "section_y")->value.integer;
    unsigned long long sz = yvex_program_physical_attribute(s, "section_z")->value.integer;
    double theta = yvex_program_physical_attribute(s, "theta")->value.real;
    if (cosine->bytes > c->host_staging_bytes / 2u)
        return kernel_refuse(err, YVEX_ERR_STATE, "rotary table population was not prepared");
    for (unsigned long long row = 0u; row < r->rows; ++row) {
        if (r->cancel_requested && r->cancel_requested(r->cancel_context))
            return kernel_refuse(err, YVEX_ERR_CANCELLED, "rotary table construction cancelled");
        for (unsigned long long column = 0u; column < width; ++column) {
            unsigned long long pair = column % (width / 2u), axis = 0u, i = row * width + column;
            if (pair % 3u == 1u && pair / 3u < sy) axis = 1u;
            else if (pair % 3u == 2u && pair / 3u < sz) axis = 2u;
            double frequency = pow(theta, -(double)(2u * pair) / (double)width);
            double angle = (double)r->indices[s->operands[axis]].values[row] * frequency;
            c->host_staging[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)cos(angle)));
            c->host_staging[count + i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)sin(angle)));
        }
    }
    int rc = yvex_backend_tensor_write(c->backend, cosine, c->host_staging, cosine->bytes, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_write(c->backend, sine,
        c->host_staging + count, sine->bytes, err);
    if (rc == YVEX_OK) {
        if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) {
            facts->h2d_bytes = cosine->bytes + sine->bytes;
            facts->upload_count = 2u;
        }
        facts->compulsory_memory_facts_available = 1;
    }
    return rc;
}

static int kernel_masked_rows(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    yvex_device_tensor *x = &r->values[s->operands[0]], *source = &r->values[s->operands[1]];
    yvex_device_tensor *mask = &r->values[s->operands[2]], *out = &r->values[s->results[0]];
    unsigned long long count = out->bytes / sizeof(float), width = out->dims[1], bytes;
    int add = yvex_program_physical_attribute(s, "add")->value.integer != 0u;
    if (!yvex_core_u64_add(x->bytes, source->bytes, &bytes) ||
        !yvex_core_u64_add(bytes, mask->bytes, &bytes) || bytes > c->host_staging_bytes)
        return kernel_refuse(err, YVEX_ERR_STATE, "masked-row host implementation was not prepared");
    float *values = c->host_staging, *incoming = values + count, *selected = incoming + count;
    int rc = yvex_backend_tensor_read(c->backend, x, values, x->bytes, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_read(c->backend, source, incoming, source->bytes, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_read(c->backend, mask, selected, mask->bytes, err);
    for (unsigned long long row = 0u; rc == YVEX_OK && row < r->rows; ++row) {
        if (r->cancel_requested && r->cancel_requested(r->cancel_context))
            return kernel_refuse(err, YVEX_ERR_CANCELLED, "masked-row operation cancelled");
        if (selected[row] != 0.0f && selected[row] != 1.0f)
            return kernel_refuse(err, YVEX_ERR_FORMAT, "row mask is not Boolean-valued");
        for (unsigned long long column = 0u; selected[row] == 1.0f && column < width; ++column) {
            unsigned long long i = row * width + column;
            float v = add ? values[i] + incoming[i] : incoming[i];
            if (add) v = yvex_quant_bf16_decode(yvex_quant_bf16_encode(v));
            if (!isfinite(v)) return kernel_refuse(err, YVEX_ERR_FORMAT, "masked-row result is not finite");
            values[i] = v;
        }
    }
    if (rc == YVEX_OK) rc = yvex_backend_tensor_write(c->backend, out, values, out->bytes, err);
    if (rc == YVEX_OK) {
        if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) {
            facts->d2h_bytes = bytes; facts->h2d_bytes = out->bytes;
            facts->download_count = 3u; facts->upload_count = 1u;
        }
        facts->compulsory_memory_facts_available = 1;
    }
    return rc;
}

static int kernel_round_result(yvex_program_kernels *c, yvex_device_tensor *output,
    int rc, yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_backend_operation_facts rounding = {0};
    if (rc == YVEX_OK) rc = c->ops->bf16_round(c->backend, output, output->bytes / sizeof(float), &rounding, err);
    if (rc != YVEX_OK) { output->is_written = 0; return rc; }
    if (!yvex_core_u64_add(facts->kernel_launches, rounding.kernel_launches, &facts->kernel_launches) ||
        !yvex_core_u64_add(facts->queue_synchronizations, rounding.queue_synchronizations,
            &facts->queue_synchronizations) ||
        !yvex_core_u64_add(facts->device_synchronizations, rounding.device_synchronizations,
            &facts->device_synchronizations) ||
        !yvex_core_u64_add(facts->upload_count, rounding.upload_count, &facts->upload_count) ||
        !yvex_core_u64_add(facts->download_count, rounding.download_count, &facts->download_count) ||
        !yvex_core_u64_add(facts->h2d_bytes, rounding.h2d_bytes, &facts->h2d_bytes) ||
        !yvex_core_u64_add(facts->d2h_bytes, rounding.d2h_bytes, &facts->d2h_bytes) ||
        !yvex_core_u64_add(facts->d2d_bytes, rounding.d2d_bytes, &facts->d2d_bytes)) {
        output->is_written = 0;
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "rounded result accounting overflowed");
    }
    if (rounding.temporary_bytes > facts->temporary_bytes) facts->temporary_bytes = rounding.temporary_bytes;
    facts->compulsory_memory_facts_available &= rounding.compulsory_memory_facts_available;
    return YVEX_OK;
}

static int kernel_attention(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_ir_type *result = &yvex_program_physical_value_at(c->program, s->results[0])->type;
    yvex_device_tensor *output = &r->values[s->results[0]];
    yvex_transformer_attention_request request = {
        .requirement = kernel_attention_requirement(c, s, r->rows),
        .query = &r->values[s->operands[0]], .key = &r->values[s->operands[1]],
        .value = &r->values[s->operands[2]], .output = output, .workspace = c->small[s->results[0]]};
    int rc = c->ops->attention_execute(c->backend, &request, facts, err);
    return result->scalar == YVEX_IR_BF16 ? kernel_round_result(c, output, rc, facts, err) : rc;
}

static int kernel_rows(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    yvex_device_tensor *output = &r->values[s->results[0]];
    int rc = YVEX_OK;
    output->is_written = 0;
    if (!strcmp(s->implementation, "zeros.f32.v1"))
        rc = yvex_backend_tensor_zero(c->backend, output, err);
    else {
        unsigned long long offset = 0u;
        for (size_t i = 0u; rc == YVEX_OK && i < s->operand_count; ++i) {
            const yvex_device_tensor *source = !strcmp(s->implementation, "parameter_copy.f32.v1") ?
                c->small[s->operands[i]] : &r->values[s->operands[i]];
            yvex_device_tensor destination;
            if (!source || !source->is_written ||
                !yvex_backend_tensor_f32_subview(output, offset, source->bytes / sizeof(float), &destination))
                return kernel_refuse(err, YVEX_ERR_STATE, "row construction exceeds compiled output storage");
            destination.rank = source->rank;
            memcpy(destination.dims, source->dims, sizeof(destination.dims));
            rc = yvex_backend_tensor_copy(c->backend, &destination, source, err);
            offset += source->bytes / sizeof(float);
        }
        if (rc == YVEX_OK && offset != output->bytes / sizeof(float))
            return kernel_refuse(err, YVEX_ERR_STATE, "row construction did not cover its compiled result");
    }
    if (rc == YVEX_OK) {
        output->is_written = 1;
        facts->compulsory_memory_facts_available = 1;
        facts->activation_bytes = output->bytes;
        if (s->operand_count && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU)
            facts->d2d_bytes = output->bytes;
    }
    return rc;
}

static int kernel_signal_execute(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    int kind = kernel_signal_kind(s->implementation);
    const yvex_device_tensor *input = &r->values[s->operands[0]];
    yvex_device_tensor *output = &r->values[s->results[0]], scratch;
    if (kind == 4) {
        unsigned long long count;
        if (!yvex_core_u64_mul(input->bytes / sizeof(float), 2u, &count) ||
            !yvex_backend_tensor_f32_subview(c->signal_workspace, 0u, count, &scratch))
            return kernel_refuse(err, YVEX_ERR_STATE, "signal workspace is not prepared for admitted population");
        yvex_alias_snake_request request = {.batch = input->dims[0], .channels = input->dims[1],
            .length = input->dims[2], .input = input, .output = output, .workspace = &scratch,
            .alpha = c->weights + s->operands[1], .beta = c->weights + s->operands[2],
            .up_filter = c->weights + s->operands[3], .down_filter = c->weights + s->operands[4]};
        return c->ops->alias_snake(c->backend, &request, facts, err);
    }
    yvex_convolution_1d_request request = {.input = input, .output = output,
        .output_length = output->dims[2], .weight = c->weights + s->operands[1]};
    request.geometry = (yvex_convolution_1d_geometry){.batch = input->dims[0],
        .input_channels = input->dims[1], .output_channels = output->dims[1], .input_length = input->dims[2],
        .kernel_size = yvex_program_physical_value_at(c->program, s->operands[1])->type.shape[2].extent,
        .stride = yvex_program_physical_attribute(s, "stride")->value.integer,
        .dilation = yvex_program_physical_attribute(s, "dilation")->value.integer,
        .padding = yvex_program_physical_attribute(s, "padding")->value.integer,
        .output_padding = yvex_program_physical_attribute(s, "output_padding")->value.integer,
        .transposed = (int)yvex_program_physical_attribute(s, "transposed")->value.integer};
    if (kind != 1) {
        unsigned long long count = request.geometry.transposed ? input->dims[1] : output->dims[1];
        if (!yvex_backend_tensor_f32_subview(c->signal_workspace, 0u, count, &scratch))
            return kernel_refuse(err, YVEX_ERR_STATE, "convolution scales exceed prepared workspace");
        request.gain = c->weights + s->operands[2]; request.workspace = &scratch;
    }
    if (kind != 3) request.bias = c->weights + s->operands[kind == 1 ? 2u : 3u];
    return c->ops->convolution_1d(c->backend, &request, facts, err);
}

static int kernel_spatial_execute(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_device_tensor *input = &r->values[s->operands[0]];
    yvex_device_tensor *output = &r->values[s->results[0]];
    if (!strcmp(s->implementation, "spatial_group_norm_silu.f32.v1"))
        return c->ops->spatial_norm_silu(c->backend, input, c->weights + s->operands[1],
            c->weights + s->operands[2], yvex_program_physical_attribute(s, "groups")->value.integer,
            (float)yvex_program_physical_attribute(s, "epsilon")->value.real, output, facts, err);
    const yvex_ir_type *weight = &yvex_program_physical_value_at(c->program, s->operands[1])->type;
    yvex_convolution_2d_geometry g = {.batch = input->dims[0], .input_channels = input->dims[1],
        .input_height = input->dims[2], .input_width = input->dims[3], .output_channels = output->dims[1],
        .kernel_height = weight->shape[3].extent, .kernel_width = weight->shape[4].extent,
        .weight_temporal_extent = weight->shape[2].extent,
        .weight_temporal_index = yvex_program_physical_attribute(s, "kernel_plane")->value.integer,
        .stride_height = yvex_program_physical_attribute(s, "stride_height")->value.integer,
        .stride_width = yvex_program_physical_attribute(s, "stride_width")->value.integer,
        .padding_top = yvex_program_physical_attribute(s, "padding_top")->value.integer,
        .padding_bottom = yvex_program_physical_attribute(s, "padding_bottom")->value.integer,
        .padding_left = yvex_program_physical_attribute(s, "padding_left")->value.integer,
        .padding_right = yvex_program_physical_attribute(s, "padding_right")->value.integer,
        .padding = yvex_program_physical_attribute(s, "reflect")->value.integer ?
            YVEX_CONVOLUTION_PADDING_REFLECT : YVEX_CONVOLUTION_PADDING_ZERO};
    return c->ops->convolution_2d(c->backend, &g, input, c->weights + s->operands[1],
        c->weights + s->operands[2], output, facts, err);
}

static int kernel_f32_execute(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    int *handled, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_device_tensor *input = s->operand_count ? &r->values[s->operands[0]] : NULL;
    yvex_device_tensor *output = &r->values[s->results[0]];
    unsigned long long width = output->dims[1];
    *handled = 1;
    if (!strcmp(s->implementation, "clamped_swiglu.f64math.bf16.v1"))
        return c->ops->clamped_swiglu_bf16(c->backend, input, &r->values[s->operands[1]], output,
            yvex_program_physical_attribute(s, "limit")->value.real, facts, err);
    if (!strcmp(s->implementation, "cast.exact.f32.v1") || !strcmp(s->implementation, "cast.rne.bf16.v1")) {
        int rc = yvex_backend_tensor_copy(c->backend, output, input, err);
        if (rc == YVEX_OK && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU)
            facts->d2d_bytes = input->bytes;
        return !strcmp(s->implementation, "cast.rne.bf16.v1") ? kernel_round_result(c, output, rc, facts, err) : rc;
    }
    if (!strcmp(s->implementation, "silu.bf16.v1") || !strcmp(s->implementation, "silu.f32.v1"))
        return c->ops->silu(c->backend, input, output, output->bytes / sizeof(float),
            !strcmp(s->implementation, "silu.bf16.v1"), facts, err);
    if (kernel_signal_kind(s->implementation)) return kernel_signal_execute(c, r, facts, err);
    if (!strcmp(s->implementation, "conv2d_slice.f32.v1") ||
        !strcmp(s->implementation, "spatial_group_norm_silu.f32.v1")) return kernel_spatial_execute(c, r, facts, err);
    if (!strcmp(s->implementation, "add.f32.v1") || !strcmp(s->implementation, "mean.f32.v1")) {
        const yvex_device_tensor *inputs[YVEX_PROGRAM_OPERAND_CAP];
        for (size_t i = 0u; i < s->operand_count; ++i) inputs[i] = &r->values[s->operands[i]];
        return c->ops->combine_f32(c->backend, inputs, s->operand_count,
            !strcmp(s->implementation, "mean.f32.v1"), output, facts, err);
    }
    if (!strcmp(s->implementation, "clamp.f32.v1"))
        return c->ops->clamp_f32(c->backend, input, output,
            (float)yvex_program_physical_attribute(s, "lower")->value.real,
            (float)yvex_program_physical_attribute(s, "upper")->value.real, facts, err);
    if (!strcmp(s->implementation, "linear_bias.f32.v1")) {
        const yvex_component_encoded_weight *w = c->weights + s->operands[1];
        return c->ops->linear_bias_f32(c->backend, w->encoded, w->encoded_bytes,
            c->small[s->operands[2]], r->rows, w->row_width, w->row_count,
            input, output, facts, err);
    }
    if (!strcmp(s->implementation, "attention.full.f32.v1")) return kernel_attention(c, r, facts, err);
    if (!strcmp(s->implementation, "channel_bias.f32.v1"))
        return c->ops->channel_bias(c->backend, input, c->small[s->operands[1]], output,
            r->rows, width, 0, facts, err);
    if (!strcmp(s->implementation, "scaled_residual.f32.v1"))
        return c->ops->scaled_residual_f32(c->backend, input, &r->values[s->operands[1]],
            c->small[s->operands[2]], output, r->rows, width, facts, err);
    if (!strcmp(s->implementation, "split_interleaved_three.f32.v1") ||
        !strcmp(s->implementation, "split_interleaved_three.bf16.v1")) {
        unsigned long long head = yvex_program_physical_attribute(s, "head_dimension")->value.integer;
        return c->ops->split_interleaved_three(c->backend, input, output, &r->values[s->results[1]],
            &r->values[s->results[2]], r->rows, width / head, head, facts, err);
    }
    if (!strcmp(s->implementation, "swiglu_split.f32.v1"))
        return c->ops->swiglu_split_f32(c->backend, input, output, r->rows, width,
            (int)yvex_program_physical_attribute(s, "gate_first")->value.integer, facts, err);
    if (!strcmp(s->implementation, "swiglu_split.bf16.v1"))
        return c->ops->swiglu_split_bf16(c->backend, input, output, r->rows, width, facts, err);
    if (!strcmp(s->implementation, "slice_rows.f32.v1")) {
        yvex_device_tensor view;
        unsigned long long start = yvex_program_physical_attribute(s, "start")->value.integer, offset;
        if (!yvex_core_u64_mul(start, width, &offset) ||
            !yvex_backend_tensor_f32_subview(input, offset, output->bytes / sizeof(float), &view))
            return kernel_refuse(err, YVEX_ERR_STATE, "row slice exceeds its compiled operand");
        view.rank = output->rank;
        memcpy(view.dims, output->dims, sizeof(view.dims));
        int rc = yvex_backend_tensor_copy(c->backend, output, &view, err);
        if (rc == YVEX_OK) {
            facts->compulsory_memory_facts_available = 1;
            if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) facts->d2d_bytes = view.bytes;
        }
        return rc;
    }
    if (!strcmp(s->implementation, "layer_norm.f32.v1"))
        return c->ops->normalization_f32(c->backend, input, c->small[s->operands[1]], c->small[s->operands[2]],
            output, r->rows, width, yvex_program_physical_attribute(s, "epsilon")->value.real, facts, err);
    if (!strcmp(s->implementation, "rms_norm.f32.v1") || !strcmp(s->implementation, "rms_normalize.f32.v1")) {
        int unit = !strcmp(s->implementation, "rms_normalize.f32.v1");
        const yvex_device_tensor *weight = c->small[unit ? s->results[0] : s->operands[1]];
        yvex_device_tensor x = *input, y = *output;
        if (unit) {
            unsigned long long group = yvex_program_physical_attribute(s, "group_width")->value.integer;
            x.dims[0] = y.dims[0] = input->bytes / sizeof(float) / group;
            x.dims[1] = y.dims[1] = group;
        }
        int rc = c->ops->normalization_f32(c->backend, &x, weight, NULL, &y, x.dims[0], x.dims[1],
            yvex_program_physical_attribute(s, "epsilon")->value.real, facts, err);
        output->is_written = rc == YVEX_OK;
        return rc;
    }
    if (!strcmp(s->implementation, "rotary_half.f32.v1")) {
        unsigned long long head = yvex_program_physical_attribute(s, "head_dimension")->value.integer;
        const yvex_device_tensor *cosine = &r->values[s->operands[1]], *sine = &r->values[s->operands[2]];
        int rc = yvex_backend_tensor_copy(c->backend, output, input, err);
        if (rc == YVEX_OK) rc = c->ops->rotary_half_f32(c->backend, output, cosine, sine,
            r->rows, width / head, head, cosine->dims[1], facts, err);
        if (rc == YVEX_OK && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU &&
            !yvex_core_u64_add(facts->d2d_bytes, input->bytes, &facts->d2d_bytes))
            rc = kernel_refuse(err, YVEX_ERR_BOUNDS, "rotary input copy accounting overflowed");
        return rc;
    }
    *handled = 0;
    return YVEX_OK;
}

static int kernel_encoded_linear(yvex_program_kernels *c, const yvex_program_device_invocation *r,
    const yvex_device_tensor *input, yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_component_encoded_weight *w = &c->weights[s->operands[1]];
    const yvex_ir_type *type = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
    yvex_encoded_input_policy precision = type->scalar == YVEX_IR_BF16 && w->qtype == YVEX_GGUF_QTYPE_BF16 ?
        YVEX_ENCODED_INPUT_BF16 : YVEX_ENCODED_INPUT_F32;
    int q8 = !strcmp(s->implementation, "linear.row_dot.q8.v1");
    if (q8) precision = YVEX_ENCODED_INPUT_Q8;
    yvex_encoded_reduction_policy reduction = q8 || !strcmp(s->implementation, "linear.row_dot.f32.v1") ?
        YVEX_ENCODED_REDUCTION_ROW : YVEX_ENCODED_REDUCTION_DEFAULT;
    if (yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU)
        return kernel_linear_cpu(w, r, input, output, facts, err);
    return yvex_backend_encoded_matvec(c->backend, w->encoded, w->encoded_bytes, w->qtype,
        w->row_count, w->row_width, w->row_bytes, r->rows, input, NULL, 0u, NULL,
        output, precision, reduction, facts, err);
}

static int kernel_index_linearize(const yvex_program_device_invocation *r, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_program_index_value *a = &r->indices[s->operands[0]], *b = &r->indices[s->operands[1]];
    yvex_program_index_value *out = &r->indices[s->results[0]];
    unsigned long long major = yvex_program_physical_attribute(s, "major_extent")->value.integer;
    unsigned long long minor = yvex_program_physical_attribute(s, "minor_extent")->value.integer;
    if (!a->is_written || !b->is_written || !a->values || !b->values || !out->output ||
        a->count != r->rows || b->count != r->rows || out->count != r->rows)
        return kernel_refuse(err, YVEX_ERR_STATE, "index operation requires defined, population-bound SSA slots");
    out->is_written = 0;
    for (unsigned long long i = 0u; i < r->rows; ++i) {
        if (r->cancel_requested && r->cancel_requested(r->cancel_context))
            return kernel_refuse(err, YVEX_ERR_CANCELLED, "index operation cancelled before publication");
        if (a->values[i] >= major || b->values[i] >= minor)
            return kernel_refuse(err, YVEX_ERR_BOUNDS, "coordinate exceeds its compiled index domain");
        out->output[i] = (unsigned int)((unsigned long long)a->values[i] * minor + b->values[i]);
    }
    out->is_written = 1;
    return YVEX_OK;
}

int yvex_program_kernels_invoke(yvex_program_kernels *c, const yvex_program_device_invocation *r,
                                yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s;
    yvex_device_tensor *output;
    const yvex_device_tensor *input;
    if (!c || !r || r->program != c->program || !facts || !r->arguments || !r->values || !r->indices ||
        r->step != yvex_program_physical_step_at(c->program, r->step_index) || !r->step)
        return kernel_refuse(err, YVEX_ERR_INVALID_ARG, "kernel requires its bound physical invocation");
    s = r->step;
    if (!strcmp(s->implementation, "index_linearize.host.u32.v1")) return kernel_index_linearize(r, err);
    unsigned long long staging_offset = 0u;
    for (size_t i = 0u; i < s->operand_count; ++i) {
        int rc = kernel_source_read(c, s->operands[i], &staging_offset, err);
        if (rc != YVEX_OK) return rc;
    }
    if (!s->result_count)
        return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "operation has no admitted numerical operands/results");
    output = &r->values[s->results[0]];
    if (!strcmp(s->implementation, "grid_rotary.f32.v1") ||
        !strcmp(s->implementation, "grid_bilinear.f64weights.bf16.v1"))
        return kernel_grid(c, r, facts, err);
    if (!strcmp(s->implementation, "zeros.f32.v1") || !strcmp(s->implementation, "concat_rows.f32.v1") ||
        !strcmp(s->implementation, "parameter_copy.f32.v1")) return kernel_rows(c, r, facts, err);
    if (!s->operand_count)
        return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "operation requires materialized operands");
    input = &r->values[s->operands[0]];
    if (!strcmp(s->implementation, "indexed_modulate.bf16.v1") ||
        !strcmp(s->implementation, "indexed_gated_residual.bf16.v1")) {
        const yvex_device_tensor *table = &r->values[s->operands[1]];
        const unsigned int *indices = r->indices[s->operands[2]].values;
        if (s->operand_count == 4u)
            return c->ops->gated_residual_bf16(c->backend, input, table, indices,
                &r->values[s->operands[3]], output, input->dims[0], input->dims[1],
                table->dims[0], table->dims[1],
                (unsigned int)yvex_program_physical_attribute(s, "gate")->value.integer, facts, err);
        return c->ops->modulate_bf16(c->backend, input, table, indices, output,
            input->dims[0], input->dims[1], table->dims[0], table->dims[1],
            (unsigned int)yvex_program_physical_attribute(s, "shift")->value.integer,
            (unsigned int)yvex_program_physical_attribute(s, "scale")->value.integer, facts, err);
    }
    if (!strncmp(s->implementation, "linear_bias.cuda.", 17u)) {
        const yvex_component_encoded_weight *w = &c->weights[s->operands[1]], *bias = &c->weights[s->operands[2]];
        return c->ops->linear_bias_target(c->backend, s->implementation, w->encoded, w->encoded_bytes,
            bias->encoded, bias->encoded_bytes, w->row_count, w->row_width, r->rows, input, output, facts, err);
    }
    int handled;
    int f32_rc = kernel_f32_execute(c, r, &handled, facts, err);
    if (f32_rc != YVEX_OK || handled) return f32_rc;
    if (!strcmp(s->implementation, "reshape.f32-storage.v1")) {
        yvex_device_tensor view;
        if (input->bytes != output->bytes ||
            !yvex_backend_tensor_f32_subview(input, 0u, input->bytes / sizeof(float), &view))
            return kernel_refuse(err, YVEX_ERR_STATE, "reshape requires its verified contiguous element extent");
        view.rank = output->rank;
        memcpy(view.dims, output->dims, sizeof(view.dims));
        int rc = yvex_backend_tensor_copy(c->backend, output, &view, err);
        if (rc == YVEX_OK) {
            facts->compulsory_memory_facts_available = 1;
            if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) facts->d2d_bytes = view.bytes;
        }
        return rc;
    }
    if (!strcmp(s->implementation, "attention.full.f32acc.bf16.v1"))
        return kernel_attention(c, r, facts, err);
    if (!strncmp(s->implementation, "indexed_rows.", 13u) ||
        !strncmp(s->implementation, "partition_rows.", 15u)) return kernel_indexed_rows(c, r, facts, err);
    if (!strcmp(s->implementation, "axis_rotary.f32math.bf16.v1")) return kernel_axis_rotary(c, r, facts, err);
    if (!strcmp(s->implementation, "sinusoidal_embedding.f32.v1"))
        return c->ops->sinusoidal_embedding(c->backend, input, output, r->rows, output->dims[1] / 2u,
            (float)yvex_program_physical_attribute(s, "maximum_period")->value.real, facts, err);
    if (!strcmp(s->implementation, "rotary_tables.f64.bf16.v1"))
        return kernel_rotary_tables(c, r, facts, err);
    if (!strcmp(s->implementation, "masked_rows.host.bf16.v1"))
        return kernel_masked_rows(c, r, facts, err);
    if (!strcmp(s->implementation, "linear_bias.bf16.f32acc.v1")) {
        const yvex_component_encoded_weight *w = &c->weights[s->operands[1]], *bias = &c->weights[s->operands[2]];
        return c->ops->linear_bias_bf16(c->backend, w->encoded, w->encoded_bytes,
            bias->encoded, bias->encoded_bytes, w->row_count, w->row_width, r->rows, input, output, facts, err);
    }
    if (!strcmp(s->implementation, "layer_norm.f32acc.bf16.v1")) {
        int rc = c->ops->layer_norm_f32(c->backend, input, c->small[s->operands[1]], c->small[s->operands[2]],
            output, r->rows, output->dims[1], (float)yvex_program_physical_attribute(s, "epsilon")->value.real,
            facts, err);
        return kernel_round_result(c, output, rc, facts, err);
    }
    if (!strcmp(s->implementation, "gelu.erf.bf16.v1") || !strcmp(s->implementation, "gelu.tanh.bf16.v1"))
        return c->ops->gelu(c->backend, input, output, output->bytes / sizeof(float),
            !strcmp(s->implementation, "gelu.tanh.bf16.v1"), 1, facts, err);
    if (!strcmp(s->implementation, "split_three.bf16.v1"))
        return c->ops->split_three(c->backend, input, output, &r->values[s->results[1]],
            &r->values[s->results[2]], r->rows, output->dims[1], facts, err);
    if (!strcmp(s->implementation, "rotary_half.f32acc.bf16.v1")) {
        const yvex_ir_type *table = &yvex_program_physical_value_at(c->program, s->operands[1])->type;
        unsigned long long head = yvex_program_physical_attribute(s, "head_dimension")->value.integer;
        int rc = yvex_backend_tensor_copy(c->backend, output, input, err);
        if (rc == YVEX_OK) rc = c->ops->rotary_half_f32(c->backend, output,
            &r->values[s->operands[1]], &r->values[s->operands[2]], r->rows,
            output->dims[1] / head, head, table->shape[1].extent, facts, err);
        if (rc == YVEX_OK && !yvex_core_u64_add(facts->d2d_bytes, input->bytes, &facts->d2d_bytes))
            rc = kernel_refuse(err, YVEX_ERR_BOUNDS, "rotate-half copy accounting overflowed");
        return kernel_round_result(c, output, rc, facts, err);
    }
    if (!strcmp(s->implementation, "rotary_half.bf16.products.v1")) {
        const yvex_ir_type *x = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        const yvex_ir_type *table = &yvex_program_physical_value_at(c->program, s->operands[1])->type;
        unsigned long long head = yvex_program_physical_attribute(s, "head_dimension")->value.integer;
        int rc = yvex_backend_tensor_copy(c->backend, output, input, err);
        if (rc == YVEX_OK) rc = c->ops->rotary_half_bf16(c->backend, output,
            &r->values[s->operands[1]], &r->values[s->operands[2]], r->rows,
            x->shape[1].extent / head, head, table->shape[1].extent, facts, err);
        if (rc == YVEX_OK && !yvex_core_u64_add(facts->d2d_bytes, input->bytes, &facts->d2d_bytes))
            rc = kernel_refuse(err, YVEX_ERR_BOUNDS, "rotate-half copy accounting overflowed");
        if (rc != YVEX_OK) output->is_written = 0;
        return rc;
    }
    if (!strcmp(s->implementation, "linear_residual.bf16.f32add.v1"))
        return kernel_linear_residual(c, r, input, output, facts, err);
    if (!strcmp(s->implementation, "mhc.residual_pre.bf16.v1")) {
        const yvex_ir_type *x = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        yvex_device_tensor scratch = {0};
        if (yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU &&
            !yvex_backend_tensor_f32_subview(c->small[s->results[0]], 0u, input->bytes / sizeof(float), &scratch))
            return kernel_refuse(err, YVEX_ERR_STATE, "mHC workspace cannot cover the admitted population");
        scratch.rank = input->rank;
        memcpy(scratch.dims, input->dims, sizeof(scratch.dims));
        yvex_mhc_device_request call = {
            .geometry = {x->shape[1].extent, x->shape[2].extent,
                yvex_program_physical_attribute(s, "sinkhorn_iterations")->value.integer,
                yvex_program_physical_attribute(s, "epsilon")->value.real,
                yvex_program_physical_attribute(s, "mhc_epsilon")->value.real,
                yvex_program_physical_attribute(s, "post_multiplier")->value.real},
            .rows = r->rows, .inputs = {input, &r->values[s->operands[1]],
                c->small[s->operands[2]], c->small[s->operands[3]]},
            .outputs = {output, &r->values[s->results[1]], &r->values[s->results[2]]}, .workspace = &scratch};
        return c->ops->residual_pre(c->backend, &call, facts, err);
    }
    if (!strcmp(s->implementation, "weighted_rms.f64scale.bf16.v1"))
        return c->ops->weighted_rms_bf16(c->backend, input, c->small[s->operands[1]], output,
            r->rows, output->dims[1], yvex_program_physical_attribute(s, "epsilon")->value.real, facts, err);
    if (!strcmp(s->implementation, "mhc.residual_post.f64acc.bf16.v1")) {
        const yvex_ir_type *type = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        return c->ops->residual_post(c->backend, input, &r->values[s->operands[1]],
            &r->values[s->operands[2]], &r->values[s->operands[3]], r->rows,
            type->shape[1].extent, type->shape[2].extent, output, facts, err);
    }
    if (!strcmp(s->implementation, "stream_mean.f32.f64acc.v1")) {
        const yvex_ir_type *type = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        return c->ops->feature_mean(c->backend, input, r->rows, type->shape[2].extent,
            type->shape[1].extent, output, NULL, 0u, 0u, 0u, NULL, facts, err);
    }
    if (!strcmp(s->implementation, "mhc.head_norm.bf16.v1")) {
        const yvex_ir_type *type = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        const yvex_ir_attribute *epsilon = yvex_program_physical_attribute(s, "epsilon");
        const yvex_ir_attribute *mhc = yvex_program_physical_attribute(s, "mhc_epsilon");
        return c->ops->final(c->backend, input, c->small[s->operands[1]],
            c->small[s->operands[2]], c->small[s->operands[3]], c->small[s->operands[4]], r->rows,
            type->shape[2].extent, type->shape[1].extent, epsilon->value.real, mhc->value.real,
            &r->values[s->results[1]], output, facts, err);
    }
    if (!strcmp(s->implementation, "linear.encoded.f32.v1") ||
        !strcmp(s->implementation, "linear.row_dot.f32.v1") || !strcmp(s->implementation, "linear.row_dot.q8.v1"))
        return kernel_encoded_linear(c, r, input, output, facts, err);
    if (!strcmp(s->implementation, "linear.bf16.f32acc.v1")) {
        const program_kernel_linear *l = &c->linears[c->linear_slots[r->step_index]];
        yvex_transformer_linear_execution_request request = {
            .executable = r->rows == 1u ? l->single : l->multiple,
            .weight = &c->weights[s->operands[1]], .input = input, .output = output};
        if (!request.executable || (r->rows != 1u && l->prepared_multiple_rows != r->rows))
            return kernel_refuse(err, YVEX_ERR_STATE, "linear invocation requires exact prepared population");
        return c->ops->linear_execute(c->backend, &request, facts, err);
    }
    if (!strcmp(s->implementation, "embedding.bf16.v1") ||
        !strcmp(s->implementation, "embedding.encoded.f32.v1"))
        return kernel_embedding(c, r, output, facts, err);
    if (!strcmp(s->implementation, "silu_product.bf16.v1"))
        return c->ops->silu_product_bf16(c->backend, input, &r->values[s->operands[1]], output,
            output->bytes / sizeof(float), facts, err);
    if (!strcmp(s->implementation, "add.bf16.v1"))
        return c->ops->add_bf16(c->backend, input, &r->values[s->operands[1]], output,
            r->rows, output->dims[1], facts, err);
    if (!strcmp(s->implementation, "rms_norm.bf16.v1")) {
        const yvex_ir_attribute *epsilon = yvex_program_physical_attribute(s, "epsilon");
        int rc = yvex_backend_op_rms_norm(c->backend, input, c->small[s->operands[1]],
                                         (float)epsilon->value.real, output, err);
        return rc == YVEX_OK ? c->ops->bf16_round(c->backend, output, output->bytes / sizeof(float), facts, err) : rc;
    }
    if (!strcmp(s->implementation, "group_rms_norm.bf16.vector4.v1")) {
        const yvex_ir_type *weight = &yvex_program_physical_value_at(c->program, s->operands[1])->type;
        const yvex_ir_attribute *epsilon = yvex_program_physical_attribute(s, "epsilon");
        unsigned long long width = weight->shape[0].extent;
        return c->ops->group_rms_norm_bf16(c->backend, input, c->small[s->operands[1]], output,
            output->bytes / sizeof(float) / width, width, (float)epsilon->value.real, facts, err);
    }
    return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "operation belongs to another implementation owner");
}

const yvex_device_tensor *yvex_program_kernels_small_weight(const yvex_program_kernels *c, yvex_ir_id slot)
{
    return c && slot < c->summary->value_count ? c->small[slot] : NULL;
}

void yvex_program_kernels_resources(const yvex_program_kernels *c, unsigned long long *host, unsigned long long *device)
{
    if (host) *host = c ? c->host_bytes : 0u;
    if (device) *device = c ? c->device_bytes : 0u;
}

int yvex_program_kernels_close(yvex_program_kernels **out, yvex_error *err)
{
    yvex_program_kernels *c = out ? *out : NULL;
    size_t i;
    int rc;
    if (!c) return YVEX_OK;
    if (c->signal_workspace &&
        (rc = yvex_backend_tensor_release(c->backend, &c->signal_workspace, err)) != YVEX_OK) return rc;
    for (i = 0u; c->linears && i < c->linear_count; ++i) {
        if (c->linears[i].single &&
            (rc = c->ops->linear_release(c->backend, &c->linears[i].single, err)) != YVEX_OK) return rc;
        if (c->linears[i].multiple &&
            (rc = c->ops->linear_release(c->backend, &c->linears[i].multiple, err)) != YVEX_OK) return rc;
    }
    for (i = 0u; c->small && i < c->summary->value_count; ++i)
        if (c->small[i] && (rc = yvex_backend_tensor_release(c->backend, &c->small[i], err)) != YVEX_OK) return rc;
    free(c->linear_slots); free(c->linears); free(c->offsets); free(c->small); free(c->weights);
    free(c->host_staging);
    free(c->source_staging); free(c->sources);
    yvex_program_physical_close(&c->program);
    free(c);
    *out = NULL;
    return YVEX_OK;
}
