/*
 * yvex_model_artifacts.c - Local model references, registry, and gates.
 *
 * This file owns operator-facing model artifact checks, registry helpers,
 * metadata drift reports, model gates, and registry command surfaces. It does
 * not implement model execution.
 */

#include "yvex_console_private.h"
#include <yvex/artifact_integrity.h>
#include <yvex/materialize_gate.h>
#include <yvex/model_gate.h>
#include <yvex/model_ref.h>
#include <yvex/model_registry.h>
#include <yvex/yvex.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


/* Private registry types */

typedef struct {
    char *alias;
    char *family;
    char *model;
    char *scope;
    char *artifact_class;
    char *qprofile;
    char *calibration;
    char *producer;
    char *schema_version;
    char *path;
    char *sha256;
    unsigned long long file_size;
    char *format;
    char *architecture;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    char *primary_tensor_name;
    char *primary_tensor_role;
    char *primary_tensor_dtype;
    unsigned int primary_tensor_rank;
    char *primary_tensor_dims;
    unsigned long long primary_tensor_bytes;
    char *support_level;
    int selected_embedding_ready;
    unsigned long long selected_embedding_hidden_size;
    unsigned long long selected_embedding_vocab_size;
    unsigned long long selected_embedding_output_count;
    unsigned long long selected_embedding_slice_bytes;
    int execution_ready;
} yvex_model_registry_owned_entry;

struct yvex_model_registry {
    char *selected;
    yvex_model_registry_owned_entry *entries;
    unsigned long long count;
    unsigned long long cap;
};

/* Private helper declarations */

static char *yvex_model_registry_strdup(const char *s);
static void yvex_model_registry_owned_entry_clear(yvex_model_registry_owned_entry *entry);
static void yvex_model_registry_entry_view(const yvex_model_registry_owned_entry *owned,
                                           yvex_model_registry_entry *view);
static int yvex_model_registry_copy_entry(yvex_model_registry_owned_entry *dst,
                                          const yvex_model_registry_entry *src,
                                          yvex_error *err);
static int yvex_model_registry_parse_json_file(const char *path,
                                               yvex_model_registry *registry,
                                               yvex_error *err);
static int yvex_model_registry_write_json_file(const yvex_model_registry *registry,
                                               const char *path,
                                               yvex_error *err);
static int yvex_model_registry_mkdir_parent(const char *path,
                                            yvex_error *err);
static char *yvex_model_ref_strdup(const char *s);
static int yvex_model_ref_copy_from_entry(yvex_model_ref *out,
                                          const char *input,
                                          const yvex_model_registry_entry *entry,
                                          yvex_error *err);
static int yvex_model_gate_dtype_matches(const char *expected, yvex_dtype actual)
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
    if (!yvex_model_gate_dtype_matches(expected->dtype, actual->dtype)) return 0;
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
    summary.expected_sha256 = options->artifact_sha256 && options->artifact_sha256[0]
        ? options->artifact_sha256 : "";
    summary.digest_status = options->artifact_sha256 && options->artifact_sha256[0]
        ? "unchecked" : "unrequested";
    summary.identity_status = options->artifact_sha256 && options->artifact_sha256[0]
        ? "unchecked" : "unrequested";
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
        rc = yvex_artifact_sha256_hex_bytes(yvex_artifact_data(artifact),
                                            yvex_artifact_size(artifact),
                                            actual_sha256,
                                            err);
        snprintf(summary.actual_sha256, sizeof(summary.actual_sha256), "%s",
                 rc == YVEX_OK ? actual_sha256 : "");
        if (rc != YVEX_OK) {
            summary.status = YVEX_MODEL_GATE_FAIL;
            summary.digest_status = "fail";
            summary.identity_status = "fail";
            *summary_out = summary;
            yvex_artifact_close(artifact);
            yvex_error_set(err, rc, "yvex_model_gate_check", "sha256 calculation failed");
            return rc;
        }
        if (strcmp(actual_sha256, options->artifact_sha256) != 0) {
            summary.status = YVEX_MODEL_GATE_BLOCKED;
            summary.digest_status = "fail";
            summary.identity_status = "fail";
            *summary_out = summary;
            yvex_artifact_close(artifact);
            yvex_error_setf(err, YVEX_ERR_STATE, "yvex_model_gate_check",
                            "sha256 mismatch: expected %s got %s",
                            options->artifact_sha256, actual_sha256);
            return YVEX_ERR_STATE;
        }
        summary.digest_status = "pass";
        summary.identity_status = "pass";
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


int yvex_model_gate_json_translation_unit_anchor(void)
{
    return 0;
}



static int path_exists(const char *path)
{
    FILE *fp;
    if (!path) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int yvex_materialize_gate_dtype_matches(const char *expected, yvex_dtype actual)
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
    if (!yvex_materialize_gate_dtype_matches(expected->dtype, actual->dtype)) return 0;
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
                                yvex_materialize_gate_summary *gate_summary,
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
        if (gate_summary) gate_summary->backend_status = "unavailable";
        *failure_class = YVEX_MATERIALIZE_FAILURE_BACKEND_UNAVAILABLE;
        return YVEX_OK;
    }
    if (rc != YVEX_OK) {
        if (gate_summary) gate_summary->backend_status = "fail";
        *failure_class = classify_materialize_failure(rc, err);
        return rc;
    }
    if (gate_summary) gate_summary->backend_status = "ready";

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
            if (gate_summary) {
                gate_summary->materialization_gate = "fail";
                gate_summary->backend_status = "fail";
                if (getenv("YVEX_TEST_FAIL_MATERIALIZE_AFTER_TRANSFER")) {
                    gate_summary->materialization_phase = "transfer";
                    gate_summary->allocation_attempted = 1;
                    gate_summary->transfer_attempted = 1;
                    gate_summary->cleanup_attempted = 1;
                    gate_summary->cleanup_status = "pass";
                    gate_summary->bytes_allocated = gate_summary->bytes_planned;
                    gate_summary->bytes_transferred = gate_summary->bytes_planned;
                } else if (getenv("YVEX_TEST_FAIL_MATERIALIZE_AFTER_ALLOC")) {
                    gate_summary->materialization_phase = "allocation";
                    gate_summary->allocation_attempted = 1;
                    gate_summary->transfer_attempted = 0;
                    gate_summary->cleanup_attempted = 1;
                    gate_summary->cleanup_status = "pass";
                    gate_summary->bytes_allocated = gate_summary->bytes_planned;
                } else {
                    gate_summary->materialization_phase = "allocation";
                    gate_summary->cleanup_attempted = check_cleanup ? 1 : 0;
                    gate_summary->cleanup_status = check_cleanup ? "pass" : "not-needed";
                }
            }
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
        if (gate_summary) {
            gate_summary->allocation_attempted = gate_summary->allocation_attempted ||
                summary.allocation_attempted;
            gate_summary->transfer_attempted = gate_summary->transfer_attempted ||
                summary.transfer_attempted;
            gate_summary->bytes_planned = summary.bytes_planned;
            gate_summary->bytes_allocated = summary.bytes_allocated;
            gate_summary->bytes_transferred = summary.bytes_transferred;
        }
        yvex_weight_table_close(weights);

        if (check_cleanup && have_before) {
            if (yvex_backend_get_memory_stats(backend, &after_stats, err) != YVEX_OK ||
                after_stats.allocated_bytes != before_stats.allocated_bytes) {
                if (cleanup_verified) *cleanup_verified = 0;
                if (gate_summary) {
                    gate_summary->cleanup_attempted = 1;
                    gate_summary->cleanup_status = "fail";
                }
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
    if (gate_summary) {
        gate_summary->materialization_gate = "pass";
        gate_summary->materialization_phase = "complete";
        gate_summary->cleanup_attempted = check_cleanup ? 1 : 0;
        gate_summary->cleanup_status = check_cleanup ? "pass" : "not-needed";
        gate_summary->backend_status = "ready";
    }
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
    yvex_artifact_integrity_report integrity_report;
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
    summary.expected_sha256 = options->sha256 && options->sha256[0] ? options->sha256 : "";
    summary.digest_status = options->sha256 && options->sha256[0] ? "unchecked" : "unrequested";
    summary.identity_status = options->sha256 && options->sha256[0] ? "unchecked" : "unrequested";
    summary.metadata_status = options->metadata_status && options->metadata_status[0]
                                  ? options->metadata_status
                                  : "unregistered";
    summary.materialization_gate = "fail";
    summary.materialization_phase = "preflight";
    summary.integrity_status = "unchecked";
    summary.shape_status = "unchecked";
    summary.range_status = "unchecked";
    summary.backend_status = "not-opened";
    summary.cleanup_status = "not-needed";
    summary.cpu_status = YVEX_MATERIALIZE_BACKEND_NOT_TESTED;
    summary.cuda_status = YVEX_MATERIALIZE_BACKEND_NOT_TESTED;
    summary.repeat_count = options->repeat_count ? options->repeat_count : 1u;
    summary.cleanup_verified = options->check_cleanup ? 0 : 1;
    summary.execution_ready = 0;
    *summary_out = summary;

    if (!path_exists(options->model_path)) {
        summary.status = YVEX_MATERIALIZE_GATE_BLOCKED;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_MISSING_FILE;
        summary.integrity_status = "fail";
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
        rc = yvex_artifact_sha256_hex_bytes(yvex_artifact_data(artifact),
                                            yvex_artifact_size(artifact),
                                            actual_sha,
                                            err);
        snprintf(summary.actual_sha256, sizeof(summary.actual_sha256), "%s",
                 rc == YVEX_OK ? actual_sha : "");
        if (rc != YVEX_OK || strcmp(actual_sha, options->sha256) != 0) {
            summary.status = YVEX_MATERIALIZE_GATE_BLOCKED;
            summary.failure_class = YVEX_MATERIALIZE_FAILURE_HASH_MISMATCH;
            summary.digest_status = "fail";
            summary.identity_status = "fail";
            *summary_out = summary;
            yvex_artifact_close(artifact);
            yvex_error_setf(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                            "sha256 mismatch: expected %s got %s",
                            options->sha256, rc == YVEX_OK ? actual_sha : "unavailable");
            return YVEX_ERR_STATE;
        }
        summary.digest_status = "pass";
        summary.identity_status = "pass";
    }

    rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) {
        summary.status = YVEX_MATERIALIZE_GATE_FAIL;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_GGUF_PARSE;
        summary.integrity_status = "fail";
        *summary_out = summary;
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return rc;
    }
    summary.tensor_count = yvex_tensor_table_count(tensors);

    memset(&integrity_report, 0, sizeof(integrity_report));
    rc = yvex_artifact_integrity_validate(artifact, gguf, tensors, NULL, &integrity_report, err);
    summary.integrity_status = (rc == YVEX_OK && integrity_report.passed) ? "pass" : "fail";
    summary.shape_status =
        integrity_report.tensor_shapes_invalid == 0 &&
        integrity_report.tensor_dtypes_invalid == 0 &&
        integrity_report.tensor_byte_counts_invalid == 0 ? "pass" : "fail";
    summary.range_status = integrity_report.tensor_ranges_invalid == 0 ? "pass" : "fail";
    summary.bytes_planned = integrity_report.known_tensor_bytes;
    if (rc != YVEX_OK || !integrity_report.passed) {
        summary.status = YVEX_MATERIALIZE_GATE_FAIL;
        summary.failure_class = YVEX_MATERIALIZE_FAILURE_GGUF_PARSE;
        *summary_out = summary;
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "yvex_materialize_gate_check",
                           "artifact integrity preflight failed");
        }
        return rc == YVEX_OK ? YVEX_ERR_STATE : rc;
    }

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
        summary.shape_status = "fail";
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
                                  &summary,
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
                                  &summary,
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


int yvex_materialize_gate_json_translation_unit_anchor(void)
{
    return 0;
}


int yvex_materialize_gate_report_translation_unit_anchor(void)
{
    return 0;
}



static char *yvex_model_ref_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

void yvex_model_ref_clear(yvex_model_ref *ref)
{
    if (!ref) return;
    free((char *)ref->input);
    free((char *)ref->path);
    free((char *)ref->alias);
    free((char *)ref->family);
    free((char *)ref->sha256);
    free((char *)ref->support_level);
    free((char *)ref->format);
    free((char *)ref->architecture);
    free((char *)ref->primary_tensor_name);
    free((char *)ref->primary_tensor_role);
    free((char *)ref->primary_tensor_dtype);
    free((char *)ref->primary_tensor_dims);
    memset(ref, 0, sizeof(*ref));
}

static int set_path_ref(yvex_model_ref *out, const char *input, yvex_error *err)
{
    memset(out, 0, sizeof(*out));
    out->input = yvex_model_ref_strdup(input);
    out->path = yvex_model_ref_strdup(input);
    out->alias = yvex_model_ref_strdup("");
    out->family = yvex_model_ref_strdup("");
    out->sha256 = yvex_model_ref_strdup("");
    out->support_level = yvex_model_ref_strdup("");
    out->format = yvex_model_ref_strdup("");
    out->architecture = yvex_model_ref_strdup("");
    out->primary_tensor_name = yvex_model_ref_strdup("");
    out->primary_tensor_role = yvex_model_ref_strdup("");
    out->primary_tensor_dtype = yvex_model_ref_strdup("");
    out->primary_tensor_dims = yvex_model_ref_strdup("");
    out->status = YVEX_MODEL_REF_STATUS_RESOLVED;
    out->kind = YVEX_MODEL_REF_PATH;
    out->execution_ready = 0;
    if (!out->input || !out->path || !out->alias || !out->family ||
        !out->sha256 || !out->support_level || !out->format ||
        !out->architecture || !out->primary_tensor_name ||
        !out->primary_tensor_role || !out->primary_tensor_dtype ||
        !out->primary_tensor_dims) {
        yvex_model_ref_clear(out);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_ref", "path reference allocation failed");
        return YVEX_ERR_NOMEM;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int is_path_like_reference(const char *input)
{
    size_t len;

    if (!input || !input[0]) return 0;
    if (strchr(input, '/') || strchr(input, '\\')) return 1;
    len = strlen(input);
    if (len >= 5u && strcmp(input + len - 5u, ".gguf") == 0) return 1;
    return 0;
}

static int yvex_model_ref_copy_from_entry(yvex_model_ref *out,
                                   const char *input,
                                   const yvex_model_registry_entry *entry,
                                   yvex_error *err)
{
    memset(out, 0, sizeof(*out));
    if (!input || !entry || !entry->alias || !entry->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_ref", "input and registry entry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    out->input = yvex_model_ref_strdup(input);
    out->path = yvex_model_ref_strdup(entry->path);
    out->alias = yvex_model_ref_strdup(entry->alias);
    out->family = yvex_model_ref_strdup(entry->family);
    out->sha256 = yvex_model_ref_strdup(entry->sha256);
    out->registered_file_size = entry->file_size;
    out->support_level = yvex_model_ref_strdup(entry->support_level);
    out->format = yvex_model_ref_strdup(entry->format);
    out->architecture = yvex_model_ref_strdup(entry->architecture);
    out->tensor_count = entry->tensor_count;
    out->known_tensor_bytes = entry->known_tensor_bytes;
    out->primary_tensor_name = yvex_model_ref_strdup(entry->primary_tensor_name);
    out->primary_tensor_role = yvex_model_ref_strdup(entry->primary_tensor_role);
    out->primary_tensor_dtype = yvex_model_ref_strdup(entry->primary_tensor_dtype);
    out->primary_tensor_rank = entry->primary_tensor_rank;
    out->primary_tensor_dims = yvex_model_ref_strdup(entry->primary_tensor_dims);
    out->primary_tensor_bytes = entry->primary_tensor_bytes;
    out->selected_embedding_ready = entry->selected_embedding_ready;
    out->selected_embedding_hidden_size = entry->selected_embedding_hidden_size;
    out->selected_embedding_vocab_size = entry->selected_embedding_vocab_size;
    out->selected_embedding_output_count = entry->selected_embedding_output_count;
    out->selected_embedding_slice_bytes = entry->selected_embedding_slice_bytes;
    out->status = YVEX_MODEL_REF_STATUS_RESOLVED;
    out->kind = YVEX_MODEL_REF_ALIAS;
    out->execution_ready = entry->execution_ready;
    if (!out->input || !out->path || !out->alias || !out->family ||
        !out->sha256 || !out->support_level || !out->format ||
        !out->architecture || !out->primary_tensor_name ||
        !out->primary_tensor_role || !out->primary_tensor_dtype ||
        !out->primary_tensor_dims) {
        yvex_model_ref_clear(out);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_ref", "alias reference allocation failed");
        return YVEX_ERR_NOMEM;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static void append_available_aliases(char *buf,
                                     size_t cap,
                                     const yvex_model_registry *registry)
{
    unsigned long long i;
    size_t used;

    if (!buf || cap == 0 || !registry) return;
    used = strlen(buf);
    for (i = 0; i < yvex_model_registry_count(registry); ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        int n;
        if (!entry || !entry->alias || !entry->alias[0]) continue;
        if (used + 4u >= cap) break;
        n = snprintf(buf + used, cap - used, "\n  %s", entry->alias);
        if (n < 0 || (size_t)n >= cap - used) {
            buf[cap - 1u] = '\0';
            break;
        }
        used += (size_t)n;
    }
}

int yvex_model_ref_resolve(yvex_model_ref *out,
                           const char *input,
                           const yvex_model_ref_options *options,
                           yvex_error *err)
{
    yvex_model_registry *registry = NULL;
    yvex_model_registry_options registry_options;
    const yvex_model_registry_entry *entry;
    char message[1024];
    int rc;

    if (!out || !input || !input[0]) {
        if (out) {
            memset(out, 0, sizeof(*out));
            out->status = YVEX_MODEL_REF_STATUS_INVALID;
        }
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_ref", "model reference is required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    if (access(input, F_OK) == 0) {
        return set_path_ref(out, input, err);
    }

    if (is_path_like_reference(input)) {
        return set_path_ref(out, input, err);
    }

    if (options && !options->allow_registry) {
        out->status = YVEX_MODEL_REF_STATUS_NOT_FOUND;
        out->kind = YVEX_MODEL_REF_UNKNOWN;
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "model_ref",
                        "model path does not exist: %s", input);
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&registry_options, 0, sizeof(registry_options));
    registry_options.registry_path = options ? options->registry_path : NULL;
    registry_options.create_if_missing = 0;
    rc = yvex_model_registry_open(&registry, &registry_options, err);
    if (rc != YVEX_OK) {
        const char *env_registry = getenv("YVEX_MODELS_REGISTRY");

        out->status = YVEX_MODEL_REF_STATUS_REGISTRY_UNAVAILABLE;
        out->kind = YVEX_MODEL_REF_UNKNOWN;
        if (env_registry && env_registry[0]) {
            yvex_error_setf(err, YVEX_ERR_IO, "model_ref",
                            "model registry unavailable for reference: %s; YVEX_MODELS_REGISTRY=%s; hint: register the alias in that registry, unset YVEX_MODELS_REGISTRY, or pass an existing path",
                            input, env_registry);
        } else {
            yvex_error_setf(err, YVEX_ERR_IO, "model_ref",
                            "model registry unavailable for reference: %s; hint: run './yvex models list' or pass an existing path",
                            input);
        }
        return YVEX_ERR_IO;
    }

    entry = yvex_model_registry_find(registry, input);
    if (!entry) {
        snprintf(message, sizeof(message),
                 "model reference not found: %s; hint: run './yvex models list'; available models:",
                 input);
        append_available_aliases(message, sizeof(message), registry);
        out->status = YVEX_MODEL_REF_STATUS_NOT_FOUND;
        out->kind = YVEX_MODEL_REF_UNKNOWN;
        yvex_model_registry_close(registry);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_ref", message);
        return YVEX_ERR_INVALID_ARG;
    }

    if (access(entry->path, F_OK) != 0) {
        out->status = YVEX_MODEL_REF_STATUS_ALIAS_PATH_MISSING;
        out->kind = YVEX_MODEL_REF_ALIAS;
        out->alias = yvex_model_ref_strdup(entry->alias);
        out->path = yvex_model_ref_strdup(entry->path);
        yvex_error_setf(err, YVEX_ERR_IO, "model_ref",
                        "model alias exists but path is missing: alias=%s path=%s; hint: update or remove the registry entry with './yvex models remove %s'",
                        entry->alias, entry->path, entry->alias);
        yvex_model_registry_close(registry);
        return YVEX_ERR_IO;
    }

    rc = yvex_model_ref_copy_from_entry(out, input, entry, err);
    yvex_model_registry_close(registry);
    return rc;
}


const char *yvex_model_ref_kind_name(yvex_model_ref_kind kind)
{
    switch (kind) {
    case YVEX_MODEL_REF_PATH:
        return "path";
    case YVEX_MODEL_REF_ALIAS:
        return "alias";
    case YVEX_MODEL_REF_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *yvex_model_ref_status_name(yvex_model_ref_status status)
{
    switch (status) {
    case YVEX_MODEL_REF_STATUS_RESOLVED:
        return "resolved";
    case YVEX_MODEL_REF_STATUS_NOT_FOUND:
        return "not-found";
    case YVEX_MODEL_REF_STATUS_ALIAS_PATH_MISSING:
        return "alias-path-missing";
    case YVEX_MODEL_REF_STATUS_REGISTRY_UNAVAILABLE:
        return "registry-unavailable";
    case YVEX_MODEL_REF_STATUS_INVALID:
        return "invalid";
    case YVEX_MODEL_REF_STATUS_UNKNOWN:
    default:
        return "unknown";
    }
}



static char *yvex_model_registry_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

static void yvex_model_registry_owned_entry_clear(yvex_model_registry_owned_entry *entry)
{
    if (!entry) return;
    free(entry->alias);
    free(entry->family);
    free(entry->model);
    free(entry->scope);
    free(entry->artifact_class);
    free(entry->qprofile);
    free(entry->calibration);
    free(entry->producer);
    free(entry->schema_version);
    free(entry->path);
    free(entry->sha256);
    free(entry->format);
    free(entry->architecture);
    free(entry->primary_tensor_name);
    free(entry->primary_tensor_role);
    free(entry->primary_tensor_dtype);
    free(entry->primary_tensor_dims);
    free(entry->support_level);
    memset(entry, 0, sizeof(*entry));
}

static void yvex_model_registry_entry_view(const yvex_model_registry_owned_entry *owned,
                                    yvex_model_registry_entry *view)
{
    memset(view, 0, sizeof(*view));
    if (!owned) return;
    view->alias = owned->alias;
    view->family = owned->family;
    view->model = owned->model;
    view->scope = owned->scope;
    view->artifact_class = owned->artifact_class;
    view->qprofile = owned->qprofile;
    view->calibration = owned->calibration;
    view->producer = owned->producer;
    view->schema_version = owned->schema_version;
    view->path = owned->path;
    view->sha256 = owned->sha256;
    view->file_size = owned->file_size;
    view->format = owned->format;
    view->architecture = owned->architecture;
    view->tensor_count = owned->tensor_count;
    view->known_tensor_bytes = owned->known_tensor_bytes;
    view->primary_tensor_name = owned->primary_tensor_name;
    view->primary_tensor_role = owned->primary_tensor_role;
    view->primary_tensor_dtype = owned->primary_tensor_dtype;
    view->primary_tensor_rank = owned->primary_tensor_rank;
    view->primary_tensor_dims = owned->primary_tensor_dims;
    view->primary_tensor_bytes = owned->primary_tensor_bytes;
    view->support_level = owned->support_level;
    view->selected_embedding_ready = owned->selected_embedding_ready;
    view->selected_embedding_hidden_size = owned->selected_embedding_hidden_size;
    view->selected_embedding_vocab_size = owned->selected_embedding_vocab_size;
    view->selected_embedding_output_count = owned->selected_embedding_output_count;
    view->selected_embedding_slice_bytes = owned->selected_embedding_slice_bytes;
    view->execution_ready = owned->execution_ready;
}

static int yvex_model_registry_copy_entry(yvex_model_registry_owned_entry *dst,
                                   const yvex_model_registry_entry *src,
                                   yvex_error *err)
{
    memset(dst, 0, sizeof(*dst));
    if (!src || !src->alias || !src->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry", "entry alias and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    dst->alias = yvex_model_registry_strdup(src->alias);
    dst->family = yvex_model_registry_strdup(src->family);
    dst->model = yvex_model_registry_strdup(src->model);
    dst->scope = yvex_model_registry_strdup(src->scope);
    dst->artifact_class = yvex_model_registry_strdup(src->artifact_class);
    dst->qprofile = yvex_model_registry_strdup(src->qprofile);
    dst->calibration = yvex_model_registry_strdup(src->calibration);
    dst->producer = yvex_model_registry_strdup(src->producer);
    dst->schema_version = yvex_model_registry_strdup(src->schema_version);
    dst->path = yvex_model_registry_strdup(src->path);
    dst->sha256 = yvex_model_registry_strdup(src->sha256);
    dst->file_size = src->file_size;
    dst->format = yvex_model_registry_strdup(src->format);
    dst->architecture = yvex_model_registry_strdup(src->architecture);
    dst->tensor_count = src->tensor_count;
    dst->known_tensor_bytes = src->known_tensor_bytes;
    dst->primary_tensor_name = yvex_model_registry_strdup(src->primary_tensor_name);
    dst->primary_tensor_role = yvex_model_registry_strdup(src->primary_tensor_role);
    dst->primary_tensor_dtype = yvex_model_registry_strdup(src->primary_tensor_dtype);
    dst->primary_tensor_rank = src->primary_tensor_rank;
    dst->primary_tensor_dims = yvex_model_registry_strdup(src->primary_tensor_dims);
    dst->primary_tensor_bytes = src->primary_tensor_bytes;
    dst->support_level = yvex_model_registry_strdup(src->support_level);
    dst->selected_embedding_ready = src->selected_embedding_ready;
    dst->selected_embedding_hidden_size = src->selected_embedding_hidden_size;
    dst->selected_embedding_vocab_size = src->selected_embedding_vocab_size;
    dst->selected_embedding_output_count = src->selected_embedding_output_count;
    dst->selected_embedding_slice_bytes = src->selected_embedding_slice_bytes;
    dst->execution_ready = src->execution_ready;
    if (!dst->alias || !dst->family || !dst->model || !dst->scope ||
        !dst->artifact_class || !dst->qprofile || !dst->calibration ||
        !dst->producer || !dst->schema_version || !dst->path ||
        !dst->sha256 || !dst->format || !dst->architecture ||
        !dst->primary_tensor_name || !dst->primary_tensor_role ||
        !dst->primary_tensor_dtype ||
        !dst->primary_tensor_dims || !dst->support_level) {
        yvex_model_registry_owned_entry_clear(dst);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry", "entry allocation failed");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

static const char *metadata_value_or_empty(const char *s)
{
    return s ? s : "";
}

static void metadata_set_status(char *dst, size_t cap, const char *status)
{
    if (!dst || cap == 0u) return;
    snprintf(dst, cap, "%s", status ? status : "");
}

static void metadata_add_issue(yvex_model_metadata_drift_report *out,
                               const char *code,
                               const char *registered_value,
                               const char *current_value)
{
    yvex_model_metadata_issue *issue;

    if (!out || out->issue_count >= YVEX_MODEL_METADATA_MAX_ISSUES) {
        return;
    }
    issue = &out->issues[out->issue_count++];
    snprintf(issue->code, sizeof(issue->code), "%s", code ? code : "");
    snprintf(issue->registered_value, sizeof(issue->registered_value), "%s",
             registered_value ? registered_value : "");
    snprintf(issue->current_value, sizeof(issue->current_value), "%s",
             current_value ? current_value : "");
}

static int metadata_string_missing(const char *s)
{
    return !s || !s[0];
}

static int metadata_registered_summary_missing(const yvex_model_registry_entry *entry)
{
    if (!entry) return 1;
    if (metadata_string_missing(entry->support_level)) return 1;
    if (metadata_string_missing(entry->format)) return 1;
    if (metadata_string_missing(entry->architecture)) return 1;
    if (entry->tensor_count == 0ull) return 1;
    if (metadata_string_missing(entry->primary_tensor_name)) return 1;
    if (metadata_string_missing(entry->primary_tensor_role)) return 1;
    if (metadata_string_missing(entry->primary_tensor_dtype)) return 1;
    if (entry->primary_tensor_rank == 0u) return 1;
    if (metadata_string_missing(entry->primary_tensor_dims)) return 1;
    return 0;
}

static void metadata_u64_to_text(unsigned long long value,
                                 char out[YVEX_MODEL_METADATA_VALUE_CAP])
{
    snprintf(out, YVEX_MODEL_METADATA_VALUE_CAP, "%llu", value);
}

static void metadata_bool_to_text(int value,
                                  char out[YVEX_MODEL_METADATA_VALUE_CAP])
{
    snprintf(out, YVEX_MODEL_METADATA_VALUE_CAP, "%s", value ? "true" : "false");
}

static void metadata_compare_string_field(yvex_model_metadata_drift_report *out,
                                          const char *code,
                                          const char *registered_value,
                                          const char *current_value)
{
    registered_value = metadata_value_or_empty(registered_value);
    current_value = metadata_value_or_empty(current_value);
    if (strcmp(registered_value, current_value) != 0) {
        metadata_add_issue(out, code, registered_value, current_value);
    }
}

static void metadata_compare_u64_field(yvex_model_metadata_drift_report *out,
                                       const char *code,
                                       unsigned long long registered_value,
                                       unsigned long long current_value)
{
    char registered_text[YVEX_MODEL_METADATA_VALUE_CAP];
    char current_text[YVEX_MODEL_METADATA_VALUE_CAP];

    if (registered_value == current_value) return;
    metadata_u64_to_text(registered_value, registered_text);
    metadata_u64_to_text(current_value, current_text);
    metadata_add_issue(out, code, registered_text, current_text);
}

static void metadata_compare_bool_field(yvex_model_metadata_drift_report *out,
                                        const char *code,
                                        int registered_value,
                                        int current_value)
{
    char registered_text[YVEX_MODEL_METADATA_VALUE_CAP];
    char current_text[YVEX_MODEL_METADATA_VALUE_CAP];

    if (!!registered_value == !!current_value) return;
    metadata_bool_to_text(registered_value, registered_text);
    metadata_bool_to_text(current_value, current_text);
    metadata_add_issue(out, code, registered_text, current_text);
}

int yvex_model_registry_compare_metadata(
    const yvex_model_registry_entry *registered,
    const yvex_model_registry_entry *current,
    yvex_model_metadata_drift_report *out,
    yvex_error *err)
{
    unsigned int before_selected_issues;

    if (!registered || !current || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_metadata",
                       "registered, current, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    metadata_set_status(out->metadata_status, sizeof(out->metadata_status), "pass");
    metadata_set_status(out->readiness_status, sizeof(out->readiness_status), "pass");

    if (metadata_registered_summary_missing(registered)) {
        metadata_set_status(out->metadata_status, sizeof(out->metadata_status), "missing");
        if (strcmp(metadata_value_or_empty(registered->support_level),
                   "selected-tensor-materialized") == 0) {
            metadata_set_status(out->readiness_status, sizeof(out->readiness_status), "missing");
        }
        metadata_add_issue(out, "registered-metadata-missing", "missing", "available");
        return YVEX_OK;
    }

    metadata_compare_string_field(out, "support-level-mismatch",
                                  registered->support_level, current->support_level);
    metadata_compare_string_field(out, "format-mismatch",
                                  registered->format, current->format);
    metadata_compare_string_field(out, "architecture-mismatch",
                                  registered->architecture, current->architecture);
    metadata_compare_u64_field(out, "tensor-count-mismatch",
                               registered->tensor_count, current->tensor_count);
    metadata_compare_u64_field(out, "known-tensor-bytes-mismatch",
                               registered->known_tensor_bytes, current->known_tensor_bytes);
    metadata_compare_string_field(out, "primary-tensor-name-mismatch",
                                  registered->primary_tensor_name, current->primary_tensor_name);
    metadata_compare_string_field(out, "primary-tensor-role-mismatch",
                                  registered->primary_tensor_role, current->primary_tensor_role);
    metadata_compare_string_field(out, "primary-tensor-dtype-mismatch",
                                  registered->primary_tensor_dtype, current->primary_tensor_dtype);
    metadata_compare_u64_field(out, "primary-tensor-rank-mismatch",
                               registered->primary_tensor_rank, current->primary_tensor_rank);
    metadata_compare_string_field(out, "primary-tensor-dims-mismatch",
                                  registered->primary_tensor_dims, current->primary_tensor_dims);
    metadata_compare_u64_field(out, "primary-tensor-bytes-mismatch",
                               registered->primary_tensor_bytes, current->primary_tensor_bytes);

    before_selected_issues = out->issue_count;
    metadata_compare_bool_field(out, "selected-embedding-readiness-mismatch",
                                registered->selected_embedding_ready,
                                current->selected_embedding_ready);
    metadata_compare_u64_field(out, "selected-embedding-hidden-size-mismatch",
                               registered->selected_embedding_hidden_size,
                               current->selected_embedding_hidden_size);
    metadata_compare_u64_field(out, "selected-embedding-vocab-size-mismatch",
                               registered->selected_embedding_vocab_size,
                               current->selected_embedding_vocab_size);
    metadata_compare_u64_field(out, "selected-embedding-output-count-mismatch",
                               registered->selected_embedding_output_count,
                               current->selected_embedding_output_count);
    metadata_compare_u64_field(out, "selected-embedding-slice-bytes-mismatch",
                               registered->selected_embedding_slice_bytes,
                               current->selected_embedding_slice_bytes);
    if (out->issue_count > before_selected_issues) {
        metadata_set_status(out->readiness_status, sizeof(out->readiness_status), "fail");
    }

    if (out->issue_count > 0u) {
        metadata_set_status(out->metadata_status, sizeof(out->metadata_status), "fail");
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int registry_reserve(yvex_model_registry *registry,
                            unsigned long long need,
                            yvex_error *err)
{
    yvex_model_registry_owned_entry *next;
    unsigned long long cap;

    if (need <= registry->cap) return YVEX_OK;
    cap = registry->cap ? registry->cap * 2u : 4u;
    while (cap < need) cap *= 2u;
    next = (yvex_model_registry_owned_entry *)realloc(registry->entries, (size_t)cap * sizeof(*next));
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry", "registry allocation failed");
        return YVEX_ERR_NOMEM;
    }
    memset(next + registry->cap, 0, (size_t)(cap - registry->cap) * sizeof(*next));
    registry->entries = next;
    registry->cap = cap;
    return YVEX_OK;
}

static int is_ambiguous_token(const char *alias)
{
    return strcmp(alias, "latest") == 0 ||
           strcmp(alias, "final") == 0 ||
           strcmp(alias, "new") == 0 ||
           strcmp(alias, "test") == 0 ||
           strcmp(alias, "tmp") == 0 ||
           strcmp(alias, "debug") == 0 ||
           strstr(alias, "-latest") || strstr(alias, "latest-") ||
           strstr(alias, "-final") || strstr(alias, "final-") ||
           strstr(alias, "-new") || strstr(alias, "new-") ||
           strstr(alias, "-test") || strstr(alias, "test-") ||
           strstr(alias, "-tmp") || strstr(alias, "tmp-") ||
           strstr(alias, "-debug") || strstr(alias, "debug-");
}

int yvex_model_alias_validate(const char *alias, yvex_error *err)
{
    const char *p;
    int hyphens = 0;

    if (!alias || !alias[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (alias[0] == '-' || alias[strlen(alias) - 1u] == '-') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must not start or end with hyphen");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strstr(alias, "--")) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must not contain empty segments");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strchr(alias, '/') || strstr(alias, "..")) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must not be path-like");
        return YVEX_ERR_INVALID_ARG;
    }
    for (p = alias; *p; ++p) {
        if (*p == '-') {
            hyphens++;
            continue;
        }
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias uses invalid characters");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    if (hyphens < 3) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias",
                       "alias must include family, model, scope, and artifact class; example: deepseek4-v4-flash-selected-embed");
        return YVEX_ERR_INVALID_ARG;
    }
    if (is_ambiguous_token(alias)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias contains ambiguous vocabulary");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_model_registry_default_path(char *out,
                                     unsigned long long out_size,
                                     yvex_error *err)
{
    const char *env = getenv("YVEX_MODELS_REGISTRY");
    int n;

    if (!out || out_size == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_path", "output buffer is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (env && env[0]) {
        n = snprintf(out, (size_t)out_size, "%s", env);
    } else {
        n = snprintf(out, (size_t)out_size, ".yvex/models.local.json");
    }
    if (n < 0 || (unsigned long long)n >= out_size) {
        out[0] = '\0';
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model_registry_path", "registry path buffer too small");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

int yvex_model_registry_open(yvex_model_registry **out,
                             const yvex_model_registry_options *options,
                             yvex_error *err)
{
    yvex_model_registry *registry;
    char path[4096];
    const char *registry_path = NULL;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    registry = (yvex_model_registry *)calloc(1u, sizeof(*registry));
    if (!registry) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_open", "registry allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (options && options->registry_path && options->registry_path[0]) {
        registry_path = options->registry_path;
    } else {
        rc = yvex_model_registry_default_path(path, sizeof(path), err);
        if (rc != YVEX_OK) {
            free(registry);
            return rc;
        }
        registry_path = path;
    }
    if (access(registry_path, F_OK) == 0) {
        rc = yvex_model_registry_parse_json_file(registry_path, registry, err);
        if (rc != YVEX_OK) {
            yvex_model_registry_close(registry);
            return rc;
        }
    } else if (!(options && options->create_if_missing)) {
        yvex_model_registry_close(registry);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_open", "registry does not exist: %s", registry_path);
        return YVEX_ERR_IO;
    }
    *out = registry;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_model_registry_close(yvex_model_registry *registry)
{
    unsigned long long i;

    if (!registry) return;
    free(registry->selected);
    for (i = 0; i < registry->count; ++i) {
        yvex_model_registry_owned_entry_clear(&registry->entries[i]);
    }
    free(registry->entries);
    free(registry);
}

unsigned long long yvex_model_registry_count(const yvex_model_registry *registry)
{
    return registry ? registry->count : 0u;
}

const yvex_model_registry_entry *yvex_model_registry_at(const yvex_model_registry *registry,
                                                        unsigned long long index)
{
    static yvex_model_registry_entry view;

    if (!registry || index >= registry->count) return NULL;
    yvex_model_registry_entry_view(&registry->entries[index], &view);
    return &view;
}

const yvex_model_registry_entry *yvex_model_registry_find(const yvex_model_registry *registry,
                                                          const char *alias)
{
    unsigned long long i;
    static yvex_model_registry_entry view;

    if (!registry || !alias) return NULL;
    for (i = 0; i < registry->count; ++i) {
        if (strcmp(registry->entries[i].alias, alias) == 0) {
            yvex_model_registry_entry_view(&registry->entries[i], &view);
            return &view;
        }
    }
    return NULL;
}

const yvex_model_registry_entry *yvex_model_registry_selected(const yvex_model_registry *registry)
{
    if (!registry || !registry->selected || !registry->selected[0]) return NULL;
    return yvex_model_registry_find(registry, registry->selected);
}

int yvex_model_registry_add(yvex_model_registry *registry,
                            const yvex_model_registry_entry *entry,
                            yvex_error *err)
{
    yvex_model_registry_owned_entry copy;
    int rc;

    if (!registry || !entry) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_add", "registry and entry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_model_alias_validate(entry->alias, err);
    if (rc != YVEX_OK) return rc;
    if (yvex_model_registry_find(registry, entry->alias)) {
        yvex_error_setf(err, YVEX_ERR_STATE, "model_registry_add", "duplicate alias: %s", entry->alias);
        return YVEX_ERR_STATE;
    }
    if (access(entry->path, F_OK) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_add", "model path does not exist: %s", entry->path);
        return YVEX_ERR_IO;
    }
    rc = registry_reserve(registry, registry->count + 1u, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_model_registry_copy_entry(&copy, entry, err);
    if (rc != YVEX_OK) return rc;
    registry->entries[registry->count++] = copy;
    return YVEX_OK;
}

int yvex_model_registry_remove(yvex_model_registry *registry,
                               const char *alias,
                               yvex_error *err)
{
    unsigned long long i;

    if (!registry || !alias) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_remove", "registry and alias are required");
        return YVEX_ERR_INVALID_ARG;
    }
    for (i = 0; i < registry->count; ++i) {
        if (strcmp(registry->entries[i].alias, alias) == 0) {
            yvex_model_registry_owned_entry_clear(&registry->entries[i]);
            if (i + 1u < registry->count) {
                memmove(&registry->entries[i], &registry->entries[i + 1u],
                        (size_t)(registry->count - i - 1u) * sizeof(registry->entries[0]));
            }
            registry->count--;
            memset(&registry->entries[registry->count], 0, sizeof(registry->entries[0]));
            if (registry->selected && strcmp(registry->selected, alias) == 0) {
                free(registry->selected);
                registry->selected = NULL;
            }
            return YVEX_OK;
        }
    }
    yvex_error_setf(err, YVEX_ERR_STATE, "model_registry_remove", "alias not found: %s", alias);
    return YVEX_ERR_STATE;
}

int yvex_model_registry_select(yvex_model_registry *registry,
                               const char *alias,
                               yvex_error *err)
{
    char *copy;

    if (!registry || !alias) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_select", "registry and alias are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_model_registry_find(registry, alias)) {
        yvex_error_setf(err, YVEX_ERR_STATE, "model_registry_select", "alias not found: %s", alias);
        return YVEX_ERR_STATE;
    }
    copy = yvex_model_registry_strdup(alias);
    if (!copy) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_select", "selected alias allocation failed");
        return YVEX_ERR_NOMEM;
    }
    free(registry->selected);
    registry->selected = copy;
    return YVEX_OK;
}

int yvex_model_registry_save(const yvex_model_registry *registry,
                             const char *path,
                             yvex_error *err)
{
    char default_path[4096];

    if (!registry) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_save", "registry is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!path || !path[0]) {
        int rc = yvex_model_registry_default_path(default_path, sizeof(default_path), err);
        if (rc != YVEX_OK) return rc;
        path = default_path;
    }
    return yvex_model_registry_write_json_file(registry, path, err);
}

static int split_canonical_stem(const char *stem,
                                char *family, size_t family_cap,
                                char *model, size_t model_cap,
                                char *scope, size_t scope_cap,
                                char *artifact_class, size_t class_cap,
                                char *qprofile, size_t qprofile_cap,
                                char *calibration, size_t calibration_cap,
                                char *producer, size_t producer_cap,
                                char *schema, size_t schema_cap,
                                char *alias, size_t alias_cap)
{
    char buf[1024];
    char *parts[64];
    int count = 0;
    char *tok;
    int tail;
    int i;
    size_t pos = 0;

    if (strlen(stem) >= sizeof(buf)) return 0;
    strcpy(buf, stem);
    tok = strtok(buf, "-");
    while (tok && count < 64) {
        parts[count++] = tok;
        tok = strtok(NULL, "-");
    }
    if (count < 8) return 0;
    tail = count - 4;
    snprintf(qprofile, qprofile_cap, "%s", parts[tail]);
    snprintf(calibration, calibration_cap, "%s", parts[tail + 1]);
    snprintf(producer, producer_cap, "%s", parts[tail + 2]);
    snprintf(schema, schema_cap, "%s", parts[tail + 3]);
    snprintf(family, family_cap, "%s", parts[0]);
    snprintf(scope, scope_cap, "%s", parts[tail - 2]);
    snprintf(artifact_class, class_cap, "%s", parts[tail - 1]);
    model[0] = '\0';
    for (i = 1; i < tail - 2; ++i) {
        int n = snprintf(model + pos, model_cap > pos ? model_cap - pos : 0,
                         "%s%s", pos ? "-" : "", parts[i]);
        if (n < 0 || (size_t)n >= (model_cap > pos ? model_cap - pos : 0)) return 0;
        pos += (size_t)n;
    }
    snprintf(alias, alias_cap, "%s-%s-%s-%s", family, model, scope, artifact_class);
    return strcmp(producer, "yvex") == 0 && strcmp(schema, "v1") == 0 &&
           family[0] && model[0] && scope[0] && artifact_class[0] &&
           qprofile[0] && calibration[0];
}

int yvex_model_registry_entry_derive_from_path(yvex_model_registry_entry *entry,
                                               const char *path,
                                               yvex_error *err)
{
    static char alias[256];
    static char family[128];
    static char model[128];
    static char scope[64];
    static char artifact_class[128];
    static char qprofile[64];
    static char calibration[128];
    static char producer[64];
    static char schema[64];
    static char path_copy[4096];
    char stem[1024];
    const char *base;
    size_t len;

    if (!entry || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_derive", "entry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    len = strlen(base);
    if (len <= 5u || strcmp(base + len - 5u, ".gguf") != 0 || len >= sizeof(stem)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_derive", "filename is not a canonical GGUF artifact name");
        return YVEX_ERR_FORMAT;
    }
    memcpy(stem, base, len - 5u);
    stem[len - 5u] = '\0';
    if (!split_canonical_stem(stem, family, sizeof(family), model, sizeof(model),
                              scope, sizeof(scope), artifact_class, sizeof(artifact_class),
                              qprofile, sizeof(qprofile), calibration, sizeof(calibration),
                              producer, sizeof(producer), schema, sizeof(schema),
                              alias, sizeof(alias))) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_derive", "filename does not match YVEX artifact naming grammar");
        return YVEX_ERR_FORMAT;
    }
    if (yvex_model_alias_validate(alias, err) != YVEX_OK) return yvex_error_code(err);
    snprintf(path_copy, sizeof(path_copy), "%s", path);
    memset(entry, 0, sizeof(*entry));
    entry->alias = alias;
    entry->family = family;
    entry->model = model;
    entry->scope = scope;
    entry->artifact_class = artifact_class;
    entry->qprofile = qprofile;
    entry->calibration = calibration;
    entry->producer = producer;
    entry->schema_version = schema;
    entry->path = path_copy;
    entry->sha256 = "";
    entry->file_size = 0ull;
    entry->format = "";
    entry->architecture = "";
    entry->tensor_count = 0ull;
    entry->known_tensor_bytes = 0ull;
    entry->primary_tensor_name = "";
    entry->primary_tensor_role = "";
    entry->primary_tensor_dtype = "";
    entry->primary_tensor_rank = 0u;
    entry->primary_tensor_dims = "";
    entry->primary_tensor_bytes = 0ull;
    entry->support_level = "";
    entry->selected_embedding_ready = 0;
    entry->selected_embedding_hidden_size = 0ull;
    entry->selected_embedding_vocab_size = 0ull;
    entry->selected_embedding_output_count = 0ull;
    entry->selected_embedding_slice_bytes = 0ull;
    entry->execution_ready = 0;
    return YVEX_OK;
}



static int read_file(const char *path, char **out, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot open registry: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot size registry: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_json", "read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot read registry: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    return YVEX_OK;
}

static char *extract_string_in(const char *start, const char *end, const char *key)
{
    char needle[128];
    const char *p;
    const char *colon;
    const char *s;
    char *out;
    size_t n = 0;

    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(start, needle);
    if (!p || (end && p >= end)) return yvex_model_registry_strdup("");
    colon = strchr(p, ':');
    if (!colon || (end && colon >= end)) return NULL;
    s = colon + 1;
    while ((!end || s < end) && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
    if (end && s >= end) return NULL;
    if (*s != '"') return NULL;
    s++;
    out = (char *)malloc((size_t)(end ? end - s : (long)strlen(s)) + 1u);
    if (!out) return NULL;
    while (*s && (!end || s < end)) {
        char ch = *s++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\' && *s && (!end || s < end)) {
            ch = *s++;
            if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else out[n++] = ch;
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    return NULL;
}

static int extract_bool_in(const char *start, const char *end, const char *key)
{
    char needle[128];
    const char *p;
    const char *colon;
    const char *s;

    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(start, needle);
    if (!p || (end && p >= end)) return 0;
    colon = strchr(p, ':');
    if (!colon || (end && colon >= end)) return 0;
    s = colon + 1;
    while ((!end || s < end) && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
    return strncmp(s, "true", 4) == 0 ? 1 : 0;
}

static unsigned long long extract_ull_in(const char *start, const char *end, const char *key)
{
    char needle[128];
    const char *p;
    const char *colon;
    const char *s;
    unsigned long long value = 0ull;

    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(start, needle);
    if (!p || (end && p >= end)) return 0ull;
    colon = strchr(p, ':');
    if (!colon || (end && colon >= end)) return 0ull;
    s = colon + 1;
    while ((!end || s < end) && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
    while ((!end || s < end) && *s >= '0' && *s <= '9') {
        unsigned int digit = (unsigned int)(*s - '0');
        if (value > (ULLONG_MAX - (unsigned long long)digit) / 10ull) {
            return 0ull;
        }
        value = value * 10ull + (unsigned long long)digit;
        s++;
    }
    return value;
}

static void free_entry_view_strings(yvex_model_registry_entry *view)
{
    if (!view) return;
    free((char *)view->alias);
    free((char *)view->family);
    free((char *)view->model);
    free((char *)view->scope);
    free((char *)view->artifact_class);
    free((char *)view->qprofile);
    free((char *)view->calibration);
    free((char *)view->producer);
    free((char *)view->schema_version);
    free((char *)view->path);
    free((char *)view->sha256);
    free((char *)view->format);
    free((char *)view->architecture);
    free((char *)view->primary_tensor_name);
    free((char *)view->primary_tensor_role);
    free((char *)view->primary_tensor_dtype);
    free((char *)view->primary_tensor_dims);
    free((char *)view->support_level);
    memset(view, 0, sizeof(*view));
}

static const char *find_matching_object_end(const char *start)
{
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    const char *p;

    for (p = start; *p; ++p) {
        if (in_string) {
            if (escape) escape = 0;
            else if (*p == '\\') escape = 1;
            else if (*p == '"') in_string = 0;
            continue;
        }
        if (*p == '"') in_string = 1;
        else if (*p == '{') depth++;
        else if (*p == '}') {
            depth--;
            if (depth == 0) return p + 1;
        }
    }
    return NULL;
}

static int yvex_model_registry_parse_json_file(const char *path,
                                        yvex_model_registry *registry,
                                        yvex_error *err)
{
    char *json = NULL;
    const char *models;
    const char *p;
    int rc;

    if (!path || !registry) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json", "path and registry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = read_file(path, &json, err);
    if (rc != YVEX_OK) return rc;
    if (!strstr(json, "\"schema\"") || !strstr(json, "yvex.models.local.v1")) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "registry schema missing or unsupported");
        return YVEX_ERR_FORMAT;
    }
    registry->selected = extract_string_in(json, NULL, "selected");
    if (!registry->selected) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "malformed selected field");
        return YVEX_ERR_FORMAT;
    }
    models = strstr(json, "\"models\"");
    if (!models) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "models array missing");
        return YVEX_ERR_FORMAT;
    }
    p = strchr(models, '[');
    if (!p) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "models array malformed");
        return YVEX_ERR_FORMAT;
    }
    p++;
    while (*p) {
        const char *obj_start;
        const char *obj_end;
        yvex_model_registry_entry view;
        yvex_model_registry_owned_entry owned;
        memset(&view, 0, sizeof(view));
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (*p == ']') break;
        if (*p != '{') {
            free(json);
            yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "model entry malformed");
            return YVEX_ERR_FORMAT;
        }
        obj_start = p;
        obj_end = find_matching_object_end(obj_start);
        if (!obj_end) {
            free(json);
            yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "model entry unterminated");
            return YVEX_ERR_FORMAT;
        }
        view.alias = extract_string_in(obj_start, obj_end, "alias");
        view.family = extract_string_in(obj_start, obj_end, "family");
        view.model = extract_string_in(obj_start, obj_end, "model");
        view.scope = extract_string_in(obj_start, obj_end, "scope");
        view.artifact_class = extract_string_in(obj_start, obj_end, "artifact_class");
        view.qprofile = extract_string_in(obj_start, obj_end, "qprofile");
        view.calibration = extract_string_in(obj_start, obj_end, "calibration");
        view.producer = extract_string_in(obj_start, obj_end, "producer");
        view.schema_version = extract_string_in(obj_start, obj_end, "schema_version");
        view.path = extract_string_in(obj_start, obj_end, "path");
        view.sha256 = extract_string_in(obj_start, obj_end, "sha256");
        view.file_size = extract_ull_in(obj_start, obj_end, "file_size");
        view.format = extract_string_in(obj_start, obj_end, "format");
        view.architecture = extract_string_in(obj_start, obj_end, "architecture");
        view.tensor_count = extract_ull_in(obj_start, obj_end, "tensor_count");
        view.known_tensor_bytes = extract_ull_in(obj_start, obj_end, "known_tensor_bytes");
        view.primary_tensor_name = extract_string_in(obj_start, obj_end, "primary_tensor_name");
        view.primary_tensor_role = extract_string_in(obj_start, obj_end, "primary_tensor_role");
        view.primary_tensor_dtype = extract_string_in(obj_start, obj_end, "primary_tensor_dtype");
        view.primary_tensor_rank = (unsigned int)extract_ull_in(obj_start, obj_end, "primary_tensor_rank");
        view.primary_tensor_dims = extract_string_in(obj_start, obj_end, "primary_tensor_dims");
        view.primary_tensor_bytes = extract_ull_in(obj_start, obj_end, "primary_tensor_bytes");
        view.support_level = extract_string_in(obj_start, obj_end, "support_level");
        view.selected_embedding_ready = extract_bool_in(obj_start, obj_end, "selected_embedding_ready");
        view.selected_embedding_hidden_size = extract_ull_in(obj_start, obj_end, "selected_embedding_hidden_size");
        view.selected_embedding_vocab_size = extract_ull_in(obj_start, obj_end, "selected_embedding_vocab_size");
        view.selected_embedding_output_count = extract_ull_in(obj_start, obj_end, "selected_embedding_output_count");
        view.selected_embedding_slice_bytes = extract_ull_in(obj_start, obj_end, "selected_embedding_slice_bytes");
        view.execution_ready = extract_bool_in(obj_start, obj_end, "execution_ready");
        if (!view.alias || !view.family || !view.model || !view.scope ||
            !view.artifact_class || !view.qprofile || !view.calibration ||
            !view.producer || !view.schema_version || !view.path ||
            !view.sha256 || !view.format || !view.architecture ||
            !view.primary_tensor_name || !view.primary_tensor_role ||
            !view.primary_tensor_dtype ||
            !view.primary_tensor_dims || !view.support_level) {
            free_entry_view_strings(&view);
            free(json);
            yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "model entry has malformed string field");
            return YVEX_ERR_FORMAT;
        }
        rc = yvex_model_alias_validate(view.alias, err);
        if (rc != YVEX_OK) {
            free_entry_view_strings(&view);
            free(json);
            return rc;
        }
        rc = yvex_model_registry_copy_entry(&owned, &view, err);
        free_entry_view_strings(&view);
        if (rc != YVEX_OK) {
            free(json);
            return rc;
        }
        if (registry->count == registry->cap) {
            yvex_model_registry_owned_entry *next;
            unsigned long long cap = registry->cap ? registry->cap * 2u : 4u;
            next = (yvex_model_registry_owned_entry *)realloc(registry->entries, (size_t)cap * sizeof(*next));
            if (!next) {
                yvex_model_registry_owned_entry_clear(&owned);
                free(json);
                yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_json", "registry allocation failed");
                return YVEX_ERR_NOMEM;
            }
            memset(next + registry->cap, 0, (size_t)(cap - registry->cap) * sizeof(*next));
            registry->entries = next;
            registry->cap = cap;
        }
        registry->entries[registry->count++] = owned;
        p = obj_end;
    }
    free(json);
    return YVEX_OK;
}

static void write_escaped(FILE *fp, const char *s)
{
    if (!s) s = "";
    fputc('"', fp);
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

static void write_field(FILE *fp, const char *indent, const char *key, const char *value, int comma)
{
    fputs(indent, fp);
    fprintf(fp, "\"%s\": ", key);
    write_escaped(fp, value);
    fprintf(fp, "%s\n", comma ? "," : "");
}

static void write_u64_field(FILE *fp,
                            const char *indent,
                            const char *key,
                            unsigned long long value,
                            int comma)
{
    fputs(indent, fp);
    fprintf(fp, "\"%s\": %llu%s\n", key, value, comma ? "," : "");
}

static int yvex_model_registry_mkdir_parent(const char *path, yvex_error *err)
{
    char buf[4096];
    char *slash;
    char *p;

    if (!path || strlen(path) >= sizeof(buf)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json", "invalid registry path");
        return YVEX_ERR_INVALID_ARG;
    }
    strcpy(buf, path);
    slash = strrchr(buf, '/');
    if (!slash) return YVEX_OK;
    *slash = '\0';
    if (!buf[0]) return YVEX_OK;
    for (p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0775) != 0 && errno != EEXIST) {
                yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot create directory: %s", buf);
                return YVEX_ERR_IO;
            }
            *p = '/';
        }
    }
    if (mkdir(buf, 0775) != 0 && errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot create directory: %s", buf);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int yvex_model_registry_write_json_file(const yvex_model_registry *registry,
                                        const char *path,
                                        yvex_error *err)
{
    char tmp[4096];
    FILE *fp;
    unsigned long long i;
    int n;
    int rc;

    if (!registry || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json", "registry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_model_registry_mkdir_parent(path, err);
    if (rc != YVEX_OK) return rc;
    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model_registry_json", "temporary path too long");
        return YVEX_ERR_BOUNDS;
    }
    fp = fopen(tmp, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot write registry: %s", tmp);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    write_field(fp, "  ", "schema", "yvex.models.local.v1", 1);
    write_field(fp, "  ", "selected", registry->selected ? registry->selected : "", 1);
    fprintf(fp, "  \"models\": [\n");
    for (i = 0; i < registry->count; ++i) {
        const yvex_model_registry_owned_entry *e = &registry->entries[i];
        fprintf(fp, "    {\n");
        write_field(fp, "      ", "alias", e->alias, 1);
        write_field(fp, "      ", "family", e->family, 1);
        write_field(fp, "      ", "model", e->model, 1);
        write_field(fp, "      ", "scope", e->scope, 1);
        write_field(fp, "      ", "artifact_class", e->artifact_class, 1);
        write_field(fp, "      ", "qprofile", e->qprofile, 1);
        write_field(fp, "      ", "calibration", e->calibration, 1);
        write_field(fp, "      ", "producer", e->producer, 1);
        write_field(fp, "      ", "schema_version", e->schema_version, 1);
        write_field(fp, "      ", "path", e->path, 1);
        write_field(fp, "      ", "sha256", e->sha256, 1);
        write_u64_field(fp, "      ", "file_size", e->file_size, 1);
        write_field(fp, "      ", "format", e->format, 1);
        write_field(fp, "      ", "architecture", e->architecture, 1);
        write_u64_field(fp, "      ", "tensor_count", e->tensor_count, 1);
        write_u64_field(fp, "      ", "known_tensor_bytes", e->known_tensor_bytes, 1);
        write_field(fp, "      ", "primary_tensor_name", e->primary_tensor_name, 1);
        write_field(fp, "      ", "primary_tensor_role", e->primary_tensor_role, 1);
        write_field(fp, "      ", "primary_tensor_dtype", e->primary_tensor_dtype, 1);
        write_u64_field(fp, "      ", "primary_tensor_rank", e->primary_tensor_rank, 1);
        write_field(fp, "      ", "primary_tensor_dims", e->primary_tensor_dims, 1);
        write_u64_field(fp, "      ", "primary_tensor_bytes", e->primary_tensor_bytes, 1);
        write_field(fp, "      ", "support_level", e->support_level, 1);
        fprintf(fp, "      \"selected_embedding_ready\": %s,\n",
                e->selected_embedding_ready ? "true" : "false");
        write_u64_field(fp, "      ", "selected_embedding_hidden_size",
                        e->selected_embedding_hidden_size, 1);
        write_u64_field(fp, "      ", "selected_embedding_vocab_size",
                        e->selected_embedding_vocab_size, 1);
        write_u64_field(fp, "      ", "selected_embedding_output_count",
                        e->selected_embedding_output_count, 1);
        write_u64_field(fp, "      ", "selected_embedding_slice_bytes",
                        e->selected_embedding_slice_bytes, 1);
        fprintf(fp, "      \"execution_ready\": %s\n", e->execution_ready ? "true" : "false");
        fprintf(fp, "    }%s\n", (i + 1u < registry->count) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    if (fflush(fp) != 0 || fclose(fp) != 0) {
        remove(tmp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot close registry: %s", tmp);
        return YVEX_ERR_IO;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot replace registry: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}



static int append_scan_entry(yvex_model_registry_entry **entries,
                             unsigned long long *count,
                             unsigned long long *cap,
                             const yvex_model_registry_entry *entry,
                             yvex_error *err)
{
    yvex_model_registry_owned_entry owned;
    yvex_model_registry_entry view;
    yvex_model_registry_entry *next;
    int rc;

    if (*count == *cap) {
        unsigned long long new_cap = *cap ? *cap * 2u : 8u;
        next = (yvex_model_registry_entry *)realloc(*entries, (size_t)new_cap * sizeof(*next));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_scan", "scan allocation failed");
            return YVEX_ERR_NOMEM;
        }
        memset(next + *cap, 0, (size_t)(new_cap - *cap) * sizeof(*next));
        *entries = next;
        *cap = new_cap;
    }
    rc = yvex_model_registry_copy_entry(&owned, entry, err);
    if (rc != YVEX_OK) return rc;
    yvex_model_registry_entry_view(&owned, &view);
    (*entries)[*count] = view;
    (*entries)[*count].alias = owned.alias;
    (*entries)[*count].family = owned.family;
    (*entries)[*count].model = owned.model;
    (*entries)[*count].scope = owned.scope;
    (*entries)[*count].artifact_class = owned.artifact_class;
    (*entries)[*count].qprofile = owned.qprofile;
    (*entries)[*count].calibration = owned.calibration;
    (*entries)[*count].producer = owned.producer;
    (*entries)[*count].schema_version = owned.schema_version;
    (*entries)[*count].path = owned.path;
    (*entries)[*count].sha256 = owned.sha256;
    (*entries)[*count].file_size = owned.file_size;
    (*entries)[*count].format = owned.format;
    (*entries)[*count].architecture = owned.architecture;
    (*entries)[*count].tensor_count = owned.tensor_count;
    (*entries)[*count].known_tensor_bytes = owned.known_tensor_bytes;
    (*entries)[*count].primary_tensor_name = owned.primary_tensor_name;
    (*entries)[*count].primary_tensor_role = owned.primary_tensor_role;
    (*entries)[*count].primary_tensor_dtype = owned.primary_tensor_dtype;
    (*entries)[*count].primary_tensor_rank = owned.primary_tensor_rank;
    (*entries)[*count].primary_tensor_dims = owned.primary_tensor_dims;
    (*entries)[*count].primary_tensor_bytes = owned.primary_tensor_bytes;
    (*entries)[*count].support_level = owned.support_level;
    (*entries)[*count].selected_embedding_ready = owned.selected_embedding_ready;
    (*entries)[*count].selected_embedding_hidden_size = owned.selected_embedding_hidden_size;
    (*entries)[*count].selected_embedding_vocab_size = owned.selected_embedding_vocab_size;
    (*entries)[*count].selected_embedding_output_count = owned.selected_embedding_output_count;
    (*entries)[*count].selected_embedding_slice_bytes = owned.selected_embedding_slice_bytes;
    (*entries)[*count].execution_ready = owned.execution_ready;
    (*count)++;
    return YVEX_OK;
}

static int scan_dir(const char *dir,
                    yvex_model_registry_entry **entries,
                    unsigned long long *count,
                    unsigned long long *cap,
                    yvex_error *err)
{
    DIR *dp;
    struct dirent *de;

    dp = opendir(dir);
    if (!dp) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_scan", "cannot open scan root: %s", dir);
        return YVEX_ERR_IO;
    }
    while ((de = readdir(dp)) != NULL) {
        char path[4096];
        struct stat st;
        size_t len;
        yvex_model_registry_entry entry;
        int n;

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        n = snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) continue;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            int rc = scan_dir(path, entries, count, cap, err);
            if (rc != YVEX_OK) {
                closedir(dp);
                return rc;
            }
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        len = strlen(path);
        if (len <= 5u || strcmp(path + len - 5u, ".gguf") != 0) continue;
        if (yvex_model_registry_entry_derive_from_path(&entry, path, err) == YVEX_OK) {
            int rc = append_scan_entry(entries, count, cap, &entry, err);
            if (rc != YVEX_OK) {
                closedir(dp);
                return rc;
            }
        }
    }
    closedir(dp);
    return YVEX_OK;
}

int yvex_model_registry_scan_root(const char *root,
                                  yvex_model_registry_entry **entries_out,
                                  unsigned long long *count_out,
                                  yvex_error *err)
{
    yvex_model_registry_entry *entries = NULL;
    unsigned long long count = 0;
    unsigned long long cap = 0;
    int rc;

    if (!root || !entries_out || !count_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_scan", "root and outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *entries_out = NULL;
    *count_out = 0;
    rc = scan_dir(root, &entries, &count, &cap, err);
    if (rc != YVEX_OK) {
        yvex_model_registry_scan_free(entries, count);
        return rc;
    }
    *entries_out = entries;
    *count_out = count;
    return YVEX_OK;
}

void yvex_model_registry_scan_free(yvex_model_registry_entry *entries,
                                   unsigned long long count)
{
    unsigned long long i;

    if (!entries) return;
    for (i = 0; i < count; ++i) {
        free((char *)entries[i].alias);
        free((char *)entries[i].family);
        free((char *)entries[i].model);
        free((char *)entries[i].scope);
        free((char *)entries[i].artifact_class);
        free((char *)entries[i].qprofile);
        free((char *)entries[i].calibration);
        free((char *)entries[i].producer);
        free((char *)entries[i].schema_version);
        free((char *)entries[i].path);
        free((char *)entries[i].sha256);
        free((char *)entries[i].format);
        free((char *)entries[i].architecture);
        free((char *)entries[i].primary_tensor_name);
        free((char *)entries[i].primary_tensor_role);
        free((char *)entries[i].primary_tensor_dtype);
        free((char *)entries[i].primary_tensor_dims);
        free((char *)entries[i].support_level);
    }
    free(entries);
}

/* Domain-owned command surface moved from yvex_model_commands.c. */

int models_registry_open(yvex_model_registry **registry,
                                const char *registry_path,
                                int create_if_missing,
                                yvex_error *err)
{
    yvex_model_registry_options options;

    memset(&options, 0, sizeof(options));
    options.registry_path = registry_path;
    options.create_if_missing = create_if_missing;
    return yvex_model_registry_open(registry, &options, err);
}

static void print_model_registry_entry_cli(const yvex_model_registry_entry *entry,
                                           int selected)
{
    if (!entry) return;
    printf("%c %s\n", selected ? '*' : '-', entry->alias ? entry->alias : "");
    printf("  family: %s\n", entry->family ? entry->family : "");
    printf("  model: %s\n", entry->model ? entry->model : "");
    printf("  scope: %s\n", entry->scope ? entry->scope : "");
    printf("  artifact_class: %s\n", entry->artifact_class ? entry->artifact_class : "");
    printf("  qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    printf("  calibration: %s\n", entry->calibration ? entry->calibration : "");
    printf("  producer: %s\n", entry->producer ? entry->producer : "");
    printf("  schema_version: %s\n", entry->schema_version ? entry->schema_version : "");
    printf("  support_level: %s\n", entry->support_level ? entry->support_level : "");
    printf("  execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    printf("  path: %s\n", entry->path ? entry->path : "");
    printf("  registered_file_size: %llu\n", entry->file_size);
    printf("  registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("  registered_format: %s\n", entry->format ? entry->format : "");
    printf("  registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("  registered_tensor_count: %llu\n", entry->tensor_count);
    printf("  registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("  registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("  registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("  registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("  registered_primary_rank: %u\n", entry->primary_tensor_rank);
    printf("  registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("  registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("  registered_selected_embedding_ready: %s\n",
           entry->selected_embedding_ready ? "true" : "false");
}

static void print_model_registry_scan_entry_cli(const yvex_model_registry_entry *entry)
{
    if (!entry) return;
    printf("candidate: %s\n", entry->alias ? entry->alias : "");
    printf("path: %s\n", entry->path ? entry->path : "");
    printf("family: %s\n", entry->family ? entry->family : "");
    printf("model: %s\n", entry->model ? entry->model : "");
    printf("scope: %s\n", entry->scope ? entry->scope : "");
    printf("artifact_class: %s\n", entry->artifact_class ? entry->artifact_class : "");
    printf("qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    printf("calibration: %s\n", entry->calibration ? entry->calibration : "");
}

static void dims_to_text(const unsigned long long *dims,
                         unsigned int rank,
                         char *out,
                         size_t out_cap)
{
    unsigned int i;
    size_t used = 0u;

    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (used + 1u < out_cap) {
        out[used++] = '[';
        out[used] = '\0';
    }
    for (i = 0; i < rank && used < out_cap; ++i) {
        int n = snprintf(out + used,
                         used < out_cap ? out_cap - used : 0u,
                         "%s%llu",
                         i == 0 ? "" : ",",
                         dims[i]);
        if (n < 0 || (size_t)n >= (used < out_cap ? out_cap - used : 0u)) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (used + 1u < out_cap) {
        out[used++] = ']';
        out[used] = '\0';
    }
}

static const char *current_support_from_metadata(const yvex_model_registry_entry *entry)
{
    if (entry && entry->primary_tensor_name && entry->primary_tensor_name[0]) {
        return "selected-tensor-materialized";
    }
    if (entry && entry->format && entry->format[0]) {
        return "descriptor-only";
    }
    return "";
}

void model_ref_registry_entry_view(const yvex_model_ref *ref,
                                          yvex_model_registry_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    if (!ref) return;
    entry->alias = ref->alias;
    entry->path = ref->path;
    entry->sha256 = ref->sha256;
    entry->file_size = ref->registered_file_size;
    entry->format = ref->format;
    entry->architecture = ref->architecture;
    entry->tensor_count = ref->tensor_count;
    entry->known_tensor_bytes = ref->known_tensor_bytes;
    entry->primary_tensor_name = ref->primary_tensor_name;
    entry->primary_tensor_role = ref->primary_tensor_role;
    entry->primary_tensor_dtype = ref->primary_tensor_dtype;
    entry->primary_tensor_rank = ref->primary_tensor_rank;
    entry->primary_tensor_dims = ref->primary_tensor_dims;
    entry->primary_tensor_bytes = ref->primary_tensor_bytes;
    entry->support_level = ref->support_level;
    entry->selected_embedding_ready = ref->selected_embedding_ready;
    entry->selected_embedding_hidden_size = ref->selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = ref->selected_embedding_vocab_size;
    entry->selected_embedding_output_count = ref->selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = ref->selected_embedding_slice_bytes;
    entry->execution_ready = ref->execution_ready;
}

void print_metadata_drift_cli(const yvex_model_metadata_drift_report *report)
{
    unsigned int i;

    if (!report) return;
    printf("metadata_status: %s\n", report->metadata_status[0] ? report->metadata_status : "unknown");
    printf("readiness_status: %s\n", report->readiness_status[0] ? report->readiness_status : "unknown");
    for (i = 0; i < report->issue_count; ++i) {
        printf("metadata_issue_%u_code: %s\n", i, report->issues[i].code);
        printf("metadata_issue_%u_registered: %s\n", i, report->issues[i].registered_value);
        printf("metadata_issue_%u_current: %s\n", i, report->issues[i].current_value);
    }
}

int populate_registry_metadata(yvex_cli_metadata_snapshot *snapshot,
                                      const char *path,
                                      yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *primary = NULL;
    const yvex_tensor_info *embedding = NULL;
    yvex_selected_embedding_shape selected_shape;
    unsigned long long known_bytes = 0ull;
    unsigned long long i;
    int rc;

    if (!snapshot || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_metadata",
                       "metadata snapshot and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    snprintf(snapshot->format, sizeof(snapshot->format), "gguf");
    snprintf(snapshot->architecture, sizeof(snapshot->architecture), "%s",
             yvex_arch_name(yvex_model_arch(ctx.model)));

    for (i = 0; i < yvex_tensor_table_count(ctx.table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) {
            continue;
        }
        known_bytes += tensor->storage_bytes;
        if (!primary && strcmp(tensor->name, "token_embd.weight") == 0) {
            primary = tensor;
            embedding = tensor;
        }
    }
    if (!primary && yvex_tensor_table_count(ctx.table) > 0ull) {
        primary = yvex_tensor_table_at(ctx.table, 0);
    }

    if (primary) {
        snprintf(snapshot->primary_tensor_name, sizeof(snapshot->primary_tensor_name),
                 "%s", primary->name ? primary->name : "");
        snprintf(snapshot->primary_tensor_role, sizeof(snapshot->primary_tensor_role),
                 "%s", yvex_tensor_role_name(primary->role));
        snprintf(snapshot->primary_tensor_dtype, sizeof(snapshot->primary_tensor_dtype),
                 "%s", yvex_dtype_name(primary->dtype));
        dims_to_text(primary->dims, primary->rank, snapshot->primary_tensor_dims,
                     sizeof(snapshot->primary_tensor_dims));
        snapshot->entry.primary_tensor_rank = primary->rank;
        snapshot->entry.primary_tensor_bytes = primary->storage_bytes;
    }

    if (embedding) {
        yvex_error shape_err;
        yvex_error_clear(&shape_err);
        memset(&selected_shape, 0, sizeof(selected_shape));
        if (yvex_selected_embedding_shape_validate(embedding, 0u, &selected_shape,
                                                   &shape_err) == YVEX_OK) {
            snapshot->entry.selected_embedding_ready = 1;
            snapshot->entry.selected_embedding_hidden_size = selected_shape.hidden_size;
            snapshot->entry.selected_embedding_vocab_size = selected_shape.vocab_size;
            snapshot->entry.selected_embedding_output_count = selected_shape.output_count;
            snapshot->entry.selected_embedding_slice_bytes = selected_shape.slice_bytes;
        } else {
            yvex_error_clear(&shape_err);
        }
    }

    snapshot->entry.path = path;
    snapshot->entry.format = snapshot->format;
    snapshot->entry.architecture = snapshot->architecture;
    snapshot->entry.tensor_count = yvex_tensor_table_count(ctx.table);
    snapshot->entry.known_tensor_bytes = known_bytes;
    snapshot->entry.primary_tensor_name = snapshot->primary_tensor_name;
    snapshot->entry.primary_tensor_role = snapshot->primary_tensor_role;
    snapshot->entry.primary_tensor_dtype = snapshot->primary_tensor_dtype;
    snapshot->entry.primary_tensor_dims = snapshot->primary_tensor_dims;
    snprintf(snapshot->support_level, sizeof(snapshot->support_level), "%s",
             current_support_from_metadata(&snapshot->entry));
    snapshot->entry.support_level = snapshot->support_level;

    close_model_context(&ctx);
    return YVEX_OK;
}

static int populate_registry_identity(yvex_model_registry_entry *entry,
                                      char sha256[YVEX_SHA256_HEX_CAP],
                                      char format[16],
                                      char architecture[64],
                                      char primary_name[128],
                                      char primary_role[64],
                                      char primary_dtype[32],
                                      char primary_dims[128],
                                      yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot snapshot;
    int rc;

    if (!entry || !entry->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_add_identity",
                       "registry entry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_artifact_identity_read(entry->path, &identity, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = populate_registry_metadata(&snapshot, entry->path, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    snprintf(sha256, YVEX_SHA256_HEX_CAP, "%s", identity.sha256);
    snprintf(format, 16u, "%s", snapshot.format);
    snprintf(architecture, 64u, "%s", snapshot.architecture);
    snprintf(primary_name, 128u, "%s", snapshot.primary_tensor_name);
    snprintf(primary_role, 64u, "%s", snapshot.primary_tensor_role);
    snprintf(primary_dtype, 32u, "%s", snapshot.primary_tensor_dtype);
    snprintf(primary_dims, 128u, "%s", snapshot.primary_tensor_dims);

    entry->sha256 = sha256;
    entry->file_size = identity.file_size;
    entry->format = format;
    entry->architecture = architecture;
    entry->tensor_count = snapshot.entry.tensor_count;
    entry->known_tensor_bytes = snapshot.entry.known_tensor_bytes;
    entry->primary_tensor_name = primary_name;
    entry->primary_tensor_role = primary_role;
    entry->primary_tensor_dtype = primary_dtype;
    entry->primary_tensor_rank = snapshot.entry.primary_tensor_rank;
    entry->primary_tensor_dims = primary_dims;
    entry->primary_tensor_bytes = snapshot.entry.primary_tensor_bytes;
    entry->selected_embedding_ready = snapshot.entry.selected_embedding_ready;
    entry->selected_embedding_hidden_size = snapshot.entry.selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = snapshot.entry.selected_embedding_vocab_size;
    entry->selected_embedding_output_count = snapshot.entry.selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = snapshot.entry.selected_embedding_slice_bytes;

    return YVEX_OK;
}

static int parse_models_registry_option(int argc, char **argv, int start, const char **registry_path)
{
    int i;

    for (i = start; i < argc; ++i) {
        if (strcmp(argv[i], "--registry") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: models --registry requires a file\n");
                return 2;
            }
            *registry_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            /* Reserved for future machine-readable output; text output remains canonical. */
        } else {
            fprintf(stderr, "yvex: unknown models option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_models_add_options(int argc, char **argv,
                                    yvex_cli_models_add_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: models add option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--registry") == 0) options->registry_path = argv[++i];
        else if (strcmp(argv[i], "--path") == 0) options->path = argv[++i];
        else if (strcmp(argv[i], "--alias") == 0) options->alias = argv[++i];
        else if (strcmp(argv[i], "--family") == 0) options->family = argv[++i];
        else if (strcmp(argv[i], "--model") == 0) options->model = argv[++i];
        else if (strcmp(argv[i], "--scope") == 0) options->scope = argv[++i];
        else if (strcmp(argv[i], "--class") == 0) options->artifact_class = argv[++i];
        else if (strcmp(argv[i], "--qprofile") == 0) options->qprofile = argv[++i];
        else if (strcmp(argv[i], "--calibration") == 0) options->calibration = argv[++i];
        else if (strcmp(argv[i], "--sha256") == 0) options->sha256 = argv[++i];
        else if (strcmp(argv[i], "--support-level") == 0) options->support_level = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown models add option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int command_models_scan(int argc, char **argv)
{
    yvex_model_registry_entry *entries = NULL;
    yvex_error err;
    const char *root = NULL;
    const char *registry_path = NULL;
    unsigned long long count = 0;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);
    for (i = 3; (int)i < argc; ++i) {
        if (strcmp(argv[i], "--root") == 0) {
            if ((int)i + 1 >= argc) {
                fprintf(stderr, "yvex: models scan --root requires a directory\n");
                return 2;
            }
            root = argv[++i];
        } else if (strcmp(argv[i], "--registry") == 0) {
            if ((int)i + 1 >= argc) {
                fprintf(stderr, "yvex: models scan --registry requires a file\n");
                return 2;
            }
            registry_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            /* Reserved for model selection work compatibility; text output remains canonical. */
        } else {
            fprintf(stderr, "yvex: unknown models scan option: %s\n", argv[i]);
            return 2;
        }
    }
    (void)registry_path;
    if (!root) {
        fprintf(stderr, "yvex: models scan requires --root DIR\n");
        return 2;
    }
    rc = yvex_model_registry_scan_root(root, &entries, &count, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    printf("models: scan\n");
    printf("root: %s\n", root);
    for (i = 0; i < count; ++i) {
        if (i > 0) printf("\n");
        print_model_registry_scan_entry_cli(&entries[i]);
    }
    printf("candidates: %llu\n", count);
    printf("status: models-scan\n");
    yvex_model_registry_scan_free(entries, count);
    return 0;
}

static int command_models_add(int argc, char **argv)
{
    yvex_cli_models_add_options cli_options;
    yvex_model_registry *registry = NULL;
    yvex_model_registry_entry derived;
    yvex_model_registry_entry entry;
    yvex_error err;
    char registered_sha256[YVEX_SHA256_HEX_CAP];
    char registered_format[16];
    char registered_architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    int have_derived = 0;
    int rc;

    yvex_error_clear(&err);
    memset(&derived, 0, sizeof(derived));
    memset(&entry, 0, sizeof(entry));
    memset(registered_sha256, 0, sizeof(registered_sha256));
    memset(registered_format, 0, sizeof(registered_format));
    memset(registered_architecture, 0, sizeof(registered_architecture));
    memset(primary_tensor_name, 0, sizeof(primary_tensor_name));
    memset(primary_tensor_role, 0, sizeof(primary_tensor_role));
    memset(primary_tensor_dtype, 0, sizeof(primary_tensor_dtype));
    memset(primary_tensor_dims, 0, sizeof(primary_tensor_dims));
    rc = parse_models_add_options(argc, argv, &cli_options);
    if (rc != 0) return rc;
    if (!cli_options.path) {
        fprintf(stderr, "yvex: models add requires --path FILE\n");
        return 2;
    }
    if (yvex_model_registry_entry_derive_from_path(&derived, cli_options.path, &err) == YVEX_OK) {
        have_derived = 1;
    } else {
        yvex_error_clear(&err);
    }
    if (!cli_options.alias && !have_derived) {
        fprintf(stderr, "yvex: models add requires --alias when filename is not canonical\n");
        return 2;
    }
    entry.alias = cli_options.alias ? cli_options.alias : derived.alias;
    entry.family = cli_options.family ? cli_options.family : (have_derived ? derived.family : "");
    entry.model = cli_options.model ? cli_options.model : (have_derived ? derived.model : "");
    entry.scope = cli_options.scope ? cli_options.scope : (have_derived ? derived.scope : "");
    entry.artifact_class = cli_options.artifact_class ? cli_options.artifact_class : (have_derived ? derived.artifact_class : "");
    entry.qprofile = cli_options.qprofile ? cli_options.qprofile : (have_derived ? derived.qprofile : "");
    entry.calibration = cli_options.calibration ? cli_options.calibration : (have_derived ? derived.calibration : "");
    entry.producer = have_derived ? derived.producer : "yvex";
    entry.schema_version = have_derived ? derived.schema_version : "v1";
    entry.path = cli_options.path;
    entry.sha256 = "";
    entry.file_size = 0ull;
    entry.format = "";
    entry.architecture = "";
    entry.tensor_count = 0ull;
    entry.known_tensor_bytes = 0ull;
    entry.primary_tensor_name = "";
    entry.primary_tensor_role = "";
    entry.primary_tensor_dtype = "";
    entry.primary_tensor_rank = 0u;
    entry.primary_tensor_dims = "";
    entry.primary_tensor_bytes = 0ull;
    entry.support_level = cli_options.support_level ? cli_options.support_level : "";
    entry.selected_embedding_ready = 0;
    entry.selected_embedding_hidden_size = 0ull;
    entry.selected_embedding_vocab_size = 0ull;
    entry.selected_embedding_output_count = 0ull;
    entry.selected_embedding_slice_bytes = 0ull;
    entry.execution_ready = 0;

    rc = populate_registry_identity(&entry,
                                    registered_sha256,
                                    registered_format,
                                    registered_architecture,
                                    primary_tensor_name,
                                    primary_tensor_role,
                                    primary_tensor_dtype,
                                    primary_tensor_dims,
                                    &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (cli_options.sha256 && cli_options.sha256[0] &&
        strcmp(cli_options.sha256, registered_sha256) != 0) {
        yvex_error_setf(&err, YVEX_ERR_STATE, "models_add_identity",
                        "sha256 mismatch: expected %s got %s",
                        cli_options.sha256, registered_sha256);
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_STATE));
    }

    rc = models_registry_open(&registry, cli_options.registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_add(registry, &entry, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, cli_options.registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: add\n");
    printf("alias: %s\n", entry.alias);
    printf("path: %s\n", entry.path);
    printf("registered_file_size: %llu\n", entry.file_size);
    printf("registered_sha256: %s\n", entry.sha256);
    printf("registered_format: %s\n", entry.format);
    printf("registered_architecture: %s\n", entry.architecture);
    printf("registered_tensor_count: %llu\n", entry.tensor_count);
    printf("registered_known_tensor_bytes: %llu\n", entry.known_tensor_bytes);
    printf("registered_primary_tensor: %s\n", entry.primary_tensor_name);
    printf("registered_primary_role: %s\n", entry.primary_tensor_role);
    printf("registered_primary_dtype: %s\n", entry.primary_tensor_dtype);
    printf("registered_primary_rank: %u\n", entry.primary_tensor_rank);
    printf("registered_primary_dims: %s\n", entry.primary_tensor_dims);
    printf("registered_primary_bytes: %llu\n", entry.primary_tensor_bytes);
    printf("registered_selected_embedding_ready: %s\n",
           entry.selected_embedding_ready ? "true" : "false");
    printf("registered_selected_embedding_hidden_size: %llu\n",
           entry.selected_embedding_hidden_size);
    printf("registered_selected_embedding_vocab_size: %llu\n",
           entry.selected_embedding_vocab_size);
    printf("registered_selected_embedding_output_count: %llu\n",
           entry.selected_embedding_output_count);
    printf("registered_selected_embedding_slice_bytes: %llu\n",
           entry.selected_embedding_slice_bytes);
    printf("identity_status: recorded\n");
    printf("status: models-added\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_list(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *selected;
    char selected_alias[256];
    unsigned long long i;
    unsigned long long count;
    int rc;

    yvex_error_clear(&err);
    rc = parse_models_registry_option(argc, argv, 3, &registry_path);
    if (rc != 0) return rc;
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    selected_alias[0] = '\0';
    if (selected && selected->alias) {
        snprintf(selected_alias, sizeof(selected_alias), "%s", selected->alias);
    }
    count = yvex_model_registry_count(registry);
    printf("models: list\n");
    for (i = 0; i < count; ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        int is_selected = selected_alias[0] && entry && strcmp(selected_alias, entry->alias) == 0;
        print_model_registry_entry_cli(entry, is_selected);
    }
    printf("count: %llu\n", count);
    printf("status: models-list\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_use(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models use requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_select(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: use\n");
    printf("selected: %s\n", alias);
    printf("status: models-selected\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_current(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *selected;
    int rc;

    rc = parse_models_registry_option(argc, argv, 3, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    printf("models: current\n");
    if (selected) {
        printf("selected: %s\n", selected->alias);
        printf("path: %s\n", selected->path);
        printf("registered_file_size: %llu\n", selected->file_size);
        printf("registered_sha256: %s\n", selected->sha256 && selected->sha256[0] ? selected->sha256 : "absent");
        printf("registered_format: %s\n", selected->format ? selected->format : "");
        printf("registered_architecture: %s\n", selected->architecture ? selected->architecture : "");
        printf("registered_tensor_count: %llu\n", selected->tensor_count);
        printf("registered_known_tensor_bytes: %llu\n", selected->known_tensor_bytes);
        printf("registered_primary_tensor: %s\n", selected->primary_tensor_name ? selected->primary_tensor_name : "");
        printf("registered_primary_role: %s\n", selected->primary_tensor_role ? selected->primary_tensor_role : "");
        printf("registered_primary_dtype: %s\n", selected->primary_tensor_dtype ? selected->primary_tensor_dtype : "");
        printf("registered_primary_rank: %u\n", selected->primary_tensor_rank);
        printf("registered_primary_dims: %s\n", selected->primary_tensor_dims ? selected->primary_tensor_dims : "");
        printf("metadata_status: %s\n",
               selected->primary_tensor_name && selected->primary_tensor_name[0] ? "recorded" : "missing");
        printf("execution_ready: %s\n", selected->execution_ready ? "true" : "false");
        printf("status: models-current\n");
    } else {
        printf("selected: none\n");
        printf("status: models-none\n");
    }
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_verify(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *entry;
    const char *alias;
    const char *identity_status = "unknown";
    const char *digest_status = "unknown";
    const char *metadata_status = "not-checked";
    const char *readiness_status = "not-checked";
    const char *status = "models-identity-fail";
    const char *reason = "";
    int pass = 0;
    int metadata_checked = 0;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models verify requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        fprintf(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }

    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(entry->path, &identity, &err);
    if (rc != YVEX_OK) {
        identity_status = "fail";
        digest_status = "fail";
        reason = yvex_error_message(&err);
    } else if (!entry->sha256 || !entry->sha256[0] ||
               !yvex_sha256_hex_is_valid(entry->sha256)) {
        identity_status = "missing";
        digest_status = "missing";
        reason = "registered alias lacks digest identity; re-add model";
    } else if (strcmp(entry->sha256, identity.sha256) != 0 ||
               (entry->file_size != 0ull && entry->file_size != identity.file_size)) {
        identity_status = "fail";
        digest_status = "fail";
        reason = "digest mismatch for registered alias";
    } else {
        identity_status = "pass";
        digest_status = "pass";
        reason = "current file identity matches registered alias";
        pass = 1;
        memset(&current_metadata, 0, sizeof(current_metadata));
        memset(&metadata_report, 0, sizeof(metadata_report));
        rc = populate_registry_metadata(&current_metadata, entry->path, &err);
        if (rc != YVEX_OK) {
            metadata_status = "fail";
            readiness_status = "fail";
            reason = "current artifact metadata could not be parsed";
            pass = 0;
            status = "models-metadata-drift";
        } else {
            rc = yvex_model_registry_compare_metadata(entry,
                                                      &current_metadata.entry,
                                                      &metadata_report,
                                                      &err);
            metadata_checked = 1;
            if (rc != YVEX_OK) {
                metadata_status = "fail";
                readiness_status = "fail";
                reason = yvex_error_message(&err);
                pass = 0;
                status = "models-metadata-drift";
            } else {
                metadata_status = metadata_report.metadata_status;
                readiness_status = metadata_report.readiness_status;
                if (strcmp(metadata_status, "pass") == 0 &&
                    strcmp(readiness_status, "pass") == 0) {
                    status = "models-identity-pass";
                } else if (strcmp(metadata_status, "missing") == 0 ||
                           strcmp(readiness_status, "missing") == 0) {
                    reason = "registered alias lacks metadata summary; re-add model";
                    pass = 0;
                    status = "models-metadata-missing";
                } else {
                    reason = "registered alias metadata does not match current artifact facts";
                    pass = 0;
                    status = "models-metadata-drift";
                }
            }
        }
    }
    if (strcmp(identity_status, "missing") == 0) {
        status = "models-identity-missing";
    } else if (strcmp(identity_status, "fail") == 0) {
        status = "models-identity-fail";
    }

    printf("models: verify\n");
    printf("alias: %s\n", entry->alias);
    printf("path: %s\n", entry->path);
    printf("registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
    printf("registered_file_size: %llu\n", entry->file_size);
    printf("current_file_size: %llu\n", identity.file_size);
    printf("digest_status: %s\n", digest_status);
    printf("identity_status: %s\n", identity_status);
    printf("registered_support_level: %s\n", entry->support_level ? entry->support_level : "");
    printf("current_support_level: %s\n",
           metadata_checked ? current_metadata.entry.support_level : "not-checked");
    printf("registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("current_architecture: %s\n",
           metadata_checked ? current_metadata.entry.architecture : "not-checked");
    printf("registered_tensor_count: %llu\n", entry->tensor_count);
    printf("current_tensor_count: %llu\n",
           metadata_checked ? current_metadata.entry.tensor_count : 0ull);
    printf("registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("current_known_tensor_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.known_tensor_bytes : 0ull);
    printf("registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("current_primary_tensor: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_name : "not-checked");
    printf("registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("current_primary_role: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_role : "not-checked");
    printf("registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("current_primary_dtype: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_dtype : "not-checked");
    printf("registered_primary_rank: %u\n", entry->primary_tensor_rank);
    printf("current_primary_rank: %u\n",
           metadata_checked ? current_metadata.entry.primary_tensor_rank : 0u);
    printf("registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("current_primary_dims: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_dims : "not-checked");
    printf("registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("current_primary_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.primary_tensor_bytes : 0ull);
    printf("registered_selected_embedding_ready: %s\n",
           entry->selected_embedding_ready ? "true" : "false");
    printf("current_selected_embedding_ready: %s\n",
           metadata_checked && current_metadata.entry.selected_embedding_ready ? "true" : "false");
    printf("registered_selected_embedding_hidden_size: %llu\n",
           entry->selected_embedding_hidden_size);
    printf("current_selected_embedding_hidden_size: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_hidden_size : 0ull);
    printf("registered_selected_embedding_vocab_size: %llu\n",
           entry->selected_embedding_vocab_size);
    printf("current_selected_embedding_vocab_size: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_vocab_size : 0ull);
    printf("registered_selected_embedding_output_count: %llu\n",
           entry->selected_embedding_output_count);
    printf("current_selected_embedding_output_count: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_output_count : 0ull);
    printf("registered_selected_embedding_slice_bytes: %llu\n",
           entry->selected_embedding_slice_bytes);
    printf("current_selected_embedding_slice_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_slice_bytes : 0ull);
    if (metadata_checked) {
        print_metadata_drift_cli(&metadata_report);
    } else {
        printf("metadata_status: %s\n", metadata_status);
        printf("readiness_status: %s\n", readiness_status);
    }
    printf("reason: %s\n", reason);
    printf("status: %s\n", status);
    yvex_model_registry_close(registry);
    return pass ? 0 : exit_for_status(YVEX_ERR_STATE);
}

static int command_models_remove(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models remove requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_remove(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: remove\n");
    printf("removed: %s\n", alias);
    printf("status: models-removed\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_inspect(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const yvex_model_registry_entry *entry;
    const yvex_gguf_header *header;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models inspect requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        fprintf(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }
    printf("models: inspect\n");
    printf("alias: %s\n", entry->alias);
    printf("path: %s\n", entry->path);
    printf("family: %s\n", entry->family);
    printf("model: %s\n", entry->model);
    printf("scope: %s\n", entry->scope);
    printf("artifact_class: %s\n", entry->artifact_class);
    printf("qprofile: %s\n", entry->qprofile);
    printf("calibration: %s\n", entry->calibration);
    printf("support_level: %s\n", entry->support_level);
    printf("registered_file_size: %llu\n", entry->file_size);
    printf("registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("registered_format: %s\n", entry->format ? entry->format : "");
    printf("registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("registered_tensor_count: %llu\n", entry->tensor_count);
    printf("registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("primary_tensor_name: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("primary_tensor_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("primary_tensor_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("primary_tensor_rank: %u\n", entry->primary_tensor_rank);
    printf("primary_tensor_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("primary_tensor_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("selected_embedding_ready: %s\n", entry->selected_embedding_ready ? "true" : "false");
    printf("selected_embedding_hidden_size: %llu\n", entry->selected_embedding_hidden_size);
    printf("selected_embedding_vocab_size: %llu\n", entry->selected_embedding_vocab_size);
    printf("selected_embedding_output_count: %llu\n", entry->selected_embedding_output_count);
    printf("selected_embedding_slice_bytes: %llu\n", entry->selected_embedding_slice_bytes);
    printf("execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    rc = open_model_context(entry->path, &ctx, &err);
    if (rc == YVEX_OK) {
        header = yvex_gguf_header_view(ctx.gguf);
        printf("gguf:\n");
        printf("  version: %u\n", header->version);
        printf("  tensor_count: %llu\n", header->tensor_count);
        close_model_context(&ctx);
    } else {
        printf("gguf:\n");
        printf("  status: unavailable\n");
        printf("  reason: %s\n", yvex_error_message(&err));
        yvex_error_clear(&err);
    }
    printf("status: models-inspect\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models(int argc, char **argv)
{
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_models_help(stdout);
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: models requires scan, add, list, use, current, verify, inspect, or remove\n");
        return 2;
    }
    if (strcmp(argv[2], "scan") == 0) return command_models_scan(argc, argv);
    if (strcmp(argv[2], "add") == 0) return command_models_add(argc, argv);
    if (strcmp(argv[2], "list") == 0) return command_models_list(argc, argv);
    if (strcmp(argv[2], "use") == 0) return command_models_use(argc, argv);
    if (strcmp(argv[2], "current") == 0) return command_models_current(argc, argv);
    if (strcmp(argv[2], "verify") == 0) return command_models_verify(argc, argv);
    if (strcmp(argv[2], "inspect") == 0) return command_models_inspect(argc, argv);
    if (strcmp(argv[2], "remove") == 0) return command_models_remove(argc, argv);
    fprintf(stderr, "yvex: unknown models subcommand: %s\n", argv[2]);
    return 2;
}

int yvex_models_command(int argc, char **argv)
{
    return command_models(argc, argv);
}

void yvex_models_help(FILE *fp)
{
    fprintf(fp, "usage: yvex models scan --root DIR [--registry FILE]\n");
    fprintf(fp, "       yvex models add --path FILE [--alias ALIAS] [--support-level LEVEL] [--registry FILE]\n");
    fprintf(fp, "       yvex models list|current [--registry FILE]\n");
    fprintf(fp, "       yvex models use|verify|inspect|remove ALIAS [--registry FILE]\n");
    fprintf(fp, "\nModels manages the local alias registry, including digest identity and metadata drift facts for registered artifacts.\n");
}
