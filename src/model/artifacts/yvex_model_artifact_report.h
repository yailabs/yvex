/*
 * yvex_model_artifact_report.h - typed model artifact reports.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   model artifact report request and report structs.
 *
 * Does not own:
 *   command argument parsing, command dispatch, renderer formatting, stdout/stderr,
 *   explicit file writing, artifact emission, runtime generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   report requests contain typed values only and never raw command vectors.
 *
 * Boundary:
 *   reports are not artifact emission, runtime generation, benchmark evidence,
 *   or release readiness.
 */
#ifndef YVEX_MODEL_ARTIFACT_REPORT_H
#define YVEX_MODEL_ARTIFACT_REPORT_H

#include <yvex/yvex.h>

#include "src/artifact/yvex_artifact_roundtrip_gate.h"

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

#endif
