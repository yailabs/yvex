/*
 * yvex_deepseek_payload_handoff.h - DeepSeek mapping-to-payload adapter ABI.
 *
 * Owner: src/model/target.
 * Owns: exact map contribution resolution into common source payload ranges.
 * Does not own: common payload IO, source parsing, transforms, quantization, or output.
 * Invariants: the pinned mapping identity and every contribution resolve exactly once.
 * Boundary: handoff coverage does not execute any mapped transformation.
 */
#ifndef YVEX_DEEPSEEK_PAYLOAD_HANDOFF_H
#define YVEX_DEEPSEEK_PAYLOAD_HANDOFF_H

#include "yvex_deepseek_gguf_map.h"
#include "../compilation/yvex_deepseek_transform_ir.h"
#include "../compilation/yvex_transform_binding.h"
#include "../../source/yvex_source_payload.h"

#define YVEX_DEEPSEEK_PAYLOAD_MAPPING_IDENTITY \
    YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY

typedef enum {
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_SOURCE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_ARCHITECTURE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_COVERAGE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_TRANSFORM_IR,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING_IDENTITY,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_BINDING,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_PLAN,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION
} yvex_deepseek_payload_failure_code;

typedef struct {
    yvex_deepseek_payload_failure_code code;
    unsigned long long descriptor_index;
    unsigned long long contribution_index;
    yvex_source_payload_failure payload_failure;
} yvex_deepseek_payload_failure;

typedef struct {
    unsigned long long mapping_identity;
    char transform_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    unsigned long long source_snapshot_identity;
    unsigned long long descriptor_count;
    unsigned long long descriptors_covered;
    unsigned long long contribution_count;
    unsigned long long contributions_resolved;
    unsigned long long direct_contributions;
    unsigned long long fp8_weight_contributions;
    unsigned long long e8m0_scale_contributions;
    unsigned long long expert_contributions;
    unsigned long long i64_router_contributions;
    unsigned long long global_contributions;
    unsigned long long norm_contributions;
    unsigned long long shared_expert_contributions;
    unsigned long long output_head_contributions;
    unsigned long long mtp_contributions;
    unsigned long long routed_expert_logical_bytes;
    unsigned long long output_head_logical_bytes;
    unsigned long long range_lookup_count;
    int complete;
} yvex_deepseek_payload_handoff_summary;

typedef struct yvex_deepseek_payload_handoff yvex_deepseek_payload_handoff;

typedef struct {
    const char *source_path;
    const char *models_root;
    const char *manifest_path;
    yvex_source_payload_budget budget;
    size_t chunk_bytes;
    size_t page_bytes;
} yvex_deepseek_payload_handoff_options;

int yvex_deepseek_payload_handoff_open(
    yvex_deepseek_payload_handoff **out,
    const yvex_deepseek_payload_handoff_options *options,
    yvex_deepseek_payload_failure *failure,
    yvex_error *err);
void yvex_deepseek_payload_handoff_close(
    yvex_deepseek_payload_handoff *handoff);
const yvex_deepseek_payload_handoff_summary *
yvex_deepseek_payload_handoff_summary_get(
    const yvex_deepseek_payload_handoff *handoff);
const yvex_source_verification *yvex_deepseek_payload_handoff_verification(
    const yvex_deepseek_payload_handoff *handoff);
const yvex_deepseek_gguf_map *yvex_deepseek_payload_handoff_map(
    const yvex_deepseek_payload_handoff *handoff);
const yvex_transform_ir *yvex_deepseek_payload_handoff_transform_ir(
    const yvex_deepseek_payload_handoff *handoff);
const yvex_transform_binding *yvex_deepseek_payload_handoff_binding(
    const yvex_deepseek_payload_handoff *handoff);
yvex_source_payload_session *yvex_deepseek_payload_handoff_session(
    yvex_deepseek_payload_handoff *handoff);
const yvex_source_payload_plan *yvex_deepseek_payload_handoff_plan(
    const yvex_deepseek_payload_handoff *handoff);
const char *yvex_deepseek_payload_failure_name(
    yvex_deepseek_payload_failure_code code);

#endif
