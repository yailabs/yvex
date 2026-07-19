/* Owner: src/backend
 * Owns: backend capability/qtype report construction, device/context facts, generated kernel-bundle admission, and
 *   exact operation projections.
 * Does not own: CLI rendering, graph execution, quantization, runtime generation, eval, benchmark, or release
 *   claims.
 * Invariants: one backend open produces one immutable report; report construction does not execute kernels and
 *   never infers support from context availability.
 * Boundary: backend reports do not promote backend runtime readiness.
 * Purpose: Build immutable backend reports from canonical capability and device facts.
 * Inputs: A backend request and caller-owned typed report storage.
 * Effects: Opens only the requested probe backend and releases it before returning.
 * Failure: Probe failures are represented as typed report facts without promoting support. */
#include <yvex/internal/backend.h>

#include <stdio.h>
#include <string.h>

/* Contract: returns the stable presentation name for typed bundle admission. */
/* Purpose: Return the canonical diagnostic label for bundle admission name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_bundle_admission_name(
    yvex_backend_bundle_admission admission)
{
    switch (admission) {
    case YVEX_BACKEND_BUNDLE_NOT_APPLICABLE: return "not-applicable";
    case YVEX_BACKEND_BUNDLE_ABSENT: return "absent";
    case YVEX_BACKEND_BUNDLE_ADMITTED: return "admitted";
    case YVEX_BACKEND_BUNDLE_REJECTED: return "rejected";
    }
    return "rejected";
}

/* Contract: copies one error reason into report-owned storage without IO. */
/* Purpose: Publish report set reason only within its admitted destination range. */
static void backend_report_set_reason(yvex_backend_report *report,
                                      const yvex_error *err)
{
    const char *message = yvex_error_message(err);

    snprintf(report->reason, sizeof(report->reason), "%s",
             message && message[0] ? message : "backend unavailable");
}

/* Contract: projects exact CUDA admission from one canonical variant result. */
/* Purpose: Publish report set admission only within its admitted destination range. */
static void backend_report_set_cuda_admission(
    yvex_backend_report *report,
    const yvex_backend_capability_result *result)
{
    report->context_available = result->context_available;
    report->bundle_reason = result->reason;
    if (result->kernel_bundle_available) {
        report->bundle_admission = YVEX_BACKEND_BUNDLE_ADMITTED;
    } else if (result->reason ==
               YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT) {
        report->bundle_admission = YVEX_BACKEND_BUNDLE_ABSENT;
    } else {
        report->bundle_admission = YVEX_BACKEND_BUNDLE_REJECTED;
    }
}

/*
 * Contract: opens one requested backend, copies capability/device facts, and
 * closes it. No kernel execution, operator output, or runtime claim occurs. */
/* Purpose: Project immutable report build facts into the typed reporting surface.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_report_build(const yvex_backend_report_request *request,
                              yvex_backend_report *report,
                              yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    unsigned int i;
    int rc;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_report_build",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    memset(&options, 0, sizeof(options));
    report->kind = request->kind;
    report->backend_kind = request->backend_kind;
    report->backend_status = YVEX_BACKEND_STATUS_UNSUPPORTED;
    report->bundle_admission = YVEX_BACKEND_BUNDLE_NOT_APPLICABLE;
    options.kind = request->backend_kind;

    rc = yvex_backend_open(&backend, &options, err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        report->exit_code = 5;
        backend_report_set_reason(report, err);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (rc != YVEX_OK) {
        return rc;
    }

    report->available = 1;
    report->backend_status = yvex_backend_status_of(backend);
    rc = yvex_backend_get_memory_stats(backend, &report->memory, err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_get_device_info(backend, &report->device_info, err);
    }
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        return rc;
    }
    report->has_device_info = 1;
    snprintf(report->device_name, sizeof(report->device_name), "%s",
             report->device_info.name ? report->device_info.name : "");
    report->device_info.name = report->device_name;

    for (i = 0; i <= (unsigned int)YVEX_BACKEND_CAP_OP_ATTENTION; ++i) {
        report->capabilities[i] =
            yvex_backend_supports(backend, (yvex_backend_capability)i);
    }
    if (request->backend_kind == YVEX_BACKEND_KIND_CUDA) {
        report->variant_count = (unsigned int)YVEX_BACKEND_VARIANT_COUNT;
        for (i = 0; i < report->variant_count; ++i) {
            rc = yvex_backend_query_capability(
                backend, (yvex_backend_operation_variant)i,
                &report->variants[i], err);
            if (rc != YVEX_OK) {
                yvex_backend_close(backend);
                return rc;
            }
        }
        backend_report_set_cuda_admission(
            report, &report->variants[YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32]);
    }
    yvex_backend_close(backend);
    yvex_error_clear(err);
    return YVEX_OK;
}
