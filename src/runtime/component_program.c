/* Component program resources and transactional host publication. Compiler
 * operations, not family callbacks, own computational order and geometry. */
#include <yvex/internal/component.h>
#include <yvex/internal/program_stage.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int component_program_inputs(const yvex_component_text_request *r,
    const yvex_program_physical_summary *s, unsigned long long width,
    unsigned int *positions, float *dense, yvex_program_host_input inputs[70], yvex_error *err)
{
    const yvex_backend_text_multimodal_input *m = r->multimodal;
    unsigned long long rows = r->token_count, values = rows * width;
    size_t injections = s->input_count > 4u ? s->input_count - 6u : 0u;
    if (!!m != !!injections) goto invalid;
    inputs[0] = (yvex_program_host_input){.indices = r->token_ids};
    for (size_t axis = 0u; axis < 3u; ++axis) {
        inputs[1u + axis] = (yvex_program_host_input){.indices = positions + axis * rows};
        for (unsigned long long i = 0u; i < rows; ++i) positions[axis * rows + i] = (unsigned int)i;
    }
    if (!m) return YVEX_OK;
    unsigned long long visual_values, deep_values;
    if (!m->position_ids || m->position_capacity < rows * 3u || !m->visual_token_indices ||
        !m->visual_token_count || !m->visual_embeddings || !m->deepstack_embeddings ||
        m->deepstack_layer_count != injections || !yvex_sha256_hex_valid(m->vision_execution_identity) ||
        !yvex_core_u64_mul(m->visual_token_count, width, &visual_values) ||
        !yvex_core_u64_mul(visual_values, injections, &deep_values) ||
        m->visual_embedding_capacity < visual_values || m->deepstack_embedding_capacity < deep_values) goto invalid;
    for (size_t i = 0u; i < s->step_count; ++i) {
        const yvex_program_physical_step *op = yvex_program_physical_step_at(r->program, i);
        if (strcmp(op->implementation, "rotary_tables.f64.bf16.v1")) continue;
        const yvex_ir_type *table = &yvex_program_physical_value_at(r->program, op->results[0])->type;
        if (yvex_program_physical_attribute(op, "section_y")->value.integer != m->mrope_sections[1] ||
            yvex_program_physical_attribute(op, "section_z")->value.integer != m->mrope_sections[2] ||
            m->mrope_sections[0] > table->shape[1].extent / 2u ||
            m->mrope_sections[1] > table->shape[1].extent / 2u - m->mrope_sections[0] ||
            m->mrope_sections[2] != table->shape[1].extent / 2u - m->mrope_sections[0] - m->mrope_sections[1])
            goto invalid;
    }
    for (unsigned long long i = 0u; i < rows * 3u; ++i) {
        if (m->position_ids[i] > UINT_MAX) goto invalid;
        positions[i] = (unsigned int)m->position_ids[i];
    }
    inputs[4] = (yvex_program_host_input){.values = dense};
    for (size_t i = 5u; i < s->input_count; ++i)
        inputs[i] = (yvex_program_host_input){.values = dense + rows + (i - 5u) * values};
    for (unsigned long long row = 0u; row < m->visual_token_count; ++row) {
        unsigned long long target = m->visual_token_indices[row];
        if (target >= rows || dense[target] != 0.0f) goto invalid;
        dense[target] = 1.0f;
        memcpy(dense + rows + target * width, m->visual_embeddings + row * width, (size_t)width * sizeof(float));
        for (size_t block = 0u; block < injections; ++block)
            memcpy(dense + rows + (block + 1u) * values + target * width,
                m->deepstack_embeddings + block * visual_values + row * width, (size_t)width * sizeof(float));
    }
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.program.inputs",
        "position, unique visual-row and injection inputs differ from the admitted component signature");
    return YVEX_ERR_FORMAT;
}

static int component_program_values(yvex_sha256 *hash, const float *values, unsigned long long count)
{
    for (unsigned long long i = 0u; i < count; ++i) {
        uint32_t bits;
        memcpy(&bits, values + i, sizeof(bits));
        if (!yvex_sha256_update_u64(hash, bits)) return 0;
    }
    return 1;
}

static int component_tensor_extent(const yvex_ir_type *t, unsigned long long rows,
    unsigned long long *count)
{
    if (!t || t->kind != YVEX_IR_TENSOR || !t->rank ||
        (t->scalar != YVEX_IR_F32 && t->scalar != YVEX_IR_BF16 && t->scalar != YVEX_IR_INDEX)) return 0;
    *count = 1u;
    for (unsigned int axis = 0u; axis < t->rank; ++axis) {
        unsigned long long extent = t->shape[axis].symbol == YVEX_IR_NONE ? t->shape[axis].extent : rows;
        if ((axis && t->shape[axis].symbol != YVEX_IR_NONE) || !extent ||
            !yvex_core_u64_mul(*count, extent, count)) return 0;
    }
    return *count <= SIZE_MAX / sizeof(float);
}

static int component_tensor_admit(const yvex_component_program_request *r,
    const yvex_program_physical_summary *s, unsigned long long *total, yvex_error *err)
{
    *total = 0u;
    if (!r || !s || (!r->inputs && !r->index_inputs) || !r->outputs || !r->input_capacity || !r->output_capacity ||
        !r->parameter_name || r->input_count != s->input_count || r->output_count != s->result_count ||
        (r->parameter_axis_order != YVEX_COMPONENT_PARAMETER_GGUF_ORDER &&
         r->parameter_axis_order != YVEX_COMPONENT_PARAMETER_SOURCE_ORDER) ||
        r->rows < s->minimum_rows || r->rows > s->maximum_rows || !r->rows || r->rows % s->row_multiple)
        goto incompatible;
    for (size_t i = 0u; i < r->input_count; ++i) {
        unsigned long long count;
        const yvex_ir_type *t = &yvex_program_physical_value_at(r->program, i)->type;
        const float *values = r->inputs ? r->inputs[i] : NULL;
        const unsigned int *indices = r->index_inputs ? r->index_inputs[i] : NULL;
        if ((t->scalar == YVEX_IR_INDEX ? !indices || values : !values || indices) || !component_tensor_extent(t,
            r->rows, &count) || count > r->input_capacity[i]) goto incompatible;
    }
    for (size_t i = 0u; i < r->output_count; ++i) {
        unsigned long long count;
        const yvex_ir_type *t = &yvex_program_physical_value_at(r->program,
            yvex_program_physical_result_at(r->program, i))->type;
        if (!r->outputs[i] || t->scalar == YVEX_IR_INDEX ||
            !component_tensor_extent(t, r->rows, &count) || count > r->output_capacity[i] ||
            !yvex_core_u64_add(*total, count, total) || *total > SIZE_MAX / sizeof(float)) goto incompatible;
        uintptr_t start = (uintptr_t)r->outputs[i];
        if (count * sizeof(float) > UINTPTR_MAX - start) goto incompatible;
        for (size_t j = 0u; j < i; ++j) {
            unsigned long long other;
            t = &yvex_program_physical_value_at(r->program, yvex_program_physical_result_at(r->program, j))->type;
            if (!component_tensor_extent(t, r->rows, &other)) goto incompatible;
            uintptr_t peer = (uintptr_t)r->outputs[j];
            if (!(start + count * sizeof(float) <= peer || peer + other * sizeof(float) <= start)) goto incompatible;
        }
    }
    return YVEX_OK;
incompatible:
    yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.program.admission",
        "tensor views, populations or output aliases differ from the compiled signature");
    return YVEX_ERR_FORMAT;
}

typedef struct {
    yvex_materialization_session *session;
    const yvex_materialized_tensor_binding *binding;
    unsigned long long id, reads, bytes;
} component_parameter_source;

static int component_parameter_read(void *context, unsigned long long id, unsigned long long offset,
    void *output, size_t bytes, yvex_error *err)
{
    component_parameter_source *source = context;
    yvex_materialization_failure failure;
    unsigned long long total;
    if (!source || id != source->id || source->reads == ULLONG_MAX ||
        !yvex_core_u64_add(source->bytes, bytes, &total)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component.parameter", "bound parameter source is incompatible");
        return YVEX_ERR_STATE;
    }
    int rc = yvex_materialization_session_read(source->session, source->binding, offset,
        output, bytes, &failure, err);
    if (rc == YVEX_OK) { source->reads++; source->bytes = total; }
    return rc;
}

static int component_parameter_bind(yvex_materialization_session *session, const char *name,
    const yvex_program_physical_value *value,
    yvex_program_kernel_parameter *parameter, component_parameter_source *source,
    yvex_component_parameter_axis_order order, yvex_error *err)
{
    const yvex_materialization_summary *summary = yvex_materialization_session_summary(session);
    const yvex_materialized_tensor_binding *binding = NULL;
    for (unsigned long long i = 0u; summary && i < summary->tensor_count; ++i) {
        const yvex_materialized_tensor_binding *candidate = yvex_materialization_session_tensor_at(session, i);
        if (candidate && !strcmp(name, candidate->name)) { binding = candidate; break; }
    }
    if (!binding || !binding->row_count || !binding->rank || binding->rank > YVEX_TENSOR_MAX_DIMS ||
        binding->encoded_bytes % binding->row_count) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.parameter", "exact materialized parameter is missing");
        return YVEX_ERR_FORMAT;
    }
    if (!source && (!parameter->weight.encoded || parameter->weight.qtype != binding->qtype ||
        parameter->weight.encoded_bytes != binding->encoded_bytes)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.parameter",
            "resident payload differs from package binding");
        return YVEX_ERR_FORMAT;
    }
    if (source) *source = (component_parameter_source){
        .session = session, .binding = binding, .id = parameter->tensor_id};
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    for (unsigned int i = 0u; i < binding->rank; ++i)
        dims[i] = binding->dims[order == YVEX_COMPONENT_PARAMETER_SOURCE_ORDER ? i : binding->rank - 1u - i];
    unsigned long long rows, width;
    int rc = order == YVEX_COMPONENT_PARAMETER_SOURCE_ORDER ?
        yvex_program_physical_source_parameter_view(value, binding->rank, dims, binding->qtype,
            binding->encoded_bytes, &rows, &width, err) :
        yvex_program_physical_parameter_view(value, binding->rank, dims, binding->qtype,
            binding->encoded_bytes, &rows, &width, err);
    if (rc != YVEX_OK) return rc;
    parameter->weight = (yvex_component_encoded_weight){.qtype = binding->qtype,
        .encoded = source ? NULL : parameter->weight.encoded,
        .row_count = rows, .row_width = width,
        .encoded_bytes = binding->encoded_bytes, .row_bytes = binding->encoded_bytes / rows};
    parameter->read = source ? component_parameter_read : NULL;
    parameter->read_context = source;
    return YVEX_OK;
}

static int component_tensor_run(const yvex_component_execution *component, yvex_materialization_session *materialized,
    const yvex_component_program_request *r, yvex_component_program_result *result, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(r ? r->program : NULL);
    const yvex_materialization_summary *source = yvex_materialization_session_summary(materialized);
    yvex_component_program_result published = {0};
    yvex_program_kernel_parameter *parameters = NULL;
    yvex_program_host_input *inputs = NULL;
    component_parameter_source *sources = NULL;
    yvex_backend *cpu = NULL, *backend = component ? component->backend : NULL;
    yvex_program_stage *local_stage = NULL;
    yvex_program_stage **stage = component ? component->program_stage : &local_stage;
    const char *owner_identity = component ? component->residency_identity : source ? source->plan_identity : NULL;
    float **outputs = NULL, *staged = NULL;
    unsigned long long total, directory, bytes, host, device;
    if (result) memset(result, 0, sizeof(*result));
    if (!result || !yvex_sha256_hex_valid(owner_identity) || !stage || *stage ||
        (component && (component->schema_version != YVEX_COMPONENT_EXECUTION_SCHEMA_V2 || !backend)) ||
        (!component && (!source || !source->committed))) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component.program.owner",
            "exact component and reachable cleanup slot required");
        return YVEX_ERR_STATE;
    }
    int rc = component_tensor_admit(r, s, &total, err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_core_u64_mul(s->input_count, sizeof(*inputs), &bytes) ||
        !yvex_core_u64_mul(s->value_count, sizeof(*parameters) + (source ? sizeof(*sources) : 0u), &directory) ||
        !yvex_core_u64_add(directory, bytes, &directory) ||
        !yvex_core_u64_mul(s->result_count, sizeof(*outputs), &bytes) ||
        !yvex_core_u64_add(directory, bytes, &directory) ||
        !yvex_core_u64_mul(total, sizeof(float), &bytes) || !yvex_core_u64_add(directory, bytes, &host) ||
        host > SIZE_MAX || (r->host_limit && host >= r->host_limit)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.program.resources",
            "binding/publication storage exceeds budget");
        return YVEX_ERR_BOUNDS;
    }
    parameters = calloc(s->value_count, sizeof(*parameters));
    inputs = calloc(s->input_count, sizeof(*inputs));
    sources = source ? calloc(s->value_count, sizeof(*sources)) : NULL;
    outputs = calloc(s->result_count, sizeof(*outputs));
    staged = malloc((size_t)bytes);
    if (!parameters || !inputs || !outputs || !staged || (source && !sources)) {
        free(parameters); free(sources); free(outputs); free(staged); free(inputs);
        yvex_error_set(err, YVEX_ERR_NOMEM, "runtime.component.program.resources",
            "binding/publication allocation failed");
        return YVEX_ERR_NOMEM;
    }
    size_t used = 0u;
    for (size_t i = 0u; rc == YVEX_OK && i < s->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(r->program, i);
        char name[256];
        if (!v->parameter) continue;
        parameters[used].tensor_id = v->tensor_id;
        rc = r->parameter_name(r->parameter_context, v->tensor_id, name, err);
        if (rc == YVEX_OK) rc = source ?
            component_parameter_bind(materialized, name, v, parameters + used, sources + used,
                r->parameter_axis_order, err) :
            yvex_component_execution_weight_view(component, name, &parameters[used].weight, err);
        if (rc == YVEX_OK && !source && r->parameter_axis_order == YVEX_COMPONENT_PARAMETER_SOURCE_ORDER)
            rc = component_parameter_bind(component->materialization, name, v, parameters + used, NULL,
                r->parameter_axis_order, err);
        used++;
    }
    unsigned long long offset = 0u;
    for (size_t i = 0u; i < s->result_count; ++i) {
        unsigned long long count;
        (void)component_tensor_extent(&yvex_program_physical_value_at(r->program,
            yvex_program_physical_result_at(r->program, i))->type, r->rows, &count);
        outputs[i] = staged + offset;
        offset += count;
    }
    if (rc == YVEX_OK && source) {
        yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CPU, .memory_limit_bytes = r->device_limit};
        rc = yvex_backend_open(&cpu, &options, err);
        backend = cpu;
    }
    if (rc == YVEX_OK) rc = yvex_program_stage_open(stage, r->program, parameters, used,
        backend, r->rows, 1, r->host_limit ? r->host_limit - host : 0u, r->device_limit, err);
    for (size_t i = 0u; i < r->input_count; ++i) inputs[i] = (yvex_program_host_input){
        .values = r->inputs ? r->inputs[i] : NULL, .indices = r->index_inputs ? r->index_inputs[i] : NULL};
    if (rc == YVEX_OK) rc = yvex_program_stage_host_inputs(*stage, r->rows, inputs, r->input_count,
        outputs, r->output_count, r->cancel_requested, r->cancel_context, &published.facts, err);
    for (unsigned long long i = 0u; rc == YVEX_OK && i < total; ++i) if (!isfinite(staged[i])) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.program.output",
            "non-finite result is not publishable");
        rc = YVEX_ERR_FORMAT;
    }
    yvex_program_stage_resources(*stage, &directory, &device);
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    int identity = yvex_sha256_update_text(&hash, source ? "yvex.component.source-program.v2" :
        "yvex.component.tensor-program.v2") && yvex_sha256_update_u64(&hash, r->parameter_axis_order) &&
        yvex_sha256_update_text(&hash, s->identity) &&
        yvex_sha256_update_text(&hash, owner_identity) && yvex_sha256_update_u64(&hash, r->rows);
    for (size_t i = 0u; rc == YVEX_OK && identity && i < r->input_count; ++i) {
        unsigned long long count;
        const yvex_ir_type *type = &yvex_program_physical_value_at(r->program, i)->type;
        identity = component_tensor_extent(type, r->rows, &count);
        if (type->scalar == YVEX_IR_INDEX) {
            for (unsigned long long j = 0u; identity && j < count; ++j)
                identity = yvex_sha256_update_u64(&hash, r->index_inputs[i][j]);
        } else identity = identity && component_program_values(&hash, r->inputs[i], count);
    }
    if (rc == YVEX_OK && !(identity && component_program_values(&hash, staged, total) &&
        yvex_sha256_final(&hash, digest))) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component.program.identity", "execution identity failed");
        rc = YVEX_ERR_STATE;
    }
    yvex_error cleanup;
    int cleanup_rc = yvex_program_stage_close(stage, &cleanup);
    if (cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    if (cpu) {
        cleanup_rc = yvex_backend_close_checked(&cpu, &cleanup);
        if (cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    }
    if (rc == YVEX_OK && !yvex_core_u64_add(host, directory, &published.host_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.program.resources", "host accounting overflowed");
        rc = YVEX_ERR_BOUNDS;
    }
    for (size_t i = 0u; sources && rc == YVEX_OK && i < used; ++i)
        if (!yvex_core_u64_add(published.parameter_reads, sources[i].reads, &published.parameter_reads) ||
            !yvex_core_u64_add(published.parameter_bytes, sources[i].bytes, &published.parameter_bytes)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.program.resources",
                "parameter accounting overflowed");
            rc = YVEX_ERR_BOUNDS;
        }
    if (rc == YVEX_OK && r->cancel_requested && r->cancel_requested(r->cancel_context)) {
        yvex_error_set(err, YVEX_ERR_CANCELLED, "runtime.component.program.cancel", "cancelled before publication");
        rc = YVEX_ERR_CANCELLED;
    }
    if (rc == YVEX_OK) {
        for (size_t i = 0u; i < s->result_count; ++i) {
            size_t count = (size_t)((i + 1u < s->result_count ? outputs[i + 1u] : staged + total) - outputs[i]);
            memcpy(r->outputs[i], outputs[i], count * sizeof(float));
        }
        published.device_bytes = device;
        published.complete = 1;
        yvex_sha256_hex(digest, published.execution_identity);
        *result = published;
    }
    free(parameters); free(sources); free(outputs); free(staged); free(inputs);
    return rc;
}

int yvex_component_tensor_program_execute(const yvex_component_execution *component,
    const yvex_component_program_request *r, yvex_component_program_result *result, yvex_error *err)
{
    return component_tensor_run(component, NULL, r, result, err);
}

int yvex_component_materialized_program_execute(yvex_materialization_session *source,
    const yvex_component_program_request *r, yvex_component_program_result *result, yvex_error *err)
{
    return component_tensor_run(NULL, source, r, result, err);
}

static int component_program_identity(const yvex_program_physical_summary *program,
    const yvex_component_execution *component, const yvex_component_text_request *request,
    unsigned long long count, char identity[YVEX_SHA256_HEX_BYTES])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.component.program.result.v2") ||
        !yvex_sha256_update_text(&hash, program->identity) ||
        !yvex_sha256_update_text(&hash, component->residency_identity) ||
        !yvex_sha256_update_u64(&hash, request->token_count)) return 0;
    if (request->multimodal) {
        const yvex_backend_text_multimodal_input *m = request->multimodal;
        unsigned long long visual, deep;
        const yvex_ir_type *out = &yvex_program_physical_value_at(request->program,
            yvex_program_physical_result_at(request->program, 0u))->type;
        if (!yvex_core_u64_mul(m->visual_token_count, out->shape[1].extent, &visual) ||
            !yvex_core_u64_mul(visual, m->deepstack_layer_count, &deep) ||
            !yvex_sha256_update_text(&hash, m->vision_execution_identity) ||
            !yvex_sha256_update_u64(&hash, m->visual_token_count) ||
            !yvex_sha256_update_u64(&hash, m->deepstack_layer_count) ||
            !component_program_values(&hash, m->visual_embeddings, visual) ||
            !component_program_values(&hash, m->deepstack_embeddings, deep)) return 0;
        for (unsigned long long i = 0u; i < request->token_count * 3u; ++i)
            if (!yvex_sha256_update_u64(&hash, m->position_ids[i])) return 0;
        for (unsigned long long i = 0u; i < m->visual_token_count; ++i)
            if (!yvex_sha256_update_u64(&hash, m->visual_token_indices[i])) return 0;
    }
    for (unsigned long long i = 0u; i < request->token_count; ++i)
        if (!yvex_sha256_update_u64(&hash, request->token_ids[i])) return 0;
    if (!component_program_values(&hash, request->output, count)) return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, identity);
    return 1;
}

int yvex_component_text_program_execute(const yvex_component_execution *component,
    const yvex_component_text_request *request, const yvex_component_encoded_weight *weights,
    size_t count, yvex_backend_text_execution_result *result, yvex_error *err)
{
    const yvex_program_physical_summary *summary = request ?
        yvex_program_physical_summary_get(request->program) : NULL;
    yvex_program_stage **stage;
    yvex_program_kernel_parameter *parameters = NULL;
    unsigned int *positions = NULL;
    float *dense = NULL, *staged = NULL;
    yvex_backend_text_execution_result published = {0};
    yvex_backend_operation_facts facts = {0};
    yvex_program_host_input inputs[70];
    unsigned long long bytes, position_bytes, values, parameter_bytes, dense_values = 0u, dense_bytes = 0u;
    unsigned long long output_bytes, host_budget = 0u, host, device;
    yvex_error cleanup;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!component || component->schema_version != YVEX_COMPONENT_EXECUTION_SCHEMA_V2 ||
        !component->program_stage || *component->program_stage ||
        !component->backend || !yvex_sha256_hex_valid(component->residency_identity) ||
        !summary || (summary->input_count != 4u && (summary->input_count < 7u || summary->input_count > 70u)) ||
        summary->result_count != 1u ||
        !request->token_ids || !request->output || !result || !weights || !count ||
        !request->token_count || request->token_count > UINT_MAX ||
        !yvex_core_u64_mul(count, sizeof(*parameters), &parameter_bytes) ||
        !yvex_core_u64_mul(request->token_count, 3u * sizeof(*positions), &position_bytes) ||
        !yvex_core_u64_add(parameter_bytes, position_bytes, &bytes) || bytes > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.program",
            "admitted program and bindings required");
        return YVEX_ERR_INVALID_ARG;
    }
    stage = component->program_stage;
    const yvex_ir_type *out = &yvex_program_physical_value_at(request->program,
        yvex_program_physical_result_at(request->program, 0u))->type;
    if (out->kind != YVEX_IR_TENSOR || out->rank != 2u || out->scalar != YVEX_IR_BF16 ||
        out->shape[1].symbol != YVEX_IR_NONE ||
        !yvex_core_u64_mul(request->token_count, out->shape[1].extent, &values) ||
        values > request->output_capacity || !yvex_core_u64_mul(values, sizeof(float), &output_bytes) ||
        !yvex_core_u64_add(bytes, output_bytes, &bytes) || output_bytes > SIZE_MAX ||
        (summary->input_count > 4u &&
            (!yvex_core_u64_mul(values, summary->input_count - 5u, &dense_values) ||
             !yvex_core_u64_add(dense_values, request->token_count, &dense_values) ||
             !yvex_core_u64_mul(dense_values, sizeof(float), &dense_bytes) || dense_bytes > SIZE_MAX ||
             !yvex_core_u64_add(bytes, dense_bytes, &bytes))) ||
        (request->maximum_host_bytes && bytes >= request->maximum_host_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.program", "result geometry or staging exceeds budget");
        return YVEX_ERR_BOUNDS;
    }
    if (request->maximum_host_bytes) host_budget = request->maximum_host_bytes - bytes;
    parameters = calloc(count, sizeof(*parameters));
    positions = malloc((size_t)position_bytes);
    dense = dense_bytes ? calloc(1u, (size_t)dense_bytes) : NULL;
    staged = malloc((size_t)output_bytes);
    if (!parameters || !positions || !staged || (dense_bytes && !dense)) {
        free(parameters); free(positions); free(dense); free(staged);
        yvex_error_set(err, YVEX_ERR_NOMEM, "runtime.component.program", "component staging allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (size_t i = 0u; i < count; ++i)
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i, .weight = weights[i]};
    rc = component_program_inputs(request, summary, out->shape[1].extent, positions, dense, inputs, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(stage, request->program, parameters, count, component->backend,
        request->token_count, 1, host_budget, request->maximum_device_bytes, err);
    float *outputs[] = {staged};
    if (rc == YVEX_OK) rc = yvex_program_stage_host_inputs(*stage, request->token_count,
        inputs, summary->input_count, outputs, 1u, request->cancelled, request->cancellation_context, &facts, err);
    yvex_program_stage_resources(*stage, &host, &device);
    yvex_component_text_request publication = *request;
    publication.output = staged;
    if (rc == YVEX_OK && !component_program_identity(summary, component, &publication,
        values, published.execution_identity)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component.program", "component execution identity failed");
        rc = YVEX_ERR_STATE;
    }
    yvex_error_clear(&cleanup);
    int cleanup_rc = yvex_program_stage_close(stage, &cleanup);
    if (cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    if (rc == YVEX_OK && request->cancelled && request->cancelled(request->cancellation_context)) {
        yvex_error_set(err, YVEX_ERR_CANCELLED, "runtime.component.program", "component publication cancelled");
        rc = YVEX_ERR_CANCELLED;
    }
    if (rc == YVEX_OK) {
        published.token_count = request->token_count; published.hidden_width = out->shape[1].extent;
        for (size_t i = 0u; i < summary->step_count; ++i)
            published.layer_count += !strcmp(yvex_program_physical_step_at(request->program, i)->implementation,
                "attention.full.f32acc.bf16.v1");
        published.resident_bytes = component->resident_encoded_bytes;
        published.kernel_launches = facts.kernel_launches;
        published.h2d_bytes = facts.h2d_bytes; published.d2h_bytes = facts.d2h_bytes;
        published.device_bytes = device; published.complete = 1;
        memcpy(published.residency_identity, component->residency_identity, sizeof(published.residency_identity));
        memcpy(request->output, staged, (size_t)output_bytes);
        *result = published;
    }
    free(positions); free(parameters); free(dense); free(staged);
    return rc;
}
