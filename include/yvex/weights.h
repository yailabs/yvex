/*
 * Owner: abi.weights (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Materialized weights
 *
 * File: include/yvex/weights.h
 * Layer: public model/backend bridge API
 *
 * Purpose:
 *   Defines the first weight materialization surface. fixture materialization copies tensor bytes
 *   from a parsed GGUF artifact into backend-owned tensors and exposes an
 *   inspectable materialized weight table.
 *
 * Does not own:
 *   - graph execution
 *   - prefill/decode
 *   - sampler/logits computation
 *   - inference readiness
 */
#ifndef YVEX_WEIGHTS_H
#define YVEX_WEIGHTS_H

#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/gguf.h>
#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_weight_table yvex_weight_table;
typedef struct yvex_materialized_weight yvex_materialized_weight;

typedef enum {
    YVEX_WEIGHT_STATUS_EMPTY = 0,
    YVEX_WEIGHT_STATUS_MATERIALIZED,
    YVEX_WEIGHT_STATUS_PARTIAL,
    YVEX_WEIGHT_STATUS_FAILED
} yvex_weight_status;

typedef enum {
    YVEX_WEIGHT_RESIDENCY_HOST = 0,
    YVEX_WEIGHT_RESIDENCY_CPU_BACKEND,
    YVEX_WEIGHT_RESIDENCY_CUDA_BACKEND
} yvex_weight_residency;

typedef struct {
    const char *backend_name;
    int require_all_tensors;
    int allow_unsupported_dtype;
} yvex_materialize_options;

typedef struct {
    yvex_weight_status status;
    const char *backend_name;
    const char *materialization_gate;
    const char *materialization_phase;
    const char *shape_status;
    const char *range_status;
    const char *backend_status;
    const char *cleanup_status;
    int allocation_attempted;
    int transfer_attempted;
    int cleanup_attempted;
    unsigned long long tensors_total;
    unsigned long long tensors_materialized;
    unsigned long long tensors_failed;
    unsigned long long bytes_total;
    unsigned long long bytes_materialized;
    unsigned long long backend_allocated_bytes;
    unsigned long long bytes_planned;
    unsigned long long bytes_allocated;
    unsigned long long bytes_transferred;
    int execution_ready;
} yvex_materialize_summary;

int yvex_weight_table_materialize(yvex_weight_table **out,
                                  const yvex_artifact *artifact,
                                  const yvex_gguf *gguf,
                                  const yvex_tensor_table *tensors,
                                  yvex_backend *backend,
                                  const yvex_materialize_options *options,
                                  yvex_error *err);

void yvex_weight_table_close(yvex_weight_table *weights);

unsigned long long yvex_weight_table_count(const yvex_weight_table *weights);
const yvex_materialized_weight *yvex_weight_table_at(const yvex_weight_table *weights,
                                                     unsigned long long index);
const yvex_materialized_weight *yvex_weight_table_find(const yvex_weight_table *weights,
                                                       const char *name);

int yvex_weight_table_get_summary(const yvex_weight_table *weights,
                                  yvex_materialize_summary *out,
                                  yvex_error *err);

const char *yvex_weight_status_name(yvex_weight_status status);
const char *yvex_weight_residency_name(yvex_weight_residency residency);

const char *yvex_weight_name(const yvex_materialized_weight *weight);
yvex_dtype yvex_weight_dtype(const yvex_materialized_weight *weight);
yvex_tensor_role yvex_weight_role(const yvex_materialized_weight *weight);
unsigned long long yvex_weight_bytes(const yvex_materialized_weight *weight);
yvex_weight_residency yvex_weight_residency_of(const yvex_materialized_weight *weight);
const yvex_device_tensor *yvex_weight_device_tensor(const yvex_materialized_weight *weight);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_WEIGHTS_H */
