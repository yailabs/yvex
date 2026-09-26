/* Immutable compiled physical tensor-program binding; no model-family execution. */
#ifndef INCLUDE_YVEX_INTERNAL_TENSOR_BINDING_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_TENSOR_BINDING_H_INCLUDED

#include <yvex/internal/program_physical.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_TENSOR_BINDING_SCHEMA_V1 1u
#define YVEX_TENSOR_INPUT_TOKEN_TYPE_DUAL_ROPE_V1 1u
typedef struct yvex_tensor_binding yvex_tensor_binding;
typedef struct {
    unsigned int schema_version;
    const char *source_identity;
    const char *tokenizer_identity;
    const char *logical_model_identity;
    unsigned long long source_bytes, source_tensor_count;
    unsigned long long input_format, rotary_width;
    unsigned long long primary_theta, secondary_theta;
    unsigned long long token_domain_size, type_domain_size, marker_token_id;
    const yvex_program_physical *program;
    size_t parameter_count;
    int (*parameter_name)(void *, unsigned long long, char[256], yvex_error *);
    void *parameter_context;
} yvex_tensor_binding_build_request;
typedef struct {
    unsigned int schema_version;
    char identity[YVEX_SHA256_HEX_BYTES];
    char source_identity[YVEX_SHA256_HEX_BYTES];
    char tokenizer_identity[YVEX_SHA256_HEX_BYTES];
    char logical_model_identity[YVEX_SHA256_HEX_BYTES];
    char physical_program_identity[YVEX_SHA256_HEX_BYTES];
    unsigned long long source_bytes, source_tensor_count;
    unsigned long long input_format, rotary_width;
    unsigned long long primary_theta, secondary_theta;
    unsigned long long token_domain_size, type_domain_size, marker_token_id;
    unsigned long long parameter_count, file_bytes;
} yvex_tensor_binding_summary;

int yvex_tensor_binding_publish(const char *path, const yvex_tensor_binding_build_request *,
    yvex_tensor_binding_summary *, yvex_error *);
int yvex_tensor_binding_open(yvex_tensor_binding **, const char *path, yvex_error *);
const yvex_tensor_binding_summary *yvex_tensor_binding_summary_get(const yvex_tensor_binding *);
const yvex_program_physical *yvex_tensor_binding_program(const yvex_tensor_binding *);
int yvex_tensor_binding_parameter_name(void *, unsigned long long, char[256], yvex_error *);
void yvex_tensor_binding_close(yvex_tensor_binding **);

#ifdef __cplusplus
}
#endif
#endif
