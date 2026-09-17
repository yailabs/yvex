/* Share checked component buffers and bindings without importing family policy. */
#include <yvex/internal/component.h>

#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/core.h>
#include <yvex/internal/joint_transformer.h>
#include <yvex/internal/joint_program.h>
#include <yvex/internal/multimodal.h>
#include <yvex/internal/program_stage.h>
#include <yvex/internal/vision_program.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/transformer.h>

/* One retained program resource's measurements, not computational topology. */
typedef struct {
    unsigned long long host_arena_bytes, device_arena_bytes;
    unsigned long long request_prepared_bytes, condition_prepared_bytes;
    unsigned long long preparation_nanoseconds, allocation_count;
    char identity[YVEX_SHA256_HEX_CAP];
    int request_ready, condition_ready;
} component_prepared_summary;

typedef struct component_prepared_entry {
    struct component_prepared_entry *next;
    yvex_runtime_component_session *owner;
    yvex_program_prepared *program;
    component_prepared_summary summary;
    yvex_engine_resource_handle arena, request, condition;
    unsigned int borrowed;
} component_prepared_entry;

struct yvex_runtime_component_session {
    yvex_materialization_plan *plan;
    yvex_materialization_session *materialization;
    yvex_runtime_residency *residency;
    yvex_backend *backend;
    yvex_device_tensor *workspace;
    unsigned long long workspace_bytes, execution_transaction_leases, host_limit, device_limit;
    unsigned long long program_binding_count, program_binding_bytes;
    yvex_engine_resource_catalog *execution_resources;
    component_prepared_entry *prepared_entries;
    unsigned long long prepared_count, catalog_bytes;
    yvex_program_stage *program_stage;
    yvex_component_resource_summary resource_summary;
    char package_identity[YVEX_SHA256_HEX_CAP];
    char admission_identity[YVEX_SHA256_HEX_CAP];
    char active_request_identity[YVEX_SHA256_HEX_CAP];
    char retained_request_identity[YVEX_SHA256_HEX_CAP];
    yvex_runtime_residency_summary summary;
};

struct yvex_component_program_binding {
    yvex_component_execution execution;
    yvex_program_physical *program;
    yvex_program_kernel_parameter *parameters;
    size_t parameter_count;
    unsigned long long host_bytes;
    char identity[YVEX_SHA256_HEX_CAP];
};

static const yvex_materialized_tensor_binding *component_binding_find(
    const yvex_materialization_session *, const char *);
static int component_weight_bind(
    const yvex_materialization_session *, const yvex_runtime_residency *,
    const char *, yvex_component_encoded_weight *, yvex_error *);

static int component_prepared_totals(yvex_runtime_component_session *session,
    unsigned long long *host, unsigned long long *device)
{
    unsigned long long metadata;
    *host = *device = 0u;
    if (!yvex_core_u64_mul(session->prepared_count, sizeof(component_prepared_entry), &metadata) ||
        !yvex_core_u64_add(metadata, session->catalog_bytes, &metadata)) return 0;
    *host = metadata;
    for (component_prepared_entry *entry = session->prepared_entries; entry; entry = entry->next) {
        unsigned long long h, d;
        yvex_program_prepared_resources(entry->program, &h, &d, NULL);
        if (!yvex_core_u64_add(*host, h, host) || !yvex_core_u64_add(*device, d, device)) return 0;
    }
    session->resource_summary.metadata_host_bytes = metadata;
    return 1;
}

static int component_joint_prepared_release(void *context, yvex_error *err)
{
    component_prepared_entry *entry = context;
    int rc;
    if (!entry || !entry->owner || !entry->owner->backend || !entry->program) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource",
                       "prepared component resource ownership is unavailable");
        return YVEX_ERR_STATE;
    }
    rc = yvex_program_prepared_close(&entry->program, err);
    if (rc == YVEX_OK) memset(&entry->summary, 0, sizeof(entry->summary));
    return rc;
}

int yvex_runtime_component_session_close(yvex_runtime_component_session **session,
                                         yvex_error *err)
{
    yvex_runtime_component_session *owned;
    yvex_error cleanup;
    int rc = YVEX_OK, cleanup_rc;
    if (!session || !*session) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    owned = *session;
    if (owned->execution_transaction_leases || owned->program_binding_count) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-session",
                       "an execution transaction or compiled binding still retains component resources");
        return YVEX_ERR_STATE;
    }
    for (component_prepared_entry *entry = owned->prepared_entries; entry; entry = entry->next) {
        if (entry->borrowed) {
            yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-session",
                           "prepared execution resources remain borrowed");
            return YVEX_ERR_STATE;
        }
    }
    rc = yvex_program_stage_close(&owned->program_stage, err);
    if (rc != YVEX_OK) return rc;
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_runtime_resource_catalog_close(
        &owned->execution_resources, &cleanup);
    if (cleanup_rc != YVEX_OK) {
        if (err) *err = cleanup;
        return cleanup_rc;
    }
    /* A failed preparation may retain cleanup before catalog registration. */
    while (owned->prepared_entries) {
        component_prepared_entry *entry = owned->prepared_entries;
        rc = yvex_program_prepared_close(&entry->program, err);
        if (rc != YVEX_OK) return rc;
        owned->prepared_entries = entry->next;
        free(entry);
    }
    *session = NULL;
    if (owned->workspace) {
        yvex_backend_workspace_detach(owned->backend);
        yvex_error_clear(&cleanup);
        cleanup_rc = yvex_backend_tensor_release(owned->backend, &owned->workspace, &cleanup);
        if (cleanup_rc != YVEX_OK) {
            rc = cleanup_rc;
            if (err) *err = cleanup;
        }
    }
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_backend_close_checked(&owned->backend, &cleanup);
    if (cleanup_rc != YVEX_OK) {
        rc = cleanup_rc;
        if (err) *err = cleanup;
    }
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_runtime_residency_close(&owned->residency, &cleanup);
    if (cleanup_rc != YVEX_OK) {
        rc = cleanup_rc;
        if (err) *err = cleanup;
    }
    yvex_materialization_session_close(owned->materialization);
    yvex_materialization_plan_close(owned->plan);
    free(owned);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

static int component_session_prepare_workspace(
    yvex_runtime_component_session *session, unsigned long long bytes, yvex_error *err)
{
    yvex_backend_tensor_desc descriptor = {0};
    yvex_device_tensor *workspace = NULL;
    yvex_error primary, cleanup;
    int rc, cleanup_rc;
    if (!session || !session->backend || !bytes || bytes > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component-session.workspace",
                       "one bounded backend component workspace is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (session->workspace) {
        if (session->workspace_bytes >= bytes) {
            yvex_error_clear(err);
            return YVEX_OK;
        }
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-session.workspace",
                       "component workspace geometry is already sealed");
        return YVEX_ERR_STATE;
    }
    descriptor.name = "runtime-component-workspace";
    descriptor.dtype = YVEX_DTYPE_I8;
    descriptor.rank = 1u;
    descriptor.dims[0] = descriptor.bytes = bytes;
    rc = yvex_backend_tensor_alloc(session->backend, &descriptor, &workspace, err);
    if (rc == YVEX_OK)
        rc = yvex_backend_workspace_attach(session->backend, workspace, 1ull, err);
    if (rc != YVEX_OK) {
        primary = err ? *err : (yvex_error){0};
        yvex_error_clear(&cleanup);
        cleanup_rc = workspace
                         ? yvex_backend_tensor_release(session->backend, &workspace, &cleanup)
                         : YVEX_OK;
        if (cleanup_rc != YVEX_OK) {
            if (err) *err = cleanup;
            return cleanup_rc;
        }
        if (err) *err = primary;
        return rc;
    }
    session->workspace = workspace;
    session->workspace_bytes = bytes;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_component_session_open(
    yvex_runtime_component_session **out, const yvex_complete_artifact_admission *admission,
    const yvex_artifact *artifact, const yvex_gguf *gguf, const yvex_tensor_table *tensors,
    yvex_backend_kind backend_kind, unsigned long long maximum_host_bytes,
    unsigned long long maximum_device_bytes, yvex_error *err)
{
    yvex_runtime_component_session *session = NULL;
    yvex_backend_options backend_options = {0};
    yvex_materialization_options options;
    yvex_materialization_failure materialization_failure;
    yvex_runtime_residency_options residency_options = {0};
    yvex_runtime_residency_failure residency_failure;
    yvex_error primary, cleanup;
    int uploaded = 0, rc, cleanup_rc;
    if (out) *out = NULL;
    if (!out || !admission || !artifact || !gguf || !tensors ||
        (backend_kind != YVEX_BACKEND_KIND_CPU && backend_kind != YVEX_BACKEND_KIND_CUDA)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component-session",
                       "admitted component inputs and CPU or CUDA placement are required");
        return YVEX_ERR_INVALID_ARG;
    }
    session = (yvex_runtime_component_session *)calloc(1u, sizeof(*session));
    if (!session) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "runtime.component-session",
                       "component execution session allocation failed");
        return YVEX_ERR_NOMEM;
    }
    session->host_limit = maximum_host_bytes;
    session->device_limit = maximum_device_bytes;
    yvex_core_text_copy(session->package_identity,
                        sizeof(session->package_identity),
                        admission->artifact_identity);
    yvex_core_text_copy(session->admission_identity,
                        sizeof(session->admission_identity),
                        admission->admission_identity);
    yvex_materialization_options_default(&options);
    options.max_chunk_bytes = 64ull * 1024ull * 1024ull;
    if (maximum_host_bytes && maximum_host_bytes < options.max_chunk_bytes)
        options.max_chunk_bytes = (size_t)maximum_host_bytes;
    rc = yvex_materialization_plan_build(&session->plan, admission, artifact, gguf, tensors,
                                         NULL, &options, &materialization_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_materialization_session_open(&session->materialization, session->plan,
                                                artifact, &options, &materialization_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_materialization_session_commit(session->materialization,
                                                  &materialization_failure, err);
    /* Context creation needs independent system headroom, so establish it before the complete
     * component payload is faulted into the locked residency arena. */
    if (rc == YVEX_OK && backend_kind == YVEX_BACKEND_KIND_CUDA) {
        backend_options.kind = YVEX_BACKEND_KIND_CUDA;
        backend_options.memory_limit_bytes = maximum_device_bytes;
        rc = yvex_backend_open(&session->backend, &backend_options, err);
    }
    residency_options.maximum_host_bytes = maximum_host_bytes;
    if (rc == YVEX_OK)
        rc = yvex_runtime_component_residency_prepare(
            &session->residency, session->materialization, admission->logical_component_identity,
            &residency_options, &residency_failure, err);
    if (rc == YVEX_OK && backend_kind == YVEX_BACKEND_KIND_CUDA)
        rc = yvex_runtime_residency_cuda_session_attach(
            session->residency, &session->backend, maximum_device_bytes, &uploaded,
            &session->summary, err);
    if (rc == YVEX_OK && backend_kind == YVEX_BACKEND_KIND_CPU)
        rc = yvex_runtime_residency_snapshot(session->residency, &session->summary,
                                             NULL, NULL, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_resource_catalog_open(
            &session->execution_resources,
            session->summary.generation ? session->summary.generation : 1ull,
            session->summary.residency_identity, 4ull, err);
    if (rc == YVEX_OK) {
        unsigned long long remaining = maximum_host_bytes ? maximum_host_bytes : SIZE_MAX;
        if (session->summary.encoded_bytes >= remaining) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-session", "no host budget for resource metadata");
            rc = YVEX_ERR_BOUNDS;
        } else rc = yvex_runtime_resource_catalog_reserve(session->execution_resources, 4u,
            remaining - session->summary.encoded_bytes, &session->catalog_bytes, err);
    }
    if (rc == YVEX_OK)
        session->resource_summary.schema_version =
            YVEX_COMPONENT_RESOURCE_SUMMARY_SCHEMA_V2;
    if (rc != YVEX_OK) {
        primary = err ? *err : (yvex_error){0};
        yvex_error_clear(&cleanup);
        cleanup_rc = yvex_runtime_component_session_close(&session, &cleanup);
        if (cleanup_rc != YVEX_OK) {
            if (err) *err = cleanup;
            return cleanup_rc;
        }
        if (err) *err = primary;
        return rc;
    }
    *out = session;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int component_session_weight_view(
    void *context, const char *name, yvex_component_encoded_weight *weight, yvex_error *err)
{
    yvex_runtime_component_session *session = context;
    if (!session || !name || !weight || !session->materialization ||
        !session->residency || !session->summary.sealed || session->summary.invalidated) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.weight-view",
                       "one sealed component execution and weight name are required");
        return YVEX_ERR_INVALID_ARG;
    }
    return component_weight_bind(session->materialization, session->residency,
                                 name, weight, err);
}

static int component_session_workspace_reserve(
    void *context, unsigned long long bytes, yvex_error *err)
{
    return component_session_prepare_workspace(context, bytes, err);
}

static int component_resource_handle_present(yvex_engine_resource_handle handle)
{
    return handle.engine_generation && handle.slot && handle.generation;
}

static unsigned int component_prepared_handles(const component_prepared_entry *entry,
    yvex_engine_resource_handle handles[3])
{
    unsigned int count = 0u;
    if (component_resource_handle_present(entry->arena)) handles[count++] = entry->arena;
    if (component_resource_handle_present(entry->request)) handles[count++] = entry->request;
    if (component_resource_handle_present(entry->condition)) handles[count++] = entry->condition;
    return count;
}

static int component_prepared_resources_drop(component_prepared_entry *entry, yvex_error *err)
{
    yvex_engine_resource_handle handles[3];
    unsigned int count = component_prepared_handles(entry, handles);
    if (entry->borrowed > count) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource", "prepared borrow accounting is inconsistent");
        return YVEX_ERR_STATE;
    }
    while (entry->borrowed) {
        int rc = yvex_runtime_resource_drop(entry->owner->execution_resources, handles[entry->borrowed - 1u], err);
        if (rc != YVEX_OK) return rc;
        entry->borrowed--;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int component_prepared_resources_borrow(component_prepared_entry *entry, yvex_error *err)
{
    yvex_engine_resource_handle handles[3];
    unsigned int count = component_prepared_handles(entry, handles);
    void *value = NULL;
    int rc = YVEX_OK;
    if (!count || !entry->program || entry->borrowed) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource", "one unborrowed prepared owner is required");
        return YVEX_ERR_STATE;
    }
    while (entry->borrowed < count && rc == YVEX_OK) {
        rc = yvex_runtime_resource_acquire(entry->owner->execution_resources, handles[entry->borrowed], &value, err);
        if (rc == YVEX_OK) {
            entry->borrowed++;
            if (value != entry->program) {
                yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource",
                    "prepared resource value disagrees with its owner");
                rc = YVEX_ERR_STATE;
            }
        }
    }
    if (rc != YVEX_OK) {
        yvex_error cleanup;
        int closed = component_prepared_resources_drop(entry, &cleanup);
        if (closed != YVEX_OK) { rc = closed; if (err) *err = cleanup; }
    }
    return rc;
}

static int component_session_transaction_retain(void *context, yvex_error *err)
{
    yvex_runtime_component_session *session = context;
    if (!session || !session->materialization || !session->residency ||
        !session->summary.sealed || session->summary.invalidated ||
        session->execution_transaction_leases == ULLONG_MAX) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-session.transaction",
                       "sealed component resources are required for execution retention");
        return YVEX_ERR_STATE;
    }
    session->execution_transaction_leases++;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int component_session_transaction_release(void *context, yvex_error *err)
{
    yvex_runtime_component_session *session = context;
    if (!session || !session->execution_transaction_leases) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-session.transaction",
                       "component execution retention is not active");
        return YVEX_ERR_STATE;
    }
    if (session->execution_transaction_leases == 1ull) {
        for (component_prepared_entry *entry = session->prepared_entries; entry; entry = entry->next) {
            int rc = component_prepared_resources_drop(entry, err);
            if (rc != YVEX_OK) return rc;
        }
        session->resource_summary.retained_by_transaction = 0;
    }
    session->execution_transaction_leases--;
    if (!session->execution_transaction_leases)
        memset(session->active_request_identity, 0, sizeof(session->active_request_identity));
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_component_session_borrow(
    yvex_runtime_component_session *session, yvex_component_execution *execution,
    yvex_error *err)
{
    if (execution) memset(execution, 0, sizeof(*execution));
    if (!session || !execution || !session->materialization || !session->residency ||
        !session->summary.sealed || session->summary.invalidated ||
        !yvex_sha256_hex_valid(session->summary.residency_identity)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component-session.borrow",
                       "one sealed runtime-owned component session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    execution->schema_version = YVEX_COMPONENT_EXECUTION_SCHEMA_V2;
    execution->materialization = session->materialization;
    execution->backend = session->backend;
    execution->resident_encoded_bytes = session->summary.encoded_bytes;
    execution->owner_context = session;
    execution->weight_view = component_session_weight_view;
    execution->workspace_reserve = component_session_workspace_reserve;
    execution->program_stage = &session->program_stage;
    yvex_core_text_copy(execution->residency_identity,
                        sizeof(execution->residency_identity),
                        session->summary.residency_identity);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int component_execution_valid(const yvex_component_execution *execution)
{
    return execution &&
           execution->schema_version == YVEX_COMPONENT_EXECUTION_SCHEMA_V2 &&
           execution->materialization && execution->owner_context &&
           execution->weight_view && execution->workspace_reserve &&
           yvex_sha256_hex_valid(execution->residency_identity);
}

int yvex_component_execution_resource_lease(
    const yvex_component_execution *execution, yvex_execution_resource_lease *lease,
    yvex_error *err)
{
    if (lease) memset(lease, 0, sizeof(*lease));
    if (!component_execution_valid(execution) || !lease) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "component.execution.resource-lease",
                       "one runtime-owned component execution is required");
        return YVEX_ERR_INVALID_ARG;
    }
    lease->identity = execution->residency_identity;
    lease->context = execution->owner_context;
    lease->retain = component_session_transaction_retain;
    lease->release = component_session_transaction_release;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_component_execution_resource_summary(
    const yvex_component_execution *execution,
    yvex_component_resource_summary *summary, yvex_error *err)
{
    yvex_runtime_component_session *session;
    yvex_engine_resource_summary catalog = {0};
    unsigned long long count = 0ull;
    int rc;
    if (summary) memset(summary, 0, sizeof(*summary));
    if (!component_execution_valid(execution) || !summary) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "component.execution.resource-summary",
                       "one runtime-owned component execution is required");
        return YVEX_ERR_INVALID_ARG;
    }
    session = execution->owner_context;
    rc = yvex_runtime_resource_snapshot(
        session->execution_resources, &catalog, NULL, 0ull, &count, err);
    if (rc == YVEX_OK) {
        yvex_component_resource_summary *s = &session->resource_summary;
        s->host_arena_bytes = s->device_arena_bytes = 0u;
        s->request_prepared_bytes = s->condition_prepared_bytes = s->allocation_count = 0u;
        s->prepared_program_count = session->prepared_count;
        s->request_ready = s->condition_ready = session->prepared_count != 0u;
        unsigned long long host, device;
        if (!component_prepared_totals(session, &host, &device)) rc = YVEX_ERR_BOUNDS;
        for (component_prepared_entry *entry = session->prepared_entries; rc == YVEX_OK && entry; entry = entry->next) {
            const component_prepared_summary *p = &entry->summary;
            if (!yvex_core_u64_add(s->host_arena_bytes, p->host_arena_bytes, &s->host_arena_bytes) ||
                !yvex_core_u64_add(s->device_arena_bytes, p->device_arena_bytes, &s->device_arena_bytes) ||
                !yvex_core_u64_add(s->request_prepared_bytes, p->request_prepared_bytes, &s->request_prepared_bytes) ||
                !yvex_core_u64_add(s->condition_prepared_bytes, p->condition_prepared_bytes,
                    &s->condition_prepared_bytes) ||
                !yvex_core_u64_add(s->allocation_count, p->allocation_count, &s->allocation_count))
                rc = YVEX_ERR_BOUNDS;
            s->request_ready &= p->request_ready;
            s->condition_ready &= p->condition_ready;
        }
        if (rc != YVEX_OK) {
            yvex_error_set(err, rc, "component.execution.resource-summary", "retained resource accounting overflowed");
            return rc;
        }
        s->ready = s->request_ready && s->condition_ready;
        if (!s->ready) s->prepared_identity[0] = '\0';
        session->resource_summary.resource_count = count;
        session->resource_summary.resource_generation = catalog.generation;
        *summary = session->resource_summary;
    }
    return rc;
}

int yvex_component_execution_weight_view(
    const yvex_component_execution *execution, const char *name,
    yvex_component_encoded_weight *weight, yvex_error *err)
{
    if (!component_execution_valid(execution) || !name || !name[0] || !weight) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "component.execution.weight-view",
                       "one borrowed component execution and weight name are required");
        return YVEX_ERR_INVALID_ARG;
    }
    return execution->weight_view(execution->owner_context, name, weight, err);
}

/* Text and vision currently lower to dense parameter ordinals. The directory
 * is derived exclusively from admitted physical values, not source topology. */
static int component_parameters_bind(const yvex_component_execution *component,
    const yvex_program_physical *program, yvex_component_parameter_name_fn parameter_name,
    void *parameter_context, unsigned long long host_limit,
    yvex_component_encoded_weight **out, size_t *count, unsigned long long *bytes, yvex_error *err)
{
    const yvex_program_physical_summary *summary = yvex_program_physical_summary_get(program);
    yvex_component_encoded_weight *weights = NULL;
    unsigned char *bound = NULL;
    unsigned long long peak;
    int rc = YVEX_OK;
    *out = NULL; *count = 0u; *bytes = 0u;
    if (!summary || !parameter_name) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.parameters",
            "compiled program and parameter resolver required");
        return YVEX_ERR_INVALID_ARG;
    }
    for (size_t i = 0u; i < summary->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(program, i);
        if (!v->parameter) continue;
        if (v->tensor_id >= summary->value_count || v->tensor_id >= SIZE_MAX / sizeof(*weights)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.parameters",
                "compiled parameter directory exceeds its admitted bound");
            return YVEX_ERR_BOUNDS;
        }
        if (*count <= v->tensor_id) *count = (size_t)v->tensor_id + 1u;
    }
    if (!*count || !yvex_core_u64_mul(*count, sizeof(*weights), bytes) || *bytes > SIZE_MAX ||
        !yvex_core_u64_add(*bytes, *count, &peak) || (host_limit && peak >= host_limit)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.parameters",
            "parameter binding directory exceeds budget");
        return YVEX_ERR_BOUNDS;
    }
    weights = calloc(*count, sizeof(*weights));
    bound = calloc(*count, 1u);
    if (!weights || !bound) {
        free(weights); free(bound);
        yvex_error_set(err, YVEX_ERR_NOMEM, "runtime.component.parameters", "parameter directory allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (size_t i = 0u; rc == YVEX_OK && i < summary->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(program, i);
        if (!v->parameter || bound[v->tensor_id]) continue;
        char name[256] = {0};
        rc = parameter_name(parameter_context, v->tensor_id, name, err);
        if (rc == YVEX_OK) rc = yvex_component_execution_weight_view(component, name, weights + v->tensor_id, err);
        if (rc == YVEX_OK) bound[v->tensor_id] = 1u;
    }
    for (size_t i = 0u; rc == YVEX_OK && i < *count; ++i) {
        if (!bound[i]) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.parameters",
                "compiled parameter directory has a hole");
            rc = YVEX_ERR_FORMAT;
        }
    }
    free(bound);
    if (rc == YVEX_OK) *out = weights;
    else free(weights);
    return rc;
}

static void component_text_result_project(
    yvex_runtime_av_conditioning_result *out,
    const yvex_backend_text_execution_result *source)
{
    out->token_count = source->token_count;
    out->hidden_width = source->hidden_width;
    out->layer_count = source->layer_count;
    out->resident_bytes = source->resident_bytes;
    out->kernel_launches = source->kernel_launches;
    out->h2d_bytes = source->h2d_bytes;
    out->d2h_bytes = source->d2h_bytes;
    out->device_bytes = source->device_bytes;
    memcpy(out->residency_identity, source->residency_identity,
           sizeof(out->residency_identity));
    memcpy(out->execution_identity, source->execution_identity,
           sizeof(out->execution_identity));
    out->complete = source->complete;
}

static int component_text_request_validate(
    const yvex_component_text_request *request, const yvex_runtime_av_conditioning_result *result, yvex_error *err)
{
    if (!request || !request->program || !request->parameter_name ||
        !request->token_ids || !request->token_count || !request->output || !result) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.text",
                       "one admitted text program, parameter linkage and invocation are required");
        return YVEX_ERR_INVALID_ARG;
    }
    return YVEX_OK;
}

int yvex_component_text_execute(
    const yvex_component_execution *execution, const yvex_component_text_request *request,
    yvex_runtime_av_conditioning_result *result, yvex_error *err)
{
    yvex_component_encoded_weight *weights = NULL;
    yvex_backend_text_execution_result program_result = {0};
    unsigned long long weight_bytes;
    size_t weight_count;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    rc = component_text_request_validate(request, result, err);
    if (rc != YVEX_OK) return rc;
    if (!component_execution_valid(execution) || !execution->backend || !request->program) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.text",
                       "one borrowed component and compiled program are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = component_parameters_bind(execution, request->program, request->parameter_name,
        request->parameter_context, request->maximum_host_bytes, &weights, &weight_count, &weight_bytes, err);
    if (rc != YVEX_OK) return rc;
    yvex_component_text_request compiled = *request;
    if (compiled.maximum_host_bytes) compiled.maximum_host_bytes -= weight_bytes;
    compiled.parameter_name = NULL; compiled.parameter_context = NULL;
    rc = yvex_component_text_program_execute(execution, &compiled,
        weights, weight_count, &program_result, err);
    if (rc == YVEX_OK) component_text_result_project(result, &program_result);
    free(weights);
    return rc;
}

int yvex_runtime_component_text_artifact_execute(
    const yvex_complete_artifact_admission *admission, const yvex_artifact *artifact,
    const yvex_gguf *gguf, const yvex_tensor_table *tensors,
    yvex_backend_kind backend_kind, const yvex_component_text_request *request,
    yvex_runtime_av_conditioning_result *result, yvex_error *err)
{
    yvex_runtime_component_session *session = NULL;
    yvex_component_execution execution = {0};
    yvex_error cleanup;
    int rc, cleanup_rc;
    if (result) memset(result, 0, sizeof(*result));
    rc = component_text_request_validate(request, result, err);
    if (rc != YVEX_OK) return rc;
    if (!admission || !artifact || !gguf || !tensors || !request->maximum_device_bytes) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.text",
                       "one admitted text-component artifact and resource budget are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_runtime_component_session_open(
        &session, admission, artifact, gguf, tensors, backend_kind,
        request->maximum_host_bytes, request->maximum_device_bytes, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_component_session_borrow(session, &execution, err);
    if (rc == YVEX_OK)
        rc = yvex_component_text_execute(&execution, request, result, err);
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_runtime_component_session_close(&session, &cleanup);
    if (cleanup_rc != YVEX_OK) {
        rc = cleanup_rc;
        if (err) *err = cleanup;
    }
    return rc;
}

int yvex_component_vision_program_execute(
    const yvex_component_execution *component, const yvex_vision_program *program, const yvex_vision_request *request,
    yvex_component_parameter_name_fn parameter_name, void *parameter_context,
    yvex_vision_result *result, yvex_error *err)
{
    yvex_component_encoded_weight *weights = NULL;
    yvex_backend_vision_request execution = {0};
    const yvex_program_physical_summary *summary = program ?
        yvex_program_physical_summary_get(program->physical) : NULL;
    size_t count = 0u;
    int rc = YVEX_OK;
    if (result) memset(result, 0, sizeof(*result));
    if (!component_execution_valid(component) || !component->backend || !request || !summary ||
        !parameter_name || !result || yvex_backend_kind_of(component->backend) != YVEX_BACKEND_KIND_CUDA) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.vision",
                       "one sealed CUDA component and compiled parameter resolver are required");
        return YVEX_ERR_INVALID_ARG;
    }
    unsigned long long weight_bytes;
    rc = component_parameters_bind(component, program->physical, parameter_name, parameter_context,
        0u, &weights, &count, &weight_bytes, err);
    if (rc == YVEX_OK) {
        execution.request = request; execution.weights = weights; execution.weight_count = count;
        execution.residency_identity = component->residency_identity;
        execution.resident_bytes = component->resident_encoded_bytes;
        rc = yvex_vision_program_execute(program, component, &execution, result, err);
    }
    free(weights);
    return rc;
}

int yvex_component_program_binding_open(yvex_component_program_binding **out,
    const yvex_component_execution *execution, const yvex_program_physical *program,
    yvex_component_parameter_name_fn parameter_name, void *parameter_context, yvex_error *err)
{
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(program);
    yvex_component_program_binding *binding = NULL;
    unsigned long long bytes, total, retained_host = 0u, retained_device = 0u;
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    if (out) *out = NULL;
    if (!out || !component_execution_valid(execution) ||
        execution->weight_view != component_session_weight_view || !s || !parameter_name ||
        !execution->program_stage || *execution->program_stage) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.program-binding",
            "one runtime-owned component and complete compiled program are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_runtime_component_session *session = execution->owner_context;
    if (!component_prepared_totals(session, &retained_host, &retained_device) ||
        session->program_binding_count == ULLONG_MAX ||
        !yvex_core_u64_mul(s->value_count, sizeof(*binding->parameters), &bytes) || bytes > SIZE_MAX ||
        !yvex_core_u64_add(bytes, sizeof(*binding), &bytes) ||
        !yvex_core_u64_add(session->program_binding_bytes, bytes, &total) ||
        !yvex_core_u64_add(total, session->summary.encoded_bytes, &total) ||
        !yvex_core_u64_add(total, retained_host, &total) ||
        (session->host_limit && total >= session->host_limit)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.program-binding",
            "compiled parameter linkage exceeds the component host budget");
        return YVEX_ERR_BOUNDS;
    }
    binding = calloc(1u, sizeof(*binding));
    if (binding) binding->parameters = calloc(s->value_count, sizeof(*binding->parameters));
    if (!binding || !binding->parameters) {
        free(binding);
        yvex_error_set(err, YVEX_ERR_NOMEM, "runtime.component.program-binding", "parameter linkage allocation failed");
        return YVEX_ERR_NOMEM;
    }
    binding->execution = *execution; binding->host_bytes = bytes;
    yvex_sha256_init(&hash);
    int rc = yvex_sha256_update_text(&hash, "yvex.component.program-binding.v1") &&
        yvex_sha256_update_text(&hash, execution->residency_identity) &&
        yvex_sha256_update_text(&hash, s->identity) ? YVEX_OK : YVEX_ERR_STATE;
    for (size_t i = 0u; rc == YVEX_OK && i < s->value_count; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(program, i);
        if (!v->parameter) continue;
        size_t j;
        for (j = 0u; j < binding->parameter_count && binding->parameters[j].tensor_id != v->tensor_id; ++j) {}
        if (j < binding->parameter_count) continue;
        char name[256] = {0};
        rc = parameter_name(parameter_context, v->tensor_id, name, err);
        if (rc == YVEX_OK && (!memchr(name, '\0', sizeof(name)) || !name[0])) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.program-binding", "parameter name is not bounded");
            rc = YVEX_ERR_FORMAT;
        }
        if (rc == YVEX_OK) rc = yvex_component_execution_weight_view(execution, name,
            &binding->parameters[binding->parameter_count].weight, err);
        if (rc == YVEX_OK) {
            binding->parameters[binding->parameter_count++].tensor_id = v->tensor_id;
            if (!yvex_sha256_update_u64(&hash, v->tensor_id) || !yvex_sha256_update_text(&hash, name))
                rc = YVEX_ERR_STATE;
        }
    }
    if (rc == YVEX_OK && !yvex_sha256_final(&hash, digest)) rc = YVEX_ERR_STATE;
    if (rc == YVEX_OK) {
        binding->program = yvex_program_physical_retain(program, err);
        if (!binding->program) rc = yvex_error_is_set(err) ? err->code : YVEX_ERR_STATE;
    }
    if (rc != YVEX_OK) { free(binding->parameters); free(binding); return rc; }
    yvex_sha256_hex(digest, binding->identity);
    session->program_binding_bytes += bytes; session->program_binding_count++;
    *out = binding;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_component_program_binding_close(yvex_component_program_binding **binding)
{
    if (!binding || !*binding) return;
    yvex_component_program_binding *owned = *binding;
    yvex_runtime_component_session *session = owned->execution.owner_context;
    session->program_binding_bytes -= owned->host_bytes; session->program_binding_count--;
    yvex_program_physical_close(&owned->program);
    free(owned->parameters); free(owned); *binding = NULL;
}

typedef struct {
    const yvex_joint_program *program;
    const char *binding_identity;
    const yvex_transformer_joint_request *request;
    yvex_program_kernel_parameter *parameters;
    size_t parameter_count;
    yvex_program_host_input inputs[10];
    unsigned int *indices;
    float *storage, *outputs[2];
    unsigned long long input_count[10], output_count[2], host_bytes;
    yvex_program_observer observer;
} component_joint_invocation;

static int component_joint_observe(void *opaque, unsigned long long tag, const yvex_ir_type *type,
    unsigned long long rows, const float *values, unsigned long long count, yvex_error *err)
{
    const yvex_transformer_joint_request *r = opaque;
    unsigned long long block = (tag >> 8u) & 0xffffffffffu;
    yvex_transformer_joint_stage stage = (yvex_transformer_joint_stage)(tag & 255u);
    yvex_transformer_joint_scope scope = (yvex_transformer_joint_scope)(tag >> 56u);
    unsigned long long width = type->rank > 1u ? type->shape[type->rank - 1u].extent : count;
    (void)rows;
    if (!width || count % width) return YVEX_ERR_FORMAT;
    if (stage == YVEX_TRANSFORMER_JOINT_STAGE_COUNT && r->block_observer) {
        yvex_transformer_joint_block_observation observation = {block, count / width, width, count, values};
        return r->block_observer(r->block_observer_context, &observation, err);
    }
    if (r->stage_observer && scope == r->observed_stage_scope && block == r->observed_stage_block &&
        (r->observed_stage == YVEX_TRANSFORMER_JOINT_STAGE_COUNT || r->observed_stage == stage)) {
        yvex_transformer_joint_stage_observation observation = {
            block, count / width, width, count, scope, stage, values};
        return r->stage_observer(r->stage_observer_context, &observation, err);
    }
    yvex_error_set(err, YVEX_ERR_STATE, "runtime.component.observation", "compiled observation differs from its owner");
    return YVEX_ERR_STATE;
}

/* Public/component input adaptation, not model computation. Static signature
 * and capacities are checked against the compiler before any payload access. */
static int component_joint_inputs(component_joint_invocation *c, unsigned long long host_limit, yvex_error *err)
{
    const yvex_transformer_joint_request *r = c->request;
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(c->program->physical);
    unsigned long long rows[] = {r->video_rows, r->audio_rows, r->text_rows, r->timestep_count, r->packed_rows,
        r->video_rows, r->audio_rows, r->text_rows, r->packed_rows, r->packed_rows};
    unsigned long long widths[5] = {0};
    const float *values[] = {r->video, r->audio, r->conditioning, r->timesteps, r->position_ids};
    const unsigned int *indices[] = {r->video_indices, r->audio_indices, r->text_indices,
        r->token_tags, r->timestep_indices};
    unsigned long long bytes, total;
    if (!s || s->input_count != 10u || s->result_count != 2u || !r->video_output || !r->audio_output ||
        !r->token_tags || !r->timestep_indices || !r->packed_rows ||
        s->minimum_rows != r->packed_rows || s->maximum_rows != r->packed_rows) goto incompatible;
    for (size_t i = 0u; i < 10u; ++i) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(c->program->physical, i)->type;
        if (i < 5u) widths[i] = t->shape[1].extent;
        if (!rows[i] || t->kind != YVEX_IR_TENSOR || t->rank != (i < 5u ? 2u : 1u) ||
            t->scalar != (i < 5u ? YVEX_IR_F32 : YVEX_IR_INDEX) ||
            (t->shape[0].symbol == YVEX_IR_NONE ? t->shape[0].extent != rows[i] : rows[i] != r->packed_rows) ||
            (i < 5u && (!values[i] || t->shape[1].symbol != YVEX_IR_NONE || !widths[i])) ||
            (i >= 5u && !indices[i - 5u]) ||
            !yvex_core_u64_mul(rows[i], i < 5u ? widths[i] : 1u, c->input_count + i)) goto incompatible;
        c->inputs[i] = i < 5u ? (yvex_program_host_input){.values = values[i]} :
            (yvex_program_host_input){.indices = indices[i - 5u]};
    }
    c->output_count[0] = c->input_count[0]; c->output_count[1] = c->input_count[1];
    for (size_t i = 0u; i < 2u; ++i) {
        const yvex_program_physical_value *v = yvex_program_physical_value_at(c->program->physical,
            yvex_program_physical_result_at(c->program->physical, i));
        if (!v || v->type.kind != YVEX_IR_TENSOR || v->type.scalar != YVEX_IR_F32 || v->type.rank != 2u ||
            v->type.shape[0].symbol != YVEX_IR_NONE || v->type.shape[0].extent != rows[i] ||
            v->type.shape[1].symbol != YVEX_IR_NONE || v->type.shape[1].extent != widths[i]) goto incompatible;
    }
    uintptr_t video = (uintptr_t)r->video_output, audio = (uintptr_t)r->audio_output;
    unsigned long long video_bytes, audio_bytes;
    if (!yvex_core_u64_mul(c->output_count[0], sizeof(float), &video_bytes) ||
        !yvex_core_u64_mul(c->output_count[1], sizeof(float), &audio_bytes) ||
        video_bytes > UINTPTR_MAX - video || audio_bytes > UINTPTR_MAX - audio ||
        !(video + video_bytes <= audio || audio + audio_bytes <= video)) goto incompatible;
    if (c->output_count[0] > r->video_output_capacity || c->output_count[1] > r->audio_output_capacity ||
        !yvex_core_u64_add(c->output_count[0], c->output_count[1], &total) ||
        !yvex_core_u64_mul(total, sizeof(float), &bytes) || bytes > SIZE_MAX ||
        !yvex_core_u64_mul(r->packed_rows, sizeof(*c->indices), &total) || total > SIZE_MAX ||
        !yvex_core_u64_add(bytes, total, &c->host_bytes)) goto incompatible;
    if (c->host_bytes >= host_limit) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.joint-program.input",
            "invocation staging exhausts the admitted host budget");
        return YVEX_ERR_BOUNDS;
    }
    c->storage = malloc((size_t)bytes); c->indices = malloc((size_t)total);
    if (!c->storage || !c->indices) return YVEX_ERR_NOMEM;
    c->outputs[0] = c->storage; c->outputs[1] = c->storage + c->output_count[0];
    for (size_t i = 0u; i < r->packed_rows; ++i) c->indices[i] = UINT_MAX;
    for (size_t group = 0u; group < 3u; ++group)
        for (size_t i = 0u; i < rows[5u + group]; ++i) {
            unsigned int index = indices[group][i];
            if (index >= r->packed_rows || c->indices[index] != UINT_MAX) goto incompatible;
            c->indices[index] = 0u;
        }
    for (size_t i = 0u; i < r->packed_rows; ++i) {
        if (c->indices[i] == UINT_MAX) goto incompatible;
        for (size_t axis = 0u; axis < widths[4]; ++axis)
            if (!isfinite(r->position_ids[i * widths[4] + axis])) goto incompatible;
    }
    for (size_t i = 0u; i < r->timestep_count; ++i)
        if (!isfinite(r->timesteps[i]) || r->timesteps[i] < 0.0f || r->timesteps[i] > 1.0f) goto incompatible;
    c->observer = (yvex_program_observer){component_joint_observe, (void *)r};
    return YVEX_OK;
incompatible:
    yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.joint-program.input",
        "packed request differs from its verified signature, capacities or input constraints");
    return YVEX_ERR_FORMAT;
}

static int component_joint_hash_values(yvex_sha256 *hash, const yvex_program_host_input *inputs,
    const unsigned long long *counts, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        if (!yvex_sha256_update_u64(hash, counts[i])) return 0;
        for (size_t j = 0u; j < counts[i]; ++j) {
            uint32_t value;
            if (inputs[i].indices) value = inputs[i].indices[j];
            else memcpy(&value, inputs[i].values + j, sizeof(value));
            if (!yvex_sha256_update_u64(hash, value)) return 0;
        }
    }
    return 1;
}

static int component_joint_request_identity(
    const yvex_runtime_component_session *session,
    const component_joint_invocation *invocation,
    char identity[YVEX_SHA256_HEX_CAP], yvex_error *err)
{
    const yvex_transformer_joint_request *request = invocation->request;
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    if (!session || !request ||
        !yvex_sha256_hex_valid(request->layout_identity) ||
        !yvex_sha256_hex_valid(request->condition_identity)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component-resource",
                       "layout and condition identities are required for preparation");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.joint-program-request.v1") ||
        !yvex_sha256_update_text(&hash, session->summary.residency_identity) ||
        !yvex_sha256_update_text(&hash, request->layout_identity) ||
        !yvex_sha256_update_text(&hash, request->condition_identity) ||
        !yvex_sha256_update_text(
            &hash, request->video_output_physical.physical_identity) ||
        !yvex_sha256_update_text(
            &hash, request->audio_output_physical.physical_identity) ||
        !yvex_sha256_update_u64(&hash, request->video_rows) ||
        !yvex_sha256_update_u64(&hash, request->audio_rows) ||
        !yvex_sha256_update_u64(&hash, request->text_rows) ||
        !yvex_sha256_update_u64(&hash, request->packed_rows) ||
        !component_joint_hash_values(&hash, invocation->inputs + 2u, invocation->input_count + 2u, 1u) ||
        !component_joint_hash_values(&hash, invocation->inputs + 4u, invocation->input_count + 4u, 1u) ||
        !component_joint_hash_values(&hash, invocation->inputs + 5u, invocation->input_count + 5u, 4u) ||
        !yvex_sha256_final(&hash, digest)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource",
                       "prepared execution identity could not be sealed");
        return YVEX_ERR_STATE;
    }
    yvex_sha256_hex(digest, identity);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int component_prepared_view_release(void *context, yvex_error *err)
{
    (void)context;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int component_joint_entry_evict(component_prepared_entry *entry, yvex_error *err)
{
    if (entry->borrowed) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource",
            "borrowed prepared resources cannot be invalidated");
        return YVEX_ERR_STATE;
    }
    yvex_engine_resource_handle *handles[] = {&entry->condition, &entry->request, &entry->arena};
    for (size_t i = 0u; i < 3u; ++i) {
        if (!component_resource_handle_present(*handles[i])) continue;
        int rc = yvex_runtime_resource_evict(entry->owner->execution_resources, handles[i], err);
        if (rc != YVEX_OK) return rc;
    }
    return yvex_program_prepared_close(&entry->program, err);
}

static int component_joint_resources_evict(yvex_runtime_component_session *session, yvex_error *err)
{
    for (component_prepared_entry *entry = session->prepared_entries; entry; entry = entry->next) {
        if (entry->borrowed) {
            yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource",
                "prepared request is retained by a transaction");
            return YVEX_ERR_STATE;
        }
    }
    while (session->prepared_entries) {
        component_prepared_entry *entry = session->prepared_entries;
        int rc = component_joint_entry_evict(entry, err);
        if (rc != YVEX_OK) return rc;
        session->prepared_entries = entry->next;
        session->prepared_count--;
        free(entry);
    }
    session->retained_request_identity[0] = '\0';
    session->resource_summary.ready = 0;
    return YVEX_OK;
}

static int component_joint_resource_register_one(component_prepared_entry *entry, yvex_engine_resource_kind kind,
    yvex_engine_resource_lifetime lifetime, const char *name, yvex_engine_resource_handle dependency,
    yvex_engine_resource_bytes bytes, unsigned long long preparation_ns,
    yvex_engine_resource_release_fn release, yvex_engine_resource_handle *handle, yvex_error *err)
{
    yvex_runtime_component_session *session = entry->owner;
    yvex_engine_resource_request resource = {0};
    resource.kind = kind;
    resource.owner = YVEX_ENGINE_RESOURCE_OWNER_EXECUTION;
    resource.lifetime = lifetime;
    resource.numeric_class = kind == YVEX_ENGINE_RESOURCE_WORKSPACE
        ? YVEX_ENGINE_RESOURCE_NUMERIC_STATE : YVEX_ENGINE_RESOURCE_NUMERIC_EQUIVALENT_PREPARED;
    resource.name = name;
    resource.package_identity = session->package_identity;
    resource.specialization_identity = session->summary.residency_identity;
    resource.admission_identity = entry->summary.identity;
    resource.dependency = dependency;
    resource.bytes = bytes;
    resource.preparation_nanoseconds = preparation_ns;
    resource.value = entry->program;
    resource.release = release;
    resource.release_context = entry;
    resource.ready = 1;
    resource.evictable = 1;
    return yvex_runtime_resource_register(session->execution_resources, &resource, handle, err);
}

static int component_joint_resources_register(component_prepared_entry *entry, yvex_error *err)
{
    yvex_engine_resource_bytes bytes = {0};
    const component_prepared_summary *s = &entry->summary;
    if (!yvex_core_u64_add(s->host_arena_bytes, s->device_arena_bytes, &bytes.workspace_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-resource", "execution arena accounting overflowed");
        return YVEX_ERR_BOUNDS;
    }
    bytes.host_resident_bytes = s->host_arena_bytes;
    bytes.device_resident_bytes = s->device_arena_bytes;
    int rc = component_joint_resource_register_one(entry, YVEX_ENGINE_RESOURCE_WORKSPACE,
        YVEX_ENGINE_RESOURCE_LIFETIME_REQUEST, "joint-execution-arena", (yvex_engine_resource_handle){0},
        bytes, s->preparation_nanoseconds, component_joint_prepared_release, &entry->arena, err);
    bytes = (yvex_engine_resource_bytes){.prepared_bytes = s->request_prepared_bytes};
    if (rc == YVEX_OK) rc = component_joint_resource_register_one(entry, YVEX_ENGINE_RESOURCE_PREPARED_LAYOUT,
        YVEX_ENGINE_RESOURCE_LIFETIME_REQUEST, "joint-request-layout", entry->arena, bytes, 0u,
        component_prepared_view_release, &entry->request, err);
    bytes.prepared_bytes = s->condition_prepared_bytes;
    if (rc == YVEX_OK) rc = component_joint_resource_register_one(entry, YVEX_ENGINE_RESOURCE_PREPARED_TENSOR,
        YVEX_ENGINE_RESOURCE_LIFETIME_CONDITION, "joint-condition-state", entry->arena, bytes, 0u,
        component_prepared_view_release, &entry->condition, err);
    return rc;
}

static int component_joint_entry_prepare(component_prepared_entry *entry, const component_joint_invocation *c,
    unsigned long long host_limit, unsigned long long device_limit, const char identity[65], yvex_error *err)
{
    yvex_runtime_component_session *session = entry->owner;
    component_prepared_summary *s = &entry->summary;
    yvex_backend_operation_facts facts;
    yvex_backend_memory_stats before = {0}, after = {0};
    yvex_program_host_input inputs[] = {c->inputs[2], c->inputs[4]};
    const size_t links[] = {2u, 4u, 5u};
    unsigned long long started = yvex_core_monotonic_ns(), retained = 0u;
    int measured = yvex_backend_get_memory_stats(session->backend, &before, NULL) == YVEX_OK;
    int rc = yvex_program_prepared_open(&entry->program, c->program->preparation, c->program->step,
        c->parameters, c->parameter_count, session->backend, c->request->packed_rows, 1u, inputs, links, 3u,
        host_limit, device_limit, &c->observer, &facts, err);
    if (rc != YVEX_OK) return rc;
    s->preparation_nanoseconds = yvex_core_monotonic_ns() - started;
    yvex_program_prepared_resources(entry->program, &s->host_arena_bytes, &s->device_arena_bytes, &retained);
    const yvex_ir_type *condition = &yvex_program_physical_value_at(c->program->preparation,
        yvex_program_physical_result_at(c->program->preparation, 0u))->type;
    s->condition_prepared_bytes = condition->shape[0].extent * condition->shape[1].extent * sizeof(float);
    s->request_prepared_bytes = retained - s->condition_prepared_bytes;
    memcpy(s->identity, identity, sizeof(s->identity));
    if (measured && yvex_backend_get_memory_stats(session->backend, &after, NULL) == YVEX_OK &&
        after.allocation_events >= before.allocation_events)
        s->allocation_count = after.allocation_events - before.allocation_events;
    rc = component_joint_resources_register(entry, err);
    if (rc == YVEX_OK) s->request_ready = s->condition_ready = 1;
    return rc;
}

static int component_joint_resources_prepare(yvex_runtime_component_session *session,
    const component_joint_invocation *c, unsigned long long host_limit, int *reused,
    component_prepared_entry **out, yvex_error *err)
{
    char request_identity[65], identity[65];
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long host, device, metadata, capacity, remaining, preparation_total;
    component_prepared_entry *entry;
    *reused = 0; *out = NULL;
    int rc = component_joint_request_identity(session, c, request_identity, err);
    if (rc != YVEX_OK) return rc;
    if (session->active_request_identity[0] && strcmp(session->active_request_identity, request_identity)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource",
            "one transaction cannot change its prepared request identity");
        return YVEX_ERR_STATE;
    }
    if (session->retained_request_identity[0] && strcmp(session->retained_request_identity, request_identity)) {
        if (session->resource_summary.rebuild_count == ULLONG_MAX) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-resource", "prepared rebuild count overflowed");
            return YVEX_ERR_BOUNDS;
        }
        rc = component_joint_resources_evict(session, err);
        if (rc != YVEX_OK) return rc;
        session->resource_summary.rebuild_count++;
    }
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.joint-program-preparation.v4") ||
        !yvex_sha256_update_text(&hash, request_identity) ||
        !yvex_sha256_update_text(&hash, c->binding_identity) ||
        !yvex_sha256_update_text(&hash, yvex_program_physical_summary_get(c->program->preparation)->identity) ||
        !yvex_sha256_update_text(&hash, yvex_program_physical_summary_get(c->program->step)->identity) ||
        !yvex_sha256_final(&hash, digest)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource", "prepared program identity failed");
        return YVEX_ERR_STATE;
    }
    yvex_sha256_hex(digest, identity);
    for (entry = session->prepared_entries; entry; entry = entry->next) {
        if (!entry->summary.request_ready || !entry->summary.condition_ready) {
            yvex_error_set(err, YVEX_ERR_STATE, "runtime.component-resource", "failed preparation still owns cleanup");
            return YVEX_ERR_STATE;
        }
        if (!strcmp(entry->summary.identity, identity)) break;
    }
    if (entry) *reused = 1;
    else {
        if (!component_prepared_totals(session, &host, &device) ||
            !yvex_core_u64_add(session->prepared_count, 1u, &capacity) ||
            !yvex_core_u64_mul(capacity, 3u, &capacity) ||
            !yvex_core_u64_add(host - session->catalog_bytes, sizeof(*entry), &remaining) ||
            remaining >= host_limit || (session->device_limit && device >= session->device_limit) ||
            session->resource_summary.preparation_count == ULLONG_MAX) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-resource",
                "prepared programs exhaust component budget");
            return YVEX_ERR_BOUNDS;
        }
        rc = yvex_runtime_resource_catalog_reserve(session->execution_resources, capacity,
            host_limit - remaining, &metadata, err);
        if (rc != YVEX_OK) return rc;
        session->catalog_bytes = metadata;
        remaining += metadata;
        if (remaining >= host_limit) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-resource", "metadata leaves no execution budget");
            return YVEX_ERR_BOUNDS;
        }
        entry = calloc(1u, sizeof(*entry));
        if (!entry) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "runtime.component-resource",
                "prepared program owner allocation failed");
            return YVEX_ERR_NOMEM;
        }
        entry->owner = session; entry->next = session->prepared_entries;
        session->prepared_entries = entry; session->prepared_count++;
        rc = component_joint_entry_prepare(entry, c, host_limit - remaining,
            session->device_limit ? session->device_limit - device : 0u, identity, err);
        if (rc == YVEX_OK && !yvex_core_u64_add(session->resource_summary.preparation_nanoseconds,
            entry->summary.preparation_nanoseconds, &preparation_total)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-resource", "preparation timing overflowed");
            rc = YVEX_ERR_BOUNDS;
        }
        if (rc != YVEX_OK) {
            yvex_error cleanup;
            int closed = component_joint_entry_evict(entry, &cleanup);
            if (closed != YVEX_OK) { if (err) *err = cleanup; return closed; }
            session->prepared_entries = entry->next; session->prepared_count--; free(entry);
            return rc;
        }
        session->resource_summary.preparation_count++;
        session->resource_summary.preparation_nanoseconds = preparation_total;
    }
    if (!entry->borrowed) {
        rc = component_prepared_resources_borrow(entry, err);
        if (rc != YVEX_OK) return rc;
    }
    memcpy(session->retained_request_identity, request_identity, sizeof(request_identity));
    if (session->execution_transaction_leases)
        memcpy(session->active_request_identity, request_identity, sizeof(request_identity));
    session->resource_summary.schema_version = YVEX_COMPONENT_RESOURCE_SUMMARY_SCHEMA_V2;
    session->resource_summary.ready = 1;
    session->resource_summary.retained_by_transaction = session->execution_transaction_leases != 0u;
    memcpy(session->resource_summary.prepared_identity, identity, sizeof(identity));
    *out = entry;
    return YVEX_OK;
}

static int component_joint_result_identity(const yvex_component_execution *execution,
    const component_joint_invocation *c, char identity[65])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_program_host_input outputs[] = {{.values = c->outputs[0]}, {.values = c->outputs[1]}};
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.component.joint-program.result.v2") ||
        !yvex_sha256_update_text(&hash, c->binding_identity) ||
        !yvex_sha256_update_text(&hash, yvex_program_physical_summary_get(c->program->physical)->identity) ||
        !yvex_sha256_update_text(&hash, execution->residency_identity) ||
        !component_joint_hash_values(&hash, c->inputs, c->input_count, 10u) ||
        !component_joint_hash_values(&hash, outputs, c->output_count, 2u) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, identity);
    return 1;
}

int yvex_component_joint_program_execute(
    const yvex_component_program_binding *binding, const yvex_joint_program *program,
    const yvex_transformer_joint_request *request, yvex_transformer_joint_result *result, yvex_error *err)
{
    const yvex_component_execution *execution = binding ? &binding->execution : NULL;
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(program ? program->physical : NULL);
    component_joint_invocation c = {.program = program, .request = request};
    component_prepared_entry *entry = NULL;
    yvex_transformer_joint_result published = {0};
    yvex_backend_operation_facts facts = {0};
    yvex_backend_memory_stats before = {0}, after = {0};
    unsigned long long host_limit, device_limit, retained_host = 0u, retained_device = 0u;
    unsigned long long host = 0u, device = 0u;
    int reused = 0, prepared = 0, have_before = 0, rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!component_execution_valid(execution) || !execution->backend || !s ||
        !program->preparation || !program->step || binding->program != program->physical || !request || !result ||
        !execution->program_stage || *execution->program_stage || s->value_count > SIZE_MAX / sizeof(*c.parameters)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.component.joint-program",
            "one admitted component, verified program, parameter binding and request are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_runtime_component_session *session = execution->owner_context;
    c.parameters = binding->parameters; c.parameter_count = binding->parameter_count;
    c.binding_identity = binding->identity;
    host_limit = session->host_limit ? session->host_limit : SIZE_MAX;
    unsigned long long occupied;
    if (!yvex_core_u64_add(session->summary.encoded_bytes, session->program_binding_bytes, &occupied) ||
        occupied >= host_limit) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.joint-program",
            "resident parameters exhaust the host budget");
        return YVEX_ERR_BOUNDS;
    }
    host_limit -= occupied;
    device_limit = session->device_limit;
    if (!component_prepared_totals(session, &retained_host, &retained_device) || retained_host >= host_limit) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.joint-program",
            "retained preparation exhausts the invocation host budget");
        return YVEX_ERR_BOUNDS;
    }
    rc = component_joint_inputs(&c, host_limit - retained_host, err);
    if (rc == YVEX_OK) host_limit -= c.host_bytes;
    prepared = rc == YVEX_OK && yvex_sha256_hex_valid(request->layout_identity) &&
        yvex_sha256_hex_valid(request->condition_identity) &&
        !(request->stage_observer && request->observed_stage_scope == YVEX_TRANSFORMER_JOINT_SCOPE_REFINER);
    if (prepared) rc = component_joint_resources_prepare(session, &c, host_limit, &reused, &entry, err);
    if (rc == YVEX_OK) have_before = yvex_backend_get_memory_stats(execution->backend, &before, NULL) == YVEX_OK;
    if (rc == YVEX_OK && prepared) {
        yvex_program_host_input inputs[] = {c.inputs[0], c.inputs[1], {0}, c.inputs[3], {0}, {0},
            c.inputs[5], c.inputs[6], c.inputs[7], c.inputs[8], c.inputs[9]};
        rc = yvex_program_prepared_run(entry->program, inputs, 11u, c.outputs, 2u,
            NULL, NULL, &c.observer, &facts, err);
        yvex_program_prepared_resources(entry->program, &host, &device, NULL);
        entry->summary.host_arena_bytes = host; entry->summary.device_arena_bytes = device;
        if (rc == YVEX_OK && (!component_prepared_totals(session, &retained_host, &retained_device) ||
            retained_host > host_limit || (device_limit && retained_device > device_limit))) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.joint-program",
                "aggregate prepared resources exceed budget");
            rc = YVEX_ERR_BOUNDS;
        }
    } else if (rc == YVEX_OK) {
        /* A diagnostic full invocation may coexist with a retained step.
         * Their storage is disjoint and consumes one session budget. */
        host_limit -= retained_host;
        if (device_limit && retained_device >= device_limit) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component.joint-program",
                "retained preparation exhausts the invocation device budget");
            rc = YVEX_ERR_BOUNDS;
        }
        if (device_limit && rc == YVEX_OK) device_limit -= retained_device;
        if (rc == YVEX_OK)
            rc = yvex_program_stage_open(execution->program_stage, program->physical, c.parameters, c.parameter_count,
                execution->backend, request->packed_rows, 1, host_limit, device_limit, err);
        if (rc == YVEX_OK) rc = yvex_program_stage_observe(*execution->program_stage, &c.observer, err);
        if (rc == YVEX_OK) rc = yvex_program_stage_host_inputs(*execution->program_stage, request->packed_rows,
            c.inputs, 10u, c.outputs, 2u, NULL, NULL, &facts, err);
        yvex_program_stage_resources(*execution->program_stage, &host, &device);
    }
    if (rc == YVEX_OK && !component_joint_result_identity(execution, &c, published.execution_identity)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.component.joint-program", "execution identity failed");
        rc = YVEX_ERR_STATE;
    }
    if (prepared && rc == YVEX_OK) {
        unsigned long long events = 0u;
        if (have_before && yvex_backend_get_memory_stats(execution->backend, &after, NULL) == YVEX_OK &&
            after.allocation_events >= before.allocation_events)
            events = after.allocation_events - before.allocation_events;
        if (session->resource_summary.use_count == ULLONG_MAX ||
            (reused && session->resource_summary.reuse_count == ULLONG_MAX) ||
            !yvex_core_u64_add(session->resource_summary.execution_allocation_events, events,
                &session->resource_summary.execution_allocation_events)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.component-resource", "execution accounting overflowed");
            rc = YVEX_ERR_BOUNDS;
        } else {
            session->resource_summary.use_count++;
            session->resource_summary.reuse_count += reused != 0;
            session->resource_summary.last_execution_allocation_events = events;
        }
    }
    yvex_error cleanup;
    int closed = yvex_program_stage_close(execution->program_stage, &cleanup);
    if (closed != YVEX_OK) { rc = closed; if (err) *err = cleanup; }
    if (entry && !session->execution_transaction_leases && entry->borrowed) {
        closed = component_prepared_resources_drop(entry, &cleanup);
        if (closed != YVEX_OK) { rc = closed; if (err) *err = cleanup; }
    }
    if (rc == YVEX_OK) {
        published.video_rows = request->video_rows; published.audio_rows = request->audio_rows;
        published.text_rows = request->text_rows; published.packed_rows = request->packed_rows;
        published.block_count = request->block_count;
        published.resident_bytes = execution->resident_encoded_bytes; published.device_bytes = device;
        published.kernel_launches = facts.kernel_launches; published.h2d_bytes = facts.h2d_bytes;
        published.d2h_bytes = facts.d2h_bytes; published.complete = 1;
        memcpy(published.residency_identity, execution->residency_identity, sizeof(published.residency_identity));
        memcpy(request->video_output, c.outputs[0], (size_t)c.output_count[0] * sizeof(float));
        memcpy(request->audio_output, c.outputs[1], (size_t)c.output_count[1] * sizeof(float));
        *result = published;
    }
    free(c.storage); free(c.indices);
    return rc;
}


int yvex_component_buffer_open(
    yvex_component_f32_buffer *buffer, unsigned long long count,
    unsigned long long maximum, unsigned long long *live,
    unsigned long long *peak, const char *stage, const char *label, yvex_error *err)
{
    unsigned long long bytes, next;
    if (buffer) memset(buffer, 0, sizeof(*buffer));
    if (!buffer || !live || !peak || !stage || !label || !count ||
        !yvex_core_u64_mul(count, sizeof(float), &bytes) ||
        bytes > (unsigned long long)SIZE_MAX ||
        !yvex_core_u64_add(*live, bytes, &next)) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, stage,
                        "%s workspace extent overflowed", label);
        return YVEX_ERR_BOUNDS;
    }
    if (next > maximum) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, stage,
                        "%s workspace budget was exceeded", label);
        return YVEX_ERR_BOUNDS;
    }
    buffer->data = (float *)malloc((size_t)bytes);
    if (!buffer->data) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, stage,
                        "%s workspace allocation failed", label);
        return YVEX_ERR_NOMEM;
    }
    buffer->count = count;
    *live = next;
    if (next > *peak) *peak = next;
    return YVEX_OK;
}

void yvex_component_buffer_close(yvex_component_f32_buffer *buffer,
                                 unsigned long long *live)
{
    unsigned long long bytes;
    if (!buffer || !live) return;
    bytes = buffer->count * sizeof(float);
    if (bytes <= *live) *live -= bytes;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

static const yvex_materialized_tensor_binding *component_binding_find(
    const yvex_materialization_session *session, const char *name)
{
    unsigned long long index;
    if (!session || !name) return NULL;
    for (index = 0ull;; ++index) {
        const yvex_materialized_tensor_binding *binding =
            yvex_materialization_session_tensor_at(session, index);
        if (!binding || strcmp(binding->name, name) == 0) return binding;
    }
}

static int component_weight_bind(
    const yvex_materialization_session *session,
    const yvex_runtime_residency *residency, const char *name,
    yvex_component_encoded_weight *weight, yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding =
        component_binding_find(session, name);
    if (!binding || !binding->row_count || !residency || !weight) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.component.binding",
                       "an exact resident component weight binding is unavailable");
        return YVEX_ERR_FORMAT;
    }
    memset(weight, 0, sizeof(*weight));
    if (yvex_runtime_residency_binding_view(
            residency, binding, &weight->encoded, &weight->encoded_bytes, err) != YVEX_OK)
        return yvex_error_code(err);
    weight->qtype = binding->qtype;
    weight->row_count = binding->row_count;
    weight->row_width = binding->row_width;
    weight->row_bytes = binding->encoded_bytes / binding->row_count;
    return YVEX_OK;
}
