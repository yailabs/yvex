/* Bind exact parameter handles and prepare reusable numerical implementations.
 * Neither tensor roles nor model/family names participate in invocation. */
#include <yvex/internal/program_kernels.h>
#include <yvex/internal/component.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    yvex_transformer_linear_requirement requirement;
    yvex_transformer_linear_executable *single, *multiple;
} program_kernel_linear;

struct yvex_program_kernels {
    const yvex_program_physical *program;
    const yvex_program_physical_summary *summary;
    yvex_backend *backend;
    const yvex_backend_transformer_operations *ops;
    yvex_component_encoded_weight *weights;
    yvex_device_tensor **small;
    double *offsets;
    program_kernel_linear *linears;
    size_t *linear_slots, linear_count;
    unsigned long long host_bytes, device_bytes, host_limit, device_limit, multiple_rows;
    int single_ready, multiple_ready;
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
        const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(v->qtype);
        unsigned long long bytes, row_bytes, elements = 1u;
        size_t matches = 0u;
        unsigned int axis;
        if (!v->parameter) continue;
        for (j = 0u; j < parameter_count; ++j) {
            if (parameters[j].tensor_id == v->tensor_id) { binding = &parameters[j].weight; matches++; }
        }
        if (matches != 1u || !binding || binding->qtype != v->qtype || !binding->row_count ||
            binding->row_width != v->type.shape[v->type.rank - 1u].extent)
            return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter handle has no exact artifact storage");
        for (axis = 0u; axis < v->type.rank; ++axis)
            if (!yvex_core_u64_mul(elements, v->type.shape[axis].extent, &elements))
                return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter shape differs from artifact storage");
        if (!geometry || !geometry->block_size || !geometry->bytes_per_block ||
            binding->row_width % geometry->block_size ||
            !yvex_core_u64_mul(binding->row_width / geometry->block_size, geometry->bytes_per_block, &row_bytes) ||
            !yvex_core_u64_mul(elements / binding->row_width, row_bytes, &bytes) || bytes != binding->encoded_bytes)
            return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter block geometry is inconsistent");
        if (!binding->encoded || binding->row_count != elements / binding->row_width ||
            binding->row_bytes != row_bytes)
            return kernel_refuse(err, YVEX_ERR_FORMAT, "compiled parameter residency is incomplete");
        c->weights[i] = *binding;
    }
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
    for (i = 0u; rc == YVEX_OK && i < count; ++i) host[i] += (float)offset;
    d.dims[0] = count;
    if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(c->backend, &d, &c->small[slot], err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_write(c->backend, c->small[slot], host, d.bytes, err);
    if (rc == YVEX_OK) rc = kernel_account(c, 0u, d.bytes, err);
    if (rc == YVEX_OK) c->offsets[slot] = offset;
    free(host);
    return rc;
}

static int kernel_instructions_bind(yvex_program_kernels *c, yvex_error *err)
{
    size_t i, j;
    int rc = YVEX_OK;
    for (i = 0u; rc == YVEX_OK && i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        c->linear_slots[i] = SIZE_MAX;
        if (!strcmp(s->implementation, "parameter.encoded.v1") ||
            !strcmp(s->implementation, "linear.encoded.f32.v1")) continue;
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
                    c->linears[j].requirement.output_width == r->shape[1].extent) break;
            if (j == c->linear_count) {
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
    host = sizeof(*c) + s->value_count * (sizeof(*c->weights) + sizeof(*c->small) + sizeof(*c->offsets)) +
        s->step_count * (sizeof(*c->linears) + sizeof(*c->linear_slots));
    if (host_limit && host > host_limit)
        return kernel_refuse(err, YVEX_ERR_BOUNDS, "kernel directory exceeds host budget");
    c = calloc(1u, sizeof(*c));
    if (!c) return kernel_refuse(err, YVEX_ERR_NOMEM, "kernel owner allocation failed");
    c->program = program; c->summary = s; c->backend = backend; c->ops = ops;
    c->host_bytes = host; c->host_limit = host_limit; c->device_limit = device_limit;
    c->weights = calloc(s->value_count, sizeof(*c->weights));
    c->small = calloc(s->value_count, sizeof(*c->small));
    c->offsets = calloc(s->value_count, sizeof(*c->offsets));
    c->linears = calloc(s->step_count, sizeof(*c->linears));
    c->linear_slots = calloc(s->step_count, sizeof(*c->linear_slots));
    rc = c->weights && c->small && c->offsets && c->linears && c->linear_slots ? YVEX_OK :
        kernel_refuse(err, YVEX_ERR_NOMEM, "kernel directory allocation failed");
    if (rc == YVEX_OK) rc = kernel_parameters_bind(c, parameters, parameter_count, err);
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
    return rc;
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
    for (i = 0u; rc == YVEX_OK && i < c->linear_count; ++i) rc = kernel_linear_prepare(c, &c->linears[i], rows, err);
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

int yvex_program_kernels_invoke(yvex_program_kernels *c, const yvex_program_device_invocation *r,
                                yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s;
    yvex_device_tensor *output;
    const yvex_device_tensor *input;
    if (!c || !r || r->program != c->program || !facts || !r->arguments || !r->values ||
        r->step != yvex_program_physical_step_at(c->program, r->step_index) || !r->step)
        return kernel_refuse(err, YVEX_ERR_INVALID_ARG, "kernel requires its bound physical invocation");
    s = r->step;
    if (!s->operand_count || !s->result_count)
        return kernel_refuse(err, YVEX_ERR_UNSUPPORTED, "operation has no admitted numerical operands/results");
    output = &r->values[s->results[0]];
    input = &r->values[s->operands[0]];
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
    if (!strcmp(s->implementation, "linear.encoded.f32.v1")) {
        const yvex_component_encoded_weight *w = &c->weights[s->operands[1]];
        const yvex_ir_type *type = &yvex_program_physical_value_at(c->program, s->operands[0])->type;
        yvex_encoded_input_policy precision = type->scalar == YVEX_IR_BF16 && w->qtype == YVEX_GGUF_QTYPE_BF16 ?
            YVEX_ENCODED_INPUT_BF16 : YVEX_ENCODED_INPUT_F32;
        if (yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU)
            return kernel_linear_cpu(w, r, input, output, facts, err);
        return yvex_backend_encoded_matvec(c->backend, w->encoded, w->encoded_bytes, w->qtype,
            w->row_count, w->row_width, w->row_bytes, r->rows, input, NULL, 0u, NULL,
            output, precision, facts, err);
    }
    if (!strcmp(s->implementation, "linear.bf16.f32acc.v1")) {
        const program_kernel_linear *l = &c->linears[c->linear_slots[r->step_index]];
        yvex_transformer_linear_execution_request request = {
            .executable = r->rows == 1u ? l->single : l->multiple,
            .weight = &c->weights[s->operands[1]], .input = input, .output = output};
        if (r->rows == 1u ? !c->single_ready : !c->multiple_ready || c->multiple_rows != r->rows)
            return kernel_refuse(err, YVEX_ERR_STATE, "linear invocation requires exact prepared population");
        return c->ops->linear_execute(c->backend, &request, facts, err);
    }
    if (!strcmp(s->implementation, "embedding.bf16.v1")) {
        const yvex_component_encoded_weight *w = &c->weights[s->operands[1]];
        return yvex_backend_encoded_gather(c->backend, w->encoded, w->encoded_bytes, w->qtype,
            w->row_count, w->row_width, w->row_bytes, r->arguments[s->operands[0]].indices,
            r->rows, output, facts, err);
    }
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
    for (i = 0u; c->linears && i < c->linear_count; ++i) {
        if (c->linears[i].single &&
            (rc = c->ops->linear_release(c->backend, &c->linears[i].single, err)) != YVEX_OK) return rc;
        if (c->linears[i].multiple &&
            (rc = c->ops->linear_release(c->backend, &c->linears[i].multiple, err)) != YVEX_OK) return rc;
    }
    for (i = 0u; c->small && i < c->summary->value_count; ++i)
        if (c->small[i] && (rc = yvex_backend_tensor_release(c->backend, &c->small[i], err)) != YVEX_OK) return rc;
    free(c->linear_slots); free(c->linears); free(c->offsets); free(c->small); free(c->weights);
    free(c);
    *out = NULL;
    return YVEX_OK;
}
