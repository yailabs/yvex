#ifndef YVEX_RUNTIME_PRIVATE_H
#define YVEX_RUNTIME_PRIVATE_H

/*
 * yvex_runtime_private.h - Private engine/session runtime state.
 *
 * This header is shared only by runtime-owned source files that need the
 * concrete engine shape. It is not a public API and must not become a generic
 * dumping ground for command state.
 */

#include <yvex/yvex.h>

#define YVEX_RUNTIME_REASON_CAP 256u

struct yvex_engine {
    char *model_path;
    yvex_engine_status status;
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_graph *graph;
    yvex_backend *weight_backend;
    yvex_weight_table *weights;
    yvex_materialize_summary weight_summary;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

#endif
