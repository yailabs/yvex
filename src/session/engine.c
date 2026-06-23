/*
 * YVEX - Engine runtime object
 *
 * File: src/session/engine.c
 * Layer: session implementation
 *
 * Purpose:
 *   Opens a GGUF artifact and assembles the descriptor/tokenizer/graph stack
 *   into an inspectable engine/session layer engine object. The engine does not own a backend
 *   and does not execute a model.
 *
 * Implements:
 *   - yvex_engine_open
 *   - yvex_engine_open_path
 *   - yvex_engine_close
 *   - engine accessors and summaries
 *
 * Invariants:
 *   - engine owns all opened parse/descriptor objects
 *   - partial graph status is explicit and diagnostic
 *   - no inference/session/backend execution is performed
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_engine
 */
#include "session_internal.h"

#include <stdlib.h>
#include <string.h>

static void set_engine_status_from_graph(yvex_engine *engine)
{
    if (!engine->graph) {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        yvex_runtime_set_text_reason(engine->reason, sizeof(engine->reason),
                                     "graph not requested; execution not implemented in engine/session layer");
        return;
    }

    if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        engine->status = YVEX_ENGINE_STATUS_PARTIAL;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else if (yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_UNSUPPORTED ||
               yvex_graph_status_of(engine->graph) == YVEX_GRAPH_STATUS_INVALID) {
        engine->status = YVEX_ENGINE_STATUS_UNSUPPORTED;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    } else {
        engine->status = YVEX_ENGINE_STATUS_LOADED;
        yvex_runtime_set_graph_reason(engine->reason, sizeof(engine->reason), engine->graph);
    }
}

int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err)
{
    yvex_engine *engine;
    yvex_artifact_options artifact_options;
    yvex_graph_build_options graph_options;
    int rc;
    int load_tokenizer;
    int build_default_graph;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!options || !options->model_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_open", "model_path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    engine = (yvex_engine *)calloc(1, sizeof(*engine));
    if (!engine) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to allocate engine");
        return YVEX_ERR_NOMEM;
    }
    engine->status = YVEX_ENGINE_STATUS_EMPTY;
    engine->model_path = yvex_runtime_strdup(options->model_path);
    if (!engine->model_path) {
        yvex_engine_close(engine);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_open", "failed to copy model path");
        return YVEX_ERR_NOMEM;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = options->model_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;

    rc = yvex_artifact_open(&engine->artifact, &artifact_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_gguf_open(&engine->gguf, engine->artifact, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&engine->tensors, engine->gguf, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&engine->model, engine->gguf, engine->tensors, err);
    }

    load_tokenizer = options->load_tokenizer != 0;
    if (rc == YVEX_OK && load_tokenizer) {
        rc = yvex_tokenizer_from_gguf(&engine->tokenizer, engine->gguf, engine->model, err);
    }

    build_default_graph = options->build_default_graph != 0;
    if (rc == YVEX_OK && build_default_graph) {
        memset(&graph_options, 0, sizeof(graph_options));
        graph_options.sequence_length = 1;
        graph_options.context_length = yvex_model_context_length(engine->model) > 0
                                           ? yvex_model_context_length(engine->model)
                                           : 1;
        graph_options.include_prefill_path = 1;
        rc = yvex_graph_build_for_model(&engine->graph, engine->model, engine->tensors, &graph_options, err);
    }

    if (rc != YVEX_OK) {
        engine->status = YVEX_ENGINE_STATUS_FAILED;
        yvex_engine_close(engine);
        return rc;
    }

    set_engine_status_from_graph(engine);
    *out = engine;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_engine_open_path(yvex_engine **out,
                          const char *model_path,
                          yvex_error *err)
{
    yvex_engine_options options;

    memset(&options, 0, sizeof(options));
    options.model_path = model_path;
    options.load_tokenizer = 1;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    return yvex_engine_open(out, &options, err);
}

void yvex_engine_close(yvex_engine *engine)
{
    if (!engine) {
        return;
    }
    yvex_graph_close(engine->graph);
    yvex_tokenizer_close(engine->tokenizer);
    yvex_model_descriptor_close(engine->model);
    yvex_tensor_table_close(engine->tensors);
    yvex_gguf_close(engine->gguf);
    yvex_artifact_close(engine->artifact);
    free(engine->model_path);
    free(engine);
}

yvex_engine_status yvex_engine_status_of(const yvex_engine *engine)
{
    return engine ? engine->status : YVEX_ENGINE_STATUS_EMPTY;
}

const char *yvex_engine_status_name(yvex_engine_status status)
{
    switch (status) {
    case YVEX_ENGINE_STATUS_EMPTY: return "empty";
    case YVEX_ENGINE_STATUS_LOADED: return "loaded";
    case YVEX_ENGINE_STATUS_PARTIAL: return "partial";
    case YVEX_ENGINE_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_ENGINE_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_engine_model_path(const yvex_engine *engine)
{
    return engine && engine->model_path ? engine->model_path : "";
}

const yvex_model_descriptor *yvex_engine_model(const yvex_engine *engine)
{
    return engine ? engine->model : NULL;
}

const yvex_tensor_table *yvex_engine_tensors(const yvex_engine *engine)
{
    return engine ? engine->tensors : NULL;
}

const yvex_tokenizer *yvex_engine_tokenizer(const yvex_engine *engine)
{
    return engine ? engine->tokenizer : NULL;
}

const yvex_graph *yvex_engine_graph(const yvex_engine *engine)
{
    return engine ? engine->graph : NULL;
}

int yvex_engine_get_summary(const yvex_engine *engine,
                            yvex_engine_summary *out,
                            yvex_error *err)
{
    const yvex_gguf_header *header;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_get_summary", "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->status = engine->status;
    out->model_path = yvex_engine_model_path(engine);
    out->model_name = yvex_model_name(engine->model);
    out->architecture = yvex_arch_name(yvex_model_arch(engine->model));
    if (engine->gguf) {
        header = yvex_gguf_header_view(engine->gguf);
        if (header) {
            out->metadata_count = header->metadata_count;
            out->tensor_count = header->tensor_count;
        }
    }
    out->known_tensor_bytes = yvex_model_total_storage_bytes(engine->model);
    out->unsupported_tensor_accounting =
        yvex_model_unsupported_tensor_accounting_count(engine->model);
    out->tokenizer_model = engine->tokenizer
                               ? yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(engine->tokenizer))
                               : "absent";
    out->tokenizer_support = engine->tokenizer
                                 ? yvex_tokenizer_support_name(yvex_tokenizer_support_of(engine->tokenizer))
                                 : "absent";
    out->graph_status = engine->graph
                            ? yvex_graph_status_name(yvex_graph_status_of(engine->graph))
                            : "not-built";

    yvex_error_clear(err);
    return YVEX_OK;
}

const char *yvex_engine_diagnostic_reason(const yvex_engine *engine)
{
    return engine ? engine->reason : "";
}
