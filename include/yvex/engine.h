/*
 * YVEX - Engine runtime object
 *
 * File: include/yvex/engine.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the engine/session layer engine object that assembles artifact, GGUF, tensor table,
 *   model descriptor, tokenizer, and optional graph planning state. The
 *   engine is inspectable runtime structure; it does not execute inference.
 *
 * Owns:
 *   - yvex_engine
 *   - engine lifecycle
 *   - engine summary and diagnostics
 *
 * Does not own:
 *   - sessions
 *   - sampler/runtime generation
 *   - server/provider behavior
 *
 * Used by:
 *   - yvex engine
 *   - yvex session
 *   - engine/session layer runtime tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_engine
 */
#ifndef YVEX_ENGINE_H
#define YVEX_ENGINE_H

#include <yvex/graph.h>
#include <yvex/model.h>
#include <yvex/tensor.h>
#include <yvex/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_engine yvex_engine;

typedef enum {
    YVEX_ENGINE_STATUS_EMPTY = 0,
    YVEX_ENGINE_STATUS_LOADED,
    YVEX_ENGINE_STATUS_PARTIAL,
    YVEX_ENGINE_STATUS_UNSUPPORTED,
    YVEX_ENGINE_STATUS_FAILED
} yvex_engine_status;

typedef struct {
    const char *model_path;
    int load_tokenizer;
    int build_descriptor;
    int build_default_graph;
    int attach_weights;
    const char *backend_name;
    int require_all_weights;
} yvex_engine_options;

typedef struct {
    yvex_engine_status status;
    const char *model_path;
    const char *model_name;
    const char *architecture;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    unsigned long long unsupported_tensor_accounting;
    const char *tokenizer_model;
    const char *tokenizer_support;
    const char *graph_status;
    int weights_attached;
    const char *weights_backend;
    unsigned long long weight_tensor_count;
    unsigned long long weight_total_bytes;
    unsigned long long weight_backend_allocated_bytes;
    int graph_execution_ready;
} yvex_engine_summary;

#define YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES 16u
#define YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES 16u

typedef struct {
    unsigned int token_id;
} yvex_fixture_graph_options;

typedef struct {
    int executed;
    const char *backend_name;
    const char *op_name;
    const char *weight_name;
    unsigned int token_id;
    unsigned long long node_count;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long output_checksum;
    unsigned long long output_value_count;
    float output_values[YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES];
    int execution_ready;
    int graph_execution_ready;
} yvex_fixture_graph_result;

typedef struct {
    unsigned int token_id;
} yvex_partial_graph_options;

typedef struct {
    int executed;
    const char *backend_name;
    const char *segment_name;
    const char *weight_name;
    const char *weight_dtype;
    const char *output_dtype;
    unsigned int token_id;
    unsigned long long node_count;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long output_checksum;
    unsigned long long reference_checksum;
    double max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES];
    int execution_ready;
    int graph_execution_ready;
} yvex_partial_graph_result;

int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err);
int yvex_engine_open_path(yvex_engine **out,
                          const char *model_path,
                          yvex_error *err);
void yvex_engine_close(yvex_engine *engine);

yvex_engine_status yvex_engine_status_of(const yvex_engine *engine);
const char *yvex_engine_status_name(yvex_engine_status status);

const char *yvex_engine_model_path(const yvex_engine *engine);
const yvex_model_descriptor *yvex_engine_model(const yvex_engine *engine);
const yvex_tensor_table *yvex_engine_tensors(const yvex_engine *engine);
const yvex_tokenizer *yvex_engine_tokenizer(const yvex_engine *engine);
const yvex_graph *yvex_engine_graph(const yvex_engine *engine);

int yvex_engine_get_summary(const yvex_engine *engine,
                            yvex_engine_summary *out,
                            yvex_error *err);

int yvex_engine_execute_fixture_graph(yvex_engine *engine,
                                      const yvex_fixture_graph_options *options,
                                      yvex_fixture_graph_result *out,
                                      yvex_error *err);
int yvex_engine_execute_partial_graph(yvex_engine *engine,
                                      const yvex_partial_graph_options *options,
                                      yvex_partial_graph_result *out,
                                      yvex_error *err);

const char *yvex_engine_diagnostic_reason(const yvex_engine *engine);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ENGINE_H */
