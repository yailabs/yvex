/*
 * report.h - typed model-target report API.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   model-target report request and report value contracts.
 *
 * Does not own:
 *   CLI argument parsing, command dispatch, CLI rendering, stdout/stderr
 *   writing, runtime execution, generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   reports package existing model-target facts as typed rows and fields; they
 *   do not carry pre-rendered command-output blobs.
 *
 * Boundary:
 *   model-target reports are report-only facts and do not create quantization,
 *   artifact emission, runtime, generation, benchmark, or release readiness.
 */
#ifndef YVEX_MODEL_TARGET_REPORT_H
#define YVEX_MODEL_TARGET_REPORT_H

#include <stddef.h>

#include <yvex/error.h>

typedef struct yvex_deepseek_v4_ir yvex_deepseek_v4_ir;
typedef struct yvex_deepseek_tensor_coverage yvex_deepseek_tensor_coverage;
typedef struct yvex_deepseek_gguf_map yvex_deepseek_gguf_map;

#define YVEX_MODEL_TARGET_TEXT_CAP 512u
#define YVEX_MODEL_TARGET_ROW_CAP 384u
#define YVEX_MODEL_TARGET_TABLE_COL_CAP 8u
#define YVEX_MODEL_TARGET_TABLE_ROW_CAP 128u

typedef enum {
    YVEX_MODEL_TARGET_OUTPUT_NORMAL = 0,
    YVEX_MODEL_TARGET_OUTPUT_TABLE,
    YVEX_MODEL_TARGET_OUTPUT_AUDIT,
    YVEX_MODEL_TARGET_OUTPUT_JSON
} yvex_model_target_render_mode;

typedef enum {
    YVEX_MODEL_TARGET_COMMAND_HELP = 0,
    YVEX_MODEL_TARGET_COMMAND_CLASSES,
    YVEX_MODEL_TARGET_COMMAND_LIST,
    YVEX_MODEL_TARGET_COMMAND_DECISION,
    YVEX_MODEL_TARGET_COMMAND_CANDIDATE,
    YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE,
    YVEX_MODEL_TARGET_COMMAND_QWEN_METAL,
    YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE,
    YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION,
    YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP,
    YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP,
    YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES,
    YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY,
    YVEX_MODEL_TARGET_COMMAND_INSPECT,
    YVEX_MODEL_TARGET_COMMAND_UNKNOWN
} yvex_model_target_command_kind;

typedef struct {
    char value[YVEX_MODEL_TARGET_TEXT_CAP];
} yvex_model_target_text_value;

typedef struct {
    unsigned int column_count;
    char columns[YVEX_MODEL_TARGET_TABLE_COL_CAP][YVEX_MODEL_TARGET_TEXT_CAP];
} yvex_model_target_table_row;

typedef struct {
    yvex_model_target_command_kind kind;
    yvex_model_target_render_mode mode;
    int help_requested;
    char target_id[128];
    char release[64];
    char family[32];
    char models_root[512];
    char source_path[512];
    char role[64];
    char gate[64];
    char candidate_kind[64];
    char output_contract[32];
    int include_hardware;
    int include_backend;
    int include_source;
    int include_blockers;
    int include_next;
    int include_examples;
    int include_candidates;
    int include_pressure_targets;
    int include_critical_path;
    int include_requirements;
    int include_paths;
    int output_json;
    int strict;
    int write_sidecar;
    char sidecar_path[512];
} yvex_model_target_request;

typedef struct {
    yvex_model_target_command_kind kind;
    yvex_model_target_render_mode mode;
    int help_requested;
    const char *status;
    char target_id[128];
    char family[32];
    char model[128];
    char target_class[128];
    char stage[128];
    char eligibility[128];
    char source_status[128];
    char artifact_status[128];
    char tensor_map_status[128];
    char qtype_policy_status[128];
    char runtime_status[128];
    char generation_status[128];
    char benchmark_status[128];
    char next_row[128];
    char boundary[256];
    char reason[256];
    yvex_model_target_text_value rows[YVEX_MODEL_TARGET_ROW_CAP];
    unsigned long row_count;
    yvex_model_target_text_value error_rows[64];
    unsigned long error_row_count;
    yvex_model_target_table_row table_rows[YVEX_MODEL_TARGET_TABLE_ROW_CAP];
    unsigned long table_row_count;
    yvex_deepseek_v4_ir *deepseek_architecture_ir;
    yvex_deepseek_tensor_coverage *deepseek_tensor_coverage;
    yvex_deepseek_gguf_map *deepseek_gguf_map;
    int exit_code;
} yvex_model_target_report;

int yvex_model_target_report_add_row(yvex_model_target_report *report,
                                     const char *fmt,
                                     ...);

int yvex_model_target_report_add_error(yvex_model_target_report *report,
                                       const char *fmt,
                                       ...);

int yvex_model_target_report_add_table_row(yvex_model_target_report *report,
                                           unsigned int column_count,
                                           const char *c0,
                                           const char *c1,
                                           const char *c2,
                                           const char *c3,
                                           const char *c4,
                                           const char *c5,
                                           const char *c6,
                                           const char *c7);

int yvex_model_target_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err);

int yvex_model_target_help_report_build(yvex_model_target_report *report,
                                        yvex_error *err);

void yvex_model_target_report_close(yvex_model_target_report *report);

#endif
