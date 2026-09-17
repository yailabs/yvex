/* Run an admitted neural component; batching/publication remain runtime-owned.
 * No layer execution loop or reconstruction of numerical operations. */
#include <yvex/internal/vision_program.h>
#include <yvex/internal/multimodal.h>
#include <yvex/internal/program_stage.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int vision_refuse(yvex_error *err, yvex_status status, const char *why)
{
    yvex_error_set(err, status, "runtime.vision-program", why);
    return status;
}

static int vision_values(yvex_sha256 *hash, const float *values, unsigned long long count)
{
    for (unsigned long long i = 0u; i < count; ++i) {
        uint32_t bits;
        memcpy(&bits, values + i, sizeof(bits));
        if (!yvex_sha256_update_u64(hash, bits)) return 0;
    }
    return 1;
}

static int vision_geometry(const yvex_vision_program *p, const yvex_backend_vision_request *binding,
    unsigned long long *input_values, unsigned long long *merged_values, unsigned long long *staging_values,
    yvex_error *err)
{
    const yvex_program_physical_summary *s = p ? yvex_program_physical_summary_get(p->physical) : NULL;
    const yvex_vision_request *r = binding ? binding->request : NULL;
    unsigned long long rows, values, total, deep, stage = 0u;
    if (!s || !r || s->input_count != 1u || s->minimum_rows != s->maximum_rows ||
        s->result_count != 4u + p->observation_count || !r->image_count || !r->patches ||
        !r->merged || !r->deepstack || !binding->weights || !binding->weight_count ||
        !yvex_sha256_hex_valid(binding->residency_identity) ||
        !yvex_core_u64_mul(r->grid_height, r->grid_width, &rows) || rows != s->maximum_rows ||
        !yvex_core_u64_mul(rows, r->image_count, &total) || total != r->patch_rows)
        return vision_refuse(err, YVEX_ERR_FORMAT, "request must fit its exact compiled component signature");
    const yvex_ir_type *input = &yvex_program_physical_value_at(p->physical, 0u)->type;
    if (input->kind != YVEX_IR_TENSOR || input->rank != 2u ||
        !yvex_core_u64_mul(rows, input->shape[1].extent, input_values) ||
        !yvex_core_u64_mul(*input_values, r->image_count, &values) || values > r->patch_capacity ||
        values > SIZE_MAX / sizeof(float))
        return vision_refuse(err, YVEX_ERR_BOUNDS, "patch storage does not cover compiled input population");
    for (size_t i = 0u; i < s->result_count; ++i) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(p->physical,
            yvex_program_physical_result_at(p->physical, i))->type;
        if (t->kind != YVEX_IR_TENSOR || t->rank != 2u || t->shape[0].symbol != YVEX_IR_NONE ||
            t->shape[1].symbol != YVEX_IR_NONE ||
            !yvex_core_u64_mul(t->shape[0].extent, t->shape[1].extent, &values) ||
            !yvex_core_u64_add(stage, values, &stage))
            return vision_refuse(err, YVEX_ERR_BOUNDS, "compiled result extent is incompatible");
        if (!i) *merged_values = values;
        if (i < 4u && values != *merged_values)
            return vision_refuse(err, YVEX_ERR_FORMAT, "component result populations differ");
    }
    if (!yvex_core_u64_mul(*merged_values, r->image_count, &total) || total > r->merged_capacity ||
        !yvex_core_u64_mul(total, 3u, &deep) || deep > r->deepstack_capacity ||
        !yvex_core_u64_add(total, deep, &total) || !yvex_core_u64_add(total, stage, staging_values) ||
        *staging_values > SIZE_MAX / sizeof(float))
        return vision_refuse(err, YVEX_ERR_BOUNDS, "component publication storage exceeds admitted bounds");
    uintptr_t a = (uintptr_t)r->merged, b = (uintptr_t)r->deepstack;
    unsigned long long a_bytes = (*merged_values * r->image_count) * sizeof(float);
    unsigned long long b_bytes = deep * sizeof(float);
    if (a_bytes > UINTPTR_MAX - a || b_bytes > UINTPTR_MAX - b ||
        !(a + a_bytes <= b || b + b_bytes <= a))
        return vision_refuse(err, YVEX_ERR_FORMAT, "component publication destinations alias or overflow");
    for (size_t i = 0u; i < s->step_count; ++i) {
        const yvex_program_physical_step *op = yvex_program_physical_step_at(p->physical, i);
        const yvex_ir_attribute *h = yvex_program_physical_attribute(op, "grid_height");
        const yvex_ir_attribute *w = yvex_program_physical_attribute(op, "grid_width");
        if ((h && h->value.integer != r->grid_height) || (w && w->value.integer != r->grid_width))
            return vision_refuse(err, YVEX_ERR_FORMAT, "spatial inputs differ from compiled grid constraints");
    }
    return YVEX_OK;
}

static int vision_observe(const yvex_vision_program *p, const yvex_vision_request *r,
    float *const *outputs, yvex_error *err)
{
    if (!r->observe) return p->observation_count ?
        vision_refuse(err, YVEX_ERR_FORMAT, "inspection program requires its observer") : YVEX_OK;
    for (size_t i = 0u; i < p->observation_count; ++i) {
        const yvex_vision_program_observation *o = &p->observations[i];
        int rc = r->observe(r->observer_context, o->stage, o->layer, outputs[4u + i], o->rows, o->width, err);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

int yvex_vision_program_execute(const yvex_vision_program *p, const yvex_component_execution *component,
    const yvex_backend_vision_request *binding, yvex_vision_result *result, yvex_error *err)
{
    unsigned long long input_values, merged_values = 0u, staging_values, host, device;
    yvex_program_kernel_parameter *parameters = NULL;
    float *staging = NULL, **outputs = NULL;
    yvex_vision_result published = {0};
    yvex_backend_operation_facts facts = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_error cleanup;
    if (result) memset(result, 0, sizeof(*result));
    if (!component || component->schema_version != YVEX_COMPONENT_EXECUTION_SCHEMA_V2 ||
        !component->backend || !component->program_stage || *component->program_stage || !result)
        return vision_refuse(err, YVEX_ERR_INVALID_ARG, "component requires reachable stage cleanup ownership");
    int rc = vision_geometry(p, binding, &input_values, &merged_values, &staging_values, err);
    if (rc != YVEX_OK) return rc;
    if (strcmp(binding->residency_identity, component->residency_identity))
        return vision_refuse(err, YVEX_ERR_STATE, "compiled bindings require the exact component residency");
    const yvex_vision_request *r = binding->request;
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p->physical);
    if (binding->weight_count > SIZE_MAX / sizeof(*parameters))
        return vision_refuse(err, YVEX_ERR_BOUNDS, "parameter directory is unbounded");
    parameters = calloc((size_t)binding->weight_count, sizeof(*parameters));
    outputs = calloc(s->result_count, sizeof(*outputs));
    staging = malloc((size_t)staging_values * sizeof(float));
    if (!parameters || !outputs || !staging) {
        free(parameters); free(outputs); free(staging);
        return vision_refuse(err, YVEX_ERR_NOMEM, "component publication staging allocation failed");
    }
    for (size_t i = 0u; i < binding->weight_count; ++i)
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i, .weight = binding->weights[i]};
    unsigned long long retained = merged_values * r->image_count * 4u, offset = retained;
    for (size_t i = 0u; i < s->result_count; ++i) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(p->physical,
            yvex_program_physical_result_at(p->physical, i))->type;
        outputs[i] = staging + offset;
        offset += t->shape[0].extent * t->shape[1].extent;
    }
    rc = yvex_program_stage_open(component->program_stage, p->physical, parameters, binding->weight_count,
        component->backend, s->maximum_rows, 1, 0u, 0u, err);
    for (unsigned long long image = 0u; rc == YVEX_OK && image < r->image_count; ++image) {
        const float *inputs[] = {r->patches + image * input_values};
        rc = yvex_program_stage_host(*component->program_stage, s->maximum_rows, inputs, 1u, outputs, s->result_count,
            r->cancel_requested, r->cancel_context, &facts, err);
        if (rc == YVEX_OK) rc = vision_observe(p, r, outputs, err);
        if (rc == YVEX_OK &&
            (!yvex_core_u64_add(published.kernel_launches, facts.kernel_launches, &published.kernel_launches) ||
            !yvex_core_u64_add(published.h2d_bytes, facts.h2d_bytes, &published.h2d_bytes) ||
            !yvex_core_u64_add(published.d2h_bytes, facts.d2h_bytes, &published.d2h_bytes)))
            rc = vision_refuse(err, YVEX_ERR_BOUNDS, "component evidence accounting overflowed");
        if (rc == YVEX_OK) {
            memcpy(staging + image * merged_values, outputs[0], (size_t)merged_values * sizeof(float));
            for (size_t i = 0u; i < 3u; ++i)
                memcpy(staging + (r->image_count + image * 3u + i) * merged_values,
                    outputs[1u + i], (size_t)merged_values * sizeof(float));
        }
    }
    yvex_program_stage_resources(*component->program_stage, &host, &device);
    yvex_sha256_init(&hash);
    if (rc == YVEX_OK && (!yvex_sha256_update_text(&hash, "yvex.component.vision-program.v2") ||
        !yvex_sha256_update_text(&hash, s->identity) ||
        !yvex_sha256_update_text(&hash, binding->residency_identity) ||
        !yvex_sha256_update_u64(&hash, r->image_count) ||
        !vision_values(&hash, r->patches, input_values * r->image_count) ||
        !vision_values(&hash, staging, retained) || !yvex_sha256_final(&hash, digest)))
        rc = vision_refuse(err, YVEX_ERR_STATE, "component result identity failed");
    int cleanup_rc = yvex_program_stage_close(component->program_stage, &cleanup);
    if (cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    if (rc == YVEX_OK && r->cancel_requested && r->cancel_requested(r->cancel_context))
        rc = vision_refuse(err, YVEX_ERR_CANCELLED, "component cancelled before publication");
    if (rc == YVEX_OK) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(p->physical,
            yvex_program_physical_result_at(p->physical, 0u))->type;
        for (size_t i = 0u; i < s->step_count; ++i) {
            const yvex_program_physical_step *op = yvex_program_physical_step_at(p->physical, i);
            if (strcmp(op->implementation, "attention.full.f32acc.bf16.v1")) continue;
            published.layer_count++;
            published.hidden_width = yvex_program_physical_value_at(p->physical, op->results[0])->type.shape[1].extent;
        }
        published.patch_rows = r->patch_rows;
        published.merged_rows = t->shape[0].extent * r->image_count;
        published.output_width = t->shape[1].extent;
        published.device_bytes = device;
        yvex_core_text_copy(published.residency_identity, sizeof(published.residency_identity),
            binding->residency_identity);
        yvex_sha256_hex(digest, published.execution_identity);
        published.complete = 1;
        memcpy(r->merged, staging, (size_t)(merged_values * r->image_count) * sizeof(float));
        memcpy(r->deepstack, staging + merged_values * r->image_count,
            (size_t)(merged_values * r->image_count * 3u) * sizeof(float));
        *result = published;
    }
    free(parameters); free(outputs); free(staging);
    return rc;
}
