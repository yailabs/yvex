/*
 * yvex_model_target_report.h - typed model-target report API.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   model-target report request and report value contracts.
 *
 * Does not own:
 *   CLI argv parsing, command dispatch, CLI rendering, stdout/stderr writing,
 *   runtime execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   reports package existing model-target facts as typed values; owned text
 *   segments are released with the report close helper.
 *
 * Boundary:
 *   model-target reports are report-only facts and do not create quantization,
 *   artifact emission, runtime, generation, benchmark, or release readiness.
 */
#ifndef YVEX_MODEL_TARGET_REPORT_H
#define YVEX_MODEL_TARGET_REPORT_H

#include <stddef.h>

#include <yvex/error.h>

typedef enum {
    YVEX_MODEL_TARGET_OUTPUT_NORMAL = 0,
    YVEX_MODEL_TARGET_OUTPUT_TABLE,
    YVEX_MODEL_TARGET_OUTPUT_AUDIT,
    YVEX_MODEL_TARGET_OUTPUT_JSON
} yvex_model_target_output_mode;

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
    yvex_model_target_command_kind kind;
    yvex_model_target_output_mode mode;
    int argc;
    char **argv;
} yvex_model_target_request;

typedef struct {
    yvex_model_target_command_kind kind;
    yvex_model_target_output_mode mode;
    const char *status;
    char *primary_text;
    char *diagnostic_text;
    size_t primary_len;
    size_t diagnostic_len;
    int exit_code;
} yvex_model_target_report;

int yvex_model_target_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err);

int yvex_model_target_help_report_build(yvex_model_target_report *report,
                                        yvex_error *err);

void yvex_model_target_report_close(yvex_model_target_report *report);

#endif
