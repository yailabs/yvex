/* Import-only compatibility for authenticated v3-v5 computational records.
 * Role lookup occurs once here, never in the executable consumer. New imports
 * do not use this language; they project typed IR directly from source. */
#include <yvex/internal/decoder_plan.h>
#include <yvex/internal/program_physical.h>
#include <yvex/internal/execution.h>
#include <yvex/internal/graph.h>
#include <yvex/qtype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_ir_module *module;
    const yvex_decoder_plan *decoder;
    const yvex_decoder_plan_summary *summary;
    const yvex_physical_execution_ir *parameters;
    const yvex_physical_execution_summary *physical;
    const yvex_attention_layer_plan *attention;
    yvex_program_parameter_binding *bindings;
    size_t binding_count;
    yvex_ir_id block, rows, hidden, position;
    unsigned long long layer;
    int rc;
    yvex_error *error;
} decoder_import;

static void import_refuse(decoder_import *b, const char *reason)
{
    if (b->rc != YVEX_OK) return;
    b->rc = YVEX_ERR_FORMAT;
    yvex_error_set(b->error, YVEX_ERR_FORMAT, "compiler.decoder-import", reason);
}

static yvex_ir_id import_type(decoder_import *b, yvex_ir_type_kind kind, yvex_ir_scalar scalar,
    const uint64_t *shape, unsigned int rank, const char *domain, int rows)
{
    yvex_ir_type t = {.kind = kind, .scalar = scalar, .rank = rank};
    yvex_ir_id type = YVEX_IR_NONE;
    unsigned int i;
    for (i = 0u; i < rank; ++i) t.shape[i] = (yvex_ir_extent){YVEX_IR_NONE, shape[i]};
    if (rows) t.shape[0] = (yvex_ir_extent){b->rows, 0u};
    if (domain) yvex_core_text_copy(t.domain, sizeof(t.domain), domain);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &type, b->error);
    return type;
}

static yvex_ir_id import_rows(decoder_import *b, uint64_t width)
{
    const uint64_t shape[] = {1u, width};
    return import_type(b, YVEX_IR_TENSOR, YVEX_IR_BF16, shape, 2u, NULL, 1);
}

static yvex_ir_id import_emit(decoder_import *b, const char *name, const yvex_ir_id *args,
    unsigned int count, const yvex_ir_id *types, unsigned int results,
    const yvex_ir_attribute *attrs, unsigned int attribute_count)
{
    yvex_ir_id id = YVEX_IR_NONE;
    yvex_ir_operation_request r = {.operation = name, .operands = args, .operand_count = count,
        .result_types = types, .result_count = results, .attributes = attrs, .attribute_count = attribute_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &id, b->error);
    return id;
}

static yvex_ir_id import_result(decoder_import *b, yvex_ir_id op, unsigned int index)
{
    const yvex_ir_operation *o = yvex_ir_operation_at(b->module, op);
    return o && index < o->result_count ? o->results[index] : YVEX_IR_NONE;
}

static yvex_ir_id import_parameter(decoder_import *b, yvex_tensor_role role,
    const uint64_t *shape, unsigned int rank)
{
    const yvex_physical_execution_decision *found = NULL;
    unsigned long long i, elements = 1u;
    size_t matches = 0u;
    yvex_ir_id type = import_type(b, YVEX_IR_TENSOR, YVEX_IR_BF16, shape, rank, NULL, 0), value;
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    if (!rank || !shape[rank - 1u]) {
        import_refuse(b, "legacy parameter has no nonzero physical row width");
        return YVEX_IR_NONE;
    }
    for (i = 0u; i < b->physical->decision_count; ++i) {
        const yvex_physical_execution_decision *d = yvex_physical_execution_ir_decision_at(b->parameters, i);
        unsigned int scope = b->layer == YVEX_ATTENTION_NO_LAYER ?
            YVEX_TENSOR_SCOPE_GLOBAL : YVEX_TENSOR_SCOPE_MAIN_LAYER;
        if (d->role == (unsigned int)role && d->scope == scope &&
            d->layer_index == b->layer && d->predictor_index == YVEX_ATTENTION_NO_TENSOR_INDEX) {
            found = d;
            matches++;
        }
    }
    for (i = 0u; i < rank; ++i)
        if (!yvex_core_u64_mul(elements, shape[i], &elements)) import_refuse(b, "legacy parameter shape overflows");
    if (matches != 1u || !found || found->canonical_qtype != YVEX_GGUF_QTYPE_BF16 ||
        found->canonical_row_width != shape[rank - 1u] ||
        found->canonical_row_count != elements / shape[rank - 1u] ||
        b->binding_count >= b->physical->decision_count) {
        import_refuse(b, "legacy parameter role lacks one exact BF16 physical binding");
        return YVEX_IR_NONE;
    }
    snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "parameter_%llu", found->terminal_tensor_id);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->physical->identity);
    value = import_result(b, import_emit(b, "core.parameter", NULL, 0u, &type, 1u, attrs, 2u), 0u);
    b->bindings[b->binding_count++] = (yvex_program_parameter_binding){value,
        found->terminal_tensor_id, found->canonical_qtype};
    return value;
}

static yvex_ir_id import_linear(decoder_import *b, yvex_ir_id input, yvex_tensor_role role,
    uint64_t width, uint64_t output_width)
{
    const uint64_t shape[] = {output_width, width};
    yvex_ir_id args[] = {input, import_parameter(b, role, shape, 2u)};
    yvex_ir_id type = import_rows(b, output_width);
    return import_result(b, import_emit(b, "nn.linear", args, 2u, &type, 1u, NULL, 0u), 0u);
}

static yvex_ir_id import_norm(decoder_import *b, yvex_ir_id input, yvex_tensor_role role,
    const yvex_decoder_layer_plan *layer)
{
    const uint64_t shape[] = {layer->hidden_width};
    yvex_ir_id args[] = {input, import_parameter(b, role, shape, 1u)};
    yvex_ir_attribute attrs[] = {
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = layer->normalization_epsilon},
        {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64,
         .value.real = layer->normalization_weight_convention == YVEX_NORMALIZATION_WEIGHT_ONE_PLUS ? 1.0 : 0.0}};
    return import_result(b, import_emit(b, "nn.rms_norm", args, 2u, &b->hidden, 1u, attrs, 2u), 0u);
}

static yvex_ir_id import_add(decoder_import *b, yvex_ir_id left, yvex_ir_id right)
{
    yvex_ir_id args[] = {left, right};
    return import_result(b, import_emit(b, "tensor.add", args, 2u, &b->hidden, 1u, NULL, 0u), 0u);
}

static yvex_ir_id import_delta(decoder_import *b, const yvex_decoder_layer_plan *layer,
    yvex_ir_id input, const yvex_ir_id *types, yvex_ir_id *state)
{
    const yvex_gated_delta_plan *p = &layer->gated_delta;
    const yvex_gated_delta_requirement *d = &p->requirement;
    const uint64_t convolution[] = {p->qkv_width, 1u, d->convolution_kernel};
    const uint64_t heads[] = {d->value_heads}, dimension[] = {d->value_head_dimension};
    yvex_ir_id args[10], outputs[] = {import_rows(b, p->value_width), types[0], types[1]}, id;
    yvex_ir_attribute attrs[] = {
        {.name = "key_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = d->key_heads},
        {.name = "value_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = d->value_heads},
        {.name = "key_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = d->key_head_dimension},
        {.name = "value_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = d->value_head_dimension},
        {.name = "convolution_kernel", .kind = YVEX_IR_ATTR_U64, .value.integer = d->convolution_kernel},
        {.name = "qk_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = d->qk_normalization_epsilon},
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = d->output_normalization_epsilon},
        {.name = "query_scale", .kind = YVEX_IR_ATTR_F64, .value.real = d->query_scale}};
    if (d->output_normalization_weight_convention != YVEX_NORMALIZATION_WEIGHT_DIRECT)
        import_refuse(b, "legacy recurrent normalization has no equivalent physical legalization");
    args[0] = import_linear(b, input, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_QKV_PROJECTION,
        layer->hidden_width, p->qkv_width);
    args[1] = import_linear(b, input, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_OUTPUT_GATE,
        layer->hidden_width, p->value_width);
    args[2] = import_linear(b, input, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_BETA_PROJECTION,
        layer->hidden_width, d->value_heads);
    args[3] = import_linear(b, input, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_DECAY_PROJECTION,
        layer->hidden_width, d->value_heads);
    args[4] = import_parameter(b, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_CONVOLUTION, convolution, 3u);
    args[5] = import_parameter(b, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_DECAY_LOG, heads, 1u);
    args[6] = import_parameter(b, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_TIME_BIAS, heads, 1u);
    args[7] = import_parameter(b, YVEX_TENSOR_ROLE_SEQUENCE_MIXER_OUTPUT_NORM, dimension, 1u);
    args[8] = state[0]; args[9] = state[1];
    id = import_emit(b, "sequence.gated_delta", args, 10u, outputs, 3u, attrs, 8u);
    state[0] = import_result(b, id, 1u); state[1] = import_result(b, id, 2u);
    return import_linear(b, import_result(b, id, 0u), YVEX_TENSOR_ROLE_SEQUENCE_MIXER_OUTPUT,
        p->value_width, layer->hidden_width);
}

static yvex_ir_id import_attention(decoder_import *b, const yvex_decoder_layer_plan *layer,
    yvex_ir_id input, yvex_ir_id start, yvex_ir_id type, yvex_ir_id *state)
{
    const yvex_attention_layer_plan *a = &b->attention[layer->attention_ordinal];
    const uint64_t shape[] = {a->head_dimension};
    uint64_t query = a->query_heads * a->head_dimension, kv = a->kv_heads * a->head_dimension;
    yvex_ir_id args[7], outputs[] = {import_rows(b, query), type}, id;
    yvex_ir_attribute attrs[] = {
        {.name = "query_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = a->query_heads},
        {.name = "kv_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = a->kv_heads},
        {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = a->head_dimension},
        {.name = "rotary_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = a->rope_head_dimension},
        {.name = "maximum_context", .kind = YVEX_IR_ATTR_U64, .value.integer = b->summary->maximum_context},
        {.name = "theta", .kind = YVEX_IR_ATTR_U64, .value.integer = a->position.theta},
        {.name = "qk_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-6}};
    args[0] = import_linear(b, input, YVEX_TENSOR_ROLE_ATTENTION_Q, layer->hidden_width, 2u * query);
    args[1] = import_linear(b, input, YVEX_TENSOR_ROLE_ATTENTION_K, layer->hidden_width, kv);
    args[2] = import_linear(b, input, YVEX_TENSOR_ROLE_ATTENTION_V, layer->hidden_width, kv);
    args[3] = import_parameter(b, YVEX_TENSOR_ROLE_ATTENTION_Q_NORM, shape, 1u);
    args[4] = import_parameter(b, YVEX_TENSOR_ROLE_ATTENTION_K_NORM, shape, 1u);
    args[5] = start; args[6] = *state;
    id = import_emit(b, "attention.gated_causal", args, 7u, outputs, 2u, attrs, 7u);
    *state = import_result(b, id, 1u);
    return import_linear(b, import_result(b, id, 0u), YVEX_TENSOR_ROLE_ATTENTION_OUT, query, layer->hidden_width);
}

static yvex_ir_id import_ffn(decoder_import *b, yvex_ir_id input, const yvex_decoder_layer_plan *layer)
{
    yvex_ir_id args[2], type = import_rows(b, layer->intermediate_width), product;
    args[0] = import_linear(b, input, YVEX_TENSOR_ROLE_FFN_GATE, layer->hidden_width, layer->intermediate_width);
    args[1] = import_linear(b, input, YVEX_TENSOR_ROLE_FFN_UP, layer->hidden_width, layer->intermediate_width);
    product = import_result(b, import_emit(b, "nn.silu_product", args, 2u, &type, 1u, NULL, 0u), 0u);
    return import_linear(b, product, YVEX_TENSOR_ROLE_FFN_DOWN, layer->intermediate_width, layer->hidden_width);
}

static size_t import_state_types(decoder_import *b, yvex_ir_id *types)
{
    unsigned long long i;
    size_t count = 0u;
    for (i = 0u; b->rc == YVEX_OK && i < b->summary->layer_count; ++i) {
        const yvex_decoder_layer_plan *layer = yvex_decoder_plan_layer_at(b->decoder, i);
        if (layer->mixer == YVEX_SEMANTIC_DECODER_MIXER_GATED_DELTA) {
            const yvex_gated_delta_plan *p = &layer->gated_delta;
            const uint64_t conv[] = {p->qkv_width, p->requirement.convolution_kernel - 1u};
            const uint64_t recurrent[] = {p->requirement.value_heads,
                p->requirement.key_head_dimension, p->requirement.value_head_dimension};
            types[count++] = import_type(b, YVEX_IR_STATE, YVEX_IR_F32, conv, 2u, "convolution.causal", 0);
            types[count++] = import_type(b, YVEX_IR_STATE, YVEX_IR_F32, recurrent, 3u, "recurrent.gated_delta", 0);
        } else {
            const yvex_attention_layer_plan *a;
            uint64_t kv[4];
            if (layer->attention_ordinal >= b->summary->attention_layer_count) {
                import_refuse(b, "legacy attention ordinal is out of bounds");
                break;
            }
            a = &b->attention[layer->attention_ordinal];
            if (a->ordinal != layer->attention_ordinal || a->layer_index != layer->layer_index ||
                a->sliding_window != b->summary->maximum_context || a->position.scaling_factor != 1u ||
                a->compute_contract != YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1)
                import_refuse(b, "legacy attention cannot normalize to the admitted causal operation");
            kv[0] = 2u; kv[1] = b->summary->maximum_context;
            kv[2] = a->kv_heads; kv[3] = a->head_dimension;
            types[count++] = import_type(b, YVEX_IR_STATE, YVEX_IR_BF16, kv, 4u, "attention.causal_kv", 0);
        }
    }
    return count;
}

static void import_forward(decoder_import *b, yvex_ir_id *signature, yvex_ir_id *results)
{
    const uint64_t tokens[] = {1u}, embedding[] = {b->summary->vocabulary_size, b->summary->hidden_width};
    yvex_ir_id function, args[2], input, start;
    size_t count, state, i;
    yvex_ir_id *state_types = signature + 2u;
    signature[0] = import_type(b, YVEX_IR_TENSOR, YVEX_IR_INDEX, tokens, 1u, NULL, 1);
    signature[1] = b->position;
    count = import_state_types(b, state_types);
    results[0] = b->hidden;
    memcpy(results + 1u, state_types, count * sizeof(*results));
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "forward", signature, 2u + count,
        results, 1u + count, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, &function, b->error);
    if (b->rc != YVEX_OK) return;
    b->block = yvex_ir_function_at(b->module, function)->body;
    args[0] = yvex_ir_block_at(b->module, b->block)->arguments[0];
    start = yvex_ir_block_at(b->module, b->block)->arguments[1];
    memcpy(results + 1u, yvex_ir_block_at(b->module, b->block)->arguments + 2u, count * sizeof(*results));
    b->layer = YVEX_ATTENTION_NO_LAYER;
    args[1] = import_parameter(b, YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, embedding, 2u);
    input = import_result(b, import_emit(b, "nn.embedding", args, 2u, &b->hidden, 1u, NULL, 0u), 0u);
    for (i = 0u, state = 0u; b->rc == YVEX_OK && i < b->summary->layer_count; ++i) {
        const yvex_decoder_layer_plan *layer = yvex_decoder_plan_layer_at(b->decoder, i);
        yvex_ir_id normalized, update;
        b->layer = layer->layer_index;
        normalized = import_norm(b, input, YVEX_TENSOR_ROLE_ATTENTION_NORM, layer);
        if (layer->mixer == YVEX_SEMANTIC_DECODER_MIXER_GATED_DELTA) {
            update = import_delta(b, layer, normalized, state_types + state, results + 1u + state);
            state += 2u;
        } else {
            update = import_attention(b, layer, normalized, start, state_types[state], &results[1u + state]);
            state++;
        }
        input = import_add(b, input, update);
        normalized = import_norm(b, input, YVEX_TENSOR_ROLE_FFN_NORM, layer);
        input = import_add(b, input, import_ffn(b, normalized, layer));
    }
    b->layer = YVEX_ATTENTION_NO_LAYER;
    /* v3-v5 decoder contract used layer zero's norm convention for final norm. */
    results[0] = import_norm(b, input, YVEX_TENSOR_ROLE_OUTPUT_NORM, yvex_decoder_plan_layer_at(b->decoder, 0u));
    (void)import_emit(b, "core.return", results, 1u + (unsigned int)count, NULL, 0u, NULL, 0u);
}

int yvex_decoder_plan_normalize_program(yvex_program_physical **out, const yvex_decoder_plan *decoder,
    const yvex_attention_layer_plan *attention, size_t attention_count,
    const yvex_physical_execution_ir *parameters, yvex_error *err)
{
    const yvex_decoder_plan_summary *s = yvex_decoder_plan_summary_get(decoder);
    const yvex_physical_execution_summary *p = yvex_physical_execution_ir_summary(parameters);
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect(), *yvex_ir_sequence_dialect()};
    decoder_import b = {.decoder = decoder, .summary = s, .parameters = parameters,
        .physical = p, .attention = attention, .error = err};
    yvex_program_execution *execution = NULL;
    yvex_ir_id *signature = NULL, *results = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .multiple = 1u};
    if (out) *out = NULL;
    if (!out || !s || !p || !s->layer_count || s->layer_count > 65536u || !p->decision_count ||
        p->decision_count > 1048576u || attention_count != s->attention_layer_count ||
        (attention_count && !attention)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "compiler.decoder-import", "exact legacy records required");
        return YVEX_ERR_INVALID_ARG;
    }
    b.bindings = calloc((size_t)p->decision_count, sizeof(*b.bindings));
    signature = calloc(2u + 2u * (size_t)s->layer_count, sizeof(*signature));
    results = calloc(1u + 2u * (size_t)s->layer_count, sizeof(*results));
    b.rc = b.bindings && signature && results ? YVEX_OK : YVEX_ERR_NOMEM;
    if (b.rc == YVEX_OK) b.rc = yvex_ir_module_open(&b.module, "compiled_decoder_import",
        s->decoder_plan_identity, dialects, 3u, err);
    rows.maximum = s->maximum_context;
    if (b.rc == YVEX_OK) b.rc = yvex_ir_dimension_add(b.module, &rows, &b.rows, err);
    b.hidden = import_rows(&b, s->hidden_width);
    b.position = import_type(&b, YVEX_IR_SCALAR, YVEX_IR_INDEX, NULL, 0u, NULL, 0);
    if (b.rc == YVEX_OK) import_forward(&b, signature, results);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_seal(b.module, err);
    if (b.rc == YVEX_OK) b.rc = yvex_program_execution_compile(&execution, b.module, err);
    if (b.rc == YVEX_OK) b.rc = yvex_program_physical_compile(out, execution, "forward", b.bindings,
        b.binding_count, p->identity, err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&b.module);
    free(results); free(signature); free(b.bindings);
    return b.rc;
}
