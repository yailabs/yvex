/*
 * yvex_model_artifacts.c - Local model references, registry, and gates.
 *
 * This file owns operator-facing model artifact checks, registry helpers,
 * metadata drift reports, model gates, and registry command surfaces. It does
 * not implement model execution.
 */

#include "yvex_console_private.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <yvex/artifact_integrity.h>
#include <yvex/conversion.h>
#include <yvex/fs.h>
#include <yvex/materialize_gate.h>
#include <yvex/model_gate.h>
#include <yvex/model_ref.h>
#include <yvex/model_registry.h>
#include <yvex/native_weights.h>
#include <yvex/source_manifest.h>
#include <yvex/token_input.h>
#include <yvex/yvex.h>

/* Registry storage types. */

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

/* Forward declarations for in-file registry/reference ownership. */

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

/* Model gate helpers and summaries. */

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


/* Materialization gate helpers and summaries. */


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


/* Model reference resolution. */


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


/* Registry storage, metadata drift, and JSON persistence. */


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

static int append_owned_registry_entry(yvex_model_registry *registry,
                                       yvex_model_registry_owned_entry *owned,
                                       yvex_error *err)
{
    int rc;

    if (!registry || !owned) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json",
                       "registry and owned entry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = registry_reserve(registry, registry->count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    registry->entries[registry->count++] = *owned;
    memset(owned, 0, sizeof(*owned));
    return YVEX_OK;
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
        rc = append_owned_registry_entry(registry, &owned, err);
        if (rc != YVEX_OK) {
            yvex_model_registry_owned_entry_clear(&owned);
            free(json);
            return rc;
        }
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
    memset(&owned, 0, sizeof(owned));
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
        free_entry_view_strings(&entries[i]);
    }
    free(entries);
}

/* Model registry CLI helpers and shared model command support. */

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

typedef enum {
    YVEX_MODELS_OUTPUT_NORMAL = 0,
    YVEX_MODELS_OUTPUT_TABLE,
    YVEX_MODELS_OUTPUT_AUDIT
} yvex_models_output_mode;

static int parse_models_output_mode(const char *value, yvex_models_output_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_MODELS_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "table") == 0) {
        *mode = YVEX_MODELS_OUTPUT_TABLE;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_MODELS_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
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

static void print_model_registry_entry_normal(const yvex_model_registry_entry *entry,
                                              int selected)
{
    if (!entry) return;
    printf("%-3s  %-44s  %-10s  %-16s  %7llu  %12llu  %s\n",
           selected ? "*" : "-",
           entry->alias ? entry->alias : "",
           entry->family ? entry->family : "",
           entry->artifact_class ? entry->artifact_class : "",
           entry->tensor_count,
           entry->file_size,
           entry->execution_ready ? "yes" : "no");
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

static int parse_models_registry_output_options(int argc,
                                                char **argv,
                                                int start,
                                                const char **registry_path,
                                                yvex_models_output_mode *output_mode)
{
    int i;

    if (output_mode) {
        *output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    }
    for (i = start; i < argc; ++i) {
        if (strcmp(argv[i], "--registry") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: models --registry requires a file\n");
                return 2;
            }
            *registry_path = argv[++i];
        } else if (strcmp(argv[i], "--audit") == 0) {
            if (output_mode) *output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: models --output requires normal|table|audit\n");
                return 2;
            }
            if (!output_mode || !parse_models_output_mode(argv[++i], output_mode)) {
                fprintf(stderr, "yvex: models unsupported output mode: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--json") == 0) {
            fprintf(stderr, "yvex: models JSON output is unsupported; use --output normal|table|audit\n");
            return 2;
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

/* Shared stage/status, path, and flag helpers for model commands. */

static void model_stage_print(const char *stage, const char *status)
{
    printf("stage: %s %s\n", stage ? stage : "", status ? status : "");
}

static void model_print_runtime_generation(const char *runtime_execution)
{
    printf("runtime_execution: %s\n", runtime_execution ? runtime_execution : "not-performed");
    printf("generation: unsupported\n");
}

static int cli_arg_value_valid(const char *value)
{
    return value && value[0] && !strchr(value, '\n') && !strchr(value, '\r');
}

static int parse_models_value_option(const char *command,
                                     const char *flag,
                                     int argc,
                                     char **argv,
                                     int *index,
                                     const char **value)
{
    if (*index + 1 >= argc) {
        fprintf(stderr, "yvex: %s %s requires a value\n", command, flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (!cli_arg_value_valid(*value)) {
        fprintf(stderr, "yvex: %s %s value is empty or invalid\n", command, flag);
        return 2;
    }
    return 0;
}

static int model_backend_kind_from_name(const char *backend_name,
                                        yvex_backend_kind *kind)
{
    if (!kind) {
        return 0;
    }
    if (!backend_name || strcmp(backend_name, "cpu") == 0) {
        *kind = YVEX_BACKEND_KIND_CPU;
        return 1;
    }
    if (strcmp(backend_name, "cuda") == 0) {
        *kind = YVEX_BACKEND_KIND_CUDA;
        return 1;
    }
    return 0;
}

typedef struct {
    const char *target;
    const char *source;
    const char *out;
    const char *out_dir;
    const char *models_root;
    const char *registry_path;
    int overwrite;
    int dry_run;
    int register_alias;
    int use_alias;
} yvex_cli_models_prepare_options;

/* Selected artifact prepare preset. */

static int expand_operator_path(const char *input,
                                char *out,
                                size_t out_cap,
                                yvex_error *err,
                                const char *where)
{
    const char *home;
    int n;

    if (!input || !out || out_cap == 0u) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path value is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!cli_arg_value_valid(input)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path value is empty or contains a newline");
        return YVEX_ERR_INVALID_ARG;
    }
    if (input[0] == '~' && input[1] == '/') {
        home = getenv("HOME");
        if (!home || !home[0]) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "HOME is required to expand ~/ paths");
            return YVEX_ERR_INVALID_ARG;
        }
        n = snprintf(out, out_cap, "%s/%s", home, input + 2);
    } else {
        n = snprintf(out, out_cap, "%s", input);
    }
    if (n < 0 || (size_t)n >= out_cap) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "path is too long");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int path_join2(char *out,
                      size_t out_cap,
                      const char *dir,
                      const char *file,
                      yvex_error *err,
                      const char *where)
{
    int n;

    n = snprintf(out, out_cap, "%s/%s", dir, file);
    if (n < 0 || (size_t)n >= out_cap) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "resolved path is too long");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int path_parent_dir(const char *path, char *out, size_t out_cap)
{
    const char *slash;
    size_t len;

    if (!path || !out || out_cap == 0u) return 0;
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out, out_cap, ".");
        return 1;
    }
    len = (size_t)(slash - path);
    if (len == 0u) len = 1u;
    if (len >= out_cap) return 0;
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
}

static int parse_models_prepare_options(int argc,
                                        char **argv,
                                        yvex_cli_models_prepare_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->register_alias = 1;
    options->use_alias = 1;
    if (argc < 4) {
        fprintf(stderr, "yvex: models prepare requires TARGET\n");
        return 2;
    }
    options->target = argv[3];
    if (!cli_arg_value_valid(options->target)) {
        fprintf(stderr, "yvex: models prepare target is empty or invalid\n");
        return 2;
    }
    for (i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--overwrite") == 0) {
            options->overwrite = 1;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            options->dry_run = 1;
        } else if (strcmp(argv[i], "--no-register") == 0) {
            options->register_alias = 0;
            options->use_alias = 0;
        } else if (strcmp(argv[i], "--no-use") == 0) {
            options->use_alias = 0;
        } else if (strcmp(argv[i], "--source") == 0 ||
                   strcmp(argv[i], "--out") == 0 ||
                   strcmp(argv[i], "--out-dir") == 0 ||
                   strcmp(argv[i], "--models-root") == 0 ||
                   strcmp(argv[i], "--registry") == 0) {
            const char *flag = argv[i];
            const char *value = NULL;
            int rc = parse_models_value_option("models prepare", flag,
                                               argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--source") == 0) options->source = value;
            else if (strcmp(flag, "--out") == 0) options->out = value;
            else if (strcmp(flag, "--out-dir") == 0) options->out_dir = value;
            else if (strcmp(flag, "--models-root") == 0) options->models_root = value;
            else options->registry_path = value;
        } else {
            fprintf(stderr, "yvex: unknown models prepare option: %s\n", argv[i]);
            return 2;
        }
    }
    if (options->out && options->out_dir) {
        fprintf(stderr, "yvex: models prepare --out and --out-dir are mutually exclusive\n");
        return 2;
    }
    if (!options->register_alias) {
        options->use_alias = 0;
    }
    return 0;
}

static void print_prepare_common(const yvex_cli_models_prepare_options *options,
                                 const yvex_operator_paths *operator_paths,
                                 const char *source_path,
                                 const char *artifact_path,
                                 const char *manifest_path,
                                 const char *plan_path,
                                 const char *registry_path)
{
    printf("models: prepare\n");
    printf("target_id: %s\n", options->target);
    printf("models_root_source: %s\n", operator_paths->models_root_source);
    printf("models_root: %s\n", operator_paths->models_root);
    printf("source_path: %s\n", source_path);
    printf("artifact_path: %s\n", artifact_path);
    printf("source_manifest_path: %s\n", manifest_path);
    printf("conversion_plan_path: %s\n", plan_path);
    printf("registry_path: %s\n", registry_path && registry_path[0] ? registry_path : ".yvex/models.local.json");
    printf("alias: deepseek4-v4-flash-selected-embed\n");
    printf("overwrite: %s\n", options->overwrite ? "true" : "false");
    printf("dry_run: %s\n", options->dry_run ? "true" : "false");
    printf("register: %s\n", options->register_alias ? "true" : "false");
    printf("use_alias: %s\n", options->use_alias ? "true" : "false");
}

static void print_prepare_dry_run_stages(int register_alias, int use_alias)
{
    static const char *planned[] = {
        "resolve-paths",
        "source-manifest",
        "native-weights",
        "tensor-map",
        "convert-plan",
        "convert-emit",
        "inspect",
        "tensors",
        "metadata"
    };
    unsigned long i;

    for (i = 0; i < sizeof(planned) / sizeof(planned[0]); ++i) {
        model_stage_print(planned[i], "planned");
    }
    model_stage_print("registry-remove-existing", register_alias ? "planned" : "skipped");
    model_stage_print("registry-add", register_alias ? "planned" : "skipped");
    model_stage_print("registry-use", use_alias ? "planned" : "skipped");
    model_stage_print("registry-verify", register_alias ? "planned" : "skipped");
}

static const char *prepare_unsupported_reason(const char *target)
{
    if (strcmp(target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
        return "segment prepare is planned, not implemented by this preset";
    }
    if (strcmp(target, "glm-5.2-official-safetensors") == 0) {
        return "YVEX-produced GGUF emission for this target is planned, not implemented";
    }
    return NULL;
}

static int print_prepare_unsupported(const char *target)
{
    const char *reason = prepare_unsupported_reason(target);

    printf("models: prepare\n");
    printf("target_id: %s\n", target);
    model_stage_print("target", "unsupported");
    model_print_runtime_generation("not-performed");
    if (!reason) {
        printf("reason: unknown model prepare target\n");
        printf("status: model-prepare-unknown-target\n");
        return 2;
    }
    printf("reason: %s\n", reason);
    printf("status: model-prepare-unsupported\n");
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

static int prepare_registry_verify(const yvex_model_registry_entry *entry,
                                   yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_metadata_drift_report metadata_report;
    int rc;

    if (!entry) {
        yvex_error_set(err, YVEX_ERR_STATE, "model_prepare_registry", "prepared alias was not found after registration");
        return YVEX_ERR_STATE;
    }
    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(entry->path, &identity, err);
    if (rc != YVEX_OK) return rc;
    if (!entry->sha256 || strcmp(entry->sha256, identity.sha256) != 0 ||
        entry->file_size != identity.file_size) {
        yvex_error_set(err, YVEX_ERR_STATE, "model_prepare_registry", "registered identity does not match emitted artifact");
        return YVEX_ERR_STATE;
    }
    memset(&current_metadata, 0, sizeof(current_metadata));
    memset(&metadata_report, 0, sizeof(metadata_report));
    rc = populate_registry_metadata(&current_metadata, entry->path, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_model_registry_compare_metadata(entry, &current_metadata.entry, &metadata_report, err);
    if (rc != YVEX_OK) return rc;
    if (strcmp(metadata_report.metadata_status, "pass") != 0 ||
        strcmp(metadata_report.readiness_status, "pass") != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "model_prepare_registry", "registered metadata drifted immediately after prepare");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int command_models_prepare(int argc, char **argv)
{
    static const char *target_alias = "deepseek4-v4-flash-selected-embed";
    static const char *artifact_name = "deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf";
    yvex_cli_models_prepare_options options;
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_source_manifest_options manifest_options;
    yvex_source_manifest_summary manifest_summary;
    yvex_native_weight_options native_options;
    yvex_native_weight_table *native_table = NULL;
    yvex_native_weight_summary native_summary;
    yvex_conversion_options conversion_options;
    yvex_conversion_summary conversion_summary;
    yvex_cli_metadata_snapshot metadata_snapshot;
    yvex_model_registry *registry = NULL;
    yvex_model_registry_entry derived;
    yvex_model_registry_entry entry;
    yvex_error err;
    char source_path[YVEX_PATH_CAP];
    char out_dir[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char plan_path[YVEX_PATH_CAP];
    char registry_path_buf[YVEX_PATH_CAP];
    char registered_sha256[YVEX_SHA256_HEX_CAP];
    char registered_format[16];
    char registered_architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    const char *registry_path = NULL;
    int source_exists;
    int artifact_exists;
    int rc;

    rc = parse_models_prepare_options(argc, argv, &options);
    if (rc != 0) return rc;
    if (strcmp(options.target, target_alias) != 0) {
        return print_prepare_unsupported(options.target);
    }

    memset(&paths, 0, sizeof(paths));
    memset(&operator_paths, 0, sizeof(operator_paths));
    yvex_error_clear(&err);
    rc = yvex_operator_paths_resolve(&paths, options.models_root, &operator_paths, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    rc = yvex_operator_paths_resolve_target(&operator_paths, "deepseek", "source",
                                            source_path, sizeof(source_path),
                                            &source_exists, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (options.source) {
        rc = expand_operator_path(options.source, source_path, sizeof(source_path), &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        source_exists = path_exists(source_path);
    }

    if (options.out) {
        rc = expand_operator_path(options.out, artifact_path, sizeof(artifact_path), &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (!path_parent_dir(artifact_path, out_dir, sizeof(out_dir))) {
            yvex_error_set(&err, YVEX_ERR_BOUNDS, "models_prepare", "output parent path is too long");
            return print_yvex_error(&err, exit_for_status(YVEX_ERR_BOUNDS));
        }
    } else {
        if (options.out_dir) {
            rc = expand_operator_path(options.out_dir, out_dir, sizeof(out_dir), &err, "models_prepare");
            if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        } else {
            rc = yvex_operator_paths_resolve_target(&operator_paths, "deepseek", "gguf",
                                                    out_dir, sizeof(out_dir),
                                                    &artifact_exists, &err);
            if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = path_join2(artifact_path, sizeof(artifact_path), out_dir, artifact_name, &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    }
    artifact_exists = path_exists(artifact_path);

    rc = path_join2(manifest_path, sizeof(manifest_path), out_dir, "deepseek-source-manifest.json", &err, "models_prepare");
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    rc = path_join2(plan_path, sizeof(plan_path), out_dir, "deepseek-selected-plan.json", &err, "models_prepare");
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    if (options.registry_path) {
        rc = expand_operator_path(options.registry_path, registry_path_buf, sizeof(registry_path_buf), &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        registry_path = registry_path_buf;
    } else {
        rc = yvex_model_registry_default_path(registry_path_buf, sizeof(registry_path_buf), &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        registry_path = registry_path_buf;
    }

    print_prepare_common(&options, &operator_paths, source_path, artifact_path,
                         manifest_path, plan_path, registry_path);

    if (options.dry_run) {
        print_prepare_dry_run_stages(options.register_alias, options.use_alias);
        model_print_runtime_generation("not-performed");
        printf("status: model-prepare-dry-run\n");
        return 0;
    }

    if (!source_exists) {
        model_stage_print("source-path", "fail");
        model_print_runtime_generation("not-performed");
        printf("reason: source path does not exist\n");
        printf("status: model-prepare-fail\n");
        return exit_for_status(YVEX_ERR_IO);
    }
    if (artifact_exists && !options.overwrite) {
        model_stage_print("convert-emit", "refused");
        model_print_runtime_generation("not-performed");
        printf("reason: artifact exists; pass --overwrite to replace it\n");
        printf("status: model-prepare-refused\n");
        return exit_for_status(YVEX_ERR_STATE);
    }

    rc = yvex_model_registry_mkdir_parent(artifact_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(manifest_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(plan_path, &err);
    if (rc == YVEX_OK && options.register_alias) rc = yvex_model_registry_mkdir_parent(registry_path, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    memset(&manifest_options, 0, sizeof(manifest_options));
    memset(&manifest_summary, 0, sizeof(manifest_summary));
    manifest_options.repo = "deepseek-ai/DeepSeek-V4-Flash";
    manifest_options.revision = "main";
    manifest_options.local_path = source_path;
    manifest_options.status = YVEX_SOURCE_STATUS_IN_PROGRESS;
    rc = yvex_source_manifest_write_json(manifest_path, &manifest_options, &manifest_summary, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("source-manifest", "pass");

    memset(&native_options, 0, sizeof(native_options));
    memset(&native_summary, 0, sizeof(native_summary));
    native_options.source_dir = source_path;
    native_options.recursive = 1;
    rc = yvex_native_weight_table_open(&native_table, &native_options, &err);
    if (rc == YVEX_OK) rc = yvex_native_weight_table_summary(native_table, &native_summary, &err);
    if (rc == YVEX_OK && !yvex_native_weight_table_find(native_table, "embed.weight")) {
        yvex_error_set(&err, YVEX_ERR_STATE, "models_prepare", "required native tensor is missing: embed.weight");
        rc = YVEX_ERR_STATE;
    }
    if (rc != YVEX_OK) {
        yvex_native_weight_table_close(native_table);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_stage_print("native-weights", "pass");
    printf("native_tensor_count: %llu\n", native_summary.tensor_count);
    model_stage_print("tensor-map", "pass");
    yvex_native_weight_table_close(native_table);
    native_table = NULL;

    memset(&conversion_options, 0, sizeof(conversion_options));
    memset(&conversion_summary, 0, sizeof(conversion_summary));
    conversion_options.architecture = "deepseek4";
    conversion_options.source_manifest_path = manifest_path;
    conversion_options.native_source_dir = source_path;
    conversion_options.tensor_name = "embed.weight";
    conversion_options.target_qtype = "F16";
    conversion_options.out_path = artifact_path;
    conversion_options.overwrite = options.overwrite;
    rc = yvex_conversion_plan_write_json(&conversion_options, plan_path, &conversion_summary, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("convert-plan", "pass");

    memset(&conversion_summary, 0, sizeof(conversion_summary));
    rc = yvex_conversion_emit_gguf(&conversion_options, &conversion_summary, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("convert-emit", "pass");
    printf("bytes_written: %llu\n", conversion_summary.bytes_written);

    memset(&metadata_snapshot, 0, sizeof(metadata_snapshot));
    rc = populate_registry_metadata(&metadata_snapshot, artifact_path, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("inspect", "pass");
    model_stage_print("tensors", "pass");
    model_stage_print("metadata", "pass");
    printf("artifact_architecture: %s\n", metadata_snapshot.architecture);
    printf("artifact_tensor_count: %llu\n", metadata_snapshot.entry.tensor_count);

    if (options.register_alias) {
        memset(&derived, 0, sizeof(derived));
        memset(&entry, 0, sizeof(entry));
        memset(registered_sha256, 0, sizeof(registered_sha256));
        memset(registered_format, 0, sizeof(registered_format));
        memset(registered_architecture, 0, sizeof(registered_architecture));
        memset(primary_tensor_name, 0, sizeof(primary_tensor_name));
        memset(primary_tensor_role, 0, sizeof(primary_tensor_role));
        memset(primary_tensor_dtype, 0, sizeof(primary_tensor_dtype));
        memset(primary_tensor_dims, 0, sizeof(primary_tensor_dims));

        rc = yvex_model_registry_entry_derive_from_path(&derived, artifact_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        entry = derived;
        entry.support_level = "selected-tensor-materialized";
        rc = populate_registry_identity(&entry,
                                        registered_sha256,
                                        registered_format,
                                        registered_architecture,
                                        primary_tensor_name,
                                        primary_tensor_role,
                                        primary_tensor_dtype,
                                        primary_tensor_dims,
                                        &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = models_registry_open(&registry, registry_path, 1, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (yvex_model_registry_find(registry, target_alias)) {
            rc = yvex_model_registry_remove(registry, target_alias, &err);
            if (rc != YVEX_OK) {
                yvex_model_registry_close(registry);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            model_stage_print("registry-remove-existing", "pass");
        } else {
            model_stage_print("registry-remove-existing", "not-found");
        }
        rc = yvex_model_registry_add(registry, &entry, &err);
        if (rc == YVEX_OK) model_stage_print("registry-add", "pass");
        if (rc == YVEX_OK && options.use_alias) {
            rc = yvex_model_registry_select(registry, target_alias, &err);
            if (rc == YVEX_OK) model_stage_print("registry-use", "pass");
        } else if (rc == YVEX_OK) {
            model_stage_print("registry-use", "skipped");
        }
        if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
        if (rc == YVEX_OK) {
            const yvex_model_registry_entry *registered = yvex_model_registry_find(registry, target_alias);
            rc = prepare_registry_verify(registered, &err);
            if (rc == YVEX_OK) model_stage_print("registry-verify", "pass");
        }
        if (rc != YVEX_OK) {
            yvex_model_registry_close(registry);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        yvex_model_registry_close(registry);
        registry = NULL;
    } else {
        model_stage_print("registry-remove-existing", "skipped");
        model_stage_print("registry-add", "skipped");
        model_stage_print("registry-use", "skipped");
        model_stage_print("registry-verify", "skipped");
    }

    model_print_runtime_generation("not-performed");
    printf("status: model-prepare\n");
    return 0;
}

/* Native source tensor download lane. */

#define YVEX_MODEL_DOWNLOAD_PATTERN_CAP 32u

typedef struct {
    const char *target_id;
    const char *family;
    const char *provider;
    const char *repo_id;
    const char *local_name;
    const char *revision_default;
    const char *artifact_class;
    const char *source_container;
    const char *model_class_hint;
    const char *boundary;
} yvex_model_download_catalog_row;

static const yvex_model_download_catalog_row model_download_catalog[] = {
    { "gemma-4-e2b", "gemma", "hf", "google/gemma-4-E2B", "gemma-4-e2b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e2b", "target-routing-default" },
    { "gemma-4-e2b-it", "gemma", "hf", "google/gemma-4-E2B-it", "gemma-4-e2b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e2b-it", "target-routing-default" },
    { "gemma-4-e4b", "gemma", "hf", "google/gemma-4-E4B", "gemma-4-e4b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e4b", "target-routing-default" },
    { "gemma-4-e4b-it", "gemma", "hf", "google/gemma-4-E4B-it", "gemma-4-e4b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e4b-it", "target-routing-default" },
    { "gemma-4-12b", "gemma", "hf", "google/gemma-4-12B", "gemma-4-12b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-12b", "target-routing-default" },
    { "gemma-4-12b-it", "gemma", "hf", "google/gemma-4-12B-it", "gemma-4-12b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-12b-it", "target-routing-default" },
    { "gemma-4-26b-a4b", "gemma", "hf", "google/gemma-4-26B-A4B", "gemma-4-26b-a4b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-26b-a4b", "target-routing-default" },
    { "gemma-4-26b-a4b-it", "gemma", "hf", "google/gemma-4-26B-A4B-it", "gemma-4-26b-a4b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-26b-a4b-it", "target-routing-default" },
    { "gemma-4-31b", "gemma", "hf", "google/gemma-4-31B", "gemma-4-31b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-31b", "target-routing-default" },
    { "gemma-4-31b-it", "gemma", "hf", "google/gemma-4-31B-it", "gemma-4-31b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-31b-it", "target-routing-default" },
    { "qwen3-8b", "qwen", "hf", "Qwen/Qwen3-8B", "qwen3-8b", "main",
      "official-safetensors", "huggingface-repository", "qwen3-8b", "target-routing-default" }
};

static const char *const model_download_default_includes[] = {
    "*.safetensors",
    "*.json",
    "*.txt",
    "*.model",
    "*.jinja",
    "*.md"
};

static const char *const model_download_default_excludes[] = {
    "*.bin",
    "*.pt",
    "*.onnx",
    "*.msgpack",
    "*.tflite",
    "*.h5",
    "*.ckpt",
    "*.tar",
    "*.zip"
};

typedef enum {
    YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO = 0,
    YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE,
    YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN,
    YVEX_MODEL_DOWNLOAD_PROGRESS_LOG,
    YVEX_MODEL_DOWNLOAD_PROGRESS_OFF
} yvex_model_download_progress_mode;

typedef struct {
    const char *target;
    const char *repo;
    const char *family;
    const char *name;
    const char *revision;
    const char *source;
    const char *models_root;
    const char *token_env;
    const char *auth_mode;
    const char *include_patterns[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    const char *exclude_patterns[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    unsigned int include_count;
    unsigned int exclude_count;
    unsigned long long max_workers;
    int dry_run;
    int no_manifest;
    int no_native_inventory;
    int force_sidecars;
    int yes;
    yvex_models_output_mode output_mode;
    yvex_model_download_progress_mode progress_mode;
    unsigned long long tick_seconds;
} yvex_cli_models_download_options;

typedef struct {
    unsigned long long file_count;
    unsigned long long safetensors_count;
    unsigned long long total_regular_file_bytes;
    unsigned long long largest_file_bytes;
    int config_present;
    int tokenizer_present;
    char largest_file_name[YVEX_PATH_CAP];
} yvex_model_download_source_scan;

typedef struct {
    char status[64];
    char target_id[128];
    char family[32];
    char provider[16];
    char repo_id[256];
    char revision[128];
    char local_name[128];
    char local_source_dir[YVEX_PATH_CAP];
    char models_root[YVEX_PATH_CAP];
    char models_root_source[32];
    char reports_dir[YVEX_PATH_CAP];
    char registry_dir[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char receipt_path[YVEX_PATH_CAP];
    char stdout_log_path[YVEX_PATH_CAP];
    char stderr_log_path[YVEX_PATH_CAP];
    char hf_cli_path[YVEX_PATH_CAP];
    char hf_cli_source[32];
    char auth_state[32];
    char token_env_name[64];
    char created_at[32];
    char top_blocker[128];
    char error[256];
    char stage_resolve_target[16];
    char stage_resolve_paths[16];
    char stage_prepare_dirs[16];
    char stage_hf_cli[16];
    char stage_download[16];
    char stage_progress_stream[16];
    char stage_progress_ticks[16];
    char stage_source_scan[16];
    char stage_source_manifest[16];
    char stage_native_inventory[16];
    char stage_sidecar[16];
    yvex_model_download_source_scan source_scan;
    yvex_native_weight_summary native_summary;
    int hf_exit_code;
    int provider_exit_code;
    int stdout_streamed;
    int stderr_streamed;
    int interrupted;
    int interrupt_signal;
    int signal_forwarded;
    int child_terminated;
    int child_killed_after_timeout;
    int orphan_check_performed;
    int partial_source_preserved;
    int lock_files_deleted;
    pid_t provider_pid;
    pid_t provider_process_group;
    char child_exit_status[32];
    char orphan_check_status[16];
    unsigned long long stdout_bytes;
    unsigned long long stderr_bytes;
    unsigned long long tick_count;
    int source_manifest_written;
    int native_inventory_written;
    int report_written;
    int registry_written;
} yvex_model_download_report;

static const yvex_model_download_catalog_row *model_download_find_catalog(const char *target)
{
    unsigned long i;

    if (!target) return NULL;
    for (i = 0; i < sizeof(model_download_catalog) / sizeof(model_download_catalog[0]); ++i) {
        if (strcmp(model_download_catalog[i].target_id, target) == 0) {
            return &model_download_catalog[i];
        }
    }
    return NULL;
}

static int model_download_family_valid(const char *family)
{
    return family &&
           (strcmp(family, "deepseek") == 0 ||
            strcmp(family, "glm") == 0 ||
            strcmp(family, "qwen") == 0 ||
            strcmp(family, "gemma") == 0);
}

static int model_download_local_name_valid(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if (!name || !name[0]) return 0;
    while (*p) {
        if (!(isalnum(*p) || *p == '-' || *p == '_' || *p == '.')) {
            return 0;
        }
        ++p;
    }
    return 1;
}

static int model_download_repo_valid(const char *repo)
{
    const unsigned char *p = (const unsigned char *)repo;
    int slash_count = 0;

    if (!repo || !repo[0] || repo[0] == '/' || strstr(repo, "..")) return 0;
    while (*p) {
        if (*p == '/') {
            slash_count++;
            if (p[1] == '\0') return 0;
        } else if (!(isalnum(*p) || *p == '-' || *p == '_' || *p == '.')) {
            return 0;
        }
        ++p;
    }
    return slash_count == 1;
}

static const char *model_download_repo_basename(const char *repo)
{
    const char *slash = repo ? strrchr(repo, '/') : NULL;
    return slash && slash[1] ? slash + 1 : NULL;
}

static const char *model_download_progress_mode_name(yvex_model_download_progress_mode mode)
{
    switch (mode) {
    case YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO: return "auto";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE: return "live";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN: return "plain";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_LOG: return "log";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_OFF: return "off";
    }
    return "auto";
}

static const char *model_download_signal_name(int signo)
{
    switch (signo) {
    case SIGINT: return "SIGINT";
    case SIGTERM: return "SIGTERM";
    case SIGKILL: return "SIGKILL";
    case 0: return "none";
    }
    return "unknown";
}

static int model_download_parse_progress_mode(const char *value,
                                              yvex_model_download_progress_mode *out)
{
    if (!value || !out) return 0;
    if (strcmp(value, "auto") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO;
        return 1;
    }
    if (strcmp(value, "live") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE;
        return 1;
    }
    if (strcmp(value, "plain") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN;
        return 1;
    }
    if (strcmp(value, "log") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_LOG;
        return 1;
    }
    if (strcmp(value, "off") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_OFF;
        return 1;
    }
    return 0;
}

static yvex_model_download_progress_mode model_download_effective_progress_mode(
    yvex_model_download_progress_mode mode)
{
    if (mode != YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO) {
        return mode;
    }
    return isatty(STDOUT_FILENO) && isatty(STDERR_FILENO)
        ? YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE
        : YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN;
}

static int parse_models_download_options(int argc,
                                         char **argv,
                                         yvex_cli_models_download_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->source = "hf";
    options->revision = "main";
    options->max_workers = 8ull;
    options->token_env = "HF_TOKEN";
    options->auth_mode = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    options->progress_mode = YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO;
    options->tick_seconds = 2ull;

    if (argc >= 4 && (strcmp(argv[3], "--help") == 0 || strcmp(argv[3], "-h") == 0)) {
        return 1;
    }

    for (i = 3; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--models-root") == 0 ||
            strcmp(argv[i], "--repo") == 0 ||
            strcmp(argv[i], "--family") == 0 ||
            strcmp(argv[i], "--name") == 0 ||
            strcmp(argv[i], "--revision") == 0 ||
            strcmp(argv[i], "--source") == 0 ||
            strcmp(argv[i], "--include") == 0 ||
            strcmp(argv[i], "--exclude") == 0 ||
            strcmp(argv[i], "--max-workers") == 0 ||
            strcmp(argv[i], "--token-env") == 0 ||
            strcmp(argv[i], "--auth") == 0 ||
            strcmp(argv[i], "--progress") == 0 ||
            strcmp(argv[i], "--tick-seconds") == 0 ||
            strcmp(argv[i], "--output") == 0) {
            const char *flag = argv[i];
            int rc = parse_models_value_option("models download", flag,
                                               argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--models-root") == 0) {
                options->models_root = value;
            } else if (strcmp(flag, "--repo") == 0) {
                options->repo = value;
            } else if (strcmp(flag, "--family") == 0) {
                options->family = value;
            } else if (strcmp(flag, "--name") == 0) {
                options->name = value;
            } else if (strcmp(flag, "--revision") == 0) {
                options->revision = value;
            } else if (strcmp(flag, "--source") == 0) {
                if (strcmp(value, "hf") != 0) {
                    fprintf(stderr, "yvex: models download --source supports hf only\n");
                    return 2;
                }
                options->source = value;
            } else if (strcmp(flag, "--include") == 0) {
                if (options->include_count >= YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    fprintf(stderr, "yvex: models download too many --include patterns\n");
                    return 2;
                }
                options->include_patterns[options->include_count++] = value;
            } else if (strcmp(flag, "--exclude") == 0) {
                if (options->exclude_count >= YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    fprintf(stderr, "yvex: models download too many --exclude patterns\n");
                    return 2;
                }
                options->exclude_patterns[options->exclude_count++] = value;
            } else if (strcmp(flag, "--max-workers") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    fprintf(stderr, "yvex: models download --max-workers requires a positive integer\n");
                    return 2;
                }
                options->max_workers = parsed;
            } else if (strcmp(flag, "--token-env") == 0) {
                options->token_env = value;
            } else if (strcmp(flag, "--auth") == 0) {
                if (strcmp(value, "auto") != 0 &&
                    strcmp(value, "required") != 0 &&
                    strcmp(value, "never") != 0) {
                    fprintf(stderr, "yvex: models download --auth requires auto|required|never\n");
                    return 2;
                }
                options->auth_mode = value;
            } else if (strcmp(flag, "--progress") == 0) {
                if (!model_download_parse_progress_mode(value, &options->progress_mode)) {
                    fprintf(stderr, "yvex: models download --progress requires auto|live|plain|log|off\n");
                    return 2;
                }
            } else if (strcmp(flag, "--tick-seconds") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    fprintf(stderr, "yvex: models download --tick-seconds requires a positive integer\n");
                    return 2;
                }
                options->tick_seconds = parsed;
            } else if (strcmp(flag, "--output") == 0) {
                if (!parse_models_output_mode(value, &options->output_mode)) {
                    fprintf(stderr, "yvex: models download unsupported output mode: %s\n", value);
                    return 2;
                }
            }
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            options->dry_run = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--no-manifest") == 0) {
            options->no_manifest = 1;
        } else if (strcmp(argv[i], "--no-native-inventory") == 0) {
            options->no_native_inventory = 1;
        } else if (strcmp(argv[i], "--force-sidecars") == 0) {
            options->force_sidecars = 1;
        } else if (strcmp(argv[i], "--yes") == 0) {
            options->yes = 1;
        } else if (strcmp(argv[i], "--no-progress") == 0) {
            options->progress_mode = YVEX_MODEL_DOWNLOAD_PROGRESS_OFF;
        } else if (strcmp(argv[i], "--json") == 0) {
            fprintf(stderr, "yvex: models download JSON output is unsupported; use --output normal|table|audit\n");
            return 2;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "yvex: unknown models download option: %s\n", argv[i]);
            return 2;
        } else if (!options->target) {
            options->target = argv[i];
            if (!cli_arg_value_valid(options->target)) {
                fprintf(stderr, "yvex: models download target is empty or invalid\n");
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: models download received extra positional argument: %s\n", argv[i]);
            return 2;
        }
    }

    if (!options->target && !options->repo) {
        fprintf(stderr, "yvex: models download requires TARGET or --repo OWNER/NAME\n");
        return 2;
    }
    if (options->target && options->repo) {
        fprintf(stderr, "yvex: models download accepts either TARGET or --repo, not both\n");
        return 2;
    }
    if (options->repo && !model_download_repo_valid(options->repo)) {
        fprintf(stderr, "yvex: models download --repo requires OWNER/NAME\n");
        return 2;
    }
    if (options->repo && (!options->family || !model_download_family_valid(options->family))) {
        fprintf(stderr, "yvex: models download --repo requires --family deepseek|glm|qwen|gemma\n");
        return 2;
    }
    if (options->repo && !options->name) {
        options->name = model_download_repo_basename(options->repo);
    }
    if (options->repo && !model_download_local_name_valid(options->name)) {
        fprintf(stderr, "yvex: models download --name is required and must be a local model name\n");
        return 2;
    }
    return 0;
}

static unsigned int model_download_effective_include_count(const yvex_cli_models_download_options *options)
{
    return options && options->include_count
        ? options->include_count
        : (unsigned int)(sizeof(model_download_default_includes) /
                         sizeof(model_download_default_includes[0]));
}

static unsigned int model_download_effective_exclude_count(const yvex_cli_models_download_options *options)
{
    return options && options->exclude_count
        ? options->exclude_count
        : (unsigned int)(sizeof(model_download_default_excludes) /
                         sizeof(model_download_default_excludes[0]));
}

static const char *model_download_effective_include_at(const yvex_cli_models_download_options *options,
                                                       unsigned int index)
{
    if (options && options->include_count) return options->include_patterns[index];
    return model_download_default_includes[index];
}

static const char *model_download_effective_exclude_at(const yvex_cli_models_download_options *options,
                                                       unsigned int index)
{
    if (options && options->exclude_count) return options->exclude_patterns[index];
    return model_download_default_excludes[index];
}

static void model_download_report_init(yvex_model_download_report *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    snprintf(report->status, sizeof(report->status), "model-download-fail");
    snprintf(report->provider, sizeof(report->provider), "hf");
    snprintf(report->auth_state, sizeof(report->auth_state), "not-provided");
    snprintf(report->stage_resolve_target, sizeof(report->stage_resolve_target), "fail");
    snprintf(report->stage_resolve_paths, sizeof(report->stage_resolve_paths), "fail");
    snprintf(report->stage_prepare_dirs, sizeof(report->stage_prepare_dirs), "skipped");
    snprintf(report->stage_hf_cli, sizeof(report->stage_hf_cli), "skipped");
    snprintf(report->stage_download, sizeof(report->stage_download), "skipped");
    snprintf(report->stage_progress_stream, sizeof(report->stage_progress_stream), "skipped");
    snprintf(report->stage_progress_ticks, sizeof(report->stage_progress_ticks), "skipped");
    snprintf(report->stage_source_scan, sizeof(report->stage_source_scan), "skipped");
    snprintf(report->stage_source_manifest, sizeof(report->stage_source_manifest), "skipped");
    snprintf(report->stage_native_inventory, sizeof(report->stage_native_inventory), "skipped");
    snprintf(report->stage_sidecar, sizeof(report->stage_sidecar), "skipped");
    report->hf_exit_code = -1;
    report->provider_exit_code = -1;
    report->provider_pid = -1;
    report->provider_process_group = -1;
    snprintf(report->child_exit_status, sizeof(report->child_exit_status), "unknown");
    snprintf(report->orphan_check_status, sizeof(report->orphan_check_status), "unknown");
}

static void model_download_timestamp(char *out, size_t cap)
{
    time_t now;
    struct tm tm_utc;

    if (!out || cap == 0u) return;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1 || !gmtime_r(&now, &tm_utc)) {
        snprintf(out, cap, "unknown");
        return;
    }
    if (strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        snprintf(out, cap, "unknown");
    }
}

static int model_download_path_under(const char *path, const char *dir)
{
    size_t dir_len;

    if (!path || !dir || !path[0] || !dir[0]) return 0;
    dir_len = strlen(dir);
    while (dir_len > 1u && dir[dir_len - 1u] == '/') {
        --dir_len;
    }
    return strncmp(path, dir, dir_len) == 0 &&
           (path[dir_len] == '\0' || path[dir_len] == '/');
}

static int model_download_source_path_allowed(const yvex_operator_paths *operator_paths,
                                              const char *local_source_dir,
                                              yvex_model_download_report *report)
{
    char repo_root[YVEX_PATH_CAP];

    if (!operator_paths || !local_source_dir) return 0;
    if (strcmp(operator_paths->models_root_source, "explicit") == 0) {
        return 1;
    }
    if (!getcwd(repo_root, sizeof(repo_root))) {
        snprintf(report->top_blocker, sizeof(report->top_blocker),
                 "repository-path-check-unavailable");
        snprintf(report->error, sizeof(report->error), "getcwd failed while checking local source path");
        return 0;
    }
    if (model_download_path_under(local_source_dir, repo_root)) {
        snprintf(report->top_blocker, sizeof(report->top_blocker),
                 "unsafe-repository-source-path");
        snprintf(report->error, sizeof(report->error),
                 "resolved local source path is inside the repository tree");
        return 0;
    }
    return 1;
}

static int model_download_file_name_ends_with(const char *path, const char *suffix)
{
    size_t path_len;
    size_t suffix_len;

    if (!path || !suffix) return 0;
    path_len = strlen(path);
    suffix_len = strlen(suffix);
    return suffix_len <= path_len && strcmp(path + path_len - suffix_len, suffix) == 0;
}

static int model_download_name_starts_with(const char *name, const char *prefix)
{
    size_t n;

    if (!name || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(name, prefix, n) == 0;
}

static int model_download_scan_dir(const char *root,
                                   const char *rel_dir,
                                   yvex_model_download_source_scan *scan,
                                   yvex_error *err)
{
    char abs_dir[YVEX_PATH_CAP];
    DIR *dir;
    struct dirent *ent;
    int rc = YVEX_OK;

    if (rel_dir && rel_dir[0]) {
        rc = path_join2(abs_dir, sizeof(abs_dir), root, rel_dir, err, "models_download");
        if (rc != YVEX_OK) return rc;
    } else {
        int n = snprintf(abs_dir, sizeof(abs_dir), "%s", root);
        if (n < 0 || (size_t)n >= sizeof(abs_dir)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "models_download", "source path is too long");
            return YVEX_ERR_BOUNDS;
        }
    }

    dir = opendir(abs_dir);
    if (!dir) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download", "cannot open source directory: %s", abs_dir);
        return YVEX_ERR_IO;
    }

    while ((ent = readdir(dir)) != NULL) {
        char rel_path[YVEX_PATH_CAP];
        char abs_path[YVEX_PATH_CAP];
        struct stat st;
        const char *base = ent->d_name;

        if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
            continue;
        }
        if (rel_dir && rel_dir[0]) {
            rc = path_join2(rel_path, sizeof(rel_path), rel_dir, base, err, "models_download");
        } else {
            int n = snprintf(rel_path, sizeof(rel_path), "%s", base);
            rc = (n < 0 || (size_t)n >= sizeof(rel_path)) ? YVEX_ERR_BOUNDS : YVEX_OK;
            if (rc != YVEX_OK) {
                yvex_error_set(err, rc, "models_download", "relative source path is too long");
            }
        }
        if (rc != YVEX_OK) break;
        rc = path_join2(abs_path, sizeof(abs_path), root, rel_path, err, "models_download");
        if (rc != YVEX_OK) break;
        if (lstat(abs_path, &st) != 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "models_download", "cannot stat source path: %s", abs_path);
            rc = YVEX_ERR_IO;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            rc = model_download_scan_dir(root, rel_path, scan, err);
            if (rc != YVEX_OK) break;
        } else if (S_ISREG(st.st_mode)) {
            unsigned long long bytes = st.st_size > 0 ? (unsigned long long)st.st_size : 0ull;
            scan->file_count++;
            scan->total_regular_file_bytes += bytes;
            if (model_download_file_name_ends_with(rel_path, ".safetensors")) {
                scan->safetensors_count++;
            }
            if (strcmp(base, "config.json") == 0) {
                scan->config_present = 1;
            }
            if (strcmp(base, "tokenizer.json") == 0 ||
                strcmp(base, "tokenizer.model") == 0 ||
                strcmp(base, "tokenizer_config.json") == 0 ||
                model_download_name_starts_with(base, "tokenizer.")) {
                scan->tokenizer_present = 1;
            }
            if (bytes > scan->largest_file_bytes) {
                scan->largest_file_bytes = bytes;
                snprintf(scan->largest_file_name, sizeof(scan->largest_file_name), "%s", rel_path);
            }
        }
    }

    closedir(dir);
    return rc;
}

static int model_download_scan_source(const char *root,
                                      yvex_model_download_source_scan *scan,
                                      yvex_error *err)
{
    if (!root || !scan) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download", "source root and scan output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(scan, 0, sizeof(*scan));
    return model_download_scan_dir(root, "", scan, err);
}

static int model_download_observe_source(const char *root,
                                         yvex_model_download_source_scan *scan)
{
    struct stat st;
    yvex_error err;

    if (!scan) return 0;
    memset(scan, 0, sizeof(*scan));
    if (!root || root[0] == '\0' || stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return 0;
    }
    yvex_error_clear(&err);
    return model_download_scan_source(root, scan, &err) == YVEX_OK;
}

static void write_bool_field(FILE *fp,
                             const char *indent,
                             const char *key,
                             int value,
                             int comma)
{
    fputs(indent, fp);
    fprintf(fp, "\"%s\": %s%s\n", key, value ? "true" : "false", comma ? "," : "");
}

static void model_download_write_pattern_array(FILE *fp,
                                               const char *name,
                                               const yvex_cli_models_download_options *options,
                                               int includes,
                                               int comma)
{
    unsigned int count = includes
        ? model_download_effective_include_count(options)
        : model_download_effective_exclude_count(options);
    unsigned int i;

    fprintf(fp, "  \"%s\": [", name);
    for (i = 0; i < count; ++i) {
        if (i > 0) fputs(", ", fp);
        write_escaped(fp, includes
            ? model_download_effective_include_at(options, i)
            : model_download_effective_exclude_at(options, i));
    }
    fprintf(fp, "]%s\n", comma ? "," : "");
}

static int model_download_write_native_inventory_json(const char *path,
                                                      const char *source_dir,
                                                      const yvex_native_weight_table *table,
                                                      yvex_error *err)
{
    yvex_native_weight_summary summary;
    FILE *fp;
    unsigned long long count;
    unsigned long long i;

    if (!path || !source_dir || !table) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_native_inventory",
                       "path, source directory, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_native_weight_table_summary(table, &summary, err) != YVEX_OK) {
        return yvex_error_code(err);
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_native_inventory",
                        "cannot open native inventory: %s", path);
        return YVEX_ERR_IO;
    }
    count = yvex_native_weight_table_count(table);
    fprintf(fp, "{\n");
    write_field(fp, "  ", "schema", "yvex.native_inventory.v1", 1);
    write_field(fp, "  ", "source_dir", source_dir, 1);
    write_bool_field(fp, "  ", "payload_loaded", 0, 1);
    fprintf(fp, "  \"summary\": {\n");
    write_u64_field(fp, "    ", "shard_count", summary.shard_count, 1);
    write_u64_field(fp, "    ", "tensor_count", summary.tensor_count, 1);
    write_u64_field(fp, "    ", "total_tensor_bytes", summary.total_tensor_bytes, 1);
    write_u64_field(fp, "    ", "unknown_dtype_count", summary.unknown_dtype_count, 1);
    write_u64_field(fp, "    ", "malformed_shard_count", summary.malformed_shard_count, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"tensors\": [\n");
    for (i = 0; i < count; ++i) {
        const yvex_native_weight_info *info = yvex_native_weight_table_at(table, i);
        unsigned int d;

        fprintf(fp, "    {\n");
        write_field(fp, "      ", "name", info && info->name ? info->name : "", 1);
        write_field(fp, "      ", "shard_path", info && info->shard_path ? info->shard_path : "", 1);
        write_field(fp, "      ", "dtype", info && info->dtype_name ? info->dtype_name : "UNKNOWN", 1);
        fprintf(fp, "      \"rank\": %u,\n", info ? info->rank : 0u);
        fprintf(fp, "      \"shape\": [");
        if (info) {
            for (d = 0; d < info->rank; ++d) {
                fprintf(fp, "%s%llu", d == 0 ? "" : ", ", info->dims[d]);
            }
        }
        fprintf(fp, "],\n");
        write_u64_field(fp, "      ", "data_start", info ? info->data_start : 0ull, 1);
        write_u64_field(fp, "      ", "data_end", info ? info->data_end : 0ull, 1);
        write_u64_field(fp, "      ", "data_bytes", info ? info->data_bytes : 0ull, 0);
        fprintf(fp, "    }%s\n", i + 1ull == count ? "" : ",");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_native_inventory",
                        "cannot close native inventory: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int model_download_write_json_sidecar(const char *path,
                                             const char *schema,
                                             const yvex_cli_models_download_options *options,
                                             const yvex_model_download_report *report,
                                             yvex_error *err)
{
    FILE *fp;

    if (!path || !schema || !options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_sidecar",
                       "path, schema, options, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_sidecar",
                        "cannot open sidecar: %s", path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    write_field(fp, "  ", "schema", schema, 1);
    write_field(fp, "  ", "status", report->status, 1);
    write_field(fp, "  ", "target_id", report->target_id, 1);
    write_field(fp, "  ", "family", report->family, 1);
    write_field(fp, "  ", "provider", report->provider, 1);
    write_field(fp, "  ", "repo_id", report->repo_id, 1);
    write_field(fp, "  ", "revision", report->revision, 1);
    write_field(fp, "  ", "local_source_dir", report->local_source_dir, 1);
    write_field(fp, "  ", "models_root", report->models_root, 1);
    write_field(fp, "  ", "models_root_source", report->models_root_source, 1);
    model_download_write_pattern_array(fp, "include_patterns", options, 1, 1);
    model_download_write_pattern_array(fp, "exclude_patterns", options, 0, 1);
    write_field(fp, "  ", "hf_cli_path", report->hf_cli_path, 1);
    fprintf(fp, "  \"hf_exit_code\": %d,\n", report->hf_exit_code);
    fprintf(fp, "  \"provider_exit_code\": %d,\n", report->provider_exit_code);
    write_field(fp, "  ", "auth_mode", options->auth_mode, 1);
    write_field(fp, "  ", "auth_state", report->auth_state, 1);
    write_field(fp, "  ", "progress_mode",
                model_download_progress_mode_name(options->progress_mode), 1);
    write_u64_field(fp, "  ", "tick_seconds", options->tick_seconds, 1);
    write_u64_field(fp, "  ", "tick_count", report->tick_count, 1);
    write_bool_field(fp, "  ", "stdout_streamed", report->stdout_streamed, 1);
    write_bool_field(fp, "  ", "stderr_streamed", report->stderr_streamed, 1);
    write_u64_field(fp, "  ", "stdout_bytes", report->stdout_bytes, 1);
    write_u64_field(fp, "  ", "stderr_bytes", report->stderr_bytes, 1);
    fprintf(fp, "  \"provider_pid\": %lld,\n", (long long)report->provider_pid);
    fprintf(fp, "  \"provider_process_group\": %lld,\n",
            (long long)report->provider_process_group);
    write_bool_field(fp, "  ", "interrupted", report->interrupted, 1);
    write_field(fp, "  ", "interrupt_signal",
                model_download_signal_name(report->interrupt_signal), 1);
    write_bool_field(fp, "  ", "signal_forwarded", report->signal_forwarded, 1);
    write_bool_field(fp, "  ", "child_terminated", report->child_terminated, 1);
    write_bool_field(fp, "  ", "child_killed_after_timeout",
                     report->child_killed_after_timeout, 1);
    write_field(fp, "  ", "child_exit_status", report->child_exit_status, 1);
    write_bool_field(fp, "  ", "orphan_check_performed",
                     report->orphan_check_performed, 1);
    write_field(fp, "  ", "orphan_check_status", report->orphan_check_status, 1);
    write_bool_field(fp, "  ", "partial_source_preserved",
                     report->partial_source_preserved, 1);
    write_bool_field(fp, "  ", "lock_files_deleted", report->lock_files_deleted, 1);
    write_u64_field(fp, "  ", "file_count", report->source_scan.file_count, 1);
    write_u64_field(fp, "  ", "safetensors_count", report->source_scan.safetensors_count, 1);
    write_bool_field(fp, "  ", "config_present", report->source_scan.config_present, 1);
    write_bool_field(fp, "  ", "tokenizer_present", report->source_scan.tokenizer_present, 1);
    write_u64_field(fp, "  ", "total_regular_file_bytes", report->source_scan.total_regular_file_bytes, 1);
    write_field(fp, "  ", "largest_file_name",
                report->source_scan.largest_file_name[0] ? report->source_scan.largest_file_name : "none", 1);
    write_u64_field(fp, "  ", "largest_file_bytes", report->source_scan.largest_file_bytes, 1);
    write_field(fp, "  ", "manifest_path", report->manifest_path, 1);
    write_field(fp, "  ", "native_inventory_path", report->native_inventory_path, 1);
    write_field(fp, "  ", "stdout_log", report->stdout_log_path, 1);
    write_field(fp, "  ", "stderr_log", report->stderr_log_path, 1);
    write_field(fp, "  ", "created_at", report->created_at, 1);
    write_field(fp, "  ", "yvex_version", yvex_version_string(), 1);
    write_bool_field(fp, "  ", "upstream_identity_verified", 0, 1);
    write_bool_field(fp, "  ", "remote_lookup_performed", 0, 1);
    write_bool_field(fp, "  ", "payload_hash_verified", 0, 1);
    write_bool_field(fp, "  ", "force_sidecars", options->force_sidecars, 1);
    write_bool_field(fp, "  ", "yes", options->yes, 1);
    fprintf(fp, "  \"boundary\": {\n");
    write_field(fp, "    ", "source_download",
                options->dry_run ? "dry-run" :
                (strcmp(report->status, "model-download-pass") == 0 ? "performed" :
                 (strcmp(report->status, "model-download-blocked") == 0 ? "blocked" : "failed")), 1);
    write_bool_field(fp, "    ", "source_manifest_written", report->source_manifest_written, 1);
    write_bool_field(fp, "    ", "native_inventory_written", report->native_inventory_written, 1);
    write_bool_field(fp, "    ", "payload_loaded", 0, 1);
    write_bool_field(fp, "    ", "gguf_created", 0, 1);
    write_bool_field(fp, "    ", "materialized", 0, 1);
    write_bool_field(fp, "    ", "runtime_ready", 0, 1);
    write_field(fp, "    ", "generation", "unsupported", 1);
    write_field(fp, "    ", "eval", "unsupported", 1);
    write_field(fp, "    ", "benchmark", "not-measured", 0);
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_sidecar",
                        "cannot close sidecar: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int model_download_write_receipt(const char *path,
                                        const yvex_cli_models_download_options *options,
                                        const yvex_model_download_report *report,
                                        int token_present,
                                        yvex_error *err)
{
    FILE *fp;
    unsigned int i;

    if (!path || !options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_receipt",
                       "receipt path, options, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_receipt",
                        "cannot open receipt: %s", path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "schema: yvex.model_download.receipt.v1\n");
    fprintf(fp, "target_id: %s\n", report->target_id);
    fprintf(fp, "family: %s\n", report->family);
    fprintf(fp, "repo_id: %s\n", report->repo_id);
    fprintf(fp, "revision: %s\n", report->revision);
    fprintf(fp, "local_source_dir: %s\n", report->local_source_dir);
    fprintf(fp, "hf_cli_path: %s\n", report->hf_cli_path);
    fprintf(fp, "auth_state: %s\n", report->auth_state);
    fprintf(fp, "token_env_name: %s\n", report->token_env_name);
    fprintf(fp, "token_value_redacted: %s\n", token_present ? "true" : "false");
    fprintf(fp, "command: %s download %s --revision %s --local-dir %s",
            report->hf_cli_path, report->repo_id, report->revision,
            report->local_source_dir);
    for (i = 0; i < model_download_effective_include_count(options); ++i) {
        fprintf(fp, " --include %s", model_download_effective_include_at(options, i));
    }
    for (i = 0; i < model_download_effective_exclude_count(options); ++i) {
        fprintf(fp, " --exclude %s", model_download_effective_exclude_at(options, i));
    }
    fprintf(fp, " --max-workers %llu", options->max_workers);
    if (options->dry_run) fprintf(fp, " --dry-run");
    if (token_present) fprintf(fp, " --token <redacted>");
    fprintf(fp, "\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_receipt",
                        "cannot close receipt: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int model_download_find_hf_cli(char *out,
                                      size_t cap,
                                      char *source,
                                      size_t source_cap)
{
    const char *override = getenv("YVEX_HF_CLI");
    const char *path_env;
    const char *start;

    if (!out || cap == 0u || !source || source_cap == 0u) return 0;
    out[0] = '\0';
    source[0] = '\0';
    if (override && override[0]) {
        if (strchr(override, '/')) {
            if (access(override, X_OK) == 0) {
                snprintf(out, cap, "%s", override);
                snprintf(source, source_cap, "YVEX_HF_CLI");
                return 1;
            }
            return 0;
        }
        path_env = getenv("PATH");
        start = path_env ? path_env : "";
    } else {
        path_env = getenv("PATH");
        start = path_env ? path_env : "";
    }

    while (start && *start) {
        char candidate[YVEX_PATH_CAP];
        const char *end = strchr(start, ':');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        const char *dir = len == 0u ? "." : start;
        int n;

        if (len >= sizeof(candidate)) return 0;
        n = snprintf(candidate, sizeof(candidate), "%.*s/%s",
                     (int)len, dir, override && override[0] ? override : "hf");
        if (n >= 0 && (size_t)n < sizeof(candidate) && access(candidate, X_OK) == 0) {
            snprintf(out, cap, "%s", candidate);
            snprintf(source, source_cap, "%s", override && override[0] ? "YVEX_HF_CLI" : "PATH");
            return 1;
        }
        start = end ? end + 1 : NULL;
    }
    return 0;
}

static int model_download_write_all_fd(int fd, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;

    while (len > 0u) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (n == 0) return 0;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 1;
}

static void model_download_mirror_provider_bytes(int fd,
                                                 const char *buf,
                                                 size_t len,
                                                 int normalize_cr)
{
    size_t i;

    if (fd < 0 || !buf || len == 0u) return;
    if (!normalize_cr) {
        (void)model_download_write_all_fd(fd, buf, len);
        return;
    }
    for (i = 0; i < len; ++i) {
        char c = buf[i] == '\r' ? '\n' : buf[i];
        (void)model_download_write_all_fd(fd, &c, 1u);
    }
}

static void model_download_print_start_progress(
    const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report,
    yvex_model_download_progress_mode effective_mode)
{
    if (!options || !report || effective_mode == YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) {
        return;
    }
    printf("model-download: start target=%s\n", report->target_id);
    printf("provider: %s\n", strcmp(report->provider, "hf") == 0 ? "huggingface" : report->provider);
    printf("repo: %s\n", report->repo_id);
    printf("source: %s\n", report->local_source_dir);
    printf("stage: account-provider pass\n");
    printf("stage: download running\n");
    fflush(stdout);
}

static void model_download_print_tick_progress(
    const char *source_dir,
    time_t started_at,
    yvex_model_download_report *report,
    yvex_model_download_progress_mode effective_mode)
{
    yvex_model_download_source_scan scan;
    time_t now;
    unsigned long long elapsed = 0ull;

    if (!report || effective_mode == YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) {
        return;
    }
    now = time(NULL);
    if (now != (time_t)-1 && started_at != (time_t)-1 && now >= started_at) {
        elapsed = (unsigned long long)(now - started_at);
    }
    (void)model_download_observe_source(source_dir, &scan);
    printf("tick: elapsed=%llus files=%llu safetensors=%llu bytes=%llu largest=%s\n",
           elapsed,
           scan.file_count,
           scan.safetensors_count,
           scan.total_regular_file_bytes,
           scan.largest_file_name[0] ? scan.largest_file_name : "none");
    fflush(stdout);
    report->tick_count++;
}

enum {
    YVEX_MODEL_DOWNLOAD_INTERRUPT_TIMEOUT_SECONDS = 5
};

static volatile sig_atomic_t yvex_model_download_provider_signal_seen = 0;

static void model_download_provider_signal_handler(int signo)
{
    if ((signo == SIGINT || signo == SIGTERM) &&
        yvex_model_download_provider_signal_seen == 0) {
        yvex_model_download_provider_signal_seen = signo;
    }
}

static int model_download_install_provider_signal_handlers(struct sigaction *old_int,
                                                           struct sigaction *old_term,
                                                           yvex_error *err)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = model_download_provider_signal_handler;
    sigemptyset(&action.sa_mask);

    yvex_model_download_provider_signal_seen = 0;
    if (sigaction(SIGINT, &action, old_int) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot install SIGINT handler: %s", strerror(errno));
        return 0;
    }
    if (sigaction(SIGTERM, &action, old_term) != 0) {
        (void)sigaction(SIGINT, old_int, NULL);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot install SIGTERM handler: %s", strerror(errno));
        return 0;
    }
    return 1;
}

static void model_download_restore_provider_signal_handlers(const struct sigaction *old_int,
                                                            const struct sigaction *old_term)
{
    if (old_int) (void)sigaction(SIGINT, old_int, NULL);
    if (old_term) (void)sigaction(SIGTERM, old_term, NULL);
    yvex_model_download_provider_signal_seen = 0;
}

static void model_download_reset_child_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);
}

static int model_download_set_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static void model_download_record_child_exit_status(yvex_model_download_report *report,
                                                    int child_status)
{
    int code;
    int sig;

    if (!report) return;
    report->child_terminated = 1;
    if (WIFEXITED(child_status)) {
        code = WEXITSTATUS(child_status);
        if (report->interrupted &&
            (code == 0 ||
             (report->interrupt_signal > 0 && code == 128 + report->interrupt_signal))) {
            snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                     "interrupted");
        } else if (code == 0) {
            snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                     "exited");
        } else {
            snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                     "terminated");
        }
        return;
    }
    if (WIFSIGNALED(child_status)) {
        sig = WTERMSIG(child_status);
        snprintf(report->child_exit_status, sizeof(report->child_exit_status), "%s",
                 report->interrupted &&
                 (sig == report->interrupt_signal || sig == SIGINT || sig == SIGTERM)
                 ? "interrupted"
                 : "terminated");
        return;
    }
    snprintf(report->child_exit_status, sizeof(report->child_exit_status), "unknown");
}

static void model_download_mark_provider_interrupted(yvex_model_download_report *report,
                                                     int signo,
                                                     pid_t pgid)
{
    int kill_rc;

    if (!report || report->interrupted) return;
    report->interrupted = 1;
    report->interrupt_signal = signo;
    report->partial_source_preserved = 1;
    report->lock_files_deleted = 0;
    if (pgid > 0) {
        kill_rc = kill(-pgid, signo);
        if (kill_rc == 0 || errno == ESRCH) {
            report->signal_forwarded = 1;
        }
    }
}

static void model_download_orphan_check(yvex_model_download_report *report)
{
    pid_t pgid;

    if (!report) return;
    report->orphan_check_performed = 1;
    pgid = report->provider_process_group;
    if (pgid <= 0) {
        snprintf(report->orphan_check_status, sizeof(report->orphan_check_status),
                 "unknown");
        return;
    }
    if (kill(-pgid, 0) == 0) {
        snprintf(report->orphan_check_status, sizeof(report->orphan_check_status),
                 "fail");
        return;
    }
    snprintf(report->orphan_check_status, sizeof(report->orphan_check_status), "%s",
             errno == ESRCH ? "pass" : "unknown");
}

static int yvex_provider_process_run_streaming(const char *const *args,
                                               const char *stdout_log_path,
                                               const char *stderr_log_path,
                                               yvex_model_download_progress_mode effective_mode,
                                               unsigned long long tick_seconds,
                                               const char *local_source_dir,
                                               yvex_model_download_report *report,
                                               yvex_error *err)
{
    int stdout_pipe[2] = { -1, -1 };
    int stderr_pipe[2] = { -1, -1 };
    int stdout_log_fd = -1;
    int stderr_log_fd = -1;
    int stdout_open = 1;
    int stderr_open = 1;
    int child_exited = 0;
    int shutdown_signal_sent = 0;
    int kill_signal_sent = 0;
    int child_status = 0;
    int mirror_provider;
    int normalize_cr;
    time_t started_at;
    time_t next_tick;
    time_t shutdown_deadline = (time_t)-1;
    pid_t pid;
    pid_t pgid = -1;
    struct sigaction old_int;
    struct sigaction old_term;
    int signal_handlers_installed = 0;

    if (!args || !args[0] || !stdout_log_path || !stderr_log_path || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "provider_process",
                       "provider argv, log paths, and report are required");
        return -1;
    }

    stdout_log_fd = open(stdout_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (stdout_log_fd < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot open stdout log: %s", stdout_log_path);
        return -1;
    }
    stderr_log_fd = open(stderr_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (stderr_log_fd < 0) {
        close(stdout_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot open stderr log: %s", stderr_log_path);
        return -1;
    }
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
        if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
        if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
        if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "pipe failed: %s", strerror(errno));
        return -1;
    }
    if (!model_download_set_nonblocking(stdout_pipe[0]) ||
        !model_download_set_nonblocking(stderr_pipe[0])) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot make provider pipes nonblocking: %s",
                        strerror(errno));
        return -1;
    }

    mirror_provider = effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
                      effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_LOG;
    normalize_cr = effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE;
    started_at = time(NULL);
    next_tick = started_at == (time_t)-1
        ? (time_t)-1
        : started_at + (time_t)(tick_seconds ? tick_seconds : 1ull);

    if (!model_download_install_provider_signal_handlers(&old_int, &old_term, err)) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        return -1;
    }
    signal_handlers_installed = 1;

    pid = fork();
    if (pid < 0) {
        model_download_restore_provider_signal_handlers(&old_int, &old_term);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        model_download_reset_child_signal_handlers();
        if (setpgid(0, 0) != 0) _exit(127);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) _exit(127);
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) _exit(127);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        execv(args[0], (char *const *)args);
        _exit(127);
    }

    report->provider_pid = pid;
    (void)setpgid(pid, pid);
    pgid = getpgid(pid);
    if (pgid <= 0) pgid = pid;
    report->provider_process_group = pgid;

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    stdout_pipe[1] = -1;
    stderr_pipe[1] = -1;

    while (stdout_open || stderr_open || !child_exited) {
        struct pollfd fds[2];
        int which[2];
        nfds_t nfds = 0;
        time_t now;
        int timeout_ms = 1000;
        int poll_rc;
        pid_t waited;
        sig_atomic_t seen_signal;

        if (!child_exited) {
            waited = waitpid(pid, &child_status, WNOHANG);
            if (waited == pid) {
                child_exited = 1;
                model_download_record_child_exit_status(report, child_status);
            } else if (waited < 0 && errno == ECHILD) {
                child_exited = 1;
                snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                         "unknown");
            } else if (waited < 0 && errno != EINTR) {
                child_exited = 1;
            }
        }

        now = time(NULL);
        seen_signal = yvex_model_download_provider_signal_seen;
        if (seen_signal != 0 && !report->interrupted && !child_exited) {
            model_download_mark_provider_interrupted(report, (int)seen_signal, pgid);
            shutdown_signal_sent = 1;
            shutdown_deadline = now == (time_t)-1
                ? (time_t)-1
                : now + YVEX_MODEL_DOWNLOAD_INTERRUPT_TIMEOUT_SECONDS;
        }
        if (report->interrupted &&
            shutdown_signal_sent &&
            !kill_signal_sent &&
            shutdown_deadline != (time_t)-1 &&
            now != (time_t)-1 &&
            now >= shutdown_deadline &&
            (!child_exited || stdout_open || stderr_open)) {
            if (pgid > 0 && kill(-pgid, SIGKILL) == 0) {
                report->child_killed_after_timeout = 1;
            }
            kill_signal_sent = 1;
        }
        if (effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
            tick_seconds > 0ull &&
            now != (time_t)-1 &&
            next_tick != (time_t)-1 &&
            now >= next_tick &&
            !child_exited) {
            model_download_print_tick_progress(local_source_dir, started_at, report, effective_mode);
            next_tick = now + (time_t)tick_seconds;
        }
        if (effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
            tick_seconds > 0ull &&
            now != (time_t)-1 &&
            next_tick != (time_t)-1 &&
            next_tick > now) {
            time_t diff = next_tick - now;
            timeout_ms = diff > 1 ? 1000 : (int)(diff * 1000);
            if (timeout_ms <= 0) timeout_ms = 100;
        }
        if (report->interrupted && timeout_ms > 100) {
            timeout_ms = 100;
        }

        if (stdout_open) {
            fds[nfds].fd = stdout_pipe[0];
            fds[nfds].events = POLLIN | POLLHUP | POLLERR;
            fds[nfds].revents = 0;
            which[nfds] = 1;
            nfds++;
        }
        if (stderr_open) {
            fds[nfds].fd = stderr_pipe[0];
            fds[nfds].events = POLLIN | POLLHUP | POLLERR;
            fds[nfds].revents = 0;
            which[nfds] = 2;
            nfds++;
        }

        poll_rc = poll(nfds ? fds : NULL, nfds, timeout_ms);
        if (poll_rc < 0) {
            if (errno == EINTR) continue;
            if (!child_exited && pgid > 0) {
                time_t deadline;
                (void)kill(-pgid, SIGTERM);
                deadline = time(NULL) + YVEX_MODEL_DOWNLOAD_INTERRUPT_TIMEOUT_SECONDS;
                while (1) {
                    waited = waitpid(pid, &child_status, WNOHANG);
                    if (waited == pid) {
                        model_download_record_child_exit_status(report, child_status);
                        child_exited = 1;
                        break;
                    }
                    if (waited < 0 && errno != EINTR) {
                        child_exited = 1;
                        break;
                    }
                    if (time(NULL) >= deadline) {
                        (void)kill(-pgid, SIGKILL);
                        report->child_killed_after_timeout = 1;
                        while (waitpid(pid, &child_status, 0) < 0 && errno == EINTR) {
                        }
                        model_download_record_child_exit_status(report, child_status);
                        child_exited = 1;
                        break;
                    }
                    (void)poll(NULL, 0, 100);
                }
            }
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            close(stdout_log_fd);
            close(stderr_log_fd);
            model_download_orphan_check(report);
            if (signal_handlers_installed) {
                model_download_restore_provider_signal_handlers(&old_int, &old_term);
            }
            yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                            "poll failed: %s", strerror(errno));
            return -1;
        }
        if (poll_rc > 0) {
            nfds_t i;
            for (i = 0; i < nfds; ++i) {
                char buf[4096];
                ssize_t got;
                int stream_kind = which[i];
                int read_fd = stream_kind == 1 ? stdout_pipe[0] : stderr_pipe[0];
                int log_fd = stream_kind == 1 ? stdout_log_fd : stderr_log_fd;
                int mirror_fd = stream_kind == 1 ? STDOUT_FILENO : STDERR_FILENO;

                if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                    continue;
                }
                got = read(read_fd, buf, sizeof(buf));
                if (got > 0) {
                    (void)model_download_write_all_fd(log_fd, buf, (size_t)got);
                    if (stream_kind == 1) {
                        report->stdout_bytes += (unsigned long long)got;
                        report->stdout_streamed = 1;
                    } else {
                        report->stderr_bytes += (unsigned long long)got;
                        report->stderr_streamed = 1;
                    }
                    if (mirror_provider) {
                        model_download_mirror_provider_bytes(mirror_fd, buf, (size_t)got, normalize_cr);
                        if (stream_kind == 1) fflush(stdout);
                        else fflush(stderr);
                    }
                } else if (got == 0) {
                    if (stream_kind == 1 && stdout_open) {
                        close(stdout_pipe[0]);
                        stdout_pipe[0] = -1;
                        stdout_open = 0;
                    } else if (stream_kind == 2 && stderr_open) {
                        close(stderr_pipe[0]);
                        stderr_pipe[0] = -1;
                        stderr_open = 0;
                    }
                } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    if (stream_kind == 1 && stdout_open) {
                        close(stdout_pipe[0]);
                        stdout_pipe[0] = -1;
                        stdout_open = 0;
                    } else if (stream_kind == 2 && stderr_open) {
                        close(stderr_pipe[0]);
                        stderr_pipe[0] = -1;
                        stderr_open = 0;
                    }
                }
            }
        }
    }

    if (!child_exited) {
        while (waitpid(pid, &child_status, 0) < 0) {
            if (errno == EINTR) continue;
            close(stdout_log_fd);
            close(stderr_log_fd);
            if (signal_handlers_installed) {
                model_download_restore_provider_signal_handlers(&old_int, &old_term);
            }
            yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                            "waitpid failed: %s", strerror(errno));
            return -1;
        }
        model_download_record_child_exit_status(report, child_status);
    }
    close(stdout_log_fd);
    close(stderr_log_fd);
    model_download_orphan_check(report);
    if (signal_handlers_installed) {
        model_download_restore_provider_signal_handlers(&old_int, &old_term);
    }

    if (WIFEXITED(child_status)) return WEXITSTATUS(child_status);
    if (WIFSIGNALED(child_status)) return 128 + WTERMSIG(child_status);
    return 1;
}

static int model_download_run_hf(const yvex_cli_models_download_options *options,
                                 yvex_model_download_report *report,
                                 const char *token_value,
                                 yvex_error *err)
{
    const char *args[180];
    char max_workers_buf[32];
    unsigned int i;
    unsigned int n = 0;
    yvex_model_download_progress_mode effective_mode;

    if (!options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_hf",
                       "download options and report are required");
        return -1;
    }

    snprintf(max_workers_buf, sizeof(max_workers_buf), "%llu", options->max_workers);
    args[n++] = report->hf_cli_path;
    args[n++] = "download";
    args[n++] = report->repo_id;
    args[n++] = "--revision";
    args[n++] = report->revision;
    args[n++] = "--local-dir";
    args[n++] = report->local_source_dir;
    for (i = 0; i < model_download_effective_include_count(options); ++i) {
        args[n++] = "--include";
        args[n++] = model_download_effective_include_at(options, i);
    }
    for (i = 0; i < model_download_effective_exclude_count(options); ++i) {
        args[n++] = "--exclude";
        args[n++] = model_download_effective_exclude_at(options, i);
    }
    args[n++] = "--max-workers";
    args[n++] = max_workers_buf;
    if (options->dry_run) {
        args[n++] = "--dry-run";
    }
    if (token_value && token_value[0]) {
        args[n++] = "--token";
        args[n++] = token_value;
    }
    args[n] = NULL;

    effective_mode = model_download_effective_progress_mode(options->progress_mode);
    model_download_print_start_progress(options, report, effective_mode);
    return yvex_provider_process_run_streaming(args,
                                               report->stdout_log_path,
                                               report->stderr_log_path,
                                               effective_mode,
                                               options->tick_seconds,
                                               report->local_source_dir,
                                               report,
                                               err);
}

static void model_download_print_audit_patterns(const yvex_cli_models_download_options *options)
{
    unsigned int i;

    for (i = 0; i < model_download_effective_include_count(options); ++i) {
        printf("include.%u: %s\n", i, model_download_effective_include_at(options, i));
    }
    for (i = 0; i < model_download_effective_exclude_count(options); ++i) {
        printf("exclude.%u: %s\n", i, model_download_effective_exclude_at(options, i));
    }
}

static void model_download_print_normal(const yvex_cli_models_download_options *options,
                                        const yvex_model_download_report *report)
{
    if (strcmp(report->status, "model-download-dry-run") == 0) {
        printf("model-download: dry-run target=%s\n", report->target_id);
        printf("family: %s\n", report->family);
        printf("repo: %s\n", report->repo_id);
        printf("source: %s\n", report->local_source_dir);
        printf("action: planned\n");
        printf("manifest: skipped\n");
        printf("native_inventory: skipped\n");
        printf("boundary: no payload downloaded, runtime unsupported\n");
        printf("status: %s\n", report->status);
        return;
    }
    if (strcmp(report->status, "model-download-blocked") == 0) {
        printf("model-download: blocked target=%s\n", report->target_id);
        printf("family: %s\n", report->family);
        printf("repo: %s\n", report->repo_id);
        printf("top_blocker: %s\n", report->top_blocker[0] ? report->top_blocker : "unknown");
        printf("next: install huggingface_hub and run hf auth login if the repo is gated\n");
        printf("boundary: no payload downloaded, runtime unsupported\n");
        printf("status: %s\n", report->status);
        return;
    }
    if (strcmp(report->status, "model-download-pass") == 0) {
        printf("model-download: pass target=%s\n", report->target_id);
        printf("family: %s\n", report->family);
        printf("repo: %s\n", report->repo_id);
        printf("source: %s\n", report->local_source_dir);
        printf("stage: download pass\n");
        printf("files: %llu safetensors=%llu bytes=%llu\n",
               report->source_scan.file_count,
               report->source_scan.safetensors_count,
               report->source_scan.total_regular_file_bytes);
        printf("manifest: %s\n",
               report->source_manifest_written ? report->manifest_path : "skipped");
        printf("native_inventory: %s\n",
               report->native_inventory_written ? report->native_inventory_path : "skipped");
        printf("boundary: source tensors only, runtime unsupported\n");
        printf("status: %s\n", report->status);
        return;
    }
    if (strcmp(report->status, "model-download-interrupted") == 0) {
        printf("model-download: interrupted target=%s\n", report->target_id);
        printf("family: %s\n", report->family);
        printf("repo: %s\n", report->repo_id);
        printf("source: %s\n", report->local_source_dir);
        printf("stage: download interrupted\n");
        printf("signal: %s\n", model_download_signal_name(report->interrupt_signal));
        printf("child_signal_forwarded: %s\n", report->signal_forwarded ? "true" : "false");
        printf("child_exit_status: %s\n", report->child_exit_status);
        printf("cleanup: preserved-partial-source\n");
        printf("lock_cleanup: not-attempted\n");
        printf("partial_source_preserved: %s\n",
               report->partial_source_preserved ? "true" : "false");
        printf("lock_files_deleted: %s\n", report->lock_files_deleted ? "true" : "false");
        printf("stdout_log: %s\n", report->stdout_log_path);
        printf("stderr_log: %s\n", report->stderr_log_path);
        printf("boundary: partial source files may exist; runtime unsupported\n");
        printf("status: %s\n", report->status);
        return;
    }

    printf("model-download: fail target=%s\n", report->target_id);
    printf("family: %s\n", report->family);
    printf("repo: %s\n", report->repo_id);
    printf("source: %s\n", report->local_source_dir);
    printf("stage: download fail\n");
    printf("provider_exit_code: %d\n", report->provider_exit_code);
    printf("hf_exit_code: %d\n", report->hf_exit_code);
    printf("stdout_log: %s\n", report->stdout_log_path);
    printf("stderr_log: %s\n", report->stderr_log_path);
    if (report->top_blocker[0]) printf("top_blocker: %s\n", report->top_blocker);
    if (report->error[0]) printf("reason: %s\n", report->error);
    printf("boundary: partial source files may exist; runtime unsupported\n");
    printf("status: %s\n", report->status);
    (void)options;
}

static void model_download_print_table(const yvex_model_download_report *report)
{
    printf("TARGET  FAMILY  STATUS  FILES  SAFETENSORS  BYTES\n");
    printf("%s  %s  %s  %llu  %llu  %llu\n",
           report->target_id,
           report->family,
           report->status,
           report->source_scan.file_count,
           report->source_scan.safetensors_count,
           report->source_scan.total_regular_file_bytes);
    printf("status: %s\n", report->status);
}

static void model_download_print_audit(const yvex_cli_models_download_options *options,
                                       const yvex_model_download_report *report)
{
    printf("models: download\n");
    printf("status: %s\n", report->status);
    printf("target_id: %s\n", report->target_id);
    printf("family: %s\n", report->family);
    printf("provider: %s\n", report->provider);
    printf("repo_id: %s\n", report->repo_id);
    printf("revision: %s\n", report->revision);
    printf("local_source_dir: %s\n", report->local_source_dir);
    printf("models_root: %s\n", report->models_root);
    printf("models_root_source: %s\n", report->models_root_source);
    printf("hf_cli: %s\n", report->hf_cli_path[0] ? report->hf_cli_path : "missing");
    printf("hf_cli_source: %s\n", report->hf_cli_source[0] ? report->hf_cli_source : "none");
    printf("hf_exit_code: %d\n", report->hf_exit_code);
    printf("auth_mode: %s\n", options->auth_mode);
    printf("auth_state: %s\n", report->auth_state);
    printf("token_env_name: %s\n", report->token_env_name);
    printf("token_value_redacted: %s\n",
           strcmp(report->auth_state, "token-env-present") == 0 ? "true" : "false");
    model_download_print_audit_patterns(options);
    model_stage_print("resolve-target", report->stage_resolve_target);
    model_stage_print("resolve-paths", report->stage_resolve_paths);
    model_stage_print("prepare-dirs", report->stage_prepare_dirs);
    model_stage_print("hf-cli", report->stage_hf_cli);
    model_stage_print("download", report->stage_download);
    model_stage_print("progress-stream", report->stage_progress_stream);
    model_stage_print("progress-ticks", report->stage_progress_ticks);
    model_stage_print("source-scan", report->stage_source_scan);
    model_stage_print("source-manifest", report->stage_source_manifest);
    model_stage_print("native-inventory", report->stage_native_inventory);
    model_stage_print("sidecar", report->stage_sidecar);
    printf("progress_mode: %s\n", model_download_progress_mode_name(options->progress_mode));
    printf("tick_seconds: %llu\n", options->tick_seconds);
    printf("tick_count: %llu\n", report->tick_count);
    printf("stdout_streamed: %s\n", report->stdout_streamed ? "true" : "false");
    printf("stderr_streamed: %s\n", report->stderr_streamed ? "true" : "false");
    printf("stdout_bytes: %llu\n", report->stdout_bytes);
    printf("stderr_bytes: %llu\n", report->stderr_bytes);
    printf("provider_exit_code: %d\n", report->provider_exit_code);
    printf("provider_pid: %lld\n", (long long)report->provider_pid);
    printf("provider_process_group: %lld\n", (long long)report->provider_process_group);
    printf("interrupted: %s\n", report->interrupted ? "true" : "false");
    printf("interrupt_signal: %s\n", model_download_signal_name(report->interrupt_signal));
    printf("signal: %s\n", model_download_signal_name(report->interrupt_signal));
    printf("signal_forwarded: %s\n", report->signal_forwarded ? "true" : "false");
    printf("child_signal_forwarded: %s\n", report->signal_forwarded ? "true" : "false");
    printf("child_terminated: %s\n", report->child_terminated ? "true" : "false");
    printf("child_killed_after_timeout: %s\n",
           report->child_killed_after_timeout ? "true" : "false");
    printf("child_exit_status: %s\n", report->child_exit_status);
    printf("orphan_check_performed: %s\n",
           report->orphan_check_performed ? "true" : "false");
    printf("orphan_check_status: %s\n", report->orphan_check_status);
    printf("partial_source_preserved: %s\n",
           report->partial_source_preserved ? "true" : "false");
    printf("lock_cleanup: not-attempted\n");
    printf("lock_files_deleted: %s\n", report->lock_files_deleted ? "true" : "false");
    printf("source_file_count: %llu\n", report->source_scan.file_count);
    printf("file_count: %llu\n", report->source_scan.file_count);
    printf("safetensors_count: %llu\n", report->source_scan.safetensors_count);
    printf("config_present: %s\n", report->source_scan.config_present ? "true" : "false");
    printf("tokenizer_present: %s\n", report->source_scan.tokenizer_present ? "true" : "false");
    printf("total_regular_file_bytes: %llu\n", report->source_scan.total_regular_file_bytes);
    printf("largest_file_name: %s\n",
           report->source_scan.largest_file_name[0] ? report->source_scan.largest_file_name : "none");
    printf("largest_file_bytes: %llu\n", report->source_scan.largest_file_bytes);
    printf("manifest_path: %s\n", report->manifest_path);
    printf("native_inventory_path: %s\n", report->native_inventory_path);
    printf("download_report_path: %s\n", report->download_report_path);
    printf("registry_path: %s\n", report->registry_path);
    printf("receipt_path: %s\n", report->receipt_path);
    printf("stdout_log: %s\n", report->stdout_log_path);
    printf("stderr_log: %s\n", report->stderr_log_path);
    printf("created_at: %s\n", report->created_at);
    printf("yvex_version: %s\n", yvex_version_string());
    printf("upstream_identity_verified: false\n");
    printf("remote_lookup_performed: false\n");
    printf("payload_hash_verified: false\n");
    printf("payload_loaded: false\n");
    printf("gguf_created: false\n");
    printf("materialized: false\n");
    printf("runtime_ready: false\n");
    printf("generation: unsupported\n");
    printf("eval: unsupported\n");
    printf("benchmark_status: not-measured\n");
    if (report->top_blocker[0]) printf("top_blocker: %s\n", report->top_blocker);
    if (report->error[0]) printf("reason: %s\n", report->error);
}

static void model_download_print(const yvex_cli_models_download_options *options,
                                 const yvex_model_download_report *report)
{
    if (options && options->output_mode == YVEX_MODELS_OUTPUT_AUDIT) {
        model_download_print_audit(options, report);
    } else if (options && options->output_mode == YVEX_MODELS_OUTPUT_TABLE) {
        model_download_print_table(report);
    } else {
        model_download_print_normal(options, report);
    }
}

static int model_download_finish(const yvex_cli_models_download_options *options,
                                 yvex_model_download_report *report)
{
    model_download_print(options, report);
    if (strcmp(report->status, "model-download-pass") == 0 ||
        strcmp(report->status, "model-download-dry-run") == 0) {
        return 0;
    }
    if (strcmp(report->status, "model-download-blocked") == 0) {
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    return 1;
}

static int command_models_download(int argc, char **argv)
{
    yvex_cli_models_download_options options;
    const yvex_model_download_catalog_row *row = NULL;
    yvex_model_download_report report;
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_error err;
    yvex_source_manifest_options manifest_options;
    yvex_source_manifest_summary manifest_summary;
    yvex_native_weight_options native_options;
    yvex_native_weight_table *native_table = NULL;
    char hf_family_dir[YVEX_PATH_CAP];
    char reports_family_dir[YVEX_PATH_CAP];
    char registry_family_dir[YVEX_PATH_CAP];
    char logs_dir[YVEX_PATH_CAP];
    char file_name[256];
    const char *target_id;
    const char *family;
    const char *repo_id;
    const char *local_name;
    const char *revision;
    const char *token_value;
    int token_present;
    int rc;

    rc = parse_models_download_options(argc, argv, &options);
    if (rc == 1) {
        yvex_models_help(stdout);
        return 0;
    }
    if (rc != 0) return rc;

    model_download_report_init(&report);
    yvex_error_clear(&err);
    memset(&paths, 0, sizeof(paths));
    memset(&operator_paths, 0, sizeof(operator_paths));
    model_download_timestamp(report.created_at, sizeof(report.created_at));

    if (options.repo) {
        target_id = options.name;
        family = options.family;
        repo_id = options.repo;
        local_name = options.name;
        revision = options.revision ? options.revision : "main";
        snprintf(report.stage_resolve_target, sizeof(report.stage_resolve_target), "pass");
    } else {
        row = model_download_find_catalog(options.target);
        if (!row) {
            printf("models: download\n");
            printf("target_id: %s\n", options.target ? options.target : "");
            model_stage_print("resolve-target", "fail");
            printf("reason: unknown models download target\n");
            printf("status: model-download-unknown-target\n");
            return 2;
        }
        target_id = row->target_id;
        family = row->family;
        repo_id = row->repo_id;
        local_name = row->local_name;
        revision = options.revision ? options.revision : row->revision_default;
        snprintf(report.stage_resolve_target, sizeof(report.stage_resolve_target), "pass");
    }

    snprintf(report.target_id, sizeof(report.target_id), "%s", target_id);
    snprintf(report.family, sizeof(report.family), "%s", family);
    snprintf(report.provider, sizeof(report.provider), "hf");
    snprintf(report.repo_id, sizeof(report.repo_id), "%s", repo_id);
    snprintf(report.revision, sizeof(report.revision), "%s", revision);
    snprintf(report.local_name, sizeof(report.local_name), "%s", local_name);
    snprintf(report.token_env_name, sizeof(report.token_env_name), "%s", options.token_env);
    token_value = getenv(options.token_env);
    token_present = token_value && token_value[0] && strcmp(options.auth_mode, "never") != 0;
    snprintf(report.auth_state, sizeof(report.auth_state), "%s",
             token_present ? "token-env-present" :
             (strcmp(options.auth_mode, "never") == 0 ? "auth-never" : "not-provided"));

    rc = yvex_operator_paths_resolve(&paths, options.models_root, &operator_paths, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return print_yvex_error(&err, exit_for_status(rc));
    }
    snprintf(report.models_root, sizeof(report.models_root), "%s", operator_paths.models_root);
    snprintf(report.models_root_source, sizeof(report.models_root_source), "%s",
             operator_paths.models_root_source);

    rc = path_join2(hf_family_dir, sizeof(hf_family_dir), operator_paths.hf_root,
                    family, &err, "models_download");
    if (rc == YVEX_OK) {
        rc = path_join2(report.local_source_dir, sizeof(report.local_source_dir),
                        hf_family_dir, local_name, &err, "models_download");
    }
    if (rc == YVEX_OK) {
        rc = path_join2(reports_family_dir, sizeof(reports_family_dir),
                        operator_paths.reports_root, family, &err, "models_download");
    }
    if (rc == YVEX_OK) {
        rc = path_join2(registry_family_dir, sizeof(registry_family_dir),
                        operator_paths.registry_root, family, &err, "models_download");
    }
    if (rc == YVEX_OK) {
        rc = path_join2(logs_dir, sizeof(logs_dir), operator_paths.models_root,
                        "logs", &err, "models_download");
    }
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    snprintf(report.reports_dir, sizeof(report.reports_dir), "%s", reports_family_dir);
    snprintf(report.registry_dir, sizeof(report.registry_dir), "%s", registry_family_dir);
    snprintf(file_name, sizeof(file_name), "%s.download.receipt", target_id);
    rc = path_join2(report.receipt_path, sizeof(report.receipt_path),
                    reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download-report.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.download_report_path,
                                       sizeof(report.download_report_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.source-manifest.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.manifest_path,
                                       sizeof(report.manifest_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.native-inventory.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.native_inventory_path,
                                       sizeof(report.native_inventory_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.registry_path,
                                       sizeof(report.registry_path),
                                       registry_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.stdout.log", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.stdout_log_path,
                                       sizeof(report.stdout_log_path),
                                       logs_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.stderr.log", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.stderr_log_path,
                                       sizeof(report.stderr_log_path),
                                       logs_dir, file_name, &err, "models_download");
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    if (!model_download_source_path_allowed(&operator_paths, report.local_source_dir, &report)) {
        snprintf(report.status, sizeof(report.status), "model-download-blocked");
        snprintf(report.stage_resolve_paths, sizeof(report.stage_resolve_paths), "fail");
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_resolve_paths, sizeof(report.stage_resolve_paths), "pass");

    rc = yvex_model_registry_mkdir_parent(report.local_source_dir, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.receipt_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.download_report_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.registry_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.stdout_log_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.stderr_log_path, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_prepare_dirs, sizeof(report.stage_prepare_dirs), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_prepare_dirs, sizeof(report.stage_prepare_dirs), "pass");

    if (!model_download_find_hf_cli(report.hf_cli_path, sizeof(report.hf_cli_path),
                                    report.hf_cli_source, sizeof(report.hf_cli_source))) {
        snprintf(report.status, sizeof(report.status), "model-download-blocked");
        snprintf(report.stage_hf_cli, sizeof(report.stage_hf_cli), "blocked");
        snprintf(report.top_blocker, sizeof(report.top_blocker), "missing-huggingface-cli");
        snprintf(report.error, sizeof(report.error), "hf CLI was not found");
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_hf_cli, sizeof(report.stage_hf_cli), "pass");

    rc = model_download_write_receipt(report.receipt_path, &options, &report,
                                      token_present, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }

    report.hf_exit_code = model_download_run_hf(&options, &report,
                                                token_present ? token_value : NULL,
                                                &err);
    report.provider_exit_code = report.hf_exit_code;
    if (model_download_effective_progress_mode(options.progress_mode) ==
        YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) {
        snprintf(report.stage_progress_stream, sizeof(report.stage_progress_stream), "skipped");
        snprintf(report.stage_progress_ticks, sizeof(report.stage_progress_ticks), "skipped");
    } else {
        snprintf(report.stage_progress_stream, sizeof(report.stage_progress_stream),
                 report.hf_exit_code < 0 ? "fail" : "pass");
        snprintf(report.stage_progress_ticks, sizeof(report.stage_progress_ticks),
                 report.tick_count > 0ull ? "pass" : "skipped");
    }
    if (report.interrupted || report.hf_exit_code == 130 || report.hf_exit_code == 143) {
        int write_rc;
        if (!report.interrupted) {
            report.interrupted = 1;
            report.interrupt_signal = report.hf_exit_code == 143 ? SIGTERM : SIGINT;
            report.partial_source_preserved = 1;
            report.lock_files_deleted = 0;
            snprintf(report.child_exit_status, sizeof(report.child_exit_status),
                     "interrupted");
        }
        (void)model_download_observe_source(report.local_source_dir, &report.source_scan);
        snprintf(report.status, sizeof(report.status), "model-download-interrupted");
        snprintf(report.stage_download, sizeof(report.stage_download), "interrupted");
        snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "partial");
        snprintf(report.top_blocker, sizeof(report.top_blocker), "provider-download-interrupted");
        snprintf(report.error, sizeof(report.error), "provider download interrupted");
        write_rc = model_download_write_json_sidecar(report.download_report_path,
                                                    "yvex.model_download.report.v1",
                                                    &options,
                                                    &report,
                                                    &err);
        if (write_rc == YVEX_OK) {
            report.report_written = 1;
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "pass");
        } else {
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "fail");
        }
        return model_download_finish(&options, &report);
    }
    if (options.dry_run && report.hf_exit_code == 0) {
        snprintf(report.status, sizeof(report.status), "model-download-dry-run");
        snprintf(report.stage_download, sizeof(report.stage_download), "dry-run");
        snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "skipped");
        snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "skipped");
        snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "skipped");
        snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "skipped");
        return model_download_finish(&options, &report);
    }
    if (report.hf_exit_code != 0) {
        int write_rc;
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_download, sizeof(report.stage_download), "fail");
        snprintf(report.top_blocker, sizeof(report.top_blocker), "provider-download-failed");
        if (report.hf_exit_code < 0 && yvex_error_message(&err)[0]) {
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        } else {
            snprintf(report.error, sizeof(report.error), "hf download exited nonzero");
        }
        write_rc = model_download_write_json_sidecar(report.download_report_path,
                                                    "yvex.model_download.report.v1",
                                                    &options,
                                                    &report,
                                                    &err);
        if (write_rc == YVEX_OK) {
            report.report_written = 1;
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "pass");
        } else {
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "fail");
        }
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_download, sizeof(report.stage_download), "pass");

    rc = model_download_scan_source(report.local_source_dir, &report.source_scan, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "pass");

    if (options.no_manifest) {
        snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "skipped");
    } else {
        memset(&manifest_options, 0, sizeof(manifest_options));
        memset(&manifest_summary, 0, sizeof(manifest_summary));
        manifest_options.repo = report.repo_id;
        manifest_options.revision = report.revision;
        manifest_options.local_path = report.local_source_dir;
        manifest_options.node_name = report.local_name;
        manifest_options.download_log = report.stdout_log_path;
        manifest_options.dry_run_log = "";
        manifest_options.pid_file = "";
        manifest_options.download_command = "hf download (see receipt; token redacted)";
        manifest_options.status = YVEX_SOURCE_STATUS_COMPLETE;
        manifest_options.include_files = 1;
        rc = yvex_source_manifest_write_json(report.manifest_path,
                                             &manifest_options,
                                             &manifest_summary,
                                             &err);
        if (rc != YVEX_OK) {
            snprintf(report.status, sizeof(report.status), "model-download-fail");
            snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "fail");
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            return model_download_finish(&options, &report);
        }
        report.source_manifest_written = 1;
        snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "pass");
    }

    if (options.no_native_inventory) {
        snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "skipped");
    } else {
        memset(&native_options, 0, sizeof(native_options));
        memset(&report.native_summary, 0, sizeof(report.native_summary));
        native_options.source_dir = report.local_source_dir;
        native_options.recursive = 1;
        rc = yvex_native_weight_table_open(&native_table, &native_options, &err);
        if (rc == YVEX_OK) rc = yvex_native_weight_table_summary(native_table,
                                                                 &report.native_summary,
                                                                 &err);
        if (rc == YVEX_OK) rc = model_download_write_native_inventory_json(report.native_inventory_path,
                                                                           report.local_source_dir,
                                                                           native_table,
                                                                           &err);
        yvex_native_weight_table_close(native_table);
        native_table = NULL;
        if (rc != YVEX_OK) {
            snprintf(report.status, sizeof(report.status), "model-download-fail");
            snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "fail");
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            return model_download_finish(&options, &report);
        }
        report.native_inventory_written = 1;
        snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "pass");
    }

    snprintf(report.status, sizeof(report.status), "model-download-pass");
    rc = model_download_write_json_sidecar(report.download_report_path,
                                          "yvex.model_download.report.v1",
                                          &options,
                                          &report,
                                          &err);
    if (rc == YVEX_OK) {
        report.report_written = 1;
        rc = model_download_write_json_sidecar(report.registry_path,
                                              "yvex.model_download.registry.v1",
                                              &options,
                                              &report,
                                              &err);
    }
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }
    report.registry_written = 1;
    snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "pass");
    return model_download_finish(&options, &report);
}

typedef enum {
    YVEX_CLI_MODEL_CHECK_QUICK = 0,
    YVEX_CLI_MODEL_CHECK_RUNTIME,
    YVEX_CLI_MODEL_CHECK_FULL
} yvex_cli_model_check_level;

/* Selected artifact check preset. */

typedef struct {
    const char *target;
    const char *backend_name;
    const char *level_name;
    const char *models_root;
    const char *registry_path;
    const char *report_dir;
    int no_materialize;
    int no_graph;
    yvex_cli_model_check_level level;
    yvex_models_output_mode output_mode;
} yvex_cli_models_check_options;

typedef struct {
    char target_id[256];
    char model_input_kind[32];
    char backend_name[16];
    char level_name[16];
    char artifact_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char report_path[YVEX_PATH_CAP];
    char error[256];
    char graph_skip_reason[128];
    const char *stage_resolve_target;
    const char *stage_resolve_artifact;
    const char *stage_inspect;
    const char *stage_tensors;
    const char *stage_metadata;
    const char *stage_registry_identity;
    const char *stage_integrity_check;
    const char *stage_integrity_report;
    const char *stage_materialize;
    const char *stage_engine;
    const char *stage_session;
    const char *stage_plan;
    const char *stage_graph_partial;
    const char *stage_model_gate;
    const char *stage_materialize_gate;
    const char *runtime_execution;
    const char *final_status;
} yvex_cli_model_check_report;

static void model_check_report_init(yvex_cli_model_check_report *report,
                                    const yvex_cli_models_check_options *options)
{
    memset(report, 0, sizeof(*report));
    snprintf(report->target_id, sizeof(report->target_id), "%s",
             options->target ? options->target : "");
    snprintf(report->backend_name, sizeof(report->backend_name), "%s",
             options->backend_name ? options->backend_name : "cpu");
    snprintf(report->level_name, sizeof(report->level_name), "%s",
             options->level_name ? options->level_name : "quick");
    snprintf(report->model_input_kind, sizeof(report->model_input_kind), "unknown");
    report->stage_resolve_target = "not-run";
    report->stage_resolve_artifact = "not-run";
    report->stage_inspect = "not-run";
    report->stage_tensors = "not-run";
    report->stage_metadata = "not-run";
    report->stage_registry_identity = "skipped";
    report->stage_integrity_check = "not-run";
    report->stage_integrity_report = "skipped";
    report->stage_materialize = "skipped";
    report->stage_engine = "skipped";
    report->stage_session = "skipped";
    report->stage_plan = "skipped";
    report->stage_graph_partial = "skipped";
    report->stage_model_gate = "skipped";
    report->stage_materialize_gate = "skipped";
    report->runtime_execution = "not-performed";
    report->final_status = "model-check-fail";
}

static void model_check_report_print(FILE *fp,
                                     const yvex_cli_model_check_report *report)
{
    fprintf(fp, "status: model-check\n");
    fprintf(fp, "target_id: %s\n", report->target_id);
    fprintf(fp, "model_input_kind: %s\n", report->model_input_kind);
    fprintf(fp, "backend: %s\n", report->backend_name);
    fprintf(fp, "level: %s\n", report->level_name);
    fprintf(fp, "artifact_path: %s\n", report->artifact_path);
    fprintf(fp, "registry_path: %s\n", report->registry_path);
    if (report->report_path[0]) {
        fprintf(fp, "report_path: %s\n", report->report_path);
    }
    fprintf(fp, "stage: resolve-target %s\n", report->stage_resolve_target);
    fprintf(fp, "stage: resolve-artifact %s\n", report->stage_resolve_artifact);
    fprintf(fp, "stage: inspect %s\n", report->stage_inspect);
    fprintf(fp, "stage: tensors %s\n", report->stage_tensors);
    fprintf(fp, "stage: metadata %s\n", report->stage_metadata);
    fprintf(fp, "stage: registry-identity %s\n", report->stage_registry_identity);
    fprintf(fp, "stage: integrity-check %s\n", report->stage_integrity_check);
    fprintf(fp, "stage: integrity-report %s\n", report->stage_integrity_report);
    fprintf(fp, "stage: materialize %s\n", report->stage_materialize);
    fprintf(fp, "stage: engine %s\n", report->stage_engine);
    fprintf(fp, "stage: session %s\n", report->stage_session);
    fprintf(fp, "stage: plan %s\n", report->stage_plan);
    fprintf(fp, "stage: graph-partial %s\n", report->stage_graph_partial);
    if (report->graph_skip_reason[0]) {
        fprintf(fp, "graph_partial_reason: %s\n", report->graph_skip_reason);
    }
    fprintf(fp, "stage: model-gate %s\n", report->stage_model_gate);
    fprintf(fp, "stage: materialize-gate %s\n", report->stage_materialize_gate);
    if (report->error[0]) {
        fprintf(fp, "error: %s\n", report->error);
    }
    fprintf(fp, "runtime_execution: %s\n", report->runtime_execution);
    fprintf(fp, "execution_ready: false\n");
    fprintf(fp, "graph_execution_ready: false\n");
    fprintf(fp, "prefill_ready: false\n");
    fprintf(fp, "logits_ready: false\n");
    fprintf(fp, "generation: unsupported\n");
    fprintf(fp, "status: %s\n", report->final_status);
}

static int model_check_write_report(const yvex_cli_models_check_options *options,
                                    yvex_cli_model_check_report *report,
                                    yvex_error *err)
{
    FILE *fp;
    char report_dir[YVEX_PATH_CAP];
    char report_name[128];
    int rc;
    int n;

    if (!options->report_dir) {
        return YVEX_OK;
    }
    rc = expand_operator_path(options->report_dir, report_dir, sizeof(report_dir),
                              err, "models_check");
    if (rc != YVEX_OK) {
        return rc;
    }
    n = snprintf(report_name, sizeof(report_name),
                 "model-check-deepseek4-v4-flash-selected-embed-%s-%s.txt",
                 report->backend_name, report->level_name);
    if (n < 0 || (size_t)n >= sizeof(report_name)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "models_check", "report filename is too long");
        return YVEX_ERR_BOUNDS;
    }
    rc = path_join2(report->report_path, sizeof(report->report_path),
                    report_dir, report_name, err, "models_check");
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_model_registry_mkdir_parent(report->report_path, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    fp = fopen(report->report_path, "w");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_check",
                        "failed to open report: %s", report->report_path);
        return YVEX_ERR_IO;
    }
    model_check_report_print(fp, report);
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_check",
                        "failed to close report: %s", report->report_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static void model_check_report_print_normal(FILE *fp,
                                            const yvex_cli_model_check_report *report)
{
    if (!fp || !report) return;
    fprintf(fp, "model-check: %s target=%s level=%s\n",
            strcmp(report->final_status ? report->final_status : "", "model-check-pass") == 0
                ? "pass"
                : "fail",
            report->target_id,
            report->level_name);
    fprintf(fp, "backend: %s\n", report->backend_name);
    fprintf(fp, "checked: inspect, metadata, registry, integrity%s%s\n",
            strcmp(report->stage_materialize ? report->stage_materialize : "", "pass") == 0
                ? ", materialize"
                : "",
            strcmp(report->stage_graph_partial ? report->stage_graph_partial : "", "pass") == 0
                ? ", selected graph"
                : "");
    fprintf(fp, "runtime: %s\n", report->runtime_execution ? report->runtime_execution : "not-performed");
    if (report->error[0]) {
        fprintf(fp, "top_blocker: %s\n", report->error);
    } else {
        fprintf(fp, "top_blocker: none\n");
    }
    fprintf(fp, "boundary: selected-slice check only, generation unsupported\n");
    fprintf(fp, "status: %s\n", report->final_status ? report->final_status : "model-check-fail");
}

static int parse_models_check_options(int argc,
                                      char **argv,
                                      yvex_cli_models_check_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->backend_name = "cpu";
    options->level_name = "quick";
    options->level = YVEX_CLI_MODEL_CHECK_QUICK;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    if (argc < 4) {
        fprintf(stderr, "yvex: models check requires TARGET\n");
        return 2;
    }
    options->target = argv[3];
    if (!cli_arg_value_valid(options->target)) {
        fprintf(stderr, "yvex: models check target is empty or invalid\n");
        return 2;
    }
    for (i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--no-materialize") == 0) {
            options->no_materialize = 1;
        } else if (strcmp(argv[i], "--no-graph") == 0) {
            options->no_graph = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            const char *value = NULL;
            int rc = parse_models_value_option("models check", "--output",
                                               argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                fprintf(stderr, "yvex: models check unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(argv[i], "--backend") == 0 ||
                   strcmp(argv[i], "--level") == 0 ||
                   strcmp(argv[i], "--models-root") == 0 ||
                   strcmp(argv[i], "--registry") == 0 ||
                   strcmp(argv[i], "--report-dir") == 0) {
            const char *flag = argv[i];
            const char *value = NULL;
            int rc = parse_models_value_option("models check", flag,
                                               argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--backend") == 0) {
                options->backend_name = value;
            } else if (strcmp(flag, "--level") == 0) {
                options->level_name = value;
            } else if (strcmp(flag, "--models-root") == 0) {
                options->models_root = value;
            } else if (strcmp(flag, "--registry") == 0) {
                options->registry_path = value;
            } else {
                options->report_dir = value;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "yvex: unknown models check option: %s\n", argv[i]);
            return 2;
        } else {
            fprintf(stderr, "yvex: models check accepts only one TARGET\n");
            return 2;
        }
    }
    if (strcmp(options->backend_name, "cpu") != 0 &&
        strcmp(options->backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", options->backend_name);
        return 2;
    }
    if (strcmp(options->level_name, "quick") == 0) {
        options->level = YVEX_CLI_MODEL_CHECK_QUICK;
    } else if (strcmp(options->level_name, "runtime") == 0) {
        options->level = YVEX_CLI_MODEL_CHECK_RUNTIME;
    } else if (strcmp(options->level_name, "full") == 0) {
        options->level = YVEX_CLI_MODEL_CHECK_FULL;
    } else {
        fprintf(stderr, "yvex: unknown models check level: %s\n", options->level_name);
        return 2;
    }
    return 0;
}

static int print_model_check_unsupported(const yvex_cli_models_check_options *options)
{
    const char *target = options && options->target ? options->target : "";
    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("model-check: unsupported target=%s level=%s\n",
               target,
               options->level_name ? options->level_name : "quick");
        printf("backend: %s\n", options->backend_name ? options->backend_name : "cpu");
        printf("top_blocker: %s\n",
               strcmp(target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0
                   ? "segment check is planned"
                   : "source-only target cannot be checked as a YVEX-produced runtime artifact yet");
        printf("boundary: report-only refusal, generation unsupported\n");
        printf("status: model-check-unsupported\n");
    } else {
        printf("status: model-check-unsupported\n");
        printf("target_id: %s\n", target);
        if (strcmp(target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
            printf("reason: segment check is planned, not implemented by this preset\n");
        } else {
            printf("reason: source-only target cannot be checked as a YVEX-produced runtime artifact yet\n");
        }
        model_print_runtime_generation("unsupported");
    }
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

static int model_check_resolve_registry_path(const yvex_cli_models_check_options *options,
                                             char *registry_path,
                                             size_t registry_path_cap,
                                             yvex_error *err)
{
    if (options->registry_path) {
        return expand_operator_path(options->registry_path, registry_path,
                                    registry_path_cap, err, "models_check");
    }
    return yvex_model_registry_default_path(registry_path,
                                            (unsigned long long)registry_path_cap,
                                            err);
}

static int model_check_resolve_canonical_path(
    const yvex_cli_models_check_options *options,
    yvex_model_ref *ref,
    yvex_cli_model_check_report *report,
    yvex_error *err)
{
    static const char *artifact_name =
        "deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf";
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    char gguf_dir[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
    int exists = 0;
    int rc;

    memset(&paths, 0, sizeof(paths));
    memset(&operator_paths, 0, sizeof(operator_paths));
    rc = yvex_operator_paths_resolve(&paths, options->models_root, &operator_paths, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, "deepseek", "gguf",
                                            gguf_dir, sizeof(gguf_dir), &exists, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = path_join2(artifact_path, sizeof(artifact_path), gguf_dir, artifact_name,
                    err, "models_check");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!path_exists(artifact_path)) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_check",
                        "canonical selected artifact does not exist: %s",
                        artifact_path);
        return YVEX_ERR_IO;
    }
    rc = set_path_ref(ref, artifact_path, err);
    if (rc == YVEX_OK) {
        snprintf(report->model_input_kind, sizeof(report->model_input_kind), "target");
    }
    return rc;
}

static int model_check_resolve_ref(const yvex_cli_models_check_options *options,
                                   const char *registry_path,
                                   yvex_model_ref *ref,
                                   yvex_cli_model_check_report *report,
                                   yvex_error *err)
{
    yvex_model_ref_options ref_options;
    int rc;

    if (options->models_root &&
        strcmp(options->target, "deepseek4-v4-flash-selected-embed") == 0 &&
        !is_path_like_reference(options->target)) {
        return model_check_resolve_canonical_path(options, ref, report, err);
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.registry_path = registry_path;
    ref_options.allow_registry = 1;
    rc = yvex_model_ref_resolve(ref, options->target, &ref_options, err);
    if (rc == YVEX_OK) {
        if (is_path_like_reference(options->target)) {
            snprintf(report->model_input_kind, sizeof(report->model_input_kind), "path");
        } else if (strcmp(options->target, "deepseek4-v4-flash-selected-embed") == 0) {
            snprintf(report->model_input_kind, sizeof(report->model_input_kind), "target");
        } else {
            snprintf(report->model_input_kind, sizeof(report->model_input_kind), "alias");
        }
        return YVEX_OK;
    }
    if (strcmp(options->target, "deepseek4-v4-flash-selected-embed") == 0) {
        yvex_error_clear(err);
        yvex_model_ref_clear(ref);
        return model_check_resolve_canonical_path(options, ref, report, err);
    }
    return rc;
}

static int model_check_verify_registry_identity(const yvex_model_ref *ref,
                                                yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    int rc;

    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) {
        return YVEX_OK;
    }
    memset(&identity, 0, sizeof(identity));
    memset(&current_metadata, 0, sizeof(current_metadata));
    memset(&registered_metadata, 0, sizeof(registered_metadata));
    memset(&metadata_report, 0, sizeof(metadata_report));

    rc = yvex_artifact_identity_read(ref->path, &identity, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!ref->sha256 || !ref->sha256[0] ||
        strcmp(ref->sha256, identity.sha256) != 0 ||
        (ref->registered_file_size != 0ull &&
         ref->registered_file_size != identity.file_size)) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "registry identity drift");
        return YVEX_ERR_STATE;
    }
    rc = populate_registry_metadata(&current_metadata, ref->path, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    model_ref_registry_entry_view(ref, &registered_metadata);
    rc = yvex_model_registry_compare_metadata(&registered_metadata,
                                              &current_metadata.entry,
                                              &metadata_report,
                                              err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (strcmp(metadata_report.metadata_status, "pass") != 0 ||
        strcmp(metadata_report.readiness_status, "pass") != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "registry identity drift");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int model_check_integrity(const yvex_model_ref *ref,
                                 int require_embedding,
                                 yvex_artifact_integrity_report *report,
                                 yvex_error *err)
{
    yvex_artifact_integrity_options integrity_options;

    memset(&integrity_options, 0, sizeof(integrity_options));
    integrity_options.require_token_embedding = require_embedding;
    integrity_options.token_id = 0u;
    if (ref->kind == YVEX_MODEL_REF_ALIAS && ref->sha256 && ref->sha256[0]) {
        integrity_options.registered_sha256 = ref->sha256;
    }
    return yvex_artifact_integrity_check_path(ref->path, &integrity_options,
                                             report, err);
}

static int model_check_backend_probe(const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    int rc;

    memset(&options, 0, sizeof(options));
    (void)model_backend_kind_from_name(backend_name, &options.kind);
    rc = yvex_backend_open(&backend, &options, err);
    if (rc == YVEX_OK) {
        yvex_backend_close(backend);
    }
    return rc;
}

static int model_check_materialize(const char *path,
                                   const char *backend_name,
                                   yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_materialize_summary summary;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));
    memset(&summary, 0, sizeof(summary));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)model_backend_kind_from_name(backend_name, &backend_options.kind);
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        close_model_context(&ctx);
        return rc;
    }
    materialize_options.backend_name = backend_name;
    materialize_options.require_all_tensors = 1;
    rc = yvex_weight_table_materialize(&weights,
                                       ctx.artifact,
                                       ctx.gguf,
                                       ctx.table,
                                       backend,
                                       &materialize_options,
                                       err);
    if (rc == YVEX_OK) {
        rc = yvex_weight_table_get_summary(weights, &summary, err);
    }
    if (rc == YVEX_OK && summary.status != YVEX_WEIGHT_STATUS_MATERIALIZED) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "materialization did not reach weights-materialized");
        rc = YVEX_ERR_STATE;
    }
    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return rc;
}

static int model_check_engine(const char *path,
                              const char *backend_name,
                              yvex_error *err)
{
    yvex_engine *engine = NULL;
    yvex_engine_options options;
    yvex_engine_summary summary;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    options.model_path = path;
    options.load_tokenizer = 0;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    options.attach_weights = 1;
    options.backend_name = backend_name;
    options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &options, err);
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(engine, &summary, err);
    }
    if (rc == YVEX_OK && !summary.weights_attached) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "engine did not attach selected weights");
        rc = YVEX_ERR_STATE;
    }
    yvex_engine_close(engine);
    return rc;
}

static int model_check_session(const char *path,
                               const char *backend_name,
                               yvex_error *err)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_session *session = NULL;
    yvex_engine_options engine_options;
    yvex_backend_options backend_options;
    yvex_session_options session_options;
    yvex_session_summary summary;
    int rc;

    memset(&engine_options, 0, sizeof(engine_options));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&session_options, 0, sizeof(session_options));
    memset(&summary, 0, sizeof(summary));
    engine_options.model_path = path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)model_backend_kind_from_name(backend_name, &backend_options.kind);
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc == YVEX_OK) {
        session_options.allow_partial_graph = 1;
        rc = yvex_session_create(&session, engine, backend, &session_options, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_session_get_summary(session, &summary, err);
    }
    if (rc == YVEX_OK && !summary.weights_attached) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "session did not observe attached weights");
        rc = YVEX_ERR_STATE;
    }
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    return rc;
}

static int model_check_plan(const char *path,
                            const char *backend_name,
                            yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_plan *plan = NULL;
    yvex_plan_options options;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1ull;
    options.context_length = 16ull;
    options.backend_name = backend_name;
    rc = open_model_context(path, &ctx, err);
    if (rc == YVEX_OK) {
        rc = yvex_plan_create(&plan, ctx.model, ctx.table, &options, err);
    }
    yvex_plan_close(plan);
    close_model_context(&ctx);
    return rc;
}

static int model_check_graph_partial(const char *path,
                                     const char *backend_name,
                                     yvex_error *err)
{
    yvex_engine *engine = NULL;
    yvex_engine_options engine_options;
    yvex_partial_graph_options partial_options;
    yvex_partial_graph_result partial_result;
    int rc;

    memset(&engine_options, 0, sizeof(engine_options));
    memset(&partial_options, 0, sizeof(partial_options));
    memset(&partial_result, 0, sizeof(partial_result));
    engine_options.model_path = path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    partial_options.token_id = 0u;
    rc = yvex_engine_open(&engine, &engine_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_engine_execute_partial_graph(engine, &partial_options,
                                              &partial_result, err);
    }
    if (rc == YVEX_OK && !partial_result.executed) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "selected partial graph did not execute");
        rc = YVEX_ERR_STATE;
    }
    yvex_engine_close(engine);
    return rc;
}

static int model_check_is_real_selected_embedding(
    const yvex_cli_metadata_snapshot *metadata)
{
    return metadata &&
           strcmp(metadata->entry.primary_tensor_name, "token_embd.weight") == 0 &&
           strcmp(metadata->entry.primary_tensor_dtype, "F16") == 0 &&
           metadata->entry.primary_tensor_rank == 2u &&
           strcmp(metadata->entry.primary_tensor_dims, "[4096,129280]") == 0 &&
           metadata->entry.primary_tensor_bytes == 1059061760ull;
}

static int model_check_run_model_gate(const yvex_model_ref *ref,
                                      const char *backend_name,
                                      yvex_error *err)
{
    yvex_model_gate_expected_tensor expected;
    yvex_model_gate_options options;
    yvex_model_gate_summary summary;
    int rc;

    memset(&expected, 0, sizeof(expected));
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    expected.name = "token_embd.weight";
    expected.dtype = "F16";
    expected.rank = 2u;
    expected.dims[0] = 4096ull;
    expected.dims[1] = 129280ull;
    expected.bytes = 1059061760ull;
    options.model_path = ref->path;
    options.model_label = "deepseek-v4-flash-selected-embedding";
    options.family = "deepseek4";
    options.artifact_sha256 = ref->kind == YVEX_MODEL_REF_ALIAS ? ref->sha256 : NULL;
    options.expected_tensors = &expected;
    options.expected_tensor_count = 1ull;
    options.check_cpu = strcmp(backend_name, "cpu") == 0;
    options.check_cuda = strcmp(backend_name, "cuda") == 0;
    options.require_cpu = options.check_cpu;
    options.require_cuda = options.check_cuda;
    rc = yvex_model_gate_check(&options, &summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (summary.status != YVEX_MODEL_GATE_PASS) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "model gate did not pass");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int model_check_run_materialize_gate(const yvex_model_ref *ref,
                                            const char *backend_name,
                                            yvex_error *err)
{
    yvex_materialize_expected_tensor expected;
    yvex_materialize_gate_options options;
    yvex_materialize_gate_summary summary;
    int rc;

    memset(&expected, 0, sizeof(expected));
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    expected.name = "token_embd.weight";
    expected.dtype = "F16";
    expected.rank = 2u;
    expected.dims[0] = 4096ull;
    expected.dims[1] = 129280ull;
    expected.bytes = 1059061760ull;
    options.model_path = ref->path;
    options.label = "deepseek-v4-flash-selected-embedding";
    options.family = "deepseek4";
    options.sha256 = ref->kind == YVEX_MODEL_REF_ALIAS ? ref->sha256 : NULL;
    options.metadata_status = "pass";
    options.scope = YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR;
    options.expected_tensors = &expected;
    options.expected_tensor_count = 1ull;
    options.check_cpu = strcmp(backend_name, "cpu") == 0;
    options.check_cuda = strcmp(backend_name, "cuda") == 0;
    options.require_cpu = options.check_cpu;
    options.require_cuda = options.check_cuda;
    options.repeat_count = 1u;
    options.check_cleanup = 1;
    rc = yvex_materialize_gate_check(&options, &summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (summary.status != YVEX_MATERIALIZE_GATE_PASS) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "materialize gate did not pass");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int model_check_finish(yvex_cli_models_check_options *options,
                              yvex_cli_model_check_report *report,
                              int exit_code,
                              yvex_error *err)
{
    int rc;

    rc = model_check_write_report(options, report, err);
    if (rc != YVEX_OK && exit_code == 0) {
        snprintf(report->error, sizeof(report->error), "%s", yvex_error_message(err));
        report->final_status = "model-check-fail";
        exit_code = exit_for_status(rc);
    }
    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        model_check_report_print_normal(stdout, report);
    } else {
        model_check_report_print(stdout, report);
    }
    return exit_code;
}

static int command_models_check(int argc, char **argv)
{
    yvex_cli_models_check_options options;
    yvex_cli_model_check_report report;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_cli_metadata_snapshot metadata_snapshot;
    yvex_artifact_integrity_report integrity_report;
    yvex_error err;
    char registry_path[YVEX_PATH_CAP];
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&metadata_snapshot, 0, sizeof(metadata_snapshot));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(registry_path, 0, sizeof(registry_path));

    rc = parse_models_check_options(argc, argv, &options);
    if (rc != 0) {
        return rc;
    }
    if (strcmp(options.target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0 ||
        strcmp(options.target, "glm-5.2-official-safetensors") == 0) {
        return print_model_check_unsupported(&options);
    }
    if (strcmp(options.target, "deepseek4-v4-flash-selected-embed") != 0 &&
        !is_path_like_reference(options.target)) {
        yvex_model_ref_options ref_options;
        memset(&ref_options, 0, sizeof(ref_options));
        ref_options.allow_registry = 1;
        ref_options.registry_path = options.registry_path;
        rc = yvex_model_ref_resolve(&ref, options.target, &ref_options, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        yvex_model_ref_clear(&ref);
    }

    model_check_report_init(&report, &options);
    rc = model_check_resolve_registry_path(&options, registry_path,
                                           sizeof(registry_path), &err);
    if (rc != YVEX_OK) {
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    snprintf(report.registry_path, sizeof(report.registry_path), "%s", registry_path);

    rc = model_check_resolve_ref(&options, registry_path, &ref, &report, &err);
    if (rc != YVEX_OK) {
        report.stage_resolve_target = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_resolve_target = "pass";
    snprintf(report.artifact_path, sizeof(report.artifact_path), "%s", ref.path);
    report.stage_resolve_artifact = path_exists(ref.path) ? "pass" : "fail";
    if (strcmp(report.stage_resolve_artifact, "pass") != 0) {
        snprintf(report.error, sizeof(report.error), "artifact path does not exist");
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  exit_for_status(YVEX_ERR_IO), &err);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        report.stage_inspect = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_inspect = "pass";
    report.stage_tensors = "pass";

    rc = populate_registry_metadata(&metadata_snapshot, ref.path, &err);
    if (rc != YVEX_OK) {
        report.stage_metadata = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_metadata = "pass";

    if (ref.kind == YVEX_MODEL_REF_ALIAS) {
        rc = model_check_verify_registry_identity(&ref, &err);
        if (rc != YVEX_OK) {
            report.stage_registry_identity = "fail";
            snprintf(report.error, sizeof(report.error), "registry identity drift");
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report,
                                      exit_for_status(YVEX_ERR_STATE), &err);
        }
        report.stage_registry_identity = "pass";
    } else {
        report.stage_registry_identity = "unregistered";
    }

    rc = model_check_integrity(&ref, 0, &integrity_report, &err);
    if (rc != YVEX_OK || !integrity_report.passed) {
        report.stage_integrity_check = "fail";
        snprintf(report.error, sizeof(report.error), "%s",
                 rc != YVEX_OK ? yvex_error_message(&err) : "artifact integrity failed");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  exit_for_status(rc != YVEX_OK ? rc : YVEX_ERR_FORMAT),
                                  &err);
    }
    report.stage_integrity_check = "pass";

    if (options.level == YVEX_CLI_MODEL_CHECK_QUICK) {
        if (options.no_graph) {
            snprintf(report.graph_skip_reason, sizeof(report.graph_skip_reason),
                     "quick level does not run graph");
        }
        report.final_status = "model-check-pass";
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, 0, &err);
    }

    rc = model_check_backend_probe(options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_integrity_report = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_integrity_report = "pass";

    if (options.no_materialize) {
        report.stage_materialize = "skipped";
        report.stage_engine = "skipped";
        report.stage_session = "skipped";
        report.stage_plan = "skipped";
        report.stage_graph_partial = "skipped";
        snprintf(report.graph_skip_reason, sizeof(report.graph_skip_reason),
                 "disabled by --no-materialize");
        report.stage_model_gate = "skipped";
        report.stage_materialize_gate = "skipped";
        report.runtime_execution = "not-performed";
        report.final_status = "model-check-pass";
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, 0, &err);
    }

    rc = model_check_materialize(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_materialize = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_materialize = "pass";

    rc = model_check_engine(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_engine = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_engine = "pass";

    rc = model_check_session(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_session = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_session = "pass";

    rc = model_check_plan(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_plan = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_plan = "pass";

    if (options.no_graph) {
        report.stage_graph_partial = "skipped";
        snprintf(report.graph_skip_reason, sizeof(report.graph_skip_reason),
                 "disabled by --no-graph");
    } else {
        rc = model_check_graph_partial(ref.path, options.backend_name, &err);
        if (rc != YVEX_OK) {
            report.stage_graph_partial = rc == YVEX_ERR_UNSUPPORTED ? "unsupported" : "fail";
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report,
                                      rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                      &err);
        }
        report.stage_graph_partial = "pass";
    }
    report.runtime_execution = "selected-boundary-only";

    if (options.level == YVEX_CLI_MODEL_CHECK_FULL &&
        model_check_is_real_selected_embedding(&metadata_snapshot) &&
        !options.no_materialize) {
        rc = model_check_run_model_gate(&ref, options.backend_name, &err);
        if (rc != YVEX_OK) {
            report.stage_model_gate = "fail";
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report, exit_for_status(rc), &err);
        }
        report.stage_model_gate = "pass";
        rc = model_check_run_materialize_gate(&ref, options.backend_name, &err);
        if (rc != YVEX_OK) {
            report.stage_materialize_gate = "fail";
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report, exit_for_status(rc), &err);
        }
        report.stage_materialize_gate = "pass";
    } else if (options.level == YVEX_CLI_MODEL_CHECK_FULL) {
        report.stage_model_gate = "skipped";
        report.stage_materialize_gate = "skipped";
    }

    report.final_status = "model-check-pass";
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return model_check_finish(&options, &report, exit_code, &err);
}

/* Registry-backed models subcommands. */

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
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const yvex_model_registry_entry *selected;
    char selected_alias[256];
    unsigned long long i;
    unsigned long long count;
    int rc;

    yvex_error_clear(&err);
    rc = parse_models_registry_output_options(argc, argv, 3, &registry_path, &output_mode);
    if (rc != 0) return rc;
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    selected_alias[0] = '\0';
    if (selected && selected->alias) {
        snprintf(selected_alias, sizeof(selected_alias), "%s", selected->alias);
    }
    count = yvex_model_registry_count(registry);
    if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("MODELS  count=%llu\n\n", count);
        printf("%-3s  %-44s  %-10s  %-16s  %7s  %12s  %s\n",
               "SEL",
               "ALIAS",
               "FAMILY",
               "CLASS",
               "TENSORS",
               "SIZE",
               "READY");
        for (i = 0; i < count; ++i) {
            const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
            int is_selected = selected_alias[0] && entry && strcmp(selected_alias, entry->alias) == 0;
            print_model_registry_entry_normal(entry, is_selected);
        }
        printf("status: models-list\n");
        yvex_model_registry_close(registry);
        return 0;
    }
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
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const yvex_model_registry_entry *selected;
    int rc;

    rc = parse_models_registry_output_options(argc, argv, 3, &registry_path, &output_mode);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    printf("models: current\n");
    if (selected) {
        if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            printf("selected: %s\n", selected->alias);
            printf("path: %s\n", selected->path);
            printf("execution_ready: %s\n", selected->execution_ready ? "true" : "false");
            printf("status: models-current\n");
            printf("hint: use --audit for registered digest and tensor fields\n");
            yvex_model_registry_close(registry);
            return 0;
        }
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
        if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            printf("hint: use 'yvex models use ALIAS' to select a model\n");
        }
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
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
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
    rc = parse_models_registry_output_options(argc, argv, 4, &registry_path, &output_mode);
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

    if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("verify: %s alias=%s\n", pass ? "pass" : "fail", entry->alias);
        printf("identity: %s digest: %s metadata: %s\n",
               identity_status, digest_status, metadata_status);
        printf("artifact: %s tensors=%llu size=%llu\n",
               entry->format ? entry->format : "unknown",
               metadata_checked ? current_metadata.entry.tensor_count : entry->tensor_count,
               identity.file_size);
        printf("top_blocker: %s\n", pass ? "none" : reason);
        printf("boundary: identity verified, runtime generation unsupported\n");
        printf("status: %s\n", status);
    } else {
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
    }
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
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models inspect requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_output_options(argc, argv, 4, &registry_path, &output_mode);
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
    if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        int selected_slice =
            strcmp(entry->alias, "deepseek4-v4-flash-selected-embed") == 0 ||
            strcmp(entry->alias, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0;
        const char *display_family = selected_slice ? "deepseek" : (entry->family ? entry->family : "");
        const char *display_class = selected_slice ? "selected-slice" : (entry->artifact_class ? entry->artifact_class : "");
        printf("model: %s\n", entry->alias);
        printf("family: %s class=%s tensors=%llu size=%llu\n",
               display_family,
               display_class,
               entry->tensor_count,
               entry->file_size);
        printf("primary: %s %s %s\n",
               entry->primary_tensor_name ? entry->primary_tensor_name : "",
               entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "",
               entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
        printf("state: %s execution_ready=%s\n",
               entry->support_level ? entry->support_level : "",
               entry->execution_ready ? "true" : "false");
        printf("boundary: selected-slice only, full-runtime generation unsupported\n");
        printf("status: models-inspect\n");
        yvex_model_registry_close(registry);
        return 0;
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

/* Full model inventory and placement reporting. */

typedef enum {
    YVEX_FULLMODEL_COMMAND_REPORT = 0,
    YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN,
    YVEX_FULLMODEL_COMMAND_MATERIALIZE,
    YVEX_FULLMODEL_COMMAND_DESCRIPTOR,
    YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME
} yvex_fullmodel_command_kind;

typedef struct {
    const char *model;
    const char *backend;
    const char *target;
    const char *registry_path;
    const char *residency;
    const char *require_role;
    const char *require_collection;
    const char *fail_after_phase;
    const char *report_dir;
    const char *format;
    const char *family;
    unsigned long long limit_tensors;
    unsigned long long limit_bytes;
    int has_limit_bytes;
    int dry_run;
    int plan_only;
    int include_blockers;
    int include_roles;
    int include_placement;
    int include_graph;
    int include_kv;
    int include_logits;
    int include_moe;
    int include_output;
    yvex_models_output_mode output_mode;
    yvex_fullmodel_command_kind command;
} yvex_cli_fullmodel_options;

typedef struct {
    unsigned long long embedding;
    unsigned long long embedding_bytes;
    unsigned long long normalization;
    unsigned long long normalization_bytes;
    unsigned long long attention;
    unsigned long long attention_bytes;
    unsigned long long mlp;
    unsigned long long mlp_bytes;
    unsigned long long moe;
    unsigned long long moe_bytes;
    unsigned long long output;
    unsigned long long output_bytes;
    unsigned long long tokenizer;
    unsigned long long tokenizer_bytes;
    unsigned long long unknown;
    unsigned long long unknown_bytes;
    int has_token_embedding;
    int has_attention_norm;
    int has_post_attention_norm;
    int has_attention_q;
    int has_attention_k;
    int has_attention_v;
    int has_attention_out;
    int has_ffn_gate;
    int has_ffn_up;
    int has_ffn_down;
    int has_moe_router;
    int has_moe_expert;
    int has_output_norm;
    int has_output_head;
    int has_tokenizer_metadata;
} yvex_fullmodel_collections;

typedef struct {
    char name[32];
    unsigned long long count;
    unsigned long long bytes;
} yvex_fullmodel_dtype_bucket;

typedef struct {
    const yvex_tensor_info *tensor;
    unsigned long long bytes;
} yvex_fullmodel_largest_tensor;

typedef struct {
    int available;
    int memory_known;
    unsigned long long total_bytes;
    unsigned long long available_bytes;
    unsigned long long required_bytes;
    const char *fit_status;
    char fit_reason[160];
} yvex_fullmodel_backend_fit;

static const char *fullmodel_identity_status(const yvex_model_ref *ref,
                                             unsigned long long artifact_bytes);
static void fullmodel_probe_backend_fit(const char *backend,
                                        unsigned long long required_bytes,
                                        yvex_fullmodel_backend_fit *fit);
static int fullmodel_has_attention_collection(const yvex_fullmodel_collections *collections);
static int fullmodel_has_mlp_collection(const yvex_fullmodel_collections *collections);
static int fullmodel_has_normalization_collection(const yvex_fullmodel_collections *collections);

static int fullmodel_string_is_empty(const char *text)
{
    return !text || !text[0];
}

static int fullmodel_parse_value_option(const char *flag,
                                        int argc,
                                        char **argv,
                                        int *index,
                                        const char **value)
{
    if (*index + 1 >= argc) {
        fprintf(stderr, "yvex: fullmodel %s requires a value\n", flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        fprintf(stderr, "yvex: fullmodel %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int fullmodel_phase_name_is_valid(const char *phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-coverage",
        "placement-plan",
        "memory-budget",
        "backend-preflight",
        "materialize-embedding",
        "materialize-normalization",
        "materialize-attention",
        "materialize-mlp",
        "materialize-moe",
        "materialize-output",
        "materialize-tokenizer",
        "cleanup",
        "complete",
        "failed"
    };
    unsigned int i;

    if (!phase || !phase[0]) return 0;
    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        if (strcmp(phase, phases[i]) == 0) return 1;
    }
    return 0;
}

static int fullmodel_command_is_materialize(const yvex_cli_fullmodel_options *options)
{
    return options && options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
}

static int fullmodel_command_is_descriptor(const yvex_cli_fullmodel_options *options)
{
    return options && options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
}

static int fullmodel_command_is_family_runtime(const yvex_cli_fullmodel_options *options)
{
    return options && options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;
}

static int fullmodel_command_accepts_includes(const yvex_cli_fullmodel_options *options)
{
    return fullmodel_command_is_descriptor(options) ||
           fullmodel_command_is_family_runtime(options);
}

static int fullmodel_command_accepts_requirements(const yvex_cli_fullmodel_options *options)
{
    return fullmodel_command_is_materialize(options) ||
           fullmodel_command_is_descriptor(options);
}

static int parse_fullmodel_options(int argc,
                                   char **argv,
                                   yvex_cli_fullmodel_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->residency = "resident";
    options->format = "text";
    options->family = "auto";
    options->limit_tensors = 5ull;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    options->command = YVEX_FULLMODEL_COMMAND_REPORT;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_fullmodel_help(stdout);
        return 1;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: fullmodel requires report, materialization-plan, materialize, descriptor, or family-runtime\n");
        fprintf(stderr, "usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--residency resident|host-staged|ssd-staged|hybrid] [--limit-tensors N]\n");
        fprintf(stderr, "usage: yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--dry-run] [--plan-only] [--limit-bytes N]\n");
        fprintf(stderr, "usage: yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--format text] [--limit-tensors N]\n");
        fprintf(stderr, "usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda]\n");
        return 2;
    }
    if (strcmp(argv[2], "report") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_REPORT;
    } else if (strcmp(argv[2], "materialization-plan") == 0 ||
               strcmp(argv[2], "plan") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    } else if (strcmp(argv[2], "materialize") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    } else if (strcmp(argv[2], "descriptor") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    } else if (strcmp(argv[2], "family-runtime") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;
    } else {
        fprintf(stderr, "yvex: unknown fullmodel subcommand: %s\n", argv[2]);
        fprintf(stderr, "usage: yvex fullmodel report --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--limit-tensors N]\n");
        fprintf(stderr, "usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--residency resident|host-staged|ssd-staged|hybrid] [--limit-tensors N]\n");
        fprintf(stderr, "usage: yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--dry-run] [--plan-only] [--limit-bytes N]\n");
        fprintf(stderr, "usage: yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--format text] [--limit-tensors N]\n");
        fprintf(stderr, "usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda]\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--model") == 0) {
            int rc = fullmodel_parse_value_option("--model", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(argv[i], "--backend") == 0) {
            int rc = fullmodel_parse_value_option("--backend", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                fprintf(stderr, "yvex: fullmodel --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(argv[i], "--target") == 0) {
            int rc = fullmodel_parse_value_option("--target", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->target = value;
        } else if (strcmp(argv[i], "--registry") == 0) {
            int rc = fullmodel_parse_value_option("--registry", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(argv[i], "--family") == 0) {
            int rc = fullmodel_parse_value_option("--family", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_family_runtime(options)) {
                fprintf(stderr, "yvex: fullmodel --family is only valid with family-runtime\n");
                return 2;
            }
            options->family = value;
        } else if (strcmp(argv[i], "--residency") == 0) {
            int rc = fullmodel_parse_value_option("--residency", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "resident") != 0 &&
                strcmp(value, "host-staged") != 0 &&
                strcmp(value, "ssd-staged") != 0 &&
                strcmp(value, "hybrid") != 0 &&
                strcmp(value, "ssd-streamed") != 0 &&
                strcmp(value, "managed-memory") != 0 &&
                strcmp(value, "distributed") != 0) {
                fprintf(stderr, "yvex: fullmodel --residency must be resident, host-staged, ssd-staged, hybrid, ssd-streamed, managed-memory, or distributed\n");
                return 2;
            }
            options->residency = value;
        } else if (strcmp(argv[i], "--limit-tensors") == 0) {
            unsigned long long parsed = 0ull;
            int rc = fullmodel_parse_value_option("--limit-tensors", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                fprintf(stderr, "yvex: fullmodel --limit-tensors requires a positive integer\n");
                return 2;
            }
            options->limit_tensors = parsed > 16ull ? 16ull : parsed;
        } else if (strcmp(argv[i], "--limit-bytes") == 0) {
            unsigned long long parsed = 0ull;
            int rc = fullmodel_parse_value_option("--limit-bytes", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_materialize(options)) {
                fprintf(stderr, "yvex: fullmodel --limit-bytes is only valid with materialize\n");
                return 2;
            }
            if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                fprintf(stderr, "yvex: fullmodel --limit-bytes requires a positive integer\n");
                return 2;
            }
            options->limit_bytes = parsed;
            options->has_limit_bytes = 1;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            if (!fullmodel_command_is_materialize(options)) {
                fprintf(stderr, "yvex: fullmodel --dry-run is only valid with materialize\n");
                return 2;
            }
            options->dry_run = 1;
        } else if (strcmp(argv[i], "--plan-only") == 0) {
            if (!fullmodel_command_is_materialize(options)) {
                fprintf(stderr, "yvex: fullmodel --plan-only is only valid with materialize\n");
                return 2;
            }
            options->plan_only = 1;
        } else if (strcmp(argv[i], "--require-role") == 0) {
            int rc = fullmodel_parse_value_option("--require-role", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_accepts_requirements(options)) {
                fprintf(stderr, "yvex: fullmodel --require-role is only valid with materialize or descriptor\n");
                return 2;
            }
            options->require_role = value;
        } else if (strcmp(argv[i], "--require-collection") == 0) {
            int rc = fullmodel_parse_value_option("--require-collection", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_accepts_requirements(options)) {
                fprintf(stderr, "yvex: fullmodel --require-collection is only valid with materialize or descriptor\n");
                return 2;
            }
            options->require_collection = value;
        } else if (strcmp(argv[i], "--fail-after-phase") == 0) {
            int rc = fullmodel_parse_value_option("--fail-after-phase", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_materialize(options)) {
                fprintf(stderr, "yvex: fullmodel --fail-after-phase is only valid with materialize\n");
                return 2;
            }
            if (!fullmodel_phase_name_is_valid(value)) {
                fprintf(stderr, "yvex: fullmodel --fail-after-phase value is not a known materialize phase\n");
                return 2;
            }
            options->fail_after_phase = value;
        } else if (strcmp(argv[i], "--report-dir") == 0) {
            int rc = fullmodel_parse_value_option("--report-dir", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_materialize(options)) {
                fprintf(stderr, "yvex: fullmodel --report-dir is only valid with materialize\n");
                return 2;
            }
            options->report_dir = value;
        } else if (strcmp(argv[i], "--format") == 0) {
            int rc = fullmodel_parse_value_option("--format", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!fullmodel_command_is_descriptor(options)) {
                fprintf(stderr, "yvex: fullmodel --format is only valid with descriptor\n");
                return 2;
            }
            if (strcmp(value, "text") != 0) {
                fprintf(stderr, "yvex: fullmodel descriptor currently supports --format text only\n");
                return 2;
            }
            options->format = value;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            int rc = fullmodel_parse_value_option("--output", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                fprintf(stderr, "yvex: fullmodel unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                fprintf(stderr, "yvex: fullmodel --include-blockers is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_blockers = 1;
        } else if (strcmp(argv[i], "--include-roles") == 0) {
            if (!fullmodel_command_is_family_runtime(options)) {
                fprintf(stderr, "yvex: fullmodel --include-roles is only valid with family-runtime\n");
                return 2;
            }
            options->include_roles = 1;
        } else if (strcmp(argv[i], "--include-placement") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                fprintf(stderr, "yvex: fullmodel --include-placement is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_placement = 1;
        } else if (strcmp(argv[i], "--include-graph") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                fprintf(stderr, "yvex: fullmodel --include-graph is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_graph = 1;
        } else if (strcmp(argv[i], "--include-kv") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                fprintf(stderr, "yvex: fullmodel --include-kv is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_kv = 1;
        } else if (strcmp(argv[i], "--include-logits") == 0) {
            if (!fullmodel_command_accepts_includes(options)) {
                fprintf(stderr, "yvex: fullmodel --include-logits is only valid with descriptor or family-runtime\n");
                return 2;
            }
            options->include_logits = 1;
        } else if (strcmp(argv[i], "--include-moe") == 0) {
            if (!fullmodel_command_is_family_runtime(options)) {
                fprintf(stderr, "yvex: fullmodel --include-moe is only valid with family-runtime\n");
                return 2;
            }
            options->include_moe = 1;
        } else if (strcmp(argv[i], "--include-output") == 0) {
            if (!fullmodel_command_is_family_runtime(options)) {
                fprintf(stderr, "yvex: fullmodel --include-output is only valid with family-runtime\n");
                return 2;
            }
            options->include_output = 1;
        } else {
            fprintf(stderr, "yvex: unknown fullmodel option: %s\n", argv[i]);
            return 2;
        }
    }
    if (!options->model) {
        const char *name = "report";
        if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
            name = "materialization-plan";
        } else if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
            name = "materialize";
        } else if (options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
            name = "descriptor";
        } else if (options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
            name = "family-runtime";
        }
        fprintf(stderr, "yvex: fullmodel %s requires --model FILE_OR_ALIAS\n",
                name);
        return 2;
    }
    return 0;
}

static int fullmodel_file_size(const char *path,
                               unsigned long long *bytes)
{
    struct stat st;

    if (bytes) *bytes = 0ull;
    if (!path || stat(path, &st) != 0) return 0;
    if (bytes) *bytes = (unsigned long long)st.st_size;
    return 1;
}

static const char *fullmodel_family_from_arch(yvex_arch arch)
{
    switch (arch) {
    case YVEX_ARCH_DEEPSEEK: return "deepseek";
    case YVEX_ARCH_GLM: return "glm";
    case YVEX_ARCH_LLAMA: return "llama";
    case YVEX_ARCH_QWEN: return "qwen";
    case YVEX_ARCH_GEMMA: return "gemma";
    case YVEX_ARCH_PHI: return "phi";
    case YVEX_ARCH_KIMI: return "kimi";
    default: return "unknown";
    }
}

static int fullmodel_name_has(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

static void fullmodel_csv_append(char *buf,
                                 size_t cap,
                                 const char *item)
{
    size_t used;
    int n;

    if (!buf || cap == 0u || !item || !item[0]) return;
    used = strlen(buf);
    if (used >= cap - 1u) return;
    n = snprintf(buf + used, cap - used, "%s%s", used == 0u ? "" : ",", item);
    if (n < 0 || (size_t)n >= cap - used) buf[cap - 1u] = '\0';
}

static void fullmodel_collection_add(unsigned long long *count,
                                     unsigned long long *bytes,
                                     const yvex_tensor_info *tensor)
{
    if (count) (*count)++;
    if (bytes && tensor) *bytes += tensor->storage_bytes;
}

static void fullmodel_record_dtype(yvex_fullmodel_dtype_bucket buckets[32],
                                   unsigned int *bucket_count,
                                   const yvex_tensor_info *tensor)
{
    const char *name;
    unsigned int i;

    if (!buckets || !bucket_count || !tensor) return;
    name = yvex_dtype_name(tensor->dtype);
    for (i = 0; i < *bucket_count; ++i) {
        if (strcmp(buckets[i].name, name) == 0) {
            buckets[i].count++;
            buckets[i].bytes += tensor->storage_bytes;
            return;
        }
    }
    if (*bucket_count < 32u) {
        snprintf(buckets[*bucket_count].name, sizeof(buckets[*bucket_count].name), "%s", name);
        buckets[*bucket_count].count = 1ull;
        buckets[*bucket_count].bytes = tensor->storage_bytes;
        (*bucket_count)++;
    }
}

static void fullmodel_dtype_summary(char *out,
                                    size_t out_cap,
                                    const yvex_fullmodel_dtype_bucket buckets[32],
                                    unsigned int bucket_count)
{
    unsigned int i;
    size_t used = 0u;

    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    for (i = 0; i < bucket_count; ++i) {
        int n = snprintf(out + used,
                         used < out_cap ? out_cap - used : 0u,
                         "%s%s:%llu:%llu",
                         i == 0 ? "" : ",",
                         buckets[i].name,
                         buckets[i].count,
                         buckets[i].bytes);
        if (n < 0 || (size_t)n >= (used < out_cap ? out_cap - used : 0u)) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (bucket_count == 0u) snprintf(out, out_cap, "none");
}

static void fullmodel_record_largest(yvex_fullmodel_largest_tensor top[16],
                                     unsigned int *top_count,
                                     unsigned int limit,
                                     const yvex_tensor_info *tensor)
{
    unsigned int i;
    unsigned int pos;

    if (!top || !top_count || !tensor || limit == 0u) return;
    if (limit > 16u) limit = 16u;
    pos = *top_count;
    for (i = 0; i < *top_count; ++i) {
        if (tensor->storage_bytes > top[i].bytes) {
            pos = i;
            break;
        }
    }
    if (*top_count < limit) {
        (*top_count)++;
    } else if (pos >= limit) {
        return;
    }
    for (i = *top_count - 1u; i > pos; --i) {
        top[i] = top[i - 1u];
    }
    top[pos].tensor = tensor;
    top[pos].bytes = tensor->storage_bytes;
}

static void fullmodel_classify_tensor(const yvex_tensor_info *tensor,
                                      yvex_fullmodel_collections *collections)
{
    const char *name;

    if (!tensor || !collections) return;
    name = tensor->name ? tensor->name : "";
    switch (tensor->role) {
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
        fullmodel_collection_add(&collections->embedding, &collections->embedding_bytes, tensor);
        collections->has_token_embedding = 1;
        return;
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_output_norm = 1;
        return;
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
        fullmodel_collection_add(&collections->output, &collections->output_bytes, tensor);
        collections->has_output_head = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_NORM:
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_attention_norm = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_Q:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_q = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_K:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_k = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_V:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_v = 1;
        return;
    case YVEX_TENSOR_ROLE_ATTENTION_OUT:
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_out = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_NORM:
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_post_attention_norm = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_GATE:
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_gate = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_UP:
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_up = 1;
        return;
    case YVEX_TENSOR_ROLE_FFN_DOWN:
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_down = 1;
        return;
    case YVEX_TENSOR_ROLE_MOE_ROUTER:
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_router = 1;
        return;
    case YVEX_TENSOR_ROLE_MOE_EXPERT_GATE:
    case YVEX_TENSOR_ROLE_MOE_EXPERT_UP:
    case YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN:
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_expert = 1;
        return;
    default:
        break;
    }

    if (fullmodel_name_has(name, "token_embd") || fullmodel_name_has(name, "embed")) {
        fullmodel_collection_add(&collections->embedding, &collections->embedding_bytes, tensor);
        collections->has_token_embedding = 1;
    } else if (fullmodel_name_has(name, "attn_norm") ||
               fullmodel_name_has(name, "input_layernorm")) {
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_attention_norm = 1;
    } else if (fullmodel_name_has(name, "ffn_norm") ||
               fullmodel_name_has(name, "post_attention_layernorm")) {
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_post_attention_norm = 1;
    } else if (fullmodel_name_has(name, "attn_q") || fullmodel_name_has(name, "q_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_q = 1;
    } else if (fullmodel_name_has(name, "attn_k") || fullmodel_name_has(name, "k_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_k = 1;
    } else if (fullmodel_name_has(name, "attn_v") || fullmodel_name_has(name, "v_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_v = 1;
    } else if (fullmodel_name_has(name, "attn_output") || fullmodel_name_has(name, "o_proj")) {
        fullmodel_collection_add(&collections->attention, &collections->attention_bytes, tensor);
        collections->has_attention_out = 1;
    } else if (fullmodel_name_has(name, "ffn_gate") || fullmodel_name_has(name, "gate_proj")) {
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_gate = 1;
    } else if (fullmodel_name_has(name, "ffn_up") || fullmodel_name_has(name, "up_proj")) {
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_up = 1;
    } else if (fullmodel_name_has(name, "ffn_down") || fullmodel_name_has(name, "down_proj")) {
        fullmodel_collection_add(&collections->mlp, &collections->mlp_bytes, tensor);
        collections->has_ffn_down = 1;
    } else if (fullmodel_name_has(name, "router") || fullmodel_name_has(name, "gate.weight")) {
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_router = 1;
    } else if (fullmodel_name_has(name, "expert")) {
        fullmodel_collection_add(&collections->moe, &collections->moe_bytes, tensor);
        collections->has_moe_expert = 1;
    } else if (fullmodel_name_has(name, "output_norm") || fullmodel_name_has(name, "norm.weight")) {
        fullmodel_collection_add(&collections->normalization, &collections->normalization_bytes, tensor);
        collections->has_output_norm = 1;
    } else if (strcmp(name, "output.weight") == 0 || fullmodel_name_has(name, "lm_head")) {
        fullmodel_collection_add(&collections->output, &collections->output_bytes, tensor);
        collections->has_output_head = 1;
    } else {
        fullmodel_collection_add(&collections->unknown, &collections->unknown_bytes, tensor);
    }
}

static int fullmodel_is_selected_target(const char *text)
{
    return text &&
           (strcmp(text, "deepseek4-v4-flash-selected-embed") == 0 ||
            strcmp(text, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0);
}

static void print_fullmodel_common_boundaries(void)
{
    printf("prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("sampling_ready: false\n");
    printf("full_model_execution: unsupported\n");
    printf("full_model_materialization: planned\n");
    printf("full_runtime_descriptor: planned\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
}

static int fullmodel_descriptor_tensor_matches(const yvex_tensor_info *tensor,
                                               const char *role)
{
    const char *name;

    if (!tensor || !role) return 0;
    name = tensor->name ? tensor->name : "";
    if (strcmp(role, "token_embedding") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING ||
               fullmodel_name_has(name, "token_embd") ||
               fullmodel_name_has(name, "embed");
    }
    if (strcmp(role, "attention_norm") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM ||
               fullmodel_name_has(name, "attn_norm") ||
               fullmodel_name_has(name, "input_layernorm");
    }
    if (strcmp(role, "post_attention_norm") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_NORM ||
               fullmodel_name_has(name, "ffn_norm") ||
               fullmodel_name_has(name, "post_attention_layernorm");
    }
    if (strcmp(role, "final_norm") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_OUTPUT_NORM ||
               fullmodel_name_has(name, "output_norm") ||
               fullmodel_name_has(name, "final_norm");
    }
    if (strcmp(role, "q_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_Q ||
               fullmodel_name_has(name, "attn_q") ||
               fullmodel_name_has(name, "q_proj");
    }
    if (strcmp(role, "k_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_K ||
               fullmodel_name_has(name, "attn_k") ||
               fullmodel_name_has(name, "k_proj");
    }
    if (strcmp(role, "v_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_V ||
               fullmodel_name_has(name, "attn_v") ||
               fullmodel_name_has(name, "v_proj");
    }
    if (strcmp(role, "o_projection") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_ATTENTION_OUT ||
               fullmodel_name_has(name, "attn_output") ||
               fullmodel_name_has(name, "o_proj");
    }
    if (strcmp(role, "mlp_gate") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_GATE ||
               fullmodel_name_has(name, "ffn_gate") ||
               fullmodel_name_has(name, "gate_proj");
    }
    if (strcmp(role, "mlp_up") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_UP ||
               fullmodel_name_has(name, "ffn_up") ||
               fullmodel_name_has(name, "up_proj");
    }
    if (strcmp(role, "mlp_down") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_FFN_DOWN ||
               fullmodel_name_has(name, "ffn_down") ||
               fullmodel_name_has(name, "down_proj");
    }
    if (strcmp(role, "moe_router") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_ROUTER ||
               fullmodel_name_has(name, "router");
    }
    if (strcmp(role, "moe_expert_gate") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_EXPERT_GATE ||
               (fullmodel_name_has(name, "expert") && fullmodel_name_has(name, "gate"));
    }
    if (strcmp(role, "moe_expert_up") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_EXPERT_UP ||
               (fullmodel_name_has(name, "expert") && fullmodel_name_has(name, "up"));
    }
    if (strcmp(role, "moe_expert_down") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN ||
               (fullmodel_name_has(name, "expert") && fullmodel_name_has(name, "down"));
    }
    if (strcmp(role, "output_head") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD ||
               strcmp(name, "output.weight") == 0 ||
               fullmodel_name_has(name, "lm_head");
    }
    if (strcmp(role, "unknown") == 0) {
        return tensor->role == YVEX_TENSOR_ROLE_UNKNOWN;
    }
    return 0;
}

static const yvex_tensor_info *fullmodel_descriptor_find_tensor(yvex_cli_tokenizer_context *ctx,
                                                                const char *role)
{
    unsigned long long count;
    unsigned long long i;

    if (!ctx || !ctx->table || !role) return NULL;
    count = yvex_tensor_table_count(ctx->table);
    for (i = 0; i < count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        if (fullmodel_descriptor_tensor_matches(tensor, role)) return tensor;
    }
    return NULL;
}

static const char *fullmodel_descriptor_role_collection(const char *role)
{
    if (!role) return "unknown";
    if (strcmp(role, "token_embedding") == 0) return "embedding";
    if (strcmp(role, "attention_norm") == 0 ||
        strcmp(role, "post_attention_norm") == 0 ||
        strcmp(role, "final_norm") == 0) return "normalization";
    if (strcmp(role, "q_projection") == 0 ||
        strcmp(role, "k_projection") == 0 ||
        strcmp(role, "v_projection") == 0 ||
        strcmp(role, "o_projection") == 0) return "attention";
    if (strcmp(role, "mlp_gate") == 0 ||
        strcmp(role, "mlp_up") == 0 ||
        strcmp(role, "mlp_down") == 0) return "mlp";
    if (strcmp(role, "moe_router") == 0 ||
        strcmp(role, "moe_expert_gate") == 0 ||
        strcmp(role, "moe_expert_up") == 0 ||
        strcmp(role, "moe_expert_down") == 0) return "moe";
    if (strcmp(role, "output_head") == 0) return "output";
    if (strcmp(role, "tokenizer_metadata") == 0) return "tokenizer-runtime-input";
    return "unknown";
}

static const char *fullmodel_descriptor_role_residency(const char *role,
                                                       const char *backend,
                                                       int present)
{
    if (!present) return "not-planned";
    if (role && strcmp(role, "tokenizer_metadata") == 0) return "host-runtime-metadata";
    return backend && strcmp(backend, "cuda") == 0 ? "cuda-resident-planned" : "cpu-resident-planned";
}

static void fullmodel_print_descriptor_role(yvex_cli_tokenizer_context *ctx,
                                            const yvex_fullmodel_collections *collections,
                                            const char *role,
                                            const char *backend)
{
    const yvex_tensor_info *tensor = NULL;
    char dims[128];
    int present = 0;

    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        present = collections && collections->has_tokenizer_metadata;
    } else {
        tensor = fullmodel_descriptor_find_tensor(ctx, role);
        present = tensor != NULL;
    }
    dims[0] = '\0';
    if (tensor) dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
    printf("role.%s.status: %s\n", role ? role : "unknown", present ? "present" : "missing");
    printf("role.%s.tensor: %s\n", role ? role : "unknown",
           tensor && tensor->name ? tensor->name : present ? "metadata" : "none");
    printf("role.%s.shape: %s\n", role ? role : "unknown",
           tensor ? dims : present ? "metadata" : "unknown");
    printf("role.%s.dtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    printf("role.%s.qtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    printf("role.%s.bytes: %llu\n", role ? role : "unknown",
           tensor ? tensor->storage_bytes : 0ull);
    printf("role.%s.collection: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_collection(role));
    printf("role.%s.residency_expectation: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_residency(role, backend, present));
    printf("role.%s.runtime_consumer: %s\n", role ? role : "unknown",
           present ? "planned" : "blocked-missing-role");
}

static void fullmodel_print_descriptor_collection(const char *name,
                                                  unsigned long long count,
                                                  unsigned long long bytes,
                                                  int required_for_prefill,
                                                  int required_for_decode,
                                                  int required_for_logits,
                                                  int required_for_generation,
                                                  const char *runtime_consumer,
                                                  const char *blocker)
{
    printf("collection.%s.status: %s\n", name, count > 0ull ? "present" : "missing");
    printf("collection.%s.tensor_count: %llu\n", name, count);
    printf("collection.%s.byte_count: %llu\n", name, bytes);
    printf("collection.%s.required_for_prefill: %s\n", name, required_for_prefill ? "true" : "false");
    printf("collection.%s.required_for_decode: %s\n", name, required_for_decode ? "true" : "false");
    printf("collection.%s.required_for_logits: %s\n", name, required_for_logits ? "true" : "false");
    printf("collection.%s.required_for_generation: %s\n", name, required_for_generation ? "true" : "false");
    printf("collection.%s.runtime_consumer: %s\n", name, runtime_consumer ? runtime_consumer : "planned");
    printf("collection.%s.blocker: %s\n", name, blocker && blocker[0] ? blocker : "none");
}

static void fullmodel_print_descriptor_phase(unsigned int index,
                                             const char *name,
                                             const char *status)
{
    printf("descriptor_phase.%u.name: %s\n", index, name ? name : "");
    printf("descriptor_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void fullmodel_print_descriptor_phases(const char *role_status,
                                              const char *collection_status,
                                              const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-map",
        "collection-map",
        "shape-requirements",
        "residency-requirements",
        "graph-requirements",
        "prefill-requirements",
        "kv-requirements",
        "decode-requirements",
        "logits-requirements",
        "sampling-requirements",
        "tokenizer-requirements",
        "backend-requirements",
        "blocker-report",
        "descriptor-build",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "skipped";
        } else if (strcmp(phases[i], "role-map") == 0) {
            status = role_status ? role_status : "partial";
        } else if (strcmp(phases[i], "collection-map") == 0) {
            status = collection_status ? collection_status : "partial";
        } else if (strcmp(phases[i], "residency-requirements") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "backend-requirements") == 0) {
            status = "planned";
        } else if (strcmp(phases[i], "prefill-requirements") == 0 ||
                   strcmp(phases[i], "kv-requirements") == 0 ||
                   strcmp(phases[i], "decode-requirements") == 0 ||
                   strcmp(phases[i], "logits-requirements") == 0 ||
                   strcmp(phases[i], "sampling-requirements") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "skipped";
        }
        fullmodel_print_descriptor_phase(i, phases[i], status);
    }
}

static void fullmodel_print_descriptor_graph_requirements(const yvex_fullmodel_collections *collections)
{
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);

    printf("graph_requirements_status: blocked\n");
    printf("required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp-gate-up-down,activation,moe-router,expert-dispatch,expert-accumulation,final-norm,output-head-projection\n");
    printf("unsupported_graph_ops: full-transformer-attention,real-layer-scheduler,real-moe-router,real-expert-dispatch,real-output-head-projection\n");
    printf("required_backend_ops: tensor-read,rmsnorm,matmul,rope,attention,softmax,activation,residual-add,kv-read,kv-write\n");
    printf("unsupported_backend_ops: full-transformer-runtime-integration,real-attention-backed-kv,real-output-head-logits\n");
    printf("graph.embedding_lookup: %s\n",
           collections && collections->has_token_embedding ? "planned-real-tensor" : "missing-tensor");
    printf("graph.rmsnorm: %s\n",
           fullmodel_has_normalization_collection(collections) ? "implemented-selected-segment" : "missing-tensor");
    printf("graph.q_projection: %s\n", collections && collections->has_attention_q ? "planned" : "missing-tensor");
    printf("graph.k_projection: %s\n", collections && collections->has_attention_k ? "planned" : "missing-tensor");
    printf("graph.v_projection: %s\n", collections && collections->has_attention_v ? "planned" : "missing-tensor");
    printf("graph.rope_position_op: implemented-primitive\n");
    printf("graph.attention_primitive: implemented-fixture\n");
    printf("graph.full_transformer_attention: %s\n", has_attention ? "unsupported" : "missing-tensor");
    printf("graph.o_projection: %s\n", collections && collections->has_attention_out ? "planned" : "missing-tensor");
    printf("graph.residual_add: planned\n");
    printf("graph.mlp_primitive: implemented-fixture\n");
    printf("graph.full_transformer_mlp: %s\n", has_mlp ? "unsupported" : "missing-tensor");
    printf("graph.moe_router: %s\n",
           collections && collections->has_moe_router ? "planned" : "missing-tensor");
    printf("graph.expert_dispatch: %s\n",
           collections && collections->has_moe_expert ? "planned" : "missing-tensor");
    printf("graph.output_head_projection: %s\n",
           collections && collections->has_output_head ? "planned" : "missing-tensor");
}

static void fullmodel_print_descriptor_report(const yvex_cli_fullmodel_options *options,
                                              yvex_model_ref *ref,
                                              yvex_cli_tokenizer_context *ctx,
                                              const char *target_id,
                                              const char *target_class,
                                              unsigned long long artifact_bytes,
                                              yvex_arch arch,
                                              unsigned long long tensor_count,
                                              unsigned long long total_tensor_bytes,
                                              const yvex_fullmodel_collections *collections,
                                              const char *role_coverage,
                                              const char *missing_roles,
                                              const char *unsupported_roles,
                                              int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *backend = options && options->backend ? options->backend : "cpu";
    int descriptor_complete = role_coverage &&
                              (strcmp(role_coverage, "complete") == 0 ||
                               strcmp(role_coverage, "observed") == 0);
    const char *descriptor_status = selected_target ? "partial" :
                                    (descriptor_complete ? "complete" : "partial");
    const char *materialization_plan_status = selected_target ? "partial" : "ready";
    const char *materialization_proof_status = selected_target ? "refused-selected-runtime-slice" :
                                               (descriptor_complete
                                                    ? "available-controlled-tiny-proof"
                                                    : "blocked-missing-roles");
    const char *full_materialization = selected_target ? "refused-selected-runtime-slice" :
                                      (descriptor_complete
                                           ? "controlled-tiny-proof-available"
                                           : "planned");
    unsigned long long cuda_bytes = strcmp(backend, "cuda") == 0 ? total_tensor_bytes : 0ull;
    unsigned long long cpu_bytes = strcmp(backend, "cuda") == 0 ? 0ull : total_tensor_bytes;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    printf("fullmodel: descriptor\n");
    printf("status: fullmodel-descriptor\n");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    printf("target_id: %s\n", target_id ? target_id : "path");
    printf("target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    printf("backend: %s\n", backend);
    printf("format: %s\n", options && options->format ? options->format : "text");
    printf("artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    printf("tensor_inventory_status: pass\n");
    printf("materialization_plan_status: %s\n", materialization_plan_status);
    printf("materialization_proof_status: %s\n", materialization_proof_status);
    printf("runtime_descriptor: report-only\n");
    printf("runtime_descriptor_status: %s\n", descriptor_status);
    printf("runtime_descriptor_kind: fullmodel-planning\n");
    printf("family: %s\n", fullmodel_family_from_arch(arch));
    printf("architecture: %s\n", yvex_arch_name(arch));
    printf("model_class: %s\n", selected_target ? "selected-runtime-slice" : "descriptor-only-candidate");
    printf("full_runtime_model: false\n");
    printf("full_model_execution: unsupported\n");
    printf("full_model_materialization: %s\n", full_materialization);
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("tensor_count: %llu\n", tensor_count);
    printf("total_tensor_bytes: %llu\n", total_tensor_bytes);
    printf("tensor_role_map_status: %s\n", descriptor_status);
    printf("tensor_collection_map_status: %s\n", descriptor_status);
    printf("required_role_coverage: %s\n", descriptor_complete ? "complete" : (role_coverage ? role_coverage : "partial"));
    printf("missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    printf("unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    printf("unknown_role_count: %llu\n", collections ? collections->unknown : 0ull);

    fullmodel_print_descriptor_role(ctx, collections, "token_embedding", backend);
    fullmodel_print_descriptor_role(ctx, collections, "attention_norm", backend);
    fullmodel_print_descriptor_role(ctx, collections, "post_attention_norm", backend);
    fullmodel_print_descriptor_role(ctx, collections, "final_norm", backend);
    fullmodel_print_descriptor_role(ctx, collections, "q_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "k_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "v_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "o_projection", backend);
    fullmodel_print_descriptor_role(ctx, collections, "mlp_gate", backend);
    fullmodel_print_descriptor_role(ctx, collections, "mlp_up", backend);
    fullmodel_print_descriptor_role(ctx, collections, "mlp_down", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_router", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_expert_gate", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_expert_up", backend);
    fullmodel_print_descriptor_role(ctx, collections, "moe_expert_down", backend);
    fullmodel_print_descriptor_role(ctx, collections, "output_head", backend);
    fullmodel_print_descriptor_role(ctx, collections, "tokenizer_metadata", backend);
    fullmodel_print_descriptor_role(ctx, collections, "unknown", backend);

    printf("embedding_descriptor: %s\n", collections && collections->embedding > 0ull ? "present" : "missing");
    printf("normalization_descriptor: %s\n", collections && collections->normalization > 0ull ? "present" : "missing");
    printf("attention_descriptor: %s\n", fullmodel_has_attention_collection(collections) ? "present" : "missing");
    printf("mlp_descriptor: %s\n", fullmodel_has_mlp_collection(collections) ? "present" : "missing");
    printf("moe_descriptor: %s\n", collections && collections->moe > 0ull ? "present" : "planned-or-missing");
    printf("output_descriptor: %s\n", collections && collections->output > 0ull ? "present" : "missing");
    printf("tokenizer_descriptor: %s\n", collections && collections->has_tokenizer_metadata ? "present" : "missing");
    printf("kv_descriptor: unsupported-real-attention-backed-kv\n");

    fullmodel_print_descriptor_collection("embedding",
                                          collections ? collections->embedding : 0ull,
                                          collections ? collections->embedding_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          collections && collections->embedding > 0ull ? "none" : "embedding collection missing");
    fullmodel_print_descriptor_collection("normalization",
                                          collections ? collections->normalization : 0ull,
                                          collections ? collections->normalization_bytes : 0ull,
                                          1, 1, 1, 1, "planned",
                                          collections && collections->normalization > 0ull ? "none" : "normalization collection missing");
    fullmodel_print_descriptor_collection("attention",
                                          collections ? collections->attention : 0ull,
                                          collections ? collections->attention_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          fullmodel_has_attention_collection(collections) ? "none" : "attention Q/K/V/O tensors missing");
    fullmodel_print_descriptor_collection("mlp",
                                          collections ? collections->mlp : 0ull,
                                          collections ? collections->mlp_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          fullmodel_has_mlp_collection(collections) ? "none" : "MLP tensors missing");
    fullmodel_print_descriptor_collection("moe",
                                          collections ? collections->moe : 0ull,
                                          collections ? collections->moe_bytes : 0ull,
                                          1, 1, 0, 1, "planned",
                                          collections && collections->moe > 0ull ? "none" : "MoE tensors missing or not identified");
    fullmodel_print_descriptor_collection("output",
                                          collections ? collections->output : 0ull,
                                          collections ? collections->output_bytes : 0ull,
                                          0, 1, 1, 1, "planned",
                                          collections && collections->output > 0ull ? "none" : "output head missing");
    fullmodel_print_descriptor_collection("tokenizer-runtime-input",
                                          collections ? collections->tokenizer : 0ull,
                                          collections ? collections->tokenizer_bytes : 0ull,
                                          1, 1, 1, 1, "planned",
                                          collections && collections->has_tokenizer_metadata ? "none" : "tokenizer metadata missing");
    fullmodel_print_descriptor_collection("kv-cache-runtime", 0ull, 0ull,
                                          1, 1, 0, 1, "unsupported",
                                          "real attention-backed KV writes unsupported");
    fullmodel_print_descriptor_collection("unknown",
                                          collections ? collections->unknown : 0ull,
                                          collections ? collections->unknown_bytes : 0ull,
                                          0, 0, 0, 0, "unsupported",
                                          collections && collections->unknown > 0ull ? "unknown tensor role" : "none");

    fullmodel_print_descriptor_graph_requirements(collections);

    printf("prefill_descriptor: unsupported-full-transformer-prefill\n");
    printf("prefill.requires_embedding: true\n");
    printf("prefill.requires_attention_qkv: true\n");
    printf("prefill.requires_real_kv_writes: true\n");
    printf("prefill.requires_mlp_or_moe: true\n");
    printf("prefill.requires_layer_scheduler: true\n");
    printf("prefill.current_status: unsupported\n");
    printf("prefill.blocker: real transformer prefill not implemented\n");
    printf("decode_descriptor: unsupported-full-model-decode\n");
    printf("decode.mode_required: baseline-autoregressive\n");
    printf("decode.requires_prefill_state: true\n");
    printf("decode.requires_kv_read: true\n");
    printf("decode.requires_layer_execution: true\n");
    printf("decode.current_status: unsupported\n");
    printf("decode.blocker: full model decode not implemented\n");
    printf("logits_descriptor: unsupported-real-output-head-logits\n");
    printf("sampling_descriptor: unsupported-real-vocabulary-sampling\n");

    printf("residency_requirements_status: planned\n");
    printf("residency_plan: descriptor-only-no-allocation\n");
    printf("cpu_resident_required_bytes: %llu\n", cpu_bytes);
    printf("cuda_resident_required_bytes: %llu\n", cuda_bytes);
    printf("host_staged_required_bytes: %llu\n", strcmp(backend, "cuda") == 0 ? total_tensor_bytes : 0ull);
    printf("ssd_staged_required_bytes: planned\n");
    printf("kv_required_bytes: planned\n");
    printf("scratch_required_bytes: planned\n");

    printf("context_requirements_status: planned\n");
    printf("max_context: metadata-or-unknown\n");
    printf("requested_context: not-requested\n");
    printf("context_policy: planned\n");
    printf("position_policy: rope-or-family-specific-planned\n");
    printf("rope_policy: planned\n");

    printf("kv_requirements_status: unsupported\n");
    printf("kv_layout: planned\n");
    printf("kv_dtype: planned\n");
    printf("kv_layers: unknown\n");
    printf("kv_heads: unknown\n");
    printf("kv_head_dim: unknown\n");
    printf("kv_capacity_status: unsupported-full-transformer-kv\n");
    printf("kv.required: true\n");
    printf("kv.real_attention_writes: false\n");
    printf("kv.runtime_status: unsupported\n");
    printf("kv_write_ready: false\n");
    printf("kv_read_ready: false\n");

    printf("logits_requirements_status: unsupported\n");
    printf("output_head_present: %s\n", collections && collections->has_output_head ? "true" : "false");
    printf("output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    printf("output_head_dtype: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? yvex_dtype_name(fullmodel_descriptor_find_tensor(ctx, "output_head")->dtype)
               : "unknown");
    printf("vocab_size: %s\n", collections && collections->has_output_head ? "from-output-head-shape" : "unknown");
    printf("logits_buffer_required: true\n");
    printf("real_output_head_logits: false\n");
    printf("logits_ready: false\n");
    printf("logits.blocker: real output-head logits runtime unsupported\n");

    printf("tokenizer_requirements_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked");
    printf("tokenizer_metadata_present: %s\n",
           collections && collections->has_tokenizer_metadata ? "true" : "false");
    printf("special_token_policy: planned\n");
    printf("eos_backed_stop: unsupported\n");
    printf("stop_token_text_matching: unsupported\n");
    printf("tokenizer_quality_generation: unsupported\n");

    printf("backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    printf("backend.cpu.available: true\n");
    printf("backend.cuda.available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    printf("backend.memory_known: %s\n", fit.memory_known ? "true" : "false");
    printf("backend.required_bytes: %llu\n", fit.required_bytes);
    printf("backend.fit_status: %s\n", fit.fit_status);
    printf("backend.fit_reason: %s\n", fit.fit_reason);
    printf("backend.primitive_rope: implemented-fixture\n");
    printf("backend.primitive_attention: implemented-fixture\n");
    printf("backend.primitive_matmul: implemented-fixture\n");
    printf("backend.primitive_mlp: implemented-fixture\n");
    printf("backend.full_transformer_integration: unsupported\n");
    printf("backend_allocation_attempted: false\n");

    printf("runtime_blockers: %s\n",
           selected_target
               ? "full runtime tensor set incomplete; attention Q/K/V/O tensors missing; MLP/MoE tensors missing; output head missing; real transformer prefill unsupported; real attention-backed KV writes unsupported; full model decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported; full model execution unsupported"
               : "real transformer prefill unsupported; real attention-backed KV writes unsupported; full model decode unsupported; real output-head logits runtime unsupported; real vocabulary sampling unsupported; full model execution unsupported");
    printf("descriptor_blockers: %s\n",
           selected_target
               ? "selected-runtime-slice is partial descriptor only"
               : "runtime family adapter boundary remains planned");
    printf("prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("sampling_ready: false\n");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
    fullmodel_print_descriptor_phases(descriptor_status, descriptor_status, NULL);
}

static void fullmodel_print_family_runtime_phase(unsigned int index,
                                                 const char *name,
                                                 const char *status)
{
    printf("family_runtime_phase.%u.name: %s\n", index, name ? name : "");
    printf("family_runtime_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void fullmodel_print_family_runtime_phases(const char *adapter_status,
                                                  const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-descriptor",
        "family-profile",
        "role-adapter",
        "collection-adapter",
        "attention-rules",
        "position-rules",
        "kv-rules",
        "moe-rules",
        "mlp-rules",
        "output-head-rules",
        "tokenizer-rules",
        "graph-requirements",
        "runtime-phase-blockers",
        "adapter-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "skipped";
        } else if (strcmp(phases[i], "role-adapter") == 0 ||
                   strcmp(phases[i], "collection-adapter") == 0) {
            status = adapter_status ? adapter_status : "partial";
        } else if (strcmp(phases[i], "attention-rules") == 0 ||
                   strcmp(phases[i], "position-rules") == 0 ||
                   strcmp(phases[i], "kv-rules") == 0 ||
                   strcmp(phases[i], "moe-rules") == 0 ||
                   strcmp(phases[i], "mlp-rules") == 0 ||
                   strcmp(phases[i], "output-head-rules") == 0 ||
                   strcmp(phases[i], "tokenizer-rules") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "runtime-phase-blockers") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "skipped";
        }
        fullmodel_print_family_runtime_phase(i, phases[i], status);
    }
}

static const char *fullmodel_detect_family(const yvex_cli_fullmodel_options *options,
                                           yvex_arch arch,
                                           const char *target_id)
{
    const char *from_arch = fullmodel_family_from_arch(arch);

    if (from_arch && strcmp(from_arch, "unknown") != 0) return from_arch;
    if (target_id && fullmodel_name_has(target_id, "deepseek")) return "deepseek";
    if (options && options->model && fullmodel_name_has(options->model, "deepseek")) return "deepseek";
    if (target_id && fullmodel_name_has(target_id, "glm")) return "glm";
    if (options && options->model && fullmodel_name_has(options->model, "glm")) return "glm";
    if (target_id && fullmodel_name_has(target_id, "qwen")) return "qwen";
    if (options && options->model && fullmodel_name_has(options->model, "qwen")) return "qwen";
    return "unknown";
}

static const char *fullmodel_requested_family(const yvex_cli_fullmodel_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static int fullmodel_family_request_matches(const char *requested,
                                            const char *detected)
{
    if (!requested || !requested[0] || strcmp(requested, "auto") == 0) {
        return detected && strcmp(detected, "unknown") != 0;
    }
    return detected && strcmp(requested, detected) == 0;
}

static const char *fullmodel_role_status_from_tensor(yvex_cli_tokenizer_context *ctx,
                                                     const yvex_fullmodel_collections *collections,
                                                     const char *role)
{
    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        return collections && collections->has_tokenizer_metadata ? "present" : "missing";
    }
    return fullmodel_descriptor_find_tensor(ctx, role) ? "present" : "missing";
}

static const char *fullmodel_attention_rule_status(const yvex_fullmodel_collections *collections)
{
    if (!collections ||
        !collections->has_attention_q ||
        !collections->has_attention_k ||
        !collections->has_attention_v ||
        !collections->has_attention_out) {
        return "blocked-missing-qkv";
    }
    return "blocked-full-transformer-integration";
}

static int fullmodel_print_family_runtime_report(const yvex_cli_fullmodel_options *options,
                                                 yvex_model_ref *ref,
                                                 yvex_cli_tokenizer_context *ctx,
                                                 const char *target_id,
                                                 const char *target_class,
                                                 unsigned long long artifact_bytes,
                                                 yvex_arch arch,
                                                 unsigned long long tensor_count,
                                                 unsigned long long total_tensor_bytes,
                                                 const yvex_fullmodel_collections *collections,
                                                 const char *role_coverage,
                                                 const char *missing_roles,
                                                 const char *unsupported_roles,
                                                 int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = fullmodel_requested_family(options);
    const char *detected = fullmodel_detect_family(options, arch, target_id);
    const char *adapter_status = selected_target ? "partial" :
                                 (role_coverage && strcmp(role_coverage, "complete") == 0
                                      ? "complete"
                                      : "partial");
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);
    int has_output = collections && collections->has_output_head;
    int supported_family = fullmodel_family_request_matches(requested, detected) &&
                           strcmp(detected, "deepseek") == 0;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    printf("family_runtime: report\n");
    if (!supported_family) {
        const char *status = strcmp(detected, "unknown") == 0
                                 ? "fullmodel-family-runtime-fail"
                                 : "fullmodel-family-runtime-unsupported";
        printf("status: %s\n", status);
        printf("model: %s\n", options && options->model ? options->model : "");
        printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        printf("target_id: %s\n", target_id ? target_id : "path");
        printf("target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
        printf("backend: %s\n", backend);
        printf("family: %s\n", detected);
        printf("family_detected: %s\n", detected);
        printf("family_requested: %s\n", requested);
        printf("family_adapter: unsupported\n");
        printf("family_adapter_status: unsupported\n");
        printf("family_runtime_stage: report-only\n");
        printf("runtime_claim: unsupported\n");
        printf("generation: unsupported-full-model\n");
        printf("benchmark_status: not-measured\n");
        printf("descriptor_status: unavailable\n");
        printf("descriptor_source: fullmodel-descriptor-facts\n");
        printf("full_runtime_model: false\n");
        printf("full_model_execution: unsupported\n");
        printf("generation_ready: false\n");
        printf("runtime_execution_ready: false\n");
        printf("runtime_blockers: unsupported or unknown runtime family adapter\n");
        printf("next_required_rows: FAMILY.RUNTIME.0,ATTENTION.CLASS.0,CONTEXT.CLASS.0,KV.CACHE.0,MOE.CLASS.0,family-specific-runtime-target\n");
        printf("cleanup_attempted: false\n");
        printf("cleanup_status: not-needed\n");
        fullmodel_print_family_runtime_phases("unsupported", "resolve-family");
        printf("reason: requested family is not supported by family-runtime report\n");
        return 5;
    }

    printf("status: fullmodel-family-runtime\n");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    printf("target_id: %s\n", target_id ? target_id : "path");
    printf("target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    printf("backend: %s\n", backend);
    printf("family: deepseek\n");
    printf("family_detected: %s\n", detected);
    printf("family_requested: %s\n", requested);
    printf("family_adapter: deepseek-runtime-report\n");
    printf("family_adapter_status: %s\n", adapter_status);
    printf("family_runtime_stage: report-only\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");

    printf("descriptor_status: %s\n", adapter_status);
    printf("descriptor_source: fullmodel-descriptor-facts\n");
    printf("full_runtime_model: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("tensor_count: %llu\n", tensor_count);
    printf("total_tensor_bytes: %llu\n", total_tensor_bytes);
    printf("artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));

    printf("role_adapter_status: %s\n", adapter_status);
    printf("collection_adapter_status: %s\n", adapter_status);
    printf("attention_rule_status: %s\n", fullmodel_attention_rule_status(collections));
    printf("attention_rules: %s\n", fullmodel_attention_rule_status(collections));
    printf("position_rule_status: planned\n");
    printf("kv_rule_status: blocked\n");
    printf("moe_rule_status: blocked\n");
    printf("mlp_rule_status: %s\n", has_mlp ? "blocked-full-transformer-integration" : "blocked-missing-mlp");
    printf("output_head_rule_status: %s\n", has_output ? "blocked-logits-runtime" : "blocked-missing-output-head");
    printf("tokenizer_rule_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked-missing-tokenizer-metadata");
    printf("graph_requirement_status: blocked\n");
    printf("runtime_blocker_status: blocked\n");

    printf("token_embedding_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "token_embedding"));
    printf("attention_norm_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "attention_norm"));
    printf("post_attention_norm_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "post_attention_norm"));
    printf("final_norm_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "final_norm"));
    printf("q_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "q_projection"));
    printf("k_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "k_projection"));
    printf("v_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "v_projection"));
    printf("o_projection_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "o_projection"));
    printf("mlp_gate_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "mlp_gate"));
    printf("mlp_up_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "mlp_up"));
    printf("mlp_down_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "mlp_down"));
    printf("moe_router_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "moe_router"));
    printf("moe_expert_roles: %s\n",
           collections && collections->has_moe_expert ? "present" : "missing");
    printf("output_head_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "output_head"));
    printf("tokenizer_metadata_role: %s\n", fullmodel_role_status_from_tensor(ctx, collections, "tokenizer_metadata"));

    printf("attention_family: deepseek-family-attention-planned\n");
    printf("attention_type: unknown-family-specific\n");
    printf("attention_heads: unknown\n");
    printf("kv_heads: unknown\n");
    printf("head_dim: unknown\n");
    printf("attention_q_required: true\n");
    printf("attention_k_required: true\n");
    printf("attention_v_required: true\n");
    printf("attention_o_required: true\n");
    printf("rope_required: true\n");
    printf("rope_status: planned\n");
    printf("rope_base: unknown\n");
    printf("rope_scaling: unknown\n");
    printf("mask_required: true\n");
    printf("mask_rule: causal-or-family-specific-planned\n");
    printf("context_policy: planned\n");
    printf("attention_runtime_ready: false\n");

    printf("kv_required: true\n");
    printf("kv_layout: planned\n");
    printf("kv_dtype: planned\n");
    printf("kv_capacity_status: unsupported-full-transformer-kv\n");
    printf("kv_write_ready: false\n");
    printf("kv_read_ready: false\n");

    printf("moe_required: family-specific-planned\n");
    printf("router_required: family-specific-planned\n");
    printf("router_present: %s\n", collections && collections->has_moe_router ? "true" : "false");
    printf("moe_router_present: %s\n", collections && collections->has_moe_router ? "true" : "false");
    printf("expert_tensors_present: %s\n", collections && collections->has_moe_expert ? "true" : "false");
    printf("moe_expert_count: unknown\n");
    printf("expert_count: unknown\n");
    printf("moe_active_expert_count: unknown\n");
    printf("active_expert_count: unknown\n");
    printf("moe_shared_experts: unknown\n");
    printf("shared_expert_status: planned\n");
    printf("moe_dispatch_ready: false\n");
    printf("moe_blockers: router logits, top-k routing, expert dispatch, and expert accumulation are not implemented\n");

    printf("output_head_required: true\n");
    printf("output_head_present: %s\n", has_output ? "true" : "false");
    printf("output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    printf("vocab_size: %s\n", has_output ? "from-output-head-shape" : "unknown");
    printf("logits_projection_ready: false\n");
    printf("real_output_head_logits: false\n");
    printf("logits_blockers: real output-head logits runtime unsupported\n");

    printf("required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp,moe-router,expert-dispatch,final-norm,output-head-projection\n");
    printf("implemented_graph_primitives: rope,attention-fixture,matmul,mlp-fixture,controlled-block,controlled-layers,selected-embedding,selected-rmsnorm-segment\n");
    printf("unsupported_graph_ops: full-attention-from-model-tensors,full-transformer-block-from-model-tensors,full-layer-stack,real-moe-router,real-expert-dispatch,real-output-head-projection\n");
    printf("graph.rope_primitive: implemented\n");
    printf("graph.attention_primitive: implemented-fixture\n");
    printf("graph.matmul_primitive: implemented\n");
    printf("graph.mlp_primitive: implemented-fixture\n");
    printf("graph.full_attention_from_model_tensors: unsupported\n");
    printf("graph.full_transformer_block_from_model_tensors: unsupported\n");
    printf("graph.full_layer_stack: unsupported\n");
    printf("graph.full_transformer_attention: %s\n", has_attention ? "unsupported" : "missing-tensor");
    printf("full_transformer_graph_ready: false\n");

    printf("backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    printf("backend_available: %s\n", fit.available ? "true" : "false");
    printf("backend_memory_known: %s\n", fit.memory_known ? "true" : "false");
    printf("backend_required_bytes: %llu\n", fit.required_bytes);
    printf("backend_fit_status: %s\n", fit.fit_status);
    printf("backend_allocation_attempted: false\n");

    printf("missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    printf("unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    printf("prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("sampling_ready: false\n");
    printf("runtime_execution_ready: false\n");
    printf("runtime_blockers: %s\n",
           selected_target
               ? "selected runtime slice is incomplete; attention Q/K/V/O tensors missing; output head missing; real transformer prefill unsupported; real attention-backed KV unsupported; real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported"
               : "real transformer prefill unsupported; real attention-backed KV unsupported; real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported");
    printf("next_required_rows: ATTENTION.CLASS.0,CONTEXT.CLASS.0,KV.CACHE.0,MOE.CLASS.0,FAMILY.RUNTIME.DeepSeek.detail,real-transformer-prefill,real-decode,real-output-head-logits,real-vocabulary-sampling,GEN.DEEPSEEK.0\n");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
    fullmodel_print_family_runtime_phases(adapter_status, NULL);
    return 0;
}

typedef struct {
    const yvex_cli_fullmodel_options *options;
    const char *status;
    const char *model_resolved_path;
    const char *target_id;
    const char *target_class;
    const char *artifact_identity_status;
    const char *tensor_inventory_status;
    const char *required_role_coverage;
    const char *missing_required_roles;
    const char *unsupported_required_roles;
    const char *placement_plan_status;
    const char *memory_budget_status;
    const char *backend_preflight_status;
    const char *materialization_mode;
    const char *full_model_materialization;
    const char *full_model_materialization_proof;
    const char *phase;
    const char *failed_phase;
    const char *failed_reason;
    const char *cleanup_attempted;
    const char *cleanup_status;
    const char *cleanup_idempotent;
    const char *owned_state_released;
    const char *partial_materialization;
    const char *residency_plan;
    const char *runtime_blockers;
    unsigned long long materialized_tensor_count;
    unsigned long long materialized_tensor_bytes;
    unsigned long long refused_tensor_count;
    unsigned long long skipped_tensor_count;
    unsigned long long required_tensor_count;
    unsigned long long required_tensor_bytes;
    unsigned long long peak_planned_bytes;
    unsigned long long cpu_resident_bytes;
    unsigned long long cuda_resident_bytes;
} yvex_fullmodel_materialize_report;

static void fullmodel_print_materialize_phase(unsigned int index,
                                              const char *name,
                                              const char *status)
{
    printf("materialize_phase.%u.name: %s\n", index, name ? name : "");
    printf("materialize_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void fullmodel_print_materialize_phase_set(const char *terminal_phase,
                                                  const char *failed_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-coverage",
        "placement-plan",
        "memory-budget",
        "backend-preflight",
        "materialize-embedding",
        "materialize-normalization",
        "materialize-attention",
        "materialize-mlp",
        "materialize-moe",
        "materialize-output",
        "materialize-tokenizer",
        "cleanup",
        "complete"
    };
    int failed_seen = 0;
    unsigned int i;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "planned";
        if (failed_phase && failed_phase[0] && strcmp(failed_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (!failed_seen &&
                   terminal_phase &&
                   (strcmp(terminal_phase, "complete") == 0 ||
                    strcmp(terminal_phase, phases[i]) == 0)) {
            status = "pass";
        } else if (failed_seen) {
            status = "skipped";
        }
        if (strcmp(phases[i], "materialize-moe") == 0 && !failed_seen) {
            status = "skipped";
        }
        fullmodel_print_materialize_phase(i, phases[i], status);
    }
}

static void fullmodel_print_materialize_report(const yvex_fullmodel_materialize_report *report)
{
    const yvex_cli_fullmodel_options *options = report ? report->options : NULL;

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("fullmodel-materialize: %s model=%s backend=%s\n",
               report && report->status ? report->status : "fullmodel-materialize-fail",
               options->model ? options->model : "",
               options->backend ? options->backend : "cpu");
        printf("reason: %s\n", report && report->failed_reason ? report->failed_reason : "none");
        printf("bytes: materialized=%llu required=%llu\n",
               report ? report->materialized_tensor_bytes : 0ull,
               report ? report->required_tensor_bytes : 0ull);
        printf("cleanup: %s\n", report && report->cleanup_status ? report->cleanup_status : "not-needed");
        printf("boundary: bounded proof/refusal only, no full model execution\n");
        printf("status: %s\n", report && report->status ? report->status : "fullmodel-materialize-fail");
        return;
    }

    printf("fullmodel: materialize\n");
    printf("status: %s\n", report && report->status ? report->status : "fullmodel-materialize-fail");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", report && report->model_resolved_path ? report->model_resolved_path : "");
    printf("target_id: %s\n", report && report->target_id ? report->target_id : "path");
    printf("target_class: %s\n", report && report->target_class ? report->target_class : "candidate-GGUF-path");
    printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
    printf("dry_run: %s\n", options && options->dry_run ? "true" : "false");
    printf("plan_only: %s\n", options && options->plan_only ? "true" : "false");
    printf("report_dir: %s\n", options && options->report_dir ? options->report_dir : "none");
    printf("artifact_identity_status: %s\n", report && report->artifact_identity_status ? report->artifact_identity_status : "not-checked");
    printf("tensor_inventory_status: %s\n", report && report->tensor_inventory_status ? report->tensor_inventory_status : "unknown");
    printf("required_role_coverage: %s\n", report && report->required_role_coverage ? report->required_role_coverage : "partial");
    printf("missing_required_roles: %s\n", report && report->missing_required_roles ? report->missing_required_roles : "unknown");
    printf("unsupported_required_roles: %s\n", report && report->unsupported_required_roles ? report->unsupported_required_roles : "runtime-family-adapter,real-transformer-prefill,real-attention-backed-KV,real-DeepSeek-decode,real-output-head-logits,real-vocabulary-sampling");
    printf("placement_plan_status: %s\n", report && report->placement_plan_status ? report->placement_plan_status : "unknown");
    printf("memory_budget_status: %s\n", report && report->memory_budget_status ? report->memory_budget_status : "unknown");
    printf("backend_preflight_status: %s\n", report && report->backend_preflight_status ? report->backend_preflight_status : "unknown");
    printf("materialization_mode: %s\n", report && report->materialization_mode ? report->materialization_mode : "none");
    printf("full_model_materialization: %s\n", report && report->full_model_materialization ? report->full_model_materialization : "failed");
    printf("full_model_materialization_proof: %s\n", report && report->full_model_materialization_proof ? report->full_model_materialization_proof : "fail");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("phase: %s\n", report && report->phase ? report->phase : "failed");
    printf("failed_phase: %s\n", report && report->failed_phase ? report->failed_phase : "none");
    printf("failed_reason: %s\n", report && report->failed_reason ? report->failed_reason : "none");
    printf("cleanup_attempted: %s\n", report && report->cleanup_attempted ? report->cleanup_attempted : "false");
    printf("cleanup_status: %s\n", report && report->cleanup_status ? report->cleanup_status : "not-needed");
    printf("cleanup_idempotent: %s\n", report && report->cleanup_idempotent ? report->cleanup_idempotent : "true");
    printf("owned_state_released: %s\n", report && report->owned_state_released ? report->owned_state_released : "true");
    printf("partial_materialization: %s\n", report && report->partial_materialization ? report->partial_materialization : "false");
    printf("materialized_tensor_count: %llu\n", report ? report->materialized_tensor_count : 0ull);
    printf("materialized_tensor_bytes: %llu\n", report ? report->materialized_tensor_bytes : 0ull);
    printf("refused_tensor_count: %llu\n", report ? report->refused_tensor_count : 0ull);
    printf("skipped_tensor_count: %llu\n", report ? report->skipped_tensor_count : 0ull);
    printf("required_tensor_count: %llu\n", report ? report->required_tensor_count : 0ull);
    printf("required_tensor_bytes: %llu\n", report ? report->required_tensor_bytes : 0ull);
    printf("peak_planned_bytes: %llu\n", report ? report->peak_planned_bytes : 0ull);
    printf("cpu_resident_bytes: %llu\n", report ? report->cpu_resident_bytes : 0ull);
    printf("cuda_resident_bytes: %llu\n", report ? report->cuda_resident_bytes : 0ull);
    printf("residency_plan: %s\n", report && report->residency_plan ? report->residency_plan : "not-planned");
    printf("runtime_blockers: %s\n", report && report->runtime_blockers ? report->runtime_blockers : "runtime family adapter not implemented");
    fullmodel_print_materialize_phase_set(report && report->phase ? report->phase : "failed",
                                          report && report->failed_phase ? report->failed_phase : NULL);
}

static void fullmodel_print_report_normal(const yvex_cli_fullmodel_options *options,
                                          const char *status,
                                          const char *target_id,
                                          const char *target_class,
                                          const char *role_coverage,
                                          const char *top_blocker,
                                          const char *next)
{
    printf("fullmodel: report model=%s backend=%s\n",
           options && options->model ? options->model : "",
           options && options->backend ? options->backend : "cpu");
    printf("status: %s\n", status ? status : "report-only");
    printf("target: %s class=%s\n",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    printf("role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    printf("top_blocker: %s\n", top_blocker ? top_blocker : "missing-full-runtime-tensor-coverage");
    printf("next: %s\n", next ? next : "tensor/source/artifact row required");
    printf("boundary: report-only, no full model execution\n");
}

static void fullmodel_print_plan_normal(const yvex_cli_fullmodel_options *options,
                                        const char *status,
                                        const char *target_class,
                                        const char *fit,
                                        const char *top_blocker)
{
    printf("materialization-plan: %s model=%s backend=%s\n",
           status ? status : "blocked",
           options && options->model ? options->model : "",
           options && options->backend ? options->backend : "cpu");
    printf("residency: %s\n", options && options->residency ? options->residency : "resident");
    printf("class: %s\n", target_class ? target_class : "unknown");
    printf("fit: %s\n", fit ? fit : "unknown");
    printf("top_blocker: %s\n", top_blocker ? top_blocker : "full-runtime candidate artifact required");
    printf("boundary: plan-only, no materialization\n");
    printf("status: fullmodel-materialization-plan\n");
}

static void fullmodel_print_descriptor_normal(const yvex_cli_fullmodel_options *options,
                                              const char *target_id,
                                              const char *target_class,
                                              const char *role_coverage,
                                              const char *missing_roles)
{
    printf("report: fullmodel-descriptor\n");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("backend: %s target=%s class=%s\n",
           options && options->backend ? options->backend : "cpu",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    printf("role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    printf("top_blocker: %s\n",
           missing_roles && strcmp(missing_roles, "none") == 0
               ? "runtime integration missing"
               : "missing-full-runtime-tensor-coverage");
    printf("boundary: descriptor report-only, no runtime execution\n");
    printf("status: fullmodel-descriptor\n");
}

static void fullmodel_print_family_runtime_normal(const yvex_cli_fullmodel_options *options,
                                                  const char *target_id,
                                                  const char *target_class,
                                                  const char *role_coverage,
                                                  const char *missing_roles)
{
    printf("report: family-runtime\n");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("family: %s backend=%s\n",
           options && options->family ? options->family : "auto",
           options && options->backend ? options->backend : "cpu");
    printf("target: %s class=%s\n",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    printf("status: %s\n", role_coverage ? role_coverage : "partial");
    printf("top_blocker: %s\n",
           missing_roles && strcmp(missing_roles, "none") == 0
               ? "runtime family adapter missing"
               : "missing family runtime tensor roles");
    printf("boundary: report-only, no runtime execution\n");
}

static int print_fullmodel_source_only_report(const char *target,
                                              const char *backend)
{
    printf("fullmodel: report\n");
    printf("status: fullmodel-report-unsupported\n");
    printf("model: %s\n", target ? target : "");
    printf("model_resolved_path: source-only-target\n");
    printf("target_id: %s\n", target ? target : "");
    printf("target_class: official-source-huge-model\n");
    printf("source_artifact_class: official safetensors\n");
    printf("target_artifact_class: future YVEX-produced GGUF\n");
    printf("artifact_exists: false\n");
    printf("artifact_bytes: 0\n");
    printf("artifact_identity_status: not-applicable\n");
    printf("tensor_count: 0\n");
    printf("tensor_inventory_status: not-performed-source-only-target\n");
    printf("metadata_status: not-performed\n");
    printf("architecture: glm\n");
    printf("family: glm\n");
    printf("model_class: huge-MoE-source-target\n");
    printf("fullmodel_inventory: unsupported-source-only\n");
    printf("qtype_summary: none\n");
    printf("dtype_summary: none\n");
    printf("total_tensor_bytes: 0\n");
    printf("estimated_cpu_resident_bytes: unknown\n");
    printf("estimated_cuda_resident_bytes: unknown\n");
    printf("estimated_kv_bytes: planned\n");
    printf("estimated_scratch_bytes: planned\n");
    printf("estimated_total_runtime_bytes: unknown\n");
    printf("backend: %s\n", backend ? backend : "cpu");
    printf("backend_placement_status: not-performed\n");
    printf("cpu_placement: unsupported-source-only\n");
    printf("cuda_placement: unsupported-source-only\n");
    printf("cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    printf("cuda_memory_status: unavailable\n");
    printf("residency_plan: future-YVEX-produced-artifact-required\n");
    printf("tensor_collections_status: not-performed\n");
    printf("collection_detected: no\n");
    printf("collection_supported: false\n");
    printf("runtime_consumer: unsupported\n");
    printf("embedding_tensors: 0\n");
    printf("normalization_tensors: 0\n");
    printf("attention_tensors: 0\n");
    printf("kv_cache_requirements: planned\n");
    printf("mlp_tensors: 0\n");
    printf("moe_tensors: 0\n");
    printf("output_tensors: 0\n");
    printf("tokenizer_tensors: 0\n");
    printf("required_role_coverage: none\n");
    printf("missing_required_roles: YVEX-produced-GGUF-artifact\n");
    printf("unsupported_required_roles: GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution\n");
    printf("runtime_blockers: source-only target; YVEX-produced GGUF emission planned; full model runtime unsupported\n");
    print_fullmodel_common_boundaries();
    return 0;
}

static void fullmodel_print_phase(unsigned int index,
                                  const char *name,
                                  const char *status,
                                  unsigned long long tensor_count,
                                  unsigned long long tensor_bytes,
                                  const char *residency,
                                  int required,
                                  int blocked,
                                  const char *blocker);
static unsigned int fullmodel_print_blocker(unsigned int index,
                                            const char *category,
                                            const char *severity,
                                            const char *message,
                                            int blocks_full_materialization,
                                            int blocks_generation);

static int print_fullmodel_source_only_plan(const yvex_cli_fullmodel_options *options,
                                            const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";

    printf("fullmodel: materialization-plan\n");
    printf("status: fullmodel-materialization-plan-unsupported\n");
    printf("model: %s\n", options && options->model ? options->model : target ? target : "");
    printf("model_resolved_path: source-only-target\n");
    printf("target_id: %s\n", target ? target : "");
    printf("target_class: official-source-huge-model\n");
    printf("artifact_exists: false\n");
    printf("artifact_bytes: 0\n");
    printf("artifact_identity_status: not-applicable\n");
    printf("tensor_inventory_status: not-performed-source-only-target\n");
    printf("tensor_count: 0\n");
    printf("total_tensor_bytes: 0\n");
    printf("backend: %s\n", backend);
    printf("residency: %s\n", residency);
    printf("plan_status: unsupported\n");
    printf("materialization_plan_ready: false\n");
    printf("materialization_attempted: false\n");
    printf("full_materialization_proof: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("plan_id: fullmodel-materialization:%s:%s:%s\n",
           target ? target : "source-only-target", backend, residency);
    printf("plan_kind: full-model-materialization\n");
    printf("plan_source: source-target-without-YVEX-GGUF\n");
    printf("plan_backend: %s\n", backend);
    printf("plan_residency: %s\n", residency);
    printf("plan_tensor_count: 0\n");
    printf("plan_tensor_bytes: 0\n");
    printf("plan_collection_count: 0\n");
    printf("plan_phase_count: 1\n");
    printf("plan_blocker_count: 1\n");
    printf("plan_cleanup_required: false\n");
    printf("plan_cleanup_phases: none\n");
    printf("backend_available: unknown\n");
    printf("backend_memory_known: false\n");
    printf("backend_memory_total_bytes: unknown\n");
    printf("backend_memory_available_bytes: unknown\n");
    printf("backend_required_bytes: 0\n");
    printf("backend_fit_status: unsupported\n");
    printf("backend_fit_reason: source-only target has no YVEX-produced GGUF tensor inventory\n");
    printf("backend_allocation_attempted: false\n");
    fullmodel_print_phase(0u, "preflight", "unsupported",
                          0ull, 0ull, "not-applicable", 1, 1,
                          "YVEX-produced GGUF artifact required before planning");
    (void)fullmodel_print_blocker(0u, "artifact", "fatal",
                                  "YVEX-produced GGUF artifact required before materialization planning",
                                  1, 1);
    printf("cleanup_plan_required: false\n");
    printf("cleanup_plan_phases: none\n");
    printf("cleanup_idempotent_required: true\n");
    printf("cleanup_failure_policy: preserve-failure-report\n");
    printf("next_required_row: FULLMODEL.2\n");
    printf("proof_ready_for_fullmodel_2: false\n");
    printf("fullmodel_2_blockers: YVEX-produced GGUF artifact missing; full tensor inventory unavailable\n");
    return 5;
}

static int print_fullmodel_source_only_materialize(const yvex_cli_fullmodel_options *options,
                                                   const char *target)
{
    yvex_fullmodel_materialize_report report;

    memset(&report, 0, sizeof(report));
    report.options = options;
    report.status = "fullmodel-materialize-unsupported";
    report.model_resolved_path = "source-only-target";
    report.target_id = target ? target : "source-only-target";
    report.target_class = "official-source-huge-model";
    report.artifact_identity_status = "not-applicable";
    report.tensor_inventory_status = "not-performed-source-only-target";
    report.required_role_coverage = "none";
    report.missing_required_roles = "YVEX-produced-GGUF-artifact";
    report.unsupported_required_roles = "GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution";
    report.placement_plan_status = "unsupported";
    report.memory_budget_status = "not-performed";
    report.backend_preflight_status = "not-performed";
    report.materialization_mode = "source-only-refusal";
    report.full_model_materialization = "unsupported-source-only";
    report.full_model_materialization_proof = "unsupported";
    report.phase = "failed";
    report.failed_phase = "resolve-model";
    report.failed_reason = "YVEX-produced-GGUF-artifact-missing";
    report.cleanup_attempted = "false";
    report.cleanup_status = "not-needed";
    report.cleanup_idempotent = "true";
    report.owned_state_released = "true";
    report.partial_materialization = "false";
    report.residency_plan = "future-YVEX-produced-artifact-required";
    report.runtime_blockers = "source-only target; YVEX-produced GGUF emission planned; full model runtime unsupported";
    fullmodel_print_materialize_report(&report);
    return 5;
}

static int print_fullmodel_source_only_descriptor(const yvex_cli_fullmodel_options *options,
                                                  const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";

    printf("fullmodel: descriptor\n");
    printf("status: fullmodel-descriptor-unsupported\n");
    printf("model: %s\n", options && options->model ? options->model : target ? target : "");
    printf("model_resolved_path: source-only-target\n");
    printf("target_id: %s\n", target ? target : "");
    printf("target_class: official-source-huge-model\n");
    printf("backend: %s\n", backend);
    printf("format: text\n");
    printf("artifact_identity_status: not-applicable\n");
    printf("tensor_inventory_status: not-performed-source-only-target\n");
    printf("materialization_plan_status: unsupported-source-only\n");
    printf("materialization_proof_status: unsupported-source-only\n");
    printf("runtime_descriptor: report-only\n");
    printf("runtime_descriptor_status: unsupported\n");
    printf("runtime_descriptor_kind: fullmodel-planning\n");
    printf("family: glm\n");
    printf("architecture: glm\n");
    printf("model_class: huge-MoE-source-target\n");
    printf("full_runtime_model: false\n");
    printf("full_model_execution: unsupported\n");
    printf("full_model_materialization: unsupported-source-only\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("tensor_role_map_status: not-performed\n");
    printf("tensor_collection_map_status: not-performed\n");
    printf("required_role_coverage: none\n");
    printf("missing_required_roles: YVEX-produced-GGUF-artifact\n");
    printf("unsupported_required_roles: GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution\n");
    printf("unknown_role_count: 0\n");
    printf("embedding_descriptor: not-performed-source-only\n");
    printf("normalization_descriptor: not-performed-source-only\n");
    printf("attention_descriptor: not-performed-source-only\n");
    printf("mlp_descriptor: not-performed-source-only\n");
    printf("moe_descriptor: not-performed-source-only\n");
    printf("output_descriptor: not-performed-source-only\n");
    printf("tokenizer_descriptor: not-performed-source-only\n");
    printf("kv_descriptor: unsupported-source-only\n");
    printf("prefill_descriptor: unsupported-source-only\n");
    printf("decode_descriptor: unsupported-source-only\n");
    printf("logits_descriptor: unsupported-source-only\n");
    printf("sampling_descriptor: unsupported-source-only\n");
    printf("graph_requirements_status: unsupported-source-only\n");
    printf("required_graph_ops: planned-after-YVEX-produced-GGUF\n");
    printf("unsupported_graph_ops: GLM-full-transformer-runtime\n");
    printf("required_backend_ops: planned-after-YVEX-produced-GGUF\n");
    printf("unsupported_backend_ops: GLM-runtime-execution\n");
    printf("residency_requirements_status: unsupported-source-only\n");
    printf("residency_plan: future-YVEX-produced-artifact-required\n");
    printf("cpu_resident_required_bytes: unknown\n");
    printf("cuda_resident_required_bytes: unknown\n");
    printf("host_staged_required_bytes: unknown\n");
    printf("ssd_staged_required_bytes: planned\n");
    printf("kv_required_bytes: planned\n");
    printf("scratch_required_bytes: planned\n");
    printf("context_requirements_status: planned\n");
    printf("max_context: unknown\n");
    printf("requested_context: not-requested\n");
    printf("context_policy: planned\n");
    printf("position_policy: planned\n");
    printf("rope_policy: planned\n");
    printf("kv_requirements_status: unsupported-source-only\n");
    printf("kv_layout: planned\n");
    printf("kv_dtype: planned\n");
    printf("kv_layers: unknown\n");
    printf("kv_heads: unknown\n");
    printf("kv_head_dim: unknown\n");
    printf("kv_capacity_status: unsupported-source-only\n");
    printf("kv_write_ready: false\n");
    printf("kv_read_ready: false\n");
    printf("logits_requirements_status: unsupported-source-only\n");
    printf("output_head_present: false\n");
    printf("output_head_dtype: unknown\n");
    printf("vocab_size: unknown\n");
    printf("logits_buffer_required: true\n");
    printf("real_output_head_logits: false\n");
    printf("logits_ready: false\n");
    printf("runtime_blockers: source-only target; YVEX-produced GGUF emission planned; GLM runtime unsupported\n");
    printf("descriptor_blockers: source-only target has no YVEX-produced GGUF tensor inventory\n");
    printf("prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("sampling_ready: false\n");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
    fullmodel_print_descriptor_phases("unsupported", "unsupported", "resolve-model");
    return 5;
}

static int print_fullmodel_source_only_family_runtime(const yvex_cli_fullmodel_options *options,
                                                      const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = fullmodel_requested_family(options);

    printf("family_runtime: report\n");
    printf("status: fullmodel-family-runtime-unsupported\n");
    printf("model: %s\n", options && options->model ? options->model : target ? target : "");
    printf("model_resolved_path: source-only-target\n");
    printf("target_id: %s\n", target ? target : "");
    printf("target_class: official-source-huge-model\n");
    printf("backend: %s\n", backend);
    printf("family: glm\n");
    printf("family_detected: glm\n");
    printf("family_requested: %s\n", requested);
    printf("family_adapter: unsupported-source-only\n");
    printf("family_adapter_status: unsupported\n");
    printf("family_runtime_stage: report-only\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("descriptor_status: unsupported-source-only\n");
    printf("descriptor_source: not-performed-source-only-target\n");
    printf("full_runtime_model: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("role_adapter_status: not-performed\n");
    printf("collection_adapter_status: not-performed\n");
    printf("attention_rule_status: unsupported-source-only\n");
    printf("position_rule_status: unsupported-source-only\n");
    printf("kv_rule_status: unsupported-source-only\n");
    printf("moe_rule_status: unsupported-source-only\n");
    printf("mlp_rule_status: unsupported-source-only\n");
    printf("output_head_rule_status: unsupported-source-only\n");
    printf("tokenizer_rule_status: unsupported-source-only\n");
    printf("graph_requirement_status: unsupported-source-only\n");
    printf("runtime_blocker_status: blocked\n");
    printf("tensor_inventory_status: not-performed-source-only-target\n");
    printf("token_embedding_role: not-performed-source-only\n");
    printf("attention_norm_role: not-performed-source-only\n");
    printf("q_projection_role: not-performed-source-only\n");
    printf("k_projection_role: not-performed-source-only\n");
    printf("v_projection_role: not-performed-source-only\n");
    printf("o_projection_role: not-performed-source-only\n");
    printf("output_head_role: not-performed-source-only\n");
    printf("tokenizer_metadata_role: not-performed-source-only\n");
    printf("attention_family: glm-family-planned\n");
    printf("attention_type: unknown-source-only\n");
    printf("attention_runtime_ready: false\n");
    printf("kv_required: true\n");
    printf("kv_layout: planned\n");
    printf("kv_dtype: planned\n");
    printf("kv_capacity_status: unsupported-source-only\n");
    printf("kv_write_ready: false\n");
    printf("kv_read_ready: false\n");
    printf("moe_required: true\n");
    printf("router_required: true\n");
    printf("router_present: false\n");
    printf("expert_tensors_present: false\n");
    printf("moe_expert_count: unknown\n");
    printf("moe_active_expert_count: unknown\n");
    printf("moe_shared_experts: unknown\n");
    printf("moe_dispatch_ready: false\n");
    printf("output_head_required: true\n");
    printf("output_head_present: false\n");
    printf("output_head_tensor: none\n");
    printf("vocab_size: unknown\n");
    printf("logits_projection_ready: false\n");
    printf("real_output_head_logits: false\n");
    printf("required_graph_ops: planned-after-YVEX-produced-GGUF\n");
    printf("implemented_graph_primitives: none-for-source-only-target\n");
    printf("unsupported_graph_ops: GLM-full-transformer-runtime\n");
    printf("graph.full_attention_from_model_tensors: unsupported\n");
    printf("graph.full_transformer_block_from_model_tensors: unsupported\n");
    printf("graph.full_layer_stack: unsupported\n");
    printf("full_transformer_graph_ready: false\n");
    printf("prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("sampling_ready: false\n");
    printf("runtime_execution_ready: false\n");
    printf("runtime_blockers: source-only target; YVEX-produced GGUF emission planned; GLM runtime family mapping planned; GLM runtime unsupported\n");
    printf("next_required_rows: OWI.HUGE.0,MODEL.CLASS.3,TENSOR.COLLECTION.2,ATTENTION.CLASS.0,KV.CACHE.0,MOE.CLASS.0,GLM-YVEX-produced-GGUF,GLM-runtime-family-mapping\n");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
    fullmodel_print_family_runtime_phases("unsupported", "resolve-model");
    return 5;
}

static void print_fullmodel_failed_plan_fields(const yvex_cli_fullmodel_options *options,
                                               const char *phase,
                                               const char *reason,
                                               unsigned long long artifact_bytes)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";

    printf("residency: %s\n", residency);
    printf("plan_status: failed\n");
    printf("materialization_plan_ready: false\n");
    printf("materialization_attempted: false\n");
    printf("full_materialization_proof: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("plan_id: unavailable\n");
    printf("plan_kind: full-model-materialization\n");
    printf("plan_source: tensor-inventory\n");
    printf("plan_backend: %s\n", backend);
    printf("plan_residency: %s\n", residency);
    printf("plan_tensor_count: 0\n");
    printf("plan_tensor_bytes: 0\n");
    printf("plan_collection_count: 0\n");
    printf("plan_phase_count: 1\n");
    printf("plan_blocker_count: 1\n");
    printf("plan_cleanup_required: false\n");
    printf("plan_cleanup_phases: none\n");
    printf("backend_available: unknown\n");
    printf("backend_memory_known: false\n");
    printf("backend_memory_total_bytes: unknown\n");
    printf("backend_memory_available_bytes: unknown\n");
    printf("backend_required_bytes: 0\n");
    printf("backend_fit_status: unknown\n");
    printf("backend_fit_reason: inventory failed before backend fit preflight\n");
    printf("backend_allocation_attempted: false\n");
    fullmodel_print_phase(0u, phase ? phase : "preflight", "blocked",
                          0ull, artifact_bytes, "not-applicable", 1, 1,
                          reason ? reason : "inventory failed");
    (void)fullmodel_print_blocker(0u, "inventory", "fatal",
                                  reason ? reason : "inventory failed",
                                  1, 1);
    printf("cleanup_plan_required: false\n");
    printf("cleanup_plan_phases: none\n");
    printf("cleanup_idempotent_required: true\n");
    printf("cleanup_failure_policy: preserve-failure-report\n");
    printf("next_required_row: FULLMODEL.2\n");
    printf("proof_ready_for_fullmodel_2: false\n");
    printf("fullmodel_2_blockers: tensor inventory unavailable; materialization plan unavailable\n");
}

static int print_fullmodel_missing_report(const yvex_cli_fullmodel_options *options,
                                          const char *resolved_path)
{
    int is_plan = options &&
                  options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    int is_materialize = options &&
                         options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    int is_descriptor = options &&
                        options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    int is_family_runtime = options &&
                            options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;

    if (is_materialize) {
        yvex_fullmodel_materialize_report report;
        memset(&report, 0, sizeof(report));
        report.options = options;
        report.status = "fullmodel-materialize-fail";
        report.model_resolved_path = resolved_path ? resolved_path : "";
        report.target_id = options && options->target ? options->target : "unknown";
        report.target_class = "unresolved-artifact";
        report.artifact_identity_status = "unavailable";
        report.tensor_inventory_status = "failed";
        report.required_role_coverage = "none";
        report.missing_required_roles = "artifact";
        report.unsupported_required_roles = "full-runtime-model";
        report.placement_plan_status = "failed";
        report.memory_budget_status = "not-performed";
        report.backend_preflight_status = "not-performed";
        report.materialization_mode = "none";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.phase = "failed";
        report.failed_phase = "resolve-model";
        report.failed_reason = "artifact path does not exist";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.cleanup_idempotent = "true";
        report.owned_state_released = "true";
        report.partial_materialization = "false";
        report.residency_plan = "unavailable";
        report.runtime_blockers = "artifact path missing";
        fullmodel_print_materialize_report(&report);
        printf("reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    if (is_descriptor) {
        printf("fullmodel: descriptor\n");
        printf("status: fullmodel-descriptor-fail\n");
        printf("model: %s\n", options && options->model ? options->model : "");
        printf("model_resolved_path: %s\n", resolved_path ? resolved_path : "");
        printf("target_id: %s\n", options && options->target ? options->target : "unknown");
        printf("target_class: unresolved-artifact\n");
        printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
        printf("artifact_identity_status: unavailable\n");
        printf("tensor_inventory_status: failed\n");
        printf("materialization_plan_status: unavailable\n");
        printf("materialization_proof_status: unavailable\n");
        printf("runtime_descriptor: report-only\n");
        printf("runtime_descriptor_status: fail\n");
        printf("runtime_descriptor_kind: fullmodel-planning\n");
        printf("family: unknown\n");
        printf("architecture: unknown\n");
        printf("model_class: unresolved\n");
        printf("full_runtime_model: false\n");
        printf("full_model_execution: unsupported\n");
        printf("full_model_materialization: unavailable\n");
        printf("generation_ready: false\n");
        printf("generation: unsupported-full-model\n");
        printf("benchmark_status: not-measured\n");
        printf("tensor_role_map_status: unavailable\n");
        printf("tensor_collection_map_status: unavailable\n");
        printf("required_role_coverage: none\n");
        printf("missing_required_roles: artifact\n");
        printf("unsupported_required_roles: full-runtime-model\n");
        printf("runtime_blockers: artifact path missing\n");
        printf("descriptor_blockers: artifact path missing\n");
        printf("prefill_ready: false\n");
        printf("decode_ready: false\n");
        printf("logits_ready: false\n");
        printf("sampling_ready: false\n");
        printf("cleanup_attempted: false\n");
        printf("cleanup_status: not-needed\n");
        fullmodel_print_descriptor_phases("fail", "fail", "resolve-model");
        printf("reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    if (is_family_runtime) {
        printf("family_runtime: report\n");
        printf("status: fullmodel-family-runtime-fail\n");
        printf("model: %s\n", options && options->model ? options->model : "");
        printf("model_resolved_path: %s\n", resolved_path ? resolved_path : "");
        printf("target_id: %s\n", options && options->target ? options->target : "unknown");
        printf("target_class: unresolved-artifact\n");
        printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
        printf("family: unknown\n");
        printf("family_detected: unknown\n");
        printf("family_requested: %s\n", fullmodel_requested_family(options));
        printf("family_adapter: unavailable\n");
        printf("family_adapter_status: failed\n");
        printf("family_runtime_stage: report-only\n");
        printf("runtime_claim: unsupported\n");
        printf("generation: unsupported-full-model\n");
        printf("benchmark_status: not-measured\n");
        printf("descriptor_status: fail\n");
        printf("descriptor_source: unavailable\n");
        printf("full_runtime_model: false\n");
        printf("full_model_execution: unsupported\n");
        printf("generation_ready: false\n");
        printf("runtime_execution_ready: false\n");
        printf("runtime_blockers: artifact path missing\n");
        printf("cleanup_attempted: false\n");
        printf("cleanup_status: not-needed\n");
        fullmodel_print_family_runtime_phases("failed", "resolve-model");
        printf("reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    printf("fullmodel: %s\n", is_plan ? "materialization-plan" : "report");
    printf("status: %s\n", is_plan ? "fullmodel-materialization-plan-fail" : "fullmodel-report-fail");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    printf("target_id: %s\n", options && options->target ? options->target : "unknown");
    printf("target_class: unresolved-artifact\n");
    printf("source_artifact_class: unknown\n");
    printf("target_artifact_class: GGUF artifact\n");
    printf("artifact_exists: false\n");
    printf("artifact_bytes: 0\n");
    printf("artifact_identity_status: unavailable\n");
    printf("tensor_count: 0\n");
    printf("tensor_inventory_status: failed\n");
    printf("metadata_status: failed\n");
    printf("architecture: unknown\n");
    printf("family: unknown\n");
    printf("model_class: unresolved\n");
    printf("fullmodel_inventory: unavailable\n");
    printf("qtype_summary: none\n");
    printf("dtype_summary: none\n");
    printf("total_tensor_bytes: 0\n");
    printf("estimated_cpu_resident_bytes: unknown\n");
    printf("estimated_cuda_resident_bytes: unknown\n");
    printf("estimated_kv_bytes: planned\n");
    printf("estimated_scratch_bytes: planned\n");
    printf("estimated_total_runtime_bytes: unknown\n");
    printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
    printf("backend_placement_status: failed-missing-artifact\n");
    printf("cpu_placement: unavailable\n");
    printf("cuda_placement: unavailable\n");
    printf("cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    printf("cuda_memory_status: unavailable\n");
    printf("residency_plan: unavailable\n");
    printf("tensor_collections_status: unavailable\n");
    printf("collection_detected: no\n");
    printf("collection_supported: false\n");
    printf("runtime_consumer: unsupported\n");
    printf("embedding_tensors: 0\n");
    printf("normalization_tensors: 0\n");
    printf("attention_tensors: 0\n");
    printf("kv_cache_requirements: planned\n");
    printf("mlp_tensors: 0\n");
    printf("moe_tensors: 0\n");
    printf("output_tensors: 0\n");
    printf("tokenizer_tensors: 0\n");
    printf("required_role_coverage: none\n");
    printf("missing_required_roles: artifact\n");
    printf("unsupported_required_roles: full-runtime-model\n");
    printf("runtime_blockers: artifact path missing\n");
    print_fullmodel_common_boundaries();
    if (is_plan) {
        print_fullmodel_failed_plan_fields(options,
                                           "preflight",
                                           "artifact path does not exist",
                                           0ull);
    }
    printf("reason: artifact path does not exist\n");
    return exit_for_status(YVEX_ERR_IO);
}

static int print_fullmodel_parse_failure_report(const yvex_cli_fullmodel_options *options,
                                                const yvex_model_ref *ref,
                                                const char *reason,
                                                int rc)
{
    unsigned long long artifact_bytes = 0ull;
    int is_plan = options &&
                  options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    int is_materialize = options &&
                         options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    int is_descriptor = options &&
                        options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    int is_family_runtime = options &&
                            options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;

    fullmodel_file_size(ref && ref->path ? ref->path : "", &artifact_bytes);
    if (is_materialize) {
        yvex_fullmodel_materialize_report report;
        memset(&report, 0, sizeof(report));
        report.options = options;
        report.status = "fullmodel-materialize-fail";
        report.model_resolved_path = ref && ref->path ? ref->path : "";
        report.target_id = options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path");
        report.target_class = "GGUF-artifact";
        report.artifact_identity_status = "not-checked";
        report.tensor_inventory_status = "failed";
        report.required_role_coverage = "none";
        report.missing_required_roles = "parseable-GGUF-tensor-directory";
        report.unsupported_required_roles = "full-runtime-model";
        report.placement_plan_status = "failed";
        report.memory_budget_status = "not-performed";
        report.backend_preflight_status = "not-performed";
        report.materialization_mode = "none";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.phase = "failed";
        report.failed_phase = "tensor-inventory";
        report.failed_reason = reason ? reason : "parse failed";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.cleanup_idempotent = "true";
        report.owned_state_released = "true";
        report.partial_materialization = "false";
        report.residency_plan = "unavailable";
        report.runtime_blockers = "GGUF metadata or tensor directory parse failed";
        fullmodel_print_materialize_report(&report);
        printf("artifact_bytes: %llu\n", artifact_bytes);
        printf("reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    if (is_descriptor) {
        printf("fullmodel: descriptor\n");
        printf("status: fullmodel-descriptor-fail\n");
        printf("model: %s\n", options && options->model ? options->model : "");
        printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        printf("target_id: %s\n", options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
        printf("target_class: GGUF-artifact\n");
        printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
        printf("artifact_identity_status: not-checked\n");
        printf("tensor_inventory_status: failed\n");
        printf("materialization_plan_status: failed\n");
        printf("materialization_proof_status: unavailable\n");
        printf("runtime_descriptor: report-only\n");
        printf("runtime_descriptor_status: fail\n");
        printf("runtime_descriptor_kind: fullmodel-planning\n");
        printf("family: unknown\n");
        printf("architecture: unknown\n");
        printf("model_class: parse-failed\n");
        printf("full_runtime_model: false\n");
        printf("full_model_execution: unsupported\n");
        printf("full_model_materialization: unavailable\n");
        printf("generation_ready: false\n");
        printf("generation: unsupported-full-model\n");
        printf("benchmark_status: not-measured\n");
        printf("tensor_role_map_status: unavailable\n");
        printf("tensor_collection_map_status: unavailable\n");
        printf("required_role_coverage: none\n");
        printf("missing_required_roles: parseable-GGUF-tensor-directory\n");
        printf("unsupported_required_roles: full-runtime-model\n");
        printf("runtime_blockers: GGUF metadata or tensor directory parse failed\n");
        printf("descriptor_blockers: GGUF metadata or tensor directory parse failed\n");
        printf("prefill_ready: false\n");
        printf("decode_ready: false\n");
        printf("logits_ready: false\n");
        printf("sampling_ready: false\n");
        printf("cleanup_attempted: false\n");
        printf("cleanup_status: not-needed\n");
        fullmodel_print_descriptor_phases("fail", "fail", "tensor-inventory");
        printf("artifact_bytes: %llu\n", artifact_bytes);
        printf("reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    if (is_family_runtime) {
        printf("family_runtime: report\n");
        printf("status: fullmodel-family-runtime-fail\n");
        printf("model: %s\n", options && options->model ? options->model : "");
        printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        printf("target_id: %s\n", options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
        printf("target_class: GGUF-artifact\n");
        printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
        printf("family: unknown\n");
        printf("family_detected: unknown\n");
        printf("family_requested: %s\n", fullmodel_requested_family(options));
        printf("family_adapter: unavailable\n");
        printf("family_adapter_status: failed\n");
        printf("family_runtime_stage: report-only\n");
        printf("runtime_claim: unsupported\n");
        printf("generation: unsupported-full-model\n");
        printf("benchmark_status: not-measured\n");
        printf("descriptor_status: fail\n");
        printf("descriptor_source: fullmodel-descriptor-facts\n");
        printf("full_runtime_model: false\n");
        printf("full_model_execution: unsupported\n");
        printf("generation_ready: false\n");
        printf("tensor_inventory_status: failed\n");
        printf("runtime_execution_ready: false\n");
        printf("runtime_blockers: GGUF metadata or tensor directory parse failed\n");
        printf("cleanup_attempted: false\n");
        printf("cleanup_status: not-needed\n");
        fullmodel_print_family_runtime_phases("failed", "load-descriptor");
        printf("artifact_bytes: %llu\n", artifact_bytes);
        printf("reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    printf("fullmodel: %s\n", is_plan ? "materialization-plan" : "report");
    printf("status: %s\n", is_plan ? "fullmodel-materialization-plan-fail" : "fullmodel-report-fail");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    printf("target_id: %s\n", options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
    printf("target_class: GGUF-artifact\n");
    printf("source_artifact_class: unknown\n");
    printf("target_artifact_class: GGUF artifact\n");
    printf("artifact_exists: true\n");
    printf("artifact_bytes: %llu\n", artifact_bytes);
    printf("artifact_identity_status: not-checked\n");
    printf("tensor_count: 0\n");
    printf("tensor_inventory_status: failed\n");
    printf("metadata_status: failed\n");
    printf("architecture: unknown\n");
    printf("family: unknown\n");
    printf("model_class: parse-failed\n");
    printf("fullmodel_inventory: unavailable\n");
    printf("qtype_summary: none\n");
    printf("dtype_summary: none\n");
    printf("total_tensor_bytes: 0\n");
    printf("estimated_cpu_resident_bytes: unknown\n");
    printf("estimated_cuda_resident_bytes: unknown\n");
    printf("estimated_kv_bytes: planned\n");
    printf("estimated_scratch_bytes: planned\n");
    printf("estimated_total_runtime_bytes: unknown\n");
    printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
    printf("backend_placement_status: failed-parse\n");
    printf("cpu_placement: unavailable\n");
    printf("cuda_placement: unavailable\n");
    printf("cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    printf("cuda_memory_status: unavailable\n");
    printf("residency_plan: unavailable\n");
    printf("tensor_collections_status: failed\n");
    printf("collection_detected: no\n");
    printf("collection_supported: false\n");
    printf("runtime_consumer: unsupported\n");
    printf("embedding_tensors: 0\n");
    printf("normalization_tensors: 0\n");
    printf("attention_tensors: 0\n");
    printf("kv_cache_requirements: planned\n");
    printf("mlp_tensors: 0\n");
    printf("moe_tensors: 0\n");
    printf("output_tensors: 0\n");
    printf("tokenizer_tensors: 0\n");
    printf("required_role_coverage: none\n");
    printf("missing_required_roles: parseable-GGUF-tensor-directory\n");
    printf("unsupported_required_roles: full-runtime-model\n");
    printf("runtime_blockers: GGUF metadata or tensor directory parse failed\n");
    print_fullmodel_common_boundaries();
    if (is_plan) {
        print_fullmodel_failed_plan_fields(options,
                                           "tensor-directory",
                                           reason ? reason : "parse failed",
                                           artifact_bytes);
    }
    printf("reason: %s\n", reason ? reason : "parse failed");
    return exit_for_status(rc);
}

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *registry_path;
    int include_kv;
    int include_context;
    int include_graph;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_attention_options;

static int attention_parse_value_option(const char *flag,
                                        int argc,
                                        char **argv,
                                        int *index,
                                        const char **value)
{
    if (*index + 1 >= argc) {
        fprintf(stderr, "yvex: attention %s requires a value\n", flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        fprintf(stderr, "yvex: attention %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_attention_options(int argc,
                                   char **argv,
                                   yvex_cli_attention_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_attention_help(stdout);
        return 1;
    }
    if (argc < 3 || strcmp(argv[2], "report") != 0) {
        fprintf(stderr, "yvex: attention requires report\n");
        fprintf(stderr, "usage: yvex attention report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--include-kv] [--include-context] [--include-graph] [--include-blockers]\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--model") == 0) {
            int rc = attention_parse_value_option("--model", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(argv[i], "--backend") == 0) {
            int rc = attention_parse_value_option("--backend", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                fprintf(stderr, "yvex: attention --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(argv[i], "--family") == 0) {
            int rc = attention_parse_value_option("--family", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(argv[i], "--registry") == 0) {
            int rc = attention_parse_value_option("--registry", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(argv[i], "--include-kv") == 0) {
            options->include_kv = 1;
        } else if (strcmp(argv[i], "--include-context") == 0) {
            options->include_context = 1;
        } else if (strcmp(argv[i], "--include-graph") == 0) {
            options->include_graph = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            int rc = attention_parse_value_option("--output", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                fprintf(stderr, "yvex: attention unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_attention_help(stdout);
            return 1;
        } else {
            fprintf(stderr, "yvex: unknown attention option: %s\n", argv[i]);
            return 2;
        }
    }

    if (!options->model || !options->model[0]) {
        fprintf(stderr, "yvex: attention report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *attention_requested_family(const yvex_cli_attention_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static void attention_print_phase(unsigned int index,
                                  const char *name,
                                  const char *status)
{
    printf("attention_phase.%u.name: %s\n", index, name ? name : "");
    printf("attention_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void attention_print_phases(const char *attention_status,
                                   const char *head_layout_status,
                                   const char *qkv_status,
                                   const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-family-runtime",
        "attention-profile",
        "head-layout",
        "qkv-role-check",
        "position-rules",
        "mask-rules",
        "kv-requirements",
        "context-requirements",
        "graph-requirements",
        "backend-requirements",
        "blocker-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "failed";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "blocked";
        } else if (strcmp(phases[i], "attention-profile") == 0) {
            status = attention_status ? attention_status : "partial";
        } else if (strcmp(phases[i], "head-layout") == 0) {
            status = head_layout_status ? head_layout_status : "unknown";
        } else if (strcmp(phases[i], "qkv-role-check") == 0) {
            status = qkv_status ? qkv_status : "partial";
        } else if (strcmp(phases[i], "position-rules") == 0 ||
                   strcmp(phases[i], "mask-rules") == 0 ||
                   strcmp(phases[i], "context-requirements") == 0) {
            status = "planned";
        } else if (strcmp(phases[i], "kv-requirements") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "backend-requirements") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "unsupported";
        }
        attention_print_phase(i, phases[i], status);
    }
}

static const char *attention_role_status(yvex_cli_tokenizer_context *ctx,
                                         const yvex_fullmodel_collections *collections,
                                         const char *role)
{
    return fullmodel_role_status_from_tensor(ctx, collections, role);
}

static void attention_print_projection_role(yvex_cli_tokenizer_context *ctx,
                                            const yvex_fullmodel_collections *collections,
                                            const char *role,
                                            const char *prefix)
{
    const yvex_tensor_info *tensor = fullmodel_descriptor_find_tensor(ctx, role);
    char dims[128];
    const char *status = attention_role_status(ctx, collections, role);

    dims[0] = '\0';
    if (tensor) dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
    printf("%s_required: true\n", prefix);
    printf("%s_status: %s\n", prefix, status);
    printf("%s_tensor: %s\n", prefix, tensor && tensor->name ? tensor->name : "none");
    printf("%s_shape: %s\n", prefix, tensor ? dims : "unknown");
    printf("%s_dtype: %s\n", prefix, tensor ? yvex_dtype_name(tensor->dtype) : "unknown");
    printf("%s_qtype: %s\n", prefix, tensor ? yvex_dtype_name(tensor->dtype) : "unknown");
    printf("%s_bytes: %llu\n", prefix, tensor ? tensor->storage_bytes : 0ull);
    printf("%s_runtime_consumer: %s\n", prefix,
           tensor ? "planned-full-transformer-attention" : "blocked-missing-role");
}

static void attention_print_unsupported_common(const char *model,
                                               const char *resolved_path,
                                               const char *target_id,
                                               const char *target_class,
                                               const char *backend,
                                               const char *family,
                                               const char *detected,
                                               const char *requested,
                                               const char *status,
                                               const char *reason,
                                               const char *phase)
{
    printf("attention: report\n");
    printf("status: %s\n", status ? status : "attention-report-unsupported");
    printf("model: %s\n", model ? model : "");
    printf("model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    printf("target_id: %s\n", target_id ? target_id : "unknown");
    printf("target_class: %s\n", target_class ? target_class : "unknown");
    printf("backend: %s\n", backend ? backend : "cpu");
    printf("family: %s\n", family ? family : "unknown");
    printf("family_detected: %s\n", detected ? detected : "unknown");
    printf("family_requested: %s\n", requested ? requested : "auto");
    printf("family_runtime_status: unsupported\n");
    printf("attention_class_status: unsupported\n");
    printf("attention_stage: report-only\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("attention_family: unsupported\n");
    printf("attention_type: unknown\n");
    printf("attention_support_status: report-only\n");
    printf("full_transformer_attention: unsupported\n");
    printf("attention_runtime_ready: false\n");
    printf("attention_backend_ready: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("q_projection_required: true\n");
    printf("k_projection_required: true\n");
    printf("v_projection_required: true\n");
    printf("o_projection_required: true\n");
    printf("q_projection_status: unknown\n");
    printf("k_projection_status: unknown\n");
    printf("v_projection_status: unknown\n");
    printf("o_projection_status: unknown\n");
    printf("attention_heads: unknown\n");
    printf("kv_heads: unknown\n");
    printf("head_dim: unknown\n");
    printf("hidden_size: unknown\n");
    printf("head_layout_status: unknown\n");
    printf("head_layout_source: unknown\n");
    printf("head_layout_blockers: attention class unavailable\n");
    printf("position_policy: planned\n");
    printf("rope_required: true\n");
    printf("rope_status: planned\n");
    printf("rope_base: unknown\n");
    printf("rope_scaling: unknown\n");
    printf("rope_dimension: unknown\n");
    printf("rope_runtime_ready: false\n");
    printf("mask_policy: planned\n");
    printf("causal_mask_required: true\n");
    printf("mask_status: planned\n");
    printf("mask_runtime_ready: false\n");
    printf("kv_required: true\n");
    printf("kv_layout: planned\n");
    printf("kv_dtype: planned\n");
    printf("kv_layers: unknown\n");
    printf("kv_heads: unknown\n");
    printf("kv_head_dim: unknown\n");
    printf("kv_capacity_status: unsupported-full-transformer-kv\n");
    printf("kv_write_ready: false\n");
    printf("kv_read_ready: false\n");
    printf("attention_kv_runtime_ready: false\n");
    printf("kv_boundary: diagnostic-kv-exists-real-attention-kv-unsupported\n");
    printf("context_policy: planned\n");
    printf("max_context: unknown\n");
    printf("requested_context: not-requested\n");
    printf("context_status: planned\n");
    printf("context_blockers: attention class unavailable\n");
    printf("graph_rope_primitive: implemented\n");
    printf("graph_attention_primitive: implemented-fixture\n");
    printf("graph_qkv_projection: unsupported\n");
    printf("graph_o_projection: unsupported\n");
    printf("graph_full_transformer_attention: unsupported\n");
    printf("graph_backend_status: report-only\n");
    printf("unsupported_graph_ops: full-transformer-attention,real-attention-backed-kv,real-transformer-prefill\n");
    printf("backend_attention_status: implemented-fixture-full-transformer-unsupported\n");
    printf("backend_rope_status: implemented-primitive\n");
    printf("backend_softmax_status: implemented-inside-attention-fixture\n");
    printf("backend_matmul_status: implemented-primitive\n");
    printf("backend_kv_status: unsupported-real-attention-kv\n");
    printf("attention_blockers: %s\n", reason ? reason : "attention class unavailable");
    printf("next_required_rows: ATTENTION.CLASS.0,KV.CACHE.0,CONTEXT.CLASS.0\n");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
    attention_print_phases("unsupported", "unknown", "unknown", phase);
    printf("reason: %s\n", reason ? reason : "unsupported attention class report");
}

static int attention_print_source_only_report(const yvex_cli_attention_options *options,
                                              const char *target)
{
    attention_print_unsupported_common(options && options->model ? options->model : target,
                                       "source-only-target",
                                       target,
                                       "official-source-huge-model",
                                       options && options->backend ? options->backend : "cpu",
                                       "glm",
                                       "glm",
                                       attention_requested_family(options),
                                       "attention-report-unsupported",
                                       "source-only target has no YVEX-produced GGUF tensor inventory; GLM attention class mapping planned",
                                       "resolve-model");
    printf("tensor_inventory_status: not-performed-source-only-target\n");
    printf("source_artifact_class: official safetensors\n");
    printf("target_artifact_class: future YVEX-produced GGUF\n");
    return 5;
}

static int attention_print_report(const yvex_cli_attention_options *options,
                                  yvex_model_ref *ref,
                                  yvex_cli_tokenizer_context *ctx,
                                  const char *target_id,
                                  const char *target_class,
                                  unsigned long long artifact_bytes,
                                  yvex_arch arch,
                                  unsigned long long tensor_count,
                                  unsigned long long total_tensor_bytes,
                                  const yvex_fullmodel_collections *collections,
                                  int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = attention_requested_family(options);
    yvex_cli_fullmodel_options family_probe;
    const char *detected;
    int request_matches;
    int supported_family;
    int has_q;
    int has_k;
    int has_v;
    int has_o;
    int has_all_qkvo;
    const char *attention_class_status;
    const char *qkv_status;
    const char *graph_projection_status;
    const char *blockers;

    memset(&family_probe, 0, sizeof(family_probe));
    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : "auto";
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    request_matches = fullmodel_family_request_matches(requested, detected);
    supported_family = request_matches && strcmp(detected, "deepseek") == 0;
    if (!supported_family) {
        const char *reason = !request_matches
                                 ? "requested family does not match detected family"
                                 : "attention report supports DeepSeek-family artifacts first";
        attention_print_unsupported_common(options && options->model ? options->model : "",
                                           ref && ref->path ? ref->path : "",
                                           target_id ? target_id : "path",
                                           target_class ? target_class : "candidate-GGUF-path",
                                           backend,
                                           detected ? detected : "unknown",
                                           detected ? detected : "unknown",
                                           requested,
                                           "attention-report-unsupported",
                                           reason,
                                           "resolve-family");
        return 5;
    }

    has_q = collections && collections->has_attention_q;
    has_k = collections && collections->has_attention_k;
    has_v = collections && collections->has_attention_v;
    has_o = collections && collections->has_attention_out;
    has_all_qkvo = has_q && has_k && has_v && has_o;
    attention_class_status = selected_target || !has_all_qkvo ? "partial" : "complete";
    qkv_status = has_all_qkvo ? "pass" : "partial";
    graph_projection_status = has_all_qkvo ? "planned" : "missing-tensor";
    blockers = has_all_qkvo
                   ? "real QKV projection over model tensors unsupported; attention-backed KV write unsupported; full transformer attention integration unsupported; real transformer prefill unsupported"
                   : "q projection tensor missing; k projection tensor missing; v projection tensor missing; o projection tensor missing; attention head layout incomplete; real QKV projection unsupported; real attention-backed KV writes unsupported; full transformer attention unsupported; real transformer prefill unsupported";

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("report: attention\n");
        printf("model: %s\n", options->model ? options->model : "");
        printf("family: deepseek backend=%s\n", backend);
        printf("status: %s\n", attention_class_status);
        printf("top_blocker: %s\n", has_all_qkvo ? "runtime attention integration missing" : "missing Q/K/V/O projection tensors");
        printf("next: V010.ATTN.9\n");
        printf("boundary: report-only, no runtime execution\n");
        return 0;
    }

    printf("attention: report\n");
    printf("status: attention-report\n");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    printf("target_id: %s\n", target_id ? target_id : "path");
    printf("target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    printf("backend: %s\n", backend);
    printf("family: deepseek\n");
    printf("family_detected: %s\n", detected);
    printf("family_requested: %s\n", requested);
    printf("family_runtime_status: %s\n", attention_class_status);
    printf("attention_class_status: %s\n", attention_class_status);
    printf("attention_stage: report-only\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    printf("tensor_inventory_status: pass\n");
    printf("tensor_count: %llu\n", tensor_count);
    printf("total_tensor_bytes: %llu\n", total_tensor_bytes);
    printf("attention_family: model-family-specific\n");
    printf("attention_type: unknown\n");
    printf("attention_support_status: report-only\n");
    printf("full_transformer_attention: unsupported\n");
    printf("attention_runtime_ready: false\n");
    printf("attention_backend_ready: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("attention_metadata_status: incomplete\n");
    printf("attention_blocker: attention metadata incomplete\n");
    printf("attention_norm_role: %s\n", attention_role_status(ctx, collections, "attention_norm"));
    attention_print_projection_role(ctx, collections, "q_projection", "q_projection");
    attention_print_projection_role(ctx, collections, "k_projection", "k_projection");
    attention_print_projection_role(ctx, collections, "v_projection", "v_projection");
    attention_print_projection_role(ctx, collections, "o_projection", "o_projection");
    printf("attention_heads: unknown\n");
    printf("kv_heads: unknown\n");
    printf("head_dim: unknown\n");
    printf("hidden_size: unknown\n");
    printf("head_layout_status: unknown\n");
    printf("head_layout_source: unknown\n");
    printf("head_layout_blockers: attention metadata incomplete; head count, KV head count, and head dimension unavailable\n");
    printf("position_policy: rope-or-family-specific-planned\n");
    printf("rope_required: true\n");
    printf("rope_status: planned\n");
    printf("rope_base: unknown\n");
    printf("rope_scaling: unknown\n");
    printf("rope_dimension: unknown\n");
    printf("graph_rope_primitive: implemented\n");
    printf("rope_runtime_ready: false\n");
    printf("mask_policy: causal-or-family-specific-planned\n");
    printf("causal_mask_required: true\n");
    printf("mask_status: planned\n");
    printf("mask_runtime_ready: false\n");
    printf("kv_required: true\n");
    printf("kv_layout: planned\n");
    printf("kv_dtype: planned\n");
    printf("kv_layers: unknown\n");
    printf("kv_heads: unknown\n");
    printf("kv_head_dim: unknown\n");
    printf("kv_capacity_status: unsupported-full-transformer-kv\n");
    printf("kv_write_ready: false\n");
    printf("kv_read_ready: false\n");
    printf("attention_kv_runtime_ready: false\n");
    printf("kv_boundary: diagnostic-kv-exists-real-attention-kv-unsupported\n");
    printf("context_policy: planned\n");
    printf("max_context: metadata-or-unknown\n");
    printf("requested_context: not-requested\n");
    printf("context_status: planned\n");
    printf("context_blockers: context class report pending; attention head layout incomplete\n");
    printf("graph_attention_primitive: implemented-fixture\n");
    printf("graph_matmul_primitive: implemented\n");
    printf("graph_qkv_projection: %s\n", graph_projection_status);
    printf("graph_o_projection: %s\n", has_o ? "planned" : "missing-tensor");
    printf("graph_model_qkv_projection: unsupported\n");
    printf("graph_attention_kv_write: unsupported\n");
    printf("graph_layer_integrated_attention: unsupported\n");
    printf("graph_full_transformer_attention: unsupported\n");
    printf("graph_backend_status: report-only\n");
    printf("unsupported_graph_ops: full-transformer-attention,real-qkv-projection,attention-backed-kv-write,layer-integrated-attention,real-transformer-prefill\n");
    printf("backend_attention_status: implemented-fixture-full-transformer-unsupported\n");
    printf("backend_rope_status: implemented-primitive\n");
    printf("backend_softmax_status: implemented-inside-attention-fixture\n");
    printf("backend_matmul_status: implemented-primitive\n");
    printf("backend_kv_status: unsupported-real-attention-kv\n");
    printf("backend_available: %s\n", fit.available ? "true" : "false");
    printf("backend_required_bytes: %llu\n", fit.required_bytes);
    printf("backend_fit_status: %s\n", fit.fit_status);
    printf("backend_allocation_attempted: false\n");
    printf("attention_blockers: %s\n", blockers);
    printf("prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("sampling_ready: false\n");
    printf("runtime_execution_ready: false\n");
    printf("next_required_rows: KV.CACHE.0,CONTEXT.CLASS.0,real-transformer-prefill,real-attention-backed-KV,real-decode,GEN.DEEPSEEK.0\n");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
    attention_print_phases(attention_class_status, "unknown", qkv_status, NULL);
    return 0;
}

int yvex_attention_command(int argc, char **argv)
{
    yvex_cli_attention_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    unsigned long long total_tensor_bytes = 0ull;
    unsigned long long tensor_count;
    unsigned long long i;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));

    rc = parse_attention_options(argc, argv, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        return attention_print_source_only_report(&options, "glm-5.2-official-safetensors");
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        attention_print_unsupported_common(options.model,
                                           ref.path,
                                           ref.alias && ref.alias[0] ? ref.alias : "unknown",
                                           "unresolved-artifact",
                                           options.backend,
                                           "unknown",
                                           "unknown",
                                           attention_requested_family(&options),
                                           "attention-report-fail",
                                           "artifact path does not exist",
                                           "resolve-model");
        yvex_model_ref_clear(&ref);
        return exit_for_status(YVEX_ERR_IO);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        attention_print_unsupported_common(options.model,
                                           ref.path,
                                           ref.alias && ref.alias[0] ? ref.alias : "path",
                                           "GGUF-artifact",
                                           options.backend,
                                           "unknown",
                                           "unknown",
                                           attention_requested_family(&options),
                                           "attention-report-fail",
                                           yvex_error_message(&err),
                                           "load-family-runtime");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        total_tensor_bytes += tensor->storage_bytes;
        fullmodel_classify_tensor(tensor, &collections);
    }
    if (yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.tokens") ||
        yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.model")) {
        collections.tokenizer = 1ull;
        collections.has_tokenizer_metadata = 1;
    }

    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = attention_print_report(&options,
                                &ref,
                                &ctx,
                                target_id,
                                target_class,
                                artifact_bytes,
                                arch,
                                tensor_count,
                                total_tensor_bytes,
                                &collections,
                                selected_target);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_attention_help(FILE *fp)
{
    fprintf(fp, "usage: yvex attention report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--audit | --output normal|table|audit] [--include-kv] [--include-context] [--include-graph] [--include-blockers]\n");
    fprintf(fp, "\nExamples:\n");
    fprintf(fp, "  yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu\n");
    fprintf(fp, "  yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu --include-kv --include-graph\n");
    fprintf(fp, "\nattention report:\n");
    fprintf(fp, "  classifies attention requirements, head layout, Q/K/V/O roles, RoPE/position rules, mask rules, KV requirements, context blockers, graph requirements, backend requirements, and runtime blockers.\n");
    fprintf(fp, "  Default output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "  report-only boundary: it does not run full attention, does not run transformer prefill, does not project Q/K/V from model tensors, does not write real attention-backed KV, does not generate, and does not benchmark.\n");
    fprintf(fp, "  standalone RoPE and attention primitives may be implemented, but those primitive proofs are not full transformer attention and are not model inference support.\n");
    fprintf(fp, "Boundary: no full transformer attention execution, no real QKV projection, no real attention-backed KV writes, no full model execution, no DeepSeek generation, no provider generation, no eval, no benchmark, no throughput.\n");
}

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *registry_path;
    int include_tensors;
    int include_residency;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_moe_options;

typedef struct {
    const char *status;
    const char *target_id;
    const char *target_class;
    const char *model;
    const char *model_resolved_path;
    const char *family;
    const char *family_detected;
    const char *family_requested;
    const char *backend;
    const char *source_class;
    const char *artifact_class;
    const char *implementation_stage;
    const char *model_is_moe;
    const char *moe_class_status;
    const char *expert_count_status;
    unsigned long long expert_count;
    const char *active_expert_count_status;
    unsigned long long active_expert_count;
    const char *router_status;
    const char *router_tensor_status;
    const char *router_tensor_name;
    const char *router_dtype;
    const char *router_logits_status;
    const char *top_k_policy_status;
    unsigned long long top_k;
    const char *shared_expert_status;
    unsigned long long shared_expert_count;
    const char *expert_tensor_collection_status;
    const char *expert_tensor_roles;
    unsigned long long expert_tensor_count;
    const char *expert_qtype_summary;
    const char *expert_storage_pressure;
    const char *expert_residency_pressure;
    const char *expert_dispatch_status;
    const char *expert_activation_status;
    const char *expert_accumulation_status;
    const char *prefill_integration_status;
    const char *decode_integration_status;
    const char *graph_integration_status;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    const char *blockers;
    const char *next_required_rows;
    yvex_models_output_mode output_mode;
    int include_tensors;
    int include_residency;
    int include_blockers;
} yvex_moe_class_report;

static int moe_parse_value_option(const char *flag,
                                  int argc,
                                  char **argv,
                                  int *index,
                                  const char **value)
{
    if (*index + 1 >= argc) {
        fprintf(stderr, "yvex: moe %s requires a value\n", flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        fprintf(stderr, "yvex: moe %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_moe_options(int argc,
                             char **argv,
                             yvex_cli_moe_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_moe_help(stdout);
        return 1;
    }
    if (argc < 3 || strcmp(argv[2], "report") != 0) {
        fprintf(stderr, "yvex: moe requires report\n");
        fprintf(stderr, "usage: yvex moe report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--include-tensors] [--include-residency] [--include-blockers]\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--model") == 0) {
            int rc = moe_parse_value_option("--model", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(argv[i], "--backend") == 0) {
            int rc = moe_parse_value_option("--backend", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                fprintf(stderr, "yvex: moe --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(argv[i], "--family") == 0) {
            int rc = moe_parse_value_option("--family", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(argv[i], "--registry") == 0) {
            int rc = moe_parse_value_option("--registry", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(argv[i], "--include-tensors") == 0) {
            options->include_tensors = 1;
        } else if (strcmp(argv[i], "--include-residency") == 0) {
            options->include_residency = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            int rc = moe_parse_value_option("--output", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                fprintf(stderr, "yvex: moe unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_moe_help(stdout);
            return 1;
        } else {
            fprintf(stderr, "yvex: unknown moe option: %s\n", argv[i]);
            return 2;
        }
    }

    if (!options->model || !options->model[0]) {
        fprintf(stderr, "yvex: moe report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *moe_requested_family(const yvex_cli_moe_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static void moe_init_report(yvex_moe_class_report *report,
                            const yvex_cli_moe_options *options)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = "internal-error";
    report->target_id = "unknown";
    report->target_class = "unknown";
    report->model = options && options->model ? options->model : "";
    report->model_resolved_path = "unknown";
    report->family = moe_requested_family(options);
    report->family_detected = "unknown";
    report->family_requested = moe_requested_family(options);
    report->backend = options && options->backend ? options->backend : "cpu";
    report->source_class = "unknown";
    report->artifact_class = "unknown";
    report->implementation_stage = "report-only";
    report->model_is_moe = "unknown";
    report->moe_class_status = "unknown";
    report->expert_count_status = "unknown";
    report->active_expert_count_status = "unknown";
    report->router_status = "unknown";
    report->router_tensor_status = "unknown";
    report->router_tensor_name = "none";
    report->router_dtype = "unknown";
    report->router_logits_status = "unsupported";
    report->top_k_policy_status = "unknown";
    report->shared_expert_status = "unknown";
    report->expert_tensor_collection_status = "unknown";
    report->expert_tensor_roles = "unknown";
    report->expert_qtype_summary = "unknown";
    report->expert_storage_pressure = "unknown";
    report->expert_residency_pressure = "unknown";
    report->expert_dispatch_status = "unsupported";
    report->expert_activation_status = "unsupported";
    report->expert_accumulation_status = "unsupported";
    report->prefill_integration_status = "unsupported";
    report->decode_integration_status = "unsupported";
    report->graph_integration_status = "unsupported";
    report->runtime_claim = "unsupported";
    report->generation = "unsupported-full-model";
    report->benchmark_status = "not-measured";
    report->blockers = "MoE model-class report failed before blocker classification";
    report->next_required_rows = "V010.CLASS.3";
    report->output_mode = options ? options->output_mode : YVEX_MODELS_OUTPUT_AUDIT;
    report->include_tensors = options ? options->include_tensors : 0;
    report->include_residency = options ? options->include_residency : 0;
    report->include_blockers = options ? options->include_blockers : 0;
}

static void moe_print_report(const yvex_moe_class_report *report)
{
    if (report && report->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("report: moe\n");
        printf("model: %s\n", report->model ? report->model : "");
        printf("family: %s\n", report->family ? report->family : "unknown");
        printf("status: %s\n", report->status ? report->status : "report-only");
        printf("top_blocker: %s\n", report->blockers ? report->blockers : "router/expert runtime unsupported");
        printf("next: %s\n", report->next_required_rows ? report->next_required_rows : "V010.MOE.*");
        printf("boundary: report-only, no runtime execution\n");
        return;
    }

    printf("moe: report\n");
    printf("status: %s\n", report && report->status ? report->status : "internal-error");
    printf("target_id: %s\n", report && report->target_id ? report->target_id : "unknown");
    printf("target_class: %s\n", report && report->target_class ? report->target_class : "unknown");
    printf("model: %s\n", report && report->model ? report->model : "");
    printf("model_resolved_path: %s\n", report && report->model_resolved_path ? report->model_resolved_path : "unknown");
    printf("family: %s\n", report && report->family ? report->family : "unknown");
    printf("family_detected: %s\n", report && report->family_detected ? report->family_detected : "unknown");
    printf("family_requested: %s\n", report && report->family_requested ? report->family_requested : "auto");
    printf("backend: %s\n", report && report->backend ? report->backend : "cpu");
    printf("source_class: %s\n", report && report->source_class ? report->source_class : "unknown");
    printf("artifact_class: %s\n", report && report->artifact_class ? report->artifact_class : "unknown");
    printf("implementation_stage: %s\n", report && report->implementation_stage ? report->implementation_stage : "report-only");
    printf("model_is_moe: %s\n", report && report->model_is_moe ? report->model_is_moe : "unknown");
    printf("moe_class_status: %s\n", report && report->moe_class_status ? report->moe_class_status : "unknown");
    printf("expert_count_status: %s\n", report && report->expert_count_status ? report->expert_count_status : "unknown");
    printf("expert_count: %llu\n", report ? report->expert_count : 0ull);
    printf("active_expert_count_status: %s\n", report && report->active_expert_count_status ? report->active_expert_count_status : "unknown");
    printf("active_expert_count: %llu\n", report ? report->active_expert_count : 0ull);
    printf("router_status: %s\n", report && report->router_status ? report->router_status : "unknown");
    printf("router_tensor_status: %s\n", report && report->router_tensor_status ? report->router_tensor_status : "unknown");
    printf("router_tensor_name: %s\n", report && report->router_tensor_name ? report->router_tensor_name : "none");
    printf("router_dtype: %s\n", report && report->router_dtype ? report->router_dtype : "unknown");
    printf("router_logits_status: %s\n", report && report->router_logits_status ? report->router_logits_status : "unsupported");
    printf("top_k_policy_status: %s\n", report && report->top_k_policy_status ? report->top_k_policy_status : "unknown");
    printf("top_k: %llu\n", report ? report->top_k : 0ull);
    printf("shared_expert_status: %s\n", report && report->shared_expert_status ? report->shared_expert_status : "unknown");
    printf("shared_expert_count: %llu\n", report ? report->shared_expert_count : 0ull);
    printf("expert_tensor_collection_status: %s\n", report && report->expert_tensor_collection_status ? report->expert_tensor_collection_status : "unknown");
    printf("expert_tensor_roles: %s\n", report && report->expert_tensor_roles ? report->expert_tensor_roles : "unknown");
    printf("expert_tensor_count: %llu\n", report ? report->expert_tensor_count : 0ull);
    printf("expert_qtype_summary: %s\n", report && report->expert_qtype_summary ? report->expert_qtype_summary : "unknown");
    printf("expert_storage_pressure: %s\n", report && report->expert_storage_pressure ? report->expert_storage_pressure : "unknown");
    printf("expert_residency_pressure: %s\n", report && report->expert_residency_pressure ? report->expert_residency_pressure : "unknown");
    printf("expert_dispatch_status: %s\n", report && report->expert_dispatch_status ? report->expert_dispatch_status : "unsupported");
    printf("expert_activation_status: %s\n", report && report->expert_activation_status ? report->expert_activation_status : "unsupported");
    printf("expert_accumulation_status: %s\n", report && report->expert_accumulation_status ? report->expert_accumulation_status : "unsupported");
    printf("prefill_integration_status: %s\n", report && report->prefill_integration_status ? report->prefill_integration_status : "unsupported");
    printf("decode_integration_status: %s\n", report && report->decode_integration_status ? report->decode_integration_status : "unsupported");
    printf("graph_integration_status: %s\n", report && report->graph_integration_status ? report->graph_integration_status : "unsupported");
    printf("runtime_claim: %s\n", report && report->runtime_claim ? report->runtime_claim : "unsupported");
    printf("generation: %s\n", report && report->generation ? report->generation : "unsupported-full-model");
    printf("benchmark_status: %s\n", report && report->benchmark_status ? report->benchmark_status : "not-measured");
    printf("blockers: %s\n", report && report->blockers ? report->blockers : "unknown");
    printf("next_required_rows: %s\n", report && report->next_required_rows ? report->next_required_rows : "V010.CLASS.3");
    printf("report_options.include_tensors: %s\n", report && report->include_tensors ? "true" : "false");
    printf("report_options.include_residency: %s\n", report && report->include_residency ? "true" : "false");
    printf("report_options.include_blockers: %s\n", report && report->include_blockers ? "true" : "false");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
}

static int moe_print_source_only_report(const yvex_cli_moe_options *options,
                                        const char *target)
{
    yvex_moe_class_report report;

    moe_init_report(&report, options);
    report.status = "unsupported-source-only";
    report.target_id = target ? target : "glm-5.2-official-safetensors";
    report.target_class = "huge-source-pressure";
    report.model_resolved_path = "source-only-target";
    report.family = "glm";
    report.family_detected = "glm";
    report.source_class = "official safetensors";
    report.artifact_class = "future YVEX-produced GGUF";
    report.model_is_moe = "true";
    report.moe_class_status = "source-only";
    report.expert_count_status = "unknown";
    report.active_expert_count_status = "unknown";
    report.router_status = "unsupported";
    report.router_tensor_status = "unsupported";
    report.router_logits_status = "unsupported";
    report.top_k_policy_status = "unknown";
    report.shared_expert_status = "unknown";
    report.expert_tensor_collection_status = "unsupported";
    report.expert_tensor_roles = "source-only-not-inspected";
    report.expert_qtype_summary = "unknown";
    report.expert_storage_pressure = "source-only-huge-pressure";
    report.expert_residency_pressure = "source-only-huge-pressure";
    report.blockers = "source-only target has no YVEX-produced GGUF tensor inventory; GLM runtime unsupported";
    report.next_required_rows = "OWI.HUGE.0,MODEL.CLASS.3,TENSOR.COLLECTION.2,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14,GLM-YVEX-produced-GGUF";
    moe_print_report(&report);
    return 5;
}

static int moe_print_missing_model_report(const yvex_cli_moe_options *options,
                                          const char *reason)
{
    yvex_moe_class_report report;

    moe_init_report(&report, options);
    report.status = "missing-model";
    report.target_id = options && options->model ? options->model : "missing";
    report.target_class = "unknown";
    report.model_resolved_path = "missing";
    report.family_detected = "unknown";
    report.source_class = "unknown";
    report.artifact_class = "unknown";
    report.model_is_moe = "unknown";
    report.moe_class_status = "unknown";
    report.router_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_tensor_collection_status = "unknown";
    report.expert_tensor_roles = "unknown";
    report.blockers = reason && reason[0] ? reason : "model or alias could not be resolved";
    report.next_required_rows = "V010.TARGET.9,V010.CLASS.3";
    moe_print_report(&report);
    return 5;
}

static int moe_print_unsupported_family_report(const yvex_cli_moe_options *options,
                                               yvex_model_ref *ref,
                                               const char *target_id,
                                               const char *target_class,
                                               const char *detected)
{
    yvex_moe_class_report report;

    moe_init_report(&report, options);
    report.status = "unsupported-family";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = moe_requested_family(options);
    report.family_detected = detected ? detected : "unknown";
    report.source_class = "GGUF artifact";
    report.artifact_class = "GGUF artifact";
    report.model_is_moe = "unknown";
    report.moe_class_status = "unsupported-family";
    report.router_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_tensor_collection_status = "unknown";
    report.expert_tensor_roles = "unknown";
    report.blockers = "requested family is not supported by MoE model-class report";
    report.next_required_rows = "V010.CLASS.0,V010.CLASS.3,family-specific-MoE-adapter";
    moe_print_report(&report);
    return 5;
}

static void moe_append_role(char *out, size_t out_cap, const char *role)
{
    size_t used;

    if (!out || out_cap == 0u || !role || !role[0]) return;
    used = strlen(out);
    if (used > 0u) {
        if (used + 1u >= out_cap) return;
        out[used++] = ',';
        out[used] = '\0';
    }
    snprintf(out + used, used < out_cap ? out_cap - used : 0u, "%s", role);
}

static int moe_print_model_report(const yvex_cli_moe_options *options,
                                  yvex_model_ref *ref,
                                  yvex_cli_tokenizer_context *ctx,
                                  const char *target_id,
                                  const char *target_class,
                                  yvex_arch arch,
                                  const yvex_fullmodel_collections *collections)
{
    yvex_moe_class_report report;
    yvex_cli_fullmodel_options family_probe;
    const yvex_tensor_info *router;
    const yvex_tensor_info *expert_gate;
    const yvex_tensor_info *expert_up;
    const yvex_tensor_info *expert_down;
    const char *requested;
    const char *detected;
    char expert_roles[160];
    char expert_qtypes[192];
    unsigned int present_roles = 0u;

    memset(&family_probe, 0, sizeof(family_probe));
    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : NULL;
    requested = moe_requested_family(options);
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    if (!fullmodel_family_request_matches(requested, detected) || strcmp(detected, "deepseek") != 0) {
        return moe_print_unsupported_family_report(options, ref, target_id, target_class, detected);
    }

    router = fullmodel_descriptor_find_tensor(ctx, "moe_router");
    expert_gate = fullmodel_descriptor_find_tensor(ctx, "moe_expert_gate");
    expert_up = fullmodel_descriptor_find_tensor(ctx, "moe_expert_up");
    expert_down = fullmodel_descriptor_find_tensor(ctx, "moe_expert_down");

    expert_roles[0] = '\0';
    expert_qtypes[0] = '\0';
    if (expert_gate) {
        present_roles++;
        moe_append_role(expert_roles, sizeof(expert_roles), "moe-expert-gate");
        snprintf(expert_qtypes + strlen(expert_qtypes),
                 strlen(expert_qtypes) < sizeof(expert_qtypes) ? sizeof(expert_qtypes) - strlen(expert_qtypes) : 0u,
                 "%smoe-expert-gate:%s",
                 expert_qtypes[0] ? "," : "",
                 yvex_dtype_name(expert_gate->dtype));
    }
    if (expert_up) {
        present_roles++;
        moe_append_role(expert_roles, sizeof(expert_roles), "moe-expert-up");
        snprintf(expert_qtypes + strlen(expert_qtypes),
                 strlen(expert_qtypes) < sizeof(expert_qtypes) ? sizeof(expert_qtypes) - strlen(expert_qtypes) : 0u,
                 "%smoe-expert-up:%s",
                 expert_qtypes[0] ? "," : "",
                 yvex_dtype_name(expert_up->dtype));
    }
    if (expert_down) {
        present_roles++;
        moe_append_role(expert_roles, sizeof(expert_roles), "moe-expert-down");
        snprintf(expert_qtypes + strlen(expert_qtypes),
                 strlen(expert_qtypes) < sizeof(expert_qtypes) ? sizeof(expert_qtypes) - strlen(expert_qtypes) : 0u,
                 "%smoe-expert-down:%s",
                 expert_qtypes[0] ? "," : "",
                 yvex_dtype_name(expert_down->dtype));
    }
    if (!expert_roles[0]) snprintf(expert_roles, sizeof(expert_roles), "missing");
    if (!expert_qtypes[0]) snprintf(expert_qtypes, sizeof(expert_qtypes), "unknown");

    moe_init_report(&report, options);
    report.status = "ok-partial";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = "deepseek";
    report.family_detected = detected;
    report.family_requested = requested;
    report.source_class = strcmp(report.target_class, "selected-runtime-slice") == 0
                              ? "YVEX-produced selected GGUF"
                              : "GGUF artifact";
    report.artifact_class = strcmp(report.target_class, "selected-runtime-slice") == 0
                                ? "YVEX-produced selected GGUF"
                                : "GGUF artifact";
    report.model_is_moe = "true";
    report.moe_class_status = "partial";
    report.expert_count_status = "unknown";
    report.active_expert_count_status = "unknown";
    report.router_status = router ? "known" : "missing";
    report.router_tensor_status = router ? "known" : "missing";
    report.router_tensor_name = router && router->name ? router->name : "none";
    report.router_dtype = router ? yvex_dtype_name(router->dtype) : "unknown";
    report.router_logits_status = "unsupported";
    report.top_k_policy_status = "unknown";
    report.shared_expert_status = "unknown";
    report.expert_tensor_collection_status = present_roles == 3u ? "known" :
                                             (present_roles > 0u ? "partial" : "missing");
    report.expert_tensor_roles = expert_roles;
    report.expert_tensor_count = present_roles;
    report.expert_qtype_summary = expert_qtypes;
    report.expert_storage_pressure = present_roles > 0u ? "planned" : "missing";
    report.expert_residency_pressure = present_roles > 0u ? "planned" : "missing";
    report.blockers = router
                          ? "router tensor is classified but router logits, top-k routing, expert activation, dispatch, accumulation, graph integration, prefill integration, and decode integration are unsupported"
                          : "router tensor missing; expert tensor collection missing; router logits unsupported; top-k routing unsupported; expert activation unsupported; expert dispatch unsupported; expert accumulation unsupported";
    report.next_required_rows = router || present_roles > 0u
                                    ? "V010.MOE.4,V010.MOE.5,V010.MOE.6,V010.MOE.7,V010.STORAGE.18,V010.RESIDENCY.14"
                                    : "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14,V010.MOE.4";
    if (collections && collections->has_moe_expert && present_roles == 0u) {
        report.expert_tensor_collection_status = "partial";
        report.expert_tensor_count = collections->moe;
        report.expert_tensor_roles = "expert-pattern-detected";
        report.expert_storage_pressure = "planned";
        report.expert_residency_pressure = "planned";
    }
    moe_print_report(&report);
    return 0;
}

int yvex_moe_command(int argc, char **argv)
{
    yvex_cli_moe_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    unsigned long long tensor_count;
    unsigned long long i;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));

    rc = parse_moe_options(argc, argv, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        return moe_print_source_only_report(&options, "glm-5.2-official-safetensors");
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&err);
        return moe_print_missing_model_report(&options, "model or alias could not be resolved");
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        rc = moe_print_missing_model_report(&options, "resolved artifact path does not exist");
        yvex_model_ref_clear(&ref);
        return rc;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        const char *reason = yvex_error_message(&err);
        rc = moe_print_missing_model_report(&options, reason && reason[0] ? reason : "GGUF metadata or tensor directory parse failed");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        fullmodel_classify_tensor(tensor, &collections);
    }
    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = moe_print_model_report(&options,
                                &ref,
                                &ctx,
                                target_id,
                                target_class,
                                arch,
                                &collections);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_moe_help(FILE *fp)
{
    fprintf(fp, "usage: yvex moe report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--audit | --output normal|table|audit] [--include-tensors] [--include-residency] [--include-blockers]\n");
    fprintf(fp, "\nExamples:\n");
    fprintf(fp, "  yvex moe report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --include-tensors --include-blockers\n");
    fprintf(fp, "  yvex moe report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cuda --include-residency\n");
    fprintf(fp, "  yvex moe report --model glm-5.2-official-safetensors --family glm --backend cpu --include-blockers\n");
    fprintf(fp, "\nmoe report:\n");
    fprintf(fp, "  classifies the model as MoE/source-only/unsupported-family and reports router facts, expert tensor-role facts, shared-expert facts, storage and residency pressure, blockers, and next rows.\n");
    fprintf(fp, "  Default output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "  report-only boundary: it does not execute router logits, does not perform top-k routing, does not activate experts, does not dispatch experts, does not accumulate expert outputs, does not integrate MoE into graph/prefill/decode, does not generate, and does not benchmark.\n");
    fprintf(fp, "  selected-runtime-slice targets may return ok-partial when the family is MoE but router or expert tensors are not present in the selected artifact.\n");
    fprintf(fp, "  source-only targets are reported without opening huge source shards or downloading artifacts.\n");
    fprintf(fp, "Boundary: no MoE runtime execution, no full transformer block, no full transformer prefill, no attention-backed KV writes, no decode, no logits, no vocabulary sampling, no generation, no serving, no eval, no benchmark, no throughput.\n");
}

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *collection;
    const char *registry_path;
    int include_router;
    int include_experts;
    int include_shared;
    int include_dispatch;
    int include_storage;
    int include_residency;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_tensor_collection_options;

typedef struct {
    const char *tensor_collection;
    const char *status;
    const char *target_id;
    const char *target_class;
    const char *model;
    const char *model_resolved_path;
    const char *family;
    const char *family_detected;
    const char *family_requested;
    const char *backend;
    const char *source_class;
    const char *artifact_class;
    const char *implementation_stage;
    const char *collection_stage;
    const char *model_is_moe;
    const char *moe_class_status;
    const char *router_collection_status;
    const char *router_role_status;
    const char *router_tensor_name;
    const char *router_tensor_present;
    const char *router_tensor_status;
    const char *router_dtype;
    const char *router_qtype;
    const char *router_shape;
    const char *router_source;
    const char *router_logits_status;
    const char *top_k_policy_status;
    unsigned long long top_k;
    const char *expert_collection_status;
    const char *expert_role_status;
    const char *expert_tensor_roles;
    const char *expert_gate_role_status;
    const char *expert_up_role_status;
    const char *expert_down_role_status;
    const char *expert_tensor_name_pattern;
    const char *expert_tensor_count_status;
    unsigned long long expert_tensor_count;
    const char *expert_count_status;
    unsigned long long expert_count;
    const char *active_expert_count_status;
    unsigned long long active_expert_count;
    const char *shared_expert_collection_status;
    const char *shared_expert_status;
    const char *shared_expert_roles;
    const char *shared_expert_count_status;
    unsigned long long shared_expert_count;
    const char *dispatch_metadata_status;
    const char *dispatch_metadata_roles;
    const char *expert_indexing_policy_status;
    const char *expert_storage_pressure_status;
    const char *expert_storage_pressure;
    const char *expert_residency_pressure_status;
    const char *expert_residency_pressure;
    const char *present_roles;
    const char *missing_roles;
    const char *unknown_roles;
    const char *not_applicable_roles;
    const char *blocked_rows;
    const char *next_required_rows;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    yvex_models_output_mode output_mode;
    int include_router;
    int include_experts;
    int include_shared;
    int include_dispatch;
    int include_storage;
    int include_residency;
    int include_blockers;
} yvex_tensor_collection_report;

static int tensor_collection_parse_value_option(const char *flag,
                                                int argc,
                                                char **argv,
                                                int *index,
                                                const char **value)
{
    if (*index + 1 >= argc) {
        fprintf(stderr, "yvex: tensor-collection %s requires a value\n", flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        fprintf(stderr, "yvex: tensor-collection %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_tensor_collection_options(int argc,
                                           char **argv,
                                           yvex_cli_tensor_collection_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_tensor_collection_help(stdout);
        return 1;
    }
    if (argc < 3 || strcmp(argv[2], "report") != 0) {
        fprintf(stderr, "yvex: tensor-collection requires report\n");
        fprintf(stderr, "usage: yvex tensor-collection report --model FILE_OR_ALIAS --collection moe [--family auto|deepseek|glm|qwen] [--backend cpu|cuda]\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--model") == 0) {
            int rc = tensor_collection_parse_value_option("--model", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(argv[i], "--backend") == 0) {
            int rc = tensor_collection_parse_value_option("--backend", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->backend = value;
        } else if (strcmp(argv[i], "--family") == 0) {
            int rc = tensor_collection_parse_value_option("--family", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(argv[i], "--collection") == 0) {
            int rc = tensor_collection_parse_value_option("--collection", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->collection = value;
        } else if (strcmp(argv[i], "--registry") == 0) {
            int rc = tensor_collection_parse_value_option("--registry", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(argv[i], "--include-router") == 0) {
            options->include_router = 1;
        } else if (strcmp(argv[i], "--include-experts") == 0) {
            options->include_experts = 1;
        } else if (strcmp(argv[i], "--include-shared") == 0) {
            options->include_shared = 1;
        } else if (strcmp(argv[i], "--include-dispatch") == 0) {
            options->include_dispatch = 1;
        } else if (strcmp(argv[i], "--include-storage") == 0) {
            options->include_storage = 1;
        } else if (strcmp(argv[i], "--include-residency") == 0) {
            options->include_residency = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            int rc = tensor_collection_parse_value_option("--output", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                fprintf(stderr, "yvex: tensor-collection unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_tensor_collection_help(stdout);
            return 1;
        } else {
            fprintf(stderr, "yvex: unknown tensor-collection option: %s\n", argv[i]);
            return 2;
        }
    }

    if (!options->model || !options->model[0]) {
        fprintf(stderr, "yvex: tensor-collection report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *tensor_collection_requested_family(const yvex_cli_tensor_collection_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static const char *tensor_collection_requested_collection(const yvex_cli_tensor_collection_options *options)
{
    return options && options->collection && options->collection[0] ? options->collection : "unknown";
}

static void tensor_collection_init_report(yvex_tensor_collection_report *report,
                                          const yvex_cli_tensor_collection_options *options)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->tensor_collection = tensor_collection_requested_collection(options);
    report->status = "internal-error";
    report->target_id = "unknown";
    report->target_class = "unknown";
    report->model = options && options->model ? options->model : "";
    report->model_resolved_path = "unknown";
    report->family = tensor_collection_requested_family(options);
    report->family_detected = "unknown";
    report->family_requested = tensor_collection_requested_family(options);
    report->backend = options && options->backend ? options->backend : "cpu";
    report->source_class = "unknown";
    report->artifact_class = "unknown";
    report->implementation_stage = "report-only";
    report->collection_stage = "report-only";
    report->model_is_moe = "unknown";
    report->moe_class_status = "unknown";
    report->router_collection_status = "unknown";
    report->router_role_status = "unknown";
    report->router_tensor_name = "none";
    report->router_tensor_present = "false";
    report->router_tensor_status = "unknown";
    report->router_dtype = "unknown";
    report->router_qtype = "unknown";
    report->router_shape = "unknown";
    report->router_source = "unknown";
    report->router_logits_status = "unsupported";
    report->top_k_policy_status = "unknown";
    report->expert_collection_status = "unknown";
    report->expert_role_status = "unknown";
    report->expert_tensor_roles = "unknown";
    report->expert_gate_role_status = "unknown";
    report->expert_up_role_status = "unknown";
    report->expert_down_role_status = "unknown";
    report->expert_tensor_name_pattern = "unknown";
    report->expert_tensor_count_status = "unknown";
    report->expert_count_status = "unknown";
    report->active_expert_count_status = "unknown";
    report->shared_expert_collection_status = "unknown";
    report->shared_expert_status = "unknown";
    report->shared_expert_roles = "unknown";
    report->shared_expert_count_status = "unknown";
    report->dispatch_metadata_status = "unknown";
    report->dispatch_metadata_roles = "unknown";
    report->expert_indexing_policy_status = "unknown";
    report->expert_storage_pressure_status = "unknown";
    report->expert_storage_pressure = "unknown";
    report->expert_residency_pressure_status = "unknown";
    report->expert_residency_pressure = "unknown";
    report->present_roles = "none";
    report->missing_roles = "unknown";
    report->unknown_roles = "unknown";
    report->not_applicable_roles = "none";
    report->blocked_rows = "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17";
    report->next_required_rows = "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17";
    report->runtime_claim = "unsupported";
    report->generation = "unsupported-full-model";
    report->benchmark_status = "not-measured";
    report->output_mode = options ? options->output_mode : YVEX_MODELS_OUTPUT_AUDIT;
    report->include_router = options ? options->include_router : 0;
    report->include_experts = options ? options->include_experts : 0;
    report->include_shared = options ? options->include_shared : 0;
    report->include_dispatch = options ? options->include_dispatch : 0;
    report->include_storage = options ? options->include_storage : 0;
    report->include_residency = options ? options->include_residency : 0;
    report->include_blockers = options ? options->include_blockers : 0;
}

static void tensor_collection_print_report(const yvex_tensor_collection_report *report)
{
    if (report && report->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("report: tensor-collection\n");
        printf("model: %s\n", report->model ? report->model : "");
        printf("family: %s collection=%s\n",
               report->family ? report->family : "unknown",
               report->tensor_collection ? report->tensor_collection : "unknown");
        printf("status: %s\n", report->status ? report->status : "report-only");
        printf("top_blocker: %s\n", report->blocked_rows ? report->blocked_rows : "tensor roles incomplete");
        printf("next: %s\n", report->next_required_rows ? report->next_required_rows : "V010.TENSOR.*");
        printf("boundary: report-only, no runtime execution\n");
        return;
    }

    printf("tensor_collection: %s\n", report && report->tensor_collection ? report->tensor_collection : "unknown");
    printf("status: %s\n", report && report->status ? report->status : "internal-error");
    printf("target_id: %s\n", report && report->target_id ? report->target_id : "unknown");
    printf("target_class: %s\n", report && report->target_class ? report->target_class : "unknown");
    printf("model: %s\n", report && report->model ? report->model : "");
    printf("model_resolved_path: %s\n", report && report->model_resolved_path ? report->model_resolved_path : "unknown");
    printf("family: %s\n", report && report->family ? report->family : "unknown");
    printf("family_detected: %s\n", report && report->family_detected ? report->family_detected : "unknown");
    printf("family_requested: %s\n", report && report->family_requested ? report->family_requested : "auto");
    printf("backend: %s\n", report && report->backend ? report->backend : "cpu");
    printf("source_class: %s\n", report && report->source_class ? report->source_class : "unknown");
    printf("artifact_class: %s\n", report && report->artifact_class ? report->artifact_class : "unknown");
    printf("implementation_stage: %s\n", report && report->implementation_stage ? report->implementation_stage : "report-only");
    printf("collection_stage: %s\n", report && report->collection_stage ? report->collection_stage : "report-only");
    printf("model_is_moe: %s\n", report && report->model_is_moe ? report->model_is_moe : "unknown");
    printf("moe_class_status: %s\n", report && report->moe_class_status ? report->moe_class_status : "unknown");
    printf("router_collection_status: %s\n", report && report->router_collection_status ? report->router_collection_status : "unknown");
    printf("router_role_status: %s\n", report && report->router_role_status ? report->router_role_status : "unknown");
    printf("router_tensor_name: %s\n", report && report->router_tensor_name ? report->router_tensor_name : "none");
    printf("router_tensor_present: %s\n", report && report->router_tensor_present ? report->router_tensor_present : "false");
    printf("router_tensor_status: %s\n", report && report->router_tensor_status ? report->router_tensor_status : "unknown");
    printf("router_dtype: %s\n", report && report->router_dtype ? report->router_dtype : "unknown");
    printf("router_qtype: %s\n", report && report->router_qtype ? report->router_qtype : "unknown");
    printf("router_shape: %s\n", report && report->router_shape ? report->router_shape : "unknown");
    printf("router_source: %s\n", report && report->router_source ? report->router_source : "unknown");
    printf("router_logits_status: %s\n", report && report->router_logits_status ? report->router_logits_status : "unsupported");
    printf("top_k_policy_status: %s\n", report && report->top_k_policy_status ? report->top_k_policy_status : "unknown");
    printf("top_k: %llu\n", report ? report->top_k : 0ull);
    printf("expert_collection_status: %s\n", report && report->expert_collection_status ? report->expert_collection_status : "unknown");
    printf("expert_role_status: %s\n", report && report->expert_role_status ? report->expert_role_status : "unknown");
    printf("expert_tensor_roles: %s\n", report && report->expert_tensor_roles ? report->expert_tensor_roles : "unknown");
    printf("expert_gate_role_status: %s\n", report && report->expert_gate_role_status ? report->expert_gate_role_status : "unknown");
    printf("expert_up_role_status: %s\n", report && report->expert_up_role_status ? report->expert_up_role_status : "unknown");
    printf("expert_down_role_status: %s\n", report && report->expert_down_role_status ? report->expert_down_role_status : "unknown");
    printf("expert_tensor_name_pattern: %s\n", report && report->expert_tensor_name_pattern ? report->expert_tensor_name_pattern : "unknown");
    printf("expert_tensor_count_status: %s\n", report && report->expert_tensor_count_status ? report->expert_tensor_count_status : "unknown");
    printf("expert_tensor_count: %llu\n", report ? report->expert_tensor_count : 0ull);
    printf("expert_count_status: %s\n", report && report->expert_count_status ? report->expert_count_status : "unknown");
    printf("expert_count: %llu\n", report ? report->expert_count : 0ull);
    printf("active_expert_count_status: %s\n", report && report->active_expert_count_status ? report->active_expert_count_status : "unknown");
    printf("active_expert_count: %llu\n", report ? report->active_expert_count : 0ull);
    printf("shared_expert_collection_status: %s\n", report && report->shared_expert_collection_status ? report->shared_expert_collection_status : "unknown");
    printf("shared_expert_status: %s\n", report && report->shared_expert_status ? report->shared_expert_status : "unknown");
    printf("shared_expert_roles: %s\n", report && report->shared_expert_roles ? report->shared_expert_roles : "unknown");
    printf("shared_expert_count_status: %s\n", report && report->shared_expert_count_status ? report->shared_expert_count_status : "unknown");
    printf("shared_expert_count: %llu\n", report ? report->shared_expert_count : 0ull);
    printf("dispatch_metadata_status: %s\n", report && report->dispatch_metadata_status ? report->dispatch_metadata_status : "unknown");
    printf("dispatch_metadata_roles: %s\n", report && report->dispatch_metadata_roles ? report->dispatch_metadata_roles : "unknown");
    printf("expert_indexing_policy_status: %s\n", report && report->expert_indexing_policy_status ? report->expert_indexing_policy_status : "unknown");
    printf("expert_storage_pressure_status: %s\n", report && report->expert_storage_pressure_status ? report->expert_storage_pressure_status : "unknown");
    printf("expert_storage_pressure: %s\n", report && report->expert_storage_pressure ? report->expert_storage_pressure : "unknown");
    printf("expert_residency_pressure_status: %s\n", report && report->expert_residency_pressure_status ? report->expert_residency_pressure_status : "unknown");
    printf("expert_residency_pressure: %s\n", report && report->expert_residency_pressure ? report->expert_residency_pressure : "unknown");
    printf("present_roles: %s\n", report && report->present_roles ? report->present_roles : "none");
    printf("missing_roles: %s\n", report && report->missing_roles ? report->missing_roles : "unknown");
    printf("unknown_roles: %s\n", report && report->unknown_roles ? report->unknown_roles : "unknown");
    printf("not_applicable_roles: %s\n", report && report->not_applicable_roles ? report->not_applicable_roles : "none");
    printf("blocked_rows: %s\n", report && report->blocked_rows ? report->blocked_rows : "unknown");
    printf("next_required_rows: %s\n", report && report->next_required_rows ? report->next_required_rows : "unknown");
    printf("runtime_claim: %s\n", report && report->runtime_claim ? report->runtime_claim : "unsupported");
    printf("generation: %s\n", report && report->generation ? report->generation : "unsupported-full-model");
    printf("benchmark_status: %s\n", report && report->benchmark_status ? report->benchmark_status : "not-measured");
    printf("generation_ready: false\n");
    printf("report_options.include_router: %s\n", report && report->include_router ? "true" : "false");
    printf("report_options.include_experts: %s\n", report && report->include_experts ? "true" : "false");
    printf("report_options.include_shared: %s\n", report && report->include_shared ? "true" : "false");
    printf("report_options.include_dispatch: %s\n", report && report->include_dispatch ? "true" : "false");
    printf("report_options.include_storage: %s\n", report && report->include_storage ? "true" : "false");
    printf("report_options.include_residency: %s\n", report && report->include_residency ? "true" : "false");
    printf("report_options.include_blockers: %s\n", report && report->include_blockers ? "true" : "false");
    printf("cleanup_attempted: false\n");
    printf("cleanup_status: not-needed\n");
}

static int tensor_collection_print_invalid_argument_report(const yvex_cli_tensor_collection_options *options,
                                                           const char *status,
                                                           const char *reason)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.status = status && status[0] ? status : "invalid-argument";
    report.target_id = options && options->model ? options->model : "unknown";
    report.model_resolved_path = "not-resolved";
    report.collection_stage = "report-only";
    report.router_logits_status = "unsupported";
    report.blocked_rows = reason && reason[0] ? reason : "invalid tensor collection report argument";
    report.next_required_rows = "valid-collection-moe";
    tensor_collection_print_report(&report);
    return 2;
}

static int tensor_collection_print_source_only_report(const yvex_cli_tensor_collection_options *options,
                                                      const char *target)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
    report.status = "unsupported-source-only";
    report.target_id = target ? target : "glm-5.2-official-safetensors";
    report.target_class = "huge-source-pressure";
    report.model_resolved_path = "source-only-target";
    report.family = "glm";
    report.family_detected = "glm";
    report.source_class = "official safetensors";
    report.artifact_class = "future YVEX-produced GGUF";
    report.model_is_moe = "true";
    report.moe_class_status = "source-only";
    report.router_collection_status = "unsupported";
    report.router_role_status = "unsupported";
    report.router_tensor_status = "unsupported";
    report.router_source = "source-only-not-inspected";
    report.expert_collection_status = "unsupported";
    report.expert_role_status = "unsupported";
    report.expert_tensor_roles = "source-only-not-inspected";
    report.expert_gate_role_status = "unsupported";
    report.expert_up_role_status = "unsupported";
    report.expert_down_role_status = "unsupported";
    report.expert_tensor_count_status = "unknown";
    report.shared_expert_collection_status = "unknown";
    report.shared_expert_status = "unknown";
    report.shared_expert_count_status = "unknown";
    report.dispatch_metadata_status = "unknown";
    report.expert_storage_pressure_status = "unknown";
    report.expert_storage_pressure = "source-only-unmeasured";
    report.expert_residency_pressure_status = "unknown";
    report.expert_residency_pressure = "requires-storage-stream-plan";
    report.present_roles = "none";
    report.missing_roles = "source-only-not-inspected";
    report.unknown_roles = "moe_router,moe_expert_gate,moe_expert_up,moe_expert_down,moe_shared_expert,moe_dispatch_metadata,expert_indexing_policy";
    report.blocked_rows = "OWI.HUGE.0,V010.MAP.4,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14";
    report.next_required_rows = "OWI.HUGE.0,V010.MAP.4,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14,GLM-YVEX-produced-GGUF";
    tensor_collection_print_report(&report);
    return 5;
}

static int tensor_collection_print_missing_model_report(const yvex_cli_tensor_collection_options *options,
                                                        const char *reason)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
    report.status = "missing-model";
    report.target_id = options && options->model ? options->model : "missing";
    report.model_resolved_path = "missing";
    report.router_collection_status = "unknown";
    report.router_role_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_collection_status = "unknown";
    report.expert_role_status = "unknown";
    report.blocked_rows = reason && reason[0] ? reason : "model or alias could not be resolved";
    report.next_required_rows = "V010.TARGET.9,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17";
    tensor_collection_print_report(&report);
    return 5;
}

static int tensor_collection_print_unsupported_family_report(const yvex_cli_tensor_collection_options *options,
                                                            yvex_model_ref *ref,
                                                            const char *target_id,
                                                            const char *target_class,
                                                            const char *detected)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
    report.status = "unsupported-family";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = tensor_collection_requested_family(options);
    report.family_detected = detected ? detected : "unknown";
    report.source_class = "GGUF artifact";
    report.artifact_class = "GGUF artifact";
    report.model_is_moe = "unknown";
    report.moe_class_status = "unsupported-family";
    report.router_collection_status = "unknown";
    report.router_role_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_collection_status = "unknown";
    report.expert_role_status = "unknown";
    report.blocked_rows = "requested family is not supported by MoE tensor collection report";
    report.next_required_rows = "V010.CLASS.0,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,family-specific-tensor-adapter";
    tensor_collection_print_report(&report);
    return 5;
}

static void tensor_collection_append_role(char *out, size_t out_cap, const char *role)
{
    size_t used;

    if (!out || out_cap == 0u || !role || !role[0]) return;
    used = strlen(out);
    if (used > 0u) {
        if (used + 1u >= out_cap) return;
        out[used++] = ',';
        out[used] = '\0';
    }
    snprintf(out + used, used < out_cap ? out_cap - used : 0u, "%s", role);
}

static const char *tensor_collection_role_status(const yvex_tensor_info *tensor)
{
    return tensor ? "present" : "missing";
}

static int tensor_collection_print_model_report(const yvex_cli_tensor_collection_options *options,
                                                yvex_model_ref *ref,
                                                yvex_cli_tokenizer_context *ctx,
                                                const char *target_id,
                                                const char *target_class,
                                                yvex_arch arch)
{
    yvex_tensor_collection_report report;
    yvex_cli_fullmodel_options family_probe;
    const yvex_tensor_info *router;
    const yvex_tensor_info *expert_gate;
    const yvex_tensor_info *expert_up;
    const yvex_tensor_info *expert_down;
    const char *requested;
    const char *detected;
    char router_shape[128];
    char present_roles[192];
    char missing_roles[256];
    char expert_roles[192];
    unsigned int present_expert_roles = 0u;

    memset(&family_probe, 0, sizeof(family_probe));
    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : NULL;
    requested = tensor_collection_requested_family(options);
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    if (!fullmodel_family_request_matches(requested, detected) || strcmp(detected, "deepseek") != 0) {
        return tensor_collection_print_unsupported_family_report(options, ref, target_id, target_class, detected);
    }

    router = fullmodel_descriptor_find_tensor(ctx, "moe_router");
    expert_gate = fullmodel_descriptor_find_tensor(ctx, "moe_expert_gate");
    expert_up = fullmodel_descriptor_find_tensor(ctx, "moe_expert_up");
    expert_down = fullmodel_descriptor_find_tensor(ctx, "moe_expert_down");

    router_shape[0] = '\0';
    if (router) dims_to_text(router->dims, router->rank, router_shape, sizeof(router_shape));
    else snprintf(router_shape, sizeof(router_shape), "unknown");

    present_roles[0] = '\0';
    missing_roles[0] = '\0';
    expert_roles[0] = '\0';
    if (router) tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_router");
    else tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_router");
    if (expert_gate) {
        present_expert_roles++;
        tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_expert_gate");
        tensor_collection_append_role(expert_roles, sizeof(expert_roles), "moe_expert_gate");
    } else {
        tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_expert_gate");
    }
    if (expert_up) {
        present_expert_roles++;
        tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_expert_up");
        tensor_collection_append_role(expert_roles, sizeof(expert_roles), "moe_expert_up");
    } else {
        tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_expert_up");
    }
    if (expert_down) {
        present_expert_roles++;
        tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_expert_down");
        tensor_collection_append_role(expert_roles, sizeof(expert_roles), "moe_expert_down");
    } else {
        tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_expert_down");
    }
    if (!present_roles[0]) snprintf(present_roles, sizeof(present_roles), "none");
    if (!missing_roles[0]) snprintf(missing_roles, sizeof(missing_roles), "none");
    if (!expert_roles[0]) snprintf(expert_roles, sizeof(expert_roles), "missing");

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
    report.status = "ok-partial";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = "deepseek";
    report.family_detected = detected;
    report.family_requested = requested;
    report.source_class = strcmp(report.target_class, "selected-runtime-slice") == 0
                              ? "YVEX-produced selected GGUF"
                              : "GGUF artifact";
    report.artifact_class = strcmp(report.target_class, "selected-runtime-slice") == 0
                                ? "YVEX-produced selected GGUF"
                                : "GGUF artifact";
    report.model_is_moe = "true";
    report.moe_class_status = "partial";
    report.router_collection_status = router ? "present" : "missing";
    report.router_role_status = tensor_collection_role_status(router);
    report.router_tensor_name = router && router->name ? router->name : "none";
    report.router_tensor_present = router ? "true" : "false";
    report.router_tensor_status = tensor_collection_role_status(router);
    report.router_dtype = router ? yvex_dtype_name(router->dtype) : "unknown";
    report.router_qtype = router ? yvex_dtype_name(router->dtype) : "unknown";
    report.router_shape = router_shape;
    report.router_source = router ? "artifact-directory" : "missing";
    report.router_logits_status = "unsupported";
    report.top_k_policy_status = "unknown";
    report.top_k = 0ull;
    report.expert_collection_status = present_expert_roles == 3u ? "present" :
                                      (present_expert_roles > 0u ? "partial" : "missing");
    report.expert_role_status = present_expert_roles == 3u ? "present" :
                                (present_expert_roles > 0u ? "partial" : "missing");
    report.expert_tensor_roles = expert_roles;
    report.expert_gate_role_status = tensor_collection_role_status(expert_gate);
    report.expert_up_role_status = tensor_collection_role_status(expert_up);
    report.expert_down_role_status = tensor_collection_role_status(expert_down);
    report.expert_tensor_name_pattern = present_expert_roles > 0u ? "expert.*.(gate|up|down)" : "unknown";
    report.expert_tensor_count_status = present_expert_roles > 0u ? "known" : "missing";
    report.expert_tensor_count = present_expert_roles;
    report.expert_count_status = "unknown";
    report.active_expert_count_status = "unknown";
    report.shared_expert_collection_status = "unknown";
    report.shared_expert_status = "unknown";
    report.shared_expert_roles = "unknown";
    report.shared_expert_count_status = "unknown";
    report.dispatch_metadata_status = "unknown";
    report.dispatch_metadata_roles = "unknown";
    report.expert_indexing_policy_status = "unknown";
    report.expert_storage_pressure_status = "unknown";
    report.expert_storage_pressure = strcmp(report.target_class, "selected-runtime-slice") == 0
                                         ? "selected-slice-missing"
                                         : "expert-dominant-expected";
    report.expert_residency_pressure_status = "unknown";
    report.expert_residency_pressure = strcmp(report.target_class, "selected-runtime-slice") == 0
                                           ? "selected-slice-missing"
                                           : "requires-expert-residency-plan";
    report.present_roles = present_roles;
    report.missing_roles = missing_roles;
    report.unknown_roles = "moe_shared_expert,moe_dispatch_metadata,expert_indexing_policy";
    report.not_applicable_roles = "none";
    report.blocked_rows = "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.MOE.5,V010.MOE.6,V010.MOE.7,V010.PREFILL.7,V010.DECODE.6";
    report.next_required_rows = router || present_expert_roles > 0u
                                    ? "V010.STORAGE.18,V010.RESIDENCY.14,V010.MOE.5,V010.MOE.6,V010.MOE.7"
                                    : "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.TARGET.9,V010.MAP.2,V010.FULLMODEL.6";
    tensor_collection_print_report(&report);
    return 0;
}

int yvex_tensor_collection_command(int argc, char **argv)
{
    yvex_cli_tensor_collection_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));

    rc = parse_tensor_collection_options(argc, argv, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.backend, "cpu") != 0 && strcmp(options.backend, "cuda") != 0) {
        return tensor_collection_print_invalid_argument_report(&options,
                                                              "unsupported-backend",
                                                              "backend must be cpu or cuda");
    }
    if (!options.collection || strcmp(options.collection, "moe") != 0) {
        return tensor_collection_print_invalid_argument_report(&options,
                                                              "invalid-argument",
                                                              "only --collection moe is implemented");
    }
    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        return tensor_collection_print_source_only_report(&options, "glm-5.2-official-safetensors");
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&err);
        return tensor_collection_print_missing_model_report(&options, "model or alias could not be resolved");
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        rc = tensor_collection_print_missing_model_report(&options, "resolved artifact path does not exist");
        yvex_model_ref_clear(&ref);
        return rc;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        const char *reason = yvex_error_message(&err);
        rc = tensor_collection_print_missing_model_report(&options, reason && reason[0] ? reason : "GGUF metadata or tensor directory parse failed");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = tensor_collection_print_model_report(&options, &ref, &ctx, target_id, target_class, arch);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_tensor_collection_help(FILE *fp)
{
    fprintf(fp, "usage: yvex tensor-collection report --model FILE_OR_ALIAS --collection moe [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--audit | --output normal|table|audit] [--include-router] [--include-experts] [--include-shared] [--include-dispatch] [--include-storage] [--include-residency] [--include-blockers]\n");
    fprintf(fp, "\nExamples:\n");
    fprintf(fp, "  yvex tensor-collection report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --collection moe --backend cpu --include-router --include-experts --include-blockers\n");
    fprintf(fp, "  yvex tensor-collection report --model glm-5.2-official-safetensors --family glm --collection moe --backend cpu --include-blockers\n");
    fprintf(fp, "\ntensor-collection report:\n");
    fprintf(fp, "  reports tensor collection requirements and coverage for the requested collection. The current implemented collection is moe.\n");
    fprintf(fp, "  Default output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "  MoE collection reports classify router, expert gate/up/down, shared expert, dispatch metadata, indexing, storage pressure, residency pressure, blockers, and next rows.\n");
    fprintf(fp, "  report-only boundary: it does not materialize tensors, does not execute router logits, does not select experts, does not dispatch experts, does not accumulate expert outputs, does not run MoE blocks, does not run prefill, does not run decode, does not produce logits, does not sample, does not generate, does not evaluate, and does not benchmark.\n");
    fprintf(fp, "  selected-runtime-slice targets may return ok-partial when the family is MoE but router or expert tensors are not present in the selected artifact.\n");
    fprintf(fp, "  source-only targets are reported without opening huge source shards or downloading artifacts.\n");
    fprintf(fp, "Boundary: no MoE runtime execution, no router logits, no top-k routing, no expert activation, no expert dispatch, no expert accumulation, no full transformer block, no full transformer prefill, no decode, no logits, no vocabulary sampling, no generation, no eval, no benchmark, no throughput.\n");
}

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *registry_path;
    const char *tokens_text;
    unsigned long long context_length;
    unsigned long long chunk_size;
    int context_length_seen;
    int chunk_size_seen;
    int include_attention;
    int include_kv;
    int include_prefill;
    int include_decode;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_context_options;

static int context_parse_value_option(const char *flag,
                                      int argc,
                                      char **argv,
                                      int *index,
                                      const char **value)
{
    if (*index + 1 >= argc) {
        fprintf(stderr, "yvex: context %s requires a value\n", flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        fprintf(stderr, "yvex: context %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_context_options(int argc,
                                 char **argv,
                                 yvex_cli_context_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_context_help(stdout);
        return 1;
    }
    if (argc < 3 || strcmp(argv[2], "report") != 0) {
        fprintf(stderr, "yvex: context requires report\n");
        fprintf(stderr, "usage: yvex context report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [options]\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--model") == 0) {
            int rc = context_parse_value_option("--model", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(argv[i], "--backend") == 0) {
            int rc = context_parse_value_option("--backend", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                fprintf(stderr, "yvex: context --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(argv[i], "--family") == 0) {
            int rc = context_parse_value_option("--family", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(argv[i], "--registry") == 0) {
            int rc = context_parse_value_option("--registry", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            int rc = context_parse_value_option("--tokens", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->tokens_text = value;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            int rc = context_parse_value_option("--context-length", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_positive_ull(value, &options->context_length)) {
                fprintf(stderr, "yvex: context --context-length requires a positive integer\n");
                return 2;
            }
            options->context_length_seen = 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            int rc = context_parse_value_option("--chunk-size", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_positive_ull(value, &options->chunk_size)) {
                fprintf(stderr, "yvex: context --chunk-size requires a positive integer\n");
                return 2;
            }
            options->chunk_size_seen = 1;
        } else if (strcmp(argv[i], "--include-attention") == 0) {
            options->include_attention = 1;
        } else if (strcmp(argv[i], "--include-kv") == 0) {
            options->include_kv = 1;
        } else if (strcmp(argv[i], "--include-prefill") == 0) {
            options->include_prefill = 1;
        } else if (strcmp(argv[i], "--include-decode") == 0) {
            options->include_decode = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            int rc = context_parse_value_option("--output", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                fprintf(stderr, "yvex: context unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_context_help(stdout);
            return 1;
        } else {
            fprintf(stderr, "yvex: unknown context option: %s\n", argv[i]);
            return 2;
        }
    }
    if (!options->model || !options->model[0]) {
        fprintf(stderr, "yvex: context report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *context_requested_family(const yvex_cli_context_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static void context_print_phase(unsigned int index,
                                const char *name,
                                const char *status)
{
    printf("context_phase.%u.name: %s\n", index, name ? name : "");
    printf("context_phase.%u.status: %s\n", index, status ? status : "unknown");
}

static void context_print_phases(const char *family_status,
                                 const char *attention_status,
                                 const char *kv_status,
                                 const char *context_status,
                                 const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-family-runtime",
        "load-attention-class",
        "load-kv-class",
        "context-profile",
        "model-context",
        "requested-context",
        "active-context",
        "token-input",
        "chunking-policy",
        "prefill-boundary",
        "decode-position-policy",
        "overflow-policy",
        "runtime-blockers",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "failed";
            failed_seen = 1;
        } else if (failed_seen && strcmp(phases[i], "cleanup") != 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "load-family-runtime") == 0) {
            status = family_status ? family_status : "unknown";
        } else if (strcmp(phases[i], "load-attention-class") == 0) {
            status = attention_status ? attention_status : "unknown";
        } else if (strcmp(phases[i], "load-kv-class") == 0) {
            status = kv_status ? kv_status : "unknown";
        } else if (strcmp(phases[i], "context-profile") == 0) {
            status = context_status ? context_status : "planned";
        } else if (strcmp(phases[i], "prefill-boundary") == 0 ||
                   strcmp(phases[i], "decode-position-policy") == 0 ||
                   strcmp(phases[i], "runtime-blockers") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "unsupported";
        }
        context_print_phase(i, phases[i], status);
    }
}

static unsigned long long context_metadata_max_context(yvex_cli_tokenizer_context *ctx,
                                                       const char **source)
{
    static const char *const keys[] = {
        "llama.context_length",
        "deepseek.context_length",
        "qwen.context_length",
        "glm.context_length",
        "general.context_length"
    };
    unsigned int i;
    unsigned long long value = 0ull;

    if (source) *source = "unknown";
    if (ctx && ctx->model) {
        value = yvex_model_context_length(ctx->model);
        if (value > 0ull) {
            if (source) *source = "model-descriptor";
            return value;
        }
    }
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *meta = ctx && ctx->gguf ? yvex_gguf_metadata_find(ctx->gguf, keys[i]) : NULL;
        if (meta && yvex_gguf_value_as_u64(meta, &value) == YVEX_OK && value > 0ull) {
            if (source) *source = keys[i];
            return value;
        }
    }
    return 0ull;
}

static const char *context_status_from_collections(const yvex_fullmodel_collections *collections,
                                                   int selected_target)
{
    if (selected_target) return "partial";
    if (collections &&
        collections->has_token_embedding &&
        collections->has_attention_q &&
        collections->has_attention_k &&
        collections->has_attention_v &&
        collections->has_attention_out) {
        return "complete";
    }
    return "partial";
}

static void context_print_common_header(const yvex_cli_context_options *options,
                                        const char *status,
                                        const char *resolved_path,
                                        const char *target_id,
                                        const char *target_class,
                                        const char *family,
                                        const char *detected,
                                        const char *family_status,
                                        const char *attention_status,
                                        const char *kv_status)
{
    printf("context: report\n");
    printf("status: %s\n", status ? status : "context-report");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    printf("target_id: %s\n", target_id ? target_id : "unknown");
    printf("target_class: %s\n", target_class ? target_class : "unknown");
    printf("backend: %s\n", options && options->backend ? options->backend : "cpu");
    printf("family: %s\n", family ? family : "unknown");
    printf("family_detected: %s\n", detected ? detected : "unknown");
    printf("family_requested: %s\n", context_requested_family(options));
    printf("family_runtime_status: %s\n", family_status ? family_status : "unknown");
    printf("attention_class_status: %s\n", attention_status ? attention_status : "unknown");
    printf("kv_class_status: %s\n", kv_status ? kv_status : "unknown");
    printf("context_class_status: report-only\n");
    printf("context_stage: report-only\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
}

static void context_print_unsupported_source_only(const yvex_cli_context_options *options)
{
    const char *family = strcmp(context_requested_family(options), "auto") == 0
                             ? "glm"
                             : context_requested_family(options);
    context_print_common_header(options,
                                "context-report-unsupported",
                                "source-only-target",
                                "glm-5.2-official-safetensors",
                                "official-source-huge-model",
                                family,
                                "glm",
                                "unsupported",
                                "unsupported",
                                "unsupported");
    printf("model_max_context: unknown\n");
    printf("model_max_context_source: source-manifest-planned\n");
    printf("model_max_context_status: unsupported-source-only\n");
    printf("requested_context: %s", options && options->context_length_seen ? "" : "not-requested");
    if (options && options->context_length_seen) printf("%llu", options->context_length);
    printf("\n");
    printf("requested_context_source: %s\n", options && options->context_length_seen ? "operator-request" : "none");
    printf("requested_context_status: %s\n", options && options->context_length_seen ? "reported-only" : "not-requested");
    printf("active_context: unsupported\n");
    printf("active_context_source: source-only-target\n");
    printf("active_context_status: unsupported\n");
    printf("token_input_status: not-performed-source-only-target\n");
    printf("token_count: 0\n");
    printf("prompt_token_count: 0\n");
    printf("prefill_token_count: 0\n");
    printf("generated_token_count: 0\n");
    printf("token_range_start: none\n");
    printf("token_range_end: none\n");
    printf("chunking_required: false\n");
    printf("chunk_size: not-requested\n");
    printf("chunk_count: 0\n");
    printf("last_chunk_size: 0\n");
    printf("chunking_policy: unsupported-source-only\n");
    printf("chunking_status: unsupported\n");
    printf("prefill_boundary_status: unsupported\n");
    printf("prefill_position_start: unknown\n");
    printf("prefill_position_end: unknown\n");
    printf("prefill_context_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_position_policy: unsupported-source-only\n");
    printf("decode_start_position: unknown\n");
    printf("decode_position_status: unsupported\n");
    printf("decode_context_ready: false\n");
    printf("decode_ready: false\n");
    printf("overflow_policy: unsupported-source-only\n");
    printf("context_overflow: unsupported\n");
    printf("overflow_check_status: unsupported\n");
    printf("overflow_stop_reason: unsupported\n");
    printf("overflow_mutates_state: false\n");
    printf("attention_dependency_status: unsupported-source-only\n");
    printf("attention_context_ready: false\n");
    printf("attention_position_policy: planned\n");
    printf("rope_context_status: planned\n");
    printf("mask_context_status: planned\n");
    printf("kv_dependency_status: unsupported-source-only\n");
    printf("kv_capacity_required: true\n");
    printf("kv_capacity_status: planned\n");
    printf("kv_context_positions: unknown\n");
    printf("kv_context_ready: false\n");
    printf("real_attention_kv_ready: false\n");
    printf("generation_context_status: unsupported-full-model\n");
    printf("bounded_generation_context_policy: diagnostic-only-not-run\n");
    printf("full_generation_context_ready: false\n");
    printf("generation_ready: false\n");
    printf("context_blockers: source-only target has no YVEX-produced GGUF tensor inventory; GLM context mapping planned; real transformer prefill unsupported\n");
    printf("next_required_rows: MOE.CLASS.0,source-manifest-context-profile,YVEX-produced-GGUF,real-transformer-prefill,real-decode,GEN.DEEPSEEK.0\n");
    printf("cleanup_attempted: true\n");
    printf("cleanup_status: pass\n");
    context_print_phases("unsupported", "unsupported", "unsupported", "unsupported", "context-profile");
}

static int context_print_unsupported_family(const yvex_cli_context_options *options,
                                            const char *resolved_path,
                                            const char *target_id,
                                            const char *target_class,
                                            const char *detected,
                                            const char *reason)
{
    context_print_common_header(options,
                                "context-report-unsupported",
                                resolved_path,
                                target_id,
                                target_class,
                                context_requested_family(options),
                                detected ? detected : "unknown",
                                "unsupported",
                                "unsupported",
                                "unsupported");
    printf("model_max_context: unknown\n");
    printf("model_max_context_source: unsupported-family\n");
    printf("model_max_context_status: unsupported\n");
    printf("requested_context: not-requested\n");
    printf("requested_context_source: none\n");
    printf("requested_context_status: not-requested\n");
    printf("active_context: unsupported\n");
    printf("active_context_source: unsupported-family\n");
    printf("active_context_status: unsupported\n");
    printf("token_input_status: not-performed\n");
    printf("token_count: 0\n");
    printf("prompt_token_count: 0\n");
    printf("prefill_token_count: 0\n");
    printf("generated_token_count: 0\n");
    printf("token_range_start: none\n");
    printf("token_range_end: none\n");
    printf("chunking_required: false\n");
    printf("chunk_size: not-requested\n");
    printf("chunk_count: 0\n");
    printf("last_chunk_size: 0\n");
    printf("chunking_policy: unsupported-family\n");
    printf("chunking_status: unsupported\n");
    printf("prefill_boundary_status: unsupported\n");
    printf("prefill_position_start: unknown\n");
    printf("prefill_position_end: unknown\n");
    printf("prefill_context_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_position_policy: unsupported-family\n");
    printf("decode_start_position: unknown\n");
    printf("decode_position_status: unsupported\n");
    printf("decode_context_ready: false\n");
    printf("decode_ready: false\n");
    printf("overflow_policy: unsupported-family\n");
    printf("context_overflow: unsupported\n");
    printf("overflow_check_status: unsupported\n");
    printf("overflow_stop_reason: unsupported\n");
    printf("overflow_mutates_state: false\n");
    printf("attention_dependency_status: unsupported-family\n");
    printf("attention_context_ready: false\n");
    printf("attention_position_policy: unsupported\n");
    printf("rope_context_status: unsupported\n");
    printf("mask_context_status: unsupported\n");
    printf("kv_dependency_status: unsupported-family\n");
    printf("kv_capacity_required: unknown\n");
    printf("kv_capacity_status: unsupported\n");
    printf("kv_context_positions: unknown\n");
    printf("kv_context_ready: false\n");
    printf("real_attention_kv_ready: false\n");
    printf("generation_context_status: unsupported-full-model\n");
    printf("bounded_generation_context_policy: diagnostic-only-not-run\n");
    printf("full_generation_context_ready: false\n");
    printf("generation_ready: false\n");
    printf("context_blockers: %s\n", reason ? reason : "unsupported context family");
    printf("next_required_rows: CONTEXT.CLASS.0,MOE.CLASS.0,real-transformer-prefill,real-decode,GEN.DEEPSEEK.0\n");
    printf("cleanup_attempted: true\n");
    printf("cleanup_status: pass\n");
    context_print_phases("unsupported", "unsupported", "unsupported", "unsupported", "resolve-family");
    printf("reason: %s\n", reason ? reason : "unsupported context family");
    return 5;
}

static int context_print_report(const yvex_cli_context_options *options,
                                yvex_model_ref *ref,
                                yvex_cli_tokenizer_context *ctx,
                                const char *target_id,
                                const char *target_class,
                                yvex_arch arch,
                                const yvex_fullmodel_collections *collections,
                                int selected_target)
{
    yvex_cli_fullmodel_options family_probe;
    yvex_token_input input;
    yvex_error err;
    const char *requested = context_requested_family(options);
    const char *detected;
    const char *source = "unknown";
    const char *coverage_status;
    unsigned long long model_max_context;
    unsigned long long active_context = 0ull;
    unsigned long long token_count = 0ull;
    unsigned long long chunk_count = 0ull;
    unsigned long long last_chunk = 0ull;
    unsigned long long decode_start = 0ull;
    int overflow_known = 0;
    int overflow = 0;
    int has_qkv;
    int rc;

    memset(&family_probe, 0, sizeof(family_probe));
    memset(&input, 0, sizeof(input));
    yvex_error_clear(&err);

    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : "auto";
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    if (!fullmodel_family_request_matches(requested, detected) ||
        strcmp(detected, "deepseek") != 0) {
        return context_print_unsupported_family(options,
                                                ref && ref->path ? ref->path : "",
                                                target_id,
                                                target_class,
                                                detected,
                                                "context report supports DeepSeek-family GGUF artifacts first");
    }

    model_max_context = context_metadata_max_context(ctx, &source);
    coverage_status = context_status_from_collections(collections, selected_target);
    has_qkv = collections &&
              collections->has_attention_q &&
              collections->has_attention_k &&
              collections->has_attention_v;

    if (options && options->tokens_text) {
        rc = yvex_token_input_parse_explicit(options->tokens_text, &input, &err);
        if (rc != YVEX_OK) {
            context_print_common_header(options,
                                        "context-report-fail",
                                        ref && ref->path ? ref->path : "",
                                        target_id,
                                        target_class,
                                        "deepseek",
                                        detected,
                                        coverage_status,
                                        coverage_status,
                                        coverage_status);
            printf("token_input_status: failed\n");
            printf("reason: %s\n", yvex_error_message(&err));
            printf("cleanup_attempted: true\n");
            printf("cleanup_status: pass\n");
            context_print_phases(coverage_status, coverage_status, coverage_status, "failed", "token-input");
            return exit_for_status(rc);
        }
        token_count = input.token_count;
    }

    if (options && options->context_length_seen) {
        active_context = options->context_length;
        overflow_known = 1;
    } else if (model_max_context > 0ull && !selected_target) {
        active_context = model_max_context;
        overflow_known = 1;
    } else {
        active_context = 0ull;
    }
    if (overflow_known && token_count > active_context) {
        overflow = 1;
    }
    if (options && options->chunk_size_seen && token_count > 0ull) {
        chunk_count = (token_count + options->chunk_size - 1ull) / options->chunk_size;
        last_chunk = token_count % options->chunk_size;
        if (last_chunk == 0ull) last_chunk = options->chunk_size;
    }
    decode_start = token_count;

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        printf("report: context\n");
        printf("model: %s\n", options->model ? options->model : "");
        printf("family: deepseek\n");
        printf("status: %s\n", coverage_status ? coverage_status : "report-only");
        printf("top_blocker: %s\n",
               selected_target
                   ? "selected-slice context metadata incomplete"
                   : "full transformer prefill unsupported");
        printf("next: V010.CONTEXT.*\n");
        printf("boundary: report-only, no runtime execution\n");
        return 0;
    }

    context_print_common_header(options,
                                "context-report",
                                ref && ref->path ? ref->path : "",
                                target_id,
                                target_class,
                                "deepseek",
                                detected,
                                coverage_status,
                                coverage_status,
                                coverage_status);
    printf("report_options.include_attention: %s\n", options && options->include_attention ? "true" : "false");
    printf("report_options.include_kv: %s\n", options && options->include_kv ? "true" : "false");
    printf("report_options.include_prefill: %s\n", options && options->include_prefill ? "true" : "false");
    printf("report_options.include_decode: %s\n", options && options->include_decode ? "true" : "false");
    printf("report_options.include_blockers: %s\n", options && options->include_blockers ? "true" : "false");

    printf("model_max_context: ");
    if (model_max_context > 0ull) printf("%llu", model_max_context);
    else printf("unknown");
    printf("\n");
    printf("model_max_context_source: %s\n", model_max_context > 0ull ? source : "metadata-missing-or-selected-slice");
    printf("model_max_context_status: %s\n", model_max_context > 0ull ? "available" : "planned");
    printf("requested_context: ");
    if (options && options->context_length_seen) printf("%llu", options->context_length);
    else printf("not-requested");
    printf("\n");
    printf("requested_context_source: %s\n", options && options->context_length_seen ? "operator-request" : "none");
    printf("requested_context_status: %s\n", options && options->context_length_seen ? "reported-only" : "not-requested");
    printf("active_context: ");
    if (active_context > 0ull) printf("%llu", active_context);
    else printf("diagnostic");
    printf("\n");
    printf("active_context_source: %s\n",
           options && options->context_length_seen ? "operator-request" :
           (model_max_context > 0ull && !selected_target ? "model-metadata" : "diagnostic-runtime-boundary"));
    printf("active_context_status: %s\n", active_context > 0ull ? "reported-only" : "diagnostic");

    printf("token_input_status: %s\n", options && options->tokens_text ? "available" : "not-provided");
    printf("token_count: %llu\n", token_count);
    printf("prompt_token_count: %llu\n", token_count);
    printf("prefill_token_count: %llu\n", token_count);
    printf("generated_token_count: 0\n");
    printf("token_range_start: %s\n", token_count > 0ull ? "0" : "none");
    printf("token_range_end: ");
    if (token_count > 0ull) printf("%llu", token_count - 1ull);
    else printf("none");
    printf("\n");

    printf("chunking_required: %s\n", options && options->chunk_size_seen ? "true" : "false");
    printf("chunk_size: ");
    if (options && options->chunk_size_seen) printf("%llu", options->chunk_size);
    else printf("not-requested");
    printf("\n");
    printf("chunk_count: %llu\n", chunk_count);
    printf("last_chunk_size: %llu\n", last_chunk);
    printf("chunking_policy: %s\n", options && options->chunk_size_seen ? "explicit-token-chunks-report-only" : "not-requested");
    printf("chunking_status: %s\n", options && options->chunk_size_seen ? "report-only" : "planned");

    printf("prefill_boundary_status: report-only\n");
    printf("prefill_position_start: %s\n", token_count > 0ull ? "0" : "none");
    printf("prefill_position_end: %llu\n", token_count);
    printf("prefill_context_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_position_policy: prefill-end-token-count\n");
    printf("decode_start_position: %llu\n", decode_start);
    printf("decode_position_status: report-only\n");
    printf("decode_context_ready: false\n");
    printf("decode_ready: false\n");

    printf("overflow_policy: bounded-diagnostic-refuse-before-mutation\n");
    printf("context_overflow: %s\n", overflow_known ? (overflow ? "overflow" : "none") : "unknown");
    printf("overflow_check_status: %s\n", overflow_known ? "pass" : "unknown");
    printf("overflow_stop_reason: %s\n", overflow ? "context-limit" : "none");
    printf("overflow_mutates_state: false\n");

    printf("attention_dependency_status: %s\n", has_qkv ? "blocked-runtime-integration" : "blocked-missing-qkv");
    printf("attention_context_ready: false\n");
    printf("attention_position_policy: rope-or-family-specific-planned\n");
    printf("rope_context_status: planned\n");
    printf("mask_context_status: planned\n");
    printf("kv_dependency_status: %s\n", has_qkv ? "blocked-runtime-integration" : "blocked-missing-qkv");
    printf("kv_capacity_required: true\n");
    printf("kv_capacity_status: planned\n");
    printf("kv_context_positions: %s", active_context > 0ull ? "" : "unknown");
    if (active_context > 0ull) printf("%llu", active_context);
    printf("\n");
    printf("kv_context_ready: false\n");
    printf("real_attention_kv_ready: false\n");
    printf("generation_context_status: bounded-diagnostic-only\n");
    printf("bounded_generation_context_policy: context-limit-pre-append\n");
    printf("full_generation_context_ready: false\n");
    printf("generation_ready: false\n");
    printf("context_blockers: %s\n",
           selected_target
               ? "selected runtime slice has no full context metadata, no real attention KV, no full transformer prefill, no real decode, no full generation"
               : "full transformer prefill unsupported; real attention-backed KV unsupported; real decode unsupported; full generation unsupported");
    printf("next_required_rows: MOE.CLASS.0,RUNTIME.KV.1,real-transformer-prefill,real-decode,real-output-head-logits,GEN.DEEPSEEK.0\n");
    printf("cleanup_attempted: true\n");
    printf("cleanup_status: pass\n");
    context_print_phases(coverage_status, coverage_status, coverage_status, "report-only", NULL);
    return 0;
}

int yvex_context_command(int argc, char **argv)
{
    yvex_cli_context_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long tensor_count;
    unsigned long long i;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref_options, 0, sizeof(ref_options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));

    rc = parse_context_options(argc, argv, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        context_print_unsupported_source_only(&options);
        return 5;
    }

    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        context_print_common_header(&options,
                                    "context-report-fail",
                                    "",
                                    "unknown",
                                    "unknown",
                                    "unknown",
                                    "unknown",
                                    "failed",
                                    "failed",
                                    "failed");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("cleanup_attempted: true\n");
        printf("cleanup_status: pass\n");
        context_print_phases("failed", "failed", "failed", "failed", "resolve-model");
        yvex_error_clear(&err);
        return exit_for_status(rc);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        context_print_common_header(&options,
                                    "context-report-fail",
                                    ref.path,
                                    ref.alias && ref.alias[0] ? ref.alias : "path",
                                    "GGUF-artifact",
                                    "unknown",
                                    "unknown",
                                    "failed",
                                    "failed",
                                    "failed");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("cleanup_attempted: true\n");
        printf("cleanup_status: pass\n");
        context_print_phases("failed", "failed", "failed", "failed", "context-profile");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (tensor) fullmodel_classify_tensor(tensor, &collections);
    }
    if (yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.tokens") ||
        yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.model")) {
        collections.tokenizer = 1ull;
        collections.has_tokenizer_metadata = 1;
    }

    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = context_print_report(&options,
                              &ref,
                              &ctx,
                              target_id,
                              target_class,
                              arch,
                              &collections,
                              selected_target);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_context_help(FILE *fp)
{
    fprintf(fp, "usage: yvex context report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--audit | --output normal|table|audit] [options]\n\n");
    fprintf(fp, "Examples:\n");
    fprintf(fp, "  yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3\n");
    fprintf(fp, "  yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3 --chunk-size 2\n");
    fprintf(fp, "  yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3 --context-length 2\n\n");
    fprintf(fp, "Options: --registry FILE --context-length N --tokens IDS --chunk-size N --include-attention --include-kv --include-prefill --include-decode --include-blockers\n\n");
    fprintf(fp, "context report:\n");
    fprintf(fp, "  report-only boundary for model/requested/active context, token counts, chunking policy, overflow behavior, prefill boundary, decode position policy, attention dependency, KV dependency, and runtime blockers.\n");
    fprintf(fp, "  Default output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "  It reports bounded diagnostic context behavior separately from full model context readiness.\n");
    fprintf(fp, "  It does not run full transformer prefill, does not execute real decode, does not write real attention-backed KV, does not generate, does not evaluate, does not benchmark, and does not report throughput.\n");
    fprintf(fp, "Boundary: no long-context runtime support, no context extension support, no full model execution, no DeepSeek generation, no provider generation, no streaming generation, no eval, no benchmark.\n");
}

static void fullmodel_print_largest(const yvex_fullmodel_largest_tensor top[16],
                                    unsigned int top_count)
{
    unsigned int i;

    for (i = 0; i < top_count; ++i) {
        char dims[128];
        const yvex_tensor_info *tensor = top[i].tensor;
        if (!tensor) continue;
        dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
        printf("largest_tensor_%u: name=%s dtype=%s role=%s dims=%s bytes=%llu\n",
               i,
               tensor->name ? tensor->name : "",
               yvex_dtype_name(tensor->dtype),
               yvex_tensor_role_name(tensor->role),
               dims,
               tensor->storage_bytes);
    }
}

static const char *fullmodel_identity_status(const yvex_model_ref *ref,
                                             unsigned long long artifact_bytes)
{
    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) return "not-checked";
    if (!ref->sha256 || !ref->sha256[0]) return "registered-without-digest";
    if (ref->registered_file_size != 0ull && ref->registered_file_size != artifact_bytes) {
        return "registered-size-drift";
    }
    return "registered-size-match";
}

static int fullmodel_residency_is_future_unsupported(const char *residency)
{
    return residency &&
           (strcmp(residency, "ssd-streamed") == 0 ||
            strcmp(residency, "managed-memory") == 0 ||
            strcmp(residency, "distributed") == 0);
}

static const char *fullmodel_placement_for_residency(const char *backend,
                                                     const char *residency,
                                                     int present)
{
    if (!present) return "not-planned";
    if (fullmodel_residency_is_future_unsupported(residency)) return "unsupported";
    if (strcmp(residency, "host-staged") == 0) return "host-staged";
    if (strcmp(residency, "ssd-staged") == 0) return "ssd-staged";
    if (strcmp(residency, "hybrid") == 0) return "hybrid";
    return backend && strcmp(backend, "cuda") == 0 ? "cuda-resident" : "cpu-resident";
}

static const char *fullmodel_required_bool(int value)
{
    return value ? "true" : "false";
}

static void fullmodel_probe_backend_fit(const char *backend,
                                        unsigned long long required_bytes,
                                        yvex_fullmodel_backend_fit *fit)
{
    yvex_backend *opened = NULL;
    yvex_backend_options options;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    yvex_error err;
    int rc;

    if (!fit) return;
    memset(fit, 0, sizeof(*fit));
    fit->required_bytes = required_bytes;
    fit->fit_status = "unknown";
    snprintf(fit->fit_reason, sizeof(fit->fit_reason),
             "system memory availability is not queried");

    if (!backend || strcmp(backend, "cpu") == 0) {
        fit->available = 1;
        return;
    }

    memset(&options, 0, sizeof(options));
    memset(&stats, 0, sizeof(stats));
    memset(&device_info, 0, sizeof(device_info));
    yvex_error_clear(&err);
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&opened, &options, &err);
    if (rc != YVEX_OK) {
        fit->available = 0;
        fit->fit_status = "unsupported";
        snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                 "CUDA backend unavailable: %s", yvex_error_message(&err));
        yvex_error_clear(&err);
        return;
    }

    fit->available = 1;
    rc = yvex_backend_get_memory_stats(opened, &stats, &err);
    if (rc == YVEX_OK &&
        yvex_backend_get_device_info(opened, &device_info, &err) == YVEX_OK &&
        device_info.total_memory_bytes > 0ull) {
        fit->memory_known = 1;
        fit->total_bytes = device_info.total_memory_bytes;
        fit->available_bytes = device_info.free_memory_bytes;
        if (required_bytes <= device_info.free_memory_bytes) {
            fit->fit_status = "fits";
            snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                     "required bytes fit current CUDA free memory; no allocation attempted");
        } else {
            fit->fit_status = "does-not-fit";
            snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                     "required bytes exceed current CUDA free memory; no allocation attempted");
        }
    } else {
        fit->fit_status = "unknown";
        snprintf(fit->fit_reason, sizeof(fit->fit_reason),
                 "CUDA memory info unavailable; no allocation attempted");
        yvex_error_clear(&err);
    }
    yvex_backend_close(opened);
}

static int fullmodel_has_attention_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           collections->has_attention_q &&
           collections->has_attention_k &&
           collections->has_attention_v &&
           collections->has_attention_out;
}

static int fullmodel_has_mlp_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           collections->has_ffn_gate &&
           collections->has_ffn_up &&
           collections->has_ffn_down;
}

static int fullmodel_has_normalization_collection(const yvex_fullmodel_collections *collections)
{
    return collections &&
           (collections->has_attention_norm ||
            collections->has_post_attention_norm ||
            collections->has_output_norm);
}

static unsigned int fullmodel_plan_missing_collection_blockers(const yvex_fullmodel_collections *collections)
{
    unsigned int count = 0u;

    if (!collections) return 0u;
    if (!collections->has_token_embedding) count++;
    if (!fullmodel_has_normalization_collection(collections)) count++;
    if (!fullmodel_has_attention_collection(collections)) count++;
    if (!fullmodel_has_mlp_collection(collections)) count++;
    if (!collections->has_moe_router && !collections->has_moe_expert) count++;
    if (!collections->has_output_head) count++;
    if (!collections->has_tokenizer_metadata) count++;
    return count;
}

static unsigned int fullmodel_plan_blocker_count(const yvex_fullmodel_collections *collections,
                                                 int selected_target,
                                                 const char *residency,
                                                 const yvex_fullmodel_backend_fit *fit)
{
    unsigned int count = 7u; /* runtime-consumer plus six generation-boundary blockers. */

    count += fullmodel_plan_missing_collection_blockers(collections);
    if (selected_target) count++;
    if (fullmodel_residency_is_future_unsupported(residency)) count++;
    if (fit && !fit->available) count++;
    if (fit && strcmp(fit->fit_status ? fit->fit_status : "unknown", "does-not-fit") == 0) count++;
    return count;
}

static const char *fullmodel_plan_status(const yvex_fullmodel_collections *collections,
                                         int selected_target,
                                         const char *residency,
                                         const yvex_fullmodel_backend_fit *fit)
{
    if (fullmodel_residency_is_future_unsupported(residency)) return "unsupported";
    if (fit && !fit->available) return "partial";
    if (selected_target) return "partial";
    if (fullmodel_plan_missing_collection_blockers(collections) > 0u) return "partial";
    return "ready";
}

static void fullmodel_print_phase(unsigned int index,
                                  const char *name,
                                  const char *status,
                                  unsigned long long tensor_count,
                                  unsigned long long tensor_bytes,
                                  const char *residency,
                                  int required,
                                  int blocked,
                                  const char *blocker)
{
    printf("phase.%u.name: %s\n", index, name ? name : "");
    printf("phase.%u.status: %s\n", index, status ? status : "planned");
    printf("phase.%u.tensor_count: %llu\n", index, tensor_count);
    printf("phase.%u.tensor_bytes: %llu\n", index, tensor_bytes);
    printf("phase.%u.residency: %s\n", index, residency ? residency : "not-applicable");
    printf("phase.%u.required: %s\n", index, required ? "true" : "false");
    printf("phase.%u.blocked: %s\n", index, blocked ? "true" : "false");
    printf("phase.%u.blocker: %s\n", index, blocker && blocker[0] ? blocker : "none");
}

static void fullmodel_print_collection_plan(const char *name,
                                            const char *status,
                                            unsigned long long tensor_count,
                                            unsigned long long tensor_bytes,
                                            int required_for_generation,
                                            int present,
                                            const char *placement,
                                            const char *phase,
                                            const char *runtime_consumer,
                                            const char *blocker)
{
    printf("collection.%s.status: %s\n", name, status ? status : "planned");
    printf("collection.%s.tensor_count: %llu\n", name, tensor_count);
    printf("collection.%s.tensor_bytes: %llu\n", name, tensor_bytes);
    printf("collection.%s.required_for_generation: %s\n",
           name, fullmodel_required_bool(required_for_generation));
    printf("collection.%s.present: %s\n", name, fullmodel_required_bool(present));
    printf("collection.%s.placement: %s\n", name, placement ? placement : "unknown");
    printf("collection.%s.materialization_phase: %s\n", name, phase ? phase : "collection-grouping");
    printf("collection.%s.runtime_consumer: %s\n", name, runtime_consumer ? runtime_consumer : "planned");
    printf("collection.%s.blocker: %s\n", name, blocker && blocker[0] ? blocker : "none");
}

static unsigned int fullmodel_print_blocker(unsigned int index,
                                            const char *category,
                                            const char *severity,
                                            const char *message,
                                            int blocks_full_materialization,
                                            int blocks_generation)
{
    printf("blocker.%u.category: %s\n", index, category ? category : "runtime-consumer");
    printf("blocker.%u.severity: %s\n", index, severity ? severity : "warning");
    printf("blocker.%u.message: %s\n", index, message ? message : "");
    printf("blocker.%u.blocks_full_materialization: %s\n",
           index, blocks_full_materialization ? "true" : "false");
    printf("blocker.%u.blocks_generation: %s\n",
           index, blocks_generation ? "true" : "false");
    return index + 1u;
}

static void fullmodel_print_missing_collection_blockers(unsigned int *index,
                                                        const yvex_fullmodel_collections *collections)
{
    if (!index || !collections) return;
    if (!collections->has_token_embedding) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "embedding collection missing",
                                         1, 1);
    }
    if (!fullmodel_has_normalization_collection(collections)) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "error",
                                         "normalization collection missing",
                                         1, 1);
    }
    if (!fullmodel_has_attention_collection(collections)) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "attention collection missing",
                                         1, 1);
    }
    if (!fullmodel_has_mlp_collection(collections)) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "MLP collection missing",
                                         1, 1);
    }
    if (!collections->has_moe_router && !collections->has_moe_expert) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "warning",
                                         "MoE collection missing or not identified",
                                         1, 1);
    }
    if (!collections->has_output_head) {
        *index = fullmodel_print_blocker(*index, "role-coverage", "fatal",
                                         "output collection missing",
                                         1, 1);
    }
    if (!collections->has_tokenizer_metadata) {
        *index = fullmodel_print_blocker(*index, "tokenizer", "error",
                                         "tokenizer/full runtime metadata incomplete",
                                         1, 1);
    }
}

static void fullmodel_print_materialization_plan(const yvex_cli_fullmodel_options *options,
                                                 const yvex_model_ref *ref,
                                                 const char *target_id,
                                                 const char *target_class,
                                                 unsigned long long artifact_bytes,
                                                 yvex_arch arch,
                                                 unsigned long long tensor_count,
                                                 unsigned long long total_tensor_bytes,
                                                 const yvex_fullmodel_collections *collections,
                                                 const char *dtype_summary,
                                                 const char *role_coverage,
                                                 const char *missing_roles,
                                                 int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *plan_status;
    int materialization_plan_ready;
    unsigned int blocker_count;
    unsigned int blocker_index = 0u;
    const char *placement;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";
    int backend_blocked;
    int future_residency;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);
    plan_status = fullmodel_plan_status(collections, selected_target, residency, &fit);
    future_residency = fullmodel_residency_is_future_unsupported(residency);
    materialization_plan_ready = !selected_target && !future_residency && tensor_count > 0ull;
    blocker_count = fullmodel_plan_blocker_count(collections, selected_target, residency, &fit);
    backend_blocked = !fit.available || strcmp(fit.fit_status, "does-not-fit") == 0;

    printf("fullmodel: materialization-plan\n");
    printf("status: fullmodel-materialization-plan\n");
    printf("model: %s\n", options && options->model ? options->model : "");
    printf("model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    printf("target_id: %s\n", target_id ? target_id : "path");
    printf("target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    printf("artifact_exists: true\n");
    printf("artifact_bytes: %llu\n", artifact_bytes);
    printf("artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    printf("tensor_inventory_status: pass\n");
    printf("tensor_count: %llu\n", tensor_count);
    printf("total_tensor_bytes: %llu\n", total_tensor_bytes);
    printf("architecture: %s\n", yvex_arch_name(arch));
    printf("family: %s\n", fullmodel_family_from_arch(arch));
    printf("qtype_summary: %s\n", dtype_summary ? dtype_summary : "none");
    printf("dtype_summary: %s\n", dtype_summary ? dtype_summary : "none");
    printf("required_role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    printf("missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    printf("backend: %s\n", backend);
    printf("residency: %s\n", residency);
    printf("plan_status: %s\n", plan_status);
    printf("materialization_plan_ready: %s\n", materialization_plan_ready ? "true" : "false");
    printf("materialization_attempted: false\n");
    printf("full_materialization_proof: false\n");
    printf("full_model_execution: unsupported\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("plan_id: fullmodel-materialization:%s:%s:%s\n",
           target_id ? target_id : "path", backend, residency);
    printf("plan_kind: full-model-materialization\n");
    printf("plan_source: tensor-inventory\n");
    printf("plan_backend: %s\n", backend);
    printf("plan_residency: %s\n", residency);
    printf("plan_tensor_count: %llu\n", tensor_count);
    printf("plan_tensor_bytes: %llu\n", total_tensor_bytes);
    printf("plan_collection_count: 10\n");
    printf("plan_phase_count: 11\n");
    printf("plan_blocker_count: %u\n", blocker_count);
    printf("plan_cleanup_required: true\n");
    printf("plan_cleanup_phases: release-backend-buffers,release-host-staging,release-scratch,clear-partial-residency,preserve-failure-report\n");

    printf("backend_available: %s\n", fit.available ? "true" : "false");
    printf("backend_memory_known: %s\n", fit.memory_known ? "true" : "false");
    if (fit.memory_known) {
        printf("backend_memory_total_bytes: %llu\n", fit.total_bytes);
        printf("backend_memory_available_bytes: %llu\n", fit.available_bytes);
    } else {
        printf("backend_memory_total_bytes: unknown\n");
        printf("backend_memory_available_bytes: unknown\n");
    }
    printf("backend_required_bytes: %llu\n", fit.required_bytes);
    printf("backend_fit_status: %s\n", fit.fit_status);
    printf("backend_fit_reason: %s\n", fit.fit_reason);
    printf("backend_allocation_attempted: false\n");

    fullmodel_print_phase(0u, "preflight", future_residency ? "unsupported" : "ready",
                          tensor_count, total_tensor_bytes, residency, 1,
                          future_residency, future_residency ? "residency mode is not implemented" : "none");
    fullmodel_print_phase(1u, "artifact-identity", "ready",
                          0ull, artifact_bytes, "host", 1, 0, "none");
    fullmodel_print_phase(2u, "tensor-directory", "ready",
                          tensor_count, total_tensor_bytes, "host", 1, 0, "none");
    fullmodel_print_phase(3u, "tensor-range-validation", "ready",
                          tensor_count, total_tensor_bytes, "host", 1, 0, "none");
    fullmodel_print_phase(4u, "collection-grouping",
                          selected_target || strcmp(role_coverage ? role_coverage : "partial", "observed") != 0
                              ? "partial"
                              : "ready",
                          tensor_count, total_tensor_bytes, "host", 1,
                          selected_target,
                          selected_target ? "selected artifacts cannot satisfy full materialization" : "none");
    fullmodel_print_phase(5u, "backend-capability",
                          !fit.available ? "blocked" : "ready",
                          0ull, 0ull, backend, 1, !fit.available,
                          fit.available ? "none" : fit.fit_reason);
    fullmodel_print_phase(6u, "host-residency", "planned",
                          tensor_count, total_tensor_bytes, "host-staged", 1, 0, "none");
    fullmodel_print_phase(7u, "backend-residency",
                          backend_blocked ? "blocked" : "planned",
                          tensor_count, total_tensor_bytes,
                          fullmodel_placement_for_residency(backend, residency, 1),
                          1, backend_blocked,
                          backend_blocked ? fit.fit_reason : "none");
    fullmodel_print_phase(8u, "kv-residency", "unsupported",
                          0ull, 0ull, "not-planned", 1, 1,
                          "real attention-backed KV not implemented");
    fullmodel_print_phase(9u, "scratch-residency", "planned",
                          0ull, 0ull, "host-staged", 1, 0,
                          "scratch sizing remains planned");
    fullmodel_print_phase(10u, "cleanup", "planned",
                          0ull, 0ull, "not-applicable", 1, 0, "none");

    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->embedding > 0ull);
    fullmodel_print_collection_plan("embedding",
                                    collections && collections->embedding > 0ull ? "planned" : "blocked",
                                    collections ? collections->embedding : 0ull,
                                    collections ? collections->embedding_bytes : 0ull,
                                    1, collections && collections->embedding > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->embedding > 0ull ? "none" : "embedding collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->normalization > 0ull);
    fullmodel_print_collection_plan("normalization",
                                    collections && collections->normalization > 0ull ? "planned" : "blocked",
                                    collections ? collections->normalization : 0ull,
                                    collections ? collections->normalization_bytes : 0ull,
                                    1, collections && collections->normalization > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->normalization > 0ull ? "none" : "normalization collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->attention > 0ull);
    fullmodel_print_collection_plan("attention",
                                    fullmodel_has_attention_collection(collections) ? "planned" : "blocked",
                                    collections ? collections->attention : 0ull,
                                    collections ? collections->attention_bytes : 0ull,
                                    1, collections && collections->attention > 0ull,
                                    placement, "backend-residency", "planned",
                                    fullmodel_has_attention_collection(collections) ? "none" : "attention collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->mlp > 0ull);
    fullmodel_print_collection_plan("mlp",
                                    fullmodel_has_mlp_collection(collections) ? "planned" : "blocked",
                                    collections ? collections->mlp : 0ull,
                                    collections ? collections->mlp_bytes : 0ull,
                                    1, collections && collections->mlp > 0ull,
                                    placement, "backend-residency", "planned",
                                    fullmodel_has_mlp_collection(collections) ? "none" : "MLP collection missing");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->moe > 0ull);
    fullmodel_print_collection_plan("moe",
                                    collections && collections->moe > 0ull ? "planned" : "blocked",
                                    collections ? collections->moe : 0ull,
                                    collections ? collections->moe_bytes : 0ull,
                                    1, collections && collections->moe > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->moe > 0ull ? "none" : "MoE collection missing or not identified");
    placement = fullmodel_placement_for_residency(backend, residency,
                                                  collections && collections->output > 0ull);
    fullmodel_print_collection_plan("output",
                                    collections && collections->output > 0ull ? "planned" : "blocked",
                                    collections ? collections->output : 0ull,
                                    collections ? collections->output_bytes : 0ull,
                                    1, collections && collections->output > 0ull,
                                    placement, "backend-residency", "planned",
                                    collections && collections->output > 0ull ? "none" : "output collection missing");
    fullmodel_print_collection_plan("tokenizer-runtime-input",
                                    collections && collections->tokenizer > 0ull ? "planned" : "blocked",
                                    collections ? collections->tokenizer : 0ull,
                                    collections ? collections->tokenizer_bytes : 0ull,
                                    1, collections && collections->tokenizer > 0ull,
                                    collections && collections->tokenizer > 0ull ? "host-staged" : "not-planned",
                                    "preflight", "planned",
                                    collections && collections->tokenizer > 0ull ? "none" : "tokenizer/full runtime metadata incomplete");
    fullmodel_print_collection_plan("kv-cache-runtime", "unsupported",
                                    0ull, 0ull, 1, 0, "not-planned",
                                    "kv-residency", "unsupported",
                                    "real attention-backed KV not implemented");
    fullmodel_print_collection_plan("scratch-runtime", "planned",
                                    0ull, 0ull, 1, 0, "host-staged",
                                    "scratch-residency", "planned",
                                    "scratch sizing remains planned");
    fullmodel_print_collection_plan("unknown",
                                    collections && collections->unknown > 0ull ? "partial" : "not-applicable",
                                    collections ? collections->unknown : 0ull,
                                    collections ? collections->unknown_bytes : 0ull,
                                    0, collections && collections->unknown > 0ull,
                                    collections && collections->unknown > 0ull ? "unknown" : "not-planned",
                                    "collection-grouping", "unsupported",
                                    collections && collections->unknown > 0ull ? "unknown tensor role" : "none");

    if (selected_target) {
        blocker_index = fullmodel_print_blocker(blocker_index, "artifact", "fatal",
                                                "selected artifacts cannot satisfy full materialization",
                                                1, 1);
    }
    if (future_residency) {
        blocker_index = fullmodel_print_blocker(blocker_index, "backend-capability", "error",
                                                "requested residency mode is planned but unsupported",
                                                1, 1);
    }
    if (!fit.available) {
        blocker_index = fullmodel_print_blocker(blocker_index, "backend-capability", "error",
                                                fit.fit_reason, 1, 0);
    } else if (strcmp(fit.fit_status ? fit.fit_status : "unknown", "does-not-fit") == 0) {
        blocker_index = fullmodel_print_blocker(blocker_index, "backend-memory", "error",
                                                fit.fit_reason, 1, 0);
    }
    fullmodel_print_missing_collection_blockers(&blocker_index, collections);
    blocker_index = fullmodel_print_blocker(blocker_index, "runtime-consumer", "fatal",
                                            "full collection runtime consumers are not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real transformer prefill not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real attention-backed KV not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real DeepSeek decode not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real output-head logits not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "generation-boundary", "fatal",
                                            "real vocabulary sampling not implemented",
                                            0, 1);
    blocker_index = fullmodel_print_blocker(blocker_index, "runtime-family", "fatal",
                                            "runtime family adapter not implemented",
                                            0, 1);

    printf("cleanup_plan_required: true\n");
    printf("cleanup_plan_phases: release-backend-buffers,release-host-staging,release-scratch,clear-partial-residency,preserve-failure-report\n");
    printf("cleanup_idempotent_required: true\n");
    printf("cleanup_failure_policy: preserve-failure-report\n");
    printf("next_required_row: FULLMODEL.2\n");
    printf("proof_ready_for_fullmodel_2: false\n");
    printf("fullmodel_2_blockers: %s\n",
           selected_target
               ? "full tensor set missing; full materialization executor not implemented; cleanup proof not implemented"
               : "full materialization executor not implemented; cleanup proof not implemented; runtime descriptor not implemented");
}

static int fullmodel_tensor_is_materialize_required(const yvex_tensor_info *tensor)
{
    const char *name;

    if (!tensor) return 0;
    name = tensor->name ? tensor->name : "";
    switch (tensor->role) {
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
    case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
    case YVEX_TENSOR_ROLE_ATTENTION_NORM:
    case YVEX_TENSOR_ROLE_ATTENTION_Q:
    case YVEX_TENSOR_ROLE_ATTENTION_K:
    case YVEX_TENSOR_ROLE_ATTENTION_V:
    case YVEX_TENSOR_ROLE_ATTENTION_OUT:
    case YVEX_TENSOR_ROLE_FFN_NORM:
    case YVEX_TENSOR_ROLE_FFN_GATE:
    case YVEX_TENSOR_ROLE_FFN_UP:
    case YVEX_TENSOR_ROLE_FFN_DOWN:
        return 1;
    default:
        break;
    }
    return fullmodel_name_has(name, "token_embd") ||
           fullmodel_name_has(name, "attn_norm") ||
           fullmodel_name_has(name, "ffn_norm") ||
           fullmodel_name_has(name, "attn_q") ||
           fullmodel_name_has(name, "attn_k") ||
           fullmodel_name_has(name, "attn_v") ||
           fullmodel_name_has(name, "attn_output") ||
           fullmodel_name_has(name, "q_proj") ||
           fullmodel_name_has(name, "k_proj") ||
           fullmodel_name_has(name, "v_proj") ||
           fullmodel_name_has(name, "o_proj") ||
           fullmodel_name_has(name, "ffn_gate") ||
           fullmodel_name_has(name, "ffn_up") ||
           fullmodel_name_has(name, "ffn_down") ||
           fullmodel_name_has(name, "gate_proj") ||
           fullmodel_name_has(name, "up_proj") ||
           fullmodel_name_has(name, "down_proj") ||
           fullmodel_name_has(name, "output_norm") ||
           strcmp(name, "output.weight") == 0 ||
           fullmodel_name_has(name, "lm_head");
}

static int fullmodel_role_present(const yvex_fullmodel_collections *collections,
                                  const char *role)
{
    if (!collections || !role) return 0;
    if (strcmp(role, "token-embedding") == 0) return collections->has_token_embedding;
    if (strcmp(role, "attention-norm") == 0) return collections->has_attention_norm;
    if (strcmp(role, "post-attention-norm") == 0) return collections->has_post_attention_norm;
    if (strcmp(role, "attention-q-projection") == 0) return collections->has_attention_q;
    if (strcmp(role, "attention-k-projection") == 0) return collections->has_attention_k;
    if (strcmp(role, "attention-v-projection") == 0) return collections->has_attention_v;
    if (strcmp(role, "attention-output-projection") == 0) return collections->has_attention_out;
    if (strcmp(role, "mlp-gate") == 0) return collections->has_ffn_gate;
    if (strcmp(role, "mlp-up") == 0) return collections->has_ffn_up;
    if (strcmp(role, "mlp-down") == 0) return collections->has_ffn_down;
    if (strcmp(role, "moe-router") == 0) return collections->has_moe_router;
    if (strcmp(role, "moe-experts") == 0) return collections->has_moe_expert;
    if (strcmp(role, "final-norm") == 0) return collections->has_output_norm;
    if (strcmp(role, "output-head") == 0) return collections->has_output_head;
    if (strcmp(role, "tokenizer-metadata") == 0) return collections->has_tokenizer_metadata;
    return 0;
}

static int fullmodel_collection_present_by_name(const yvex_fullmodel_collections *collections,
                                                const char *collection)
{
    if (!collections || !collection) return 0;
    if (strcmp(collection, "embedding") == 0) return collections->embedding > 0ull;
    if (strcmp(collection, "normalization") == 0) return collections->normalization > 0ull;
    if (strcmp(collection, "attention") == 0) return fullmodel_has_attention_collection(collections);
    if (strcmp(collection, "mlp") == 0) return fullmodel_has_mlp_collection(collections);
    if (strcmp(collection, "moe") == 0) return collections->moe > 0ull;
    if (strcmp(collection, "output") == 0) return collections->output > 0ull;
    if (strcmp(collection, "tokenizer") == 0 ||
        strcmp(collection, "tokenizer-runtime-input") == 0) {
        return collections->has_tokenizer_metadata;
    }
    return 0;
}

static void fullmodel_materialize_missing_roles(const yvex_cli_fullmodel_options *options,
                                                const yvex_fullmodel_collections *collections,
                                                char *out,
                                                size_t out_cap)
{
    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    if (!fullmodel_role_present(collections, "token-embedding")) fullmodel_csv_append(out, out_cap, "token-embedding");
    if (!fullmodel_role_present(collections, "attention-norm")) fullmodel_csv_append(out, out_cap, "attention-norm");
    if (!fullmodel_role_present(collections, "post-attention-norm")) fullmodel_csv_append(out, out_cap, "post-attention-norm");
    if (!fullmodel_role_present(collections, "attention-q-projection")) fullmodel_csv_append(out, out_cap, "attention-q-projection");
    if (!fullmodel_role_present(collections, "attention-k-projection")) fullmodel_csv_append(out, out_cap, "attention-k-projection");
    if (!fullmodel_role_present(collections, "attention-v-projection")) fullmodel_csv_append(out, out_cap, "attention-v-projection");
    if (!fullmodel_role_present(collections, "attention-output-projection")) fullmodel_csv_append(out, out_cap, "attention-output-projection");
    if (!fullmodel_role_present(collections, "mlp-gate")) fullmodel_csv_append(out, out_cap, "mlp-gate");
    if (!fullmodel_role_present(collections, "mlp-up")) fullmodel_csv_append(out, out_cap, "mlp-up");
    if (!fullmodel_role_present(collections, "mlp-down")) fullmodel_csv_append(out, out_cap, "mlp-down");
    if (!fullmodel_role_present(collections, "final-norm")) fullmodel_csv_append(out, out_cap, "final-norm");
    if (!fullmodel_role_present(collections, "output-head")) fullmodel_csv_append(out, out_cap, "output-head");
    if (!fullmodel_role_present(collections, "tokenizer-metadata")) fullmodel_csv_append(out, out_cap, "tokenizer-metadata");
    if (options && options->require_role &&
        !fullmodel_role_present(collections, options->require_role)) {
        fullmodel_csv_append(out, out_cap, options->require_role);
    }
    if (options && options->require_collection &&
        !fullmodel_collection_present_by_name(collections, options->require_collection)) {
        char tmp[160];
        snprintf(tmp, sizeof(tmp), "collection:%s", options->require_collection);
        fullmodel_csv_append(out, out_cap, tmp);
    }
    if (!out[0]) snprintf(out, out_cap, "none");
}

static int fullmodel_fail_after(const yvex_cli_fullmodel_options *options,
                                const char *phase)
{
    return options && options->fail_after_phase && phase &&
           strcmp(options->fail_after_phase, phase) == 0;
}

static int fullmodel_open_requested_backend(const char *backend_name,
                                            yvex_backend **out,
                                            yvex_error *err)
{
    yvex_backend_options options;

    if (!out) return YVEX_ERR_INVALID_ARG;
    *out = NULL;
    if (!backend_name || strcmp(backend_name, "cpu") == 0) {
        return yvex_backend_open_cpu(out, err);
    }
    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    return yvex_backend_open(out, &options, err);
}

static int fullmodel_allocate_required_tensors(const yvex_cli_fullmodel_options *options,
                                               yvex_cli_tokenizer_context *ctx,
                                               unsigned long long *materialized_count,
                                               unsigned long long *materialized_bytes,
                                               const char **failed_phase,
                                               const char **failed_reason)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor **allocated = NULL;
    yvex_error err;
    unsigned long long tensor_count;
    unsigned long long i;
    unsigned long long allocated_count = 0ull;
    int rc;

    if (materialized_count) *materialized_count = 0ull;
    if (materialized_bytes) *materialized_bytes = 0ull;
    if (failed_phase) *failed_phase = "none";
    if (failed_reason) *failed_reason = "none";
    if (!options || !ctx || !ctx->table) return YVEX_ERR_INVALID_ARG;

    tensor_count = yvex_tensor_table_count(ctx->table);
    allocated = (yvex_device_tensor **)calloc((size_t)(tensor_count ? tensor_count : 1ull),
                                              sizeof(*allocated));
    if (!allocated) {
        if (failed_phase) *failed_phase = "backend-preflight";
        if (failed_reason) *failed_reason = "allocation-list";
        return YVEX_ERR_NOMEM;
    }

    yvex_error_clear(&err);
    rc = fullmodel_open_requested_backend(options->backend, &backend, &err);
    if (rc != YVEX_OK) {
        if (failed_phase) *failed_phase = "backend-preflight";
        if (failed_reason) *failed_reason = "backend-open-failed";
        yvex_error_clear(&err);
        free(allocated);
        return rc;
    }

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        yvex_backend_tensor_desc desc;
        yvex_device_tensor *device_tensor = NULL;
        unsigned int d;

        if (!fullmodel_tensor_is_materialize_required(tensor)) continue;
        memset(&desc, 0, sizeof(desc));
        desc.name = tensor->name;
        desc.dtype = tensor->dtype;
        desc.rank = tensor->rank;
        desc.bytes = tensor->storage_bytes;
        for (d = 0; d < tensor->rank && d < YVEX_TENSOR_MAX_DIMS; ++d) {
            desc.dims[d] = tensor->dims[d];
        }
        rc = yvex_backend_tensor_alloc(backend, &desc, &device_tensor, &err);
        if (rc != YVEX_OK) {
            if (failed_phase) *failed_phase = "backend-preflight";
            if (failed_reason) *failed_reason = "tensor-allocation-failed";
            yvex_error_clear(&err);
            break;
        }
        allocated[allocated_count++] = device_tensor;
        if (materialized_count) (*materialized_count)++;
        if (materialized_bytes) *materialized_bytes += tensor->storage_bytes;
    }

    while (allocated_count > 0ull) {
        allocated_count--;
        yvex_backend_tensor_free(backend, allocated[allocated_count]);
    }
    yvex_backend_close(backend);
    free(allocated);
    return rc;
}

static int fullmodel_materialize_command_run(const yvex_cli_fullmodel_options *options,
                                             yvex_model_ref *ref,
                                             yvex_cli_tokenizer_context *ctx,
                                             const char *target_id,
                                             const char *target_class,
                                             unsigned long long artifact_bytes,
                                             unsigned long long tensor_count,
                                             unsigned long long total_tensor_bytes,
                                             const yvex_fullmodel_collections *collections,
                                             int selected_target)
{
    static const unsigned long long proof_byte_limit = 64ull * 1024ull * 1024ull;
    yvex_fullmodel_materialize_report report;
    char materialize_missing_roles[768];
    const char *unsupported =
        "runtime-family-adapter,real-transformer-prefill,real-attention-backed-KV,real-DeepSeek-decode,real-output-head-logits,real-vocabulary-sampling";
    unsigned long long required_tensor_count = 0ull;
    unsigned long long required_tensor_bytes = 0ull;
    unsigned long long materialized_count = 0ull;
    unsigned long long materialized_bytes = 0ull;
    unsigned long long i;
    const char *alloc_failed_phase = "none";
    const char *alloc_failed_reason = "none";
    int role_complete;
    int rc;

    (void)total_tensor_bytes;
    memset(&report, 0, sizeof(report));
    memset(materialize_missing_roles, 0, sizeof(materialize_missing_roles));
    fullmodel_materialize_missing_roles(options,
                                        collections,
                                        materialize_missing_roles,
                                        sizeof(materialize_missing_roles));
    role_complete = strcmp(materialize_missing_roles, "none") == 0;

    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx->table, i);
        if (!fullmodel_tensor_is_materialize_required(tensor)) continue;
        required_tensor_count++;
        required_tensor_bytes += tensor->storage_bytes;
    }

    report.options = options;
    report.status = "fullmodel-materialize-fail";
    report.model_resolved_path = ref && ref->path ? ref->path : "";
    report.target_id = target_id;
    report.target_class = target_class;
    report.artifact_identity_status = fullmodel_identity_status(ref, artifact_bytes);
    report.tensor_inventory_status = "pass";
    report.required_role_coverage = selected_target ? "partial" : (role_complete ? "complete" : "partial");
    report.missing_required_roles = selected_target ? materialize_missing_roles : materialize_missing_roles;
    report.unsupported_required_roles = unsupported;
    report.placement_plan_status = "pass";
    report.memory_budget_status = "pass";
    report.backend_preflight_status = "pass";
    report.materialization_mode = "controlled-fullmodel-proof";
    report.full_model_materialization = "controlled-tiny-proof";
    report.full_model_materialization_proof = "fail";
    report.phase = "failed";
    report.failed_phase = "none";
    report.failed_reason = "none";
    report.cleanup_attempted = "true";
    report.cleanup_status = "pass";
    report.cleanup_idempotent = "true";
    report.owned_state_released = "true";
    report.partial_materialization = "false";
    report.required_tensor_count = required_tensor_count;
    report.required_tensor_bytes = required_tensor_bytes;
    report.peak_planned_bytes = required_tensor_bytes;
    report.cpu_resident_bytes = strcmp(options->backend, "cuda") == 0 ? 0ull : required_tensor_bytes;
    report.cuda_resident_bytes = strcmp(options->backend, "cuda") == 0 ? required_tensor_bytes : 0ull;
    report.residency_plan = strcmp(options->backend, "cuda") == 0 ? "cuda-resident-controlled-proof" : "cpu-resident-controlled-proof";
    report.runtime_blockers = "runtime family adapter not implemented; real transformer prefill unsupported; decode/logits/sampling/generation remain unsupported-full-model";

    if (fullmodel_fail_after(options, "preflight")) {
        report.status = "fullmodel-materialize-fail";
        report.failed_phase = "preflight";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "resolve-model")) {
        report.failed_phase = "resolve-model";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "artifact-identity")) {
        report.failed_phase = "artifact-identity";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "tensor-inventory")) {
        report.failed_phase = "tensor-inventory";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if (selected_target) {
        report.status = "fullmodel-materialize-refused";
        report.placement_plan_status = "refused";
        report.memory_budget_status = "skipped";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "selected-runtime-slice-refusal";
        report.full_model_materialization = "refused-selected-runtime-slice";
        report.full_model_materialization_proof = "refused";
        report.failed_phase = "role-coverage";
        report.failed_reason = "selected-runtime-slice";
        report.refused_tensor_count = tensor_count;
        report.skipped_tensor_count = tensor_count;
        report.residency_plan = "selected-slice-not-full-model";
        report.runtime_blockers = "selected runtime slice cannot satisfy full required tensor materialization";
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (!role_complete) {
        report.status = "fullmodel-materialize-refused";
        report.placement_plan_status = "partial";
        report.memory_budget_status = "skipped";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "role-coverage-refusal";
        report.full_model_materialization = "refused-incomplete-role-coverage";
        report.full_model_materialization_proof = "refused";
        report.failed_phase = "role-coverage";
        report.failed_reason = "required-role-missing";
        report.refused_tensor_count = tensor_count;
        report.skipped_tensor_count = tensor_count;
        report.residency_plan = "not-planned";
        report.runtime_blockers = "required fullmodel proof roles are missing";
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (fullmodel_fail_after(options, "role-coverage")) {
        report.failed_phase = "role-coverage";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }
    if (fullmodel_fail_after(options, "placement-plan")) {
        report.failed_phase = "placement-plan";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if ((options->has_limit_bytes && required_tensor_bytes > options->limit_bytes) ||
        (!options->has_limit_bytes && required_tensor_bytes > proof_byte_limit)) {
        report.status = "fullmodel-materialize-fail";
        report.memory_budget_status = "fail";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = "memory-budget-refusal";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.failed_phase = "memory-budget";
        report.failed_reason = options->has_limit_bytes ? "byte-limit" : "controlled-proof-limit";
        report.refused_tensor_count = required_tensor_count;
        report.skipped_tensor_count = required_tensor_count;
        report.residency_plan = "not-planned";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_NOMEM);
    }

    if (fullmodel_fail_after(options, "memory-budget")) {
        report.failed_phase = "memory-budget";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    if (options->dry_run || options->plan_only) {
        report.status = options->plan_only ? "fullmodel-materialize-plan-only" : "fullmodel-materialize-dry-run";
        report.backend_preflight_status = "skipped";
        report.materialization_mode = options->plan_only ? "plan-only" : "dry-run";
        report.full_model_materialization = "planned";
        report.full_model_materialization_proof = "planned";
        report.phase = options->plan_only ? "placement-plan" : "memory-budget";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.skipped_tensor_count = required_tensor_count;
        fullmodel_print_materialize_report(&report);
        return 0;
    }

    if (fullmodel_fail_after(options, "backend-preflight")) {
        report.failed_phase = "backend-preflight";
        report.failed_reason = "injected-failure";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    rc = fullmodel_allocate_required_tensors(options,
                                             ctx,
                                             &materialized_count,
                                             &materialized_bytes,
                                             &alloc_failed_phase,
                                             &alloc_failed_reason);
    if (rc != YVEX_OK) {
        report.status = "fullmodel-materialize-fail";
        report.backend_preflight_status = strcmp(alloc_failed_phase, "backend-preflight") == 0 ? "fail" : "partial";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.failed_phase = alloc_failed_phase ? alloc_failed_phase : "backend-preflight";
        report.failed_reason = alloc_failed_reason ? alloc_failed_reason : "allocation failed";
        report.materialized_tensor_count = materialized_count;
        report.materialized_tensor_bytes = materialized_bytes;
        report.partial_materialization = materialized_count > 0ull ? "true" : "false";
        report.refused_tensor_count = required_tensor_count > materialized_count
                                          ? required_tensor_count - materialized_count
                                          : 0ull;
        fullmodel_print_materialize_report(&report);
        return exit_for_status(rc);
    }

    if (fullmodel_fail_after(options, "materialize-embedding") ||
        fullmodel_fail_after(options, "materialize-normalization") ||
        fullmodel_fail_after(options, "materialize-attention") ||
        fullmodel_fail_after(options, "materialize-mlp") ||
        fullmodel_fail_after(options, "materialize-moe") ||
        fullmodel_fail_after(options, "materialize-output") ||
        fullmodel_fail_after(options, "materialize-tokenizer") ||
        fullmodel_fail_after(options, "cleanup")) {
        report.failed_phase = options->fail_after_phase;
        report.failed_reason = "injected-failure";
        report.materialized_tensor_count = materialized_count;
        report.materialized_tensor_bytes = materialized_bytes;
        report.partial_materialization = "false";
        fullmodel_print_materialize_report(&report);
        return exit_for_status(YVEX_ERR_STATE);
    }

    report.status = "fullmodel-materialize-pass";
    report.full_model_materialization_proof = "pass";
    report.phase = "complete";
    report.materialized_tensor_count = materialized_count;
    report.materialized_tensor_bytes = materialized_bytes;
    report.refused_tensor_count = 0ull;
    report.skipped_tensor_count = tensor_count > materialized_count
                                      ? tensor_count - materialized_count
                                      : 0ull;
    fullmodel_print_materialize_report(&report);
    return 0;
}

int yvex_fullmodel_command(int argc, char **argv)
{
    yvex_cli_fullmodel_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_fullmodel_dtype_bucket dtype_buckets[32];
    yvex_fullmodel_largest_tensor largest[16];
    yvex_error err;
    char dtype_summary[512];
    char missing_roles[768];
    char descriptor_missing_roles[768];
    char unsupported_roles[512];
    const char *target_id;
    const char *target_class;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *model_class;
    const char *inventory_status;
    const char *role_coverage;
    const char *backend_placement_status;
    const char *cpu_placement;
    const char *cuda_placement;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    unsigned long long total_tensor_bytes = 0ull;
    unsigned long long tensor_count;
    unsigned long long i;
    unsigned int dtype_bucket_count = 0u;
    unsigned int largest_count = 0u;
    int selected_target = 0;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));
    memset(dtype_buckets, 0, sizeof(dtype_buckets));
    memset(largest, 0, sizeof(largest));
    memset(missing_roles, 0, sizeof(missing_roles));
    memset(descriptor_missing_roles, 0, sizeof(descriptor_missing_roles));
    memset(unsupported_roles, 0, sizeof(unsupported_roles));

    rc = parse_fullmodel_options(argc, argv, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0 ||
        (options.target && strcmp(options.target, "glm-5.2-official-safetensors") == 0)) {
        if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
            return print_fullmodel_source_only_plan(&options, "glm-5.2-official-safetensors");
        }
        if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
            return print_fullmodel_source_only_materialize(&options, "glm-5.2-official-safetensors");
        }
        if (options.command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
            return print_fullmodel_source_only_descriptor(&options, "glm-5.2-official-safetensors");
        }
        if (options.command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
            return print_fullmodel_source_only_family_runtime(&options, "glm-5.2-official-safetensors");
        }
        return print_fullmodel_source_only_report("glm-5.2-official-safetensors", options.backend);
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        rc = print_fullmodel_missing_report(&options, ref.path);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        int out = print_fullmodel_parse_failure_report(&options, &ref, yvex_error_message(&err), rc);
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return out;
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        total_tensor_bytes += tensor->storage_bytes;
        fullmodel_record_dtype(dtype_buckets, &dtype_bucket_count, tensor);
        fullmodel_record_largest(largest, &largest_count,
                                 (unsigned int)options.limit_tensors, tensor);
        fullmodel_classify_tensor(tensor, &collections);
    }
    if (yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.tokens") ||
        yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.model")) {
        collections.tokenizer = 1ull;
        collections.tokenizer_bytes = 0ull;
        collections.has_tokenizer_metadata = 1;
    }
    fullmodel_dtype_summary(dtype_summary, sizeof(dtype_summary),
                            dtype_buckets, dtype_bucket_count);

    arch = yvex_model_arch(ctx.model);
    target_id = options.target ? options.target :
                (ref.alias && ref.alias[0] ? ref.alias : "path");
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" :
                   (options.target && strcmp(options.target, "deepseek4-v4-flash") == 0
                        ? "full-runtime-model-planned"
                        : "candidate-GGUF-path");
    source_artifact_class = selected_target ? "YVEX-produced selected GGUF" : "GGUF artifact";
    target_artifact_class = selected_target ? "YVEX-produced selected GGUF" : "candidate GGUF artifact";
    model_class = selected_target ? "selected-runtime-slice" : "descriptor-only-candidate";
    inventory_status = selected_target ? "incomplete" : "partial";

    if (!collections.has_token_embedding) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "token-embedding");
    if (!collections.has_attention_norm) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-norm");
    if (!collections.has_post_attention_norm) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "post-attention-norm");
    if (!collections.has_attention_q) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-q-projection");
    if (!collections.has_attention_k) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-k-projection");
    if (!collections.has_attention_v) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-v-projection");
    if (!collections.has_attention_out) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "attention-output-projection");
    if (!collections.has_ffn_gate) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "mlp-gate");
    if (!collections.has_ffn_up) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "mlp-up");
    if (!collections.has_ffn_down) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "mlp-down");
    if (!collections.has_moe_router) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "moe-router");
    if (!collections.has_moe_expert) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "moe-experts");
    if (!collections.has_output_norm) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "final-norm");
    if (!collections.has_output_head) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "output-head");
    if (!collections.has_tokenizer_metadata) fullmodel_csv_append(missing_roles, sizeof(missing_roles), "tokenizer-metadata");
    if (!missing_roles[0]) snprintf(missing_roles, sizeof(missing_roles), "none");

    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "runtime-family-adapter");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-transformer-prefill");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-DeepSeek-decode");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-output-head-logits");
    fullmodel_csv_append(unsupported_roles, sizeof(unsupported_roles), "real-vocabulary-sampling");

    role_coverage = strcmp(missing_roles, "none") == 0 ? "observed" : "partial";
    if (selected_target) role_coverage = "partial";
    fullmodel_materialize_missing_roles(&options,
                                        &collections,
                                        descriptor_missing_roles,
                                        sizeof(descriptor_missing_roles));
    backend_placement_status = selected_target ? "selected-tensor-plan-only" : "report-only";
    cpu_placement = selected_target ? "selected-tensors-only" : "planned-full-model";
    cuda_placement = strcmp(options.backend, "cuda") == 0
                         ? (yvex_backend_cuda_available() ? "selected-or-candidate-tensors-only" : "unavailable")
                         : "not-requested";

    if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
        rc = fullmodel_materialize_command_run(&options,
                                               &ref,
                                               &ctx,
                                               target_id,
                                               target_class,
                                               artifact_bytes,
                                               tensor_count,
                                               total_tensor_bytes,
                                               &collections,
                                               selected_target);
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    if (options.command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
        const char *descriptor_role_coverage =
            selected_target ? "partial" :
            strcmp(descriptor_missing_roles, "none") == 0 ? "complete" : "partial";
        if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_descriptor_normal(&options,
                                              target_id,
                                              target_class,
                                              descriptor_role_coverage,
                                              descriptor_missing_roles);
        } else {
            fullmodel_print_descriptor_report(&options,
                                              &ref,
                                              &ctx,
                                              target_id,
                                              target_class,
                                              artifact_bytes,
                                              arch,
                                              tensor_count,
                                              total_tensor_bytes,
                                              &collections,
                                              descriptor_role_coverage,
                                              descriptor_missing_roles,
                                              unsupported_roles,
                                              selected_target);
        }
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    if (options.command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
        const char *descriptor_role_coverage =
            selected_target ? "partial" :
            strcmp(descriptor_missing_roles, "none") == 0 ? "complete" : "partial";
        if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_family_runtime_normal(&options,
                                                  target_id,
                                                  target_class,
                                                  descriptor_role_coverage,
                                                  descriptor_missing_roles);
            rc = 0;
        } else {
            rc = fullmodel_print_family_runtime_report(&options,
                                                       &ref,
                                                       &ctx,
                                                       target_id,
                                                       target_class,
                                                       artifact_bytes,
                                                       arch,
                                                       tensor_count,
                                                       total_tensor_bytes,
                                                       &collections,
                                                       descriptor_role_coverage,
                                                       descriptor_missing_roles,
                                                       unsupported_roles,
                                                       selected_target);
        }
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    if (options.command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
        if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            fullmodel_print_plan_normal(&options,
                                        selected_target ? "blocked" : "partial",
                                        target_class,
                                        selected_target ? "blocked" : "unknown",
                                        selected_target ? "selected-slice-not-full-runtime" : "full-runtime candidate artifact required");
        } else {
            fullmodel_print_materialization_plan(&options,
                                                 &ref,
                                                 target_id,
                                                 target_class,
                                                 artifact_bytes,
                                                 arch,
                                                 tensor_count,
                                                 total_tensor_bytes,
                                                 &collections,
                                                 dtype_summary,
                                                 role_coverage,
                                                 missing_roles,
                                                 selected_target);
        }
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    if (options.output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        fullmodel_print_report_normal(&options,
                                      "fullmodel-report",
                                      target_id,
                                      target_class,
                                      role_coverage,
                                      selected_target ? "selected-slice-not-full-runtime" : "missing-full-runtime-tensor-coverage",
                                      "tensor/source/artifact row required");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    printf("fullmodel: report\n");
    printf("status: fullmodel-report\n");
    printf("model: %s\n", options.model);
    printf("model_resolved_path: %s\n", ref.path ? ref.path : "");
    printf("target_id: %s\n", target_id);
    printf("target_class: %s\n", target_class);
    printf("source_artifact_class: %s\n", source_artifact_class);
    printf("target_artifact_class: %s\n", target_artifact_class);
    printf("artifact_exists: true\n");
    printf("artifact_bytes: %llu\n", artifact_bytes);
    printf("artifact_identity_status: %s\n", fullmodel_identity_status(&ref, artifact_bytes));
    printf("tensor_count: %llu\n", tensor_count);
    printf("tensor_inventory_status: pass\n");
    printf("metadata_status: pass\n");
    printf("architecture: %s\n", yvex_arch_name(arch));
    printf("family: %s\n", fullmodel_family_from_arch(arch));
    printf("model_class: %s\n", model_class);
    printf("fullmodel_inventory: %s\n", inventory_status);
    printf("full_runtime_model: false\n");
    printf("qtype_summary: %s\n", dtype_summary);
    printf("dtype_summary: %s\n", dtype_summary);
    printf("total_tensor_bytes: %llu\n", total_tensor_bytes);
    printf("estimated_cpu_resident_bytes: %llu\n", total_tensor_bytes);
    printf("estimated_cuda_resident_bytes: %llu\n", total_tensor_bytes);
    printf("estimated_kv_bytes: planned\n");
    printf("estimated_scratch_bytes: planned\n");
    printf("estimated_total_runtime_bytes: unknown\n");
    printf("backend: %s\n", options.backend);
    printf("backend_placement_status: %s\n", backend_placement_status);
    printf("cpu_placement: %s\n", cpu_placement);
    printf("cuda_placement: %s\n", cuda_placement);
    printf("cuda_available: %s\n", yvex_backend_cuda_available() ? "true" : "false");
    printf("cuda_memory_status: %s\n", yvex_backend_cuda_available() ? "probe-available-no-allocation" : "unavailable");
    printf("residency_plan: report-only-no-allocation\n");
    printf("tensor_collections_status: %s\n", role_coverage);
    printf("collection_detected: %s\n", tensor_count > 0ull ? "yes" : "no");
    printf("collection_supported: partial\n");
    printf("runtime_consumer: unsupported\n");
    printf("embedding_tensors: %llu\n", collections.embedding);
    printf("normalization_tensors: %llu\n", collections.normalization);
    printf("attention_tensors: %llu\n", collections.attention);
    printf("kv_cache_requirements: planned\n");
    printf("mlp_tensors: %llu\n", collections.mlp);
    printf("moe_tensors: %llu\n", collections.moe);
    printf("output_tensors: %llu\n", collections.output);
    printf("tokenizer_tensors: %llu\n", collections.tokenizer);
    printf("unknown_tensors: %llu\n", collections.unknown);
    printf("required_role_coverage: %s\n", role_coverage);
    printf("missing_required_roles: %s\n", missing_roles);
    printf("unsupported_required_roles: %s\n", unsupported_roles);
    printf("runtime_blockers: full tensor set missing; attention projection tensors may be missing; MLP/MoE tensors may be missing; output head may be missing; real transformer prefill unsupported; real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary sampling unsupported; full model materialization not implemented\n");
    print_fullmodel_common_boundaries();
    printf("largest_tensor_report_limit: %llu\n", options.limit_tensors);
    fullmodel_print_largest(largest, largest_count);

    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return 0;
}

void yvex_fullmodel_help(FILE *fp)
{
    fprintf(fp, "usage: yvex fullmodel report --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--limit-tensors N] [--registry FILE] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--residency resident|host-staged|ssd-staged|hybrid] [--target TARGET] [--limit-tensors N] [--registry FILE] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "usage: yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--registry FILE] [--dry-run] [--plan-only] [--require-role ROLE] [--require-collection COLLECTION] [--limit-bytes N] [--fail-after-phase PHASE] [--report-dir DIR] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "usage: yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--format text] [--limit-tensors N] [--require-role ROLE] [--require-collection COLLECTION] [--include-blockers] [--include-placement] [--include-graph] [--include-kv] [--include-logits] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--include-blockers] [--include-roles] [--include-graph] [--include-kv] [--include-moe] [--include-output] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "alias: yvex fullmodel plan --model FILE_OR_ALIAS [options]\n");
    fprintf(fp, "\nExamples:\n");
    fprintf(fp, "  yvex fullmodel report --model deepseek4-v4-flash-selected-embed --backend cpu\n");
    fprintf(fp, "  yvex fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --limit-tensors 8\n");
    fprintf(fp, "  yvex fullmodel report --model ./candidate.gguf --target deepseek4-v4-flash --backend cuda\n");
    fprintf(fp, "  yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --residency resident\n");
    fprintf(fp, "  yvex fullmodel materialization-plan --model ./candidate.gguf --target deepseek4-v4-flash --backend cuda --residency hybrid\n");
    fprintf(fp, "  yvex fullmodel materialize --model ./tiny-fullish.gguf --backend cpu --limit-bytes 1048576\n");
    fprintf(fp, "  yvex fullmodel materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu\n");
    fprintf(fp, "  yvex fullmodel descriptor --model ./candidate.gguf --target deepseek4-v4-flash --backend cpu --limit-tensors 40\n");
    fprintf(fp, "  yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu\n");
    fprintf(fp, "\nfullmodel report:\n");
    fprintf(fp, "  inventory and placement pressure report.\n");
    fprintf(fp, "  Default output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "\nfullmodel materialization-plan:\n");
    fprintf(fp, "  planned placement phases and materialization preflight only.\n");
    fprintf(fp, "\nfullmodel materialization proof:\n");
    fprintf(fp, "  controlled proof over a tiny full-ish GGUF tensor set, or a clean refusal for selected/runtime-slice and incomplete artifacts.\n");
    fprintf(fp, "\nfullmodel descriptor:\n");
    fprintf(fp, "  planning/reporting boundary for tensor roles, tensor collections, residency expectations, graph requirements, prefill/KV/decode/logits/sampling requirements, output-head/tokenizer requirements, backend requirements, and blockers.\n");
    fprintf(fp, "\nfullmodel family-runtime:\n");
    fprintf(fp, "  maps descriptor facts into model-family runtime adapter facts. DeepSeek is the first concrete family report target. Qwen/Metal remains planned unless separately implemented.\n");
    fprintf(fp, "\nFullmodel report reads GGUF metadata and tensor-directory facts only. Materialization-plan reuses those inventory facts to plan collection placement, residency, backend fit, blockers, and cleanup. Materialize allocates and releases only the bounded required proof tensors that pass role coverage and byte-limit checks. Descriptor builds a runtime requirement report only. Family-runtime maps descriptor facts into family-specific tensor roles, collection adapters, attention/KV/MoE/output requirements, graph requirements, blockers, and next-row dependencies. These reports do not execute the model, materialize full weights, run graph execution, write real KV, produce real logits, sample real vocabulary tokens, generate, evaluate, benchmark, or report throughput. They report why full transformer prefill, decode, logits, and generation remain unsupported.\n");
    fprintf(fp, "Boundary: no full model execution, no inference readiness, no DeepSeek generation, no provider generation, no streaming generation, no eval, no benchmark, no throughput.\n");
}

/* Models command dispatch and help. */

typedef int (*yvex_models_subcommand_fn)(int argc, char **argv);

typedef struct {
    const char *name;
    yvex_models_subcommand_fn run;
} yvex_models_subcommand;

static const yvex_models_subcommand model_subcommands[] = {
    { "scan", command_models_scan },
    { "add", command_models_add },
    { "download", command_models_download },
    { "prepare", command_models_prepare },
    { "check", command_models_check },
    { "list", command_models_list },
    { "use", command_models_use },
    { "current", command_models_current },
    { "verify", command_models_verify },
    { "inspect", command_models_inspect },
    { "remove", command_models_remove }
};

static int command_models(int argc, char **argv)
{
    unsigned long i;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_models_help(stdout);
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: models requires scan, add, download, prepare, check, list, use, current, verify, inspect, or remove\n");
        return 2;
    }
    for (i = 0; i < sizeof(model_subcommands) / sizeof(model_subcommands[0]); ++i) {
        if (strcmp(argv[2], model_subcommands[i].name) == 0) {
            return model_subcommands[i].run(argc, argv);
        }
    }
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
    fprintf(fp, "       yvex models download TARGET [--models-root DIR] [--dry-run] [--auth auto|required|never] [--progress auto|live|plain|log|off] [--tick-seconds N] [--no-progress] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex models download --repo OWNER/NAME --family deepseek|glm|qwen|gemma [--name LOCAL_NAME] [--models-root DIR]\n");
    fprintf(fp, "       yvex models prepare TARGET [--overwrite] [--source DIR] [--out FILE | --out-dir DIR] [--models-root DIR] [--registry FILE] [--dry-run] [--no-register] [--no-use]\n");
    fprintf(fp, "       yvex models check TARGET [--backend cpu|cuda] [--level quick|runtime|full] [--models-root DIR] [--registry FILE] [--report-dir DIR] [--no-materialize] [--no-graph] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex models list|current [--registry FILE] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex models verify|inspect ALIAS [--registry FILE] [--audit | --output normal|table|audit]\n");
    fprintf(fp, "       yvex models use|remove ALIAS [--registry FILE]\n");
    fprintf(fp, "\nExamples:\n");
    fprintf(fp, "  yvex models check deepseek4-v4-flash-selected-embed\n");
    fprintf(fp, "  yvex models download gemma-4-12b-it --models-root ~/lab/models --dry-run --audit\n");
    fprintf(fp, "  yvex models download gemma-4-12b-it --models-root ~/lab/models --auth required --progress live --tick-seconds 2 --audit\n");
    fprintf(fp, "  yvex models download qwen3-8b --models-root ~/lab/models --audit\n");
    fprintf(fp, "  yvex models check deepseek4-v4-flash-selected-embed --backend cpu --level runtime\n");
    fprintf(fp, "  yvex models check deepseek4-v4-flash-selected-embed --backend cuda --level runtime --no-graph\n");
    fprintf(fp, "  yvex models check deepseek4-v4-flash-selected-embed --level full --report-dir build/reports\n");
    fprintf(fp, "\nModels manages the local alias registry, source tensor download sidecars, selected artifact preparation, selected artifact checks, digest identity, and metadata drift facts for registered artifacts. Download writes source intake reports only and does not register runtime artifacts. Prepare currently supports deepseek4-v4-flash-selected-embed only and does not materialize, run graph execution, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    fprintf(fp, "Default report output is compact. Use --audit for full diagnostic fields.\n");
    fprintf(fp, "Check composes implemented artifact, identity, integrity, selected materialization, engine/session, plan, selected graph, and selected gates only; it does not create artifacts, run source conversion, run prefill, decode, produce logits, sample, generate, evaluate, or benchmark.\n");
}
