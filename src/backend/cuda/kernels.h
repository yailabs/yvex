/*
 * Owner: backend.cuda.kernels (backend.cuda).
 * Owns: the platform-interface boundary consumed by backend,graph.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=backend and visibility=private match config/source_owners.tsv.
 * Boundary: platform-interface; moving this contract requires an ownership-manifest change.
 *
 */
#ifndef YVEX_CUDA_KERNELS_H
#define YVEX_CUDA_KERNELS_H

extern const char yvex_cuda_kernels_ptx[];

#endif /* YVEX_CUDA_KERNELS_H */
