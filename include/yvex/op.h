/*
 * Owner: abi.op (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Graph operations
 *
 * File: include/yvex/op.h
 * Layer: public graph API
 *
 * Purpose:
 *   Defines inspectable graph value and operation records for graph planner planning.
 *   These records describe planned computation only; they do not execute
 *   kernels or bind backend function pointers.
 *
 * Owns:
 *   - yvex_value_kind
 *   - yvex_residency
 *   - yvex_graph_value_info
 *   - yvex_op_kind
 *   - yvex_op_status
 *   - yvex_graph_op_info
 *
 * Does not own:
 *   - backend ABI
 *   - tensor allocation
 *   - graph execution
 *
 * Used by:
 *   - graph builder
 *   - memory planner
 *   - CLI graph/plan commands
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_graph
 */
#ifndef YVEX_OP_H
#define YVEX_OP_H

#include <yvex/dtype.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_GRAPH_MAX_DIMS 4u

typedef enum {
    YVEX_VALUE_TOKEN_IDS = 0,
    YVEX_VALUE_ACTIVATION,
    YVEX_VALUE_WEIGHT,
    YVEX_VALUE_KV_CACHE,
    YVEX_VALUE_LOGITS,
    YVEX_VALUE_TEMPORARY,
    YVEX_VALUE_UNKNOWN
} yvex_value_kind;

typedef enum {
    YVEX_RESIDENCY_HOST = 0,
    YVEX_RESIDENCY_DEVICE,
    YVEX_RESIDENCY_BACKEND_DECIDES
} yvex_residency;

typedef struct {
    unsigned int id;
    yvex_value_kind kind;
    const char *name;
    unsigned int rank;
    unsigned long long dims[YVEX_GRAPH_MAX_DIMS];
    yvex_dtype dtype;
    yvex_residency residency;
    const char *source_tensor_name;
} yvex_graph_value_info;

typedef enum {
    YVEX_OP_EMBED = 0,
    YVEX_OP_RMS_NORM,
    YVEX_OP_MATMUL,
    YVEX_OP_ROPE,
    YVEX_OP_ATTENTION_PREFILL,
    YVEX_OP_ATTENTION_DECODE,
    YVEX_OP_KV_WRITE,
    YVEX_OP_KV_READ,
    YVEX_OP_SWIGLU,
    YVEX_OP_RESIDUAL_ADD,
    YVEX_OP_LOGITS,
    YVEX_OP_SAMPLER,
    YVEX_OP_UNSUPPORTED
} yvex_op_kind;

typedef enum {
    YVEX_OP_STATUS_PLANNED = 0,
    YVEX_OP_STATUS_MISSING_INPUT,
    YVEX_OP_STATUS_UNSUPPORTED,
    YVEX_OP_STATUS_INVALID_SHAPE
} yvex_op_status;

typedef struct {
    unsigned int id;
    yvex_op_kind kind;
    yvex_op_status status;
    const char *name;
    unsigned int input_count;
    unsigned int output_count;
    const char *reason;
} yvex_graph_op_info;

const char *yvex_op_kind_name(yvex_op_kind kind);
const char *yvex_op_status_name(yvex_op_status status);
const char *yvex_value_kind_name(yvex_value_kind kind);
const char *yvex_residency_name(yvex_residency residency);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_OP_H */
