/*
 * yvex_backend_report.h - backend report facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   typed backend capability and qtype report records.
 *
 * Does not own:
 *   CLI rendering, graph execution, quantization, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   backend reports carry facts only and do not serialize operator output.
 *
 * Boundary:
 *   backend reports are not backend runtime generation.
 */
#ifndef YVEX_BACKEND_REPORT_H
#define YVEX_BACKEND_REPORT_H

#include <yvex/backend.h>

typedef enum {
    YVEX_BACKEND_REPORT_CAPABILITIES = 0,
    YVEX_BACKEND_REPORT_CUDA_INFO
} yvex_backend_report_kind;

typedef enum {
    YVEX_BACKEND_BUNDLE_NOT_APPLICABLE = 0,
    YVEX_BACKEND_BUNDLE_ABSENT,
    YVEX_BACKEND_BUNDLE_ADMITTED,
    YVEX_BACKEND_BUNDLE_REJECTED
} yvex_backend_bundle_admission;

typedef struct {
    yvex_backend_report_kind kind;
    yvex_backend_kind backend_kind;
} yvex_backend_report_request;

typedef struct {
    yvex_backend_report_kind kind;
    yvex_backend_kind backend_kind;
    yvex_backend_status backend_status;
    int exit_code;
    int available;
    int has_device_info;
    yvex_backend_device_info device_info;
    char device_name[128];
    yvex_backend_memory_stats memory;
    int capabilities[YVEX_BACKEND_CAP_OP_ATTENTION + 1];
    yvex_backend_capability_result variants[YVEX_BACKEND_VARIANT_COUNT];
    unsigned int variant_count;
    int context_available;
    yvex_backend_bundle_admission bundle_admission;
    yvex_backend_capability_reason bundle_reason;
    char reason[256];
} yvex_backend_report;

typedef struct {
    const char *kind;
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_backend_report_fact;

void yvex_backend_report_fact_init(yvex_backend_report_fact *fact,
                                   const char *kind,
                                   const char *status,
                                   const char *reason,
                                   const char *next_row);

int yvex_backend_report_build(const yvex_backend_report_request *request,
                              yvex_backend_report *report,
                              yvex_error *err);
const char *yvex_backend_bundle_admission_name(
    yvex_backend_bundle_admission admission);

#endif
