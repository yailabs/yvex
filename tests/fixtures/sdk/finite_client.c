/* Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md. */
/* SDK-only native ABI conformance peer. No host, model, tokenizer or inference.
 * This fixture must never be installed as a computational producer. */
#include <yvex/finite_decision_producer.h>
#include <math.h>
#include <string.h>

int yvex_finite_producer_execute_local(const char *socket_path,
    const yvex_finite_producer_request *request,
    yvex_finite_producer_result *result, yvex_error *err)
{
    if (!socket_path || strcmp(request->model_alias, "sdk-conformance-only") != 0)
        return YVEX_ERR_INVALID_ARG;
    if (strcmp(request->question, "refuse") == 0) {
        err->code = YVEX_ERR_STATE;
        strcpy(err->where, "fixture.refusal");
        strcpy(err->message, "CONTEXT_MESSAGE_MUST_NOT_BE_RENDERED");
        return YVEX_ERR_STATE;
    }
    memset(result, 0, sizeof(*result));
    result->schema_version = YVEX_FINITE_PRODUCER_SCHEMA_V1;
    result->score_kind = YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT;
    result->engine_generation = request->expected_generation;
    result->candidate_count = request->candidate_count;
    result->token_count = 12;
    result->model_forward_count = result->resident_backbone_count = 1;
    /* Counters are synthetic test fields, never runtime evidence. */
    char *identities[] = {result->source_identity, result->logical_model_identity,
        result->binding_identity, result->tokenizer_identity, result->physical_program_identity,
        result->input_policy_identity, result->input_identity,
        result->candidate_population_identity, result->result_identity};
    for (unsigned int i = 0; i < sizeof(identities)/sizeof(identities[0]); i++)
        memset(identities[i], 'a', 64);
    for (unsigned long long i = 0; i < request->candidate_count; i++) {
        strcpy(result->candidates[i].id, request->candidates[i].id);
        result->candidates[i].raw_score = (double)i;
        result->candidates[i].relative_candidate_probability = 1.0/(double)request->candidate_count;
    }
    if (strcmp(request->question, "stale") == 0) result->engine_generation++;
    if (strcmp(request->question, "foreign") == 0) strcpy(result->candidates[0].id, "foreign");
    if (strcmp(request->question, "nan") == 0) result->candidates[0].raw_score = NAN;
    if (strcmp(request->question, "calibrated") == 0) result->calibrated = 1;
    if (strcmp(request->question, "generated") == 0) result->generated_token_count = 1;
    if (strcmp(request->question, "oversized") == 0) result->candidate_count = 999;
    if (strcmp(request->question, "unterminated") == 0) memset(result->result_identity, 'a', 65);
    return YVEX_OK;
}
