/* Seal typed tensor work independently from model topology and device resources. */
#include <yvex/internal/program.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TENSOR_PLAN_LIMIT 65536u
#define TENSOR_PLAN_BYTES (16u * 1024u * 1024u)

struct yvex_program_tensor_plan {
    yvex_program_tensor_summary summary;
    yvex_program_tensor_value *values;
    yvex_program_tensor_step *steps;
    yvex_ir_id *results;
};

typedef struct {
    const char *semantic, *physical;
    int weight;
} tensor_legalization;

/* Static operation implementations, not architecture/model tags. Extending the
 * set requires a type-checked lowering and an admitted backend implementation. */
static const tensor_legalization tensor_operations[] = {
    {"nn.linear", "linear.bf16.f32acc.v1", 1},
    {"nn.silu_product", "silu_product.bf16.v1", 0},
    {"tensor.add", "add.bf16.v1", 0}};

static int tensor_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "compiler.program.tensor", reason);
    return code;
}

static const tensor_legalization *tensor_operation_find(const char *name, int physical)
{
    size_t i;
    for (i = 0u; name && i < sizeof(tensor_operations) / sizeof(tensor_operations[0]); ++i)
        if (!strcmp(name, physical ? tensor_operations[i].physical : tensor_operations[i].semantic))
            return &tensor_operations[i];
    return NULL;
}

static int tensor_allocate(yvex_program_tensor_plan **out, const yvex_program_tensor_summary *s,
                            yvex_error *err)
{
    yvex_program_tensor_plan *p;
    if (!s->input_count || s->value_count < s->input_count || !s->result_count || !s->step_count ||
        s->value_count > TENSOR_PLAN_LIMIT || s->step_count > TENSOR_PLAN_LIMIT ||
        s->result_count > TENSOR_PLAN_LIMIT)
        return tensor_refuse(err, YVEX_ERR_BOUNDS, "tensor program population is invalid");
    p = calloc(1u, sizeof(*p));
    if (!p) return tensor_refuse(err, YVEX_ERR_NOMEM, "tensor program allocation failed");
    p->summary = *s;
    p->values = calloc(s->value_count, sizeof(*p->values));
    p->steps = calloc(s->step_count, sizeof(*p->steps));
    p->results = calloc(s->result_count, sizeof(*p->results));
    if (!p->values || !p->steps || !p->results) {
        yvex_program_tensor_close(&p);
        return tensor_refuse(err, YVEX_ERR_NOMEM, "tensor program records allocation failed");
    }
    *out = p;
    return YVEX_OK;
}

static int tensor_verify(const yvex_program_tensor_plan *p, yvex_error *err)
{
    const yvex_program_tensor_summary *s = &p->summary;
    size_t i, defined = s->input_count;
    if (!s->entry[0] || !yvex_sha256_hex_valid(s->semantic_identity) ||
        !yvex_sha256_hex_valid(s->execution_identity) || !s->minimum_rows ||
        s->maximum_rows < s->minimum_rows || s->maximum_rows > INT_MAX ||
        !s->row_multiple || s->row_multiple > s->maximum_rows ||
        (s->minimum_rows + s->row_multiple - 1u) / s->row_multiple > s->maximum_rows / s->row_multiple)
        return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor program lineage/population constraint is invalid");
    for (i = 0u; i < s->value_count; ++i) {
        const yvex_program_tensor_value *v = &p->values[i];
        if (!v->width || v->width > INT_MAX || v->rows > INT_MAX ||
            (v->parameter != 0 && v->parameter != 1) ||
            (v->parameter && (i >= s->input_count || !v->rows)) || (!v->parameter && v->rows))
            return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor storage type is invalid");
    }
    for (i = 0u; i < s->step_count; ++i) {
        const yvex_program_tensor_step *step = &p->steps[i];
        const tensor_legalization *op = tensor_operation_find(step->implementation, 1);
        const yvex_program_tensor_value *a, *b, *r;
        if (!op || step->operands[0] >= defined || step->operands[1] >= defined ||
            step->result != defined || defined >= s->value_count)
            return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor work requires defined operands and unique results");
        a = &p->values[step->operands[0]];
        b = &p->values[step->operands[1]];
        r = &p->values[defined++];
        if (a->parameter || r->parameter || b->parameter != op->weight ||
            (op->weight && (a->width != b->width || r->width != b->rows)) ||
            (!op->weight && (a->width != b->width || a->width != r->width)))
            return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor operand/result physical geometry is incompatible");
    }
    if (defined != s->value_count)
        return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor value has no definition");
    for (i = 0u; i < s->result_count; ++i) {
        size_t prior;
        if (p->results[i] < s->input_count || p->results[i] >= defined || p->values[p->results[i]].parameter)
            return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor result must be a computed activation");
        for (prior = 0u; prior < i; ++prior)
            if (p->results[prior] == p->results[i])
                return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor output bindings must be unique");
    }
    return YVEX_OK;
}

static int tensor_put_u64(yvex_core_bytes *bytes, unsigned long long value)
{
    unsigned char data[8];
    unsigned int i;
    for (i = 0u; i < 8u; ++i) data[i] = (unsigned char)(value >> (i * 8u));
    return yvex_core_bytes_append(bytes, data, sizeof(data));
}

static int tensor_put_text(yvex_core_bytes *bytes, const char *text)
{
    return tensor_put_u64(bytes, strlen(text)) && yvex_core_bytes_append(bytes, text, strlen(text));
}

static int tensor_records(const yvex_program_tensor_plan *p, yvex_core_bytes *bytes)
{
    const yvex_program_tensor_summary *s = &p->summary;
    size_t i;
    int ok = tensor_put_text(bytes, "yvex.program.tensor.v1") && tensor_put_text(bytes, s->entry) &&
        tensor_put_text(bytes, s->semantic_identity) && tensor_put_text(bytes, s->execution_identity) &&
        tensor_put_u64(bytes, s->minimum_rows) && tensor_put_u64(bytes, s->maximum_rows) &&
        tensor_put_u64(bytes, s->row_multiple) && tensor_put_u64(bytes, s->input_count) &&
        tensor_put_u64(bytes, s->value_count) && tensor_put_u64(bytes, s->step_count) &&
        tensor_put_u64(bytes, s->result_count);
    for (i = 0u; ok && i < s->value_count; ++i)
        ok = tensor_put_u64(bytes, p->values[i].rows) && tensor_put_u64(bytes, p->values[i].width) &&
             tensor_put_u64(bytes, (unsigned int)p->values[i].parameter);
    for (i = 0u; ok && i < s->step_count; ++i)
        ok = tensor_put_text(bytes, p->steps[i].implementation) &&
             tensor_put_u64(bytes, p->steps[i].operands[0]) && tensor_put_u64(bytes, p->steps[i].operands[1]) &&
             tensor_put_u64(bytes, p->steps[i].result);
    for (i = 0u; ok && i < s->result_count; ++i) ok = tensor_put_u64(bytes, p->results[i]);
    return ok;
}

static int tensor_seal(yvex_program_tensor_plan *p, yvex_error *err)
{
    yvex_core_bytes bytes = {.maximum = TENSOR_PLAN_BYTES};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    int rc = tensor_verify(p, err);
    if (rc != YVEX_OK) return rc;
    yvex_sha256_init(&hash);
    if (!tensor_records(p, &bytes) || !yvex_sha256_update(&hash, bytes.data, bytes.count) ||
        !yvex_sha256_final(&hash, digest))
        rc = tensor_refuse(err, YVEX_ERR_NOMEM, "tensor identity encoding failed");
    else yvex_sha256_hex(digest, p->summary.identity);
    free(bytes.data);
    return rc;
}

static int tensor_values_lower(yvex_program_tensor_plan *p, const yvex_ir_module *m,
                                const yvex_program_entry *entry, yvex_error *err)
{
    yvex_ir_id population = YVEX_IR_NONE;
    size_t i;
    for (i = 0u; i < entry->value_count; ++i) {
        const yvex_ir_value *v = yvex_ir_value_at(m, entry->values[i].semantic_value);
        const yvex_ir_type *t = v ? yvex_ir_type_at(m, v->type) : NULL;
        yvex_program_tensor_value *physical = &p->values[i];
        if (!t || t->kind != YVEX_IR_TENSOR || t->scalar != YVEX_IR_BF16 || t->rank != 2u ||
            t->shape[1].symbol != YVEX_IR_NONE)
            return tensor_refuse(err, YVEX_ERR_UNSUPPORTED, "tensor legalization requires rank-2 BF16 values");
        physical->width = t->shape[1].extent;
        if (t->shape[0].symbol == YVEX_IR_NONE) {
            physical->rows = t->shape[0].extent;
            physical->parameter = 1;
        } else {
            const yvex_ir_dimension *d = yvex_ir_dimension_at(m, t->shape[0].symbol);
            if (!d || (population != YVEX_IR_NONE && population != t->shape[0].symbol))
                return tensor_refuse(err, YVEX_ERR_UNSUPPORTED, "tensor work requires one shared row population");
            population = t->shape[0].symbol;
            p->summary.minimum_rows = d->minimum;
            p->summary.maximum_rows = d->maximum;
            p->summary.row_multiple = d->multiple;
        }
    }
    return YVEX_OK;
}

int yvex_program_tensor_compile(yvex_program_tensor_plan **out, const yvex_program_execution *execution,
                                const char *symbol, yvex_error *err)
{
    const yvex_ir_module *m = yvex_program_execution_module(execution);
    const yvex_program_entry *entry = NULL;
    yvex_program_tensor_plan *p = NULL;
    yvex_program_tensor_summary s = {0};
    size_t i;
    int rc;
    if (out) *out = NULL;
    if (!out || !m || !symbol)
        return tensor_refuse(err, YVEX_ERR_INVALID_ARG, "sealed execution entry is required");
    for (i = 0u; i < yvex_program_execution_entry_count(execution); ++i) {
        const yvex_program_entry *e = yvex_program_execution_entry_at(execution, i);
        if (!strcmp(e->symbol, symbol)) entry = e;
    }
    if (!entry || entry->step_count < 2u)
        return tensor_refuse(err, YVEX_ERR_UNSUPPORTED, "tensor entry has no executable work");
    yvex_core_text_copy(s.entry, sizeof(s.entry), symbol);
    yvex_core_text_copy(s.semantic_identity, sizeof(s.semantic_identity), yvex_ir_identity(m));
    yvex_core_text_copy(s.execution_identity, sizeof(s.execution_identity), yvex_program_execution_identity(execution));
    s.input_count = entry->input_count;
    s.value_count = entry->value_count;
    s.step_count = entry->step_count - 1u;
    s.result_count = entry->result_count;
    rc = tensor_allocate(&p, &s, err);
    if (rc == YVEX_OK) rc = tensor_values_lower(p, m, entry, err);
    for (i = 0u; rc == YVEX_OK && i < s.step_count; ++i) {
        const yvex_program_step *step = &entry->steps[i];
        const yvex_ir_operation *op = yvex_ir_operation_at(m, step->semantic_operation);
        const tensor_legalization *rule = tensor_operation_find(op->definition->name, 0);
        if (!rule || op->definition->version != 1u || step->operand_count != 2u || step->result_count != 1u ||
            yvex_ir_operation_effects(m, step->semantic_operation))
            rc = tensor_refuse(err, YVEX_ERR_UNSUPPORTED, "operation has no admitted pure tensor legalization");
        else p->steps[i] = (yvex_program_tensor_step){rule->physical,
            {step->operands[0], step->operands[1]}, step->results[0]};
    }
    if (rc == YVEX_OK) {
        memcpy(p->results, entry->results, s.result_count * sizeof(*p->results));
        rc = tensor_seal(p, err);
    }
    if (rc == YVEX_OK) *out = p;
    else yvex_program_tensor_close(&p);
    return rc;
}

int yvex_program_tensor_encode(const yvex_program_tensor_plan *p, yvex_core_bytes *out, yvex_error *err)
{
    yvex_core_bytes bytes = {.maximum = TENSOR_PLAN_BYTES};
    int ok = p && out && tensor_records(p, &bytes) && tensor_put_text(&bytes, p->summary.identity) &&
             yvex_core_bytes_append(out, bytes.data, bytes.count);
    free(bytes.data);
    return ok ? YVEX_OK : tensor_refuse(err, YVEX_ERR_FORMAT, "tensor program encoding failed");
}

typedef struct { const unsigned char *data; size_t count, offset; } tensor_cursor;

static int tensor_get_u64(tensor_cursor *c, unsigned long long *out)
{
    unsigned int i;
    if (c->offset > c->count || c->count - c->offset < 8u) return 0;
    *out = 0u;
    for (i = 0u; i < 8u; ++i) *out |= (unsigned long long)c->data[c->offset++] << (i * 8u);
    return 1;
}

static int tensor_get_text(tensor_cursor *c, char *out, size_t capacity)
{
    unsigned long long n;
    if (!tensor_get_u64(c, &n) || !n || n >= capacity || n > c->count - c->offset ||
        memchr(c->data + c->offset, 0, (size_t)n)) return 0;
    memcpy(out, c->data + c->offset, (size_t)n);
    out[n] = '\0';
    c->offset += (size_t)n;
    return 1;
}

int yvex_program_tensor_decode(yvex_program_tensor_plan **out, const unsigned char *data, size_t count,
                               yvex_error *err)
{
    tensor_cursor c = {data, count, 0u};
    yvex_program_tensor_summary s = {0};
    yvex_program_tensor_plan *p = NULL;
    char text[YVEX_IR_NAME_CAP];
    unsigned long long populations[4], field;
    size_t i, j;
    int rc, ok;
    if (out) *out = NULL;
    if (!out || !data || !count || count > TENSOR_PLAN_BYTES)
        return tensor_refuse(err, YVEX_ERR_FORMAT, "bounded tensor program bytes are required");
    ok = tensor_get_text(&c, text, sizeof(text)) && !strcmp(text, "yvex.program.tensor.v1") &&
         tensor_get_text(&c, s.entry, sizeof(s.entry)) &&
         tensor_get_text(&c, s.semantic_identity, sizeof(s.semantic_identity)) &&
         tensor_get_text(&c, s.execution_identity, sizeof(s.execution_identity)) &&
         tensor_get_u64(&c, &s.minimum_rows) && tensor_get_u64(&c, &s.maximum_rows) &&
         tensor_get_u64(&c, &s.row_multiple);
    for (i = 0u; ok && i < 4u; ++i)
        ok = tensor_get_u64(&c, &populations[i]) && populations[i] <= TENSOR_PLAN_LIMIT;
    if (!ok) return tensor_refuse(err, YVEX_ERR_FORMAT, "tensor program header is malformed");
    s.input_count = (size_t)populations[0];
    s.value_count = (size_t)populations[1];
    s.step_count = (size_t)populations[2];
    s.result_count = (size_t)populations[3];
    rc = tensor_allocate(&p, &s, err);
    if (rc != YVEX_OK) return rc;
    for (i = 0u; ok && i < s.value_count; ++i) {
        ok = tensor_get_u64(&c, &p->values[i].rows) && tensor_get_u64(&c, &p->values[i].width) &&
             tensor_get_u64(&c, &field) && field <= 1u;
        if (ok) p->values[i].parameter = (int)field;
    }
    for (i = 0u; ok && i < s.step_count; ++i) {
        const tensor_legalization *op;
        ok = tensor_get_text(&c, text, sizeof(text));
        op = ok ? tensor_operation_find(text, 1) : NULL;
        ok = ok && op;
        if (ok) p->steps[i].implementation = op->physical;
        for (j = 0u; ok && j < 3u; ++j) {
            ok = tensor_get_u64(&c, &field) && field < s.value_count;
            if (ok && j < 2u) p->steps[i].operands[j] = (yvex_ir_id)field;
            else if (ok) p->steps[i].result = (yvex_ir_id)field;
        }
    }
    for (i = 0u; ok && i < s.result_count; ++i) {
        ok = tensor_get_u64(&c, &field) && field < s.value_count;
        if (ok) p->results[i] = (yvex_ir_id)field;
    }
    ok = ok && tensor_get_text(&c, text, sizeof(text)) && c.offset == c.count;
    rc = ok ? tensor_seal(p, err) : tensor_refuse(err, YVEX_ERR_FORMAT, "tensor program records are malformed");
    if (rc == YVEX_OK && strcmp(text, p->summary.identity))
        rc = tensor_refuse(err, YVEX_ERR_FORMAT, "tensor program identity mismatch");
    if (rc == YVEX_OK) *out = p;
    else yvex_program_tensor_close(&p);
    return rc;
}

const yvex_program_tensor_summary *yvex_program_tensor_summary_get(const yvex_program_tensor_plan *p)
{
    return p ? &p->summary : NULL;
}

const yvex_program_tensor_value *yvex_program_tensor_value_at(const yvex_program_tensor_plan *p, size_t i)
{
    return p && i < p->summary.value_count ? &p->values[i] : NULL;
}

const yvex_program_tensor_step *yvex_program_tensor_step_at(const yvex_program_tensor_plan *p, size_t i)
{
    return p && i < p->summary.step_count ? &p->steps[i] : NULL;
}

yvex_ir_id yvex_program_tensor_result_at(const yvex_program_tensor_plan *p, size_t i)
{
    return p && i < p->summary.result_count ? p->results[i] : YVEX_IR_NONE;
}

void yvex_program_tensor_close(yvex_program_tensor_plan **out)
{
    yvex_program_tensor_plan *p = out ? *out : NULL;
    if (!p) return;
    *out = NULL;
    free(p->values);
    free(p->steps);
    free(p->results);
    free(p);
}
