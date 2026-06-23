/*
 * YVEX - Materialization gate implementation
 */
#include "materialize_gate_internal.h"
#include "model_gate_internal.h"

#include <stdio.h>
#include <string.h>

static int path_exists(const char *path)
{
    FILE *fp;
    if (!path) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int dtype_matches(const char *expected, yvex_dtype actual)
{
    const char *actual_name = yvex_dtype_name(actual);
    return expected && actual_name && strcmp(expected, actual_name) == 0;
}

static int tensor_matches(const yvex_materialize_expected_tensor *expected,
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

static yvex_materialize_failure_class classify_materialize_failure(int rc,
                                                                   const yvex_error *err)
{
    const char *msg = err ? yvex_error_message(err) : "";
    if (rc == YVEX_ERR_NOMEM) return YVEX_MATERIALIZE_FAILURE_OOM;
    if (rc == YVEX_ERR_UNSUPPORTED) {
        if (msg && strstr(msg, "qtype")) return YVEX_MATERIALIZE_FAILURE_UNSUPPORTED_QTYPE;
        return YVEX_MATERIALIZE_FAILURE_UNSUPPORTED_DTYPE;
    }
    if (rc == YVEX_ERR_BACKEND) {
        if (msg && strstr(msg, "alloc")) return YVEX_MATERIALIZE_FAILURE_BACKEND_ALLOC;
        if (msg && (strstr(msg, "write") || strstr(msg, "copy"))) {
            return YVEX_MATERIALIZE_FAILURE_BACKEND_COPY;
        }
        return YVEX_MATERIALIZE_FAILURE_BACKEND_ALLOC;
    }
    return YVEX_MATERIALIZE_FAILURE_UNKNOWN;
}

static int materialize_repeated(const yvex_artifact *artifact,
                                const yvex_gguf *gguf,
                                const yvex_tensor_table *tensors,
                                yvex_backend_kind kind,
                                const char *backend_name,
                                unsigned int repeat_count,
                                int check_cleanup,
                                yvex_materialize_backend_status *backend_status,
                                unsigned long long *bytes_materialized,
                                int *cleanup_verified,
                                yvex_materialize_failure_class *failure_class,
                                yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options backend_options;
    yvex_backend_memory_stats before_stats;
    yvex_backend_memory_stats after_stats;
    int have_before = 0;
    unsigned int i;
    int rc;

    memset(&backend_options, 0, sizeof(backend_options));
    memset(&before_stats, 0, sizeof(before_stats));
    memset(&after_stats, 0, sizeof(after_stats));
    backend_options.kind = kind;

    if (repeat_count == 0) repeat_count = 1;
    *backend_status = YVEX_MATERIALIZE_BACKEND_FAIL;
    *bytes_materialized = 0;
    if (cleanup_verified) *cleanup_verified = check_cleanup ? 1 : 0;

    if (kind == YVEX_BACKEND_KIND_CPU) {
        rc = yvex_backend_open_cpu(&backend, err);
    } else {
        rc = yvex_backend_open(&backend, &backend_options, err);
    }
    if (rc == YVEX_ERR_UNSUPPORTED) {
        *backend_status = YVEX_MATERIALIZE_BACKEND_UNAVAILABLE;
        *failure_class = YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE;
        return YVEX_OK;
    }
    if (rc != YVEX_OK) {
        *failure_class = classify_materialize_failure(rc, err);
        return rc;
    }

    if (check_cleanup &&
        yvex_backend_get_memory_stats(backend, &before_stats, err) == YVEX_OK) {
        have_before = 1;
    }

    for (i = 0; i < repeat_count; ++i) {
        yvex_weight_table *weights = NULL;
        yvex_materialize_options options;
        yvex_materialize_summary summary;

        memset(&options, 0, sizeof(options));
        memset(&summary, 0, sizeof(summary));
        options.backend_name = backend_name;
        options.require_all_tensors = 1;

        rc = yvex_weight_table_materialize(&weights,
                                           artifact,
                                           gguf,
                                           tensors,
                                           backend,
                                           &options,
                                           err);
        if (rc == YVEX_OK) {
            rc = yvex_weight_table_get_summary(weights, &summary, err);
        }
        if (rc != YVEX_OK || summary.status != YVEX_WEIGHT_STATUS_MATERIALIZED ||
            summary.execution_ready != 0) {
            yvex_weight_table_close(weights);
            *backend_status = YVEX_MATERIALIZE_BACKEND_FAIL;
            if (rc == YVEX_OK) {
                yvex_error_set(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                               "materialization did not reach weights-materialized");
                rc = YVEX_ERR_STATE;
            }
            *failure_class = classify_materialize_failure(rc, err);
            yvex_backend_close(backend);
            return rc;
        }
        *bytes_materialized = summary.bytes_materialized;
        yvex_weight_table_close(weights);

        if (check_cleanup && have_before) {
            if (yvex_backend_get_memory_stats(backend, &after_stats, err) != YVEX_OK ||
                after_stats.allocated_bytes != before_stats.allocated_bytes) {
                if (cleanup_verified) *cleanup_verified = 0;
                *backend_status = YVEX_MATERIALIZE_BACKEND_FAIL;
                *failure_class = YVEX_MATERIALIZE_FAILURE_BACKEND_ALLOC;
                yvex_error_set(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                               "backend allocated bytes did not return to baseline after close");
                yvex_backend_close(backend);
                return YVEX_ERR_STATE;
            }
        } else if (check_cleanup && cleanup_verified) {
            *cleanup_verified = 0;
        }
    }

    *backend_status = YVEX_MATERIALIZE_BACKEND_PASS;
    yvex_backend_close(backend);
    return YVEX_OK;
}

int yvex_materialize_gate_check(const yvex_materialize_gate_options *options,
                                yvex_materialize_gate_summary *summary_out,
                                yvex_error *err)
{
    yvex_materialize_gate_summary summary;
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    char actual_sha[65];
    unsigned long long i;
    int cleanup_cpu = 0;
    int cleanup_cuda = 0;
    int rc;

    if (!options || !summary_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_materialize_gate_check",
                       "options and summary_out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->model_path || options->model_path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_materialize_gate_check",
                       "model_path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options->expected_tensor_count && !options->expected_tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_materialize_gate_check",
                       "expected_tensors is required when expected_tensor_count is nonzero");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&summary, 0, sizeof(summary));
    summary.status = YVEX_MATERIALIZE_GATE_UNKNOWN;
    summary.scope = options->scope;
    summary.failure_class = YVEX_MATERIALIZE_FAILURE_NONE;
    summary.label = options->label;
    summary.family = options->family;
    summary.model_path = options->model_path;
    summary.cpu_status = YVEX_MATERIALIZE_BACKEND_NOT_TESTED;
    summary.cuda_status = YVEX_MATERIALIZE_BACKEND_NOT_TESTED;
    summary.repeat_count = options->repeat_count ? options->repeat_count : 1u;
    summary.cleanup_verified = options->check_cleanup ? 0 : 1;
    summary.execution_ready = 0;
    *summary_out = summary;

    if (!path_exists(options->model_path)) {
        summary.status = YVEX_MATERIALIZE_GATE_BLOCKED;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_MISSING_FILE;
        *summary_out = summary;
        yvex_error_set(err, YVEX_ERR_IO, "yvex_materialize_gate_check", "model file is missing");
        return YVEX_ERR_IO;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = options->model_path;
    artifact_options.readonly = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc != YVEX_OK) {
        summary.status = YVEX_MATERIALIZE_GATE_BLOCKED;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_MISSING_FILE;
        *summary_out = summary;
        return rc;
    }
    summary.file_bytes = yvex_artifact_size(artifact);

    if (options->sha256 && options->sha256[0]) {
        rc = yvex_model_gate_sha256_hex(yvex_artifact_data(artifact),
                                        yvex_artifact_size(artifact),
                                        actual_sha);
        if (rc != YVEX_OK || strcmp(actual_sha, options->sha256) != 0) {
            summary.status = YVEX_MATERIALIZE_GATE_BLOCKED;
            summary.failure_class = YVEX_MATERIALIZE_FAILURE_HASH_MISMATCH;
            *summary_out = summary;
            yvex_artifact_close(artifact);
            yvex_error_setf(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                            "sha256 mismatch: expected %s got %s",
                            options->sha256, rc == YVEX_OK ? actual_sha : "unavailable");
            return YVEX_ERR_STATE;
        }
    }

    rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) {
        summary.status = YVEX_MATERIALIZE_GATE_FAIL;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_GGUF_PARSE;
        *summary_out = summary;
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return rc;
    }
    summary.tensor_count = yvex_tensor_table_count(tensors);

    for (i = 0; i < options->expected_tensor_count; ++i) {
        const yvex_materialize_expected_tensor *expected = &options->expected_tensors[i];
        const yvex_tensor_info *actual = yvex_tensor_table_find(tensors, expected->name);
        if (tensor_matches(expected, actual)) {
            summary.expected_tensor_matches++;
        } else {
            summary.expected_tensor_mismatches++;
        }
    }
    if (summary.expected_tensor_mismatches != 0) {
        summary.status = YVEX_MATERIALIZE_GATE_FAIL;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_TENSOR_SPEC_MISMATCH;
        *summary_out = summary;
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                       "expected tensor specification mismatch");
        return YVEX_ERR_STATE;
    }

    if (options->check_cpu) {
        rc = materialize_repeated(artifact, gguf, tensors,
                                  YVEX_BACKEND_KIND_CPU, "cpu",
                                  summary.repeat_count,
                                  options->check_cleanup,
                                  &summary.cpu_status,
                                  &summary.bytes_materialized_cpu,
                                  &cleanup_cpu,
                                  &summary.failure_class,
                                  err);
        if (rc != YVEX_OK && options->require_cpu) {
            summary.status = summary.cpu_status == YVEX_MATERIALIZE_BACKEND_UNAVAILABLE ?
                YVEX_MATERIALIZE_GATE_BLOCKED : YVEX_MATERIALIZE_GATE_FAIL;
            if (summary.cpu_status == YVEX_MATERIALIZE_BACKEND_UNAVAILABLE) {
                summary.failure_class = YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE;
            }
            *summary_out = summary;
            yvex_tensor_table_close(tensors);
            yvex_gguf_close(gguf);
            yvex_artifact_close(artifact);
            return rc;
        }
    }
    if (options->check_cuda) {
        rc = materialize_repeated(artifact, gguf, tensors,
                                  YVEX_BACKEND_KIND_CUDA, "cuda",
                                  summary.repeat_count,
                                  options->check_cleanup,
                                  &summary.cuda_status,
                                  &summary.bytes_materialized_cuda,
                                  &cleanup_cuda,
                                  &summary.failure_class,
                                  err);
        if (rc != YVEX_OK && options->require_cuda) {
            summary.status = summary.cuda_status == YVEX_MATERIALIZE_BACKEND_UNAVAILABLE ?
                YVEX_MATERIALIZE_GATE_BLOCKED : YVEX_MATERIALIZE_GATE_FAIL;
            if (summary.cuda_status == YVEX_MATERIALIZE_BACKEND_UNAVAILABLE) {
                summary.failure_class = YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE;
            }
            *summary_out = summary;
            yvex_tensor_table_close(tensors);
            yvex_gguf_close(gguf);
            yvex_artifact_close(artifact);
            return rc;
        }
    }

    if ((options->require_cpu && summary.cpu_status != YVEX_MATERIALIZE_BACKEND_PASS) ||
        (options->require_cuda && summary.cuda_status != YVEX_MATERIALIZE_BACKEND_PASS)) {
        summary.status = YVEX_MATERIALIZE_GATE_BLOCKED;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE;
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                       "required backend did not pass");
    } else if ((options->check_cpu && summary.cpu_status == YVEX_MATERIALIZE_BACKEND_FAIL) ||
               (options->check_cuda && summary.cuda_status == YVEX_MATERIALIZE_BACKEND_FAIL)) {
        summary.status = YVEX_MATERIALIZE_GATE_PARTIAL;
        if (summary.failure_class == YVEX_MATERIALIZE_FAILURE_NONE) {
            summary.failure_class = YVEX_MATERIALIZE_FAILURE_UNKNOWN;
        }
    } else {
        summary.status = YVEX_MATERIALIZE_GATE_PASS;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_NONE;
    }

    if (options->check_cleanup) {
        if (options->check_cpu && options->check_cuda) {
            summary.cleanup_verified = cleanup_cpu && cleanup_cuda;
        } else if (options->check_cpu) {
            summary.cleanup_verified = cleanup_cpu;
        } else if (options->check_cuda) {
            summary.cleanup_verified = cleanup_cuda;
        } else {
            summary.cleanup_verified = 0;
        }
    } else {
        summary.cleanup_verified = 1;
    }
    summary.execution_ready = 0;
    *summary_out = summary;

    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return summary.status == YVEX_MATERIALIZE_GATE_PASS ? YVEX_OK : YVEX_ERR_STATE;
}

const char *yvex_materialize_gate_status_name(yvex_materialize_gate_status status)
{
    switch (status) {
    case YVEX_MATERIALIZE_GATE_UNKNOWN: return "materialize-gate-unknown";
    case YVEX_MATERIALIZE_GATE_PASS: return "materialize-gate-pass";
    case YVEX_MATERIALIZE_GATE_PARTIAL: return "materialize-gate-partial";
    case YVEX_MATERIALIZE_GATE_FAIL: return "materialize-gate-fail";
    case YVEX_MATERIALIZE_GATE_BLOCKED: return "materialize-gate-blocked";
    default: return "materialize-gate-unknown";
    }
}

const char *yvex_materialize_scope_name(yvex_materialize_scope scope)
{
    switch (scope) {
    case YVEX_MATERIALIZE_SCOPE_UNKNOWN: return "unknown";
    case YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR: return "selected-tensor";
    case YVEX_MATERIALIZE_SCOPE_PARTIAL_MODEL: return "partial-model";
    case YVEX_MATERIALIZE_SCOPE_FULL_MODEL: return "full-model";
    default: return "unknown";
    }
}

const char *yvex_materialize_backend_status_name(yvex_materialize_backend_status status)
{
    switch (status) {
    case YVEX_MATERIALIZE_BACKEND_NOT_TESTED: return "not-tested";
    case YVEX_MATERIALIZE_BACKEND_PASS: return "pass";
    case YVEX_MATERIALIZE_BACKEND_FAIL: return "fail";
    case YVEX_MATERIALIZE_BACKEND_UNAVAILABLE: return "unavailable";
    default: return "not-tested";
    }
}

const char *yvex_materialize_failure_class_name(yvex_materialize_failure_class failure)
{
    switch (failure) {
    case YVEX_MATERIALIZE_FAILURE_NONE: return "none";
    case YVEX_MATERIALIZE_FAILURE_MISSING_FILE: return "missing_file";
    case YVEX_MATERIALIZE_FAILURE_HASH_MISMATCH: return "hash_mismatch";
    case YVEX_MATERIALIZE_FAILURE_GGUF_PARSE: return "gguf_parse";
    case YVEX_MATERIALIZE_FAILURE_TENSOR_SPEC_MISMATCH: return "tensor_spec_mismatch";
    case YVEX_MATERIALIZE_FAILURE_UNSUPPORTED_DTYPE: return "unsupported_dtype";
    case YVEX_MATERIALIZE_FAILURE_UNSUPPORTED_QTYPE: return "unsupported_qtype";
    case YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE: return "backend_unavailable";
    case YVEX_MATERIALIZE_FAILURE_BACKEND_ALLOC: return "backend_alloc";
    case YVEX_MATERIALIZE_FAILURE_BACKEND_COPY: return "backend_copy";
    case YVEX_MATERIALIZE_FAILURE_OOM: return "oom";
    case YVEX_MATERIALIZE_FAILURE_UNKNOWN: return "unknown";
    default: return "unknown";
    }
}
