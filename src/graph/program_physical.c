/* Legalize computational entries into typed physical work and reusable slots. */
#include <yvex/internal/program_physical.h>
#include <yvex/internal/execution.h>
#include <yvex/qtype.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHYSICAL_LIMIT 1048576u
#define PHYSICAL_BYTES (256u * 1024u * 1024u)

struct yvex_program_physical {
    yvex_program_physical_summary summary;
    yvex_program_physical_value *values;
    yvex_program_physical_step *steps;
    yvex_ir_id *results;
    yvex_gated_delta_plan **delta;
    yvex_sequence_state_binding *sequence;
    size_t sequence_count;
};

typedef struct {
    const char *semantic, *implementation;
    unsigned int effects;
} physical_rule;

static const physical_rule physical_rules[] = {
    {"core.parameter", "parameter.encoded.v1", 0u},
    {"nn.embedding", "embedding.bf16.v1", 0u},
    {"nn.linear", "linear.bf16.f32acc.v1", 0u},
    {"nn.linear", "linear.encoded.f32.v1", 0u},
    {"mhc.head_norm", "mhc.head_norm.bf16.v1", 0u},
    {"nn.rms_norm", "rms_norm.bf16.v1", 0u},
    {"nn.silu_product", "silu_product.bf16.v1", 0u},
    {"tensor.add", "add.bf16.v1", 0u},
    {"sequence.gated_delta", "gated_delta.bf16.f32state.v1", YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE},
    {"attention.gated_causal", "gated_causal.bf16.v1", YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE}};

static int physical_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.program.physical", reason);
    return status;
}

static int physical_seal(yvex_program_physical *, yvex_error *);

static const physical_rule *physical_rule_find(const char *name, int lowered)
{
    size_t i;
    for (i = 0u; name && i < sizeof(physical_rules) / sizeof(physical_rules[0]); ++i)
        if (!strcmp(name, lowered ? physical_rules[i].implementation : physical_rules[i].semantic))
            return &physical_rules[i];
    return NULL;
}

/* Implementation matching is a compiler decision. A semantic F32 result must
 * never silently use the BF16-publication implementation (or vice versa). */
static const physical_rule *physical_rule_select(const yvex_program_physical *p,
    const char *semantic, const yvex_ir_id *results, size_t result_count)
{
    if (!strcmp(semantic, "nn.linear") && result_count == 1u &&
        p->values[results[0]].type.scalar == YVEX_IR_F32)
        return physical_rule_find("linear.encoded.f32.v1", 1);
    return physical_rule_find(semantic, 0);
}

static int physical_numeric_verify(const yvex_program_physical *p,
    const yvex_program_physical_step *s, yvex_error *err)
{
    unsigned int i;
    int encoded = !strcmp(s->implementation, "linear.encoded.f32.v1");
    if (!strcmp(s->implementation, "parameter.encoded.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "mhc.head_norm.bf16.v1")) {
        for (i = 1u; i < s->operand_count; ++i)
            if (!p->values[s->operands[i]].parameter)
                return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "mHC physical head requires immutable parameters");
        return YVEX_OK; /* The semantic verifier checks every scalar and shape. */
    }
    if (encoded && (s->operand_count != 2u || s->result_count != 1u ||
        p->values[s->operands[0]].type.rank != 2u ||
        !p->values[s->operands[1]].parameter || p->values[s->results[0]].type.rank != 2u))
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
            "encoded linear requires a matrix input and an immutable parameter");
    for (i = 0u; i < s->operand_count; ++i) {
        const yvex_program_physical_value *v = &p->values[s->operands[i]];
        if (v->type.kind != YVEX_IR_TENSOR || v->type.scalar == YVEX_IR_INDEX) continue;
        if ((!encoded && v->type.scalar != YVEX_IR_BF16) ||
            (v->parameter && !encoded && v->qtype != YVEX_GGUF_QTYPE_BF16))
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                "operand precision requires another physical implementation");
    }
    for (i = 0u; i < s->result_count; ++i) {
        const yvex_program_physical_value *v = &p->values[s->results[i]];
        if (v->type.kind == YVEX_IR_TENSOR && v->type.scalar != (encoded ? YVEX_IR_F32 : YVEX_IR_BF16))
            return physical_refuse(err, YVEX_ERR_FORMAT,
                "physical implementation does not preserve result precision");
    }
    return YVEX_OK;
}

static int physical_allocate(yvex_program_physical **out, const yvex_program_physical_summary *s,
                             yvex_error *err)
{
    yvex_program_physical *p;
    if (!s->input_count || s->input_count > s->value_count || !s->step_count || !s->result_count ||
        s->value_count > PHYSICAL_LIMIT || s->step_count > PHYSICAL_LIMIT ||
        s->result_count > s->value_count || s->storage_count > s->value_count)
        return physical_refuse(err, YVEX_ERR_BOUNDS, "physical program population exceeds its envelope");
    p = calloc(1u, sizeof(*p));
    if (!p) return physical_refuse(err, YVEX_ERR_NOMEM, "physical program owner allocation failed");
    p->summary = *s;
    p->values = calloc(s->value_count, sizeof(*p->values));
    p->steps = calloc(s->step_count, sizeof(*p->steps));
    p->results = calloc(s->result_count, sizeof(*p->results));
    if (!p->values || !p->steps || !p->results) {
        yvex_program_physical_close(&p);
        return physical_refuse(err, YVEX_ERR_NOMEM, "physical program records allocation failed");
    }
    *out = p;
    return YVEX_OK;
}

static int physical_type_equal(const yvex_ir_type *a, const yvex_ir_type *b)
{
    unsigned int i;
    if (a->kind != b->kind || a->scalar != b->scalar || a->rank != b->rank ||
        strcmp(a->domain, b->domain)) return 0;
    for (i = 0u; i < a->rank; ++i)
        if (a->shape[i].symbol != b->shape[i].symbol || a->shape[i].extent != b->shape[i].extent) return 0;
    return 1;
}

static int physical_state_roots_verify(const yvex_program_physical *p, yvex_error *err)
{
    size_t i, j, k;
    for (i = 0u; i < p->summary.value_count; ++i) {
        const yvex_program_physical_value *v = &p->values[i];
        if ((v->type.kind != YVEX_IR_STATE && v->state_root != YVEX_IR_NONE) ||
            (v->type.kind == YVEX_IR_STATE && i < p->summary.input_count && v->state_root != i))
            return physical_refuse(err, YVEX_ERR_FORMAT, "physical state input lifetime is inconsistent");
    }
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_physical_step *step = &p->steps[i];
        for (j = 0u; j < step->result_count; ++j) {
            const yvex_program_physical_value *v = &p->values[step->results[j]];
            size_t matches = 0u;
            if (v->type.kind != YVEX_IR_STATE) continue;
            for (k = 0u; k < step->operand_count; ++k) {
                const yvex_program_physical_value *input = &p->values[step->operands[k]];
                if (!physical_type_equal(&input->type, &v->type)) continue;
                if (input->state_root != v->state_root)
                    return physical_refuse(err, YVEX_ERR_FORMAT, "physical state successor changes its lifetime");
                matches++;
            }
            if (matches != 1u || v->state_root >= p->summary.input_count)
                return physical_refuse(err, YVEX_ERR_FORMAT, "physical state successor lacks a unique input lifetime");
        }
    }
    return YVEX_OK;
}

static int physical_state_lower(yvex_program_physical *p, yvex_error *err)
{
    size_t i;
    p->delta = calloc(p->summary.step_count, sizeof(*p->delta));
    p->sequence = calloc(p->summary.input_count, sizeof(*p->sequence));
    if (!p->delta || !p->sequence)
        return physical_refuse(err, YVEX_ERR_NOMEM, "state implementation projection allocation failed");
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_physical_step *s = &p->steps[i];
        yvex_gated_delta_requirement r = {.schema_version = YVEX_SEQUENCE_MIXER_GATED_DELTA_SCHEMA_V2,
            .projected_dtype = YVEX_DTYPE_F32, .convolution_state_dtype = YVEX_DTYPE_F32,
            .recurrent_state_dtype = YVEX_DTYPE_F32, .accumulation_dtype = YVEX_DTYPE_F32,
            .output_dtype = YVEX_DTYPE_F32, .numeric_contract = YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE,
            .output_normalization_weight_convention = YVEX_NORMALIZATION_WEIGHT_DIRECT, .deterministic = 1};
        yvex_gated_delta_plan *d;
        yvex_ir_id root;
        int rc;
        if (strcmp(s->implementation, "gated_delta.bf16.f32state.v1")) continue;
        r.query_heads = r.key_heads = yvex_program_physical_attribute(s, "key_heads")->value.integer;
        r.value_heads = yvex_program_physical_attribute(s, "value_heads")->value.integer;
        r.key_head_dimension = yvex_program_physical_attribute(s, "key_dimension")->value.integer;
        r.value_head_dimension = yvex_program_physical_attribute(s, "value_dimension")->value.integer;
        r.convolution_kernel = yvex_program_physical_attribute(s, "convolution_kernel")->value.integer;
        r.qk_normalization_epsilon = yvex_program_physical_attribute(s, "qk_epsilon")->value.real;
        r.output_normalization_epsilon = yvex_program_physical_attribute(s, "epsilon")->value.real;
        r.query_scale = yvex_program_physical_attribute(s, "query_scale")->value.real;
        d = p->delta[i] = calloc(1u, sizeof(*d));
        if (!d) return physical_refuse(err, YVEX_ERR_NOMEM, "state implementation allocation failed");
        root = p->values[s->operands[8]].state_root;
        rc = yvex_gated_delta_plan_seal(d, &r, err);
        if (rc == YVEX_OK) rc = yvex_sequence_state_binding_seal(&p->sequence[p->sequence_count], root,
            d->convolution_state_values, d->recurrent_state_values, d->identity, err);
        if (rc != YVEX_OK) return rc;
        p->sequence_count++;
    }
    return YVEX_OK;
}

static int physical_state_transactions_verify(const yvex_program_physical *p, yvex_error *err)
{
    unsigned char *written = calloc(p->summary.input_count, 1u);
    size_t i, j;
    int rc = YVEX_OK;
    if (!written) return physical_refuse(err, YVEX_ERR_NOMEM, "state legalization allocation failed");
    /* Current providers stage one successor per root per invocation. Multiple
     * SSA transitions remain valid semantic IR, but require a different physical
     * implementation that can read its preceding candidate, not committed state. */
    for (i = 0u; rc == YVEX_OK && i < p->summary.step_count; ++i)
        for (j = 0u; rc == YVEX_OK && j < p->steps[i].result_count; ++j) {
            const yvex_program_physical_value *v = &p->values[p->steps[i].results[j]];
            if (v->type.kind != YVEX_IR_STATE) continue;
            if (written[v->state_root]) rc = physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                "multiple transitions of one state lifetime require another physical legalization");
            written[v->state_root] = 1u;
        }
    free(written);
    return rc;
}

static int physical_is_result(const yvex_program_physical *p, size_t value)
{
    size_t i;
    for (i = 0u; i < p->summary.result_count; ++i)
        if (p->results[i] == value) return 1;
    return 0;
}

/* Storage is an explicit compiler decision, not an inference performed during
 * invocation. Values alive at the same instruction never share writable slots.
 * Outputs stay caller-owned; state versions retain their original state root. */
static int physical_storage_plan(yvex_program_physical *p, int verify, yvex_error *err)
{
    yvex_ir_id *owners;
    size_t i, count = 0u;
    owners = malloc(p->summary.value_count * sizeof(*owners));
    if (!owners) return physical_refuse(err, YVEX_ERR_NOMEM, "storage planning allocation failed");
    for (i = 0u; i < p->summary.value_count; ++i) {
        yvex_program_physical_value *v = &p->values[i];
        yvex_ir_id slot = YVEX_IR_NONE;
        if (i >= p->summary.input_count && !v->parameter && v->type.kind == YVEX_IR_TENSOR &&
            !physical_is_result(p, i)) {
            size_t j;
            for (j = 0u; j < count; ++j) {
                const yvex_program_physical_value *prior = &p->values[owners[j]];
                if (prior->last_use < v->definition && physical_type_equal(&prior->type, &v->type)) break;
            }
            if (j == count) count++;
            slot = (yvex_ir_id)j;
            owners[j] = (yvex_ir_id)i;
        }
        if (verify && v->storage != slot) {
            free(owners);
            return physical_refuse(err, YVEX_ERR_FORMAT, "physical storage aliases live computational values");
        }
        if (!verify) v->storage = slot;
    }
    free(owners);
    if (verify && p->summary.storage_count != count)
        return physical_refuse(err, YVEX_ERR_FORMAT, "physical storage population is inconsistent");
    if (!verify) p->summary.storage_count = count;
    return YVEX_OK;
}

static int physical_values_lower(yvex_program_physical *p, const yvex_ir_module *m,
    const yvex_program_entry *entry, const yvex_program_parameter_binding *bindings,
    size_t binding_count, yvex_error *err)
{
    yvex_ir_id population = YVEX_IR_NONE;
    size_t i;
    for (i = 0u; i < entry->value_count; ++i) {
        yvex_program_physical_value *v = &p->values[i];
        const yvex_ir_value *logical = yvex_ir_value_at(m, entry->values[i].semantic_value);
        const yvex_ir_type *type = logical ? yvex_ir_type_at(m, logical->type) : NULL;
        const yvex_ir_operation *producer = logical ? yvex_ir_operation_at(m, logical->definition) : NULL;
        unsigned int axis;
        if (!type || type->member_count || (type->kind != YVEX_IR_TENSOR &&
            type->kind != YVEX_IR_STATE && type->kind != YVEX_IR_SCALAR) ||
            (type->kind == YVEX_IR_TENSOR && type->scalar != YVEX_IR_BF16 && type->scalar != YVEX_IR_F32 &&
             !(i < entry->input_count && type->scalar == YVEX_IR_INDEX && type->rank == 1u)) ||
            (type->kind == YVEX_IR_SCALAR && type->scalar != YVEX_IR_INDEX))
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "value type has no physical execution representation");
        v->type = *type;
        v->definition = entry->values[i].definition;
        v->last_use = entry->values[i].last_use;
        v->storage = v->state_root = YVEX_IR_NONE;
        v->tensor_id = ULLONG_MAX;
        if (type->kind == YVEX_IR_STATE && i < entry->input_count) v->state_root = (yvex_ir_id)i;
        for (axis = 0u; axis < type->rank; ++axis) {
            const yvex_ir_extent *extent = &type->shape[axis];
            if (extent->symbol != YVEX_IR_NONE) {
                const yvex_ir_dimension *d = yvex_ir_dimension_at(m, extent->symbol);
                if (axis || !d || type->kind != YVEX_IR_TENSOR ||
                    (population != YVEX_IR_NONE && population != extent->symbol))
                    return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                        "physical execution requires one shared row population");
                population = extent->symbol;
                p->summary.minimum_rows = d->minimum;
                p->summary.maximum_rows = d->maximum;
                p->summary.row_multiple = d->multiple;
                v->type.shape[axis].symbol = 0u;
            }
        }
        if (producer && !strcmp(producer->definition->name, "core.parameter")) {
            size_t j, matches = 0u;
            v->parameter = 1;
            for (j = 0u; j < binding_count; ++j)
                if (bindings[j].semantic_value == entry->values[i].semantic_value) {
                    v->tensor_id = bindings[j].tensor_id;
                    v->qtype = bindings[j].qtype;
                    matches++;
                }
            if (matches != 1u || v->tensor_id == ULLONG_MAX)
                return physical_refuse(err, YVEX_ERR_FORMAT, "constant requires one exact physical parameter binding");
        }
    }
    if (population == YVEX_IR_NONE)
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "physical entry has no admitted execution population");
    return YVEX_OK;
}

static int physical_steps_lower(yvex_program_physical *p, const yvex_ir_module *m,
    const yvex_program_entry *entry, yvex_error *err)
{
    size_t i, j;
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_step *source = &entry->steps[i];
        const yvex_ir_operation *op = yvex_ir_operation_at(m, source->semantic_operation);
        const physical_rule *rule = op ? physical_rule_select(p, op->definition->name,
            source->results, source->result_count) : NULL;
        yvex_program_physical_step *step = &p->steps[i];
        int parameter = rule && !strcmp(rule->semantic, "core.parameter");
        if (!rule || op->definition->version != 1u ||
            source->operand_count > YVEX_PROGRAM_OPERAND_CAP || source->result_count > YVEX_PROGRAM_RESULT_CAP ||
            (!parameter && op->attribute_count > YVEX_PROGRAM_ATTRIBUTE_CAP) ||
            yvex_ir_operation_effects(m, source->semantic_operation) != rule->effects)
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "operation lacks an admitted physical implementation");
        step->implementation = rule->implementation;
        step->effects = rule->effects;
        step->operand_count = (unsigned int)source->operand_count;
        step->result_count = (unsigned int)source->result_count;
        step->attribute_count = parameter ? 0u : op->attribute_count;
        memcpy(step->operands, source->operands, source->operand_count * sizeof(*step->operands));
        memcpy(step->results, source->results, source->result_count * sizeof(*step->results));
        if (step->attribute_count)
            memcpy(step->attributes, op->attributes, step->attribute_count * sizeof(*step->attributes));
        for (j = 0u; j < source->result_count; ++j) {
            yvex_program_physical_value *result = &p->values[source->results[j]];
            size_t k, matches = 0u;
            if (result->type.kind != YVEX_IR_STATE) continue;
            for (k = 0u; k < source->operand_count; ++k) {
                const yvex_program_physical_value *input = &p->values[source->operands[k]];
                if (physical_type_equal(&input->type, &result->type)) {
                    result->state_root = input->state_root;
                    matches++;
                }
            }
            if (matches != 1u || result->state_root == YVEX_IR_NONE)
                return physical_refuse(err, YVEX_ERR_FORMAT, "state result requires an unambiguous input lifetime");
        }
    }
    return YVEX_OK;
}

/* Reuse the operation/type/state verifiers at the physical trust boundary.
 * This transient verification object is discarded before execution; it never
 * legalizes work, imports a family, or becomes a second retained program. */
static int physical_verify(yvex_program_physical *p, yvex_error *err)
{
    const yvex_program_physical_summary *s = &p->summary;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect(), *yvex_ir_sequence_dialect()};
    yvex_ir_dimension rows = {.name = "rows", .minimum = s->minimum_rows,
        .maximum = s->maximum_rows, .multiple = s->row_multiple};
    yvex_ir_module *m = NULL;
    yvex_ir_id dimension, function, block = YVEX_IR_NONE, opid;
    yvex_ir_id *types = NULL, *values = NULL, *results = NULL;
    yvex_ir_id *last = NULL;
    size_t i, j, defined = s->input_count;
    unsigned int effects = 0u;
    int rc;
    if (!s->entry[0] || !yvex_sha256_hex_valid(s->semantic_identity) ||
        !yvex_sha256_hex_valid(s->execution_identity) || !yvex_sha256_hex_valid(s->parameter_identity) ||
        s->maximum_rows > INT_MAX)
        return physical_refuse(err, YVEX_ERR_FORMAT, "physical program identity/envelope is invalid");
    rc = yvex_ir_module_open(&m, "physical_verifier", s->semantic_identity, dialects, 3u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dimension, err);
    types = malloc(s->value_count * sizeof(*types));
    values = malloc(s->value_count * sizeof(*values));
    last = malloc(s->value_count * sizeof(*last));
    results = malloc(s->result_count * sizeof(*results));
    if (rc == YVEX_OK && (!types || !values || !last || !results))
        rc = physical_refuse(err, YVEX_ERR_NOMEM, "physical verification storage allocation failed");
    for (i = 0u; rc == YVEX_OK && i < s->value_count; ++i) {
        const yvex_program_physical_value *v = &p->values[i];
        last[i] = YVEX_IR_NONE;
        if (v->type.rank > YVEX_IR_RANK_CAP || v->type.member_count ||
            (v->type.kind != YVEX_IR_TENSOR && v->type.kind != YVEX_IR_STATE && v->type.kind != YVEX_IR_SCALAR) ||
            (v->type.kind == YVEX_IR_TENSOR && v->type.scalar != YVEX_IR_BF16 && v->type.scalar != YVEX_IR_F32 &&
             !(i < s->input_count && v->type.scalar == YVEX_IR_INDEX && v->type.rank == 1u)) ||
            (v->type.kind == YVEX_IR_SCALAR && v->type.scalar != YVEX_IR_INDEX) ||
            (v->parameter != 0 && v->parameter != 1) ||
            (v->parameter && (!v->type.rank || v->type.kind != YVEX_IR_TENSOR ||
                              !yvex_gguf_qtype_geometry_find(v->qtype) || v->tensor_id == ULLONG_MAX)) ||
            (!v->parameter && (v->tensor_id != ULLONG_MAX || v->qtype)) ||
            (i < s->input_count && (v->parameter || v->definition != YVEX_IR_NONE))) {
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical value storage/definition is invalid");
            break;
        }
        for (j = 0u; j < v->type.rank; ++j)
            if (v->type.shape[j].symbol != YVEX_IR_NONE &&
                (j || v->type.shape[j].symbol != 0u || v->parameter || v->type.kind != YVEX_IR_TENSOR))
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical value has an unlowered shape symbol");
        if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &v->type, &types[i], err);
    }
    for (i = 0u; rc == YVEX_OK && i < s->result_count; ++i) {
        if (p->results[i] >= s->value_count || p->values[p->results[i]].parameter)
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical entry result is invalid");
        else results[i] = types[p->results[i]];
        for (j = 0u; rc == YVEX_OK && j < i; ++j)
            if (p->results[j] == p->results[i])
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "duplicate external result requires explicit copy lowering");
    }
    for (i = 0u; i < s->step_count; ++i) effects |= p->steps[i].effects;
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, s->entry, types, s->input_count,
        results, s->result_count, effects, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        memcpy(values, yvex_ir_block_at(m, block)->arguments, s->input_count * sizeof(*values));
    }
    for (i = 0u; rc == YVEX_OK && i < s->step_count; ++i) {
        const yvex_program_physical_step *step = &p->steps[i];
        const physical_rule *rule = physical_rule_find(step->implementation, 1);
        yvex_ir_id args[YVEX_PROGRAM_OPERAND_CAP], output[YVEX_PROGRAM_RESULT_CAP];
        yvex_ir_attribute attrs[2] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
                                    {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL}};
        yvex_ir_operation_request r = {.operands = args, .result_types = output,
            .operand_count = step->operand_count, .result_count = step->result_count,
            .attributes = step->attributes, .attribute_count = step->attribute_count};
        if (!rule || step->effects != rule->effects || step->operand_count > YVEX_PROGRAM_OPERAND_CAP ||
            step->result_count > YVEX_PROGRAM_RESULT_CAP || step->attribute_count > YVEX_PROGRAM_ATTRIBUTE_CAP)
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical operation/effects are not admitted");
        if (rc != YVEX_OK) break;
        r.operation = rule->semantic;
        for (j = 0u; rc == YVEX_OK && j < step->operand_count; ++j) {
            if (step->operands[j] >= defined)
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical operand has no preceding definition");
            else { args[j] = values[step->operands[j]]; last[step->operands[j]] = (yvex_ir_id)i; }
        }
        for (j = 0u; rc == YVEX_OK && j < step->result_count; ++j) {
            if (defined >= s->value_count || step->results[j] != defined || p->values[defined].definition != i)
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical result definition is not unique");
            else output[j] = types[defined++];
        }
        if (rc == YVEX_OK && !strcmp(rule->semantic, "core.parameter")) {
            if (step->result_count != 1u || step->attribute_count ||
                !p->values[step->results[0]].parameter) rc = YVEX_ERR_FORMAT;
            if (rc == YVEX_OK) {
                yvex_core_text_copy(attrs[0].value.text, sizeof(attrs[0].value.text), s->parameter_identity);
                (void)snprintf(attrs[1].value.text, sizeof(attrs[1].value.text), "tensor_%llu",
                               p->values[step->results[0]].tensor_id);
                r.attributes = attrs;
                r.attribute_count = 2u;
            }
        } else for (j = 0u; rc == YVEX_OK && j < step->result_count; ++j)
            if (p->values[step->results[j]].parameter) rc = YVEX_ERR_FORMAT;
        if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, block, &r, &opid, err);
        if (rc == YVEX_OK) rc = physical_numeric_verify(p, step, err);
        for (j = 0u; rc == YVEX_OK && j < step->result_count; ++j)
            values[step->results[j]] = yvex_ir_operation_at(m, opid)->results[j];
    }
    if (rc == YVEX_OK && defined != s->value_count)
        rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical program contains undefined values");
    for (i = 0u; rc == YVEX_OK && i < s->result_count; ++i) {
        results[i] = values[p->results[i]];
        last[p->results[i]] = (yvex_ir_id)s->step_count;
    }
    for (i = 0u; rc == YVEX_OK && i < s->value_count; ++i)
        if (last[i] != p->values[i].last_use)
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical value lifetime differs from its uses");
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = results,
            .operand_count = (uint32_t)s->result_count};
        rc = yvex_ir_operation_add(m, block, &r, &opid, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = physical_state_roots_verify(p, err);
    if (rc == YVEX_OK) rc = physical_state_transactions_verify(p, err);
    if (rc == YVEX_OK) rc = physical_storage_plan(p, 1, err);
    if (rc == YVEX_OK) rc = physical_state_lower(p, err);
    free(results);
    free(last);
    free(values);
    free(types);
    yvex_ir_module_close(&m);
    return rc;
}

int yvex_program_physical_compile(yvex_program_physical **out, const yvex_program_execution *execution,
    const char *symbol, const yvex_program_parameter_binding *bindings, size_t binding_count,
    const char *parameter_identity, yvex_error *err)
{
    const yvex_ir_module *m = yvex_program_execution_module(execution);
    const yvex_program_entry *entry = NULL;
    yvex_program_physical *p = NULL;
    yvex_program_physical_summary s = {0};
    size_t i;
    int rc;
    if (out) *out = NULL;
    if (!out || !m || !symbol || !yvex_sha256_hex_valid(parameter_identity) || (binding_count && !bindings))
        return physical_refuse(err, YVEX_ERR_INVALID_ARG, "verified execution and exact parameter bindings required");
    for (i = 0u; i < yvex_program_execution_entry_count(execution); ++i) {
        const yvex_program_entry *e = yvex_program_execution_entry_at(execution, i);
        if (!strcmp(e->symbol, symbol)) entry = e;
    }
    if (!entry || entry->step_count < 2u)
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "physical entry has no admitted computation");
    yvex_core_text_copy(s.entry, sizeof(s.entry), symbol);
    yvex_core_text_copy(s.semantic_identity, sizeof(s.semantic_identity), yvex_ir_identity(m));
    yvex_core_text_copy(s.execution_identity, sizeof(s.execution_identity), yvex_program_execution_identity(execution));
    yvex_core_text_copy(s.parameter_identity, sizeof(s.parameter_identity), parameter_identity);
    s.input_count = entry->input_count;
    s.value_count = entry->value_count;
    s.step_count = entry->step_count - 1u;
    s.result_count = entry->result_count;
    rc = physical_allocate(&p, &s, err);
    if (rc == YVEX_OK) {
        memcpy(p->results, entry->results, s.result_count * sizeof(*p->results));
        rc = physical_values_lower(p, m, entry, bindings, binding_count, err);
    }
    if (rc == YVEX_OK) rc = physical_steps_lower(p, m, entry, err);
    if (rc == YVEX_OK) rc = physical_storage_plan(p, 0, err);
    if (rc == YVEX_OK) rc = physical_verify(p, err);
    if (rc == YVEX_OK) rc = physical_seal(p, err);
    if (rc == YVEX_OK) *out = p;
    else yvex_program_physical_close(&p);
    return rc;
}

void yvex_program_physical_close(yvex_program_physical **owner)
{
    yvex_program_physical *p = owner ? *owner : NULL;
    size_t i;
    if (!p) return;
    for (i = 0u; p->delta && i < p->summary.step_count; ++i) free(p->delta[i]);
    free(p->sequence);
    free(p->delta);
    free(p->values);
    free(p->steps);
    free(p->results);
    free(p);
    *owner = NULL;
}

const yvex_program_physical_summary *yvex_program_physical_summary_get(const yvex_program_physical *p)
{ return p ? &p->summary : NULL; }
const yvex_program_physical_value *yvex_program_physical_value_at(const yvex_program_physical *p, size_t i)
{ return p && i < p->summary.value_count ? &p->values[i] : NULL; }
const yvex_program_physical_step *yvex_program_physical_step_at(const yvex_program_physical *p, size_t i)
{ return p && i < p->summary.step_count ? &p->steps[i] : NULL; }
yvex_ir_id yvex_program_physical_result_at(const yvex_program_physical *p, size_t i)
{ return p && i < p->summary.result_count ? p->results[i] : YVEX_IR_NONE; }
const yvex_ir_attribute *yvex_program_physical_attribute(const yvex_program_physical_step *s, const char *name)
{
    unsigned int i;
    for (i = 0u; s && name && i < s->attribute_count; ++i)
        if (!strcmp(s->attributes[i].name, name)) return &s->attributes[i];
    return NULL;
}

const yvex_gated_delta_plan *yvex_program_physical_delta_at(const yvex_program_physical *p, size_t i)
{
    return p && p->delta && i < p->summary.step_count ? p->delta[i] : NULL;
}

int yvex_program_physical_sequence_state(const yvex_program_physical *p, yvex_sequence_state_plan *out)
{
    if (!p || !out) return 0;
    *out = (yvex_sequence_state_plan){YVEX_SEQUENCE_STATE_SCHEMA_V1, p->sequence, p->sequence_count};
    return 1;
}

static int physical_put(yvex_core_bytes *b, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;
    for (i = 0u; i < 8u; ++i) bytes[i] = (unsigned char)(value >> (8u * i));
    return yvex_core_bytes_append(b, bytes, sizeof(bytes));
}

static int physical_text(yvex_core_bytes *b, const char *text)
{
    size_t n = strlen(text);
    return physical_put(b, n) && yvex_core_bytes_append(b, text, n);
}

static int physical_value_write(yvex_core_bytes *b, const yvex_program_physical_value *v)
{
    unsigned int i;
    int ok = physical_put(b, v->type.kind) && physical_put(b, v->type.scalar) &&
        physical_put(b, v->type.rank) && physical_text(b, v->type.domain) &&
        physical_put(b, v->definition) && physical_put(b, v->last_use) &&
        physical_put(b, v->storage) && physical_put(b, v->state_root) &&
        physical_put(b, v->tensor_id) && physical_put(b, v->qtype) && physical_put(b, (unsigned int)v->parameter);
    for (i = 0u; ok && i < v->type.rank; ++i)
        ok = physical_put(b, v->type.shape[i].symbol) && physical_put(b, v->type.shape[i].extent);
    return ok;
}

static int physical_step_write(yvex_core_bytes *b, const yvex_program_physical_step *s)
{
    unsigned int i;
    int ok = physical_text(b, s->implementation) && physical_put(b, s->effects) &&
        physical_put(b, s->operand_count) && physical_put(b, s->result_count) && physical_put(b, s->attribute_count);
    for (i = 0u; ok && i < s->operand_count; ++i) ok = physical_put(b, s->operands[i]);
    for (i = 0u; ok && i < s->result_count; ++i) ok = physical_put(b, s->results[i]);
    for (i = 0u; ok && i < s->attribute_count; ++i) {
        const yvex_ir_attribute *a = &s->attributes[i];
        uint64_t bits = a->value.integer;
        if (a->kind == YVEX_IR_ATTR_F64) memcpy(&bits, &a->value.real, sizeof(bits));
        else if (a->kind != YVEX_IR_ATTR_U64 && a->kind != YVEX_IR_ATTR_BOOL) return 0;
        ok = physical_text(b, a->name) && physical_put(b, a->kind) && physical_put(b, bits);
    }
    return ok;
}

static int physical_records(const yvex_program_physical *p, yvex_core_bytes *b)
{
    const yvex_program_physical_summary *s = &p->summary;
    size_t i;
    int ok = physical_text(b, "yvex.program.physical.v1") && physical_text(b, s->entry) &&
        physical_text(b, s->semantic_identity) && physical_text(b, s->execution_identity) &&
        physical_text(b, s->parameter_identity) && physical_put(b, s->minimum_rows) &&
        physical_put(b, s->maximum_rows) && physical_put(b, s->row_multiple) &&
        physical_put(b, s->input_count) && physical_put(b, s->value_count) && physical_put(b, s->step_count) &&
        physical_put(b, s->result_count) && physical_put(b, s->storage_count);
    for (i = 0u; ok && i < s->value_count; ++i) ok = physical_value_write(b, &p->values[i]);
    for (i = 0u; ok && i < s->step_count; ++i) ok = physical_step_write(b, &p->steps[i]);
    for (i = 0u; ok && i < s->result_count; ++i) ok = physical_put(b, p->results[i]);
    return ok;
}

static int physical_digest(const yvex_core_bytes *b, char out[YVEX_SHA256_HEX_BYTES])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, b->data, b->count) || !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}

int yvex_program_physical_parameters_validate(const yvex_program_physical *p,
    const yvex_physical_execution_ir *parameters, yvex_error *err)
{
    const yvex_physical_execution_summary *summary = yvex_physical_execution_ir_summary(parameters);
    size_t i;
    if (!p || !summary || strcmp(p->summary.parameter_identity, summary->identity))
        return physical_refuse(err, YVEX_ERR_FORMAT, "program requires its authenticated physical parameters");
    for (i = 0u; i < p->summary.value_count; ++i) {
        const yvex_program_physical_value *v = &p->values[i];
        const yvex_physical_execution_decision *found = NULL;
        const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(v->qtype);
        unsigned long long j, elements = 1u, bytes;
        unsigned int axis;
        size_t matches = 0u;
        if (!v->parameter) continue;
        for (j = 0u; j < summary->decision_count; ++j) {
            const yvex_physical_execution_decision *d = yvex_physical_execution_ir_decision_at(parameters, j);
            if (d->terminal_tensor_id == v->tensor_id) { found = d; matches++; }
        }
        for (axis = 0u; axis < v->type.rank; ++axis)
            if (!yvex_core_u64_mul(elements, v->type.shape[axis].extent, &elements))
                return physical_refuse(err, YVEX_ERR_BOUNDS, "program parameter geometry overflows");
        if (!v->type.rank || matches != 1u || !found || found->canonical_qtype != v->qtype ||
            found->canonical_row_width != v->type.shape[v->type.rank - 1u].extent ||
            !found->canonical_row_width || found->canonical_row_count != elements / found->canonical_row_width ||
            !geometry || !geometry->block_size || !geometry->bytes_per_block ||
            found->canonical_row_width % geometry->block_size ||
            !yvex_core_u64_mul(elements / geometry->block_size, geometry->bytes_per_block, &bytes) ||
            found->encoded_bytes != bytes)
            return physical_refuse(err, YVEX_ERR_FORMAT,
                "program parameter shape/storage differs from physical binding");
    }
    return YVEX_OK;
}

static int physical_seal(yvex_program_physical *p, yvex_error *err)
{
    yvex_core_bytes b = {.maximum = PHYSICAL_BYTES};
    int ok = physical_records(p, &b) && physical_digest(&b, p->summary.identity);
    free(b.data);
    return ok ? YVEX_OK : physical_refuse(err, YVEX_ERR_NOMEM, "physical program identity encoding failed");
}

int yvex_program_physical_encode(const yvex_program_physical *p, yvex_core_bytes *out, yvex_error *err)
{
    yvex_core_bytes b = {.maximum = PHYSICAL_BYTES};
    char identity[YVEX_SHA256_HEX_BYTES];
    int ok = p && out && physical_records(p, &b) && physical_digest(&b, identity) &&
        !strcmp(identity, p->summary.identity) && physical_text(&b, identity) &&
        yvex_core_bytes_append(out, b.data, b.count);
    free(b.data);
    return ok ? YVEX_OK : physical_refuse(err, YVEX_ERR_FORMAT, "physical program encoding/identity is invalid");
}

typedef struct { const unsigned char *data; size_t count, offset; } physical_cursor;

static int physical_get(physical_cursor *c, unsigned long long *value)
{
    unsigned int i;
    if (c->offset > c->count || c->count - c->offset < 8u) return 0;
    *value = 0ull;
    for (i = 0u; i < 8u; ++i) *value |= (unsigned long long)c->data[c->offset++] << (8u * i);
    return 1;
}

static int physical_get_id(physical_cursor *c, unsigned int *out)
{
    unsigned long long value;
    if (!physical_get(c, &value) || value > UINT32_MAX) return 0;
    *out = (unsigned int)value;
    return 1;
}

static int physical_get_text(physical_cursor *c, char *out, size_t capacity)
{
    unsigned long long n;
    if (!physical_get(c, &n) || n >= capacity || n > c->count - c->offset ||
        memchr(c->data + c->offset, 0, (size_t)n)) return 0;
    memcpy(out, c->data + c->offset, (size_t)n);
    out[n] = '\0';
    c->offset += (size_t)n;
    return 1;
}

static int physical_value_read(physical_cursor *c, yvex_program_physical_value *v)
{
    unsigned int kind, scalar, parameter, axis;
    if (!physical_get_id(c, &kind) || !physical_get_id(c, &scalar) || !physical_get_id(c, &v->type.rank) ||
        v->type.rank > YVEX_IR_RANK_CAP || !physical_get_text(c, v->type.domain, sizeof(v->type.domain)) ||
        !physical_get_id(c, &v->definition) || !physical_get_id(c, &v->last_use) ||
        !physical_get_id(c, &v->storage) || !physical_get_id(c, &v->state_root) ||
        !physical_get(c, &v->tensor_id) || !physical_get_id(c, &v->qtype) ||
        !physical_get_id(c, &parameter) || parameter > 1u) return 0;
    v->type.kind = (yvex_ir_type_kind)kind;
    v->type.scalar = (yvex_ir_scalar)scalar;
    v->parameter = (int)parameter;
    for (axis = 0u; axis < v->type.rank; ++axis) {
        unsigned long long extent;
        if (!physical_get_id(c, &v->type.shape[axis].symbol) || !physical_get(c, &extent)) return 0;
        v->type.shape[axis].extent = extent;
    }
    return 1;
}

static int physical_step_read(physical_cursor *c, yvex_program_physical_step *s)
{
    char name[YVEX_IR_NAME_CAP];
    const physical_rule *rule;
    unsigned int i;
    if (!physical_get_text(c, name, sizeof(name)) || !(rule = physical_rule_find(name, 1)) ||
        !physical_get_id(c, &s->effects) || !physical_get_id(c, &s->operand_count) ||
        s->operand_count > YVEX_PROGRAM_OPERAND_CAP || !physical_get_id(c, &s->result_count) ||
        s->result_count > YVEX_PROGRAM_RESULT_CAP || !physical_get_id(c, &s->attribute_count) ||
        s->attribute_count > YVEX_PROGRAM_ATTRIBUTE_CAP) return 0;
    s->implementation = rule->implementation;
    for (i = 0u; i < s->operand_count; ++i) if (!physical_get_id(c, &s->operands[i])) return 0;
    for (i = 0u; i < s->result_count; ++i) if (!physical_get_id(c, &s->results[i])) return 0;
    for (i = 0u; i < s->attribute_count; ++i) {
        yvex_ir_attribute *a = &s->attributes[i];
        unsigned int kind;
        unsigned long long bits;
        if (!physical_get_text(c, a->name, sizeof(a->name)) || !physical_get_id(c, &kind) ||
            !physical_get(c, &bits)) return 0;
        a->kind = (yvex_ir_attribute_kind)kind;
        if (a->kind == YVEX_IR_ATTR_F64) memcpy(&a->value.real, &bits, sizeof(bits));
        else if (a->kind == YVEX_IR_ATTR_U64 || a->kind == YVEX_IR_ATTR_BOOL) a->value.integer = bits;
        else return 0;
    }
    return 1;
}

static int physical_summary_read(physical_cursor *c, yvex_program_physical_summary *s)
{
    unsigned long long populations[5];
    char domain[64];
    size_t i;
    if (!physical_get_text(c, domain, sizeof(domain)) || strcmp(domain, "yvex.program.physical.v1") ||
        !physical_get_text(c, s->entry, sizeof(s->entry)) ||
        !physical_get_text(c, s->semantic_identity, sizeof(s->semantic_identity)) ||
        !physical_get_text(c, s->execution_identity, sizeof(s->execution_identity)) ||
        !physical_get_text(c, s->parameter_identity, sizeof(s->parameter_identity)) ||
        !physical_get(c, &s->minimum_rows) || !physical_get(c, &s->maximum_rows) ||
        !physical_get(c, &s->row_multiple)) return 0;
    for (i = 0u; i < 5u; ++i)
        if (!physical_get(c, &populations[i]) || populations[i] > PHYSICAL_LIMIT) return 0;
    s->input_count = (size_t)populations[0];
    s->value_count = (size_t)populations[1];
    s->step_count = (size_t)populations[2];
    s->result_count = (size_t)populations[3];
    s->storage_count = (size_t)populations[4];
    /* Bound allocation amplification even before semantic verification. */
    return s->value_count <= c->count / 88u && s->step_count <= c->count / 40u;
}

int yvex_program_physical_decode(yvex_program_physical **out, const unsigned char *data,
                                  size_t count, yvex_error *err)
{
    physical_cursor c = {data, count, 0u};
    yvex_program_physical_summary s = {0};
    yvex_program_physical *p = NULL;
    char identity[YVEX_SHA256_HEX_BYTES];
    size_t i;
    int rc;
    if (out) *out = NULL;
    if (!out || !data || !count || count > PHYSICAL_BYTES || !physical_summary_read(&c, &s))
        return physical_refuse(err, YVEX_ERR_FORMAT, "physical program header is malformed");
    rc = physical_allocate(&p, &s, err);
    for (i = 0u; rc == YVEX_OK && i < s.value_count; ++i)
        if (!physical_value_read(&c, &p->values[i])) rc = YVEX_ERR_FORMAT;
    for (i = 0u; rc == YVEX_OK && i < s.step_count; ++i)
        if (!physical_step_read(&c, &p->steps[i])) rc = YVEX_ERR_FORMAT;
    for (i = 0u; rc == YVEX_OK && i < s.result_count; ++i)
        if (!physical_get_id(&c, &p->results[i])) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK && (!physical_get_text(&c, identity, sizeof(identity)) || c.offset != c.count))
        rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) rc = physical_verify(p, err);
    if (rc == YVEX_OK) rc = physical_seal(p, err);
    if (rc == YVEX_OK && strcmp(identity, p->summary.identity)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) *out = p;
    else {
        yvex_program_physical_close(&p);
        if (!yvex_error_is_set(err)) physical_refuse(err, (yvex_status)rc, "physical program records are malformed");
    }
    return rc;
}
