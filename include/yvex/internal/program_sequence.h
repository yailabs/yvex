/* Stateful operation implementations bind computational state roots to runtime
 * providers. They stage candidates; transaction ownership remains with session. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_SEQUENCE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_SEQUENCE_H_INCLUDED
#include <yvex/internal/program_kernels.h>
#include <yvex/internal/runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct yvex_program_sequence yvex_program_sequence;
typedef struct {
    const char *input_identity;
    int (*cancel_requested)(void *);
    void *cancel_context;
} yvex_program_sequence_request;
int yvex_program_sequence_open(yvex_program_sequence **, const yvex_program_physical *,
    const yvex_model_engine_view *, const yvex_runtime_session_view *, const yvex_program_kernels *,
    unsigned long long token_capacity, unsigned long long host_limit, unsigned long long device_limit, yvex_error *);
int yvex_program_sequence_invoke(yvex_program_sequence *, const yvex_program_device_invocation *,
    const yvex_program_sequence_request *, yvex_backend_operation_facts *, yvex_error *);
void yvex_program_sequence_resources(const yvex_program_sequence *,
    unsigned long long *host, unsigned long long *device);
int yvex_program_sequence_close(yvex_program_sequence **, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
