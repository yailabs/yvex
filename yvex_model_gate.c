/*
 * YVEX - Real model materialization gate
 */
#include "yvex_model_gate_internal.h"

#include <stdio.h>
#include <string.h>

static int dtype_matches(const char *expected, yvex_dtype actual)
{
    const char *actual_name = yvex_dtype_name(actual);
    return expected && actual_name && strcmp(expected, actual_name) == 0;
}

static int expected_tensor_matches(const yvex_model_gate_expected_tensor *expected,
                                   const yvex_tensor_info *actual)
{
    unsigned int i;
    if (!expected || !actual || !expected->name || !expected->dtype) return 0;
    if (strcmp(expected->name, actual->name) != 0) return 0;
    if (expected->rank != actual->rank) return 0;
    if (!dtype_matches(expected->dtype, actual->dtype)) return 0;
    if (expected->bytes != actual->storage_bytes) return 0;
    for (i = 0; i < expected->rank && i < 4u; ++i) {
        if (expected->dims[i] != actual->dims[i]) return 0;
    }
    return 1;
}

static int materialize_backend(const yvex_artifact *artifact,
                               const yvex_gguf *gguf,
                               const yvex_tensor_table *tensors,
                               yvex_backend_kind kind,
                               const char *backend_name,
                               yvex_model_gate_backend_status *status,
                               yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_materialize_summary materialize_summary;
    int rc;

    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));
    memset(&materialize_summary, 0, sizeof(materialize_summary));
    backend_options.kind = kind;
    materialize_options.backend_name = backend_name;
    materialize_options.require_all_tensors = 1;

    if (kind == YVEX_BACKEND_KIND_CPU) {
        rc = yvex_backend_open_cpu(&backend, err);
    } else {
        rc = yvex_backend_open(&backend, &backend_options, err);
    }
    if (rc == YVEX_ERR_UNSUPPORTED) {
        *status = YVEX_MODEL_GATE_BACKEND_UNAVAILABLE;
        return YVEX_OK;
    }
    if (rc != YVEX_OK) {
        *status = YVEX_MODEL_GATE_BACKEND_FAIL;
        return rc;
    }

    rc = yvex_weight_table_materialize(&weights,
                                       artifact,
                                       gguf,
                                       tensors,
                                       backend,
                                       &materialize_options,
                                       err);
    if (rc == YVEX_OK) {
        rc = yvex_weight_table_get_summary(weights, &materialize_summary, err);
    }
    if (rc == YVEX_OK && materialize_summary.status == YVEX_WEIGHT_STATUS_MATERIALIZED &&
        materialize_summary.execution_ready == 0) {
        *status = YVEX_MODEL_GATE_BACKEND_PASS;
    } else {
        *status = YVEX_MODEL_GATE_BACKEND_FAIL;
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "yvex_model_gate_check",
                           "materialization did not reach weights-materialized");
            rc = YVEX_ERR_STATE;
        }
    }

    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    return rc;
}

static int required_backend_failed(yvex_model_gate_backend_status status)
{
    return status != YVEX_MODEL_GATE_BACKEND_PASS;
}

int yvex_model_gate_check(const yvex_model_gate_options *options,
                          yvex_model_gate_summary *summary_out,
                          yvex_error *err)
{
    yvex_model_gate_summary summary;
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    char actual_sha256[65];
    unsigned long long i;
    int rc;

    if (!options || !summary_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_model_gate_check",
                       "options and summary_out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->model_path || options->model_path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_model_gate_check",
                       "model_path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options->expected_tensor_count && !options->expected_tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_model_gate_check",
                       "expected_tensors is required when expected_tensor_count is nonzero");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&summary, 0, sizeof(summary));
    summary.status = YVEX_MODEL_GATE_UNKNOWN;
    summary.support_level = YVEX_MODEL_SUPPORT_NONE;
    summary.model_path = options->model_path;
    summary.model_label = options->model_label;
    summary.family = options->family;
    summary.cpu_status = YVEX_MODEL_GATE_BACKEND_NOT_TESTED;
    summary.cuda_status = YVEX_MODEL_GATE_BACKEND_NOT_TESTED;
    summary.execution_ready = 0;
    *summary_out = summary;

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = options->model_path;
    artifact_options.readonly = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc != YVEX_OK) {
        summary.status = YVEX_MODEL_GATE_BLOCKED;
        *summary_out = summary;
        return rc;
    }
    summary.file_bytes = yvex_artifact_size(artifact);

    if (options->artifact_sha256 && options->artifact_sha256[0]) {
        rc = yvex_model_gate_sha256_hex(yvex_artifact_data(artifact),
                                        yvex_artifact_size(artifact),
                                        actual_sha256);
        if (rc != YVEX_OK) {
            summary.status = YVEX_MODEL_GATE_FAIL;
            *summary_out = summary;
            yvex_artifact_close(artifact);
            yvex_error_set(err, rc, "yvex_model_gate_check", "sha256 calculation failed");
            return rc;
        }
        if (strcmp(actual_sha256, options->artifact_sha256) != 0) {
            summary.status = YVEX_MODEL_GATE_BLOCKED;
            *summary_out = summary;
            yvex_artifact_close(artifact);
            yvex_error_setf(err, YVEX_ERR_STATE, "yvex_model_gate_check",
                            "sha256 mismatch: expected %s got %s",
                            options->artifact_sha256, actual_sha256);
            return YVEX_ERR_STATE;
        }
    }

    rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) {
        summary.status = YVEX_MODEL_GATE_FAIL;
        *summary_out = summary;
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return rc;
    }
    summary.tensor_count = yvex_tensor_table_count(tensors);
    summary.support_level = YVEX_MODEL_SUPPORT_DESCRIPTOR_ONLY;

    for (i = 0; i < options->expected_tensor_count; ++i) {
        const yvex_model_gate_expected_tensor *expected = &options->expected_tensors[i];
        const yvex_tensor_info *actual = yvex_tensor_table_find(tensors, expected->name);
        if (expected_tensor_matches(expected, actual)) {
            summary.expected_tensor_matches++;
        } else {
            summary.expected_tensor_mismatches++;
        }
    }

    if (summary.expected_tensor_mismatches != 0) {
        summary.status = YVEX_MODEL_GATE_FAIL;
        *summary_out = summary;
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_model_gate_check",
                       "expected tensor specification mismatch");
        return YVEX_ERR_STATE;
    }

    if (options->check_cpu) {
        rc = materialize_backend(artifact, gguf, tensors,
                                 YVEX_BACKEND_KIND_CPU, "cpu",
                                 &summary.cpu_status, err);
        if (rc != YVEX_OK && options->require_cpu) {
            summary.status = summary.cpu_status == YVEX_MODEL_GATE_BACKEND_UNAVAILABLE ?
                YVEX_MODEL_GATE_BLOCKED : YVEX_MODEL_GATE_FAIL;
            *summary_out = summary;
            yvex_tensor_table_close(tensors);
            yvex_gguf_close(gguf);
            yvex_artifact_close(artifact);
            return rc;
        }
    }
    if (options->check_cuda) {
        rc = materialize_backend(artifact, gguf, tensors,
                                 YVEX_BACKEND_KIND_CUDA, "cuda",
                                 &summary.cuda_status, err);
        if (rc != YVEX_OK && options->require_cuda) {
            summary.status = summary.cuda_status == YVEX_MODEL_GATE_BACKEND_UNAVAILABLE ?
                YVEX_MODEL_GATE_BLOCKED : YVEX_MODEL_GATE_FAIL;
            *summary_out = summary;
            yvex_tensor_table_close(tensors);
            yvex_gguf_close(gguf);
            yvex_artifact_close(artifact);
            return rc;
        }
    }

    if ((options->require_cpu && required_backend_failed(summary.cpu_status)) ||
        (options->require_cuda && required_backend_failed(summary.cuda_status))) {
        summary.status = YVEX_MODEL_GATE_BLOCKED;
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_model_gate_check",
                       "required materialization backend did not pass");
    } else if ((options->check_cpu && summary.cpu_status == YVEX_MODEL_GATE_BACKEND_FAIL) ||
               (options->check_cuda && summary.cuda_status == YVEX_MODEL_GATE_BACKEND_FAIL)) {
        summary.status = YVEX_MODEL_GATE_PARTIAL;
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_model_gate_check",
                       "one requested materialization backend failed");
    } else if (options->expected_tensor_count > 0 &&
               summary.expected_tensor_matches == options->expected_tensor_count &&
               ((!options->check_cpu) || summary.cpu_status == YVEX_MODEL_GATE_BACKEND_PASS) &&
               ((!options->check_cuda) || summary.cuda_status == YVEX_MODEL_GATE_BACKEND_PASS)) {
        summary.status = YVEX_MODEL_GATE_PASS;
        summary.support_level = YVEX_MODEL_SUPPORT_SELECTED_TENSOR_MATERIALIZED;
    } else {
        summary.status = YVEX_MODEL_GATE_PASS;
        summary.support_level = YVEX_MODEL_SUPPORT_DESCRIPTOR_ONLY;
    }

    summary.execution_ready = 0;
    *summary_out = summary;
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return summary.status == YVEX_MODEL_GATE_PASS ? YVEX_OK : YVEX_ERR_STATE;
}

const char *yvex_model_gate_status_name(yvex_model_gate_status status)
{
    switch (status) {
    case YVEX_MODEL_GATE_UNKNOWN: return "model-gate-unknown";
    case YVEX_MODEL_GATE_PASS: return "model-gate-pass";
    case YVEX_MODEL_GATE_PARTIAL: return "model-gate-partial";
    case YVEX_MODEL_GATE_FAIL: return "model-gate-fail";
    case YVEX_MODEL_GATE_BLOCKED: return "model-gate-blocked";
    default: return "model-gate-unknown";
    }
}

const char *yvex_model_support_level_name(yvex_model_support_level level)
{
    switch (level) {
    case YVEX_MODEL_SUPPORT_NONE: return "none";
    case YVEX_MODEL_SUPPORT_DESCRIPTOR_ONLY: return "descriptor-only";
    case YVEX_MODEL_SUPPORT_SELECTED_TENSOR_MATERIALIZED: return "selected-tensor-materialized";
    case YVEX_MODEL_SUPPORT_FULL_WEIGHTS_MATERIALIZED: return "full-weights-materialized";
    case YVEX_MODEL_SUPPORT_PARTIAL_GRAPH_EXECUTABLE: return "partial-graph-executable";
    case YVEX_MODEL_SUPPORT_PREFILL_READY: return "prefill-ready";
    case YVEX_MODEL_SUPPORT_DECODE_READY: return "decode-ready";
    case YVEX_MODEL_SUPPORT_GENERATION_READY: return "generation-ready";
    default: return "none";
    }
}

const char *yvex_model_gate_backend_status_name(yvex_model_gate_backend_status status)
{
    switch (status) {
    case YVEX_MODEL_GATE_BACKEND_NOT_TESTED: return "not-tested";
    case YVEX_MODEL_GATE_BACKEND_PASS: return "pass";
    case YVEX_MODEL_GATE_BACKEND_FAIL: return "fail";
    case YVEX_MODEL_GATE_BACKEND_UNAVAILABLE: return "unavailable";
    default: return "not-tested";
    }
}
