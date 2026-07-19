/* Owner: artifact materialization.
 * Owns: immutable placement plans, sessions, bindings, and bounded reads.
 * Does not own: backend arithmetic, graph execution, generation, or writer publication.
 * Invariants: every binding remains tied to one admitted immutable artifact snapshot.
 * Boundary: materialization exposes bytes but performs no model arithmetic.
 * Purpose: bind admitted tensor ranges into bounded materialization sessions.
 * Inputs: complete admission, placement policy, budgets, and caller outputs.
 * Effects: allocates plan indexes, owns an artifact session, and reads positions.
 * Failure: identity, bounds, budget, allocation, or I/O publishes no binding. */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/gguf.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/core.h>
#include <yvex/internal/families/deepseek_v4.h>

#define MATERIALIZE_DEFAULT_CHUNK (8ull * 1024ull * 1024ull)
#define MATERIALIZE_NAME_INDEX_LOAD_NUM 2ull
#define MATERIALIZE_NAME_INDEX_LOAD_DEN 3ull

static const char *const materialization_status_names[] = {
    [YVEX_MATERIALIZATION_STATUS_REFUSED] = "refused",
    [YVEX_MATERIALIZATION_STATUS_PLANNED] = "planned",
    [YVEX_MATERIALIZATION_STATUS_COMMITTED] = "committed",
    [YVEX_MATERIALIZATION_STATUS_ABORTED] = "aborted",
};

static const char *const materialization_failure_names[] = {
    [YVEX_MATERIALIZATION_FAILURE_NONE] = "none",
    [YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT] = "invalid-argument",
    [YVEX_MATERIALIZATION_FAILURE_ADMISSION] = "admission",
    [YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT] = "snapshot-drift",
    [YVEX_MATERIALIZATION_FAILURE_LAYOUT] = "layout",
    [YVEX_MATERIALIZATION_FAILURE_TENSOR_COUNT] = "tensor-count",
    [YVEX_MATERIALIZATION_FAILURE_TENSOR_RECORD] = "tensor-record",
    [YVEX_MATERIALIZATION_FAILURE_DUPLICATE_TENSOR] = "duplicate-tensor",
    [YVEX_MATERIALIZATION_FAILURE_QTYPE] = "qtype",
    [YVEX_MATERIALIZATION_FAILURE_RANGE] = "range",
    [YVEX_MATERIALIZATION_FAILURE_BUDGET] = "budget",
    [YVEX_MATERIALIZATION_FAILURE_ALLOCATION] = "allocation",
    [YVEX_MATERIALIZATION_FAILURE_READ] = "read",
    [YVEX_MATERIALIZATION_FAILURE_CANCELLED] = "cancelled",
    [YVEX_MATERIALIZATION_FAILURE_LIFECYCLE] = "lifecycle",
    [YVEX_MATERIALIZATION_FAILURE_EXPERT_SUBVIEW] = "expert-subview",
};

typedef struct {
    unsigned long long hash;
    unsigned long long index_plus_one;
} materialize_name_slot;

struct yvex_materialization_plan {
    yvex_complete_artifact_admission admission;
    yvex_artifact_snapshot snapshot;
    yvex_materialized_tensor_binding *bindings;
    materialize_name_slot *name_index;
    unsigned long long name_index_capacity;
    unsigned long long count;
    yvex_materialization_summary summary;
};

struct yvex_materialization_session {
    const yvex_materialization_plan *plan;
    const yvex_artifact *artifact;
    yvex_artifact_snapshot opened_snapshot;
    yvex_materialization_options options;
    yvex_materialization_summary summary;
    int committed;
    int aborted;
};

/* Purpose: project failure set facts while preserving the canonical materialization invariants. */
static void materialize_failure_set(yvex_materialization_failure *failure,
                                    yvex_materialization_failure_code code,
                                    const char *name,
                                    unsigned long long tensor_index,
                                    unsigned long long expected,
                                    unsigned long long actual,
                                    unsigned long long offset,
                                    const char *reason) {
    if (!failure)
        return;
    memset(failure, 0, sizeof(*failure));
    failure->code = code;
    failure->tensor_index = tensor_index;
    failure->expected = expected;
    failure->actual = actual;
    failure->offset = offset;
    failure->reason = reason;
    if (name)
        (void)snprintf(failure->tensor_name, sizeof(failure->tensor_name), "%s", name);
}

/* Purpose: project reject facts while preserving the canonical materialization invariants. */
static int materialize_reject(yvex_materialization_failure *failure,
                              yvex_materialization_failure_code code,
                              const char *name,
                              unsigned long long tensor_index,
                              unsigned long long expected,
                              unsigned long long actual,
                              unsigned long long offset,
                              yvex_error *err,
                              yvex_status status,
                              const char *message) {
    materialize_failure_set(failure, code, name, tensor_index, expected, actual, offset, message);
    yvex_error_set(err, status, "artifact.materialize", message);
    return status;
}

/* Purpose: project index capacity facts while preserving the canonical materialization invariants. */
static int materialize_index_capacity(unsigned long long count, unsigned long long *out) {
    unsigned long long capacity = 16ull;
    if (!out)
        return 0;
    while (capacity * MATERIALIZE_NAME_INDEX_LOAD_NUM < count * MATERIALIZE_NAME_INDEX_LOAD_DEN) {
        if (capacity > ULLONG_MAX / 2ull)
            return 0;
        capacity *= 2ull;
    }
    *out = capacity;
    return 1;
}

/* Purpose: admit one materialization entry while preserving uniqueness and checked capacity.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
static int materialize_index_insert(yvex_materialization_plan *plan,
                                    const char *name,
                                    unsigned long long index) {
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long step = 0ull;

    if (!plan || !plan->name_index || !plan->name_index_capacity || !name)
        return 0;
    hash = yvex_core_index_hash(name);
    slot = hash & (plan->name_index_capacity - 1ull);
    while (step < plan->name_index_capacity) {
        materialize_name_slot *candidate = &plan->name_index[slot];
        if (!candidate->index_plus_one) {
            candidate->hash = hash;
            candidate->index_plus_one = index + 1ull;
            return 1;
        }
        if (candidate->hash == hash &&
            strcmp(plan->bindings[candidate->index_plus_one - 1ull].name, name) == 0)
            return 0;
        slot = (slot + 1ull) & (plan->name_index_capacity - 1ull);
        step++;
    }
    return 0;
}

/* Purpose: locate the materialization entry associated with a canonical key.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
static const yvex_materialized_tensor_binding *
materialize_index_find(const yvex_materialization_plan *plan, const char *name) {
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long step = 0ull;

    if (!plan || !plan->name_index || !plan->name_index_capacity || !name)
        return NULL;
    hash = yvex_core_index_hash(name);
    slot = hash & (plan->name_index_capacity - 1ull);
    while (step < plan->name_index_capacity) {
        const materialize_name_slot *candidate = &plan->name_index[slot];
        if (!candidate->index_plus_one)
            return NULL;
        if (candidate->hash == hash) {
            const yvex_materialized_tensor_binding *binding =
                &plan->bindings[candidate->index_plus_one - 1ull];
            if (strcmp(binding->name, name) == 0)
                return binding;
        }
        slot = (slot + 1ull) & (plan->name_index_capacity - 1ull);
        step++;
    }
    return NULL;
}

/* Purpose: append canonical materialization fields to a deterministic identity stream.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
static void materialize_compute_plan_identity(yvex_materialization_plan *plan) {
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long i;

    yvex_sha256_init(&hash);
    yvex_sha256_update_text(&hash, plan->admission.artifact_identity);
    yvex_sha256_update_text(&hash, plan->admission.profile_identity);
    yvex_sha256_update_text(&hash, plan->admission.writer_plan_identity);
    yvex_sha256_update_u64(&hash, plan->count);
    yvex_sha256_update_u64(&hash, plan->summary.payload_bytes);
    for (i = 0ull; i < plan->count; ++i) {
        const yvex_materialized_tensor_binding *binding = &plan->bindings[i];
        yvex_sha256_update_text(&hash, binding->name);
        yvex_sha256_update_u64(&hash, binding->tensor_id);
        yvex_sha256_update_u64(&hash, binding->descriptor_index);
        yvex_sha256_update_u64(&hash, binding->qtype);
        yvex_sha256_update_u64(&hash, binding->encoded_bytes);
        yvex_sha256_update_u64(&hash, binding->absolute_offset);
        yvex_sha256_update_u64(&hash, (unsigned long long)binding->placement);
    }
    (void)yvex_sha256_final(&hash, digest);
    yvex_sha256_hex(digest, plan->summary.plan_identity);
}

/* Purpose: project select placement facts while preserving the canonical materialization invariants. */
static yvex_materialization_placement
materialize_select_placement(const yvex_tensor_info *tensor,
                             const yvex_materialization_options *options,
                             const yvex_deepseek_gguf_descriptor *descriptor) {
    if (tensor && tensor->ggml_type == YVEX_GGUF_QTYPE_Q2_K)
        return YVEX_MATERIALIZATION_PLACEMENT_STAGED_CACHE;
    if (descriptor && descriptor->expert_count > 1ull)
        return YVEX_MATERIALIZATION_PLACEMENT_STAGED_CACHE;
    if (options && options->backend_resident_budget_bytes && tensor &&
        tensor->storage_bytes <= options->backend_resident_budget_bytes)
        return YVEX_MATERIALIZATION_PLACEMENT_BACKEND_RESIDENT_CANDIDATE;
    return YVEX_MATERIALIZATION_PLACEMENT_FILE_BACKED;
}

/* Purpose: project access for placement facts while preserving the canonical materialization invariants. */
static yvex_materialization_access_mode
materialize_access_for_placement(yvex_materialization_placement placement) {
    switch (placement) {
    case YVEX_MATERIALIZATION_PLACEMENT_STAGED_CACHE:
        return YVEX_MATERIALIZATION_ACCESS_STAGED_SUBVIEW;
    case YVEX_MATERIALIZATION_PLACEMENT_BACKEND_RESIDENT_CANDIDATE:
        return YVEX_MATERIALIZATION_ACCESS_BACKEND_CANDIDATE_FILE_RANGE;
    case YVEX_MATERIALIZATION_PLACEMENT_FILE_BACKED:
    default:
        return YVEX_MATERIALIZATION_ACCESS_FILE_RANGE;
    }
}

/* Purpose: project summary add binding facts while preserving the canonical materialization invariants.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
static void materialize_summary_add_binding(yvex_materialization_summary *summary,
                                            const yvex_materialized_tensor_binding *binding) {
    if (!summary || !binding)
        return;
    summary->tensor_count++;
    summary->payload_bytes += binding->encoded_bytes;
    if (binding->qtype < YVEX_MATERIALIZATION_QTYPE_CAP) {
        summary->qtype_tensor_counts[binding->qtype]++;
        summary->qtype_bytes[binding->qtype] += binding->encoded_bytes;
    }
    switch (binding->placement) {
    case YVEX_MATERIALIZATION_PLACEMENT_STAGED_CACHE:
        summary->staged_cache_tensors++;
        summary->staged_cache_bytes += binding->encoded_bytes;
        break;
    case YVEX_MATERIALIZATION_PLACEMENT_BACKEND_RESIDENT_CANDIDATE:
        summary->backend_candidate_tensors++;
        summary->backend_candidate_bytes += binding->encoded_bytes;
        break;
    case YVEX_MATERIALIZATION_PLACEMENT_FILE_BACKED:
    default:
        summary->file_backed_tensors++;
        summary->file_backed_bytes += binding->encoded_bytes;
        break;
    }
    summary->file_backed_bytes_owned += binding->encoded_bytes;
    if (binding->expert_count > 1ull)
        summary->expert_subview_count += binding->expert_count;
}

/* Purpose: initialize materialization state to its canonical empty or default value.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
void yvex_materialization_options_default(yvex_materialization_options *options) {
    if (!options)
        return;
    memset(options, 0, sizeof(*options));
    options->max_chunk_bytes = MATERIALIZE_DEFAULT_CHUNK;
    options->cache_budget_bytes = MATERIALIZE_DEFAULT_CHUNK * 2ull;
    options->future_graph_scratch_reserve_bytes = 512ull * 1024ull * 1024ull;
    options->future_kv_reserve_bytes = 512ull * 1024ull * 1024ull;
    options->require_complete_admission = 1;
}

/* Purpose: map materialization lifecycle status to a stable diagnostic name.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
const char *yvex_materialization_status_name(yvex_materialization_status status) {
    return (unsigned int)status <
                   sizeof(materialization_status_names) / sizeof(materialization_status_names[0])
               ? materialization_status_names[status]
               : "refused";
}

/* Purpose: map materialization refusal codes to stable diagnostic names.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
const char *yvex_materialization_failure_name(yvex_materialization_failure_code code) {
    return (unsigned int)code <
                   sizeof(materialization_failure_names) / sizeof(materialization_failure_names[0])
               ? materialization_failure_names[code]
               : "unknown";
}

/* Purpose: project one admitted GGUF tensor into an immutable materialization binding.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
static int materialize_plan_add_tensor(yvex_materialization_plan *plan,
                                       const yvex_artifact *artifact,
                                       const yvex_gguf *gguf,
                                       const yvex_tensor_table *tensors,
                                       const yvex_deepseek_gguf_map *deepseek_map,
                                       const yvex_materialization_options *options,
                                       unsigned long long index,
                                       yvex_materialization_failure *failure,
                                       yvex_error *err) {
    const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
    const yvex_deepseek_gguf_descriptor *descriptor = NULL;
    const yvex_gguf_qtype_geometry *geometry;
    yvex_gguf_qtype_storage_result storage;
    yvex_materialized_tensor_binding *binding = &plan->bindings[index];
    unsigned int dimension;

    if (!tensor || !tensor->name || !tensor->name[0])
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_TENSOR_RECORD,
                                  tensor && tensor->name ? tensor->name : NULL,
                                  index,
                                  1ull,
                                  0ull,
                                  tensor ? tensor->absolute_offset : 0ull,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  "materialization tensor record is missing canonical facts");
    descriptor =
        deepseek_map
            ? yvex_model_register_deepseek_v4()->lowering.find_emitted(deepseek_map, tensor->name)
            : NULL;
    if (options->require_deepseek_map && !descriptor)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_TENSOR_RECORD,
                                  tensor->name,
                                  index,
                                  1ull,
                                  0ull,
                                  tensor->absolute_offset,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  "materialization tensor record is missing canonical facts");
    if (yvex_gguf_qtype_validate_tensor_storage(
            tensor->ggml_type, tensor->dims, tensor->rank, tensor->storage_bytes, &storage) !=
        YVEX_GGUF_QTYPE_STORAGE_OK)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_QTYPE,
                                  tensor->name,
                                  index,
                                  tensor->storage_bytes,
                                  storage.total_bytes,
                                  tensor->absolute_offset,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  storage.reason ? storage.reason : "qtype geometry refused");
    if (tensor->absolute_offset > ULLONG_MAX - tensor->storage_bytes ||
        tensor->absolute_offset + tensor->storage_bytes > yvex_artifact_size(artifact))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_RANGE,
                                  tensor->name,
                                  index,
                                  yvex_artifact_size(artifact),
                                  tensor->absolute_offset + tensor->storage_bytes,
                                  tensor->absolute_offset,
                                  err,
                                  YVEX_ERR_BOUNDS,
                                  "tensor range exceeds admitted artifact file");
    geometry = yvex_gguf_qtype_geometry_find(tensor->ggml_type);
    binding->tensor_id = index;
    binding->descriptor_index =
        descriptor
            ? (unsigned long long)(descriptor - yvex_model_register_deepseek_v4()->lowering.at(
                                                    deepseek_map, 0ull))
            : YVEX_MATERIALIZATION_NO_INDEX;
    (void)snprintf(binding->name, sizeof(binding->name), "%s", tensor->name);
    binding->role = descriptor ? descriptor->role : tensor->role;
    binding->collection = descriptor ? descriptor->collection : YVEX_TENSOR_COLLECTION_COUNT;
    binding->scope = descriptor ? descriptor->scope : YVEX_TENSOR_SCOPE_GLOBAL;
    binding->layer_index = descriptor ? descriptor->layer_index : YVEX_MATERIALIZATION_NO_INDEX;
    binding->predictor_index =
        descriptor ? descriptor->predictor_index : YVEX_MATERIALIZATION_NO_INDEX;
    binding->expert_count = descriptor ? descriptor->expert_count : 0ull;
    binding->rank = tensor->rank;
    for (dimension = 0u; dimension < YVEX_TENSOR_MAX_DIMS; ++dimension)
        binding->dims[dimension] = dimension < tensor->rank ? tensor->dims[dimension] : 0ull;
    binding->qtype = tensor->ggml_type;
    binding->storage_class = geometry ? geometry->storage_class : YVEX_GGUF_QTYPE_STORAGE_UNKNOWN;
    binding->row_width = storage.row_width;
    binding->row_count = storage.row_count;
    binding->block_size = geometry ? geometry->block_size : 0ull;
    binding->bytes_per_block = geometry ? geometry->bytes_per_block : 0ull;
    binding->encoded_bytes = tensor->storage_bytes;
    binding->absolute_offset = tensor->absolute_offset;
    if (!yvex_core_u64_add(
            tensor->absolute_offset, tensor->storage_bytes, &binding->absolute_end_offset))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_RANGE,
                                  tensor->name,
                                  index,
                                  ULLONG_MAX,
                                  tensor->storage_bytes,
                                  tensor->absolute_offset,
                                  err,
                                  YVEX_ERR_BOUNDS,
                                  "tensor range end overflowed");
    binding->alignment = yvex_gguf_alignment(gguf);
    binding->placement = materialize_select_placement(tensor, options, descriptor);
    binding->access_mode = materialize_access_for_placement(binding->placement);
    binding->backend_compatible =
        binding->qtype == YVEX_GGUF_QTYPE_F32 || binding->qtype == YVEX_GGUF_QTYPE_F16 ||
        binding->qtype == YVEX_GGUF_QTYPE_BF16 || binding->qtype == YVEX_GGUF_QTYPE_I32 ||
        binding->qtype == YVEX_GGUF_QTYPE_Q8_0 || binding->qtype == YVEX_GGUF_QTYPE_Q2_K;
    if (!materialize_index_insert(plan, binding->name, index))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_DUPLICATE_TENSOR,
                                  tensor->name,
                                  index,
                                  1ull,
                                  2ull,
                                  tensor->absolute_offset,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  "duplicate tensor name in materialization plan");
    materialize_summary_add_binding(&plan->summary, binding);
    return YVEX_OK;
}

/* Purpose: derive an immutable placement and byte-range plan from complete artifact admission.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
int yvex_materialization_plan_build(yvex_materialization_plan **out,
                                    const yvex_complete_artifact_admission *admission,
                                    const yvex_artifact *artifact,
                                    const yvex_gguf *gguf,
                                    const yvex_tensor_table *tensors,
                                    const yvex_deepseek_gguf_map *deepseek_map,
                                    const yvex_materialization_options *options,
                                    yvex_materialization_failure *failure,
                                    yvex_error *err) {
    yvex_materialization_options local;
    yvex_materialization_plan *plan = NULL;
    yvex_artifact_snapshot snapshot;
    yvex_gguf_layout_result layout;
    unsigned long long count;
    unsigned long long index_capacity;
    unsigned long long i;
    int rc;

    if (out)
        *out = NULL;
    if (!out || !admission || !artifact || !gguf || !tensors)
        return materialize_reject(
            failure,
            YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT,
            NULL,
            YVEX_MATERIALIZATION_NO_INDEX,
            1ull,
            0ull,
            0ull,
            err,
            YVEX_ERR_INVALID_ARG,
            "materialization plan requires admission, artifact, GGUF, and tensor table");
    yvex_materialization_options_default(&local);
    if (options)
        local = *options;
    if (!local.max_chunk_bytes || local.max_chunk_bytes > (unsigned long long)SIZE_MAX)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_BUDGET,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  local.max_chunk_bytes,
                                  0ull,
                                  err,
                                  YVEX_ERR_INVALID_ARG,
                                  "materialization chunk budget is invalid");
    if (local.require_complete_admission &&
        (!admission->complete || admission->artifact_class != YVEX_ARTIFACT_CLASS_COMPLETE_YVEX ||
         !admission->materialization_input_ready || admission->runtime_supported))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_ADMISSION,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_STATE,
                                  "complete YVEX artifact admission is required");
    if (yvex_artifact_snapshot_get(artifact, &snapshot, err) != YVEX_OK ||
        !yvex_artifact_snapshot_equal(&snapshot, &admission->file_snapshot))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  admission->file_snapshot.size,
                                  snapshot.size,
                                  0ull,
                                  err,
                                  YVEX_ERR_STATE,
                                  "opened artifact snapshot does not match admission");
    memset(&layout, 0, sizeof(layout));
    rc = yvex_gguf_layout_validate(artifact, gguf, &layout, err);
    if (rc != YVEX_OK || !layout.accepted)
        return materialize_reject(
            failure,
            YVEX_MATERIALIZATION_FAILURE_LAYOUT,
            layout.tensor_name,
            layout.tensor_index,
            YVEX_GGUF_LAYOUT_OK,
            layout.code,
            layout.failure_absolute_offset,
            err,
            YVEX_ERR_FORMAT,
            "GGUF global layout admission is required before materialization");
    count = yvex_tensor_table_count(tensors);
    if (count != admission->tensor_count)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_TENSOR_COUNT,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  admission->tensor_count,
                                  count,
                                  0ull,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  "tensor table count differs from complete-artifact admission");
    if (local.require_deepseek_map) {
        const yvex_deepseek_gguf_map_summary *summary =
            yvex_model_register_deepseek_v4()->lowering.summary(deepseek_map);
        if (!summary || !summary->complete || summary->descriptor_count != count)
            return materialize_reject(failure,
                                      YVEX_MATERIALIZATION_FAILURE_TENSOR_COUNT,
                                      NULL,
                                      YVEX_MATERIALIZATION_NO_INDEX,
                                      count,
                                      summary ? summary->descriptor_count : 0ull,
                                      0ull,
                                      err,
                                      YVEX_ERR_FORMAT,
                                      "DeepSeek materialization requires the canonical GGUF map");
    }
    if (!materialize_index_capacity(count, &index_capacity))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_ALLOCATION,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  count,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_NOMEM,
                                  "materialization name index capacity overflow");
    plan = (yvex_materialization_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_ALLOCATION,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_NOMEM,
                                  "materialization plan allocation failed");
    plan->bindings = (yvex_materialized_tensor_binding *)calloc((size_t)(count ? count : 1ull),
                                                                sizeof(*plan->bindings));
    plan->name_index =
        (materialize_name_slot *)calloc((size_t)index_capacity, sizeof(*plan->name_index));
    if (!plan->bindings || !plan->name_index) {
        yvex_materialization_plan_close(plan);
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_ALLOCATION,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  count,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_NOMEM,
                                  "materialization binding allocation failed");
    }
    plan->admission = *admission;
    plan->snapshot = snapshot;
    plan->name_index_capacity = index_capacity;
    plan->count = count;
    plan->summary.status = YVEX_MATERIALIZATION_STATUS_PLANNED;
    plan->summary.file_bytes = admission->file_bytes;
    plan->summary.graph_scratch_reserved_bytes = local.future_graph_scratch_reserve_bytes;
    plan->summary.kv_reserved_bytes = local.future_kv_reserve_bytes;
    (void)snprintf(plan->summary.artifact_identity,
                   sizeof(plan->summary.artifact_identity),
                   "%s",
                   admission->artifact_identity);

    for (i = 0ull; i < count; ++i) {
        rc = materialize_plan_add_tensor(
            plan, artifact, gguf, tensors, deepseek_map, &local, i, failure, err);
        if (rc != YVEX_OK) {
            yvex_materialization_plan_close(plan);
            return rc;
        }
    }
    materialize_compute_plan_identity(plan);
    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: release an immutable materialization plan and its lookup index.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by artifact materialization; cleanup remains deterministic.
 * Failure: null or released artifact materialization handles remain harmless.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
void yvex_materialization_plan_close(yvex_materialization_plan *plan) {
    if (!plan)
        return;
    free(plan->bindings);
    free(plan->name_index);
    free(plan);
}

/* Purpose: project materialization plan summary facts while preserving the canonical materialization invariants.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
const yvex_materialization_summary *
yvex_materialization_plan_summary(const yvex_materialization_plan *plan) {
    return plan ? &plan->summary : NULL;
}

/* Purpose: return the immutable materialization entry at a checked ordinal. */
static const yvex_materialized_tensor_binding *plan_tensor_at(const yvex_materialization_plan *plan,
                                                              unsigned long long index) {
    if (!plan || index >= plan->count)
        return NULL;
    return &plan->bindings[index];
}

/* Purpose: resolve one materialization binding by canonical tensor name.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
const yvex_materialized_tensor_binding *
yvex_materialization_plan_find_name(const yvex_materialization_plan *plan, const char *name) {
    return materialize_index_find(plan, name);
}

/* Purpose: bind an immutable plan to the exact admitted artifact snapshot.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
int yvex_materialization_session_open(yvex_materialization_session **out,
                                      const yvex_materialization_plan *plan,
                                      const yvex_artifact *artifact,
                                      const yvex_materialization_options *options,
                                      yvex_materialization_failure *failure,
                                      yvex_error *err) {
    yvex_materialization_options local;
    yvex_materialization_session *session;
    yvex_artifact_snapshot snapshot;

    if (out)
        *out = NULL;
    if (!out || !plan || !artifact)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_INVALID_ARG,
                                  "materialization session requires plan and artifact");
    yvex_materialization_options_default(&local);
    if (options)
        local = *options;
    if (!local.max_chunk_bytes || local.max_chunk_bytes > (unsigned long long)SIZE_MAX)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_BUDGET,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  local.max_chunk_bytes,
                                  0ull,
                                  err,
                                  YVEX_ERR_INVALID_ARG,
                                  "materialization session chunk budget is invalid");
    if (yvex_artifact_snapshot_get(artifact, &snapshot, err) != YVEX_OK ||
        !yvex_artifact_snapshot_equal(&snapshot, &plan->snapshot))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  plan->snapshot.size,
                                  snapshot.size,
                                  0ull,
                                  err,
                                  YVEX_ERR_STATE,
                                  "materialization session artifact snapshot drifted");
    session = (yvex_materialization_session *)calloc(1u, sizeof(*session));
    if (!session)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_ALLOCATION,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_NOMEM,
                                  "materialization session allocation failed");
    session->plan = plan;
    session->artifact = artifact;
    session->opened_snapshot = snapshot;
    session->options = local;
    session->summary = plan->summary;
    session->summary.status = YVEX_MATERIALIZATION_STATUS_PLANNED;
    *out = session;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: seal a fully validated materialization session for concurrent reads.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
int yvex_materialization_session_commit(yvex_materialization_session *session,
                                        yvex_materialization_failure *failure,
                                        yvex_error *err) {
    yvex_artifact_snapshot current;

    if (!session || session->aborted)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_LIFECYCLE,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_STATE,
                                  "cannot commit missing or aborted materialization session");
    if (yvex_artifact_snapshot_validate(session->artifact, &current, err) != YVEX_OK ||
        !yvex_artifact_snapshot_equal(&current, &session->opened_snapshot)) {
        session->summary.snapshot_drift_count++;
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  session->opened_snapshot.size,
                                  current.size,
                                  0ull,
                                  err,
                                  YVEX_ERR_STATE,
                                  "artifact snapshot drifted before materialization commit");
    }
    session->committed = 1;
    session->summary.status = YVEX_MATERIALIZATION_STATUS_COMMITTED;
    session->summary.committed = 1;
    session->summary.committed_bindings = session->plan->count;
    session->summary.cleanup_complete = 0;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: close one materialization session and its admitted artifact handle.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by artifact materialization; cleanup remains deterministic.
 * Failure: null or released artifact materialization handles remain harmless.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
void yvex_materialization_session_close(yvex_materialization_session *session) {
    if (!session)
        return;
    session->summary.cleanup_complete = 1;
    free(session);
}

/* Purpose: project materialization session summary facts while preserving the canonical materialization invariants.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
const yvex_materialization_summary *
yvex_materialization_session_summary(const yvex_materialization_session *session) {
    return session ? &session->summary : NULL;
}

/* Purpose: return the immutable materialization entry at a checked ordinal.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
const yvex_materialized_tensor_binding *
yvex_materialization_session_tensor_at(const yvex_materialization_session *session,
                                       unsigned long long index) {
    return session && session->plan ? plan_tensor_at(session->plan, index) : NULL;
}

/* Purpose: perform one checked positioned read from a committed tensor binding.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned artifact materialization state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
int yvex_materialization_session_read(yvex_materialization_session *session,
                                      const yvex_materialized_tensor_binding *binding,
                                      unsigned long long binding_offset,
                                      void *dst,
                                      size_t len,
                                      yvex_materialization_failure *failure,
                                      yvex_error *err) {
    yvex_artifact_snapshot current;
    unsigned long long absolute;

    if (!session || !binding || !dst || !len)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT,
                                  binding ? binding->name : NULL,
                                  binding ? binding->tensor_id : YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_INVALID_ARG,
                                  "materialization read requires session, binding, and buffer");
    if (!session->committed || session->aborted)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_LIFECYCLE,
                                  binding->name,
                                  binding->tensor_id,
                                  1ull,
                                  0ull,
                                  binding->absolute_offset,
                                  err,
                                  YVEX_ERR_STATE,
                                  "materialization read requires a committed session");
    if ((unsigned long long)len > binding->encoded_bytes ||
        binding_offset > binding->encoded_bytes - (unsigned long long)len)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_RANGE,
                                  binding->name,
                                  binding->tensor_id,
                                  binding->encoded_bytes,
                                  binding_offset + (unsigned long long)len,
                                  binding->absolute_offset,
                                  err,
                                  YVEX_ERR_BOUNDS,
                                  "materialization read exceeds binding range");
    if (yvex_artifact_snapshot_validate(session->artifact, &current, err) != YVEX_OK ||
        !yvex_artifact_snapshot_equal(&current, &session->opened_snapshot)) {
        session->summary.snapshot_drift_count++;
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT,
                                  binding->name,
                                  binding->tensor_id,
                                  session->opened_snapshot.size,
                                  current.size,
                                  binding->absolute_offset,
                                  err,
                                  YVEX_ERR_STATE,
                                  "artifact snapshot drifted during materialization read");
    }
    absolute = binding->absolute_offset + binding_offset;
    if (yvex_artifact_read_at(session->artifact, absolute, dst, len, err) != YVEX_OK)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_READ,
                                  binding->name,
                                  binding->tensor_id,
                                  len,
                                  0ull,
                                  absolute,
                                  err,
                                  YVEX_ERR_IO,
                                  "materialization positioned read failed");
    session->summary.access_calls++;
    session->summary.payload_bytes_accessed += (unsigned long long)len;
    if ((unsigned long long)len > session->summary.staging_bytes)
        session->summary.staging_bytes = (unsigned long long)len;
    if (session->summary.staging_bytes > session->summary.peak_executor_owned_bytes)
        session->summary.peak_executor_owned_bytes = session->summary.staging_bytes;
    return YVEX_OK;
}

/* Purpose: stream all admitted tensor payload ranges through a bounded visitor.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
int yvex_materialization_session_walk_payload(yvex_materialization_session *session,
                                              yvex_materialization_progress_fn progress,
                                              void *progress_context,
                                              yvex_materialization_failure *failure,
                                              yvex_error *err) {
    unsigned char *buffer;
    unsigned long long i;
    size_t chunk;

    if (!session || !session->plan)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_INVALID_ARG,
                                  "payload walk requires a materialization session");
    if (!session->committed)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_LIFECYCLE,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_STATE,
                                  "payload walk requires a committed materialization session");
    chunk = (size_t)session->options.max_chunk_bytes;
    buffer = (unsigned char *)malloc(chunk);
    if (!buffer)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_ALLOCATION,
                                  NULL,
                                  YVEX_MATERIALIZATION_NO_INDEX,
                                  chunk,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_NOMEM,
                                  "payload walk buffer allocation failed");
    for (i = 0ull; i < session->plan->count; ++i) {
        const yvex_materialized_tensor_binding *binding = &session->plan->bindings[i];
        unsigned long long delivered = 0ull;
        while (delivered < binding->encoded_bytes) {
            unsigned long long remaining = binding->encoded_bytes - delivered;
            size_t request = remaining < (unsigned long long)chunk ? (size_t)remaining : chunk;
            int rc;
            if (session->options.cancel_after_first_chunk && session->summary.access_calls > 0ull) {
                free(buffer);
                return materialize_reject(failure,
                                          YVEX_MATERIALIZATION_FAILURE_CANCELLED,
                                          binding->name,
                                          binding->tensor_id,
                                          binding->encoded_bytes,
                                          delivered,
                                          binding->absolute_offset + delivered,
                                          err,
                                          YVEX_ERR_CANCELLED,
                                          "materialization walk cancelled by options");
            }
            rc = yvex_materialization_session_read(
                session, binding, delivered, buffer, request, failure, err);
            if (rc != YVEX_OK) {
                free(buffer);
                return rc;
            }
            delivered += request;
        }
        if (progress)
            progress(progress_context, &session->summary, binding);
    }
    free(buffer);
    session->summary.full_walks++;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: project one routed-expert slice without copying its underlying bytes.
 * Inputs: typed artifact materialization arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact materialization state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: materialization exposes bytes but performs no model arithmetic. */
int yvex_materialization_session_expert_subview(const yvex_materialization_session *session,
                                                const yvex_materialized_tensor_binding *binding,
                                                unsigned long long expert_index,
                                                yvex_materialized_expert_subview *out,
                                                yvex_materialization_failure *failure,
                                                yvex_error *err) {
    unsigned long long bytes_per_expert;

    if (!session || !binding || !out)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT,
                                  binding ? binding->name : NULL,
                                  binding ? binding->tensor_id : YVEX_MATERIALIZATION_NO_INDEX,
                                  1ull,
                                  0ull,
                                  0ull,
                                  err,
                                  YVEX_ERR_INVALID_ARG,
                                  "expert subview requires session, binding, and output");
    memset(out, 0, sizeof(*out));
    if (binding->expert_count <= 1ull || expert_index >= binding->expert_count)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_EXPERT_SUBVIEW,
                                  binding->name,
                                  binding->tensor_id,
                                  binding->expert_count,
                                  expert_index,
                                  binding->absolute_offset,
                                  err,
                                  YVEX_ERR_BOUNDS,
                                  "expert subview index is outside aggregate tensor geometry");
    if (binding->encoded_bytes % binding->expert_count != 0ull)
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_EXPERT_SUBVIEW,
                                  binding->name,
                                  binding->tensor_id,
                                  binding->expert_count,
                                  binding->encoded_bytes,
                                  binding->absolute_offset,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  "aggregate expert tensor bytes are not evenly divisible");
    bytes_per_expert = binding->encoded_bytes / binding->expert_count;
    if (bytes_per_expert == 0ull ||
        (binding->bytes_per_block && bytes_per_expert % binding->bytes_per_block != 0ull))
        return materialize_reject(failure,
                                  YVEX_MATERIALIZATION_FAILURE_EXPERT_SUBVIEW,
                                  binding->name,
                                  binding->tensor_id,
                                  binding->bytes_per_block,
                                  bytes_per_expert,
                                  binding->absolute_offset,
                                  err,
                                  YVEX_ERR_FORMAT,
                                  "expert subview does not align to qtype block bytes");
    out->expert_index = expert_index;
    out->expert_count = binding->expert_count;
    out->encoded_bytes = bytes_per_expert;
    out->absolute_offset = binding->absolute_offset + expert_index * bytes_per_expert;
    out->block_size = binding->block_size;
    out->block_aligned = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}
