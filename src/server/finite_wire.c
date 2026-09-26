/* Canonical bounded finite-decision payload codec for local protocol v24. */
#include <yvex/internal/finite_producer_wire.h>
#include <yvex/internal/core.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

typedef struct { unsigned char *bytes; size_t cap, used; } writer;
typedef struct { const unsigned char *bytes; size_t count, used; } reader;

static int wire_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "server.finite-wire", reason);
    return code;
}

static int put(writer *w, const void *bytes, size_t count)
{
    if (count > w->cap - w->used) return 0;
    memcpy(w->bytes + w->used, bytes, count);
    w->used += count;
    return 1;
}
static int get(reader *r, void *bytes, size_t count)
{
    if (count > r->count - r->used) return 0;
    memcpy(bytes, r->bytes + r->used, count);
    r->used += count;
    return 1;
}
static int put_u64(writer *w, unsigned long long value)
{
    unsigned char bytes[8];
    for (unsigned int i = 0u; i < 8u; ++i) bytes[i] = (unsigned char)(value >> (56u - i * 8u));
    return put(w, bytes, sizeof(bytes));
}
static int get_u64(reader *r, unsigned long long *value)
{
    unsigned char bytes[8];
    if (!get(r, bytes, sizeof(bytes))) return 0;
    *value = 0u;
    for (unsigned int i = 0u; i < 8u; ++i) *value = (*value << 8u) | bytes[i];
    return 1;
}
static int put_text(writer *w, const char *text, size_t cap, int required)
{
    size_t count = strnlen(text, cap);
    return count < cap && (!required || count) && put_u64(w, count) && put(w, text, count);
}
static int get_text(reader *r, char *text, size_t cap, int required)
{
    unsigned long long count;
    if (!get_u64(r, &count) || count >= cap || (!count && required) ||
        !get(r, text, (size_t)count) || memchr(text, 0, (size_t)count)) return 0;
    text[count] = 0;
    return 1;
}
static int put_double(writer *w, double number)
{
    unsigned long long bits;
    if (!isfinite(number)) return 0;
    memcpy(&bits, &number, sizeof(bits));
    return put_u64(w, bits);
}
static int get_double(reader *r, double *number)
{
    unsigned long long bits;
    if (!get_u64(r, &bits)) return 0;
    memcpy(number, &bits, sizeof(bits));
    return isfinite(*number);
}
static int put_identity(writer *w, const char identity[65])
{
    return yvex_sha256_hex_valid(identity) && put(w, identity, 64u);
}
static int get_identity(reader *r, char identity[65])
{
    if (!get(r, identity, 64u)) return 0;
    identity[64] = 0;
    return yvex_sha256_hex_valid(identity);
}

int yvex_finite_producer_request_encode(const yvex_finite_producer_request *input,
    unsigned char *bytes, size_t capacity, size_t *count, yvex_error *err)
{
    writer w = {bytes, capacity, 0u};
    if (count) *count = 0u;
    if (!input || !bytes || !count ||
        input->schema_version != YVEX_FINITE_PRODUCER_SCHEMA_V1 ||
        !input->expected_generation || !input->candidate_count ||
        input->candidate_count > YVEX_FINITE_PRODUCER_MAX_CANDIDATES ||
        !put_u64(&w, input->schema_version) ||
        !put_text(&w, input->model_alias, sizeof(input->model_alias), 1) ||
        !put_u64(&w, input->expected_generation) ||
        !put_text(&w, input->question, sizeof(input->question), 1) ||
        !put_text(&w, input->context, sizeof(input->context), 0) ||
        !put_u64(&w, input->candidate_count))
        return wire_refuse(err, YVEX_ERR_INVALID_ARG, "bounded typed producer request required");
    for (unsigned long long i = 0u; i < input->candidate_count; ++i)
        if (!put_text(&w, input->candidates[i].id, sizeof(input->candidates[i].id), 1) ||
            !put_text(&w, input->candidates[i].text, sizeof(input->candidates[i].text), 1))
            return wire_refuse(err, YVEX_ERR_BOUNDS, "candidate exceeds wire bound");
    *count = w.used;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_finite_producer_request_decode(const unsigned char *bytes, size_t count,
    yvex_finite_producer_request *output, yvex_error *err)
{
    reader r = {bytes, count, 0u};
    unsigned long long version;
    if (!bytes || !output || count > 4096u)
        return wire_refuse(err, YVEX_ERR_INVALID_ARG, "bounded producer bytes required");
    memset(output, 0, sizeof(*output));
    if (!get_u64(&r, &version) || version != YVEX_FINITE_PRODUCER_SCHEMA_V1 ||
        !get_text(&r, output->model_alias, sizeof(output->model_alias), 1) ||
        !get_u64(&r, &output->expected_generation) || !output->expected_generation ||
        !get_text(&r, output->question, sizeof(output->question), 1) ||
        !get_text(&r, output->context, sizeof(output->context), 0) ||
        !get_u64(&r, &output->candidate_count) || !output->candidate_count ||
        output->candidate_count > YVEX_FINITE_PRODUCER_MAX_CANDIDATES)
        return wire_refuse(err, YVEX_ERR_FORMAT, "producer payload header is malformed");
    output->schema_version = (unsigned int)version;
    for (unsigned long long i = 0u; i < output->candidate_count; ++i)
        if (!get_text(&r, output->candidates[i].id, sizeof(output->candidates[i].id), 1) ||
            !get_text(&r, output->candidates[i].text, sizeof(output->candidates[i].text), 1))
            return wire_refuse(err, YVEX_ERR_FORMAT, "candidate payload is malformed");
    if (r.used != count) return wire_refuse(err, YVEX_ERR_FORMAT, "trailing producer payload refused");
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_finite_producer_result_encode(const yvex_finite_producer_result *input,
    unsigned char *bytes, size_t capacity, size_t *count, yvex_error *err)
{
    writer w = {bytes, capacity, 0u};
    if (count) *count = 0u;
    if (!input || !bytes || !count || input->schema_version != YVEX_FINITE_PRODUCER_SCHEMA_V1 ||
        !input->candidate_count || input->candidate_count > YVEX_FINITE_PRODUCER_MAX_CANDIDATES ||
        input->calibrated || input->score_kind != YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT ||
        input->sampling_invocation_count || input->generated_token_count ||
        input->resident_backbone_count != 1u || input->model_forward_count != 1u ||
        !put_u64(&w, input->schema_version) || !put_u64(&w, input->score_kind))
        return wire_refuse(err, YVEX_ERR_INVALID_ARG, "uncalibrated bounded result required");
    const unsigned long long facts[] = {input->engine_generation, input->token_count,
        input->candidate_count, input->model_forward_count, input->sampling_invocation_count,
        input->generated_token_count, input->resident_backbone_count,
        input->elapsed_nanoseconds, input->source_mapped_bytes,
        input->parameter_execution_bytes, input->workspace_host_bytes,
        input->workspace_device_bytes};
    for (size_t i = 0u; i < sizeof(facts) / sizeof(facts[0]); ++i)
        if (!put_u64(&w, facts[i])) return wire_refuse(err, YVEX_ERR_BOUNDS, "result facts exceed frame");
    const char *identities[] = {input->source_identity, input->logical_model_identity,
        input->binding_identity, input->tokenizer_identity, input->physical_program_identity,
        input->input_policy_identity,
        input->input_identity, input->candidate_population_identity, input->result_identity};
    for (size_t i = 0u; i < sizeof(identities) / sizeof(identities[0]); ++i)
        if (!put_identity(&w, identities[i])) return wire_refuse(err, YVEX_ERR_FORMAT, "result identity invalid");
    for (unsigned long long i = 0u; i < input->candidate_count; ++i)
        if (!put_text(&w, input->candidates[i].id, sizeof(input->candidates[i].id), 1) ||
            !put_double(&w, input->candidates[i].raw_score) ||
            !put_double(&w, input->candidates[i].relative_candidate_probability))
            return wire_refuse(err, YVEX_ERR_FORMAT, "candidate score is malformed");
    *count = w.used;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_finite_producer_result_decode(const unsigned char *bytes, size_t count,
    yvex_finite_producer_result *output, yvex_error *err)
{
    reader r = {bytes, count, 0u};
    unsigned long long version;
    if (!bytes || !output || count > 4096u)
        return wire_refuse(err, YVEX_ERR_INVALID_ARG, "bounded result bytes required");
    memset(output, 0, sizeof(*output));
    if (!get_u64(&r, &version) || version != YVEX_FINITE_PRODUCER_SCHEMA_V1)
        return wire_refuse(err, YVEX_ERR_FORMAT, "unsupported result schema");
    output->schema_version = (unsigned int)version;
    if (!get_u64(&r, &version) || version != YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT)
        return wire_refuse(err, YVEX_ERR_FORMAT, "unsupported finite score semantics");
    output->score_kind = (yvex_finite_producer_score_kind)version;
    unsigned long long *facts[] = {&output->engine_generation, &output->token_count,
        &output->candidate_count, &output->model_forward_count, &output->sampling_invocation_count,
        &output->generated_token_count, &output->resident_backbone_count,
        &output->elapsed_nanoseconds, &output->source_mapped_bytes,
        &output->parameter_execution_bytes, &output->workspace_host_bytes,
        &output->workspace_device_bytes};
    for (size_t i = 0u; i < sizeof(facts) / sizeof(facts[0]); ++i)
        if (!get_u64(&r, facts[i])) return wire_refuse(err, YVEX_ERR_FORMAT, "result facts truncated");
    if (!output->candidate_count || output->candidate_count > YVEX_FINITE_PRODUCER_MAX_CANDIDATES ||
        output->sampling_invocation_count || output->generated_token_count ||
        output->resident_backbone_count != 1u || output->model_forward_count != 1u)
        return wire_refuse(err, YVEX_ERR_FORMAT, "result contradicts finite-decision contract");
    char *identities[] = {output->source_identity, output->logical_model_identity,
        output->binding_identity, output->tokenizer_identity, output->physical_program_identity,
        output->input_policy_identity,
        output->input_identity, output->candidate_population_identity, output->result_identity};
    for (size_t i = 0u; i < sizeof(identities) / sizeof(identities[0]); ++i)
        if (!get_identity(&r, identities[i])) return wire_refuse(err, YVEX_ERR_FORMAT, "result identity malformed");
    double sum = 0.0;
    for (unsigned long long i = 0u; i < output->candidate_count; ++i) {
        yvex_finite_producer_candidate_result *candidate = &output->candidates[i];
        if (!get_text(&r, candidate->id, sizeof(candidate->id), 1) ||
            !get_double(&r, &candidate->raw_score) ||
            !get_double(&r, &candidate->relative_candidate_probability) ||
            candidate->relative_candidate_probability < 0.0 ||
            candidate->relative_candidate_probability > 1.0)
            return wire_refuse(err, YVEX_ERR_FORMAT, "candidate result malformed");
        sum += candidate->relative_candidate_probability;
    }
    if (r.used != count || fabs(sum - 1.0) > 1e-12)
        return wire_refuse(err, YVEX_ERR_FORMAT, "result extent/distribution malformed");
    yvex_error_clear(err);
    return YVEX_OK;
}
