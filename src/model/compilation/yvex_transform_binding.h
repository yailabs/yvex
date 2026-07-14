/*
 * yvex_transform_binding.h - quantizer-ready Transformation IR input binding.
 *
 * Owner:
 *   src/model/compilation
 *
 * Owns:
 *   immutable validation of sealed IR source values against one trusted source
 *   payload session and non-mutating physical-decision admission.
 *
 * Does not own:
 *   payload reads, precision selection, conversion, quantization, output
 *   buffers, GGUF encoding, writer state, rendering, or runtime execution.
 *
 * Invariants:
 *   the IR and payload session outlive the binding; all source ranges match
 *   identity, name, tensor index, dtype, shape, and relative byte geometry.
 *
 * Boundary:
 *   this binds executable inputs and exposes deterministic plan views only.
 */
#ifndef YVEX_TRANSFORM_BINDING_H
#define YVEX_TRANSFORM_BINDING_H

#include "yvex_transform_ir.h"
#include "../../source/yvex_source_payload.h"

typedef struct yvex_transform_binding yvex_transform_binding;

typedef struct {
    unsigned int physical_class;
    unsigned int encoding_id;
    int approximation_selected;
} yvex_transform_physical_decision;

typedef struct {
    unsigned long long source_count;
    unsigned long long resolved_range_count;
    unsigned long long terminal_count;
    unsigned long long node_count;
    unsigned long long range_lookup_count;
    unsigned long long source_snapshot_identity;
    char required_payload_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    char transform_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    size_t owned_bytes;
    unsigned long long payload_bytes_read;
    int payload_readable_at_bind;
    int complete;
} yvex_transform_binding_summary;

int yvex_transform_binding_create(
    yvex_transform_binding **out,
    const yvex_transform_ir *ir,
    yvex_source_payload_session *session,
    const yvex_transform_allocator *allocator,
    yvex_transform_failure *failure,
    yvex_error *err);
void yvex_transform_binding_release(yvex_transform_binding **binding);

const yvex_transform_binding_summary *yvex_transform_binding_summary_get(
    const yvex_transform_binding *binding);
const yvex_transform_ir *yvex_transform_binding_ir(
    const yvex_transform_binding *binding);
const yvex_transform_value *yvex_transform_binding_terminal_at(
    const yvex_transform_binding *binding,
    unsigned long long ordinal);
const yvex_transform_node *yvex_transform_binding_terminal_operation(
    const yvex_transform_binding *binding,
    unsigned long long ordinal);
const yvex_transform_source_value *yvex_transform_binding_source_at(
    const yvex_transform_binding *binding,
    unsigned long long source_index);
const yvex_source_payload_range *yvex_transform_binding_range_at(
    const yvex_transform_binding *binding,
    unsigned long long source_index);
int yvex_transform_binding_decision_validate(
    const yvex_transform_binding *binding,
    unsigned long long terminal_ordinal,
    const yvex_transform_physical_decision *decision,
    yvex_transform_failure *failure,
    yvex_error *err);
int yvex_transform_binding_readable_validate(
    const yvex_transform_binding *binding,
    yvex_transform_failure *failure,
    yvex_error *err);

#endif
