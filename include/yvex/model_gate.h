/*
 * YVEX - Real model materialization gate
 *
 * File: include/yvex/model_gate.h
 * Layer: public tool/support API
 *
 * Purpose:
 *   Defines the model gate model-gate surface for validating YVEX-produced GGUF
 *   artifacts by identity, tensor specification, and CPU/CUDA materialization.
 *
 * Does not own:
 *   - full model conversion
 *   - graph execution
 *   - prefill/decode
 *   - inference readiness
 */
#ifndef YVEX_MODEL_GATE_H
#define YVEX_MODEL_GATE_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_MODEL_SUPPORT_NONE = 0,
    YVEX_MODEL_SUPPORT_DESCRIPTOR_ONLY,
    YVEX_MODEL_SUPPORT_SELECTED_TENSOR_MATERIALIZED,
    YVEX_MODEL_SUPPORT_FULL_WEIGHTS_MATERIALIZED,
    YVEX_MODEL_SUPPORT_PARTIAL_GRAPH_EXECUTABLE,
    YVEX_MODEL_SUPPORT_PREFILL_READY,
    YVEX_MODEL_SUPPORT_DECODE_READY,
    YVEX_MODEL_SUPPORT_GENERATION_READY
} yvex_model_support_level;

typedef enum {
    YVEX_MODEL_GATE_UNKNOWN = 0,
    YVEX_MODEL_GATE_PASS,
    YVEX_MODEL_GATE_PARTIAL,
    YVEX_MODEL_GATE_FAIL,
    YVEX_MODEL_GATE_BLOCKED
} yvex_model_gate_status;

typedef enum {
    YVEX_MODEL_GATE_BACKEND_NOT_TESTED = 0,
    YVEX_MODEL_GATE_BACKEND_PASS,
    YVEX_MODEL_GATE_BACKEND_FAIL,
    YVEX_MODEL_GATE_BACKEND_UNAVAILABLE
} yvex_model_gate_backend_status;

typedef struct {
    const char *name;
    const char *dtype;
    unsigned int rank;
    unsigned long long dims[4];
    unsigned long long bytes;
} yvex_model_gate_expected_tensor;

typedef struct {
    const char *model_path;
    const char *model_label;
    const char *family;
    const char *artifact_sha256;
    const yvex_model_gate_expected_tensor *expected_tensors;
    unsigned long long expected_tensor_count;
    int check_cpu;
    int check_cuda;
    int require_cpu;
    int require_cuda;
} yvex_model_gate_options;

typedef struct {
    yvex_model_gate_status status;
    yvex_model_support_level support_level;
    const char *model_path;
    const char *model_label;
    const char *family;
    const char *expected_sha256;
    char actual_sha256[65];
    const char *digest_status;
    const char *identity_status;
    unsigned long long file_bytes;
    unsigned long long tensor_count;
    unsigned long long expected_tensor_matches;
    unsigned long long expected_tensor_mismatches;
    yvex_model_gate_backend_status cpu_status;
    yvex_model_gate_backend_status cuda_status;
    int execution_ready;
} yvex_model_gate_summary;

int yvex_model_gate_check(const yvex_model_gate_options *options,
                          yvex_model_gate_summary *summary_out,
                          yvex_error *err);

const char *yvex_model_gate_status_name(yvex_model_gate_status status);
const char *yvex_model_support_level_name(yvex_model_support_level level);
const char *yvex_model_gate_backend_status_name(yvex_model_gate_backend_status status);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MODEL_GATE_H */
