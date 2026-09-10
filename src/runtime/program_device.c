/* Physical values, slot lifetime and operation dispatch. No model topology. */
#include <yvex/internal/program_device.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct yvex_program_device {
    const yvex_program_physical *program;
    const yvex_program_physical_summary *summary;
    yvex_backend *backend;
    yvex_program_device_kernel *kernels;
    void *kernel_context;
    yvex_device_tensor **storage, *values;
    unsigned long long capacity, host_bytes, device_bytes;
};

static int program_device_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "runtime.program", reason);
    return status;
}

static int program_device_geometry(const yvex_ir_type *t, unsigned long long rows,
                                    yvex_backend_tensor_desc *desc)
{
    unsigned int axis;
    unsigned long long count = 1u;
    *desc = (yvex_backend_tensor_desc){.name = "program-value", .dtype = YVEX_DTYPE_F32, .rank = t->rank};
    if (t->kind != YVEX_IR_TENSOR || (t->scalar != YVEX_IR_BF16 && t->scalar != YVEX_IR_F32) ||
        !t->rank || t->rank > YVEX_TENSOR_MAX_DIMS) return 0;
    for (axis = 0u; axis < t->rank; ++axis) {
        desc->dims[axis] = t->shape[axis].symbol == YVEX_IR_NONE ? t->shape[axis].extent : rows;
        if (!desc->dims[axis] || !yvex_core_u64_mul(count, desc->dims[axis], &count)) return 0;
    }
    return yvex_core_u64_mul(count, sizeof(float), &desc->bytes);
}

static int program_device_storage_open(yvex_program_device *c, unsigned long long limit, yvex_error *err)
{
    size_t i;
    for (i = 0u; i < c->summary->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(c->program, i);
        yvex_backend_tensor_desc desc;
        unsigned long long total;
        int rc;
        if (v->storage == YVEX_IR_NONE || c->storage[v->storage]) continue;
        if (!program_device_geometry(&v->type, c->capacity, &desc) ||
            !yvex_core_u64_add(c->device_bytes, desc.bytes, &total) || (limit && total > limit))
            return program_device_refuse(err, YVEX_ERR_BOUNDS, "compiled activation slots exceed device budget");
        rc = yvex_backend_tensor_alloc(c->backend, &desc, &c->storage[v->storage], err);
        if (rc != YVEX_OK) return rc;
        c->device_bytes = total;
    }
    return YVEX_OK;
}

int yvex_program_device_open(yvex_program_device **out, const yvex_program_physical *p, yvex_backend *backend,
    unsigned long long capacity, unsigned long long host_limit, unsigned long long device_limit,
    const yvex_program_device_kernel *kernels, size_t count, void *context, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p);
    yvex_program_device *c;
    unsigned long long host;
    size_t i, j;
    int rc;
    if (out) *out = NULL;
    if (!out || !s || !backend || !kernels || !count || capacity < s->minimum_rows || capacity > s->maximum_rows)
        return program_device_refuse(err, YVEX_ERR_INVALID_ARG,
            "verified physical work, backend and capacity required");
    host = sizeof(*c) + s->step_count * sizeof(*c->kernels) +
           s->storage_count * sizeof(*c->storage) + s->value_count * sizeof(*c->values);
    if (host_limit && host > host_limit)
        return program_device_refuse(err, YVEX_ERR_BOUNDS, "execution directory exceeds host budget");
    c = calloc(1u, sizeof(*c));
    if (!c) return program_device_refuse(err, YVEX_ERR_NOMEM, "execution owner allocation failed");
    c->program = p;
    c->summary = s;
    c->backend = backend;
    c->capacity = capacity;
    c->host_bytes = host;
    c->kernel_context = context;
    c->kernels = calloc(s->step_count, sizeof(*c->kernels));
    c->storage = calloc(s->storage_count ? s->storage_count : 1u, sizeof(*c->storage));
    c->values = calloc(s->value_count, sizeof(*c->values));
    rc = c->kernels && c->storage && c->values ? YVEX_OK :
        program_device_refuse(err, YVEX_ERR_NOMEM, "execution directory allocation failed");
    for (i = 0u; rc == YVEX_OK && i < s->step_count; ++i) {
        const char *name = yvex_program_physical_step_at(p, i)->implementation;
        size_t matches = 0u;
        if (!strcmp(name, "parameter.encoded.v1")) continue;
        for (j = 0u; j < count; ++j) if (kernels[j].implementation && !strcmp(name, kernels[j].implementation)) {
            c->kernels[i] = kernels[j];
            matches++;
        }
        if (matches != 1u || !c->kernels[i].invoke)
            rc = program_device_refuse(err, YVEX_ERR_UNSUPPORTED,
                "physical operation requires one admitted implementation");
    }
    if (rc == YVEX_OK) rc = program_device_storage_open(c, device_limit, err);
    if (rc != YVEX_OK) (void)yvex_program_device_close(&c, NULL);
    *out = c; /* Retain a failed cleanup owner for checked retry. */
    return rc;
}

static int program_device_view(yvex_program_device *c, size_t slot, const yvex_device_tensor *tensor,
                                unsigned long long rows)
{
    const yvex_program_physical_value *v = yvex_program_physical_value_at(c->program, slot);
    yvex_backend_tensor_desc desc;
    unsigned int axis;
    if (!program_device_geometry(&v->type, rows, &desc) || !tensor || tensor->dtype != YVEX_DTYPE_F32 ||
        tensor->rank != desc.rank || !yvex_backend_tensor_owned_by(c->backend, tensor)) return 0;
    for (axis = 0u; axis < desc.rank; ++axis)
        if (tensor->dims[axis] != desc.dims[axis] &&
            (axis || v->type.shape[0].symbol == YVEX_IR_NONE || tensor->dims[0] < rows)) return 0;
    if (!yvex_backend_tensor_f32_subview(tensor, 0u, desc.bytes / sizeof(float), &c->values[slot])) return 0;
    c->values[slot].rank = desc.rank;
    memcpy(c->values[slot].dims, desc.dims, desc.rank * sizeof(desc.dims[0]));
    return 1;
}

static int program_device_disjoint(const yvex_device_tensor *a, const yvex_device_tensor *b)
{
    uintptr_t ap = a ? (uintptr_t)a->data : 0u, bp = b ? (uintptr_t)b->data : 0u;
    return !a || !b || (ap && bp && a->bytes <= UINTPTR_MAX - ap && b->bytes <= UINTPTR_MAX - bp &&
        (ap + a->bytes <= bp || bp + b->bytes <= ap));
}

static int program_device_arguments(yvex_program_device *c, unsigned long long rows,
    const yvex_program_device_argument *args, yvex_device_tensor *const *outputs, size_t count, yvex_error *err)
{
    size_t i, j, output = 0u;
    for (i = 0u; i < c->summary->input_count; ++i) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(c->program, i)->type;
        if (t->kind == YVEX_IR_STATE) {
            if (args[i].tensor || args[i].indices || args[i].state_handle == ULLONG_MAX)
                return program_device_refuse(err, YVEX_ERR_FORMAT, "state input requires a provider lifetime handle");
        } else if (t->kind == YVEX_IR_SCALAR && t->scalar == YVEX_IR_INDEX) {
            if (args[i].tensor || args[i].indices)
                return program_device_refuse(err, YVEX_ERR_FORMAT, "index argument has conflicting representations");
        } else if (t->kind == YVEX_IR_TENSOR && t->scalar == YVEX_IR_INDEX && t->rank == 1u) {
            if (!args[i].indices || args[i].tensor)
                return program_device_refuse(err, YVEX_ERR_FORMAT, "index stream is missing");
        } else if (args[i].indices || !program_device_view(c, i, args[i].tensor, rows) || !args[i].tensor->is_written)
            return program_device_refuse(err, YVEX_ERR_FORMAT, "input tensor differs from its admitted physical type");
    }
    for (i = 0u; i < c->summary->result_count; ++i) {
        yvex_ir_id slot = yvex_program_physical_result_at(c->program, i);
        const yvex_ir_type *t = &yvex_program_physical_value_at(c->program, slot)->type;
        if (t->kind == YVEX_IR_STATE) continue;
        if (output >= count || !program_device_view(c, slot, outputs[output], rows))
            return program_device_refuse(err, YVEX_ERR_FORMAT, "output tensor differs from its admitted physical type");
        for (j = 0u; j < c->summary->input_count; ++j)
            if (!program_device_disjoint(outputs[output], args[j].tensor))
                return program_device_refuse(err, YVEX_ERR_FORMAT, "program output aliases an input");
        for (j = 0u; j < output; ++j)
            if (!program_device_disjoint(outputs[output], outputs[j]))
                return program_device_refuse(err, YVEX_ERR_FORMAT, "program outputs alias");
        output++;
    }
    if (output != count) return program_device_refuse(err, YVEX_ERR_FORMAT, "unexpected output population");
    for (i = 0u; i < c->summary->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(c->program, i);
        if (v->storage != YVEX_IR_NONE && !program_device_view(c, i, c->storage[v->storage], rows))
            return program_device_refuse(err, YVEX_ERR_STATE, "compiled activation slot is unavailable");
        if (i >= c->summary->input_count) c->values[i].is_written = 0;
    }
    return YVEX_OK;
}

static int program_device_facts(yvex_backend_operation_facts *a, const yvex_backend_operation_facts *b)
{
#define PROGRAM_FACT_ADD(member) if (!yvex_core_u64_add(a->member, b->member, &a->member)) return 0
    PROGRAM_FACT_ADD(h2d_bytes); PROGRAM_FACT_ADD(d2h_bytes); PROGRAM_FACT_ADD(d2d_bytes);
    PROGRAM_FACT_ADD(kernel_launches); PROGRAM_FACT_ADD(upload_count); PROGRAM_FACT_ADD(download_count);
    PROGRAM_FACT_ADD(queue_synchronizations); PROGRAM_FACT_ADD(device_synchronizations);
    PROGRAM_FACT_ADD(active_weight_bytes); PROGRAM_FACT_ADD(state_bytes); PROGRAM_FACT_ADD(activation_bytes);
    PROGRAM_FACT_ADD(temporary_bytes); PROGRAM_FACT_ADD(accelerated_matrix_launches);
#undef PROGRAM_FACT_ADD
    a->compulsory_memory_facts_available &= b->compulsory_memory_facts_available;
    return 1;
}

int yvex_program_device_run(yvex_program_device *c, unsigned long long rows,
    const yvex_program_device_argument *args, size_t argument_count,
    yvex_device_tensor *const *outputs, size_t output_count,
    int (*cancel)(void *), void *cancel_context, yvex_program_device_result *result, yvex_error *err)
{
    size_t i, j, output = 0u;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!c || !args || !result || (output_count && !outputs) || argument_count != c->summary->input_count ||
        rows < c->summary->minimum_rows || rows > c->capacity || rows % c->summary->row_multiple)
        return program_device_refuse(err, YVEX_ERR_BOUNDS, "invocation does not fit the admitted program envelope");
    rc = program_device_arguments(c, rows, args, outputs, output_count, err);
    if (rc != YVEX_OK) return rc;
    for (i = 0u; i < output_count; ++i) outputs[i]->is_written = 0;
    result->backend.compulsory_memory_facts_available = 1;
    result->host_bytes = c->host_bytes;
    result->device_bytes = c->device_bytes;
    for (i = 0u; rc == YVEX_OK && i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        yvex_program_device_invocation r = {c->program, s, args, c->values, rows, i};
        yvex_backend_operation_facts facts = {0};
        if (!c->kernels[i].invoke) continue; /* Immutable parameter binding, no device work. */
        if (cancel && cancel(cancel_context)) {
            rc = program_device_refuse(err, YVEX_ERR_CANCELLED, "program invocation cancelled before operation");
            break;
        }
        for (j = 0u; j < s->operand_count; ++j) {
            const yvex_program_physical_value *v = yvex_program_physical_value_at(c->program, s->operands[j]);
            if (!v->parameter && v->type.kind == YVEX_IR_TENSOR && v->type.scalar != YVEX_IR_INDEX &&
                !c->values[s->operands[j]].is_written)
                rc = program_device_refuse(err, YVEX_ERR_STATE, "physical operand has not been produced");
        }
        if (rc == YVEX_OK) rc = c->kernels[i].invoke(c->kernel_context, &r, &facts, err);
        if (rc != YVEX_OK) break;
        for (j = 0u; j < s->result_count; ++j) {
            const yvex_ir_type *t = &yvex_program_physical_value_at(c->program, s->results[j])->type;
            if (t->kind == YVEX_IR_TENSOR && !c->values[s->results[j]].is_written)
                rc = program_device_refuse(err, YVEX_ERR_STATE, "physical implementation did not produce its result");
        }
        result->operations++;
        if (!program_device_facts(&result->backend, &facts))
            rc = program_device_refuse(err, YVEX_ERR_BOUNDS, "program operation counters overflowed");
    }
    /* Cancellation may become visible during the final kernel. Check the
     * invocation boundary before returning publishable outputs to its runner. */
    if (rc == YVEX_OK && cancel && cancel(cancel_context))
        rc = program_device_refuse(err, YVEX_ERR_CANCELLED, "program invocation cancelled before publication");
    if (rc == YVEX_OK) for (i = 0u; i < c->summary->result_count; ++i) {
        yvex_ir_id slot = yvex_program_physical_result_at(c->program, i);
        if (yvex_program_physical_value_at(c->program, slot)->type.kind == YVEX_IR_STATE) continue;
        outputs[output++]->is_written = c->values[slot].is_written;
    }
    return rc;
}

void yvex_program_device_resources(const yvex_program_device *c, unsigned long long *host, unsigned long long *device)
{
    if (host) *host = c ? c->host_bytes : 0u;
    if (device) *device = c ? c->device_bytes : 0u;
}

int yvex_program_device_close(yvex_program_device **out, yvex_error *err)
{
    yvex_program_device *c = out ? *out : NULL;
    size_t i;
    int rc;
    if (!c) return YVEX_OK;
    for (i = 0u; c->storage && i < c->summary->storage_count; ++i) if (c->storage[i]) {
        rc = yvex_backend_tensor_release(c->backend, &c->storage[i], err);
        if (rc != YVEX_OK) return rc;
    }
    free(c->values);
    free(c->storage);
    free(c->kernels);
    free(c);
    *out = NULL;
    return YVEX_OK;
}
