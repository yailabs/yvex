/* Stateful kernels over explicit physical operands. No layer loop, projections,
 * FFN composition or source interpretation is present here. */
#include <yvex/internal/program_sequence.h>
#include <yvex/internal/stateful_attention.h>
#include <yvex/internal/quant_numeric.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum {
    SEQUENCE_QUERY, SEQUENCE_GATE, SEQUENCE_KEY, SEQUENCE_MIXED, SEQUENCE_COS, SEQUENCE_SIN, SEQUENCE_SCRATCH_COUNT
};
struct yvex_program_sequence {
    const yvex_program_physical *program;
    const yvex_program_physical_summary *summary;
    const yvex_runtime_session_view *session;
    const yvex_program_kernels *kernels;
    const yvex_backend_transformer_operations *ops;
    const yvex_attention_layer_plan **attention;
    const char *attention_identity;
    yvex_device_tensor *scratch[SEQUENCE_SCRATCH_COUNT];
    float *rope, *state;
    unsigned long long *positions;
    unsigned long long capacity, rope_width, kv_width, host_bytes, device_bytes;
};

static int sequence_refuse(yvex_error *err, yvex_status status, const char *why)
{
    yvex_error_set(err, status, "runtime.program.sequence", why);
    return status;
}

static int sequence_facts(yvex_backend_operation_facts *a, const yvex_backend_operation_facts *b)
{
#define SEQUENCE_FACT_ADD(member) if (!yvex_core_u64_add(a->member, b->member, &a->member)) return 0
    SEQUENCE_FACT_ADD(h2d_bytes); SEQUENCE_FACT_ADD(d2h_bytes); SEQUENCE_FACT_ADD(d2d_bytes);
    SEQUENCE_FACT_ADD(kernel_launches); SEQUENCE_FACT_ADD(upload_count); SEQUENCE_FACT_ADD(download_count);
    SEQUENCE_FACT_ADD(queue_synchronizations); SEQUENCE_FACT_ADD(device_synchronizations);
    SEQUENCE_FACT_ADD(active_weight_bytes); SEQUENCE_FACT_ADD(state_bytes); SEQUENCE_FACT_ADD(activation_bytes);
    SEQUENCE_FACT_ADD(temporary_bytes); SEQUENCE_FACT_ADD(accelerated_matrix_launches);
#undef SEQUENCE_FACT_ADD
    a->compulsory_memory_facts_available &= b->compulsory_memory_facts_available;
    return 1;
}

static int sequence_attention_bind(yvex_program_sequence *c, const yvex_model_engine_view *model,
                                    unsigned long long widths[SEQUENCE_SCRATCH_COUNT], yvex_error *err)
{
    const yvex_attention_summary *summary = yvex_attention_plan_summary(model->attention);
    unsigned long long ordinal = 0u;
    size_t i;
    for (i = 0u; i < c->summary->step_count; ++i) {
        const yvex_program_physical_step *s = yvex_program_physical_step_at(c->program, i);
        const yvex_attention_layer_plan *a;
        unsigned long long query, kv;
        if (strcmp(s->implementation, "gated_causal.bf16.v1")) continue;
        a = yvex_attention_plan_layer_at(model->attention, ordinal++);
        if (!a || a->query_heads != yvex_program_physical_attribute(s, "query_heads")->value.integer ||
            a->kv_heads != yvex_program_physical_attribute(s, "kv_heads")->value.integer ||
            a->head_dimension != yvex_program_physical_attribute(s, "head_dimension")->value.integer ||
            a->rope_head_dimension != yvex_program_physical_attribute(s, "rotary_dimension")->value.integer ||
            a->position.theta != yvex_program_physical_attribute(s, "theta")->value.integer ||
            a->sliding_window != yvex_program_physical_attribute(s, "maximum_context")->value.integer ||
            a->position.scaling_factor != 1u || a->compute_contract != YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1 ||
            !yvex_core_u64_mul(a->query_heads, a->head_dimension, &query) ||
            !yvex_core_u64_mul(a->kv_heads, a->head_dimension, &kv))
            return sequence_refuse(err, YVEX_ERR_FORMAT, "attention provider differs from its compiled operation");
        c->attention[i] = a;
        if (query > widths[SEQUENCE_QUERY]) widths[SEQUENCE_QUERY] = query;
        if (kv > c->kv_width) c->kv_width = kv;
        if (a->rope_head_dimension > c->rope_width) c->rope_width = a->rope_head_dimension;
    }
    if (ordinal && (!summary || ordinal != summary->layer_count))
        return sequence_refuse(err, YVEX_ERR_FORMAT, "attention provider population differs from computational state");
    c->attention_identity = summary ? summary->attention_plan_identity : NULL;
    widths[SEQUENCE_GATE] = widths[SEQUENCE_MIXED] = widths[SEQUENCE_QUERY];
    widths[SEQUENCE_KEY] = c->kv_width;
    widths[SEQUENCE_COS] = widths[SEQUENCE_SIN] = c->rope_width;
    return YVEX_OK;
}

static int sequence_scratch_open(yvex_program_sequence *c, const unsigned long long *widths,
                                  unsigned long long host_limit, unsigned long long device_limit, yvex_error *err)
{
    unsigned long long state_values, rope_values, values, bytes, total;
    unsigned int i;
    if (!c->rope_width) return YVEX_OK;
    if (!yvex_core_u64_mul(c->capacity, c->rope_width, &rope_values) ||
        !yvex_core_u64_mul(rope_values, 2u, &rope_values) ||
        !yvex_core_u64_mul(c->capacity, c->kv_width, &state_values) ||
        !yvex_core_u64_mul(state_values, 4u, &state_values) ||
        !yvex_core_u64_add(state_values, rope_values, &values) ||
        !yvex_core_u64_mul(values, sizeof(float), &bytes) ||
        !yvex_core_u64_mul(c->capacity, sizeof(*c->positions), &total) ||
        !yvex_core_u64_add(bytes, total, &bytes) || !yvex_core_u64_add(c->host_bytes, bytes, &total) ||
        total > SIZE_MAX || (host_limit && total > host_limit))
        return sequence_refuse(err, YVEX_ERR_BOUNDS, "state operation host workspace exceeds admission");
    c->rope = malloc((size_t)rope_values * sizeof(float));
    c->state = malloc((size_t)state_values * sizeof(float));
    c->positions = malloc((size_t)c->capacity * sizeof(*c->positions));
    if (!c->rope || !c->state || !c->positions)
        return sequence_refuse(err, YVEX_ERR_NOMEM, "state operation host workspace allocation failed");
    c->host_bytes = total;
    for (i = 0u; i < SEQUENCE_SCRATCH_COUNT; ++i) {
        yvex_backend_tensor_desc d = {.name = "program-state-scratch", .dtype = YVEX_DTYPE_F32, .rank = 2u,
            .dims = {c->capacity, widths[i]}};
        int rc;
        if (!yvex_core_u64_mul(c->capacity, widths[i], &values) ||
            !yvex_core_u64_mul(values, sizeof(float), &d.bytes) ||
            !yvex_core_u64_add(c->device_bytes, d.bytes, &total) || (device_limit && total > device_limit))
            return sequence_refuse(err, YVEX_ERR_BOUNDS, "state operation device workspace exceeds admission");
        rc = yvex_backend_tensor_alloc(c->session->backend, &d, &c->scratch[i], err);
        if (rc != YVEX_OK) return rc;
        c->device_bytes = total;
    }
    return YVEX_OK;
}

int yvex_program_sequence_open(yvex_program_sequence **out, const yvex_program_physical *program,
    const yvex_model_engine_view *model, const yvex_runtime_session_view *session, const yvex_program_kernels *kernels,
    unsigned long long capacity, unsigned long long host_limit, unsigned long long device_limit, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(program);
    yvex_program_sequence *c;
    unsigned long long widths[SEQUENCE_SCRATCH_COUNT] = {0};
    unsigned long long host;
    int rc;
    if (out) *out = NULL;
    if (!out || !s || !model || !session || !session->backend || !kernels || !capacity || capacity > s->maximum_rows)
        return sequence_refuse(err, YVEX_ERR_INVALID_ARG, "compiled program and runtime state providers required");
    host = sizeof(*c) + s->step_count * sizeof(*c->attention);
    if (host_limit && host > host_limit)
        return sequence_refuse(err, YVEX_ERR_BOUNDS, "state operation directory exceeds admission");
    c = calloc(1u, sizeof(*c));
    if (!c) return sequence_refuse(err, YVEX_ERR_NOMEM, "state operation owner allocation failed");
    c->program = program; c->summary = s; c->session = session; c->kernels = kernels;
    c->capacity = capacity; c->host_bytes = host;
    c->ops = yvex_backend_transformer_operations_get(session->backend);
    c->attention = calloc(s->step_count, sizeof(*c->attention));
    rc = c->attention ? YVEX_OK : sequence_refuse(err, YVEX_ERR_NOMEM, "state operation directory allocation failed");
    for (size_t i = 0u; rc == YVEX_OK && i < s->step_count; ++i) {
        const char *implementation = yvex_program_physical_step_at(program, i)->implementation;
        if (!strcmp(implementation, "gated_delta.bf16.f32state.v1") &&
            (!c->ops || !c->ops->bf16_round || !c->ops->gated_delta_execute || !session->sequence_state))
            rc = sequence_refuse(err, YVEX_ERR_UNSUPPORTED, "compiled recurrence requires an admitted state backend");
        if (!strcmp(implementation, "gated_causal.bf16.v1") &&
            (!c->ops || !c->ops->bf16_round || !c->ops->rotary_half_f32 ||
             !c->ops->split_interleaved_two_f32 || !c->ops->sigmoid_product_bf16 ||
             !session->attention_state_provider))
            rc = sequence_refuse(err, YVEX_ERR_UNSUPPORTED, "compiled attention requires an admitted state backend");
    }
    if (rc == YVEX_OK) rc = sequence_attention_bind(c, model, widths, err);
    if (rc == YVEX_OK) rc = sequence_scratch_open(c, widths, host_limit, device_limit, err);
    if (rc != YVEX_OK) (void)yvex_program_sequence_close(&c, NULL);
    *out = c;
    return rc;
}

static int sequence_view(yvex_program_sequence *c, unsigned int slot, unsigned long long rows,
                          unsigned long long width, yvex_device_tensor *v)
{
    if (!yvex_backend_tensor_f32_subview(c->scratch[slot], 0u, rows * width, v)) return 0;
    v->rank = 2u; v->dims[0] = rows; v->dims[1] = width;
    return 1;
}

static int sequence_round(yvex_program_sequence *c, yvex_device_tensor *v,
                           yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_backend_operation_facts current = {0};
    int rc = c->ops->bf16_round(c->session->backend, v, v->bytes / sizeof(float), &current, err);
    if (rc == YVEX_OK && !sequence_facts(facts, &current))
        rc = sequence_refuse(err, YVEX_ERR_BOUNDS, "state operation counters overflowed");
    return rc;
}

static int sequence_rotary(yvex_program_sequence *c, const yvex_program_device_invocation *r,
    const yvex_attention_layer_plan *a, yvex_device_tensor *q, yvex_device_tensor *k,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long width = a->rope_head_dimension, values = r->rows * width, token, coordinate;
    unsigned long long start = r->arguments[r->step->operands[5]].index;
    float *cosines = c->rope, *sines = cosines + values;
    yvex_device_tensor cosine, sine;
    yvex_backend_operation_facts current = {0};
    int rc;
    if (start > ULLONG_MAX - r->rows || !sequence_view(c, SEQUENCE_COS, r->rows, width, &cosine) ||
        !sequence_view(c, SEQUENCE_SIN, r->rows, width, &sine))
        return sequence_refuse(err, YVEX_ERR_BOUNDS, "rotary operation exceeds admitted workspace");
    for (token = 0u; token < r->rows; ++token) for (coordinate = 0u; coordinate < width; ++coordinate) {
        unsigned long long pair = coordinate % (width / 2u), index = token * width + coordinate;
        double angle = (double)(start + token) * pow((double)a->position.theta, -(double)(2u * pair) / (double)width);
        cosines[index] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)cos(angle)));
        sines[index] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)sin(angle)));
    }
    rc = yvex_backend_tensor_write(c->session->backend, &cosine, cosines, cosine.bytes, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_write(c->session->backend, &sine, sines, sine.bytes, err);
    if (rc == YVEX_OK) {
        current.h2d_bytes = cosine.bytes + sine.bytes;
        if (!sequence_facts(facts, &current)) rc = sequence_refuse(err, YVEX_ERR_BOUNDS, "rotary counters overflowed");
    }
    if (rc == YVEX_OK) rc = c->ops->rotary_half_f32(c->session->backend, q, &cosine, &sine, r->rows,
        a->query_heads, a->head_dimension, width, &current, err);
    if (rc == YVEX_OK && !sequence_facts(facts, &current)) rc = YVEX_ERR_BOUNDS;
    if (rc == YVEX_OK) rc = sequence_round(c, q, facts, err);
    if (rc == YVEX_OK) rc = c->ops->rotary_half_f32(c->session->backend, k, &cosine, &sine, r->rows,
        a->kv_heads, a->head_dimension, width, &current, err);
    if (rc == YVEX_OK && !sequence_facts(facts, &current)) rc = YVEX_ERR_BOUNDS;
    if (rc == YVEX_OK) rc = sequence_round(c, k, facts, err);
    return rc;
}

static int sequence_attention(yvex_program_sequence *c, const yvex_program_device_invocation *r,
    const yvex_program_sequence_request *options, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_attention_layer_plan *a = c->attention[r->step_index];
    const yvex_ir_attribute *epsilon = yvex_program_physical_attribute(s, "qk_epsilon");
    yvex_device_tensor q, gate, k, mixed;
    yvex_backend_operation_facts current = {0};
    yvex_runtime_stateful_attention_request request = {0};
    yvex_runtime_stateful_attention_result result = {0};
    yvex_attention_failure failure = {0};
    unsigned long long qwidth = a->query_heads * a->head_dimension, kwidth = a->kv_heads * a->head_dimension;
    yvex_ir_id root = yvex_program_physical_value_at(c->program, s->operands[6])->state_root;
    unsigned long long start = r->arguments[s->operands[5]].index;
    unsigned long long maximum = yvex_program_physical_attribute(s, "maximum_context")->value.integer;
    int rc;
    if (start > maximum || r->rows > maximum - start)
        return sequence_refuse(err, YVEX_ERR_BOUNDS, "attention position exceeds the compiled context envelope");
    if (r->arguments[root].state_handle != root || !sequence_view(c, SEQUENCE_QUERY, r->rows, qwidth, &q) ||
        !sequence_view(c, SEQUENCE_GATE, r->rows, qwidth, &gate) ||
        !sequence_view(c, SEQUENCE_KEY, r->rows, kwidth, &k) ||
        !sequence_view(c, SEQUENCE_MIXED, r->rows, qwidth, &mixed))
        return sequence_refuse(err, YVEX_ERR_FORMAT, "attention state/workspace differs from compiled input");
    rc = c->ops->split_interleaved_two_f32(c->session->backend, &r->values[s->operands[0]], &q, &gate,
        r->rows, a->query_heads, a->head_dimension, &current, err);
    if (rc == YVEX_OK && !sequence_facts(facts, &current)) rc = YVEX_ERR_BOUNDS;
    /* Input SSA values are immutable even when one imported topology currently
     * has no other use. Normalization/RoPE mutate only operation scratch. */
    if (rc == YVEX_OK)
        rc = yvex_backend_tensor_copy(c->session->backend, &k, &r->values[s->operands[1]], err);
    if (rc == YVEX_OK && !yvex_core_u64_add(facts->d2d_bytes, k.bytes, &facts->d2d_bytes))
        rc = YVEX_ERR_BOUNDS;
    q.dims[0] = r->rows * a->query_heads; q.dims[1] = a->head_dimension;
    k.dims[0] = r->rows * a->kv_heads; k.dims[1] = a->head_dimension;
    if (rc == YVEX_OK) rc = yvex_backend_op_rms_norm(c->session->backend, &q,
        yvex_program_kernels_small_weight(c->kernels, s->operands[3]), (float)epsilon->value.real, &q, err);
    if (rc == YVEX_OK) rc = sequence_round(c, &q, facts, err);
    if (rc == YVEX_OK) rc = yvex_backend_op_rms_norm(c->session->backend, &k,
        yvex_program_kernels_small_weight(c->kernels, s->operands[4]), (float)epsilon->value.real, &k, err);
    if (rc == YVEX_OK) rc = sequence_round(c, &k, facts, err);
    if (rc == YVEX_OK) rc = sequence_rotary(c, r, a, &q, &k, facts, err);
    request.backend = c->session->backend; request.state = c->session->attention_state_provider;
    request.residency = c->session->state_residency; request.layer = a;
    request.attention_plan_identity = c->attention_identity; request.input_identity = options->input_identity;
    request.layer_ordinal = a->ordinal; request.token_position = r->arguments[s->operands[5]].index;
    request.token_count = r->rows; request.query = &q; request.key = &k;
    request.value = &r->values[s->operands[2]]; request.output = &mixed;
    request.host_workspace = c->state; request.host_workspace_values = c->capacity * c->kv_width * 4u;
    request.host_positions = c->positions; request.host_position_capacity = c->capacity;
    request.cancellation.requested = options->cancel_requested; request.cancellation.context = options->cancel_context;
    if (rc == YVEX_OK) rc = yvex_runtime_stateful_attention_execute(&request, &result, &failure, err);
    if (rc == YVEX_OK && !sequence_facts(facts, &result.attention)) rc = YVEX_ERR_BOUNDS;
    if (rc == YVEX_OK) rc = c->ops->sigmoid_product_bf16(c->session->backend, &mixed, &gate,
        &r->values[s->results[0]], r->rows * qwidth, &current, err);
    if (rc == YVEX_OK && !sequence_facts(facts, &current)) rc = YVEX_ERR_BOUNDS;
    return rc;
}

static int sequence_delta(yvex_program_sequence *c, const yvex_program_device_invocation *r,
    const yvex_program_sequence_request *options, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_program_physical_step *s = r->step;
    const yvex_gated_delta_plan *plan = yvex_program_physical_delta_at(c->program, r->step_index);
    yvex_ir_id root = yvex_program_physical_value_at(c->program, s->operands[8])->state_root;
    yvex_ir_id recurrent = yvex_program_physical_value_at(c->program, s->operands[9])->state_root;
    yvex_sequence_device_state_view committed = {0};
    yvex_sequence_device_state_output candidate = {0};
    yvex_gated_delta_device_request request = {0};
    yvex_gated_delta_device_result result = {0};
    int rc;
    if (!plan || r->arguments[root].state_handle != root || r->arguments[recurrent].state_handle != recurrent)
        return sequence_refuse(err, YVEX_ERR_FORMAT, "recurrent operation has incompatible state handles");
    rc = yvex_sequence_state_device_layer(c->session->sequence_state, root, &committed, &candidate, err);
    request.token_count = r->rows;
    request.projected_qkv = &r->values[s->operands[0]]; request.projected_output_gate = &r->values[s->operands[1]];
    request.projected_beta = &r->values[s->operands[2]]; request.projected_decay = &r->values[s->operands[3]];
    request.convolution_weight = yvex_program_kernels_small_weight(c->kernels, s->operands[4]);
    request.decay_log = yvex_program_kernels_small_weight(c->kernels, s->operands[5]);
    request.time_bias = yvex_program_kernels_small_weight(c->kernels, s->operands[6]);
    request.normalization_weight = yvex_program_kernels_small_weight(c->kernels, s->operands[7]);
    request.convolution_state = committed.convolution; request.recurrent_state = committed.recurrent;
    request.next_convolution_state = candidate.convolution; request.next_recurrent_state = candidate.recurrent;
    request.output = &r->values[s->results[0]];
    request.cancel_requested = options->cancel_requested; request.cancel_context = options->cancel_context;
    if (rc == YVEX_OK) rc = c->ops->gated_delta_execute(c->session->backend, plan, &request, &result, facts, err);
    if (rc == YVEX_OK) rc = sequence_round(c, request.output, facts, err);
    if (rc == YVEX_OK) rc = yvex_sequence_state_stage(c->session->sequence_state, root, err);
    return rc;
}

int yvex_program_sequence_invoke(yvex_program_sequence *c, const yvex_program_device_invocation *r,
    const yvex_program_sequence_request *options, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!c || !r || r->program != c->program || !options || !facts || !r->arguments || !r->values ||
        !r->rows || r->rows > c->capacity ||
        r->step != yvex_program_physical_step_at(c->program, r->step_index) || !r->step)
        return sequence_refuse(err, YVEX_ERR_INVALID_ARG, "state operation requires its bound physical invocation");
    if (!strcmp(r->step->implementation, "gated_delta.bf16.f32state.v1"))
        return sequence_delta(c, r, options, facts, err);
    if (!strcmp(r->step->implementation, "gated_causal.bf16.v1"))
        return sequence_attention(c, r, options, facts, err);
    return sequence_refuse(err, YVEX_ERR_UNSUPPORTED, "operation has no admitted state implementation");
}

void yvex_program_sequence_resources(const yvex_program_sequence *c,
    unsigned long long *host, unsigned long long *device)
{
    if (host) *host = c ? c->host_bytes : 0u;
    if (device) *device = c ? c->device_bytes : 0u;
}

int yvex_program_sequence_close(yvex_program_sequence **out, yvex_error *err)
{
    yvex_program_sequence *c = out ? *out : NULL;
    unsigned int i;
    int rc;
    if (!c) return YVEX_OK;
    for (i = 0u; i < SEQUENCE_SCRATCH_COUNT; ++i) if (c->scratch[i]) {
        rc = yvex_backend_tensor_release(c->session->backend, &c->scratch[i], err);
        if (rc != YVEX_OK) return rc;
    }
    free(c->positions); free(c->state); free(c->rope); free(c->attention); free(c);
    *out = NULL;
    return YVEX_OK;
}
