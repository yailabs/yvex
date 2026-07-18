/*
 * Owner: abi.model (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Model descriptor
 *
 * File: include/yvex/model.h
 * Layer: public model API
 *
 * Purpose:
 *   Defines the descriptor-only model surface built from parsed GGUF metadata
 *   and a YVEX tensor table. This describes an artifact layout; it does not
 *   load a model for execution.
 *
 * Owns:
 *   - yvex_arch
 *   - yvex_model_descriptor
 *   - descriptor summary accessors
 *
 * Does not own:
 *   - tokenizer
 *   - graph planner
 *   - backend/session runtime
 *   - inference
 *
 * Used by:
 *   - yvex inspect
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_model_descriptor
 */
#ifndef YVEX_MODEL_H
#define YVEX_MODEL_H

#include <yvex/gguf.h>
#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_ARCH_UNKNOWN = 0,
    YVEX_ARCH_LLAMA,
    YVEX_ARCH_QWEN,
    YVEX_ARCH_DEEPSEEK,
    YVEX_ARCH_GEMMA,
    YVEX_ARCH_PHI,
    YVEX_ARCH_KIMI,
    YVEX_ARCH_GLM
} yvex_arch;

typedef struct yvex_model_descriptor yvex_model_descriptor;
typedef struct yvex_artifact yvex_artifact;
typedef struct yvex_tokenizer yvex_tokenizer;

/*
 * Owns one admitted, read-only artifact/model view.  The pointers are exposed
 * as borrowed views so graph, tokenizer, and diagnostic consumers share one
 * lifecycle instead of rebuilding this stack in CLI translation units.
 */
typedef struct {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *table;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
} yvex_model_context;

int yvex_model_descriptor_from_gguf(yvex_model_descriptor **out,
                                    const yvex_gguf *gguf,
                                    const yvex_tensor_table *tensors,
                                    yvex_error *err);

void yvex_model_descriptor_close(yvex_model_descriptor *model);

yvex_arch yvex_model_arch(const yvex_model_descriptor *model);
const char *yvex_arch_name(yvex_arch arch);

const char *yvex_model_name(const yvex_model_descriptor *model);
unsigned long long yvex_model_context_length(const yvex_model_descriptor *model);
unsigned long long yvex_model_tensor_count(const yvex_model_descriptor *model);
unsigned long long yvex_model_total_storage_bytes(const yvex_model_descriptor *model);
unsigned long long yvex_model_unsupported_tensor_accounting_count(const yvex_model_descriptor *model);
unsigned long long yvex_model_role_count(const yvex_model_descriptor *model, yvex_tensor_role role);

int yvex_model_context_open(const char *path_or_alias,
                            yvex_model_context *out,
                            yvex_error *err);
int yvex_model_context_open_tokenizer(const char *path_or_alias,
                                      yvex_model_context *out,
                                      yvex_error *err);
void yvex_model_context_close(yvex_model_context *context);
int yvex_model_context_vocab_size(const char *path_or_alias,
                                  unsigned long long *out_vocab_size,
                                  yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MODEL_H */
