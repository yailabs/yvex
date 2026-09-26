/* Cold-selected admitted finite-frontier input policy. */
#ifndef INCLUDE_YVEX_INTERNAL_FINITE_INPUT_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_FINITE_INPUT_H_INCLUDED

#include <yvex/finite_decision.h>
#include <yvex/finite_decision_producer.h>

typedef struct {
    unsigned long long input_format, marker_token_id;
    unsigned long long token_domain_size, type_domain_size;
    char binding_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char source_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char tokenizer_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
} yvex_finite_input_admission;

typedef struct yvex_finite_input yvex_finite_input;
typedef struct {
    unsigned int tokens[64];
    unsigned long long token_count;
    unsigned int input_type_id;
    yvex_finite_decision_candidate candidates[YVEX_FINITE_PRODUCER_MAX_CANDIDATES];
} yvex_finite_input_compiled;
typedef struct {
    const char *policy_name;
    const char *source_identity;
    const char *tokenizer_identity;
    const char *policy_source_identity;
    unsigned long long input_format;
    int (*open)(void **, const char *, const yvex_finite_input_admission *, yvex_error *);
    int (*build)(void *, const yvex_finite_producer_request *,
                 yvex_finite_input_compiled *, yvex_error *);
    void (*close)(void **);
} yvex_finite_input_provider;

int yvex_finite_input_open(yvex_finite_input **out, const char *source_path,
    const yvex_finite_input_admission *admission, yvex_error *err);
int yvex_finite_input_build(yvex_finite_input *policy,
    const yvex_finite_producer_request *request,
    yvex_finite_input_compiled *compiled, yvex_error *err);
void yvex_finite_input_close(yvex_finite_input **policy);
const char *yvex_finite_input_identity(const yvex_finite_input *policy);

#endif
