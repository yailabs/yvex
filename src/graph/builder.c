/*
 * YVEX - Graph builder
 *
 * File: src/graph/builder.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Builds the first deterministic YVEX planning graph from a model
 *   descriptor and tensor table. graph planner emits inspectable values, planned ops and
 *   missing-role diagnostics, but it does not execute computation.
 *
 * Implements:
 *   - yvex_graph_build_for_model
 *
 * Invariants:
 *   - token embedding can produce a partial fixture graph
 *   - missing output roles are diagnostics, not crashes
 *   - no backend capability or kernel dispatch is consulted
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 */
#include "graph_internal.h"

#include <stdlib.h>

static const yvex_tensor_info *find_role(const yvex_tensor_table *table, yvex_tensor_role role)
{
    unsigned long long i;

    for (i = 0; i < yvex_tensor_table_count(table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(table, i);
        if (tensor && tensor->role == role) {
            return tensor;
        }
    }
    return NULL;
}

static int add_required_diagnostics(yvex_graph *graph,
                                    const yvex_tensor_info *token_embedding,
                                    const yvex_tensor_info *output_norm,
                                    const yvex_tensor_info *output_head,
                                    yvex_error *err)
{
    int rc;

    if (!token_embedding) {
        rc = yvex_graph_add_missing(graph, YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
                                    "required for token embedding", err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (!output_norm) {
        rc = yvex_graph_add_missing(graph, YVEX_TENSOR_ROLE_OUTPUT_NORM,
                                    "required for final normalization", err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (!output_head) {
        rc = yvex_graph_add_missing(graph, YVEX_TENSOR_ROLE_OUTPUT_HEAD,
                                    "required for logits", err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    return YVEX_OK;
}

static int add_embedding_path(yvex_graph *graph,
                              const yvex_tensor_info *token_embedding,
                              unsigned long long sequence_length,
                              yvex_error *err)
{
    unsigned long long token_shape[1];
    unsigned long long hidden_shape[2];
    unsigned int token_ids_value;
    unsigned int weight_value;
    unsigned int hidden_value;
    unsigned int inputs[2];
    unsigned int outputs[1];
    int rc;

    if (!token_embedding) {
        return YVEX_OK;
    }
    if (token_embedding->rank < 2 || token_embedding->dims[0] == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_graph_build_for_model",
                       "token embedding tensor requires rank >= 2 and non-zero hidden dimension");
        return YVEX_ERR_FORMAT;
    }

    token_shape[0] = sequence_length;
    rc = yvex_graph_add_value(graph, YVEX_VALUE_TOKEN_IDS, "token_ids", 1,
                              token_shape, YVEX_DTYPE_I32, YVEX_RESIDENCY_HOST,
                              NULL, &token_ids_value, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_graph_add_value(graph, YVEX_VALUE_WEIGHT, token_embedding->name,
                              token_embedding->rank, token_embedding->dims,
                              token_embedding->dtype, YVEX_RESIDENCY_HOST,
                              token_embedding->name, &weight_value, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    hidden_shape[0] = sequence_length;
    hidden_shape[1] = token_embedding->dims[0];
    rc = yvex_graph_add_value(graph, YVEX_VALUE_ACTIVATION, "hidden", 2,
                              hidden_shape, token_embedding->dtype,
                              YVEX_RESIDENCY_HOST, NULL, &hidden_value, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    inputs[0] = token_ids_value;
    inputs[1] = weight_value;
    outputs[0] = hidden_value;
    return yvex_graph_add_op(graph, YVEX_OP_EMBED, YVEX_OP_STATUS_PLANNED,
                             "embed", inputs, 2, outputs, 1, "", err);
}

int yvex_graph_build_for_model(yvex_graph **out,
                                const yvex_model_descriptor *model,
                                const yvex_tensor_table *tensors,
                                const yvex_graph_build_options *options,
                                yvex_error *err)
{
    yvex_graph *graph;
    const yvex_tensor_info *token_embedding;
    const yvex_tensor_info *output_norm;
    const yvex_tensor_info *output_head;
    unsigned long long sequence_length = 1;
    unsigned long long context_length = 1;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_build_for_model", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_build_for_model",
                       "model and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (options) {
        if (options->sequence_length > 0) {
            sequence_length = options->sequence_length;
        }
        if (options->context_length > 0) {
            context_length = options->context_length;
        }
        (void)options->include_decode_step;
        (void)options->include_prefill_path;
    }
    if ((!options || options->context_length == 0) && yvex_model_context_length(model) > 0) {
        context_length = yvex_model_context_length(model);
    }

    graph = (yvex_graph *)calloc(1, sizeof(*graph));
    if (!graph) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_build_for_model", "failed to allocate graph");
        return YVEX_ERR_NOMEM;
    }
    graph->status = YVEX_GRAPH_STATUS_EMPTY;
    graph->sequence_length = sequence_length;
    graph->context_length = context_length;
    graph->architecture = yvex_graph_strdup(yvex_arch_name(yvex_model_arch(model)));
    graph->model_name = yvex_graph_strdup(yvex_model_name(model));
    if (!graph->architecture || !graph->model_name) {
        yvex_graph_close(graph);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_build_for_model", "failed to copy graph labels");
        return YVEX_ERR_NOMEM;
    }

    token_embedding = find_role(tensors, YVEX_TENSOR_ROLE_TOKEN_EMBEDDING);
    output_norm = find_role(tensors, YVEX_TENSOR_ROLE_OUTPUT_NORM);
    output_head = find_role(tensors, YVEX_TENSOR_ROLE_OUTPUT_HEAD);

    rc = add_embedding_path(graph, token_embedding, sequence_length, err);
    if (rc != YVEX_OK) {
        yvex_graph_close(graph);
        return rc;
    }

    rc = add_required_diagnostics(graph, token_embedding, output_norm, output_head, err);
    if (rc != YVEX_OK) {
        yvex_graph_close(graph);
        return rc;
    }

    if (!token_embedding) {
        graph->status = YVEX_GRAPH_STATUS_UNSUPPORTED;
    } else if (graph->missing_count > 0) {
        graph->status = YVEX_GRAPH_STATUS_PARTIAL;
    } else {
        graph->status = YVEX_GRAPH_STATUS_BUILT;
    }

    *out = graph;
    yvex_error_clear(err);
    return YVEX_OK;
}
