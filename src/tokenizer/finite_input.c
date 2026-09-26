/* Cold-bind a source-owned finite-frontier input policy to admitted engine truth. */
#include <yvex/internal/finite_input.h>
#include <yvex/internal/families/laya_input.h>
#include <yvex/internal/core.h>
#include <stdlib.h>
#include <string.h>

struct yvex_finite_input {
    const yvex_finite_input_provider *provider;
    void *state;
    char identity[YVEX_SHA256_HEX_BYTES];
};

static int input_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "tokenizer.finite-input", reason);
    return code;
}

int yvex_finite_input_open(yvex_finite_input **out, const char *source_path,
    const yvex_finite_input_admission *admission, yvex_error *err)
{
    const yvex_finite_input_provider *providers[] = {
        yvex_laya_finite_input_provider()
    };
    if (out) *out = NULL;
    if (!out || !source_path || !admission ||
        !yvex_sha256_hex_valid(admission->binding_identity) ||
        !yvex_sha256_hex_valid(admission->source_identity) ||
        !yvex_sha256_hex_valid(admission->tokenizer_identity))
        return input_refuse(err, YVEX_ERR_INVALID_ARG, "admitted engine and source required");
    for (size_t i = 0u; i < sizeof(providers) / sizeof(providers[0]); ++i) {
        const yvex_finite_input_provider *provider = providers[i];
        if (!provider->source_identity ||
            provider->input_format != admission->input_format ||
            strcmp(provider->source_identity, admission->source_identity) ||
            strcmp(provider->tokenizer_identity, admission->tokenizer_identity)) continue;
        yvex_finite_input *policy = calloc(1u, sizeof(*policy));
        if (!policy) return input_refuse(err, YVEX_ERR_NOMEM, "input policy allocation failed");
        policy->provider = provider;
        int rc = provider->open(&policy->state, source_path, admission, err);
        if (rc != YVEX_OK) { free(policy); return rc; }
        yvex_sha256 hash;
        unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
        yvex_sha256_init(&hash);
        if (!provider->policy_name || !provider->policy_source_identity ||
            !yvex_sha256_hex_valid(provider->policy_source_identity) ||
            !yvex_sha256_update_text(&hash, "yvex.finite-input-policy.v1") ||
            !yvex_sha256_update_text(&hash, provider->policy_name) ||
            !yvex_sha256_update_text(&hash, provider->policy_source_identity) ||
            !yvex_sha256_update_text(&hash, admission->binding_identity) ||
            !yvex_sha256_update_text(&hash, admission->source_identity) ||
            !yvex_sha256_update_text(&hash, admission->tokenizer_identity) ||
            !yvex_sha256_final(&hash, digest)) {
            provider->close(&policy->state);
            free(policy);
            return input_refuse(err, YVEX_ERR_STATE, "input policy identity unavailable");
        }
        yvex_sha256_hex(digest, policy->identity);
        *out = policy;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    return input_refuse(err, YVEX_ERR_UNSUPPORTED,
        "no admitted finite-frontier text input policy for this engine");
}

int yvex_finite_input_build(yvex_finite_input *policy,
    const yvex_finite_producer_request *request,
    yvex_finite_input_compiled *compiled, yvex_error *err)
{
    if (compiled) memset(compiled, 0, sizeof(*compiled));
    if (!policy || !request || !compiled ||
        request->schema_version != YVEX_FINITE_PRODUCER_SCHEMA_V1 ||
        !request->expected_generation || !request->candidate_count ||
        request->candidate_count > YVEX_FINITE_PRODUCER_MAX_CANDIDATES ||
        !request->question[0] ||
        !memchr(request->question, 0, sizeof(request->question)) ||
        !memchr(request->context, 0, sizeof(request->context)))
        return input_refuse(err, YVEX_ERR_INVALID_ARG, "bounded question and frontier required");
    for (unsigned long long i = 0u; i < request->candidate_count; ++i) {
        const yvex_finite_producer_candidate *candidate = &request->candidates[i];
        if (!candidate->id[0] || !candidate->text[0] ||
            !memchr(candidate->id, 0, sizeof(candidate->id)) ||
            !memchr(candidate->text, 0, sizeof(candidate->text)))
            return input_refuse(err, YVEX_ERR_FORMAT, "candidate identity and text required");
        for (unsigned long long j = 0u; j < i; ++j)
            if (!strcmp(candidate->id, request->candidates[j].id))
                return input_refuse(err, YVEX_ERR_FORMAT, "duplicate opaque candidate identity");
    }
    return policy->provider->build(policy->state, request, compiled, err);
}

void yvex_finite_input_close(yvex_finite_input **policy)
{
    if (!policy || !*policy) return;
    (*policy)->provider->close(&(*policy)->state);
    free(*policy);
    *policy = NULL;
}

const char *yvex_finite_input_identity(const yvex_finite_input *policy)
{
    return policy ? policy->identity : NULL;
}
