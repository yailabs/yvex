/* Resident generic tensor-program engine; model semantics live in its binding. */
#ifndef INCLUDE_YVEX_INTERNAL_TENSOR_ENGINE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_TENSOR_ENGINE_H_INCLUDED

#include <yvex/internal/program_stage.h>
#include <yvex/internal/tensor_binding.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_TENSOR_ENGINE_SCHEMA_V1 1u
typedef struct yvex_tensor_engine yvex_tensor_engine;
typedef struct {
    unsigned int schema_version;
    const char *source_path, *binding_path;
    yvex_backend_kind backend;
    unsigned long long generation, maximum_rows;
    unsigned long long maximum_host_bytes, maximum_device_bytes;
} yvex_tensor_engine_options;
typedef struct {
    unsigned int schema_version;
    yvex_backend_kind backend;
    unsigned long long generation, maximum_rows;
    /* Parameter execution extent is logical F32 view, not resident backing. */
    unsigned long long source_mapped_bytes, parameter_execution_bytes;
    unsigned long long workspace_host_bytes, workspace_device_bytes;
    unsigned long long input_format, rotary_width, primary_theta, secondary_theta;
    unsigned long long token_domain_size, type_domain_size, marker_token_id;
    char binding_identity[YVEX_SHA256_HEX_BYTES];
    char source_identity[YVEX_SHA256_HEX_BYTES];
    char logical_model_identity[YVEX_SHA256_HEX_BYTES];
    char tokenizer_identity[YVEX_SHA256_HEX_BYTES];
    char physical_program_identity[YVEX_SHA256_HEX_BYTES];
} yvex_tensor_engine_summary;

/* A failed open can return a non-runnable cleanup owner when checked discharge
 * fails; callers must close that owner before abandoning it. */
int yvex_tensor_engine_open(yvex_tensor_engine **, const yvex_tensor_engine_options *,
    yvex_tensor_engine_summary *, yvex_error *);
/* Model-independent token/type input lowering from the authenticated binding. */
int yvex_tensor_engine_execute_tokens(yvex_tensor_engine *, unsigned long long generation,
    const unsigned int *token_ids, unsigned long long token_count,
    unsigned int type_id, float *row_scores, int (*cancel)(void *), void *,
    yvex_backend_operation_facts *, yvex_error *);
int yvex_tensor_engine_close(yvex_tensor_engine **, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif
