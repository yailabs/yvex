/* Owner: model.artifacts.internal (model.artifacts).
 * Owns: model-artifact references, reports, gates, and state publication.
 * Does not own: artifact byte ownership, runtime, or rendering.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: model-facing artifact control.
 * Purpose: provide the canonical model-facing artifact control contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_MODEL_ARTIFACT_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_MODEL_ARTIFACT_H_INCLUDED

#include <yvex/artifact.h>
#include <yvex/core.h>
#include <yvex/registry.h>
#include <yvex/internal/artifact.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gate contract. */
typedef struct {
    yvex_model_gate_status status;
    yvex_model_support_level support_level;
    const char *artifact_identity;
    const char *artifact_path;
    const char *profile_name;
    unsigned long long tensor_count;
    unsigned long long file_bytes;
    int complete_artifact_admitted;
    int materialization_input_ready;
    int execution_ready;
} yvex_model_complete_artifact_gate_fact;
int yvex_model_artifact_gate_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_model_complete_artifact_gate_fact *fact,
    yvex_error *err);

/* Private contract. */
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

/* Report contract. */
typedef enum {
    YVEX_MODEL_ARTIFACT_REPORT_STATUS = 0,
    YVEX_MODEL_ARTIFACT_REPORT_LIST,
    YVEX_MODEL_ARTIFACT_REPORT_CHECK
} yvex_model_artifact_report_kind;
typedef enum {
    YVEX_MODEL_ARTIFACT_RENDER_NORMAL = 0,
    YVEX_MODEL_ARTIFACT_RENDER_TABLE,
    YVEX_MODEL_ARTIFACT_RENDER_AUDIT
} yvex_model_artifact_render_mode;
typedef struct {
    yvex_model_artifact_report_kind kind;
    yvex_model_artifact_render_mode mode;
    const char *artifact_alias;
    const char *model_ref;
    const char *registry_path;
    const char *artifact_path;
    const char *expected_family;
    const char *expected_qprofile;
    const char *check_mode;
    int include_integrity;
    int include_materialization;
    int include_backend;
    int include_blockers;
    int write_sidecar;
    const char *sidecar_path;
} yvex_model_artifact_report_request;
typedef struct {
    const char *name;
    const char *status;
    const char *detail;
} yvex_model_artifact_row;
typedef struct {
    yvex_model_artifact_report_kind kind;
    const char *status;
    int exit_code;
    const char *alias;
    const char *family;
    const char *model;
    const char *artifact_class;
    const char *qprofile;
    const char *path;
    const char *sha256;
    unsigned long long file_size;
    const char *format;
    const char *architecture;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    const char *support_level;
    int execution_ready;
    const char *integrity_status;
    const char *materialization_status;
    const char *backend_status;
    yvex_model_artifact_row rows[32];
    unsigned long row_count;
    const char *reason;
    const char *boundary;
    const char *next_row;
} yvex_model_artifact_report;
int yvex_model_artifact_report_build(const yvex_model_artifact_report_request *request,
                                     yvex_model_artifact_report *report,
                                     yvex_error *err);
int yvex_model_artifact_report_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_model_artifact_report *report,
    yvex_error *err);

/* Write contract. */
int yvex_model_registry_write_json_file(const yvex_model_registry *registry,
                                        const char *path,
                                        yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_MODEL_ARTIFACT_H_INCLUDED */
