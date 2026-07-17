/*
 * yvex_runtime_descriptor.c - canonical runtime descriptor projection.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   immutable runtime descriptor construction from complete-artifact
 *   admission, committed materialization bindings, qtype/storage facts, and
 *   DeepSeek GGUF map facts.
 *
 * Does not own:
 *   backend allocation, graph binding, graph execution, attention, KV,
 *   prefill, decode, logits, sampling, generation, eval, benchmark, or
 *   release claims.
 *
 * Invariants:
 *   descriptor identity binds artifact identity, materialization plan identity,
 *   tensor names, roles, coordinates, qtypes, placement, and byte counts, but
 *   never stores pointer addresses as identity material.
 *
 * Boundary:
 *   descriptor construction makes the next graph milestone possible; it does
 *   not execute the graph.
 */
#include "yvex_runtime_descriptor.h"

#include "src/core/yvex_sha256.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned long long hash;
    unsigned long long index_plus_one;
} runtime_name_slot;

struct yvex_runtime_descriptor {
    yvex_runtime_tensor_binding *bindings;
    runtime_name_slot *name_index;
    unsigned long long name_index_capacity;
    unsigned long long count;
    yvex_runtime_descriptor_summary summary;
};

static unsigned long long runtime_hash_string(const char *text)
{
    unsigned long long hash = 1469598103934665603ull;
    const unsigned char *cursor = (const unsigned char *)(text ? text : "");
    while (*cursor) {
        hash ^= (unsigned long long)*cursor++;
        hash *= 1099511628211ull;
    }
    return hash ? hash : 1ull;
}

static int runtime_index_capacity(unsigned long long count,
                                  unsigned long long *out)
{
    unsigned long long capacity = 16ull;
    if (!out) return 0;
    while (capacity * 2ull < count * 3ull) {
        if (capacity > ULLONG_MAX / 2ull) return 0;
        capacity *= 2ull;
    }
    *out = capacity;
    return 1;
}

static void runtime_failure_set(yvex_runtime_descriptor_failure *failure,
                                yvex_runtime_descriptor_failure_code code,
                                const char *name,
                                unsigned long long tensor_index,
                                unsigned long long expected,
                                unsigned long long actual,
                                const char *reason)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->code = code;
    failure->tensor_index = tensor_index;
    failure->expected = expected;
    failure->actual = actual;
    failure->reason = reason;
    if (name)
        (void)snprintf(failure->tensor_name,
                       sizeof(failure->tensor_name), "%s", name);
}

static int runtime_reject(yvex_runtime_descriptor_failure *failure,
                          yvex_runtime_descriptor_failure_code code,
                          const char *name,
                          unsigned long long tensor_index,
                          unsigned long long expected,
                          unsigned long long actual,
                          yvex_error *err,
                          yvex_status status,
                          const char *message)
{
    runtime_failure_set(failure, code, name, tensor_index, expected, actual,
                        message);
    yvex_error_set(err, status, "runtime.descriptor", message);
    return status;
}

static int runtime_index_insert(yvex_runtime_descriptor *descriptor,
                                const char *name,
                                unsigned long long index)
{
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long step = 0ull;

    if (!descriptor || !descriptor->name_index ||
        !descriptor->name_index_capacity || !name)
        return 0;
    hash = runtime_hash_string(name);
    slot = hash & (descriptor->name_index_capacity - 1ull);
    while (step < descriptor->name_index_capacity) {
        runtime_name_slot *candidate = &descriptor->name_index[slot];
        if (!candidate->index_plus_one) {
            candidate->hash = hash;
            candidate->index_plus_one = index + 1ull;
            return 1;
        }
        if (candidate->hash == hash &&
            strcmp(descriptor->bindings[candidate->index_plus_one - 1ull]
                       .binding->name, name) == 0)
            return 0;
        slot = (slot + 1ull) & (descriptor->name_index_capacity - 1ull);
        step++;
    }
    return 0;
}

static const yvex_runtime_tensor_binding *runtime_index_find(
    const yvex_runtime_descriptor *descriptor,
    const char *name)
{
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long step = 0ull;

    if (!descriptor || !descriptor->name_index ||
        !descriptor->name_index_capacity || !name)
        return NULL;
    hash = runtime_hash_string(name);
    slot = hash & (descriptor->name_index_capacity - 1ull);
    while (step < descriptor->name_index_capacity) {
        const runtime_name_slot *candidate = &descriptor->name_index[slot];
        if (!candidate->index_plus_one) return NULL;
        if (candidate->hash == hash) {
            const yvex_runtime_tensor_binding *binding =
                &descriptor->bindings[candidate->index_plus_one - 1ull];
            if (binding->binding &&
                strcmp(binding->binding->name, name) == 0)
                return binding;
        }
        slot = (slot + 1ull) & (descriptor->name_index_capacity - 1ull);
        step++;
    }
    return NULL;
}

static void runtime_hash_u64(yvex_sha256 *hash, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;
    for (i = 0u; i < 8u; ++i)
        bytes[i] = (unsigned char)((value >> (8u * i)) & 0xffu);
    (void)yvex_sha256_update(hash, bytes, sizeof(bytes));
}

static void runtime_hash_text(yvex_sha256 *hash, const char *text)
{
    unsigned long long len = text ? (unsigned long long)strlen(text) : 0ull;
    runtime_hash_u64(hash, len);
    if (len) (void)yvex_sha256_update(hash, text, (size_t)len);
}

static void runtime_compute_identity(yvex_runtime_descriptor *descriptor)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long i;

    yvex_sha256_init(&hash);
    runtime_hash_text(&hash, descriptor->summary.artifact_identity);
    runtime_hash_text(&hash,
                      descriptor->summary.materialization_plan_identity);
    runtime_hash_u64(&hash, descriptor->count);
    runtime_hash_u64(&hash, descriptor->summary.payload_bytes);
    runtime_hash_u64(&hash, descriptor->summary.layer_count);
    runtime_hash_u64(&hash, descriptor->summary.mtp_layer_count);
    for (i = 0ull; i < descriptor->count; ++i) {
        const yvex_runtime_tensor_binding *binding = &descriptor->bindings[i];
        runtime_hash_text(&hash, binding->binding ? binding->binding->name : "");
        runtime_hash_u64(&hash, binding->tensor_id);
        runtime_hash_u64(&hash, binding->descriptor_index);
        runtime_hash_u64(&hash, (unsigned long long)binding->role);
        runtime_hash_u64(&hash, (unsigned long long)binding->scope);
        runtime_hash_u64(&hash, binding->layer_index);
        runtime_hash_u64(&hash, binding->predictor_index);
        runtime_hash_u64(&hash, binding->qtype);
        runtime_hash_u64(&hash, (unsigned long long)binding->placement);
        runtime_hash_u64(&hash,
                         binding->binding ? binding->binding->encoded_bytes : 0ull);
    }
    (void)yvex_sha256_final(&hash, digest);
    yvex_sha256_hex(digest,
                    descriptor->summary.runtime_descriptor_identity);
}

static yvex_runtime_descriptor *runtime_descriptor_alloc(
    unsigned long long count,
    yvex_runtime_descriptor_failure *failure,
    yvex_error *err)
{
    yvex_runtime_descriptor *descriptor;
    unsigned long long capacity;

    if (!runtime_index_capacity(count, &capacity)) {
        runtime_reject(failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION,
                       NULL, YVEX_MATERIALIZATION_NO_INDEX, count, 0ull, err,
                       YVEX_ERR_NOMEM,
                       "runtime descriptor index capacity overflow");
        return NULL;
    }
    descriptor = (yvex_runtime_descriptor *)calloc(1u, sizeof(*descriptor));
    if (!descriptor) {
        runtime_reject(failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION,
                       NULL, YVEX_MATERIALIZATION_NO_INDEX, 1ull, 0ull, err,
                       YVEX_ERR_NOMEM,
                       "runtime descriptor allocation failed");
        return NULL;
    }
    descriptor->bindings = (yvex_runtime_tensor_binding *)calloc(
        (size_t)(count ? count : 1ull), sizeof(*descriptor->bindings));
    descriptor->name_index = (runtime_name_slot *)calloc(
        (size_t)capacity, sizeof(*descriptor->name_index));
    if (!descriptor->bindings || !descriptor->name_index) {
        yvex_runtime_descriptor_close(descriptor);
        runtime_reject(failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION,
                       NULL, YVEX_MATERIALIZATION_NO_INDEX, count, 0ull, err,
                       YVEX_ERR_NOMEM,
                       "runtime descriptor binding allocation failed");
        return NULL;
    }
    descriptor->name_index_capacity = capacity;
    descriptor->count = count;
    return descriptor;
}

static void runtime_fill_common_summary(
    yvex_runtime_descriptor *descriptor,
    const yvex_complete_artifact_admission *admission,
    const yvex_materialization_summary *materialization)
{
    descriptor->summary.status = YVEX_RUNTIME_DESCRIPTOR_STATUS_READY;
    (void)snprintf(descriptor->summary.artifact_identity,
                   sizeof(descriptor->summary.artifact_identity), "%s",
                   admission->artifact_identity);
    (void)snprintf(descriptor->summary.materialization_plan_identity,
                   sizeof(descriptor->summary.materialization_plan_identity),
                   "%s", materialization->plan_identity);
    descriptor->summary.tensor_count = materialization->tensor_count;
    descriptor->summary.payload_bytes = materialization->payload_bytes;
    memcpy(descriptor->summary.qtype_tensor_counts,
           materialization->qtype_tensor_counts,
           sizeof(descriptor->summary.qtype_tensor_counts));
    memcpy(descriptor->summary.qtype_bytes,
           materialization->qtype_bytes,
           sizeof(descriptor->summary.qtype_bytes));
    descriptor->summary.tokenizer_metadata_available =
        admission->tokenizer_complete;
    descriptor->summary.graph_execution_ready = 0;
    descriptor->summary.generation_ready = 0;
}

void yvex_runtime_descriptor_refuse(yvex_runtime_descriptor_fact *fact)
{
    if (!fact) return;
    fact->status = "unsupported";
    fact->artifact_status = "missing";
    fact->reason = "runtime descriptor projection requires committed materialization";
    fact->next_row = "V010.RUNTIME.DESCRIPTOR.GGUF.0";
}

int yvex_runtime_descriptor_projection_supported(int artifact_descriptor_ok,
                                                 const char **reason)
{
    if (artifact_descriptor_ok) {
        if (reason) *reason = "artifact descriptor facts present; committed materialization is still required";
        return 1;
    }
    if (reason) *reason = "runtime descriptor projection blocked by missing artifact descriptor";
    return 0;
}

const char *yvex_runtime_descriptor_status_name(yvex_runtime_descriptor_status status)
{
    switch (status) {
    case YVEX_RUNTIME_DESCRIPTOR_STATUS_REFUSED: return "refused";
    case YVEX_RUNTIME_DESCRIPTOR_STATUS_READY: return "runtime-descriptor-ready";
    }
    return "refused";
}

const char *yvex_runtime_descriptor_failure_name(
    yvex_runtime_descriptor_failure_code code)
{
    switch (code) {
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_NONE: return "none";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_ADMISSION: return "admission";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION: return "materialization";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_DUPLICATE_BINDING: return "duplicate-binding";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_MISSING_BINDING: return "missing-binding";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE: return "architecture";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_QTYPE: return "qtype";
    case YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION: return "allocation";
    }
    return "unknown";
}

int yvex_runtime_descriptor_build(
    yvex_runtime_descriptor **out,
    const yvex_complete_artifact_admission *admission,
    const yvex_materialization_session *session,
    yvex_runtime_descriptor_failure *failure,
    yvex_error *err)
{
    const yvex_materialization_summary *materialization;
    yvex_runtime_descriptor *descriptor;
    unsigned long long count;
    unsigned long long i;

    if (out) *out = NULL;
    if (!out || !admission || !session)
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_INVALID_ARGUMENT,
            NULL, YVEX_MATERIALIZATION_NO_INDEX, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "runtime descriptor requires admission and materialization session");
    if (!admission->complete || !admission->materialization_input_ready ||
        admission->runtime_supported)
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ADMISSION,
            NULL, YVEX_MATERIALIZATION_NO_INDEX, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "runtime descriptor requires complete non-runtime artifact admission");
    materialization = yvex_materialization_session_summary(session);
    if (!materialization || !materialization->committed ||
        materialization->status != YVEX_MATERIALIZATION_STATUS_COMMITTED)
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION,
            NULL, YVEX_MATERIALIZATION_NO_INDEX, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "runtime descriptor requires committed materialization");
    count = materialization->tensor_count;
    if (count != admission->tensor_count)
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION,
            NULL, YVEX_MATERIALIZATION_NO_INDEX, admission->tensor_count,
            count, err, YVEX_ERR_FORMAT,
            "materialization tensor count differs from admission");
    descriptor = runtime_descriptor_alloc(count, failure, err);
    if (!descriptor) return yvex_error_code(err);
    runtime_fill_common_summary(descriptor, admission, materialization);
    for (i = 0ull; i < count; ++i) {
        const yvex_materialized_tensor_binding *source =
            yvex_materialization_session_tensor_at(session, i);
        yvex_runtime_tensor_binding *row = &descriptor->bindings[i];
        if (!source) {
            yvex_runtime_descriptor_close(descriptor);
            return runtime_reject(
                failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_MISSING_BINDING,
                NULL, i, 1ull, 0ull, err, YVEX_ERR_FORMAT,
                "materialization binding missing");
        }
        row->tensor_id = source->tensor_id;
        row->descriptor_index = source->descriptor_index;
        row->binding = source;
        row->role = source->role;
        row->collection = source->collection;
        row->scope = source->scope;
        row->layer_index = source->layer_index;
        row->predictor_index = source->predictor_index;
        row->qtype = source->qtype;
        row->placement = source->placement;
        row->access_mode = source->access_mode;
        if (!runtime_index_insert(descriptor, source->name, i)) {
            yvex_runtime_descriptor_close(descriptor);
            return runtime_reject(
                failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_DUPLICATE_BINDING,
                source->name, i, 1ull, 2ull, err, YVEX_ERR_FORMAT,
                "duplicate runtime descriptor binding name");
        }
        if (source->role < YVEX_TENSOR_ROLE_COUNT)
            descriptor->summary.role_counts[source->role]++;
    }
    runtime_compute_identity(descriptor);
    *out = descriptor;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_runtime_descriptor_build_deepseek(
    yvex_runtime_descriptor **out,
    const yvex_complete_artifact_admission *admission,
    const yvex_materialization_session *session,
    const yvex_deepseek_gguf_map *deepseek_map,
    yvex_runtime_descriptor_failure *failure,
    yvex_error *err)
{
    const yvex_deepseek_gguf_map_summary *map_summary;
    yvex_runtime_descriptor *descriptor = NULL;
    unsigned long long i;
    int rc;

    if (out) *out = NULL;
    if (!deepseek_map)
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_INVALID_ARGUMENT,
            NULL, YVEX_MATERIALIZATION_NO_INDEX, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek runtime descriptor requires canonical GGUF map");
    map_summary = yvex_deepseek_gguf_map_summary_get(deepseek_map);
    if (!map_summary || !map_summary->complete)
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE,
            NULL, YVEX_MATERIALIZATION_NO_INDEX, 1ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek GGUF map is not complete");
    rc = yvex_runtime_descriptor_build(
        &descriptor, admission, session, failure, err);
    if (rc != YVEX_OK) return rc;
    if (descriptor->count != map_summary->descriptor_count) {
        yvex_runtime_descriptor_close(descriptor);
        return runtime_reject(
            failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION,
            NULL, YVEX_MATERIALIZATION_NO_INDEX,
            map_summary->descriptor_count, descriptor->count, err,
            YVEX_ERR_FORMAT,
            "DeepSeek runtime descriptor count differs from map");
    }
    descriptor->summary.layer_count = 61ull;
    descriptor->summary.mtp_layer_count = 1ull;
    descriptor->summary.routed_experts = 256ull;
    descriptor->summary.experts_per_token = 8ull;
    descriptor->summary.vocabulary_size = 129280ull;
    for (i = 0ull; i < map_summary->descriptor_count; ++i) {
        const yvex_deepseek_gguf_descriptor *map_row =
            yvex_deepseek_gguf_map_at(deepseek_map, i);
        const yvex_runtime_tensor_binding *runtime_row;
        if (!map_row) {
            yvex_runtime_descriptor_close(descriptor);
            return runtime_reject(
                failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE,
                NULL, i, 1ull, 0ull, err, YVEX_ERR_FORMAT,
                "DeepSeek GGUF map row missing");
        }
        runtime_row = yvex_runtime_descriptor_find_name(
            descriptor, map_row->emitted_name);
        if (!runtime_row) {
            descriptor->summary.missing_required_bindings++;
            yvex_runtime_descriptor_close(descriptor);
            return runtime_reject(
                failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_MISSING_BINDING,
                map_row->emitted_name, i, 1ull, 0ull, err,
                YVEX_ERR_FORMAT,
                "DeepSeek required tensor is missing from materialization");
        }
        if (runtime_row->role != map_row->role ||
            runtime_row->scope != map_row->scope ||
            runtime_row->layer_index != map_row->layer_index ||
            runtime_row->predictor_index != map_row->predictor_index) {
            yvex_runtime_descriptor_close(descriptor);
            return runtime_reject(
                failure, YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE,
                map_row->emitted_name, i, 1ull, 0ull, err,
                YVEX_ERR_FORMAT,
                "DeepSeek runtime binding coordinates differ from map");
        }
    }
    for (i = 0ull; i < descriptor->count; ++i) {
        const yvex_runtime_tensor_binding *row = &descriptor->bindings[i];
        if (row->scope == YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL)
            descriptor->summary.global_bindings++;
        else if (row->scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER)
            descriptor->summary.main_layer_bindings++;
        else if (row->scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
            descriptor->summary.mtp_bindings++;
        if (row->collection == YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT)
            descriptor->summary.routed_expert_bindings++;
        if (row->binding && row->binding->expert_count > 1ull)
            descriptor->summary.expert_subview_count += row->binding->expert_count;
    }
    runtime_compute_identity(descriptor);
    *out = descriptor;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_runtime_descriptor_close(yvex_runtime_descriptor *descriptor)
{
    if (!descriptor) return;
    free(descriptor->bindings);
    free(descriptor->name_index);
    free(descriptor);
}

const yvex_runtime_descriptor_summary *yvex_runtime_descriptor_summary_get(
    const yvex_runtime_descriptor *descriptor)
{
    return descriptor ? &descriptor->summary : NULL;
}

unsigned long long yvex_runtime_descriptor_tensor_count(
    const yvex_runtime_descriptor *descriptor)
{
    return descriptor ? descriptor->count : 0ull;
}

const yvex_runtime_tensor_binding *yvex_runtime_descriptor_tensor_at(
    const yvex_runtime_descriptor *descriptor,
    unsigned long long index)
{
    if (!descriptor || index >= descriptor->count) return NULL;
    return &descriptor->bindings[index];
}

const yvex_runtime_tensor_binding *yvex_runtime_descriptor_find_name(
    const yvex_runtime_descriptor *descriptor,
    const char *name)
{
    return runtime_index_find(descriptor, name);
}

const yvex_runtime_tensor_binding *yvex_runtime_descriptor_find_role(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer_index,
    unsigned long long predictor_index)
{
    unsigned long long i;

    if (!descriptor) return NULL;
    for (i = 0ull; i < descriptor->count; ++i) {
        const yvex_runtime_tensor_binding *row = &descriptor->bindings[i];
        if (row->role == role && row->scope == scope &&
            row->layer_index == layer_index &&
            row->predictor_index == predictor_index)
            return row;
    }
    return NULL;
}
