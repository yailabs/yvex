/* Own live engine resources without changing immutable package or specialization identity. */
#include "src/runtime/private.h"

#include <yvex/internal/core.h>
#include <yvex/internal/engine_resource.h>

#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_engine_resource_entry summary;
    void *value;
    yvex_engine_resource_release_fn release;
    void *release_context;
    int in_use;
} engine_resource_slot;

struct yvex_engine_resource_catalog {
    pthread_mutex_t mutex;
    engine_resource_slot *slots;
    yvex_engine_resource_summary summary;
    unsigned long long next_generation;
    int mutex_ready;
};

static int resource_refuse(yvex_error *err, yvex_status status,
                           const char *reason)
{
    yvex_error_set(err, status, "runtime.engine-resource", reason);
    return status;
}

static int optional_identity_valid(const char *identity)
{
    return !identity || !identity[0] || yvex_sha256_hex_valid(identity);
}

static int resource_bytes_add(yvex_engine_resource_bytes *target,
                              const yvex_engine_resource_bytes *value)
{
#define ADD_FIELD(field_)                                                        \
    if (!yvex_core_u64_add(target->field_, value->field_, &target->field_))      \
        return 0
    ADD_FIELD(mapped_package_bytes);
    ADD_FIELD(host_resident_bytes);
    ADD_FIELD(device_resident_bytes);
    ADD_FIELD(prepared_bytes);
    ADD_FIELD(sequence_state_bytes);
    ADD_FIELD(workspace_bytes);
    ADD_FIELD(temporary_bytes);
#undef ADD_FIELD
    return 1;
}

static void resource_bytes_subtract(yvex_engine_resource_bytes *target,
                                    const yvex_engine_resource_bytes *value)
{
#define SUBTRACT_FIELD(field_) target->field_ -= value->field_
    SUBTRACT_FIELD(mapped_package_bytes);
    SUBTRACT_FIELD(host_resident_bytes);
    SUBTRACT_FIELD(device_resident_bytes);
    SUBTRACT_FIELD(prepared_bytes);
    SUBTRACT_FIELD(sequence_state_bytes);
    SUBTRACT_FIELD(workspace_bytes);
    SUBTRACT_FIELD(temporary_bytes);
#undef SUBTRACT_FIELD
}

static int handle_present(yvex_engine_resource_handle handle)
{
    return handle.engine_generation || handle.slot || handle.generation;
}

static engine_resource_slot *resource_slot_locked(
    yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_handle handle)
{
    engine_resource_slot *slot;
    if (!catalog || handle.engine_generation != catalog->summary.engine_generation ||
        !handle.slot || handle.slot > catalog->summary.capacity ||
        !handle.generation)
        return NULL;
    slot = &catalog->slots[handle.slot - 1ull];
    return slot->in_use &&
                   slot->summary.handle.generation == handle.generation
               ? slot : NULL;
}

static int resource_request_valid(
    const yvex_engine_resource_catalog *catalog,
    const yvex_engine_resource_request *request)
{
    const int prepared = request &&
        (request->kind == YVEX_ENGINE_RESOURCE_PREPARED_TENSOR ||
         request->kind == YVEX_ENGINE_RESOURCE_PREPARED_GROUP ||
         request->kind == YVEX_ENGINE_RESOURCE_PREPARED_LAYOUT);
    if (!catalog || !request || request->kind >= YVEX_ENGINE_RESOURCE_KIND_COUNT ||
        request->owner > YVEX_ENGINE_RESOURCE_OWNER_EXECUTION ||
        request->lifetime > YVEX_ENGINE_RESOURCE_LIFETIME_QUANTUM ||
        request->numeric_class > YVEX_ENGINE_RESOURCE_NUMERIC_STATE ||
        !request->name || !request->name[0] ||
        strlen(request->name) >= YVEX_ENGINE_RESOURCE_NAME_CAP ||
        !yvex_sha256_hex_valid(request->package_identity) ||
        !optional_identity_valid(request->specialization_identity) ||
        !optional_identity_valid(request->admission_identity) ||
        (prepared && (!request->specialization_identity ||
                      !request->specialization_identity[0] ||
                      !request->admission_identity ||
                      !request->admission_identity[0])) ||
        (request->numeric_class ==
             YVEX_ENGINE_RESOURCE_NUMERIC_CANONICAL_PACKAGE &&
         request->specialization_identity &&
         request->specialization_identity[0]) ||
        (request->evictable && !request->release))
        return 0;
    return 1;
}

static void resource_dependency_drop_locked(
    yvex_engine_resource_catalog *catalog,
    const yvex_engine_resource_entry *entry)
{
    engine_resource_slot *dependency;
    if (!handle_present(entry->dependency)) return;
    dependency = resource_slot_locked(catalog, entry->dependency);
    if (dependency && dependency->summary.dependent_count)
        dependency->summary.dependent_count--;
}

static void resource_remove_locked(
    yvex_engine_resource_catalog *catalog, engine_resource_slot *slot,
    int evicted)
{
    resource_dependency_drop_locked(catalog, &slot->summary);
    resource_bytes_subtract(&catalog->summary.bytes, &slot->summary.bytes);
    catalog->summary.resource_count--;
    catalog->summary.count_by_kind[slot->summary.kind]--;
    if (slot->summary.state == YVEX_ENGINE_RESOURCE_READY)
        catalog->summary.ready_count--;
    else if (slot->summary.state == YVEX_ENGINE_RESOURCE_FAILED)
        catalog->summary.failed_count--;
    catalog->summary.release_count++;
    catalog->summary.eviction_count += evicted != 0;
    catalog->summary.generation++;
    memset(slot, 0, sizeof(*slot));
}

static int resource_release(
    yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_handle handle, int evict, yvex_error *err)
{
    engine_resource_slot *slot;
    yvex_engine_resource_release_fn release;
    void *release_context;
    int rc = YVEX_OK;
    if (!catalog || !handle_present(handle) ||
        !catalog->mutex_ready || pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "one live engine resource handle is required");
    slot = resource_slot_locked(catalog, handle);
    if (!slot || slot->summary.state == YVEX_ENGINE_RESOURCE_RELEASING ||
        slot->summary.consumer_count || slot->summary.dependent_count ||
        (evict && !slot->summary.evictable)) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, slot ? YVEX_ERR_STATE : YVEX_ERR_INVALID_ARG,
            slot && evict && !slot->summary.evictable
                ? "resource is not eligible for eviction"
                : "resource is stale, referenced, or still has dependents");
    }
    if (slot->summary.state == YVEX_ENGINE_RESOURCE_READY)
        catalog->summary.ready_count--;
    else if (slot->summary.state == YVEX_ENGINE_RESOURCE_FAILED)
        catalog->summary.failed_count--;
    slot->summary.state = YVEX_ENGINE_RESOURCE_RELEASING;
    release = slot->release;
    release_context = slot->release_context;
    (void)pthread_mutex_unlock(&catalog->mutex);
    if (release) rc = release(release_context, err);
    if (pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "resource release synchronization was lost");
    slot = resource_slot_locked(catalog, handle);
    if (!slot) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "resource generation changed during release");
    }
    if (rc == YVEX_OK) {
        resource_remove_locked(catalog, slot, evict);
        yvex_error_clear(err);
    } else {
        slot->summary.state = YVEX_ENGINE_RESOURCE_FAILED;
        catalog->summary.failed_count++;
        catalog->summary.failed_release_count++;
    }
    (void)pthread_mutex_unlock(&catalog->mutex);
    return rc;
}

int yvex_runtime_resource_catalog_reserve(
    yvex_engine_resource_catalog *catalog, unsigned long long capacity,
    unsigned long long maximum_host_bytes, unsigned long long *host_bytes, yvex_error *err)
{
    unsigned long long retained, peak;
    engine_resource_slot *slots = NULL;
    int rc = YVEX_OK;
    if (host_bytes) *host_bytes = 0u;
    if (!catalog || !host_bytes || !capacity || capacity > 1024u ||
        !catalog->mutex_ready || pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(err, YVEX_ERR_INVALID_ARG, "one bounded live resource catalog is required");
    if (catalog->summary.closing) {
        rc = resource_refuse(err, YVEX_ERR_STATE, "closing resource catalog cannot reserve new capacity");
        goto finish;
    }
    if (capacity < catalog->summary.capacity) capacity = catalog->summary.capacity;
    if (!yvex_core_u64_mul(capacity, sizeof(*slots), &retained) ||
        !yvex_core_u64_add(retained, sizeof(*catalog), &retained)) {
        rc = resource_refuse(err, YVEX_ERR_BOUNDS, "resource catalog metadata overflowed");
        goto finish;
    }
    peak = retained;
    if (capacity > catalog->summary.capacity &&
        !yvex_core_u64_add(peak, catalog->summary.capacity * sizeof(*slots), &peak)) {
        rc = resource_refuse(err, YVEX_ERR_BOUNDS, "resource catalog relocation overflowed");
        goto finish;
    }
    if (peak > SIZE_MAX || (maximum_host_bytes && peak > maximum_host_bytes)) {
        rc = resource_refuse(err, YVEX_ERR_BOUNDS, "resource catalog metadata exceeds host budget");
        goto finish;
    }
    if (capacity > catalog->summary.capacity) {
        if (catalog->summary.generation == ULLONG_MAX) {
            rc = resource_refuse(err, YVEX_ERR_BOUNDS, "resource catalog generation is exhausted");
            goto finish;
        }
        slots = calloc((size_t)capacity, sizeof(*slots));
        if (!slots) {
            rc = resource_refuse(err, YVEX_ERR_NOMEM, "resource catalog relocation allocation failed");
            goto finish;
        }
        if (catalog->slots) memcpy(slots, catalog->slots,
            (size_t)catalog->summary.capacity * sizeof(*slots));
        free(catalog->slots);
        catalog->slots = slots;
        /* Initial construction is not a mutation of a published catalog. */
        catalog->summary.generation += catalog->summary.capacity != 0u;
        catalog->summary.capacity = capacity;
    }
    *host_bytes = retained;
    yvex_error_clear(err);
finish:
    (void)pthread_mutex_unlock(&catalog->mutex);
    return rc;
}

int yvex_runtime_resource_catalog_open(
    yvex_engine_resource_catalog **out, unsigned long long engine_generation,
    const char *engine_identity, unsigned long long capacity, yvex_error *err)
{
    yvex_engine_resource_catalog *catalog;
    if (out) *out = NULL;
    if (!out || !engine_generation || !yvex_sha256_hex_valid(engine_identity) ||
        !capacity || capacity > 1024ull ||
        capacity > SIZE_MAX / sizeof(*catalog->slots))
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "bounded engine generation and identity are required");
    catalog = calloc(1u, sizeof(*catalog));
    if (!catalog || pthread_mutex_init(&catalog->mutex, NULL) != 0) {
        free(catalog);
        return resource_refuse(
            err, YVEX_ERR_NOMEM,
            "engine resource catalog allocation failed");
    }
    catalog->mutex_ready = 1;
    catalog->summary.engine_generation = engine_generation;
    unsigned long long metadata;
    int rc = yvex_runtime_resource_catalog_reserve(catalog, capacity, 0u, &metadata, err);
    if (rc != YVEX_OK) {
        (void)pthread_mutex_destroy(&catalog->mutex);
        free(catalog);
        return rc;
    }
    yvex_core_text_copy(catalog->summary.engine_identity,
                        sizeof(catalog->summary.engine_identity),
                        engine_identity);
    *out = catalog;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_resource_register(
    yvex_engine_resource_catalog *catalog,
    const yvex_engine_resource_request *request,
    yvex_engine_resource_handle *handle, yvex_error *err)
{
    engine_resource_slot *slot = NULL, *dependency = NULL;
    yvex_engine_resource_bytes totals;
    unsigned long long index, preparation_total;
    if (handle) memset(handle, 0, sizeof(*handle));
    if (!resource_request_valid(catalog, request) || !handle ||
        !catalog->mutex_ready || pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "one complete engine resource declaration is required");
    if (catalog->summary.closing) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "closing engine resource catalog rejects registration");
    }
    if (handle_present(request->dependency)) {
        dependency = resource_slot_locked(catalog, request->dependency);
        if (!dependency ||
            dependency->summary.state != YVEX_ENGINE_RESOURCE_READY) {
            (void)pthread_mutex_unlock(&catalog->mutex);
            return resource_refuse(
                err, YVEX_ERR_STATE,
                "resource dependency is stale or not ready");
        }
    }
    for (index = 0ull; index < catalog->summary.capacity; ++index)
        if (!catalog->slots[index].in_use) {
            slot = &catalog->slots[index];
            break;
        }
    totals = catalog->summary.bytes;
    if (!slot || !resource_bytes_add(&totals, &request->bytes) ||
        !yvex_core_u64_add(catalog->summary.preparation_nanoseconds,
                           request->preparation_nanoseconds,
                           &preparation_total)) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, slot ? YVEX_ERR_BOUNDS : YVEX_ERR_NOMEM,
            slot ? "engine resource accounting overflowed"
                 : "engine resource catalog capacity is exhausted");
    }
    catalog->next_generation++;
    if (!catalog->next_generation) catalog->next_generation++;
    memset(slot, 0, sizeof(*slot));
    slot->in_use = 1;
    slot->summary.handle.engine_generation =
        catalog->summary.engine_generation;
    slot->summary.handle.slot = index + 1ull;
    slot->summary.handle.generation = catalog->next_generation;
    slot->summary.dependency = request->dependency;
    slot->summary.kind = request->kind;
    slot->summary.owner = request->owner;
    slot->summary.lifetime = request->lifetime;
    slot->summary.numeric_class = request->numeric_class;
    slot->summary.state = request->ready ? YVEX_ENGINE_RESOURCE_READY
                                         : YVEX_ENGINE_RESOURCE_DECLARED;
    slot->summary.bytes = request->bytes;
    slot->summary.preparation_nanoseconds = request->preparation_nanoseconds;
    slot->summary.evictable = request->evictable != 0;
    yvex_core_text_copy(slot->summary.name, sizeof(slot->summary.name),
                        request->name);
    yvex_core_text_copy(slot->summary.package_identity,
                        sizeof(slot->summary.package_identity),
                        request->package_identity);
    if (request->specialization_identity)
        yvex_core_text_copy(slot->summary.specialization_identity,
                            sizeof(slot->summary.specialization_identity),
                            request->specialization_identity);
    if (request->admission_identity)
        yvex_core_text_copy(slot->summary.admission_identity,
                            sizeof(slot->summary.admission_identity),
                            request->admission_identity);
    slot->value = request->value;
    slot->release = request->release;
    slot->release_context = request->release_context;
    if (dependency) dependency->summary.dependent_count++;
    catalog->summary.bytes = totals;
    catalog->summary.resource_count++;
    catalog->summary.registration_count++;
    catalog->summary.preparation_nanoseconds = preparation_total;
    catalog->summary.generation++;
    catalog->summary.count_by_kind[request->kind]++;
    catalog->summary.ready_count += request->ready != 0;
    *handle = slot->summary.handle;
    (void)pthread_mutex_unlock(&catalog->mutex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_resource_acquire(
    yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_handle handle, void **value, yvex_error *err)
{
    engine_resource_slot *slot;
    if (value) *value = NULL;
    if (!catalog || !value || !catalog->mutex_ready ||
        pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "resource catalog and borrowed-value output are required");
    slot = resource_slot_locked(catalog, handle);
    if (!slot || catalog->summary.closing ||
        slot->summary.state != YVEX_ENGINE_RESOURCE_READY ||
        slot->summary.consumer_count == ULLONG_MAX ||
        slot->summary.acquisition_count == ULLONG_MAX ||
        catalog->summary.acquisition_count == ULLONG_MAX) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "ready non-stale engine resource is required");
    }
    slot->summary.consumer_count++;
    slot->summary.acquisition_count++;
    catalog->summary.acquisition_count++;
    *value = slot->value;
    (void)pthread_mutex_unlock(&catalog->mutex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_resource_publish_ready(
    yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_handle handle, yvex_error *err)
{
    engine_resource_slot *slot;
    if (!catalog || !catalog->mutex_ready ||
        pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(err, YVEX_ERR_INVALID_ARG,
                               "resource catalog is required");
    slot = resource_slot_locked(catalog, handle);
    if (!slot || catalog->summary.closing ||
        slot->summary.state != YVEX_ENGINE_RESOURCE_DECLARED) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "one declared resource in an open catalog is required");
    }
    slot->summary.state = YVEX_ENGINE_RESOURCE_READY;
    catalog->summary.ready_count++;
    catalog->summary.generation++;
    (void)pthread_mutex_unlock(&catalog->mutex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_resource_drop(
    yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_handle handle, yvex_error *err)
{
    engine_resource_slot *slot;
    if (!catalog || !catalog->mutex_ready ||
        pthread_mutex_lock(&catalog->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "resource catalog is required");
    slot = resource_slot_locked(catalog, handle);
    if (!slot || !slot->summary.consumer_count) {
        (void)pthread_mutex_unlock(&catalog->mutex);
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "one exact borrowed resource generation is required");
    }
    slot->summary.consumer_count--;
    (void)pthread_mutex_unlock(&catalog->mutex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_resource_evict(
    yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_handle *handle, yvex_error *err)
{
    int rc;
    if (!handle)
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "resource eviction handle is required");
    rc = resource_release(catalog, *handle, 1, err);
    if (rc == YVEX_OK) memset(handle, 0, sizeof(*handle));
    return rc;
}

int yvex_runtime_resource_snapshot(
    const yvex_engine_resource_catalog *catalog,
    yvex_engine_resource_summary *summary,
    yvex_engine_resource_entry *entries, unsigned long long entry_capacity,
    unsigned long long *entry_count, yvex_error *err)
{
    yvex_engine_resource_catalog *owner =
        (yvex_engine_resource_catalog *)catalog;
    unsigned long long index, count = 0ull;
    if (entry_count) *entry_count = 0ull;
    if (!catalog || !summary || !entry_count ||
        ((!entries) != (entry_capacity == 0ull)) ||
        !owner->mutex_ready || pthread_mutex_lock(&owner->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "resource catalog snapshot outputs are required");
    *summary = catalog->summary;
    for (index = 0ull; index < catalog->summary.capacity; ++index) {
        if (!catalog->slots[index].in_use) continue;
        if (entries && count < entry_capacity)
            entries[count] = catalog->slots[index].summary;
        count++;
    }
    *entry_count = count;
    (void)pthread_mutex_unlock(&owner->mutex);
    if (entries && count > entry_capacity)
        return resource_refuse(
            err, YVEX_ERR_BOUNDS,
            "resource snapshot capacity is insufficient");
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_resource_catalog_close(
    yvex_engine_resource_catalog **catalog, yvex_error *err)
{
    yvex_engine_resource_catalog *owner;
    yvex_engine_resource_handle candidate = {0};
    unsigned long long index;
    int rc;
    if (!catalog || !*catalog) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    owner = *catalog;
    if (!owner->mutex_ready || pthread_mutex_lock(&owner->mutex) != 0)
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "resource catalog close synchronization is unavailable");
    owner->summary.closing = 1;
    for (index = owner->summary.capacity; index > 0ull; --index) {
        engine_resource_slot *slot = &owner->slots[index - 1ull];
        if (slot->in_use && !slot->summary.consumer_count &&
            !slot->summary.dependent_count) {
            candidate = slot->summary.handle;
            break;
        }
    }
    if (!handle_present(candidate) && owner->summary.resource_count) {
        (void)pthread_mutex_unlock(&owner->mutex);
        return resource_refuse(
            err, YVEX_ERR_STATE,
            "resource catalog still owns referenced or cyclic resources");
    }
    (void)pthread_mutex_unlock(&owner->mutex);
    while (handle_present(candidate)) {
        rc = resource_release(owner, candidate, 0, err);
        if (rc != YVEX_OK) return rc;
        memset(&candidate, 0, sizeof(candidate));
        if (pthread_mutex_lock(&owner->mutex) != 0)
            return resource_refuse(
                err, YVEX_ERR_STATE,
                "resource catalog close synchronization was lost");
        for (index = owner->summary.capacity; index > 0ull; --index) {
            engine_resource_slot *slot = &owner->slots[index - 1ull];
            if (slot->in_use && !slot->summary.consumer_count &&
                !slot->summary.dependent_count) {
                candidate = slot->summary.handle;
                break;
            }
        }
        if (!handle_present(candidate) && owner->summary.resource_count) {
            (void)pthread_mutex_unlock(&owner->mutex);
            return resource_refuse(
                err, YVEX_ERR_STATE,
                "resource catalog close found a dependency cycle");
        }
        (void)pthread_mutex_unlock(&owner->mutex);
    }
    if (owner->mutex_ready) (void)pthread_mutex_destroy(&owner->mutex);
    free(owner->slots);
    memset(owner, 0, sizeof(*owner));
    free(owner);
    *catalog = NULL;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_cleanup_lease_close(
    yvex_runtime_cleanup_lease **lease_ptr, yvex_error *err)
{
    yvex_runtime_cleanup_lease *lease;
    int rc;
    if (!lease_ptr || !*lease_ptr) return yvex_runtime_private_success(err);
    lease = *lease_ptr;
    if (lease->dependent_context && !lease->dependent_release) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.cleanup.dependent",
                       "dependent cleanup operation is unavailable");
        return YVEX_ERR_STATE;
    }
    if (lease->dependent_context) {
        rc = lease->dependent_release(&lease->dependent_context, err);
        if (rc != YVEX_OK) return rc;
        if (lease->dependent_context) {
            yvex_error_set(
                err, YVEX_ERR_STATE, "runtime.cleanup.dependent",
                "dependent cleanup reported success while retaining ownership");
            return YVEX_ERR_STATE;
        }
        lease->dependent_release = NULL;
    }
    rc = yvex_runtime_session_close(&lease->session, err);
    if (rc != YVEX_OK) return rc;
    yvex_model_engine_close(&lease->model);
    if (lease->model) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.cleanup.model-close",
                       "runtime model cleanup retained ownership for retry");
        return YVEX_ERR_STATE;
    }
    free(lease);
    *lease_ptr = NULL;
    return yvex_runtime_private_success(err);
}

int yvex_runtime_cleanup_lease_adopt(
    yvex_runtime_cleanup_lease *lease, void *context,
    yvex_runtime_cleanup_release_fn release, yvex_error *err)
{
    if (!lease || !context || !release || lease->dependent_context ||
        lease->dependent_release) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime.cleanup.adopt",
                       "one empty cleanup lease dependent slot is required");
        return YVEX_ERR_INVALID_ARG;
    }
    lease->dependent_context = context;
    lease->dependent_release = release;
    return yvex_runtime_private_success(err);
}

int yvex_runtime_cleanup_lease_acquire(
    yvex_runtime_cleanup_lease **out,
    const yvex_model_engine_open_request *model_request,
    const yvex_runtime_session_open_request *session_request,
    yvex_model_engine **borrowed_model,
    yvex_runtime_execution_session **borrowed_session,
    yvex_model_engine_failure *failure, yvex_error *err)
{
    yvex_runtime_cleanup_lease *lease;
    yvex_error primary, cleanup;
    int rc, cleanup_rc;
    if (borrowed_model) *borrowed_model = NULL;
    if (borrowed_session) *borrowed_session = NULL;
    if (!out || *out || !model_request || !borrowed_model ||
        (session_request && !borrowed_session))
        return yvex_runtime_private_reject(
            failure, YVEX_MODEL_ENGINE_FAILURE_INVALID_ARGUMENT,
            "cleanup-lease", 1ull,
            out && !*out && model_request && borrowed_model &&
                    (!session_request || borrowed_session)
                ? 1ull : 0ull,
            "empty cleanup lease, model request, and borrowed outputs are required",
            err, YVEX_ERR_INVALID_ARG);
    lease = (yvex_runtime_cleanup_lease *)calloc(1u, sizeof(*lease));
    if (!lease)
        return yvex_runtime_private_reject(
            failure, YVEX_MODEL_ENGINE_FAILURE_ALLOCATION, "cleanup-lease",
            1ull, 0ull, "lease allocation failed", err, YVEX_ERR_NOMEM);
    *out = lease;
    rc = yvex_model_engine_open(&lease->model, model_request, failure, err);
    if (rc == YVEX_OK) *borrowed_model = lease->model;
    if (rc == YVEX_OK && session_request)
        rc = yvex_runtime_cleanup_lease_session_open(
            lease, session_request, borrowed_session, failure, err);
    if (rc == YVEX_OK) return YVEX_OK;
    *borrowed_model = NULL;
    if (borrowed_session) *borrowed_session = NULL;
    primary = err ? *err : (yvex_error){0};
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_runtime_cleanup_lease_close(out, &cleanup);
    if (cleanup_rc != YVEX_OK) {
        yvex_runtime_private_failure_record(
            failure, YVEX_MODEL_ENGINE_FAILURE_CLEANUP, "cleanup-lease", 0ull,
            1ull, "runtime acquisition cleanup retained ownership for retry");
        if (err) *err = cleanup;
        return cleanup_rc;
    }
    if (err) *err = primary;
    return rc;
}

int yvex_runtime_cleanup_lease_session_open(
    yvex_runtime_cleanup_lease *lease,
    const yvex_runtime_session_open_request *request,
    yvex_runtime_execution_session **borrowed_session,
    yvex_model_engine_failure *failure, yvex_error *err)
{
    int rc;
    if (borrowed_session) *borrowed_session = NULL;
    if (!lease || !lease->model || lease->session || !request ||
        !borrowed_session)
        return yvex_runtime_private_reject(
            failure, YVEX_MODEL_ENGINE_FAILURE_INVALID_ARGUMENT,
            "cleanup-lease-session", 1ull, 0ull,
            "model-owning cleanup lease and session request are required", err,
            YVEX_ERR_INVALID_ARG);
    rc = yvex_runtime_session_open(
        &lease->session, lease->model, request, failure, err);
    if (rc == YVEX_OK) *borrowed_session = lease->session;
    return rc;
}
