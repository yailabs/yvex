/*
 * YVEX - Materialization gate
 *
 * File: include/yvex/materialize_gate.h
 * Layer: public tool/support API
 *
 * Purpose:
 *   Defines the M2 repeatable materialization gate for produced GGUF artifacts.
 *   The gate validates identity, tensor specs, backend materialization,
 *   cleanup, and failure classes. It does not imply execution readiness.
 */
#ifndef YVEX_MATERIALIZE_GATE_H
#define YVEX_MATERIALIZE_GATE_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_MATERIALIZE_GATE_UNKNOWN = 0,
    YVEX_MATERIALIZE_GATE_PASS,
    YVEX_MATERIALIZE_GATE_PARTIAL,
    YVEX_MATERIALIZE_GATE_FAIL,
    YVEX_MATERIALIZE_GATE_BLOCKED
} yvex_materialize_gate_status;

typedef enum {
    YVEX_MATERIALIZE_SCOPE_UNKNOWN = 0,
    YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR,
    YVEX_MATERIALIZE_SCOPE_PARTIAL_MODEL,
    YVEX_MATERIALIZE_SCOPE_FULL_MODEL
} yvex_materialize_scope;

typedef enum {
    YVEX_MATERIALIZE_BACKEND_NOT_TESTED = 0,
    YVEX_MATERIALIZE_BACKEND_PASS,
    YVEX_MATERIALIZE_BACKEND_FAIL,
    YVEX_MATERIALIZE_BACKEND_UNAVAILABLE
} yvex_materialize_backend_status;

typedef enum {
    YVEX_MATERIALIZE_FAILURE_NONE = 0,
    YVEX_MATERIALIZE_FAILURE_MISSING_FILE,
    YVEX_MATERIALIZE_FAILURE_HASH_MISMATCH,
    YVEX_MATERIALIZE_FAILURE_GGUF_PARSE,
    YVEX_MATERIALIZE_FAILURE_TENSOR_SPEC_MISMATCH,
    YVEX_MATERIALIZE_FAILURE_UNSUPPORTED_DTYPE,
    YVEX_MATERIALIZE_FAILURE_UNSUPPORTED_QTYPE,
    YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE,
    YVEX_MATERIALIZE_FAILURE_BACKEND_ALLOC,
    YVEX_MATERIALIZE_FAILURE_BACKEND_COPY,
    YVEX_MATERIALIZE_FAILURE_OOM,
    YVEX_MATERIALIZE_FAILURE_UNKNOWN
} yvex_materialize_failure_class;

typedef struct {
    const char *name;
    const char *dtype;
    unsigned int rank;
    unsigned long long dims[4];
    unsigned long long bytes;
} yvex_materialize_expected_tensor;

typedef struct {
    const char *model_path;
    const char *label;
    const char *family;
    const char *sha256;
    yvex_materialize_scope scope;
    const yvex_materialize_expected_tensor *expected_tensors;
    unsigned long long expected_tensor_count;
    int check_cpu;
    int check_cuda;
    int require_cpu;
    int require_cuda;
    unsigned int repeat_count;
    int check_cleanup;
    int json;
} yvex_materialize_gate_options;

typedef struct {
    yvex_materialize_gate_status status;
    yvex_materialize_scope scope;
    yvex_materialize_failure_class failure_class;
    const char *label;
    const char *family;
    const char *model_path;
    unsigned long long file_bytes;
    unsigned long long tensor_count;
    unsigned long long expected_tensor_matches;
    unsigned long long expected_tensor_mismatches;
    unsigned long long bytes_materialized_cpu;
    unsigned long long bytes_materialized_cuda;
    yvex_materialize_backend_status cpu_status;
    yvex_materialize_backend_status cuda_status;
    unsigned int repeat_count;
    int cleanup_verified;
    int execution_ready;
} yvex_materialize_gate_summary;

int yvex_materialize_gate_check(const yvex_materialize_gate_options *options,
                                yvex_materialize_gate_summary *summary_out,
                                yvex_error *err);

const char *yvex_materialize_gate_status_name(yvex_materialize_gate_status status);
const char *yvex_materialize_scope_name(yvex_materialize_scope scope);
const char *yvex_materialize_backend_status_name(yvex_materialize_backend_status status);
const char *yvex_materialize_failure_class_name(yvex_materialize_failure_class failure);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MATERIALIZE_GATE_H */
