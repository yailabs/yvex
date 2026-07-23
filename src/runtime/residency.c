/* Owner: runtime attention residency.
 * Owns: sealed host/device arenas of encoded attention/core-envelope weight ranges.
 * Does not own: artifact admission, placement policy, execution workspace, graph math, or KV.
 * Invariants: every admitted range is read exactly once cold and resolved by tensor identity.
 * Boundary: runtime models share immutable bytes; execution sessions own mutable workspaces.
 * Purpose: retain exact encoded attention bytes across warm executions without artifact rereads.
 * Inputs: sealed runtime model, imported descriptor/plan, and an explicit host budget.
 * Effects: reads admitted ranges, seals identities, and attaches one borrowed read provider.
 * Failure: partial arenas detach and release without changing the materialization session. */
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/internal/core.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/runtime.h>

typedef struct {
    const yvex_materialized_tensor_binding *binding;
    yvex_attention_binding_class binding_class;
    unsigned long long arena_offset;
} residency_record;

struct yvex_runtime_residency {
    yvex_materialization_session *materialization;
    unsigned char *arena;
    residency_record *records;
    unsigned long long *record_index;
    unsigned long long record_index_count;
    yvex_backend *cuda_backend;
    yvex_device_tensor *cuda_weights;
    yvex_runtime_residency_summary summary;
    pthread_mutex_t access_mutex;
    int access_mutex_ready;
};

/* Purpose: publish one typed residency refusal without exposing a partial arena. */
static int residency_reject(yvex_runtime_residency_failure *failure,
                            yvex_runtime_residency_failure_code code,
                            const yvex_runtime_tensor_binding *binding,
                            unsigned long long expected, unsigned long long actual,
                            const char *reason, yvex_status status, yvex_error *err)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->tensor_id = binding ? binding->tensor_id : ULLONG_MAX;
        failure->layer_index = binding ? binding->layer_index : ULLONG_MAX;
        failure->role = binding ? binding->role : YVEX_TENSOR_ROLE_UNKNOWN;
        failure->expected = expected;
        failure->actual = actual;
        failure->reason = reason;
    }
    yvex_error_set(err, status, "runtime.residency", reason);
    return status;
}

/* Purpose: resolve one exact resident record without copying or exposing arena ownership.
 * Inputs: sealed residency context, admitted binding, and caller-owned span outputs.
 * Effects: returns a borrowed immutable span and updates no lifecycle state.
 * Failure: missing ranges miss; mismatched identities return invalid.
 * Boundary: resolution performs no artifact read or qtype decode. */
static int residency_resolve(const void *context,
                             const yvex_materialized_tensor_binding *binding,
                             const unsigned char **data, unsigned long long *bytes)
{
    yvex_runtime_residency *residency = (yvex_runtime_residency *)context;
    unsigned long long slot;
    const residency_record *record;

    int invalidated;

    if (data) *data = NULL;
    if (bytes) *bytes = 0ull;
    if (!residency || !binding || !data || !bytes ||
        binding->tensor_id >= residency->record_index_count)
        return YVEX_MATERIALIZATION_READ_MISS;
    if (!residency->access_mutex_ready ||
        pthread_mutex_lock(&residency->access_mutex) != 0)
        return YVEX_MATERIALIZATION_READ_INVALID;
    invalidated = residency->summary.invalidated;
    (void)pthread_mutex_unlock(&residency->access_mutex);
    if (invalidated) return YVEX_MATERIALIZATION_READ_INVALID;
    slot = residency->record_index[binding->tensor_id];
    if (!slot) return YVEX_MATERIALIZATION_READ_MISS;
    record = &residency->records[slot - 1ull];
    if (!record->binding || record->binding->tensor_id != binding->tensor_id ||
        record->binding->qtype != binding->qtype ||
        record->binding->encoded_bytes != binding->encoded_bytes ||
        record->binding->absolute_offset != binding->absolute_offset ||
        strcmp(record->binding->name, binding->name) != 0)
        return YVEX_MATERIALIZATION_READ_INVALID;
    *data = residency->arena + record->arena_offset;
    *bytes = record->binding->encoded_bytes;
    return YVEX_MATERIALIZATION_READ_HIT;
}

/* Purpose: account one exact materialization access served from the sealed host arena.
 * Inputs: attached residency context and the borrowed byte count.
 * Effects: advances resident access counters with checked byte arithmetic.
 * Failure: counter overflow preserves the last representable byte total.
 * Boundary: accounting never changes resident content or materialization ownership. */
static int residency_note_access(const void *context, unsigned long long bytes)
{
    yvex_runtime_residency *residency = (yvex_runtime_residency *)context;
    unsigned long long next_calls, next_bytes;

    if (!residency || !residency->access_mutex_ready ||
        pthread_mutex_lock(&residency->access_mutex) != 0)
        return 0;
    if (!yvex_core_u64_add(residency->summary.resident_read_calls, 1ull,
                           &next_calls) ||
        !yvex_core_u64_add(residency->summary.resident_bytes_read, bytes,
                           &next_bytes)) {
        (void)pthread_mutex_unlock(&residency->access_mutex);
        return 0;
    }
    residency->summary.resident_read_calls = next_calls;
    residency->summary.resident_bytes_read = next_bytes;
    (void)pthread_mutex_unlock(&residency->access_mutex);
    return 1;
}

/* Purpose: invalidate the borrowed materialization link when its session detaches first.
 * Inputs: attached residency context after the materialization owner has drained all readers.
 * Effects: clears only the borrowed session link and attachment fact exactly once.
 * Failure: null contexts are harmless; the exclusive callback has no fallible synchronization.
 * Boundary: callback never releases the arena or materialization session. */
static void residency_detached(const void *context)
{
    yvex_runtime_residency *residency = (yvex_runtime_residency *)context;

    if (!residency) return;
    residency->materialization = NULL;
    residency->summary.attached = 0;
}

/* Purpose: release one residency whose provider is absent or already detached.
 * Inputs: address of an exclusively owned detached or never-published candidate.
 * Effects: releases every allocation and nulls the caller's owner exactly once.
 * Failure: null owners are harmless; exclusive storage release performs no detach.
 * Boundary: callers prove the materialization owner no longer references this context. */
static void residency_storage_release(yvex_runtime_residency **owner)
{
    yvex_runtime_residency *residency = owner ? *owner : NULL;

    if (!residency) return;
    free(residency->arena);
    free(residency->records);
    free(residency->record_index);
    if (residency->access_mutex_ready)
        (void)pthread_mutex_destroy(&residency->access_mutex);
    memset(residency, 0, sizeof(*residency));
    free(residency);
    *owner = NULL;
}

/* Purpose: release the optional model-owned CUDA pack without touching host residency.
 * Inputs: exclusively owned residency and typed cleanup output.
 * Effects: releases device bytes before their context and clears CUDA readiness.
 * Failure: preserves the first not-yet-released owner for exact retry.
 * Boundary: no session backend may remain live when model teardown calls this helper. */
static int residency_cuda_release(yvex_runtime_residency *residency, yvex_error *err)
{
    int rc = YVEX_OK;

    if (!residency) return YVEX_OK;
    if (residency->cuda_backend) {
        rc = yvex_backend_close_admit(residency->cuda_backend, err);
        if (rc != YVEX_OK) return rc;
    }
    if (residency->cuda_weights) {
        rc = yvex_backend_tensor_release(
            residency->cuda_backend, &residency->cuda_weights, err);
        if (rc != YVEX_OK) return rc;
    }
    rc = yvex_backend_close_checked(&residency->cuda_backend, err);
    if (rc != YVEX_OK) return rc;
    residency->summary.cuda_ready = 0;
    residency->summary.device_resident_bytes = 0ull;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: add one classified immutable descriptor row to checked residency accounting.
 * Inputs: candidate pack, classified runtime binding, ordinal, and core counters.
 * Effects: records one range and advances checked aggregate accounting.
 * Failure: duplicate, malformed, or overflowing records leave typed refusal evidence.
 * Boundary: selection records metadata only and reads no payload bytes. */
static int residency_add_record(yvex_runtime_residency *residency,
                                const yvex_runtime_tensor_binding *runtime,
                                yvex_attention_binding_class binding_class,
                                unsigned long long ordinal,
                                unsigned long long *core_bytes,
                                unsigned long long core_qtypes[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP],
                                yvex_runtime_residency_failure *failure, yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding = runtime ? runtime->binding : NULL;
    unsigned long long next;

    if (!binding || binding->tensor_id >= residency->record_index_count)
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_GEOMETRY, runtime,
                                residency->record_index_count,
                                binding ? binding->tensor_id : ULLONG_MAX,
                                "resident binding tensor identity is outside the descriptor",
                                YVEX_ERR_FORMAT, err);
    if (residency->record_index[binding->tensor_id])
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_DUPLICATE_BINDING,
                                runtime, 1ull, 2ull,
                                "resident binding was selected more than once",
                                YVEX_ERR_FORMAT, err);
    if (!binding->encoded_bytes || binding->encoded_bytes > (unsigned long long)SIZE_MAX ||
        binding->qtype >= YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP)
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_GEOMETRY, runtime,
                                (unsigned long long)SIZE_MAX, binding->encoded_bytes,
                                "resident encoded range or qtype is outside platform bounds",
                                YVEX_ERR_BOUNDS, err);
    if (!yvex_core_u64_add(residency->summary.encoded_bytes, binding->encoded_bytes, &next))
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_GEOMETRY, runtime,
                                ULLONG_MAX, binding->encoded_bytes,
                                "resident encoded byte accounting overflowed",
                                YVEX_ERR_BOUNDS, err);
    residency->records[ordinal].binding = binding;
    residency->records[ordinal].binding_class = binding_class;
    residency->records[ordinal].arena_offset = residency->summary.encoded_bytes;
    residency->record_index[binding->tensor_id] = ordinal + 1ull;
    residency->summary.encoded_bytes = next;
    residency->summary.qtype_binding_counts[binding->qtype]++;
    if (!yvex_core_u64_add(residency->summary.qtype_bytes[binding->qtype],
                           binding->encoded_bytes,
                           &residency->summary.qtype_bytes[binding->qtype]))
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_GEOMETRY, runtime,
                                ULLONG_MAX, binding->encoded_bytes,
                                "resident qtype byte accounting overflowed",
                                YVEX_ERR_BOUNDS, err);
    if (binding_class == YVEX_ATTENTION_BINDING_CORE) {
        residency->summary.core_binding_count++;
        core_qtypes[binding->qtype]++;
        if (!yvex_core_u64_add(*core_bytes, binding->encoded_bytes, core_bytes))
            return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_GEOMETRY, runtime,
                                    ULLONG_MAX, binding->encoded_bytes,
                                    "resident core byte accounting overflowed",
                                    YVEX_ERR_BOUNDS, err);
    } else {
        residency->summary.envelope_binding_count++;
    }
    residency->summary.binding_count++;
    return YVEX_OK;
}

/* Purpose: hash exact resident payload bytes and immutable range boundaries.
 * Inputs: allocated candidate residency with ordered records.
 * Effects: reads each admitted range once into the arena and seals its payload digest.
 * Failure: read or hash failure preserves no attached provider.
 * Boundary: cold loading preserves encoded qtype bytes without decoding. */
static int residency_load_and_hash(yvex_runtime_residency *residency,
                                   yvex_runtime_residency_failure *failure, yvex_error *err)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.runtime.resident.payload.v1") ||
        !yvex_sha256_update_u64(&hash, residency->summary.binding_count))
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_LIFECYCLE, NULL,
                                1ull, 0ull, "resident payload hash initialization failed",
                                YVEX_ERR_STATE, err);
    for (index = 0ull; index < residency->summary.binding_count; ++index) {
        const residency_record *record = &residency->records[index];
        const yvex_materialized_tensor_binding *binding = record->binding;
        unsigned char *destination = residency->arena + record->arena_offset;
        int rc = yvex_materialization_session_read(
            residency->materialization, binding, 0ull, destination,
            (size_t)binding->encoded_bytes, NULL, err);

        if (rc != YVEX_OK)
            return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_READ, NULL,
                                    binding->encoded_bytes, 0ull,
                                    "resident encoded range cold read failed",
                                    (yvex_status)rc, err);
        if (!yvex_core_u64_add(residency->summary.cold_artifact_read_calls, 1ull,
                               &residency->summary.cold_artifact_read_calls) ||
            !yvex_core_u64_add(residency->summary.cold_artifact_bytes_read,
                               binding->encoded_bytes,
                               &residency->summary.cold_artifact_bytes_read))
            return residency_reject(
                failure, YVEX_RUNTIME_RESIDENCY_FAILURE_LIFECYCLE, NULL,
                ULLONG_MAX, binding->encoded_bytes,
                "resident cold-read accounting overflowed", YVEX_ERR_BOUNDS, err);
        if (!yvex_sha256_update_u64(&hash, binding->tensor_id) ||
            !yvex_sha256_update_u64(&hash, binding->qtype) ||
            !yvex_sha256_update_u64(&hash, binding->encoded_bytes) ||
            !yvex_sha256_update(&hash, destination, (size_t)binding->encoded_bytes))
            return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_LIFECYCLE, NULL,
                                    1ull, 0ull, "resident payload hash update failed",
                                    YVEX_ERR_STATE, err);
    }
    if (!yvex_sha256_final(&hash, digest))
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_LIFECYCLE, NULL,
                                1ull, 0ull, "resident payload hash finalization failed",
                                YVEX_ERR_STATE, err);
    yvex_sha256_hex(digest, residency->summary.payload_digest);
    return YVEX_OK;
}

/* Purpose: derive residency identity from semantic range order and exact encoded payload.
 * Inputs: loaded pack plus sealed runtime-model and attention identities.
 * Effects: writes one canonical content identity.
 * Failure: canonical hash encoding returns state failure without attaching the pack.
 * Boundary: paths, pointers, timestamps, and allocation order are excluded. */
static int residency_identity_build(yvex_runtime_residency *residency,
                                    const yvex_runtime_model_summary *model,
                                    const yvex_attention_summary *attention,
                                    yvex_error *err)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.runtime.residency.v1") ||
        !yvex_sha256_update_u64(&hash, YVEX_RUNTIME_RESIDENCY_SCHEMA_V1) ||
        !yvex_sha256_update_text(&hash, model->runtime_model_identity) ||
        !yvex_sha256_update_text(&hash, model->artifact_identity) ||
        !yvex_sha256_update_text(&hash, model->materialization_identity) ||
        !yvex_sha256_update_text(&hash, attention->attention_plan_identity) ||
        !yvex_sha256_update_u64(&hash, residency->summary.core_binding_count) ||
        !yvex_sha256_update_u64(&hash, residency->summary.envelope_binding_count) ||
        !yvex_sha256_update_u64(&hash, residency->summary.encoded_bytes))
        goto failed;
    for (index = 0ull; index < residency->summary.binding_count; ++index) {
        const residency_record *record = &residency->records[index];
        if (!yvex_sha256_update_u64(&hash, record->binding->tensor_id) ||
            !yvex_sha256_update_u64(&hash, record->binding_class) ||
            !yvex_sha256_update_u64(&hash, record->binding->qtype) ||
            !yvex_sha256_update_u64(&hash, record->binding->encoded_bytes) ||
            !yvex_sha256_update_u64(&hash, record->arena_offset))
            goto failed;
    }
    if (!yvex_sha256_update_text(&hash, residency->summary.payload_digest) ||
        !yvex_sha256_final(&hash, digest))
        goto failed;
    yvex_sha256_hex(digest, residency->summary.residency_identity);
    return YVEX_OK;
failed:
    yvex_error_set(err, YVEX_ERR_STATE, "runtime.residency.identity",
                   "resident identity encoding failed");
    return YVEX_ERR_STATE;
}

/* Purpose: release a partial candidate without detaching an unowned provider.
 * Inputs: address of an owned candidate, possibly attached to its borrowed session.
 * Effects: detaches only its provider, releases storage, and nulls the owner on success.
 * Failure: detach failure retains the complete residency for a checked retry.
 * Boundary: the materialization session and artifact remain borrowed. */
static int residency_release(yvex_runtime_residency **owner, yvex_error *err)
{
    yvex_runtime_residency *residency = owner ? *owner : NULL;
    int rc;

    if (!residency) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    rc = residency_cuda_release(residency, err);
    if (rc != YVEX_OK) return rc;
    if (residency->summary.attached && residency->materialization)
        if ((rc = yvex_materialization_session_detach_read_provider(
                 residency->materialization, residency, NULL, err)) != YVEX_OK)
            return rc;
    residency_storage_release(owner);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: build and attach one exact process-lifetime attention residency pack.
 * Inputs: sealed runtime model and optional explicit host budget.
 * Effects: performs one cold read per selected range, then serves immutable warm reads.
 * Failure: checked selection, accounting, read, or attach failure publishes no pack.
 * Boundary: this owner never decodes qtypes or allocates backend/device memory. */
int yvex_runtime_residency_prepare(yvex_runtime_residency **out, yvex_runtime_model *model,
                                   const yvex_runtime_residency_options *options,
                                   yvex_runtime_residency_failure *failure, yvex_error *err)
{
    const yvex_runtime_model_view *view = yvex_runtime_model_view_get(model);
    yvex_runtime_model_summary model_summary;
    const yvex_runtime_descriptor *descriptor = view ? view->descriptor : NULL;
    const yvex_runtime_descriptor_summary *descriptor_summary =
        yvex_runtime_descriptor_summary_get(descriptor);
    const yvex_attention_plan *plan = view ? view->attention : NULL;
    const yvex_attention_summary *attention = yvex_attention_plan_summary(plan);
    yvex_materialization_session *materialization = view ? view->materialization : NULL;
    yvex_runtime_residency *residency = NULL;
    yvex_materialization_read_provider provider;
    unsigned long long core_qtypes[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP] = {0};
    unsigned long long core_bytes = 0ull;
    unsigned long long index, ordinal = 0ull;
    int rc = YVEX_OK;

    if (out) *out = NULL;
    if (failure) memset(failure, 0, sizeof(*failure));
    if (!out)
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_INVALID_ARGUMENT,
                                NULL, 1ull, 0ull, "residency output is required",
                                YVEX_ERR_INVALID_ARG, err);
    rc = yvex_runtime_model_summary_copy(model, &model_summary, err);
    if (rc != YVEX_OK) {
        if (failure) {
            failure->code = YVEX_RUNTIME_RESIDENCY_FAILURE_LIFECYCLE;
            failure->reason = "runtime model snapshot failed during residency preparation";
        }
        return rc;
    }
    if (!model_summary.sealed || !model_summary.valid ||
        !descriptor_summary || !plan || !attention || !materialization)
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_MODEL,
                                NULL, 1ull, 0ull,
                                "sealed runtime model facts are required for residency",
                                YVEX_ERR_STATE, err);
    residency = (yvex_runtime_residency *)calloc(1u, sizeof(*residency));
    if (!residency)
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_ALLOCATION,
                                NULL, sizeof(*residency), 0ull,
                                "resident pack allocation failed", YVEX_ERR_NOMEM, err);
    if (pthread_mutex_init(&residency->access_mutex, NULL) != 0) {
        free(residency);
        return residency_reject(
            failure, YVEX_RUNTIME_RESIDENCY_FAILURE_ALLOCATION, NULL, 1ull, 0ull,
            "resident access synchronization initialization failed", YVEX_ERR_STATE, err);
    }
    residency->access_mutex_ready = 1;
    residency->materialization = materialization;
    residency->record_index_count = descriptor_summary->tensor_count;
    residency->records = (residency_record *)calloc(
        (size_t)descriptor_summary->tensor_count, sizeof(*residency->records));
    residency->record_index = (unsigned long long *)calloc(
        (size_t)descriptor_summary->tensor_count, sizeof(*residency->record_index));
    if (!residency->records || !residency->record_index) {
        residency_storage_release(&residency);
        return residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_ALLOCATION, NULL,
                                descriptor_summary->tensor_count, 0ull,
                                "resident record index allocation failed",
                                YVEX_ERR_NOMEM, err);
    }
    residency->summary.expected_core_binding_count = attention->required_binding_count;
    residency->summary.expected_envelope_binding_count =
        attention->required_envelope_binding_count;
    for (index = 0ull; rc == YVEX_OK && index < descriptor_summary->tensor_count; ++index) {
        const yvex_runtime_tensor_binding *binding =
            yvex_runtime_descriptor_tensor_at(descriptor, index);
        yvex_attention_binding_class binding_class =
            yvex_attention_plan_binding_classify(plan, binding);
        if (binding_class != YVEX_ATTENTION_BINDING_NOT_REQUIRED)
            rc = residency_add_record(residency, binding, binding_class, ordinal++,
                                      &core_bytes, core_qtypes, failure, err);
    }
    if (rc == YVEX_OK &&
        (residency->summary.core_binding_count != attention->required_binding_count ||
         residency->summary.envelope_binding_count != attention->required_envelope_binding_count ||
         core_bytes != attention->payload_bytes_bound))
        rc = residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_PLAN, NULL,
                              attention->required_binding_count +
                                  attention->required_envelope_binding_count,
                              residency->summary.binding_count,
                              "resident core or envelope accounting differs from the plan",
                              YVEX_ERR_FORMAT, err);
    for (index = 0ull; rc == YVEX_OK && index < YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP; ++index)
        if (core_qtypes[index] != attention->qtype_binding_counts[index])
            rc = residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_PLAN, NULL,
                                  attention->qtype_binding_counts[index], core_qtypes[index],
                                  "resident core qtype accounting differs from the plan",
                                  YVEX_ERR_FORMAT, err);
    if (rc == YVEX_OK) {
        residency->summary.core_complete =
            residency->summary.core_binding_count ==
            residency->summary.expected_core_binding_count;
        residency->summary.envelope_complete =
            residency->summary.envelope_binding_count ==
            residency->summary.expected_envelope_binding_count;
    }
    if (rc == YVEX_OK && residency->summary.encoded_bytes > (unsigned long long)SIZE_MAX)
        rc = residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_BUDGET, NULL,
                              (unsigned long long)SIZE_MAX, residency->summary.encoded_bytes,
                              "resident arena exceeds platform allocation range",
                              YVEX_ERR_BOUNDS, err);
    if (rc == YVEX_OK && options && options->maximum_host_bytes &&
        residency->summary.encoded_bytes > options->maximum_host_bytes)
        rc = residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_BUDGET, NULL,
                              options->maximum_host_bytes, residency->summary.encoded_bytes,
                              "resident arena exceeds the configured host budget",
                              YVEX_ERR_NOMEM, err);
    if (rc == YVEX_OK) {
        residency->arena = (unsigned char *)malloc((size_t)residency->summary.encoded_bytes);
        if (!residency->arena)
            rc = residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_ALLOCATION, NULL,
                                  residency->summary.encoded_bytes, 0ull,
                                  "resident encoded arena allocation failed",
                                  YVEX_ERR_NOMEM, err);
    }
    if (rc == YVEX_OK) rc = residency_load_and_hash(residency, failure, err);
    if (rc == YVEX_OK)
        rc = residency_identity_build(residency, &model_summary, attention, err);
    if (rc == YVEX_OK) {
        provider.context = residency;
        provider.resolve = residency_resolve;
        provider.note_access = residency_note_access;
        provider.detached = residency_detached;
        rc = yvex_materialization_session_attach_read_provider(
            materialization, &provider, NULL, err);
        if (rc != YVEX_OK)
            rc = residency_reject(failure, YVEX_RUNTIME_RESIDENCY_FAILURE_ATTACH, NULL,
                                  1ull, 0ull, "resident read provider attachment failed",
                                  (yvex_status)rc, err);
    }
    if (rc != YVEX_OK) {
        residency_storage_release(&residency);
        return rc;
    }
    residency->summary.schema_version = YVEX_RUNTIME_RESIDENCY_SCHEMA_V1;
    residency->summary.generation = 1ull;
    residency->summary.host_resident_bytes = residency->summary.encoded_bytes;
    residency->summary.sealed = 1;
    residency->summary.attached = 1;
    residency->summary.host_ready = 1;
    *out = residency;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: prepare one model-owned CUDA pack and attach an isolated session backend.
 * Inputs: sealed residency, exact session device budget, and caller-owned outputs.
 * Effects: uploads once under the residency mutex, then shares its context and immutable mapping.
 * Failure: releases unpublished candidates and returns no partially attached session backend.
 * Boundary: session graph/workspace state stays private; model release owns resident bytes/context. */
int yvex_runtime_residency_cuda_session_attach(
    yvex_runtime_residency *residency, yvex_backend **backend,
    unsigned long long maximum_device_bytes, int *uploaded,
    yvex_runtime_residency_summary *summary, yvex_error *err)
{
    yvex_backend_options options;
    yvex_backend_tensor_desc descriptor;
    yvex_backend *candidate_backend = NULL, *session_backend = NULL;
    yvex_device_tensor *candidate_weights = NULL;
    yvex_error primary, cleanup;
    int rc, cleanup_rc;

    if (backend) *backend = NULL;
    if (uploaded) *uploaded = 0;
    if (summary) memset(summary, 0, sizeof(*summary));
    if (!residency || !backend || !uploaded || !summary) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.residency.cuda",
                       "residency and complete CUDA attachment outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!residency->access_mutex_ready) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.residency.cuda",
                       "resident access synchronization is not initialized");
        return YVEX_ERR_STATE;
    }
    if (pthread_mutex_lock(&residency->access_mutex) != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.residency.cuda",
                       "resident access synchronization could not be acquired");
        return YVEX_ERR_STATE;
    }
    if (!residency->summary.sealed || !residency->summary.host_ready ||
        residency->summary.invalidated || !residency->arena) {
        rc = YVEX_ERR_STATE;
        yvex_error_set(err, rc, "runtime.residency.cuda",
                       "sealed valid host residency is required");
        goto done;
    }
    if (maximum_device_bytes &&
        residency->summary.encoded_bytes > maximum_device_bytes) {
        rc = YVEX_ERR_BOUNDS;
        yvex_error_set(err, rc, "runtime.residency.cuda",
                       "resident weights exceed the session device budget");
        goto done;
    }
    if ((residency->cuda_backend || residency->cuda_weights) &&
        !residency->summary.cuda_ready) {
        rc = residency_cuda_release(residency, err);
        if (rc != YVEX_OK) goto done;
    }
    if (!residency->summary.cuda_ready) {
        memset(&options, 0, sizeof(options));
        options.kind = YVEX_BACKEND_KIND_CUDA;
        options.memory_limit_bytes = maximum_device_bytes;
        rc = yvex_backend_open(&candidate_backend, &options, err);
        memset(&descriptor, 0, sizeof(descriptor));
        descriptor.name = "runtime-attention-residency";
        descriptor.dtype = YVEX_DTYPE_I8;
        descriptor.rank = 1u;
        descriptor.dims[0] = descriptor.bytes = residency->summary.encoded_bytes;
        if (rc == YVEX_OK)
            rc = yvex_backend_tensor_alloc(
                candidate_backend, &descriptor, &candidate_weights, err);
        if (rc == YVEX_OK)
            rc = yvex_backend_tensor_write(
                candidate_backend, candidate_weights, residency->arena,
                residency->summary.encoded_bytes, err);
        if (rc != YVEX_OK) {
            primary = err ? *err : (yvex_error){0};
            yvex_error_clear(&cleanup);
            cleanup_rc = candidate_weights
                             ? yvex_backend_tensor_release(
                                   candidate_backend, &candidate_weights, &cleanup)
                             : YVEX_OK;
            if (cleanup_rc == YVEX_OK)
                cleanup_rc = yvex_backend_close_checked(&candidate_backend, &cleanup);
            if (cleanup_rc != YVEX_OK) {
                residency->cuda_backend = candidate_backend;
                residency->cuda_weights = candidate_weights;
                rc = cleanup_rc;
                if (err) *err = cleanup;
            } else if (err) *err = primary;
            goto done;
        }
        residency->cuda_backend = candidate_backend;
        residency->cuda_weights = candidate_weights;
        residency->summary.device_resident_bytes = residency->summary.encoded_bytes;
        residency->summary.cuda_upload_bytes = residency->summary.encoded_bytes;
        residency->summary.cuda_upload_count = 1ull;
        residency->summary.cuda_ready = 1;
        *uploaded = 1;
    }
    rc = yvex_backend_open_shared_cuda(
        &session_backend, residency->cuda_backend, maximum_device_bytes, err);
    if (rc == YVEX_OK)
        rc = yvex_backend_resident_attach(
            session_backend, residency->arena, residency->summary.encoded_bytes,
            residency->cuda_weights, residency->summary.generation, err);
    if (rc != YVEX_OK) {
        primary = err ? *err : (yvex_error){0};
        yvex_error_clear(&cleanup);
        cleanup_rc = yvex_backend_close_checked(&session_backend, &cleanup);
        if (cleanup_rc != YVEX_OK) {
            *backend = session_backend;
            rc = cleanup_rc;
            if (err) *err = cleanup;
        } else if (err) *err = primary;
        goto done;
    }
    *backend = session_backend;
    *summary = residency->summary;
    rc = YVEX_OK;
done:
    (void)pthread_mutex_unlock(&residency->access_mutex);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

/* Purpose: detach and release one process-lifetime resident attention arena.
 * Inputs: address of an exclusively owned residency handle or null.
 * Effects: removes its read provider and nulls the owner after releasing storage.
 * Failure: detach failure retains the owner for retry; null close is harmless.
 * Boundary: model/session teardown controls call ordering. */
int yvex_runtime_residency_close(yvex_runtime_residency **residency, yvex_error *err)
{
    return residency_release(residency, err);
}

/* Purpose: snapshot synchronized residency facts and optionally borrow its stable host arena.
 * Inputs: sealed residency, summary output, and either both arena outputs or neither.
 * Effects: copies one coherent summary and optional immutable process-lifetime span.
 * Failure: malformed, unsynchronized, or unavailable residency publishes no borrowed span.
 * Boundary: the snapshot extends no lifetime and performs no artifact read or qtype decode. */
int yvex_runtime_residency_snapshot(const yvex_runtime_residency *residency,
                                    yvex_runtime_residency_summary *summary,
                                    const unsigned char **arena,
                                    unsigned long long *arena_bytes,
                                    yvex_error *err)
{
    yvex_runtime_residency *mutable_residency =
        (yvex_runtime_residency *)residency;
    int borrow_arena = arena || arena_bytes;

    if (arena) *arena = NULL;
    if (arena_bytes) *arena_bytes = 0ull;
    if (!residency || !summary || borrow_arena != (arena && arena_bytes)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.residency.snapshot",
                       "residency, summary, and paired arena outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!residency->access_mutex_ready ||
        pthread_mutex_lock(&mutable_residency->access_mutex) != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.residency.snapshot",
                       "runtime residency synchronization is unavailable");
        return YVEX_ERR_STATE;
    }
    *summary = residency->summary;
    if (borrow_arena && (!summary->sealed || !summary->host_ready ||
                         summary->invalidated || !residency->arena)) {
        (void)pthread_mutex_unlock(&mutable_residency->access_mutex);
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.residency.snapshot",
                       "sealed available host residency is required for arena access");
        return YVEX_ERR_STATE;
    }
    if (borrow_arena) {
        *arena = residency->arena;
        *arena_bytes = summary->encoded_bytes;
    }
    (void)pthread_mutex_unlock(&mutable_residency->access_mutex);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: invalidate one shared resident generation without releasing live arena bytes.
 * Inputs: process-lifetime residency owned by an invalidated runtime model.
 * Effects: makes every later provider resolve fail closed and advances its generation.
 * Failure: synchronization or generation overflow leaves the pack invalidated.
 * Boundary: physical release remains deferred until every sharing session closes. */
int yvex_runtime_residency_invalidate(yvex_runtime_residency *residency,
                                      yvex_error *err)
{
    unsigned long long next;

    if (!residency) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.residency.invalidate",
                       "runtime residency is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!residency->access_mutex_ready ||
        getenv("YVEX_TEST_RUNTIME_RESIDENCY_INVALIDATE_FAILURE") ||
        pthread_mutex_lock(&residency->access_mutex) != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.residency.invalidate",
                       "runtime residency synchronization is unavailable");
        return YVEX_ERR_STATE;
    }
    if (residency->summary.invalidated) {
        (void)pthread_mutex_unlock(&residency->access_mutex);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    residency->summary.invalidated = 1;
    residency->summary.host_ready = 0;
    residency->summary.cuda_ready = 0;
    if (!yvex_core_u64_add(residency->summary.generation, 1ull, &next)) {
        (void)pthread_mutex_unlock(&residency->access_mutex);
        yvex_error_set(err, YVEX_ERR_BOUNDS, "runtime.residency.invalidate",
                       "resident generation overflowed");
        return YVEX_ERR_BOUNDS;
    }
    residency->summary.generation = next;
    (void)pthread_mutex_unlock(&residency->access_mutex);
    yvex_error_clear(err);
    return YVEX_OK;
}
