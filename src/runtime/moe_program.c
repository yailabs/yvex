/* Session-owned compiled ingress stages and call-scoped result carriers.
 * Computation and parameter references come only from the admitted programs. */
#include <yvex/internal/moe.h>
#include <yvex/internal/program_stage.h>
#include "src/runtime/private.h"
#include <stdlib.h>
#include <string.h>

struct yvex_runtime_moe_programs {
    const yvex_model_engine_view *model;
    const yvex_runtime_session_view *session;
    const yvex_engine_specialization *specialization;
    const yvex_moe_plan *plan;
    yvex_runtime_moe_options options;
    yvex_program_stage **stages;
    yvex_program_physical **targets;
    unsigned long long *parameter_bytes;
    unsigned long long count, capacity, host, device, reads;
    unsigned long long host_reserved, device_reserved;
    yvex_device_tensor *outputs[6], views[6];
    unsigned long long widths[6];
};

static int programs_refuse(yvex_error *err, yvex_status status, const char *message)
{
    yvex_error_set(err, status, "runtime.moe-program", message);
    return status;
}

static int programs_facts_add(yvex_backend_operation_facts *a,
    const yvex_backend_operation_facts *b, yvex_error *err)
{
    unsigned long long *dst[] = {&a->h2d_bytes, &a->d2h_bytes, &a->d2d_bytes, &a->kernel_launches,
        &a->upload_count, &a->download_count, &a->queue_synchronizations, &a->device_synchronizations,
        &a->active_weight_bytes, &a->state_bytes, &a->activation_bytes, &a->temporary_bytes,
        &a->accelerated_matrix_launches};
    const unsigned long long src[] = {b->h2d_bytes, b->d2h_bytes, b->d2d_bytes, b->kernel_launches,
        b->upload_count, b->download_count, b->queue_synchronizations, b->device_synchronizations,
        b->active_weight_bytes, b->state_bytes, b->activation_bytes, b->temporary_bytes,
        b->accelerated_matrix_launches};
    for (size_t i = 0u; i < sizeof(src) / sizeof(src[0]); ++i)
        if (!yvex_core_u64_add(*dst[i], src[i], dst[i]))
            return programs_refuse(err, YVEX_ERR_BOUNDS, "compiled evidence counter overflowed");
    a->compulsory_memory_facts_available &= b->compulsory_memory_facts_available;
    return YVEX_OK;
}

static int programs_budget(yvex_runtime_moe_programs *c, unsigned long long *host,
    unsigned long long *device, yvex_error *err)
{
    unsigned long long h, d;
    if (!yvex_core_u64_add(c->host_reserved, c->host, &h) ||
        !yvex_core_u64_add(c->device_reserved, c->device, &d) ||
        (c->options.maximum_host_bytes && h >= c->options.maximum_host_bytes) ||
        (c->options.maximum_device_bytes && d >= c->options.maximum_device_bytes))
        return programs_refuse(err, YVEX_ERR_BOUNDS, "compiled resources exceed the enclosing budget");
    *host = c->options.maximum_host_bytes ? c->options.maximum_host_bytes - h : 0u;
    *device = c->options.maximum_device_bytes ? c->options.maximum_device_bytes - d : 0u;
    return YVEX_OK;
}

static int programs_read(void *opaque, unsigned long long tensor, unsigned long long offset,
    void *output, size_t bytes, yvex_error *err)
{
    yvex_runtime_moe_programs *c = opaque;
    const yvex_materialized_tensor_binding *b =
        yvex_materialization_session_tensor_at(c->model->materialization, tensor);
    yvex_materialization_failure failure = {0};
    unsigned long long total;
    if (!b || !yvex_core_u64_add(c->reads, bytes, &total))
        return programs_refuse(err, YVEX_ERR_BOUNDS, "compiled parameter read is invalid");
    int rc = yvex_materialization_session_read(c->model->materialization, b, offset, output, bytes, &failure, err);
    if (rc == YVEX_OK) c->reads = total;
    return rc;
}

static int programs_parameter(yvex_runtime_moe_programs *c, const yvex_program_physical_value *v,
    yvex_program_kernel_parameter *p, yvex_error *err)
{
    const yvex_materialized_tensor_binding *b =
        yvex_materialization_session_tensor_at(c->model->materialization, v->tensor_id);
    if (!b || !b->row_count || b->encoded_bytes % b->row_count)
        return programs_refuse(err, YVEX_ERR_FORMAT, "compiled parameter storage is invalid");
    *p = (yvex_program_kernel_parameter){.tensor_id = v->tensor_id,
        .weight = {.encoded_bytes = b->encoded_bytes, .qtype = b->qtype, .row_width = b->row_width,
            .row_count = b->row_count, .row_bytes = b->encoded_bytes / b->row_count}};
    if (yvex_backend_kind_of(c->session->backend) == YVEX_BACKEND_KIND_CPU) {
        p->read = programs_read; p->read_context = c;
        return YVEX_OK;
    }
    unsigned long long bytes, address;
    yvex_execution_layout_class layout;
    int rc = yvex_runtime_private_residency_execution_view(c->model->residency, b,
        &p->weight.encoded, &bytes, &layout, err);
    if (rc != YVEX_OK) return rc;
    if (layout != YVEX_EXECUTION_LAYOUT_CANONICAL_ROW || bytes != b->encoded_bytes ||
        yvex_backend_resident_resolve(c->session->backend, p->weight.encoded, bytes, &address) !=
            YVEX_BACKEND_RESIDENT_HIT)
        return programs_refuse(err, YVEX_ERR_FORMAT, "compiled operand requires exact canonical resident bytes");
    return YVEX_OK;
}

/* Only cold deployment decisions select a different physical activation recipe.
 * The serialized single-row path retains its admitted F32 row-dot implementation. */
static int programs_batch_target(yvex_runtime_moe_programs *c, const yvex_program_physical *p,
    yvex_program_physical **out, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p);
    *out = NULL;
    if (yvex_backend_kind_of(c->session->backend) != YVEX_BACKEND_KIND_CUDA ||
        c->options.evidence_level == YVEX_ATTENTION_EVIDENCE_FULL) return YVEX_OK;
    yvex_program_target_choice *choices = calloc(s->step_count, sizeof(*choices));
    if (!choices) return programs_refuse(err, YVEX_ERR_NOMEM, "target choice allocation failed");
    size_t count = 0u;
    int rc = YVEX_OK;
    const yvex_runtime_execution_profile *profile = c->options.execution_profile;
    int degraded = profile && profile->moe_resolution == YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED;
    for (size_t i = 0u; rc == YVEX_OK && i < s->step_count; ++i) {
        const yvex_program_physical_step *step = yvex_program_physical_step_at(p, i);
        if (strcmp(step->implementation, "linear.row_dot.f32.v1")) continue;
        const yvex_program_physical_value *v = yvex_program_physical_value_at(p, step->operands[1]);
        const yvex_engine_implementation_record *r = runtime_specialization_tensor(
            c->specialization, c->model->physical_execution, v->tensor_id);
        if (!r) {
            rc = programs_refuse(err, YVEX_ERR_STATE, "compiled parameter has no deployment activation authority");
            break;
        }
        yvex_execution_activation_class activation = degraded ? r->fallback_activation : r->activation;
        if (activation == YVEX_EXECUTION_ACTIVATION_DEVICE_ENCODED)
            choices[count++] = (yvex_program_target_choice){i, "linear.row_dot.q8.v1"};
        else if (activation != YVEX_EXECUTION_ACTIVATION_DEVICE_F32)
            rc = programs_refuse(err, YVEX_ERR_UNSUPPORTED, "compiled row-dot deployment activation is unsupported");
    }
    if (rc == YVEX_OK && count) rc = yvex_program_physical_target_compile(out, p, choices, count, err);
    free(choices);
    return rc;
}

static int programs_stage_open(yvex_runtime_moe_programs *c, unsigned long long index, yvex_error *err)
{
    unsigned long long ordinal = index / 3u;
    int shared = (int)(index % 3u);
    const yvex_program_physical *p = shared ? yvex_moe_plan_shared(c->plan, ordinal) :
        yvex_moe_plan_ingress(c->plan, ordinal);
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p);
    unsigned long long h, d;
    if (!s || s->input_count != 1u || s->result_count != (shared ? 1u : 4u))
        return programs_refuse(err, YVEX_ERR_STATE, "compiled ingress interface is absent");
    yvex_program_physical *target = NULL;
    if (shared == 2) {
        int rc = programs_batch_target(c, p, &target, err);
        if (rc != YVEX_OK || !target) return rc;
        c->targets[index] = target;
        p = target;
    }
    yvex_program_kernel_parameter *parameters = calloc(s->value_count, sizeof(*parameters));
    if (!parameters) {
        return programs_refuse(err, YVEX_ERR_NOMEM, "parameter directory allocation failed");
    }
    size_t count = 0u;
    int rc = programs_budget(c, &h, &d, err);
    for (size_t i = 0u; rc == YVEX_OK && i < s->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(p, i);
        if (v->parameter) {
            rc = programs_parameter(c, v, parameters + count, err);
            if (rc == YVEX_OK && !yvex_core_u64_add(c->parameter_bytes[index],
                parameters[count].weight.encoded_bytes, &c->parameter_bytes[index]))
                rc = programs_refuse(err, YVEX_ERR_BOUNDS, "compiled parameter footprint overflowed");
            count++;
        }
    }
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&c->stages[index], p, parameters, count,
        c->session->backend, c->capacity, 1, h, d, err);
    free(parameters);
    if (rc == YVEX_OK) rc = yvex_program_stage_prepare(c->stages[index], c->capacity, err);
    if (rc == YVEX_OK && c->capacity != 1u) rc = yvex_program_stage_prepare(c->stages[index], 1u, err);
    if (rc != YVEX_OK) return rc;
    yvex_program_stage_resources(c->stages[index], &h, &d);
    if (!yvex_core_u64_add(c->host, h, &c->host) || !yvex_core_u64_add(c->device, d, &c->device))
        return programs_refuse(err, YVEX_ERR_BOUNDS, "prepared resource accounting overflowed");
    for (size_t i = shared ? 4u : 0u; i < (shared ? 5u : 6u); ++i) {
        if (i == 4u && !shared) continue;
        yvex_program_value_layout layout;
        rc = yvex_program_physical_value_layout(p, i == 5u ? 0u :
            yvex_program_physical_result_at(p, shared ? 0u : i),
            c->capacity, &layout, err);
        if (rc != YVEX_OK) return rc;
        if (!layout.dynamic_rows || layout.storage_scalar != YVEX_IR_F32 || layout.elements % c->capacity)
            return programs_refuse(err, YVEX_ERR_FORMAT, "compiled ingress requires row-populated F32 carriers");
        unsigned long long width = layout.elements / c->capacity;
        if (width > c->widths[i]) c->widths[i] = width;
    }
    return programs_budget(c, &h, &d, err);
}

int yvex_runtime_moe_programs_open(yvex_runtime_moe_programs **out, yvex_model_engine *model,
    yvex_runtime_execution_session *session, const yvex_moe_plan *plan, const yvex_runtime_moe_options *options,
    unsigned long long host_reserved, unsigned long long device_reserved, yvex_error *err)
{
    const yvex_moe_plan_summary *s = yvex_moe_plan_summary_get(plan);
    if (out) *out = NULL;
    if (!out || !s || !options || !s->layer_count || s->layer_count >
        (SIZE_MAX - sizeof(yvex_runtime_moe_programs)) /
            (3u * (sizeof(yvex_program_stage *) + sizeof(yvex_program_physical *) + sizeof(unsigned long long))))
        return programs_refuse(err, YVEX_ERR_INVALID_ARG, "compiled directory and resource envelope required");
    yvex_runtime_moe_programs *c = calloc(1u, sizeof(*c));
    if (!c) return programs_refuse(err, YVEX_ERR_NOMEM, "program resource owner allocation failed");
    *out = c;
    c->model = yvex_model_engine_view_get(model); c->session = yvex_runtime_session_view_get(session);
    c->specialization = session ? session->specialization : NULL;
    c->plan = plan; c->options = *options; c->count = s->layer_count * 3u;
    c->host_reserved = host_reserved; c->device_reserved = device_reserved;
    c->host = sizeof(*c) + c->count * (sizeof(*c->stages) + sizeof(*c->targets) + sizeof(*c->parameter_bytes));
    if (!c->model || !c->session) return programs_refuse(err, YVEX_ERR_STATE, "engine/session views required");
    c->capacity = yvex_backend_kind_of(c->session->backend) == YVEX_BACKEND_KIND_CPU ? 1u : options->row_capacity;
    if (!c->capacity) c->capacity = 1u;
    c->stages = calloc((size_t)c->count, sizeof(*c->stages));
    c->targets = calloc((size_t)c->count, sizeof(*c->targets));
    c->parameter_bytes = calloc((size_t)c->count, sizeof(*c->parameter_bytes));
    if (!c->stages || !c->targets || !c->parameter_bytes)
        return programs_refuse(err, YVEX_ERR_NOMEM, "program directory allocation failed");
    int rc = YVEX_OK;
    for (unsigned long long i = 0u; rc == YVEX_OK && i < c->count; ++i) rc = programs_stage_open(c, i, err);
    for (size_t i = 0u; rc == YVEX_OK && i < 6u &&
        yvex_backend_kind_of(c->session->backend) != YVEX_BACKEND_KIND_CPU; ++i) {
        yvex_backend_tensor_desc desc = {.name = "compiled-ingress-result", .dtype = YVEX_DTYPE_F32,
            .rank = 2u, .dims = {c->capacity, c->widths[i]}};
        unsigned long long h, d;
        rc = programs_budget(c, &h, &d, err);
        if (rc == YVEX_OK && (!yvex_core_u64_mul(c->capacity, c->widths[i], &desc.bytes) ||
            !yvex_core_u64_mul(desc.bytes, sizeof(float), &desc.bytes) || (d && desc.bytes > d)))
            rc = programs_refuse(err, YVEX_ERR_BOUNDS, "result carrier exceeds resource budget");
        if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(c->session->backend, &desc, &c->outputs[i], err);
        if (rc == YVEX_OK && !yvex_core_u64_add(c->device, desc.bytes, &c->device))
            rc = programs_refuse(err, YVEX_ERR_BOUNDS, "result carrier accounting overflowed");
    }
    return rc;
}

int yvex_runtime_moe_programs_host(yvex_runtime_moe_programs *c, unsigned long long ordinal,
    const float *input, float *const outputs[5], unsigned long long *read_bytes,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (read_bytes) *read_bytes = 0u;
    if (!c || !c->stages || ordinal >= c->count / 3u || !read_bytes || !facts)
        return programs_refuse(err, YVEX_ERR_STATE, "compiled stage is not admitted");
    c->reads = 0u;
    int rc = yvex_program_stage_host(c->stages[ordinal * 3u], 1u, &input, 1u, outputs, 4u,
        c->options.cancel_requested, c->options.cancel_context, facts, err);
    yvex_backend_operation_facts shared_facts = {0};
    const float *normalized = outputs[0];
    if (rc == YVEX_OK) rc = yvex_program_stage_host(c->stages[ordinal * 3u + 1u], 1u, &normalized, 1u,
        outputs + 4u, 1u, c->options.cancel_requested, c->options.cancel_context, &shared_facts, err);
    if (rc == YVEX_OK) rc = programs_facts_add(facts, &shared_facts, err);
    *read_bytes = c->reads;
    return rc;
}

int yvex_runtime_moe_programs_device(yvex_runtime_moe_programs *c, unsigned long long ordinal,
    unsigned long long rows, int batched, const yvex_device_tensor *input,
    const float *host_input, yvex_moe_device_ingress *out,
    unsigned long long *encoded_bytes, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (out) memset(out, 0, sizeof(*out));
    if (encoded_bytes) *encoded_bytes = 0u;
    if (!c || !c->stages || ordinal >= c->count / 3u || !out || !encoded_bytes || !facts || !rows ||
        (batched != 0 && batched != 1) ||
        rows > c->capacity || (!input && !host_input))
        return programs_refuse(err, YVEX_ERR_STATE, "compiled device stage is not admitted");
    const yvex_program_physical *p = yvex_moe_plan_ingress(c->plan, ordinal);
    const yvex_program_physical *shared = yvex_moe_plan_shared(c->plan, ordinal);
    yvex_device_tensor *outputs[] = {c->views, c->views + 1u, c->views + 2u,
        c->views + 3u, c->views + 4u, c->views + 5u};
    for (size_t i = 0u; i < 6u; ++i) {
        yvex_backend_tensor_desc desc;
        int rc = yvex_program_device_descriptor(i == 4u ? shared : p, i == 5u ? 0u :
            yvex_program_physical_result_at(i == 4u ? shared : p, i == 4u ? 0u : i),
            rows, &desc, err);
        if (rc != YVEX_OK) return rc;
        const yvex_device_tensor *owner = i == 5u && input ? input : c->outputs[i];
        if ((i == 5u && input && input->bytes != desc.bytes) ||
            !yvex_backend_tensor_f32_subview(owner, 0u, desc.bytes / sizeof(float), outputs[i]))
            return programs_refuse(err, YVEX_ERR_BOUNDS, "result view exceeds its owner");
        outputs[i]->rank = desc.rank;
        memcpy(outputs[i]->dims, desc.dims, sizeof(desc.dims));
    }
    unsigned long long uploaded = 0u;
    if (!input) {
        int rc = yvex_backend_tensor_write(c->session->backend, outputs[5], host_input, outputs[5]->bytes, err);
        if (rc != YVEX_OK) return rc;
        uploaded = outputs[5]->bytes;
    }
    input = outputs[5];
    unsigned long long shared_index = ordinal * 3u + (batched && c->stages[ordinal * 3u + 2u] ? 2u : 1u);
    int rc = yvex_program_stage_device(c->stages[ordinal * 3u], rows, &input, 1u, outputs, 4u,
        c->options.cancel_requested, c->options.cancel_context, facts, err);
    yvex_backend_operation_facts shared_facts = {0};
    const yvex_device_tensor *normalized = outputs[0];
    if (rc == YVEX_OK) rc = yvex_program_stage_device(c->stages[shared_index], rows, &normalized, 1u,
        outputs + 4u, 1u, c->options.cancel_requested, c->options.cancel_context, &shared_facts, err);
    if (rc == YVEX_OK) rc = programs_facts_add(facts, &shared_facts, err);
    if (rc == YVEX_OK && (!yvex_core_u64_add(facts->h2d_bytes, uploaded, &facts->h2d_bytes) ||
        !yvex_core_u64_add(c->parameter_bytes[ordinal * 3u], c->parameter_bytes[shared_index], encoded_bytes)))
        rc = programs_refuse(err, YVEX_ERR_BOUNDS, "compiled execution accounting overflowed");
    if (rc == YVEX_OK) {
        *out = (yvex_moe_device_ingress){outputs[0], outputs[1], outputs[2], outputs[3], outputs[4]};
    }
    return rc;
}

void yvex_runtime_moe_programs_resources(const yvex_runtime_moe_programs *c,
    unsigned long long *host, unsigned long long *device)
{
    if (host) *host = c ? c->host : 0u;
    if (device) *device = c ? c->device : 0u;
}

int yvex_runtime_moe_programs_close(yvex_runtime_moe_programs **out, yvex_error *err)
{
    yvex_runtime_moe_programs *c = out ? *out : NULL;
    if (!c) return YVEX_OK;
    for (unsigned long long i = 0u; c->stages && i < c->count; ++i) {
        int rc = yvex_program_stage_close(&c->stages[i], err);
        if (rc != YVEX_OK) return rc;
        if (c->targets) yvex_program_physical_close(&c->targets[i]);
    }
    for (size_t i = 0u; i < 6u; ++i) if (c->outputs[i]) {
        int rc = yvex_backend_tensor_release(c->session->backend, &c->outputs[i], err);
        if (rc != YVEX_OK) return rc;
    }
    free(c->parameter_bytes); free(c->targets); free(c->stages); free(c); *out = NULL;
    return YVEX_OK;
}
