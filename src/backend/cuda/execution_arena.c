/* Own bounded CUDA work allocations and admitted arenas with stable tensor views. */
#include "src/backend/cuda/attention_ops.h"
#include <yvex/internal/graph_state.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Lower one sealed family-neutral workspace recipe to a checked byte extent.
 *
 * Pointer-free semantic components with explicit alignment and token scaling. Malformed identity
 * or arithmetic overflow leaves required bytes zero. Backend owns alignment lowering, while
 * graph/family owners select components.
 */
static int attention_workspace_required_from_recipe(
    const struct yvex_attention_workspace_recipe *recipe,
    unsigned long long *required_bytes, int host, int device_input, yvex_error *err)
{
    yvex_attention_workspace_recipe candidate;
    unsigned long long cursor = 0ull;
    unsigned int index;
    if (required_bytes) *required_bytes = 0ull;
    if (!recipe || !required_bytes || (device_input != 0 && device_input != 1) ||
        recipe->schema_version != YVEX_ATTENTION_WORKSPACE_RECIPE_SCHEMA_V3 ||
        !yvex_sha256_hex_valid(recipe->identity)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.attention.workspace",
                       "one sealed attention workspace recipe is required");
        return YVEX_ERR_INVALID_ARG;
    }
    candidate = *recipe;
    if (yvex_attention_workspace_recipe_seal(&candidate, err) != YVEX_OK)
        return err ? yvex_error_code(err) : YVEX_ERR_FORMAT;
    if (strcmp(candidate.identity, recipe->identity) != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "cuda.attention.workspace",
                       "attention workspace recipe identity is stale");
        return YVEX_ERR_STATE;
    }
    for (index = 0u; index < recipe->component_count; ++index) {
        const yvex_attention_workspace_component *component = &recipe->components[index];
        unsigned long long count = component->element_count, bytes, aligned;
        unsigned long long scale =
            component->scales_with_tokens ? recipe->token_capacity : 1ull;
        /* Device ranking may execute independent query rows together. The
         * authored recipe keeps token-local scratch; backend lowering reserves
         * a bounded row tile without changing persisted semantic identity. */
        if (!host && (component->kind == YVEX_ATTENTION_WORKSPACE_TOPK_SCORES ||
                      component->kind == YVEX_ATTENTION_WORKSPACE_TOPK_VALID_INDICES))
            scale = recipe->token_capacity < YVEX_CUDA_ATTENTION_SELECTION_ROWS
                ? recipe->token_capacity : YVEX_CUDA_ATTENTION_SELECTION_ROWS;
        unsigned long long mask = component->alignment - 1ull;
        /* CUDA ingress is borrowed from the admitted device activation. Host
         * input jobs retain this component; forensic evidence is unchanged. */
        if (host && device_input && recipe->evidence_level == YVEX_ATTENTION_EVIDENCE_NONE &&
            component->kind == YVEX_ATTENTION_WORKSPACE_INGRESS) continue;
        /* Production host staging and retained graph publications do not own
         * device-only projection scratch. Keep ingress, histories, outputs,
         * state deltas and counters; forensic evidence keeps the full recipe. */
        if (host && recipe->evidence_level == YVEX_ATTENTION_EVIDENCE_NONE &&
            (component->kind == YVEX_ATTENTION_WORKSPACE_Q_LOW ||
             component->kind == YVEX_ATTENTION_WORKSPACE_QUERY ||
             component->kind == YVEX_ATTENTION_WORKSPACE_ATTENTION_VALUES ||
             component->kind == YVEX_ATTENTION_WORKSPACE_ENVELOPE_OUTPUT ||
             component->kind == YVEX_ATTENTION_WORKSPACE_OUTPUT ||
             component->kind == YVEX_ATTENTION_WORKSPACE_ENVELOPE_STAGING ||
             component->kind == YVEX_ATTENTION_WORKSPACE_TOPK_INDICES ||
             component->kind == YVEX_ATTENTION_WORKSPACE_TOPK_POSITIONS ||
             component->kind == YVEX_ATTENTION_WORKSPACE_TOPK_SCORES ||
             component->kind == YVEX_ATTENTION_WORKSPACE_TOPK_VALID_INDICES ||
             component->kind == YVEX_ATTENTION_WORKSPACE_OUTPUT_LOW ||
             (component->kind >= YVEX_ATTENTION_WORKSPACE_MAIN_PROJECTED_VALUES &&
              component->kind <= YVEX_ATTENTION_WORKSPACE_INDEXER_PROJECTED_SCORES) ||
             component->kind == YVEX_ATTENTION_WORKSPACE_INDEX_QUERY ||
             component->kind == YVEX_ATTENTION_WORKSPACE_INDEX_WEIGHTS)) continue;
        if (!component->scales_with_tokens &&
            component->kind >= YVEX_ATTENTION_WORKSPACE_MAIN_ROLLING_VALUES &&
                 component->kind <= YVEX_ATTENTION_WORKSPACE_INDEXER_ROLLING_SCORES &&
                 !yvex_core_u64_add(recipe->prefix_checkpoint_capacity, 1ull, &scale))
            goto overflow;
        if (component->kind >= YVEX_ATTENTION_WORKSPACE_MAIN_ROLLING_CANDIDATE_VALUES &&
            component->kind <= YVEX_ATTENTION_WORKSPACE_INDEXER_ROLLING_CANDIDATE_SCORES)
            scale = recipe->prefix_checkpoint_capacity ? recipe->prefix_checkpoint_capacity : 1ull;
        if (!yvex_core_u64_mul(count, scale, &count) ||
            !yvex_core_u64_mul(count, component->element_width, &bytes) ||
            (component->lifetime != YVEX_ATTENTION_WORKSPACE_GRAPH_STABLE &&
             !(host && recipe->evidence_level == YVEX_ATTENTION_EVIDENCE_NONE) &&
             !yvex_core_u64_add(bytes, bytes, &bytes)) ||
            cursor > ULLONG_MAX - mask) goto overflow;
        aligned = (cursor + mask) & ~mask;
        if (aligned > ULLONG_MAX - bytes) goto overflow;
        cursor = aligned + bytes;
    }
    if (!host && recipe->evidence_level < YVEX_ATTENTION_EVIDENCE_FULL) {
        unsigned long long decoded = yvex_cuda_decoded_workspace(recipe->token_capacity);
        if (decoded && (!yvex_core_u64_add(cursor, 255ull, &cursor) ||
                        !yvex_core_u64_add(cursor & ~255ull, decoded, &cursor)))
            goto overflow;
    }
    *required_bytes = cursor;
    yvex_error_clear(err);
    return YVEX_OK;
overflow:
    yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.attention.workspace",
                   "attention workspace recipe overflowed backend address space");
    return YVEX_ERR_BOUNDS;
}

int yvex_backend_attention_workspace_required_from_recipe(
    const struct yvex_attention_workspace_recipe *recipe,
    unsigned long long *required_bytes, yvex_error *err)
{
    return attention_workspace_required_from_recipe(recipe, required_bytes, 0, 0, err);
}

int yvex_backend_attention_host_workspace_required_from_recipe(
    const struct yvex_attention_workspace_recipe *recipe,
    int device_input, unsigned long long *required_bytes, yvex_error *err)
{
    return attention_workspace_required_from_recipe(recipe, required_bytes, 1, device_input, err);
}

/* Initialize one stable work range without breaking active stream capture. */
int yvex_cuda_work_initialize(yvex_cuda_work *work, CUdeviceptr target,
                              size_t bytes, const void *source, int zero,
                              const char *stage, yvex_error *err)
{
    CUstream stream;
    if (!source && !zero) return YVEX_OK;
    if (!work || !work->state || !target || !bytes) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "CUDA range initialization is invalid");
        return YVEX_ERR_INVALID_ARG;
    }
    if (work->prepare_only) return YVEX_OK;
    stream = yvex_cuda_launch_stream(work->backend);
    if (stream) {
        if ((source && !work->state->driver.cuMemcpyHtoDAsync_v2) ||
            (zero && !work->state->driver.cuMemsetD8Async)) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, stage,
                           "captured CUDA range initialization is unavailable");
            return YVEX_ERR_UNSUPPORTED;
        }
        return yvex_cuda_status(
            &work->state->driver,
            source ? work->state->driver.cuMemcpyHtoDAsync_v2(
                         target, source, bytes, stream)
                   : work->state->driver.cuMemsetD8Async(target, 0u, bytes, stream),
            stage, err);
    }
    return yvex_cuda_status(
        &work->state->driver,
        source ? work->state->driver.cuMemcpyHtoD_v2(target, source, bytes)
               : work->state->driver.cuMemsetD8_v2(target, 0u, bytes),
        stage, err);
}

/* Borrow the enclosing program's latched status without resetting it. A
 * standalone call keeps its independent allocation/completion contract. */
int yvex_cuda_work_status(yvex_cuda_work *work, CUdeviceptr *out,
                         const char *stage, yvex_error *err)
{
    if (!work || !work->state || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage, "CUDA status owner required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (work->state->program_status_active) {
        *out = work->state->program_status;
        work->status_scoped = 1;
        return YVEX_OK;
    }
    return yvex_cuda_work_allocate(work, out, sizeof(int), NULL, 1, stage, NULL, err);
}

/* Own one bounded allocation transaction across reusable and temporary device storage. */
int yvex_cuda_work_allocate(yvex_cuda_work *work,
                            CUdeviceptr *out,
                            size_t bytes,
                            const void *source,
                            int zero,
                            const char *stage,
                            yvex_cuda_work_failure *failure,
                            yvex_error *err)
{
    unsigned long long address = 0ull;
    unsigned long long next;
    int acquired;
    int rc;
    if (failure) *failure = YVEX_CUDA_WORK_FAILURE_NONE;
    if (!work || !work->backend || !work->state || !out || !bytes ||
        work->count >= YVEX_CUDA_WORK_MAX_RANGES) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "CUDA work allocation request is invalid");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = backend_dispatch_admit(work->backend, stage, err);
    if (rc != YVEX_OK) {
        if (failure) *failure = YVEX_CUDA_WORK_FAILURE_ALLOCATION;
        return rc;
    }
    if (work->count == 0u) {
        rc = yvex_cuda_deferred_release_drain(work->backend, err);
        if (rc != YVEX_OK) {
            if (failure) *failure = YVEX_CUDA_WORK_FAILURE_ALLOCATION;
            return rc;
        }
    }
    if (work->current_bytes > ULLONG_MAX - (unsigned long long)bytes ||
        (work->budget && work->current_bytes + (unsigned long long)bytes > work->budget)) {
        if (failure) *failure = YVEX_CUDA_WORK_FAILURE_BUDGET;
        yvex_error_set(err, YVEX_ERR_NOMEM, stage, "CUDA work device-byte budget exceeded");
        return YVEX_ERR_NOMEM;
    }
    next = work->current_bytes + (unsigned long long)bytes;
    acquired = work->raw_only ? YVEX_BACKEND_RESIDENT_MISS
                              : yvex_backend_workspace_acquire(
                                    work->backend, bytes, 256ull, &address);
    if (acquired == YVEX_BACKEND_RESIDENT_HIT) {
        *out = (CUdeviceptr)address;
        work->workspace_owned[work->count] = 1u;
    } else if (!work->raw_only && work->backend->workspace_device_tensor) {
        if (failure) *failure = YVEX_CUDA_WORK_FAILURE_BUDGET;
        yvex_error_setf(
            err, acquired == YVEX_BACKEND_RESIDENT_INVALID
                     ? YVEX_ERR_BOUNDS : YVEX_ERR_NOMEM,
            stage,
            "CUDA reusable workspace needs %zu bytes at cursor %llu of %llu",
            bytes, work->backend->workspace_cursor, work->backend->workspace_bytes);
        return acquired == YVEX_BACKEND_RESIDENT_INVALID ? YVEX_ERR_BOUNDS
                                                         : YVEX_ERR_NOMEM;
    } else {
        rc = yvex_backend_memory_can_add(work->backend, bytes, "CUDA", stage, err);
        if (rc != YVEX_OK) {
            if (failure) *failure = YVEX_CUDA_WORK_FAILURE_BUDGET;
            return rc;
        }
        rc = yvex_cuda_status(&work->state->driver,
                              work->state->driver.cuMemAlloc_v2(out, bytes), stage, err);
        if (rc != YVEX_OK) {
            if (failure) *failure = YVEX_CUDA_WORK_FAILURE_ALLOCATION;
            return rc;
        }
        backend_memory_acquire(work->backend, bytes);
    }
    work->pointers[work->count] = *out;
    work->sizes[work->count++] = bytes;
    work->current_bytes = next;
    if (next > work->peak_bytes) work->peak_bytes = next;
    rc = yvex_cuda_work_initialize(work, *out, bytes, source, zero, stage, err);
    if (rc != YVEX_OK && failure) *failure = YVEX_CUDA_WORK_FAILURE_COPY;
    return rc;
}

/* Release an allocation transaction in reverse order while preserving failed ownership. */
int yvex_cuda_work_cleanup(yvex_cuda_work *work, yvex_error *err)
{
    yvex_error cleanup;
    int result = YVEX_OK;
    if (!work) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    while (work->count) {
        unsigned int index = work->count - 1u;
        int rc = work->workspace_owned[index]
                     ? YVEX_OK
                     : yvex_cuda_temporary_free(
                           work->backend, work->variant, &work->pointers[index],
                           work->sizes[index], 1, "cuda.work.cleanup", &cleanup);
        if (!work->workspace_owned[index] && work->pointers[index] != 0u) {
            if (result == YVEX_OK) {
                result = rc;
                if (err) *err = cleanup;
            }
            break;
        }
        work->current_bytes = work->current_bytes >= work->sizes[index]
                                  ? work->current_bytes - work->sizes[index]
                                  : 0ull;
        work->pointers[index] = 0u;
        work->workspace_owned[index] = 0u;
        work->sizes[index] = 0ull;
        work->count = index;
        if (rc != YVEX_OK && result == YVEX_OK) {
            result = rc;
            if (err) *err = cleanup;
        }
    }
    if (result == YVEX_OK) yvex_error_clear(err);
    return result;
}
