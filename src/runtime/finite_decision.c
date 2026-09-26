/* Typed finite-population readout over an admitted non-autoregressive engine. */
#include <yvex/finite_decision.h>
#include <yvex/internal/finite_decision.h>
#include <yvex/internal/tensor_engine.h>
#include <yvex/internal/core.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct yvex_finite_decision_engine {
    yvex_tensor_engine *tensor;
    yvex_tensor_engine_summary summary;
};

static int decision_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "runtime.finite-decision", reason);
    return code;
}

static unsigned long long decision_now(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0u;
    return (unsigned long long)now.tv_sec * 1000000000ull + (unsigned long long)now.tv_nsec;
}

static int decision_seal(yvex_sha256 *hash, char identity[YVEX_FINITE_DECISION_IDENTITY_CAP])
{
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    if (!yvex_sha256_final(hash, digest)) return 0;
    yvex_sha256_hex(digest, identity);
    return 1;
}

static int decision_request_admit(const yvex_finite_decision_engine *engine,
    const yvex_finite_decision_request *request, yvex_error *err)
{
    if (!engine || !request || request->schema_version != YVEX_FINITE_DECISION_SCHEMA_V1 ||
        !request->token_ids || !request->candidates || !request->token_count ||
        request->token_count > engine->summary.maximum_rows || !request->candidate_count ||
        request->candidate_count > YVEX_FINITE_DECISION_MAX_CANDIDATES ||
        !request->expected_binding_identity || !request->expected_tokenizer_identity)
        return decision_refuse(err, YVEX_ERR_INVALID_ARG, "bounded exact finite-decision request required");
    if (strnlen(request->expected_binding_identity, YVEX_FINITE_DECISION_IDENTITY_CAP) != 64u ||
        strnlen(request->expected_tokenizer_identity, YVEX_FINITE_DECISION_IDENTITY_CAP) != 64u ||
        !yvex_sha256_hex_valid(request->expected_binding_identity) ||
        !yvex_sha256_hex_valid(request->expected_tokenizer_identity))
        return decision_refuse(err, YVEX_ERR_FORMAT, "malformed exact identity in decision request");
    if (request->expected_generation != engine->summary.generation ||
        strcmp(request->expected_binding_identity, engine->summary.binding_identity) ||
        strcmp(request->expected_tokenizer_identity, engine->summary.tokenizer_identity))
        return decision_refuse(err, YVEX_ERR_STATE, "stale model, binding or tokenizer identity");
    if (request->input_type_id >= engine->summary.type_domain_size)
        return decision_refuse(err, YVEX_ERR_BOUNDS, "input type is outside admitted domain");
    for (unsigned long long t = 0u; t < request->token_count; ++t)
        if (request->token_ids[t] >= engine->summary.token_domain_size)
            return decision_refuse(err, YVEX_ERR_BOUNDS, "token outside admitted vocabulary");
    for (unsigned long long i = 0u; i < request->candidate_count; ++i) {
        const yvex_finite_decision_candidate *candidate = request->candidates + i;
        size_t length = candidate->candidate_id ?
            strnlen(candidate->candidate_id, YVEX_FINITE_DECISION_ID_CAP) : 0u;
        if (!length || length >= YVEX_FINITE_DECISION_ID_CAP ||
            candidate->marker_position >= request->token_count ||
            request->token_ids[candidate->marker_position] != engine->summary.marker_token_id)
            return decision_refuse(err, YVEX_ERR_FORMAT, "exact candidate ID and marker required");
        for (unsigned long long j = 0u; j < i; ++j)
            if (candidate->marker_position == request->candidates[j].marker_position ||
                !strcmp(candidate->candidate_id, request->candidates[j].candidate_id))
                return decision_refuse(err, YVEX_ERR_FORMAT, "duplicate candidate identity or marker");
    }
    return YVEX_OK;
}

static int decision_input_id(const yvex_finite_decision_request *request,
    yvex_finite_decision_result *result)
{
    yvex_sha256 hash;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.finite-decision.input.v1") ||
        !yvex_sha256_update_u64(&hash, request->token_count) ||
        !yvex_sha256_update_u64(&hash, request->input_type_id)) return 0;
    for (unsigned long long i = 0u; i < request->token_count; ++i)
        if (!yvex_sha256_update_u64(&hash, request->token_ids[i])) return 0;
    if (!decision_seal(&hash, result->input_identity)) return 0;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.finite-decision.population.v1") ||
        !yvex_sha256_update_u64(&hash, request->candidate_count)) return 0;
    for (unsigned long long i = 0u; i < request->candidate_count; ++i)
        if (!yvex_sha256_update_text(&hash, request->candidates[i].candidate_id) ||
            !yvex_sha256_update_u64(&hash, request->candidates[i].marker_position)) return 0;
    return decision_seal(&hash, result->candidate_population_identity);
}

static int decision_scores(const yvex_finite_decision_request *request,
    const float *rows, yvex_finite_decision_result *result)
{
    double maximum = -INFINITY;
    for (unsigned long long i = 0u; i < request->candidate_count; ++i) {
        const yvex_finite_decision_candidate *candidate = request->candidates + i;
        double score = rows[candidate->marker_position];
        if (!isfinite(score)) return 0;
        yvex_finite_decision_candidate_result *out = result->candidates + i;
        yvex_core_text_copy(out->candidate_id, sizeof(out->candidate_id), candidate->candidate_id);
        out->marker_position = candidate->marker_position;
        out->raw_logit = score;
        if (score > maximum) maximum = score;
    }
    double total = 0.0;
    for (unsigned long long i = 0u; i < request->candidate_count; ++i) {
        double weight = exp(result->candidates[i].raw_logit - maximum);
        if (!isfinite(weight)) return 0;
        result->candidates[i].relative_candidate_probability = weight;
        total += weight;
    }
    if (!isfinite(total) || total <= 0.0) return 0;
    for (unsigned long long i = 0u; i < request->candidate_count; ++i)
        result->candidates[i].relative_candidate_probability /= total;
    return 1;
}

static int decision_result_id(yvex_finite_decision_result *result)
{
    yvex_sha256 hash;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.finite-decision.result.v1") ||
        !yvex_sha256_update_text(&hash, result->binding_identity) ||
        !yvex_sha256_update_text(&hash, result->physical_program_identity) ||
        !yvex_sha256_update_u64(&hash, result->engine_generation) ||
        !yvex_sha256_update_text(&hash, result->input_identity) ||
        !yvex_sha256_update_text(&hash, result->candidate_population_identity)) return 0;
    for (unsigned long long i = 0u; i < result->candidate_count; ++i) {
        float score = (float)result->candidates[i].raw_logit;
        uint32_t bits;
        memcpy(&bits, &score, sizeof(bits));
        if (!yvex_sha256_update_u64(&hash, bits)) return 0;
    }
    return decision_seal(&hash, result->result_identity);
}

int yvex_finite_decision_engine_open(yvex_finite_decision_engine **out,
    const yvex_finite_decision_engine_options *options, yvex_error *err)
{
    if (out) *out = NULL;
    if (!out || !options || options->schema_version != YVEX_FINITE_DECISION_SCHEMA_V1)
        return decision_refuse(err, YVEX_ERR_INVALID_ARG, "decision engine options required");
    yvex_finite_decision_engine *engine = calloc(1u, sizeof(*engine));
    if (!engine) return decision_refuse(err, YVEX_ERR_NOMEM, "decision engine allocation failed");
    yvex_tensor_engine_options tensor = {.schema_version = YVEX_TENSOR_ENGINE_SCHEMA_V1,
        .source_path = options->source_path, .binding_path = options->binding_path,
        .backend = options->backend, .generation = options->generation,
        .maximum_rows = options->maximum_tokens,
        .maximum_host_bytes = options->maximum_host_bytes,
        .maximum_device_bytes = options->maximum_device_bytes};
    int rc = yvex_tensor_engine_open(&engine->tensor, &tensor, &engine->summary, err);
    if (rc != YVEX_OK) {
        if (engine->tensor) *out = engine;
        else free(engine);
        return rc;
    }
    *out = engine;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_finite_decision_execute(yvex_finite_decision_engine *engine,
    const yvex_finite_decision_request *request,
    yvex_finite_decision_result *result, yvex_error *err)
{
    if (result) memset(result, 0, sizeof(*result));
    if (!result) return decision_refuse(err, YVEX_ERR_INVALID_ARG, "typed result required");
    int rc = decision_request_admit(engine, request, err);
    if (rc != YVEX_OK) return rc;
    if (request->cancel_requested && request->cancel_requested(request->cancel_context))
        return decision_refuse(err, YVEX_ERR_CANCELLED, "decision execution cancelled before publication");
    float *rows = calloc((size_t)request->token_count, sizeof(*rows));
    if (!rows) return decision_refuse(err, YVEX_ERR_NOMEM, "bounded decision output allocation failed");
    unsigned long long started = decision_now();
    yvex_backend_operation_facts facts = {0};
    rc = yvex_tensor_engine_execute_tokens(engine->tensor, request->expected_generation,
        request->token_ids, request->token_count, request->input_type_id,
        rows, request->cancel_requested, request->cancel_context, &facts, err);
    unsigned long long finished = decision_now();
    if (rc == YVEX_OK && request->cancel_requested && request->cancel_requested(request->cancel_context))
        rc = decision_refuse(err, YVEX_ERR_CANCELLED, "decision execution cancelled before publication");
    yvex_finite_decision_result pending = {0};
    if (rc == YVEX_OK && (!decision_input_id(request, &pending) ||
        !decision_scores(request, rows, &pending)))
        rc = decision_refuse(err, YVEX_ERR_FORMAT, "non-finite or unsealable decision result");
    free(rows);
    if (rc != YVEX_OK) return rc;
    pending.schema_version = YVEX_FINITE_DECISION_SCHEMA_V1;
    pending.engine_generation = engine->summary.generation;
    pending.token_count = request->token_count;
    pending.candidate_count = request->candidate_count;
    pending.model_forward_count = 1u;
    pending.resident_backbone_count = 1u;
    pending.elapsed_nanoseconds = finished >= started ? finished - started : 0u;
    pending.source_mapped_bytes = engine->summary.source_mapped_bytes;
    pending.parameter_execution_bytes = engine->summary.parameter_execution_bytes;
    pending.workspace_host_bytes = engine->summary.workspace_host_bytes;
    pending.workspace_device_bytes = engine->summary.workspace_device_bytes;
    yvex_core_text_copy(pending.source_identity, sizeof(pending.source_identity), engine->summary.source_identity);
    yvex_core_text_copy(pending.logical_model_identity, sizeof(pending.logical_model_identity),
        engine->summary.logical_model_identity);
    yvex_core_text_copy(pending.binding_identity, sizeof(pending.binding_identity), engine->summary.binding_identity);
    yvex_core_text_copy(pending.tokenizer_identity, sizeof(pending.tokenizer_identity),
        engine->summary.tokenizer_identity);
    yvex_core_text_copy(pending.physical_program_identity, sizeof(pending.physical_program_identity),
        engine->summary.physical_program_identity);
    if (!decision_result_id(&pending))
        return decision_refuse(err, YVEX_ERR_STATE, "decision result identity could not be sealed");
    *result = pending;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_finite_decision_engine_close(yvex_finite_decision_engine **engine, yvex_error *err)
{
    if (!engine || !*engine) { yvex_error_clear(err); return YVEX_OK; }
    int rc = yvex_tensor_engine_close(&(*engine)->tensor, err);
    if (rc == YVEX_OK) { free(*engine); *engine = NULL; }
    return rc;
}

int yvex_finite_decision_engine_summary_copy(const yvex_finite_decision_engine *engine,
    yvex_tensor_engine_summary *summary, yvex_error *err)
{
    if (!engine || !engine->tensor || !summary)
        return decision_refuse(err, YVEX_ERR_INVALID_ARG, "open decision engine and summary required");
    *summary = engine->summary;
    yvex_error_clear(err);
    return YVEX_OK;
}
