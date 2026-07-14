/*
 * yvex_deepseek_payload_handoff.c - DeepSeek mapping-to-payload adapter owner.
 *
 * Owner: src/model/target.
 * Owns: one-pass source/IR/coverage/map composition and exhaustive range handoff.
 * Does not own: common source reads, payload trust algorithms, transforms, or quantization.
 * Invariants: 69,187 unique contributions cover 1,360 descriptors at the pinned identity.
 * Boundary: resolved payload ranges are input facts for V010.QUANT.2, not conversion.
 */
#include "yvex_deepseek_payload_handoff.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_deepseek_payload_handoff {
    char *source_path;
    char *models_root;
    char *manifest_path;
    yvex_source_verify_options source_options;
    yvex_source_verification verification;
    yvex_deepseek_tensor_coverage *coverage;
    yvex_transform_ir *transform_ir;
    yvex_deepseek_gguf_map *map;
    yvex_source_payload_session *session;
    yvex_transform_binding *binding;
    yvex_source_payload_plan *plan;
    yvex_deepseek_payload_handoff_summary summary;
};

static char *handoff_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (!text) return NULL;
    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy) memcpy(copy, text, length + 1u);
    return copy;
}

static int handoff_reject(yvex_deepseek_payload_failure *failure,
                          yvex_deepseek_payload_failure_code code,
                          unsigned long long descriptor,
                          unsigned long long contribution,
                          int status,
                          yvex_error *err,
                          const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->descriptor_index = descriptor;
        failure->contribution_index = contribution;
    }
    yvex_error_set(err, (yvex_status)status, "deepseek_payload_handoff", message);
    return status;
}

/* Resolves every canonical contribution and builds one physical-order source plan. */
static int handoff_resolve(yvex_deepseek_payload_handoff *handoff,
                           const yvex_deepseek_payload_handoff_options *options,
                           yvex_deepseek_payload_failure *failure,
                           yvex_error *err)
{
    const yvex_deepseek_gguf_map_summary *map_summary =
        yvex_deepseek_gguf_map_summary_get(handoff->map);
    unsigned long long *tensor_indices;
    unsigned long long contribution_index;
    unsigned long long descriptor_index;
    int rc;

    if (!map_summary || !map_summary->complete ||
        map_summary->mapping_identity !=
            YVEX_DEEPSEEK_PAYLOAD_MAPPING_IDENTITY ||
        map_summary->source_identity !=
            handoff->verification.source_snapshot_identity) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING_IDENTITY,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_FORMAT, err,
            "canonical DeepSeek mapping identity mismatch");
    }
    if (map_summary->source_contribution_count >
        (unsigned long long)(SIZE_MAX / sizeof(tensor_indices[0]))) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_BOUNDS, err,
            "mapping contribution index allocation overflow");
    }
    tensor_indices = (unsigned long long *)calloc(
        (size_t)map_summary->source_contribution_count,
        sizeof(tensor_indices[0]));
    if (!tensor_indices) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_NOMEM, err,
            "mapping contribution index allocation failed");
    }
    handoff->summary.mapping_identity = map_summary->mapping_identity;
    (void)snprintf(handoff->summary.transform_identity,
                   sizeof(handoff->summary.transform_identity), "%s",
                   yvex_transform_ir_summary_get(
                       handoff->transform_ir)->transform_identity);
    handoff->summary.source_snapshot_identity = map_summary->source_identity;
    handoff->summary.descriptor_count = map_summary->descriptor_count;
    handoff->summary.contribution_count = map_summary->source_contribution_count;
    for (contribution_index = 0u;
         contribution_index < map_summary->source_contribution_count;
         ++contribution_index) {
        const yvex_deepseek_gguf_contribution *contribution =
            yvex_deepseek_gguf_map_contribution_at(
                handoff->map, contribution_index);
        const yvex_source_payload_range *range;
        const yvex_deepseek_tensor_coverage_row *coverage_row;
        const yvex_deepseek_gguf_descriptor *descriptor;

        if (!contribution ||
            contribution->descriptor_index >= map_summary->descriptor_count) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
                ULLONG_MAX, contribution_index, YVEX_ERR_FORMAT, err,
                "mapping contribution is incomplete");
        }
        descriptor = yvex_deepseek_gguf_map_at(
            handoff->map, contribution->descriptor_index);
        coverage_row = yvex_deepseek_tensor_coverage_at(
            handoff->coverage, contribution->source_row_index);
        range = yvex_source_payload_range_find(
            handoff->session, contribution->source_name);
        handoff->summary.range_lookup_count++;
        if (!descriptor || !coverage_row || !coverage_row->source || !range ||
            strcmp(coverage_row->source->name, contribution->source_name) != 0 ||
            range->source_snapshot_identity != map_summary->source_identity ||
            range->dtype != contribution->source_dtype ||
            range->rank != contribution->source_rank) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
                contribution->descriptor_index, contribution_index,
                YVEX_ERR_FORMAT, err,
                "mapping contribution does not resolve to its exact source range");
        }
        tensor_indices[contribution_index] = range->source_tensor_index;
        handoff->summary.contributions_resolved++;
        if (descriptor->transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT)
            handoff->summary.direct_contributions++;
        if (contribution->kind == YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY &&
            contribution->source_dtype == YVEX_NATIVE_DTYPE_F8_E4M3)
            handoff->summary.fp8_weight_contributions++;
        if (contribution->kind == YVEX_DEEPSEEK_GGUF_CONTRIBUTION_SCALE &&
            contribution->source_dtype == YVEX_NATIVE_DTYPE_F8_E8M0)
            handoff->summary.e8m0_scale_contributions++;
        if (contribution->kind ==
                YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_WEIGHT ||
            contribution->kind ==
                YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_SCALE) {
            if (ULLONG_MAX - handoff->summary.routed_expert_logical_bytes <
                range->byte_length) {
                free(tensor_indices);
                return handoff_reject(
                    failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
                    contribution->descriptor_index, contribution_index,
                    YVEX_ERR_BOUNDS, err,
                    "routed expert payload accounting overflow");
            }
            handoff->summary.expert_contributions++;
            handoff->summary.routed_expert_logical_bytes += range->byte_length;
        }
        if (descriptor->transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32 &&
            contribution->source_dtype == YVEX_NATIVE_DTYPE_I64)
            handoff->summary.i64_router_contributions++;
        if (descriptor->collection == YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL)
            handoff->summary.global_contributions++;
        if (descriptor->collection == YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM)
            handoff->summary.norm_contributions++;
        if (descriptor->collection ==
            YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT)
            handoff->summary.shared_expert_contributions++;
        if (descriptor->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD) {
            if (ULLONG_MAX - handoff->summary.output_head_logical_bytes <
                range->byte_length) {
                free(tensor_indices);
                return handoff_reject(
                    failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
                    contribution->descriptor_index, contribution_index,
                    YVEX_ERR_BOUNDS, err,
                    "output head payload accounting overflow");
            }
            handoff->summary.output_head_contributions++;
            handoff->summary.output_head_logical_bytes += range->byte_length;
        }
        if (descriptor->scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
            handoff->summary.mtp_contributions++;
    }
    for (descriptor_index = 0u;
         descriptor_index < map_summary->descriptor_count; ++descriptor_index) {
        const yvex_deepseek_gguf_descriptor *descriptor =
            yvex_deepseek_gguf_map_at(handoff->map, descriptor_index);
        unsigned long long end;

        if (!descriptor || descriptor->contribution_count == 0u ||
            ULLONG_MAX - descriptor->contribution_offset <
                descriptor->contribution_count) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
                descriptor_index, ULLONG_MAX, YVEX_ERR_FORMAT, err,
                "logical descriptor has no bounded source contribution set");
        }
        end = descriptor->contribution_offset + descriptor->contribution_count;
        if (end > handoff->summary.contributions_resolved) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
                descriptor_index, end, YVEX_ERR_FORMAT, err,
                "logical descriptor contribution span exceeds resolved mapping");
        }
        handoff->summary.descriptors_covered++;
    }
    rc = yvex_source_payload_plan_build(
        &handoff->plan, handoff->session, tensor_indices,
        map_summary->source_contribution_count, options->chunk_bytes,
        options->page_bytes, failure ? &failure->payload_failure : NULL, err);
    free(tensor_indices);
    if (rc != YVEX_OK) {
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_PLAN;
        return rc;
    }
    handoff->summary.complete =
        handoff->summary.descriptors_covered ==
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT &&
        handoff->summary.contributions_resolved ==
            YVEX_DEEPSEEK_GGUF_SOURCE_COUNT &&
        handoff->summary.fp8_weight_contributions != 0u &&
        handoff->summary.e8m0_scale_contributions != 0u &&
        handoff->summary.expert_contributions != 0u &&
        handoff->summary.i64_router_contributions != 0u &&
        handoff->summary.global_contributions != 0u &&
        handoff->summary.norm_contributions != 0u &&
        handoff->summary.shared_expert_contributions != 0u &&
        handoff->summary.output_head_contributions != 0u &&
        handoff->summary.mtp_contributions != 0u;
    if (!handoff->summary.complete)
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_FORMAT, err,
            "mapping payload handoff lacks one required contribution class");
    return YVEX_OK;
}

/* Runs one canonical source pass and retains only typed downstream owners. */
int yvex_deepseek_payload_handoff_open(
    yvex_deepseek_payload_handoff **out,
    const yvex_deepseek_payload_handoff_options *options,
    yvex_deepseek_payload_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_payload_handoff *handoff;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure ir_failure;
    yvex_deepseek_tensor_coverage_failure coverage_failure;
    yvex_deepseek_gguf_map_failure map_failure;
    yvex_transform_failure transform_failure;
    yvex_source_payload_open_options payload_options;
    int rc;

    if (out) *out = NULL;
    if (!out || !options || !options->source_path ||
        !options->source_path[0] || !options->models_root ||
        !options->models_root[0]) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_INVALID_ARGUMENT,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_INVALID_ARG, err,
            "source path, models root, and output are required");
    }
    handoff = (yvex_deepseek_payload_handoff *)calloc(1u, sizeof(*handoff));
    if (!handoff)
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_NOMEM, err,
            "payload handoff allocation failed");
    handoff->source_path = handoff_strdup(options->source_path);
    handoff->models_root = handoff_strdup(options->models_root);
    handoff->manifest_path = options->manifest_path
        ? handoff_strdup(options->manifest_path) : NULL;
    if (!handoff->source_path || !handoff->models_root ||
        (options->manifest_path && !handoff->manifest_path)) {
        yvex_deepseek_payload_handoff_close(handoff);
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_NOMEM, err,
            "payload handoff path allocation failed");
    }
    handoff->source_options.identity = yvex_model_target_release_identity();
    handoff->source_options.source_path = handoff->source_path;
    handoff->source_options.models_root = handoff->models_root;
    handoff->source_options.manifest_path = handoff->manifest_path;
    handoff->source_options.promote_manifest = 1;
    rc = yvex_source_verify_with_snapshot(
        &handoff->source_options, &handoff->verification, &snapshot, err);
    if (rc != YVEX_OK || !handoff->verification.verified || !snapshot) {
        yvex_source_tensor_snapshot_release(snapshot);
        yvex_deepseek_payload_handoff_close(handoff);
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_SOURCE,
            ULLONG_MAX, ULLONG_MAX, rc == YVEX_OK ? YVEX_ERR_STATE : rc, err,
            "exact source verification did not produce a retained snapshot");
    }
    rc = yvex_deepseek_v4_ir_build(
        &ir, &handoff->verification, &ir_failure, err);
    if (rc != YVEX_OK) {
        yvex_source_tensor_snapshot_release(snapshot);
        yvex_deepseek_payload_handoff_close(handoff);
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_ARCHITECTURE;
        return rc;
    }
    rc = yvex_deepseek_tensor_coverage_build(
        &handoff->coverage, &handoff->verification, ir, snapshot, NULL,
        &coverage_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_deepseek_transform_ir_build(
            &handoff->transform_ir, &handoff->verification, ir,
            handoff->coverage, NULL, &transform_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_deepseek_gguf_map_build(
            &handoff->map, ir, handoff->transform_ir, &map_failure, err);
    yvex_deepseek_v4_ir_close(ir);
    if (rc != YVEX_OK) {
        yvex_deepseek_payload_failure_code code = !handoff->coverage
            ? YVEX_DEEPSEEK_PAYLOAD_FAILURE_COVERAGE
            : (!handoff->transform_ir
                ? YVEX_DEEPSEEK_PAYLOAD_FAILURE_TRANSFORM_IR
                : YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING);
        yvex_source_tensor_snapshot_release(snapshot);
        yvex_deepseek_payload_handoff_close(handoff);
        if (failure) failure->code = code;
        return rc;
    }
    memset(&payload_options, 0, sizeof(payload_options));
    payload_options.verification_options = &handoff->source_options;
    payload_options.verification = &handoff->verification;
    payload_options.snapshot = snapshot;
    payload_options.budget = options->budget;
    payload_options.manifest_path = handoff->verification.manifest_path;
    rc = yvex_source_payload_session_open(
        &handoff->session, &payload_options,
        failure ? &failure->payload_failure : NULL, err);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc != YVEX_OK) {
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_SOURCE;
        yvex_deepseek_payload_handoff_close(handoff);
        return rc;
    }
    rc = yvex_transform_binding_create(
        &handoff->binding, handoff->transform_ir, handoff->session, NULL,
        &transform_failure, err);
    if (rc != YVEX_OK) {
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_BINDING;
        yvex_deepseek_payload_handoff_close(handoff);
        return rc;
    }
    rc = handoff_resolve(handoff, options, failure, err);
    if (rc != YVEX_OK) {
        yvex_deepseek_payload_handoff_close(handoff);
        return rc;
    }
    *out = handoff;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Releases plan, session, map, coverage, and copied paths in dependency order. */
void yvex_deepseek_payload_handoff_close(
    yvex_deepseek_payload_handoff *handoff)
{
    if (!handoff) return;
    yvex_source_payload_plan_close(handoff->plan);
    yvex_transform_binding_release(&handoff->binding);
    (void)yvex_source_payload_session_release(&handoff->session, NULL, NULL);
    yvex_deepseek_gguf_map_close(handoff->map);
    yvex_transform_ir_release(&handoff->transform_ir);
    yvex_deepseek_tensor_coverage_close(handoff->coverage);
    free(handoff->manifest_path);
    free(handoff->models_root);
    free(handoff->source_path);
    free(handoff);
}

const yvex_deepseek_payload_handoff_summary *
yvex_deepseek_payload_handoff_summary_get(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? &handoff->summary : NULL;
}

const yvex_source_verification *yvex_deepseek_payload_handoff_verification(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? &handoff->verification : NULL;
}

const yvex_deepseek_gguf_map *yvex_deepseek_payload_handoff_map(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->map : NULL;
}

const yvex_transform_ir *yvex_deepseek_payload_handoff_transform_ir(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->transform_ir : NULL;
}

const yvex_transform_binding *yvex_deepseek_payload_handoff_binding(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->binding : NULL;
}

yvex_source_payload_session *yvex_deepseek_payload_handoff_session(
    yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->session : NULL;
}

const yvex_source_payload_plan *yvex_deepseek_payload_handoff_plan(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->plan : NULL;
}

const char *yvex_deepseek_payload_failure_name(
    yvex_deepseek_payload_failure_code code)
{
    static const char *const names[] = {
        "none", "invalid-argument", "source-verification",
        "architecture-ir", "tensor-coverage", "transform-ir", "gguf-mapping",
        "mapping-identity-mismatch", "mapping-contribution",
        "payload-range", "transform-binding", "payload-plan",
        "allocation-failure"
    };
    size_t count = sizeof(names) / sizeof(names[0]);

    return code >= 0 && (size_t)code < count ? names[code]
                                                : "unknown-handoff-failure";
}
