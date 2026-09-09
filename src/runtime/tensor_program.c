/* Execute lowered tensor instructions with one owner for prepared work and storage. */
#include <yvex/internal/tensor_execution.h>
#include <yvex/internal/transformer.h>
#include <yvex/qtype.h>

#include <stdlib.h>
#include <string.h>

typedef struct tensor_instruction tensor_instruction;
typedef int (*tensor_invoke)(struct yvex_tensor_execution *, const tensor_instruction *,
                             const yvex_tensor_execution_argument *, unsigned long long,
                             yvex_backend_operation_facts *, yvex_error *);

struct tensor_instruction {
    const yvex_program_tensor_step *step;
    tensor_invoke invoke;
    size_t linear;
};

typedef struct {
    yvex_transformer_linear_requirement requirement;
    yvex_transformer_linear_executable *single, *multiple;
} tensor_linear;

struct yvex_tensor_execution {
    const yvex_program_tensor_plan *plan;
    const yvex_program_tensor_summary *summary;
    yvex_backend *backend;
    const yvex_backend_transformer_operations *operations;
    tensor_instruction *instructions;
    tensor_linear *linears;
    size_t linear_count;
    yvex_device_tensor **storage;
    yvex_device_tensor *views;
    unsigned long long capacity, multiple_rows, maximum_host_bytes, maximum_device_bytes;
    int single_ready, multiple_ready;
    yvex_tensor_execution_resources resources;
};

static int tensor_runtime_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "runtime.tensor-program", reason);
    return code;
}

static int tensor_linear_invoke(yvex_tensor_execution *c, const tensor_instruction *instruction,
                                 const yvex_tensor_execution_argument *arguments, unsigned long long rows,
                                 yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_tensor_step *s = instruction->step;
    tensor_linear *linear = &c->linears[instruction->linear];
    yvex_transformer_linear_execution_request request = {
        .executable = rows == 1u ? linear->single : linear->multiple,
        .weight = arguments[s->operands[1]].parameter,
        .input = &c->views[s->operands[0]], .output = &c->views[s->result]};
    return c->operations->linear_execute(c->backend, &request, facts, err);
}

static int tensor_silu_invoke(yvex_tensor_execution *c, const tensor_instruction *instruction,
                               const yvex_tensor_execution_argument *arguments, unsigned long long rows,
                               yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_tensor_step *s = instruction->step;
    const yvex_program_tensor_value *v = yvex_program_tensor_value_at(c->plan, s->result);
    (void)arguments;
    return c->operations->silu_product_bf16(c->backend, &c->views[s->operands[0]],
        &c->views[s->operands[1]], &c->views[s->result], rows * v->width, facts, err);
}

static int tensor_add_invoke(yvex_tensor_execution *c, const tensor_instruction *instruction,
                              const yvex_tensor_execution_argument *arguments, unsigned long long rows,
                              yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_tensor_step *s = instruction->step;
    const yvex_program_tensor_value *v = yvex_program_tensor_value_at(c->plan, s->result);
    (void)arguments;
    return c->operations->add_bf16(c->backend, &c->views[s->operands[0]],
        &c->views[s->operands[1]], &c->views[s->result], rows, v->width, facts, err);
}

static int tensor_instructions_bind(yvex_tensor_execution *c, yvex_error *err)
{
    static const struct { const char *name; tensor_invoke invoke; } implementations[] = {
        {"linear.bf16.f32acc.v1", tensor_linear_invoke},
        {"silu_product.bf16.v1", tensor_silu_invoke}, {"add.bf16.v1", tensor_add_invoke}};
    size_t i, j;
    for (i = 0u; i < c->summary->step_count; ++i) {
        tensor_instruction *instruction = &c->instructions[i];
        const yvex_program_tensor_step *s = yvex_program_tensor_step_at(c->plan, i);
        instruction->step = s;
        instruction->linear = SIZE_MAX;
        for (j = 0u; j < sizeof(implementations) / sizeof(implementations[0]); ++j)
            if (!strcmp(s->implementation, implementations[j].name)) instruction->invoke = implementations[j].invoke;
        if (!instruction->invoke ||
            (instruction->invoke == tensor_silu_invoke && !c->operations->silu_product_bf16) ||
            (instruction->invoke == tensor_add_invoke && !c->operations->add_bf16))
            return tensor_runtime_refuse(err, YVEX_ERR_UNSUPPORTED, "tensor implementation is not admitted");
        if (instruction->invoke == tensor_linear_invoke) {
            const yvex_program_tensor_value *a = yvex_program_tensor_value_at(c->plan, s->operands[0]);
            const yvex_program_tensor_value *r = yvex_program_tensor_value_at(c->plan, s->result);
            if (!c->operations->linear_compile || !c->operations->linear_execute ||
                !c->operations->linear_release || !c->operations->linear_summary)
                return tensor_runtime_refuse(err, YVEX_ERR_UNSUPPORTED, "backend lacks admitted linear execution");
            for (j = 0u; j < c->linear_count; ++j)
                if (c->linears[j].requirement.input_width == a->width &&
                    c->linears[j].requirement.output_width == r->width) break;
            if (j == c->linear_count) {
                c->linears[c->linear_count++].requirement = (yvex_transformer_linear_requirement){
                    .operation = YVEX_TRANSFORMER_LINEAR_OPERATION_PROJECTION,
                    .publication_contract = YVEX_TRANSFORMER_LINEAR_NUMERIC_BF16_F32_ACCUMULATION,
                    .source_dtype = YVEX_DTYPE_BF16, .input_dtype = YVEX_DTYPE_F32,
                    .accumulation_dtype = YVEX_DTYPE_F32, .output_dtype = YVEX_DTYPE_F32,
                    .publication_dtype = YVEX_DTYPE_BF16, .input_width = a->width, .output_width = r->width};
            }
            instruction->linear = j;
        }
    }
    return YVEX_OK;
}

static int tensor_is_output(const yvex_tensor_execution *c, size_t value)
{
    size_t i;
    for (i = 0u; i < c->summary->result_count; ++i)
        if (yvex_program_tensor_result_at(c->plan, i) == value) return 1;
    return 0;
}

static int tensor_workspace_open(yvex_tensor_execution *c, yvex_error *err)
{
    size_t i;
    unsigned long long bytes, total;
    for (i = c->summary->input_count; i < c->summary->value_count; ++i) {
        const yvex_program_tensor_value *v = yvex_program_tensor_value_at(c->plan, i);
        yvex_backend_tensor_desc desc = {.name = "program-activation", .dtype = YVEX_DTYPE_F32, .rank = 2u};
        int rc;
        if (tensor_is_output(c, i)) continue;
        if (!yvex_core_u64_mul(c->capacity, v->width, &bytes) ||
            !yvex_core_u64_mul(bytes, sizeof(float), &bytes) ||
            !yvex_core_u64_add(c->resources.device_bytes, bytes, &total) ||
            (c->maximum_device_bytes && total > c->maximum_device_bytes))
            return tensor_runtime_refuse(err, YVEX_ERR_BOUNDS, "program workspace exceeds the device budget");
        desc.dims[0] = c->capacity;
        desc.dims[1] = v->width;
        desc.bytes = bytes;
        rc = yvex_backend_tensor_alloc(c->backend, &desc, &c->storage[i], err);
        if (rc != YVEX_OK) return rc;
        c->resources.device_bytes = total;
    }
    return YVEX_OK;
}

int yvex_tensor_execution_open(yvex_tensor_execution **out, const yvex_program_tensor_plan *plan,
                               yvex_backend *backend, unsigned long long capacity,
                               unsigned long long host_limit, unsigned long long device_limit, yvex_error *err)
{
    const yvex_program_tensor_summary *s = yvex_program_tensor_summary_get(plan);
    const yvex_backend_transformer_operations *ops = yvex_backend_transformer_operations_get(backend);
    yvex_tensor_execution *c;
    unsigned long long bytes;
    int rc;
    if (out) *out = NULL;
    if (!out || !s || !backend || !capacity || capacity < s->minimum_rows || capacity > s->maximum_rows)
        return tensor_runtime_refuse(err, YVEX_ERR_INVALID_ARG, "admitted tensor plan, backend and capacity required");
    if (!ops)
        return tensor_runtime_refuse(err, YVEX_ERR_UNSUPPORTED, "backend lacks the selected tensor implementations");
    bytes = sizeof(*c) + s->step_count * (sizeof(tensor_instruction) + sizeof(tensor_linear)) +
            s->value_count * (sizeof(yvex_device_tensor *) + sizeof(yvex_device_tensor));
    if (host_limit && bytes > host_limit)
        return tensor_runtime_refuse(err, YVEX_ERR_BOUNDS, "tensor execution directory exceeds the host budget");
    c = calloc(1u, sizeof(*c));
    if (!c) return tensor_runtime_refuse(err, YVEX_ERR_NOMEM, "tensor execution owner allocation failed");
    c->plan = plan;
    c->summary = s;
    c->backend = backend;
    c->operations = ops;
    c->capacity = capacity;
    c->maximum_host_bytes = host_limit;
    c->maximum_device_bytes = device_limit;
    c->resources.host_bytes = bytes;
    c->instructions = calloc(s->step_count, sizeof(*c->instructions));
    c->linears = calloc(s->step_count, sizeof(*c->linears));
    c->storage = calloc(s->value_count, sizeof(*c->storage));
    c->views = calloc(s->value_count, sizeof(*c->views));
    rc = c->instructions && c->linears && c->storage && c->views ? YVEX_OK :
         tensor_runtime_refuse(err, YVEX_ERR_NOMEM, "tensor execution directory allocation failed");
    if (rc == YVEX_OK) rc = tensor_instructions_bind(c, err);
    if (rc == YVEX_OK) rc = tensor_workspace_open(c, err);
    if (rc != YVEX_OK) (void)yvex_tensor_execution_close(&c, NULL);
    *out = c; /* A failed checked cleanup retains its owner for retry. */
    return rc;
}

static int tensor_linear_prepare(yvex_tensor_execution *c, tensor_linear *linear,
                                  unsigned long long rows, yvex_error *err)
{
    yvex_transformer_linear_executable **slot = rows == 1u ? &linear->single : &linear->multiple;
    yvex_transformer_linear_compile_request request = {.semantic_domain = "program.linear.bf16.f32acc.v1",
        .requirement = &linear->requirement, .input_rows = rows};
    yvex_transformer_linear_executable_summary old = {0}, summary = {0};
    unsigned long long host, device;
    int rc = YVEX_OK;
    if (*slot) rc = c->operations->linear_summary(*slot, &old, err);
    if (rc == YVEX_OK && (old.plan_host_bytes > c->resources.host_bytes ||
        old.prepared_weight_bytes > c->resources.device_bytes))
        return tensor_runtime_refuse(err, YVEX_ERR_STATE, "backend specialization accounting is inconsistent");
    if (rc == YVEX_OK && *slot) rc = c->operations->linear_release(c->backend, slot, err);
    if (rc != YVEX_OK) return rc;
    c->resources.host_bytes -= old.plan_host_bytes;
    c->resources.device_bytes -= old.prepared_weight_bytes;
    rc = c->operations->linear_compile(c->backend, &request, slot, &summary, err);
    if (rc != YVEX_OK) return rc;
    c->resources.preparation_count++;
    if (!summary.exact || summary.input_rows != rows || !yvex_sha256_hex_valid(summary.identity) ||
        !yvex_core_u64_add(c->resources.host_bytes, summary.plan_host_bytes, &host) ||
        !yvex_core_u64_add(summary.prepared_weight_bytes, c->resources.device_bytes, &device)) {
        (void)c->operations->linear_release(c->backend, slot, NULL);
        return tensor_runtime_refuse(err, YVEX_ERR_STATE, "backend returned invalid tensor specialization facts");
    }
    c->resources.host_bytes = host;
    c->resources.device_bytes = device;
    if (summary.workspace_bytes > c->resources.workspace_required_bytes)
        c->resources.workspace_required_bytes = summary.workspace_bytes;
    if ((c->maximum_host_bytes && host > c->maximum_host_bytes) ||
        (c->maximum_device_bytes && device > c->maximum_device_bytes))
        return tensor_runtime_refuse(err, YVEX_ERR_BOUNDS, "tensor specialization exceeds the resource budget");
    return YVEX_OK;
}

int yvex_tensor_execution_prepare(yvex_tensor_execution *c, unsigned long long rows, yvex_error *err)
{
    size_t i;
    int rc = YVEX_OK;
    if (!c || rows < c->summary->minimum_rows || rows > c->capacity || rows % c->summary->row_multiple)
        return tensor_runtime_refuse(err, YVEX_ERR_BOUNDS, "row population violates admitted program constraints");
    if ((rows == 1u && c->single_ready) || (rows != 1u && c->multiple_ready && c->multiple_rows == rows))
        return YVEX_OK;
    if (rows == 1u) c->single_ready = 0;
    else c->multiple_ready = 0;
    for (i = 0u; rc == YVEX_OK && i < c->linear_count; ++i) rc = tensor_linear_prepare(c, &c->linears[i], rows, err);
    if (rc == YVEX_OK) {
        if (rows == 1u) c->single_ready = 1;
        else { c->multiple_ready = 1; c->multiple_rows = rows; }
    }
    return rc;
}

static int tensor_activation_view(yvex_tensor_execution *c, const yvex_device_tensor *tensor,
                                   size_t value, unsigned long long rows)
{
    const yvex_program_tensor_value *v = yvex_program_tensor_value_at(c->plan, value);
    unsigned long long elements;
    if (!tensor || tensor->dtype != YVEX_DTYPE_F32 || tensor->rank != 2u ||
        tensor->dims[0] < rows || tensor->dims[1] != v->width ||
        !yvex_backend_tensor_owned_by(c->backend, tensor) ||
        !yvex_core_u64_mul(rows, v->width, &elements) ||
        !yvex_backend_tensor_f32_subview(tensor, 0u, elements, &c->views[value])) return 0;
    c->views[value].rank = 2u;
    c->views[value].dims[0] = rows;
    c->views[value].dims[1] = v->width;
    return 1;
}

static int tensor_ranges_disjoint(const void *a, unsigned long long an,
                                   const void *b, unsigned long long bn)
{
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    if (!a || !b || an > UINTPTR_MAX - ap || bn > UINTPTR_MAX - bp) return 0;
    return ap + an <= bp || bp + bn <= ap;
}

static int tensor_outputs_disjoint(yvex_tensor_execution *c,
                                    const yvex_tensor_execution_argument *arguments,
                                    yvex_device_tensor *const *outputs)
{
    size_t i, j;
    for (i = 0u; i < c->summary->result_count; ++i) {
        const yvex_device_tensor *out = outputs[i];
        if (!out) return 0;
        for (j = 0u; j < c->summary->input_count; ++j) {
            const yvex_device_tensor *in = arguments[j].tensor;
            const yvex_component_encoded_weight *weight = arguments[j].parameter;
            if ((in && !tensor_ranges_disjoint(out->data, out->bytes, in->data, in->bytes)) ||
                (weight && !tensor_ranges_disjoint(out->data, out->bytes, weight->encoded, weight->encoded_bytes)))
                return 0;
        }
        for (j = 0u; j < i; ++j)
            if (!tensor_ranges_disjoint(out->data, out->bytes, outputs[j]->data, outputs[j]->bytes)) return 0;
    }
    return 1;
}

static int tensor_arguments_bind(yvex_tensor_execution *c, unsigned long long rows,
                                  const yvex_tensor_execution_argument *arguments,
                                  yvex_device_tensor *const *outputs, yvex_error *err)
{
    size_t i;
    if (!tensor_outputs_disjoint(c, arguments, outputs))
        return tensor_runtime_refuse(err, YVEX_ERR_FORMAT, "tensor output aliases an input or another output");
    for (i = 0u; i < c->summary->input_count; ++i) {
        const yvex_program_tensor_value *v = yvex_program_tensor_value_at(c->plan, i);
        const yvex_component_encoded_weight *w = arguments[i].parameter;
        unsigned long long row_bytes, bytes;
        if (v->parameter) {
            if (arguments[i].tensor || !w || !w->encoded || w->qtype != YVEX_GGUF_QTYPE_BF16 ||
                w->row_count != v->rows || w->row_width != v->width ||
                !yvex_core_u64_mul(v->width, 2u, &row_bytes) || !yvex_core_u64_mul(row_bytes, v->rows, &bytes) ||
                w->row_bytes != row_bytes || w->encoded_bytes != bytes)
                return tensor_runtime_refuse(err, YVEX_ERR_FORMAT, "parameter differs from admitted physical type");
        } else if (w || !tensor_activation_view(c, arguments[i].tensor, i, rows))
            return tensor_runtime_refuse(err, YVEX_ERR_FORMAT, "activation differs from admitted physical type");
    }
    for (i = 0u; i < c->summary->result_count; ++i)
        if (!tensor_activation_view(c, outputs[i], yvex_program_tensor_result_at(c->plan, i), rows))
            return tensor_runtime_refuse(err, YVEX_ERR_FORMAT, "output differs from admitted physical type");
    for (i = c->summary->input_count; i < c->summary->value_count; ++i)
        if (c->storage[i] && !tensor_activation_view(c, c->storage[i], i, rows))
            return tensor_runtime_refuse(err, YVEX_ERR_STATE, "prepared activation storage is incomplete");
    return YVEX_OK;
}

static int tensor_facts_add(yvex_backend_operation_facts *a, const yvex_backend_operation_facts *b)
{
#define TENSOR_FACT_ADD(member) if (!yvex_core_u64_add(a->member, b->member, &a->member)) return 0
    TENSOR_FACT_ADD(h2d_bytes); TENSOR_FACT_ADD(d2h_bytes); TENSOR_FACT_ADD(d2d_bytes);
    TENSOR_FACT_ADD(kernel_launches); TENSOR_FACT_ADD(upload_count); TENSOR_FACT_ADD(download_count);
    TENSOR_FACT_ADD(queue_synchronizations); TENSOR_FACT_ADD(device_synchronizations);
    TENSOR_FACT_ADD(active_weight_bytes); TENSOR_FACT_ADD(state_bytes);
    TENSOR_FACT_ADD(activation_bytes); TENSOR_FACT_ADD(temporary_bytes); TENSOR_FACT_ADD(accelerated_matrix_launches);
#undef TENSOR_FACT_ADD
    a->compulsory_memory_facts_available &= b->compulsory_memory_facts_available;
    return 1;
}

int yvex_tensor_execution_run(yvex_tensor_execution *c, unsigned long long rows,
                              const yvex_tensor_execution_argument *arguments, size_t argument_count,
                              yvex_device_tensor *const *outputs, size_t output_count,
                              yvex_tensor_execution_result *result, yvex_error *err)
{
    size_t i;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!c || !arguments || !outputs || !result || argument_count != c->summary->input_count ||
        output_count != c->summary->result_count || rows < c->summary->minimum_rows || rows > c->capacity ||
        rows % c->summary->row_multiple ||
        (rows == 1u ? !c->single_ready : !c->multiple_ready || rows != c->multiple_rows))
        return tensor_runtime_refuse(err, YVEX_ERR_STATE, "tensor execution requires exact prepared population");
    rc = tensor_arguments_bind(c, rows, arguments, outputs, err);
    if (rc != YVEX_OK) return rc;
    for (i = 0u; i < output_count; ++i) outputs[i]->is_written = 0;
    result->backend.compulsory_memory_facts_available = 1;
    for (i = 0u; rc == YVEX_OK && i < c->summary->step_count; ++i) {
        yvex_backend_operation_facts facts = {0};
        const tensor_instruction *instruction = &c->instructions[i];
        rc = instruction->invoke(c, instruction, arguments, rows, &facts, err);
        if (rc != YVEX_OK) break;
        result->operations++;
        if (instruction->invoke == tensor_linear_invoke) result->linear_operations++;
        if (!tensor_facts_add(&result->backend, &facts))
            rc = tensor_runtime_refuse(err, YVEX_ERR_BOUNDS, "tensor execution counters overflowed");
    }
    if (rc == YVEX_OK)
        for (i = 0u; i < output_count; ++i)
            outputs[i]->is_written = c->views[yvex_program_tensor_result_at(c->plan, i)].is_written;
    return rc;
}

const yvex_tensor_execution_resources *yvex_tensor_execution_resources_get(const yvex_tensor_execution *c)
{
    return c ? &c->resources : NULL;
}

int yvex_tensor_execution_close(yvex_tensor_execution **out, yvex_error *err)
{
    yvex_tensor_execution *c = out ? *out : NULL;
    size_t i;
    int rc;
    if (!c) return YVEX_OK;
    for (i = 0u; c->linears && i < c->linear_count; ++i) {
        if (c->linears[i].single) {
            rc = c->operations->linear_release(c->backend, &c->linears[i].single, err);
            if (rc != YVEX_OK) return rc;
        }
        if (c->linears[i].multiple) {
            rc = c->operations->linear_release(c->backend, &c->linears[i].multiple, err);
            if (rc != YVEX_OK) return rc;
        }
    }
    for (i = 0u; c->storage && i < c->summary->value_count; ++i)
        if (c->storage[i] && yvex_backend_tensor_release(c->backend, &c->storage[i], err) != YVEX_OK)
            return yvex_error_code(err);
    free(c->storage);
    free(c->views);
    free(c->linears);
    free(c->instructions);
    free(c);
    *out = NULL;
    return YVEX_OK;
}
