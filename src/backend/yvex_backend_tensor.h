/*
 * yvex_backend_tensor.h - backend tensor boundary facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   backend tensor allocation/read/write/bind status records.
 *
 * Does not own:
 *   graph execution, qtype policy, quantization, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   unsupported tensor operations refuse with typed status.
 *
 * Boundary:
 *   backend tensor binding is not graph execution.
 */
#ifndef YVEX_BACKEND_TENSOR_H
#define YVEX_BACKEND_TENSOR_H

typedef struct {
    const char *operation;
    const char *status;
    const char *reason;
} yvex_backend_tensor_fact;

void yvex_backend_tensor_refuse(yvex_backend_tensor_fact *fact,
                                const char *operation,
                                const char *reason);

#endif
