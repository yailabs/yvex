/*
 * yvex_graph.c - Graph construction state and helpers.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   graph lifecycle, value/op/missing tables, graph status helpers, shape
 *   helpers, and graph construction from model descriptors.
 *
 * Does not own:
 *   memory-plan construction, execution-plan construction, graph reports,
 *   primitive proofs, graph guards, CLI input parsing, command dispatch,
 *   rendering, stdout/stderr output, runtime generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   graph construction records descriptor-level facts only and does not execute
 *   backend operations or emit operator output.
 *
 * Boundary:
 *   graph construction is not transformer execution or generation readiness.
 */
#include "yvex_graph_private.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void yvex_graph_close(yvex_graph *graph)
{
    unsigned long long i;

    if (!graph) {
        return;
    }
    free(graph->architecture);
    free(graph->model_name);
    for (i = 0; i < graph->value_count; ++i) {
        yvex_graph_value_clear(&graph->values[i]);
    }
    for (i = 0; i < graph->op_count; ++i) {
        yvex_graph_op_clear(&graph->ops[i]);
    }
    for (i = 0; i < graph->missing_count; ++i) {
        yvex_graph_missing_clear(&graph->missing[i]);
    }
    free(graph->values);
    free(graph->ops);
    free(graph->edges);
    free(graph->missing);
    free(graph);
}

yvex_graph_status yvex_graph_status_of(const yvex_graph *graph)
{
    return graph ? graph->status : YVEX_GRAPH_STATUS_EMPTY;
}

const char *yvex_graph_status_name(yvex_graph_status status)
{
    switch (status) {
    case YVEX_GRAPH_STATUS_EMPTY: return "empty";
    case YVEX_GRAPH_STATUS_BUILT: return "built";
    case YVEX_GRAPH_STATUS_PARTIAL: return "partial";
    case YVEX_GRAPH_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_GRAPH_STATUS_INVALID: return "invalid";
    }
    return "unknown";
}

unsigned long long yvex_graph_value_count(const yvex_graph *graph)
{
    return graph ? graph->value_count : 0;
}

unsigned long long yvex_graph_op_count(const yvex_graph *graph)
{
    return graph ? graph->op_count : 0;
}

unsigned long long yvex_graph_missing_required_count(const yvex_graph *graph)
{
    return graph ? graph->missing_count : 0;
}

const yvex_graph_value_info *yvex_graph_value_at(const yvex_graph *graph,
                                                 unsigned long long index)
{
    if (!graph || index >= graph->value_count) {
        return NULL;
    }
    return &graph->values[index];
}

const yvex_graph_op_info *yvex_graph_op_at(const yvex_graph *graph,
                                           unsigned long long index)
{
    if (!graph || index >= graph->op_count) {
        return NULL;
    }
    return &graph->ops[index];
}

const yvex_graph_missing_required *yvex_graph_missing_required_at(const yvex_graph *graph,
                                                                  unsigned long long index)
{
    if (!graph || index >= graph->missing_count) {
        return NULL;
    }
    return &graph->missing[index];
}



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
const char *yvex_op_kind_name(yvex_op_kind kind)
{
    switch (kind) {
    case YVEX_OP_EMBED: return "embed";
    case YVEX_OP_RMS_NORM: return "rms_norm";
    case YVEX_OP_MATMUL: return "matmul";
    case YVEX_OP_ROPE: return "rope";
    case YVEX_OP_ATTENTION_PREFILL: return "attention_prefill";
    case YVEX_OP_ATTENTION_DECODE: return "attention_decode";
    case YVEX_OP_KV_WRITE: return "kv_write";
    case YVEX_OP_KV_READ: return "kv_read";
    case YVEX_OP_SWIGLU: return "swiglu";
    case YVEX_OP_RESIDUAL_ADD: return "residual_add";
    case YVEX_OP_LOGITS: return "logits";
    case YVEX_OP_SAMPLER: return "sampler";
    case YVEX_OP_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

const char *yvex_op_status_name(yvex_op_status status)
{
    switch (status) {
    case YVEX_OP_STATUS_PLANNED: return "planned";
    case YVEX_OP_STATUS_MISSING_INPUT: return "missing_input";
    case YVEX_OP_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_OP_STATUS_INVALID_SHAPE: return "invalid_shape";
    }
    return "unknown";
}

const char *yvex_value_kind_name(yvex_value_kind kind)
{
    switch (kind) {
    case YVEX_VALUE_TOKEN_IDS: return "token_ids";
    case YVEX_VALUE_ACTIVATION: return "activation";
    case YVEX_VALUE_WEIGHT: return "weight";
    case YVEX_VALUE_KV_CACHE: return "kv_cache";
    case YVEX_VALUE_LOGITS: return "logits";
    case YVEX_VALUE_TEMPORARY: return "temporary";
    case YVEX_VALUE_UNKNOWN: return "unknown";
    }
    return "unknown";
}

const char *yvex_residency_name(yvex_residency residency)
{
    switch (residency) {
    case YVEX_RESIDENCY_HOST: return "host";
    case YVEX_RESIDENCY_DEVICE: return "device";
    case YVEX_RESIDENCY_BACKEND_DECIDES: return "backend_decides";
    }
    return "unknown";
}
int yvex_shape_product(const unsigned long long *dims,
                       unsigned int rank,
                       unsigned long long *out,
                       yvex_error *err)
{
    unsigned long long product = 1;
    unsigned int i;

    if (!out || (rank > 0 && !dims)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_shape_product", "dims and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = 0;

    if (rank == 0 || rank > YVEX_GRAPH_MAX_DIMS) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_shape_product", "rank is out of range");
        return YVEX_ERR_FORMAT;
    }

    for (i = 0; i < rank; ++i) {
        if (dims[i] == 0) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_shape_product", "dimension must be non-zero");
            return YVEX_ERR_FORMAT;
        }
        if (product > ULLONG_MAX / dims[i]) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_shape_product", "dimension product overflow");
            return YVEX_ERR_BOUNDS;
        }
        product *= dims[i];
    }

    *out = product;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_shape_equal(const unsigned long long *a,
                     unsigned int a_rank,
                     const unsigned long long *b,
                     unsigned int b_rank)
{
    unsigned int i;

    if (a_rank != b_rank || a_rank > YVEX_GRAPH_MAX_DIMS || !a || !b) {
        return 0;
    }
    for (i = 0; i < a_rank; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int yvex_shape_copy(unsigned long long *dst,
                    unsigned int dst_cap,
                    const unsigned long long *src,
                    unsigned int src_rank,
                    yvex_error *err)
{
    unsigned int i;

    if (!dst || (src_rank > 0 && !src)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_shape_copy", "src and dst are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (src_rank > dst_cap || src_rank > YVEX_GRAPH_MAX_DIMS) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_shape_copy", "destination shape is too small");
        return YVEX_ERR_BOUNDS;
    }
    for (i = 0; i < src_rank; ++i) {
        if (src[i] == 0) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_shape_copy", "dimension must be non-zero");
            return YVEX_ERR_FORMAT;
        }
        dst[i] = src[i];
    }
    for (; i < dst_cap; ++i) {
        dst[i] = 0;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}



char *yvex_graph_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

void yvex_graph_value_clear(yvex_graph_value_info *value)
{
    if (!value) {
        return;
    }
    free((char *)value->name);
    free((char *)value->source_tensor_name);
    memset(value, 0, sizeof(*value));
}

void yvex_graph_op_clear(yvex_graph_op_info *op)
{
    if (!op) {
        return;
    }
    free((char *)op->name);
    free((char *)op->reason);
    memset(op, 0, sizeof(*op));
}

void yvex_graph_missing_clear(yvex_graph_missing_required *missing)
{
    if (!missing) {
        return;
    }
    free((char *)missing->role_name);
    free((char *)missing->reason);
    memset(missing, 0, sizeof(*missing));
}

static int reserve_values(yvex_graph *graph, unsigned long long need, yvex_error *err)
{
    yvex_graph_value_info *next;
    unsigned long long next_cap;

    if (graph->value_cap >= need) {
        return YVEX_OK;
    }
    next_cap = graph->value_cap == 0 ? 4 : graph->value_cap * 2u;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->values))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "value table too large");
        return YVEX_ERR_NOMEM;
    }
    next = (yvex_graph_value_info *)realloc(graph->values, (size_t)next_cap * sizeof(*graph->values));
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "failed to grow value table");
        return YVEX_ERR_NOMEM;
    }
    memset(next + graph->value_cap, 0, (size_t)(next_cap - graph->value_cap) * sizeof(*next));
    graph->values = next;
    graph->value_cap = next_cap;
    return YVEX_OK;
}

static int reserve_ops(yvex_graph *graph, unsigned long long need, yvex_error *err)
{
    yvex_graph_op_info *next_ops;
    yvex_graph_op_edges *next_edges;
    unsigned long long next_cap;

    if (graph->op_cap >= need) {
        return YVEX_OK;
    }
    next_cap = graph->op_cap == 0 ? 4 : graph->op_cap * 2u;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->ops)) ||
        next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->edges))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "op table too large");
        return YVEX_ERR_NOMEM;
    }
    next_ops = (yvex_graph_op_info *)realloc(graph->ops, (size_t)next_cap * sizeof(*graph->ops));
    if (!next_ops) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "failed to grow op table");
        return YVEX_ERR_NOMEM;
    }
    graph->ops = next_ops;
    next_edges = (yvex_graph_op_edges *)realloc(graph->edges, (size_t)next_cap * sizeof(*graph->edges));
    if (!next_edges) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "failed to grow op edge table");
        return YVEX_ERR_NOMEM;
    }
    memset(graph->ops + graph->op_cap, 0, (size_t)(next_cap - graph->op_cap) * sizeof(*graph->ops));
    memset(next_edges + graph->op_cap, 0, (size_t)(next_cap - graph->op_cap) * sizeof(*next_edges));
    graph->edges = next_edges;
    graph->op_cap = next_cap;
    return YVEX_OK;
}

static int reserve_missing(yvex_graph *graph, unsigned long long need, yvex_error *err)
{
    yvex_graph_missing_required *next;
    unsigned long long next_cap;

    if (graph->missing_cap >= need) {
        return YVEX_OK;
    }
    next_cap = graph->missing_cap == 0 ? 4 : graph->missing_cap * 2u;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->missing))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "missing table too large");
        return YVEX_ERR_NOMEM;
    }
    next = (yvex_graph_missing_required *)realloc(graph->missing,
                                                 (size_t)next_cap * sizeof(*graph->missing));
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "failed to grow missing table");
        return YVEX_ERR_NOMEM;
    }
    memset(next + graph->missing_cap, 0, (size_t)(next_cap - graph->missing_cap) * sizeof(*next));
    graph->missing = next;
    graph->missing_cap = next_cap;
    return YVEX_OK;
}

int yvex_graph_add_value(yvex_graph *graph,
                         yvex_value_kind kind,
                         const char *name,
                         unsigned int rank,
                         const unsigned long long *dims,
                         yvex_dtype dtype,
                         yvex_residency residency,
                         const char *source_tensor_name,
                         unsigned int *out_id,
                         yvex_error *err)
{
    yvex_graph_value_info *value;
    unsigned int i;
    int rc;

    if (!graph || !name || rank > YVEX_GRAPH_MAX_DIMS || (rank > 0 && !dims)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_add_value", "invalid value arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = reserve_values(graph, graph->value_count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    value = &graph->values[graph->value_count];
    value->id = (unsigned int)graph->value_count;
    value->kind = kind;
    value->name = yvex_graph_strdup(name);
    if (!value->name) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "failed to copy value name");
        return YVEX_ERR_NOMEM;
    }
    value->rank = rank;
    for (i = 0; i < rank; ++i) {
        value->dims[i] = dims[i];
    }
    value->dtype = dtype;
    value->residency = residency;
    if (source_tensor_name) {
        value->source_tensor_name = yvex_graph_strdup(source_tensor_name);
        if (!value->source_tensor_name) {
            yvex_graph_value_clear(value);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "failed to copy source tensor");
            return YVEX_ERR_NOMEM;
        }
    }

    if (out_id) {
        *out_id = value->id;
    }
    graph->value_count += 1u;
    return YVEX_OK;
}

int yvex_graph_add_op(yvex_graph *graph,
                      yvex_op_kind kind,
                      yvex_op_status status,
                      const char *name,
                      const unsigned int *inputs,
                      unsigned int input_count,
                      const unsigned int *outputs,
                      unsigned int output_count,
                      const char *reason,
                      yvex_error *err)
{
    yvex_graph_op_info *op;
    yvex_graph_op_edges *edges;
    unsigned int i;
    int rc;

    if (!graph || !name || input_count > 4u || output_count > 4u ||
        (input_count > 0 && !inputs) || (output_count > 0 && !outputs)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_add_op", "invalid op arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = reserve_ops(graph, graph->op_count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    op = &graph->ops[graph->op_count];
    edges = &graph->edges[graph->op_count];
    op->id = (unsigned int)graph->op_count;
    op->kind = kind;
    op->status = status;
    op->name = yvex_graph_strdup(name);
    op->reason = yvex_graph_strdup(reason ? reason : "");
    if (!op->name || !op->reason) {
        yvex_graph_op_clear(op);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "failed to copy op text");
        return YVEX_ERR_NOMEM;
    }
    op->input_count = input_count;
    op->output_count = output_count;
    for (i = 0; i < input_count; ++i) {
        edges->input_ids[i] = inputs[i];
    }
    for (i = 0; i < output_count; ++i) {
        edges->output_ids[i] = outputs[i];
    }

    graph->op_count += 1u;
    return YVEX_OK;
}

int yvex_graph_add_missing(yvex_graph *graph,
                           yvex_tensor_role role,
                           const char *reason,
                           yvex_error *err)
{
    yvex_graph_missing_required *missing;
    int rc;

    if (!graph) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_add_missing", "graph is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = reserve_missing(graph, graph->missing_count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    missing = &graph->missing[graph->missing_count];
    missing->role = role;
    missing->role_name = yvex_graph_strdup(yvex_tensor_role_name(role));
    missing->reason = yvex_graph_strdup(reason ? reason : "");
    if (!missing->role_name || !missing->reason) {
        yvex_graph_missing_clear(missing);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "failed to copy missing diagnostic");
        return YVEX_ERR_NOMEM;
    }
    graph->missing_count += 1u;
    return YVEX_OK;
}

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index)
{
    if (!graph || index >= graph->op_count) {
        return NULL;
    }
    return &graph->edges[index];
}
