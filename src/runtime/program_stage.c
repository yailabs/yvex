/* Executable tensor-stage resources and publication, independent of family and runner. */
#include <yvex/internal/program_stage.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct yvex_program_stage {
    const yvex_program_physical *program;
    yvex_backend *backend;
    yvex_program_kernels *kernels;
    yvex_program_device *vm;
    yvex_device_tensor **inputs, *input_views, *outputs[YVEX_PROGRAM_RESULT_CAP];
    yvex_program_device_argument *arguments;
    unsigned char *publication;
    unsigned long long capacity, host_bytes, device_bytes;
    size_t input_count, result_count;
    int ready;
};

static int stage_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "runtime.program-stage", reason);
    return status;
}

static int stage_geometry(const yvex_ir_type *t, unsigned long long rows, yvex_backend_tensor_desc *d)
{
    unsigned int i;
    unsigned long long count = 1u;
    *d = (yvex_backend_tensor_desc){.name = "program-stage", .dtype = YVEX_DTYPE_F32, .rank = t->rank};
    if (t->kind != YVEX_IR_TENSOR || (t->scalar != YVEX_IR_F32 && t->scalar != YVEX_IR_BF16) ||
        !t->rank || t->rank > YVEX_TENSOR_MAX_DIMS) return 0;
    for (i = 0u; i < t->rank; ++i) {
        d->dims[i] = t->shape[i].symbol == YVEX_IR_NONE ? t->shape[i].extent : rows;
        if (!yvex_core_u64_mul(count, d->dims[i], &count)) return 0;
    }
    return yvex_core_u64_mul(count, sizeof(float), &d->bytes);
}

static int stage_invoke(void *opaque, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_program_stage *c = opaque;
    return yvex_program_kernels_invoke(c->kernels, r, facts, err);
}

static int stage_buffer(yvex_program_stage *c, yvex_device_tensor **out, yvex_ir_id value, yvex_error *err)
{
    yvex_backend_tensor_desc d;
    if (!stage_geometry(&yvex_program_physical_value_at(c->program, value)->type, c->capacity, &d))
        return stage_refuse(err, YVEX_ERR_UNSUPPORTED, "stage requires tensor-only physical input/results");
    int rc = yvex_backend_tensor_alloc(c->backend, &d, out, err);
    if (rc == YVEX_OK && !yvex_core_u64_add(c->device_bytes, d.bytes, &c->device_bytes))
        rc = stage_refuse(err, YVEX_ERR_BOUNDS, "stage buffer accounting overflowed");
    return rc;
}

int yvex_program_stage_open(yvex_program_stage **out, const yvex_program_physical *program,
    const yvex_program_kernel_parameter *parameters, size_t count, yvex_backend *backend,
    unsigned long long capacity, int host_io, unsigned long long host_limit,
    unsigned long long device_limit, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(program);
    yvex_program_device_kernel *implementations;
    yvex_program_stage *c;
    yvex_backend_tensor_desc d;
    size_t i, j, n = 0u;
    unsigned long long kh, kd, vh, vd, bytes = 0u, publication = 0u, host_bytes, input_bytes;
    int rc;
    if (out) *out = NULL;
    if (!out || !s || !s->input_count || !s->result_count || s->result_count > YVEX_PROGRAM_RESULT_CAP ||
        !backend || !capacity || capacity > s->maximum_rows)
        return stage_refuse(err, YVEX_ERR_FORMAT, "verified tensor stage and backend required");
    for (i = 0u; i < s->input_count; ++i) {
        if (!stage_geometry(&yvex_program_physical_value_at(program, i)->type, capacity, &d) ||
            (host_io && !yvex_core_u64_add(bytes, d.bytes, &bytes)))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "tensor stage input extent is incompatible");
    }
    for (i = 0u; i < s->result_count; ++i) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(program,
            yvex_program_physical_result_at(program, i))->type;
        if (!stage_geometry(t, capacity, &d) || (host_io &&
            (!yvex_core_u64_add(bytes, d.bytes, &bytes) || !yvex_core_u64_add(publication, d.bytes, &publication))))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "tensor stage result extent overflowed");
    }
    if (!yvex_core_u64_mul(s->input_count,
            sizeof(*c->inputs) + sizeof(*c->input_views) + sizeof(*c->arguments), &input_bytes) ||
        input_bytes > SIZE_MAX || publication > SIZE_MAX ||
        !yvex_core_u64_add(sizeof(*c), publication, &host_bytes) ||
        !yvex_core_u64_add(host_bytes, input_bytes, &host_bytes) ||
        (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU &&
            !yvex_core_u64_add(host_bytes, bytes, &host_bytes)) ||
        (device_limit && bytes > device_limit) || (host_limit && host_bytes > host_limit))
        return stage_refuse(err, YVEX_ERR_BOUNDS, "stage staging exceeds resource budget");
    c = calloc(1u, sizeof(*c));
    implementations = calloc(s->step_count, sizeof(*implementations));
    if (!c || !implementations) {
        free(c);
        free(implementations);
        return stage_refuse(err, YVEX_ERR_NOMEM, "stage allocation failed");
    }
    c->program = program; c->backend = backend; c->capacity = capacity;
    c->input_count = s->input_count; c->result_count = s->result_count;
    c->inputs = calloc(c->input_count, sizeof(*c->inputs));
    c->input_views = calloc(c->input_count, sizeof(*c->input_views));
    c->arguments = calloc(c->input_count, sizeof(*c->arguments));
    if (!c->inputs || !c->input_views || !c->arguments) {
        free(implementations);
        (void)yvex_program_stage_close(&c, NULL);
        return stage_refuse(err, YVEX_ERR_NOMEM, "stage input owners could not be allocated");
    }
    for (i = 0u; i < s->step_count; ++i) {
        const char *name = yvex_program_physical_step_at(program, i)->implementation;
        if (!strcmp(name, "parameter.encoded.v1")) continue;
        for (j = 0u; j < n && strcmp(implementations[j].implementation, name); ++j) {}
        if (j == n) implementations[n++] = (yvex_program_device_kernel){name, stage_invoke};
    }
    rc = yvex_program_kernels_open(&c->kernels, program, parameters, count, backend, host_limit, device_limit, err);
    if (rc == YVEX_OK) rc = yvex_program_device_open(&c->vm, program, backend, capacity,
        host_limit, device_limit, implementations, n, c, err);
    free(implementations);
    if (rc == YVEX_OK && publication) {
        c->publication = malloc((size_t)publication);
        if (!c->publication) rc = stage_refuse(err, YVEX_ERR_NOMEM, "stage publication allocation failed");
    }
    for (i = 0u; rc == YVEX_OK && host_io && i < c->input_count; ++i)
        rc = stage_buffer(c, &c->inputs[i], (yvex_ir_id)i, err);
    for (i = 0u; rc == YVEX_OK && host_io && i < c->result_count; ++i)
        rc = stage_buffer(c, &c->outputs[i], yvex_program_physical_result_at(program, i), err);
    if (rc == YVEX_OK) {
        yvex_program_kernels_resources(c->kernels, &kh, &kd);
        yvex_program_device_resources(c->vm, &vh, &vd);
        if (!yvex_core_u64_add(sizeof(*c), publication, &c->host_bytes) ||
            !yvex_core_u64_add(c->host_bytes, input_bytes, &c->host_bytes) ||
            !yvex_core_u64_add(c->host_bytes, kh, &c->host_bytes) ||
            !yvex_core_u64_add(c->host_bytes, vh, &c->host_bytes) ||
            !yvex_core_u64_add(c->device_bytes, kd, &c->device_bytes) ||
            !yvex_core_u64_add(c->device_bytes, vd, &c->device_bytes))
            rc = stage_refuse(err, YVEX_ERR_BOUNDS, "aggregate stage resources overflowed");
        if (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU) {
            if (!yvex_core_u64_add(c->host_bytes, c->device_bytes, &c->host_bytes))
                rc = stage_refuse(err, YVEX_ERR_BOUNDS, "aggregate CPU stage resources overflowed");
            c->device_bytes = 0u;
        }
        if ((host_limit && c->host_bytes > host_limit) || (device_limit && c->device_bytes > device_limit))
            rc = stage_refuse(err, YVEX_ERR_BOUNDS, "aggregate stage resources exceed budget");
    }
    if (rc != YVEX_OK) (void)yvex_program_stage_close(&c, NULL);
    else c->ready = 1;
    *out = c;
    return rc;
}

int yvex_program_stage_device(yvex_program_stage *c, unsigned long long rows,
    const yvex_device_tensor *const *inputs, size_t input_count,
    yvex_device_tensor *const *outputs, size_t count,
    int (*cancel)(void *), void *context, yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_program_device_result result;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!c || !c->ready || !facts || !inputs || input_count != c->input_count)
        return stage_refuse(err, YVEX_ERR_INVALID_ARG, "prepared stage and result owner required");
    for (size_t i = 0u; i < input_count; ++i)
        c->arguments[i] = (yvex_program_device_argument){.tensor = inputs[i]};
    int rc = yvex_program_device_run(c->vm, rows, c->arguments, input_count,
        outputs, count, cancel, context, &result, err);
    if (rc == YVEX_OK) *facts = result.backend;
    return rc;
}

int yvex_program_stage_host(yvex_program_stage *c, unsigned long long rows,
    const float *const *inputs, size_t input_count,
    float *const *outputs, size_t count, int (*cancel)(void *), void *context,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_backend_tensor_desc d;
    unsigned long long sizes[YVEX_PROGRAM_RESULT_CAP], offset = 0u;
    yvex_device_tensor output_view;
    yvex_program_device_result result;
    size_t i, j;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!c || !c->ready || !inputs || input_count != c->input_count ||
        !outputs || count != c->result_count || !facts ||
        !rows || rows > c->capacity)
        return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host stage arguments incompatible");
    for (i = 0u; i < input_count; ++i)
        if (!inputs[i] || !c->inputs[i])
            return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host input and staging required");
    for (i = 0u; i < count; ++i) {
        if (!outputs[i]) return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host result destination required");
        const yvex_ir_type *t = &yvex_program_physical_value_at(c->program,
            yvex_program_physical_result_at(c->program, i))->type;
        if (!stage_geometry(t, rows, &d))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "host result geometry overflowed");
        sizes[i] = d.bytes;
        uintptr_t address = (uintptr_t)outputs[i];
        if (sizes[i] > UINTPTR_MAX - address)
            return stage_refuse(err, YVEX_ERR_BOUNDS, "host result address overflowed");
        for (j = 0u; j < i; ++j)
            if (!(address + sizes[i] <= (uintptr_t)outputs[j] || (uintptr_t)outputs[j] + sizes[j] <= address))
                return stage_refuse(err, YVEX_ERR_FORMAT, "host results alias");
    }
    rc = YVEX_OK;
    for (i = 0u; rc == YVEX_OK && i < input_count; ++i) {
        yvex_device_tensor *view = &c->input_views[i];
        if (!stage_geometry(&yvex_program_physical_value_at(c->program, i)->type, rows, &d) ||
            !yvex_backend_tensor_f32_subview(c->inputs[i], 0u, d.bytes / sizeof(float), view))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "host input geometry or storage is insufficient");
        view->rank = d.rank;
        memcpy(view->dims, d.dims, sizeof(d.dims));
        rc = yvex_backend_tensor_write(c->backend, view, inputs[i], d.bytes, err);
        c->arguments[i] = (yvex_program_device_argument){.tensor = view};
    }
    if (rc == YVEX_OK) {
        rc = yvex_program_device_run(c->vm, rows, c->arguments, input_count,
            c->outputs, count, cancel, context, &result, err);
        if (rc == YVEX_OK) *facts = result.backend;
    }
    for (i = 0u; rc == YVEX_OK && i < count; ++i) {
        if (!yvex_backend_tensor_f32_subview(c->outputs[i], 0u, sizes[i] / sizeof(float), &output_view))
            rc = stage_refuse(err, YVEX_ERR_BOUNDS, "host result storage is insufficient");
        if (rc == YVEX_OK) rc = yvex_backend_tensor_read(c->backend, &output_view,
            c->publication + offset, sizes[i], err);
        offset += sizes[i];
    }
    if (rc == YVEX_OK && cancel && cancel(context))
        rc = stage_refuse(err, YVEX_ERR_CANCELLED, "host results cancelled before publication");
    offset = 0u;
    for (i = 0u; rc == YVEX_OK && i < count; ++i) {
        memcpy(outputs[i], c->publication + offset, (size_t)sizes[i]);
        offset += sizes[i];
    }
    if (rc != YVEX_OK) memset(facts, 0, sizeof(*facts));
    return rc;
}

void yvex_program_stage_resources(const yvex_program_stage *c, unsigned long long *host, unsigned long long *device)
{
    if (host) *host = c ? c->host_bytes : 0u;
    if (device) *device = c ? c->device_bytes : 0u;
}

int yvex_program_stage_close(yvex_program_stage **out, yvex_error *err)
{
    yvex_program_stage *c = out ? *out : NULL;
    int rc;
    size_t i;
    if (!c) return YVEX_OK;
    rc = yvex_program_device_close(&c->vm, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_close(&c->kernels, err);
    for (i = 0u; rc == YVEX_OK && c->inputs && i < c->input_count; ++i)
        if (c->inputs[i]) rc = yvex_backend_tensor_release(c->backend, &c->inputs[i], err);
    for (i = 0u; rc == YVEX_OK && i < c->result_count; ++i)
        if (c->outputs[i]) rc = yvex_backend_tensor_release(c->backend, &c->outputs[i], err);
    if (rc != YVEX_OK) return rc;
    free(c->publication);
    free(c->arguments); free(c->input_views); free(c->inputs);
    free(c); *out = NULL;
    return YVEX_OK;
}
