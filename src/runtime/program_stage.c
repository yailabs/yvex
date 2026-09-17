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
    yvex_device_tensor **inputs, *input_views, **outputs;
    unsigned long long *output_sizes;
    yvex_program_device_argument *arguments;
    yvex_program_host_input *host_inputs;
    unsigned char *publication;
    float *observation;
    unsigned long long observation_bytes;
    yvex_program_observer observer;
    unsigned long long capacity, host_bytes, device_bytes;
    unsigned long long host_limit, device_limit;
    size_t input_count, result_count;
    int ready;
};

static int stage_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "runtime.program-stage", reason);
    return status;
}

static int stage_index(const yvex_ir_type *t)
{
    return t->kind == YVEX_IR_TENSOR && t->scalar == YVEX_IR_INDEX && t->rank == 1u;
}

static int stage_invoke(void *opaque, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_program_stage *c = opaque;
    if (!strcmp(r->step->implementation, "observe.f32-storage.v1")) {
        const yvex_device_tensor *value = &r->values[r->step->operands[0]];
        const yvex_ir_type *type = &yvex_program_physical_value_at(c->program, r->step->operands[0])->type;
        if (!c->observer.publish || !value->bytes || value->bytes > c->observation_bytes)
            return stage_refuse(err, YVEX_ERR_STATE, "compiled observation has no compatible publication owner");
        int rc = yvex_backend_tensor_read(c->backend, value, c->observation, value->bytes, err);
        if (rc == YVEX_OK) rc = c->observer.publish(c->observer.context,
            yvex_program_physical_attribute(r->step, "tag")->value.integer, type,
            r->rows, c->observation, value->bytes / sizeof(float), err);
        if (rc == YVEX_OK && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU) {
            facts->download_count = 1u; facts->d2h_bytes = value->bytes;
        }
        return rc;
    }
    return yvex_program_kernels_invoke(c->kernels, r, facts, err);
}

int yvex_program_stage_observe(yvex_program_stage *c, const yvex_program_observer *observer, yvex_error *err)
{
    if (!c || !c->ready || (observer && !observer->publish))
        return stage_refuse(err, YVEX_ERR_INVALID_ARG, "ready stage and valid observation callback required");
    c->observer = observer ? *observer : (yvex_program_observer){0};
    return YVEX_OK;
}

static int stage_buffer(yvex_program_stage *c, yvex_device_tensor **out, yvex_ir_id value, yvex_error *err)
{
    yvex_backend_tensor_desc d;
    if (yvex_program_device_descriptor(c->program, value, c->capacity, &d, err) != YVEX_OK)
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
    unsigned long long kh, kd, vh, vd, bytes = 0u, publication = 0u, observation = 0u;
    unsigned long long host_bytes, input_bytes, output_bytes;
    int rc;
    if (out) *out = NULL;
    if (!out || !s || !s->input_count || !s->result_count ||
        !backend || !capacity || capacity > s->maximum_rows)
        return stage_refuse(err, YVEX_ERR_FORMAT, "verified tensor stage and backend required");
    for (i = 0u; i < s->input_count; ++i) {
        if (stage_index(&yvex_program_physical_value_at(program, i)->type)) continue;
        if (yvex_program_device_descriptor(program, i, capacity, &d, err) != YVEX_OK ||
            (host_io && !yvex_core_u64_add(bytes, d.bytes, &bytes)))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "tensor stage input extent is incompatible");
    }
    for (i = 0u; i < s->result_count; ++i) {
        if (yvex_program_device_descriptor(program, yvex_program_physical_result_at(program, i),
            capacity, &d, err) != YVEX_OK || (host_io &&
            (!yvex_core_u64_add(bytes, d.bytes, &bytes) || !yvex_core_u64_add(publication, d.bytes, &publication))))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "tensor stage result extent overflowed");
    }
    for (i = 0u; i < s->step_count; ++i) {
        const yvex_program_physical_step *step = yvex_program_physical_step_at(program, i);
        if (strcmp(step->implementation, "observe.f32-storage.v1")) continue;
        if (yvex_program_device_descriptor(program, step->operands[0], capacity, &d, err) != YVEX_OK)
            return stage_refuse(err, YVEX_ERR_BOUNDS, "observation geometry overflowed");
        if (d.bytes > observation) observation = d.bytes;
    }
    if (!yvex_core_u64_mul(s->input_count,
            sizeof(*c->inputs) + sizeof(*c->input_views) + sizeof(*c->arguments) + sizeof(*c->host_inputs),
            &input_bytes) ||
        !yvex_core_u64_mul(s->result_count, sizeof(*c->outputs) + sizeof(*c->output_sizes), &output_bytes) ||
        !yvex_core_u64_add(input_bytes, output_bytes, &input_bytes) ||
        input_bytes > SIZE_MAX || publication > SIZE_MAX || observation > SIZE_MAX ||
        !yvex_core_u64_add(sizeof(*c), publication, &host_bytes) ||
        !yvex_core_u64_add(host_bytes, observation, &host_bytes) ||
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
    c->host_limit = host_limit; c->device_limit = device_limit;
    c->observation_bytes = observation;
    c->input_count = s->input_count; c->result_count = s->result_count;
    c->inputs = calloc(c->input_count, sizeof(*c->inputs));
    c->input_views = calloc(c->input_count, sizeof(*c->input_views));
    c->arguments = calloc(c->input_count, sizeof(*c->arguments));
    c->host_inputs = calloc(c->input_count, sizeof(*c->host_inputs));
    c->outputs = calloc(c->result_count, sizeof(*c->outputs));
    c->output_sizes = calloc(c->result_count, sizeof(*c->output_sizes));
    if (!c->inputs || !c->input_views || !c->arguments || !c->host_inputs || !c->outputs || !c->output_sizes) {
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
    if (rc == YVEX_OK && observation) {
        c->observation = malloc((size_t)observation);
        if (!c->observation) rc = stage_refuse(err, YVEX_ERR_NOMEM, "observation staging allocation failed");
    }
    for (i = 0u; rc == YVEX_OK && host_io && i < c->input_count; ++i)
        if (!stage_index(&yvex_program_physical_value_at(program, i)->type))
            rc = stage_buffer(c, &c->inputs[i], (yvex_ir_id)i, err);
    for (i = 0u; rc == YVEX_OK && host_io && i < c->result_count; ++i)
        rc = stage_buffer(c, &c->outputs[i], yvex_program_physical_result_at(program, i), err);
    if (rc == YVEX_OK) {
        yvex_program_kernels_resources(c->kernels, &kh, &kd);
        yvex_program_device_resources(c->vm, &vh, &vd);
        if (!yvex_core_u64_add(sizeof(*c), publication, &c->host_bytes) ||
            !yvex_core_u64_add(c->host_bytes, observation, &c->host_bytes) ||
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

int yvex_program_stage_prepare(yvex_program_stage *c, unsigned long long rows, yvex_error *err)
{
    if (!c || !c->ready || !rows || rows > c->capacity)
        return stage_refuse(err, YVEX_ERR_BOUNDS, "stage preparation requires an admitted population");
    if (c->observation_bytes && !c->observer.publish)
        return stage_refuse(err, YVEX_ERR_STATE, "inspection program requires an observation owner before execution");
    unsigned long long kh, kd, base_host, base_device, host_limit = 0u, device_limit = 0u;
    yvex_program_kernels_resources(c->kernels, &kh, &kd);
    int cpu = yvex_backend_kind_of(c->backend) == YVEX_BACKEND_KIND_CPU;
    if (cpu) {
        if (!yvex_core_u64_add(kh, kd, &kh))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "CPU operation resource accounting overflowed");
        kd = 0u;
    }
    if (kh > c->host_bytes || kd > c->device_bytes)
        return stage_refuse(err, YVEX_ERR_STATE, "stage operation resource accounting is inconsistent");
    base_host = c->host_bytes - kh; base_device = c->device_bytes - kd;
    if ((c->host_limit && base_host >= c->host_limit) || (c->device_limit && base_device >= c->device_limit))
        return stage_refuse(err, YVEX_ERR_BOUNDS, "stage has no remaining preparation budget");
    if (c->host_limit) host_limit = c->host_limit - base_host;
    if (c->device_limit) device_limit = c->device_limit - base_device;
    int rc = yvex_program_kernels_prepare(c->kernels, rows, host_limit, device_limit, err);
    yvex_program_kernels_resources(c->kernels, &kh, &kd);
    if (cpu) {
        if (!yvex_core_u64_add(kh, kd, &kh))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "prepared CPU operation resource accounting overflowed");
        kd = 0u;
    }
    if (!yvex_core_u64_add(base_host, kh, &c->host_bytes) ||
        !yvex_core_u64_add(base_device, kd, &c->device_bytes) ||
        (c->host_limit && c->host_bytes > c->host_limit) ||
        (c->device_limit && c->device_bytes > c->device_limit))
        return stage_refuse(err, YVEX_ERR_BOUNDS, "stage prepared resources exceed aggregate budget");
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
    int rc = yvex_program_stage_prepare(c, rows, err);
    if (rc == YVEX_OK) rc = yvex_program_device_run(c->vm, rows, c->arguments, input_count,
        outputs, count, cancel, context, &result, err);
    if (rc == YVEX_OK) *facts = result.backend;
    return rc;
}

int yvex_program_stage_host_inputs(yvex_program_stage *c, unsigned long long rows,
    const yvex_program_host_input *inputs, size_t input_count,
    float *const *outputs, size_t count, int (*cancel)(void *), void *context,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_backend_tensor_desc d;
    unsigned long long *sizes, offset = 0u;
    unsigned long long uploaded = 0u, downloaded = 0u, uploads = 0u;
    yvex_device_tensor output_view;
    yvex_program_device_result result;
    size_t i, j;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!c || !c->ready || !inputs || input_count != c->input_count ||
        !outputs || count != c->result_count || !facts ||
        !rows || rows > c->capacity)
        return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host stage arguments incompatible");
    sizes = c->output_sizes;
    for (i = 0u; i < input_count; ++i) {
        int index = stage_index(&yvex_program_physical_value_at(c->program, i)->type);
        if (index ? !inputs[i].indices || inputs[i].values :
            !inputs[i].values || inputs[i].indices || !c->inputs[i])
            return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host input representation differs from program signature");
    }
    for (i = 0u; i < count; ++i) {
        if (!outputs[i]) return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host result destination required");
        if (yvex_program_device_descriptor(c->program, yvex_program_physical_result_at(c->program, i),
            rows, &d, err) != YVEX_OK)
            return stage_refuse(err, YVEX_ERR_BOUNDS, "host result geometry overflowed");
        sizes[i] = d.bytes;
        uintptr_t address = (uintptr_t)outputs[i];
        if (sizes[i] > UINTPTR_MAX - address)
            return stage_refuse(err, YVEX_ERR_BOUNDS, "host result address overflowed");
        for (j = 0u; j < i; ++j)
            if (!(address + sizes[i] <= (uintptr_t)outputs[j] || (uintptr_t)outputs[j] + sizes[j] <= address))
                return stage_refuse(err, YVEX_ERR_FORMAT, "host results alias");
    }
    rc = yvex_program_stage_prepare(c, rows, err);
    for (i = 0u; rc == YVEX_OK && i < input_count; ++i) {
        if (inputs[i].indices) {
            c->arguments[i] = (yvex_program_device_argument){.indices = inputs[i].indices};
            continue;
        }
        yvex_device_tensor *view = &c->input_views[i];
        if (yvex_program_device_descriptor(c->program, i, rows, &d, err) != YVEX_OK ||
            !yvex_backend_tensor_f32_subview(c->inputs[i], 0u, d.bytes / sizeof(float), view))
            return stage_refuse(err, YVEX_ERR_BOUNDS, "host input geometry or storage is insufficient");
        view->rank = d.rank;
        memcpy(view->dims, d.dims, sizeof(d.dims));
        rc = yvex_backend_tensor_write(c->backend, view, inputs[i].values, d.bytes, err);
        if (rc == YVEX_OK && !yvex_core_u64_add(uploaded, d.bytes, &uploaded))
            rc = stage_refuse(err, YVEX_ERR_BOUNDS, "host input transfer accounting overflowed");
        if (rc == YVEX_OK) uploads++;
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
        if (rc == YVEX_OK && !yvex_core_u64_add(downloaded, sizes[i], &downloaded))
            rc = stage_refuse(err, YVEX_ERR_BOUNDS, "host result transfer accounting overflowed");
        offset += sizes[i];
    }
    if (rc == YVEX_OK && yvex_backend_kind_of(c->backend) != YVEX_BACKEND_KIND_CPU &&
        (!yvex_core_u64_add(facts->h2d_bytes, uploaded, &facts->h2d_bytes) ||
         !yvex_core_u64_add(facts->d2h_bytes, downloaded, &facts->d2h_bytes) ||
         !yvex_core_u64_add(facts->upload_count, uploads, &facts->upload_count) ||
         !yvex_core_u64_add(facts->download_count, count, &facts->download_count)))
        rc = stage_refuse(err, YVEX_ERR_BOUNDS, "stage transfer accounting overflowed");
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

int yvex_program_stage_host(yvex_program_stage *c, unsigned long long rows,
    const float *const *inputs, size_t input_count, float *const *outputs, size_t count,
    int (*cancel)(void *), void *context, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!c || !inputs || input_count != c->input_count) {
        if (facts) memset(facts, 0, sizeof(*facts));
        return stage_refuse(err, YVEX_ERR_INVALID_ARG, "host stage input count is incompatible");
    }
    for (size_t i = 0u; i < input_count; ++i)
        c->host_inputs[i] = (yvex_program_host_input){.values = inputs[i]};
    return yvex_program_stage_host_inputs(c, rows, c->host_inputs, input_count,
        outputs, count, cancel, context, facts, err);
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
    for (i = 0u; rc == YVEX_OK && c->outputs && i < c->result_count; ++i)
        if (c->outputs[i]) rc = yvex_backend_tensor_release(c->backend, &c->outputs[i], err);
    if (rc != YVEX_OK) return rc;
    free(c->publication);
    free(c->observation);
    free(c->arguments); free(c->input_views); free(c->inputs);
    free(c->host_inputs);
    free(c->outputs); free(c->output_sizes);
    free(c); *out = NULL;
    return YVEX_OK;
}

struct yvex_program_prepared {
    yvex_program_stage *preparation, *step;
    yvex_program_host_input *inputs;
    float **outputs, *retained;
    size_t *links;
    size_t input_count, output_count, link_count;
    unsigned long long rows, host_bytes, base_host_bytes, device_bytes, retained_bytes;
    int ready;
};

int yvex_program_prepared_close(yvex_program_prepared **out, yvex_error *err)
{
    yvex_program_prepared *c = out ? *out : NULL;
    if (!c) return YVEX_OK;
    int rc = yvex_program_stage_close(&c->step, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_close(&c->preparation, err);
    if (rc != YVEX_OK) return rc;
    free(c->inputs); free(c->outputs); free(c->retained); free(c->links);
    free(c); *out = NULL;
    return YVEX_OK;
}

static int prepared_geometry(const yvex_program_physical *preparation, const yvex_program_physical *step,
    unsigned long long preparation_rows, unsigned long long step_rows, const size_t *links, size_t count,
    unsigned long long *total, yvex_error *err)
{
    const yvex_program_physical_summary *a = yvex_program_physical_summary_get(preparation);
    const yvex_program_physical_summary *b = yvex_program_physical_summary_get(step);
    *total = 0u;
    if (!a || !b || !links || !count || count != a->result_count || count > b->input_count ||
        preparation_rows < a->minimum_rows || preparation_rows > a->maximum_rows ||
        preparation_rows % a->row_multiple || step_rows < b->minimum_rows || step_rows > b->maximum_rows ||
        step_rows % b->row_multiple)
        return stage_refuse(err, YVEX_ERR_FORMAT, "prepared entrypoint populations or links are incompatible");
    for (size_t i = 0u; i < count; ++i) {
        yvex_backend_tensor_desc source, target;
        const yvex_ir_type *from = &yvex_program_physical_value_at(preparation,
            yvex_program_physical_result_at(preparation, i))->type;
        if (links[i] >= b->input_count)
            return stage_refuse(err, YVEX_ERR_BOUNDS, "prepared link does not name a step input");
        const yvex_ir_type *to = &yvex_program_physical_value_at(step, links[i])->type;
        if (from->scalar != to->scalar || yvex_program_device_descriptor(preparation,
            yvex_program_physical_result_at(preparation, i), preparation_rows, &source, err) != YVEX_OK ||
            yvex_program_device_descriptor(step, links[i], step_rows, &target, err) != YVEX_OK ||
            source.rank != target.rank || source.bytes != target.bytes ||
            memcmp(source.dims, target.dims, sizeof(source.dims)))
            return stage_refuse(err, YVEX_ERR_FORMAT, "prepared result type/geometry differs from linked input");
        for (size_t j = 0u; j < i; ++j) if (links[j] == links[i])
            return stage_refuse(err, YVEX_ERR_FORMAT, "prepared results cannot overwrite the same input");
        if (!yvex_core_u64_add(*total, source.bytes, total) || *total > SIZE_MAX)
            return stage_refuse(err, YVEX_ERR_BOUNDS, "retained result storage overflowed");
    }
    return YVEX_OK;
}

int yvex_program_prepared_open(yvex_program_prepared **out,
    const yvex_program_physical *preparation, const yvex_program_physical *step,
    const yvex_program_kernel_parameter *parameters, size_t parameter_count, yvex_backend *backend,
    unsigned long long preparation_rows, unsigned long long step_rows,
    const yvex_program_host_input *inputs, const size_t *links, size_t count,
    unsigned long long host_limit, unsigned long long device_limit,
    const yvex_program_observer *observer, yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long bytes = 0u, base, directories, offset = 0u, sh = 0u, sd = 0u;
    if (out) *out = NULL;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!out || !inputs || !facts || !backend)
        return stage_refuse(err, YVEX_ERR_INVALID_ARG, "prepared program requires an owner and typed inputs");
    int rc = prepared_geometry(preparation, step, preparation_rows, step_rows, links, count, &bytes, err);
    if (rc != YVEX_OK) return rc;
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(step);
    if (!yvex_core_u64_mul(s->input_count, sizeof(yvex_program_host_input), &directories) ||
        !yvex_core_u64_mul(count, sizeof(float *) + sizeof(size_t), &base) ||
        !yvex_core_u64_add(directories, base, &directories) ||
        !yvex_core_u64_add(directories, bytes, &base) ||
        !yvex_core_u64_add(base, sizeof(yvex_program_prepared), &base) || base > SIZE_MAX ||
        (host_limit && base >= host_limit))
        return stage_refuse(err, YVEX_ERR_BOUNDS, "prepared result directories exceed the host budget");
    yvex_program_prepared *c = calloc(1u, sizeof(*c));
    if (!c) return stage_refuse(err, YVEX_ERR_NOMEM, "prepared owner allocation failed");
    *out = c;
    c->rows = step_rows; c->input_count = s->input_count; c->output_count = s->result_count; c->link_count = count;
    c->retained_bytes = bytes; c->host_bytes = c->base_host_bytes = base;
    c->inputs = calloc(s->input_count, sizeof(*c->inputs));
    c->outputs = calloc(count, sizeof(*c->outputs)); c->links = calloc(count, sizeof(*c->links));
    c->retained = malloc((size_t)bytes);
    if (!c->inputs || !c->outputs || !c->links || !c->retained)
        rc = stage_refuse(err, YVEX_ERR_NOMEM, "prepared result storage allocation failed");
    if (rc == YVEX_OK) memcpy(c->links, links, count * sizeof(*links));
    for (size_t i = 0u; rc == YVEX_OK && i < count; ++i) {
        yvex_backend_tensor_desc d;
        rc = yvex_program_device_descriptor(preparation, yvex_program_physical_result_at(preparation, i),
            preparation_rows, &d, err);
        if (rc == YVEX_OK) { c->outputs[i] = c->retained + offset / sizeof(float); offset += d.bytes; }
    }
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&c->preparation, preparation, parameters, parameter_count,
        backend, preparation_rows, 1, host_limit ? host_limit - base : SIZE_MAX - base, device_limit, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_observe(c->preparation, observer, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_host_inputs(c->preparation, preparation_rows, inputs,
        yvex_program_physical_summary_get(preparation)->input_count, c->outputs, count, NULL, NULL, facts, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_close(&c->preparation, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&c->step, step, parameters, parameter_count, backend,
        step_rows, 1, host_limit ? host_limit - base : SIZE_MAX - base, device_limit, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_observe(c->step, observer, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_prepare(c->step, step_rows, err);
    if (c->step) (void)yvex_program_stage_observe(c->step, NULL, NULL);
    yvex_program_stage_resources(c->step, &sh, &sd);
    if (rc == YVEX_OK && (!yvex_core_u64_add(base, sh, &c->host_bytes) ||
        (host_limit && c->host_bytes > host_limit)))
        rc = stage_refuse(err, YVEX_ERR_BOUNDS, "prepared aggregate storage exceeds host budget");
    c->device_bytes = sd;
    if (rc == YVEX_OK) c->ready = 1;
    else {
        yvex_error cleanup;
        int closed = yvex_program_prepared_close(out, &cleanup);
        if (closed != YVEX_OK) { rc = closed; if (err) *err = cleanup; }
        memset(facts, 0, sizeof(*facts));
    }
    return rc;
}

int yvex_program_prepared_run(yvex_program_prepared *c, const yvex_program_host_input *inputs, size_t count,
    float *const *outputs, size_t output_count, int (*cancel)(void *), void *context,
    const yvex_program_observer *observer, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!c || !c->ready || !inputs || count != c->input_count || output_count != c->output_count || !facts)
        return stage_refuse(err, YVEX_ERR_STATE, "prepared invocation does not match a ready owner");
    for (size_t i = 0u; i < c->link_count; ++i)
        if (inputs[c->links[i]].values || inputs[c->links[i]].indices)
            return stage_refuse(err, YVEX_ERR_FORMAT, "caller cannot substitute an identity-bound prepared input");
    memcpy(c->inputs, inputs, count * sizeof(*inputs));
    for (size_t i = 0u; i < c->link_count; ++i)
        c->inputs[c->links[i]] = (yvex_program_host_input){.values = c->outputs[i]};
    int rc = yvex_program_stage_observe(c->step, observer, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_host_inputs(c->step, c->rows, c->inputs, count, outputs, output_count,
        cancel, context, facts, err);
    (void)yvex_program_stage_observe(c->step, NULL, NULL);
    unsigned long long sh, sd;
    yvex_program_stage_resources(c->step, &sh, &sd);
    /* Stage admission reserves the remaining SIZE_MAX-bounded host envelope;
     * its preparation checks that budget before execution/publication. */
    c->host_bytes = c->base_host_bytes + sh;
    c->device_bytes = sd;
    return rc;
}

void yvex_program_prepared_resources(const yvex_program_prepared *c, unsigned long long *host,
    unsigned long long *device, unsigned long long *retained)
{
    if (host) *host = c ? c->host_bytes : 0u;
    if (device) *device = c ? c->device_bytes : 0u;
    if (retained) *retained = c ? c->retained_bytes : 0u;
}
