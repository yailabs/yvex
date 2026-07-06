/*
 * yvex_graph.c - Graph construction, primitive probes, and execution reports.
 *
 * This file owns graph construction, controlled graph execution proofs,
 * selected real graph segments, standalone graph-op probes, independent
 * references for graph correctness checks, and graph-facing operator reports.
 * It does not own token generation, provider behavior, or benchmark claims.
 */

#include "yvex_console_private.h"
#include "yvex_cli_out.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/graph.h>
#include <yvex/op.h>
#include <yvex/yvex.h>

/* Graph ownership and planning state. */

typedef struct {
    unsigned int input_ids[4];
    unsigned int output_ids[4];
} yvex_graph_op_edges;

struct yvex_graph {
    yvex_graph_status status;
    char *architecture;
    char *model_name;
    unsigned long long sequence_length;
    unsigned long long context_length;
    yvex_graph_value_info *values;
    unsigned long long value_count;
    unsigned long long value_cap;
    yvex_graph_op_info *ops;
    yvex_graph_op_edges *edges;
    unsigned long long op_count;
    unsigned long long op_cap;
    yvex_graph_missing_required *missing;
    unsigned long long missing_count;
    unsigned long long missing_cap;
};

struct yvex_memory_plan {
    yvex_memory_plan_status status;
    yvex_memory_plan_summary summary;
};

struct yvex_plan {
    char *backend_name;
    char *backend_status;
    int backend_tensor_alloc;
    int backend_tensor_read_write;
    int backend_op_embed;
    int backend_op_matmul;
    int backend_op_mlp;
    int backend_op_rms_norm;
    int backend_op_rope;
    int backend_op_attention;
    yvex_graph *graph;
    yvex_memory_plan *memory;
};

char *yvex_graph_strdup(const char *text);
void yvex_graph_value_clear(yvex_graph_value_info *value);
void yvex_graph_op_clear(yvex_graph_op_info *op);
void yvex_graph_missing_clear(yvex_graph_missing_required *missing);

int yvex_graph_add_value(yvex_graph *graph,
                         yvex_value_kind kind,
                         const char *name,
                         unsigned int rank,
                         const unsigned long long *dims,
                         yvex_dtype dtype,
                         yvex_residency residency,
                         const char *source_tensor_name,
                         unsigned int *out_id,
                         yvex_error *err);
int yvex_graph_add_op(yvex_graph *graph,
                      yvex_op_kind kind,
                      yvex_op_status status,
                      const char *name,
                      const unsigned int *inputs,
                      unsigned int input_count,
                      const unsigned int *outputs,
                      unsigned int output_count,
                      const char *reason,
                      yvex_error *err);
int yvex_graph_add_missing(yvex_graph *graph,
                           yvex_tensor_role role,
                           const char *reason,
                           yvex_error *err);

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index);



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


static void dump_shape(FILE *fp, const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    yvex_cli_out_writef(fp, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            yvex_cli_out_writef(fp, ",");
        }
        yvex_cli_out_writef(fp, "%llu", dims[i]);
    }
    yvex_cli_out_writef(fp, "]");
}

static void dump_edge_list(FILE *fp, const unsigned int *ids, unsigned int count)
{
    unsigned int i;

    yvex_cli_out_writef(fp, "[");
    if (!ids) {
        yvex_cli_out_writef(fp, "]");
        return;
    }
    for (i = 0; i < count; ++i) {
        if (i > 0) {
            yvex_cli_out_writef(fp, ",");
        }
        yvex_cli_out_writef(fp, "%u", ids[i]);
    }
    yvex_cli_out_writef(fp, "]");
}

int yvex_graph_dump(const yvex_graph *graph, FILE *fp, yvex_error *err)
{
    unsigned long long i;

    if (!graph || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_dump", "graph and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_cli_out_writef(fp, "graph status: %s\n", yvex_graph_status_name(graph->status));
    yvex_cli_out_writef(fp, "architecture: %s\n", graph->architecture ? graph->architecture : "unknown");
    yvex_cli_out_writef(fp, "model_name: %s\n", graph->model_name ? graph->model_name : "");
    yvex_cli_out_writef(fp, "values: %llu\n", graph->value_count);
    yvex_cli_out_writef(fp, "ops: %llu\n", graph->op_count);
    yvex_cli_out_writef(fp, "missing_required: %llu\n", graph->missing_count);
    yvex_cli_out_writef(fp, "\n");

    for (i = 0; i < graph->value_count; ++i) {
        const yvex_graph_value_info *value = &graph->values[i];
        yvex_cli_out_writef(fp, "value %u %s kind=%s shape=",
                value->id,
                value->name ? value->name : "",
                yvex_value_kind_name(value->kind));
        dump_shape(fp, value->dims, value->rank);
        yvex_cli_out_writef(fp, " dtype=%s residency=%s",
                yvex_dtype_name(value->dtype),
                yvex_residency_name(value->residency));
        if (value->source_tensor_name && value->source_tensor_name[0] != '\0') {
            yvex_cli_out_writef(fp, " source=%s", value->source_tensor_name);
        }
        yvex_cli_out_writef(fp, "\n");
    }
    if (graph->value_count > 0 || graph->op_count > 0 || graph->missing_count > 0) {
        yvex_cli_out_writef(fp, "\n");
    }

    for (i = 0; i < graph->op_count; ++i) {
        const yvex_graph_op_info *op = &graph->ops[i];
        const yvex_graph_op_edges *edges = yvex_graph_op_edges_at(graph, i);
        yvex_cli_out_writef(fp, "op %u %s status=%s inputs=",
                op->id,
                op->name ? op->name : yvex_op_kind_name(op->kind),
                yvex_op_status_name(op->status));
        dump_edge_list(fp, edges ? edges->input_ids : NULL, op->input_count);
        yvex_cli_out_writef(fp, " outputs=");
        dump_edge_list(fp, edges ? edges->output_ids : NULL, op->output_count);
        if (op->reason && op->reason[0] != '\0') {
            yvex_cli_out_writef(fp, " reason=\"%s\"", op->reason);
        }
        yvex_cli_out_writef(fp, "\n");
    }
    if (graph->op_count > 0 && graph->missing_count > 0) {
        yvex_cli_out_writef(fp, "\n");
    }

    for (i = 0; i < graph->missing_count; ++i) {
        const yvex_graph_missing_required *missing = &graph->missing[i];
        yvex_cli_out_writef(fp, "missing %s reason=\"%s\"\n",
                missing->role_name ? missing->role_name : yvex_tensor_role_name(missing->role),
                missing->reason ? missing->reason : "");
    }
    yvex_cli_out_writef(fp, "status: graph-%s\n", yvex_graph_status_name(graph->status));

    yvex_error_clear(err);
    return YVEX_OK;
}



static int add_checked(unsigned long long a,
                       unsigned long long b,
                       unsigned long long *out,
                       yvex_error *err)
{
    if (a > ULLONG_MAX - b) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_memory_plan_from_graph", "memory byte total overflow");
        return YVEX_ERR_BOUNDS;
    }
    *out = a + b;
    return YVEX_OK;
}

static int compute_activation_peak(const yvex_graph *graph,
                                   unsigned long long *out,
                                   yvex_error *err)
{
    unsigned long long peak = 0;
    unsigned long long i;

    for (i = 0; i < yvex_graph_value_count(graph); ++i) {
        const yvex_graph_value_info *value = yvex_graph_value_at(graph, i);
        unsigned long long elements;
        unsigned long long bytes;
        int rc;

        if (!value || value->kind != YVEX_VALUE_ACTIVATION) {
            continue;
        }
        rc = yvex_shape_product(value->dims, value->rank, &elements, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        rc = yvex_dtype_storage_bytes(value->dtype, elements, &bytes, err);
        if (rc == YVEX_ERR_UNSUPPORTED) {
            yvex_error_clear(err);
            continue;
        }
        if (rc != YVEX_OK) {
            return rc;
        }
        if (bytes > peak) {
            peak = bytes;
        }
    }

    *out = peak;
    return YVEX_OK;
}

int yvex_memory_plan_from_graph(yvex_memory_plan **out,
                                const yvex_graph *graph,
                                const yvex_tensor_table *tensors,
                                yvex_error *err)
{
    yvex_memory_plan *plan;
    unsigned long long i;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_from_graph", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!graph || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_from_graph",
                       "graph and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    plan = (yvex_memory_plan *)calloc(1, sizeof(*plan));
    if (!plan) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_memory_plan_from_graph",
                       "failed to allocate memory plan");
        return YVEX_ERR_NOMEM;
    }

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        if (!tensor) {
            continue;
        }
        if (tensor->storage_bytes == 0) {
            plan->summary.model_tensor_bytes_unknown_count += 1u;
        } else {
            rc = add_checked(plan->summary.model_tensor_bytes_known,
                             tensor->storage_bytes,
                             &plan->summary.model_tensor_bytes_known,
                             err);
            if (rc != YVEX_OK) {
                yvex_memory_plan_close(plan);
                return rc;
            }
        }
    }

    rc = compute_activation_peak(graph, &plan->summary.activation_peak_bytes, err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }

    rc = add_checked(plan->summary.model_tensor_bytes_known,
                     plan->summary.activation_peak_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }
    rc = add_checked(plan->summary.total_known_bytes,
                     plan->summary.kv_cache_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }
    rc = add_checked(plan->summary.total_known_bytes,
                     plan->summary.scratch_peak_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }

    switch (yvex_graph_status_of(graph)) {
    case YVEX_GRAPH_STATUS_BUILT:
        plan->status = YVEX_MEMORY_PLAN_ESTIMATED;
        break;
    case YVEX_GRAPH_STATUS_PARTIAL:
        plan->status = YVEX_MEMORY_PLAN_PARTIAL;
        break;
    case YVEX_GRAPH_STATUS_UNSUPPORTED:
    case YVEX_GRAPH_STATUS_INVALID:
        plan->status = YVEX_MEMORY_PLAN_UNSUPPORTED;
        break;
    case YVEX_GRAPH_STATUS_EMPTY:
    default:
        plan->status = YVEX_MEMORY_PLAN_EMPTY;
        break;
    }

    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_memory_plan_close(yvex_memory_plan *plan)
{
    free(plan);
}

yvex_memory_plan_status yvex_memory_plan_status_of(const yvex_memory_plan *plan)
{
    return plan ? plan->status : YVEX_MEMORY_PLAN_EMPTY;
}

const char *yvex_memory_plan_status_name(yvex_memory_plan_status status)
{
    switch (status) {
    case YVEX_MEMORY_PLAN_EMPTY: return "empty";
    case YVEX_MEMORY_PLAN_ESTIMATED: return "estimated";
    case YVEX_MEMORY_PLAN_PARTIAL: return "partial";
    case YVEX_MEMORY_PLAN_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

int yvex_memory_plan_get_summary(const yvex_memory_plan *plan,
                                 yvex_memory_plan_summary *out,
                                 yvex_error *err)
{
    if (!plan || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_get_summary",
                       "plan and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = plan->summary;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_memory_plan_dump(const yvex_memory_plan *plan,
                          FILE *fp,
                          yvex_error *err)
{
    if (!plan || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_dump",
                       "plan and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_cli_out_writef(fp, "memory:\n");
    yvex_cli_out_writef(fp, "  model_tensor_bytes_known: %llu\n", plan->summary.model_tensor_bytes_known);
    yvex_cli_out_writef(fp, "  model_tensor_bytes_unknown_count: %llu\n",
            plan->summary.model_tensor_bytes_unknown_count);
    yvex_cli_out_writef(fp, "  activation_peak_bytes: %llu\n", plan->summary.activation_peak_bytes);
    yvex_cli_out_writef(fp, "  kv_cache_bytes: %llu\n", plan->summary.kv_cache_bytes);
    yvex_cli_out_writef(fp, "  scratch_peak_bytes: %llu\n", plan->summary.scratch_peak_bytes);
    yvex_cli_out_writef(fp, "  total_known_bytes: %llu\n", plan->summary.total_known_bytes);

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



static int backend_allowed(const char *name)
{
    return strcmp(name, "cpu") == 0 || strcmp(name, "none") == 0 || strcmp(name, "cuda") == 0;
}

static int graph_backend_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

static yvex_backend_kind graph_backend_kind_from_name(const char *name)
{
    return strcmp(name, "cuda") == 0 ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU;
}

static int fill_backend_status(yvex_plan *plan, const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options backend_options;
    int rc;

    if (strcmp(backend_name, "cpu") == 0 || strcmp(backend_name, "cuda") == 0) {
        memset(&backend_options, 0, sizeof(backend_options));
        backend_options.kind = graph_backend_kind_from_name(backend_name);
        rc = yvex_backend_open(&backend, &backend_options, err);
        if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
            plan->backend_status = yvex_graph_strdup("unavailable");
            yvex_error_clear(err);
            return plan->backend_status ? YVEX_OK : YVEX_ERR_NOMEM;
        }
        if (rc != YVEX_OK) {
            return rc;
        }
        plan->backend_status = yvex_graph_strdup("available");
        plan->backend_tensor_alloc = yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC);
        plan->backend_tensor_read_write = yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE);
        plan->backend_op_embed = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED);
        plan->backend_op_matmul = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL);
        plan->backend_op_mlp = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MLP);
        plan->backend_op_rms_norm = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM);
        plan->backend_op_rope = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE);
        plan->backend_op_attention = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION);
        yvex_backend_close(backend);
    } else {
        plan->backend_status = yvex_graph_strdup("not-selected");
    }

    if (!plan->backend_status) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to copy backend status");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_plan_create(yvex_plan **out,
                     const yvex_model_descriptor *model,
                     const yvex_tensor_table *tensors,
                     const yvex_plan_options *options,
                     yvex_error *err)
{
    yvex_plan *plan;
    yvex_graph_build_options graph_options;
    const char *backend_name = "cpu";
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_create", "model and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&graph_options, 0, sizeof(graph_options));
    graph_options.sequence_length = 1;
    graph_options.context_length = yvex_model_context_length(model) > 0
                                       ? yvex_model_context_length(model)
                                       : 1;
    graph_options.include_prefill_path = 1;

    if (options) {
        if (options->sequence_length > 0) {
            graph_options.sequence_length = options->sequence_length;
        }
        if (options->context_length > 0) {
            graph_options.context_length = options->context_length;
        }
        if (options->backend_name) {
            backend_name = options->backend_name;
        }
    }

    if (!backend_allowed(backend_name)) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_plan_create",
                        "backend label unsupported in graph planner: %s", backend_name);
        return YVEX_ERR_UNSUPPORTED;
    }

    plan = (yvex_plan *)calloc(1, sizeof(*plan));
    if (!plan) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to allocate plan");
        return YVEX_ERR_NOMEM;
    }
    plan->backend_name = yvex_graph_strdup(backend_name);
    if (!plan->backend_name) {
        yvex_plan_close(plan);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to copy backend label");
        return YVEX_ERR_NOMEM;
    }
    rc = fill_backend_status(plan, backend_name, err);
    if (rc != YVEX_OK) {
        yvex_plan_close(plan);
        return rc;
    }

    rc = yvex_graph_build_for_model(&plan->graph, model, tensors, &graph_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_memory_plan_from_graph(&plan->memory, plan->graph, tensors, err);
    }
    if (rc != YVEX_OK) {
        yvex_plan_close(plan);
        return rc;
    }

    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_plan_close(yvex_plan *plan)
{
    if (!plan) {
        return;
    }
    free(plan->backend_name);
    free(plan->backend_status);
    yvex_memory_plan_close(plan->memory);
    yvex_graph_close(plan->graph);
    free(plan);
}

const yvex_graph *yvex_plan_graph(const yvex_plan *plan)
{
    return plan ? plan->graph : NULL;
}

const yvex_memory_plan *yvex_plan_memory(const yvex_plan *plan)
{
    return plan ? plan->memory : NULL;
}

int yvex_plan_dump(const yvex_plan *plan, FILE *fp, yvex_error *err)
{
    if (!plan || !fp || !plan->graph || !plan->memory) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_dump", "complete plan and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_cli_out_writef(fp, "plan status: %s\n", yvex_memory_plan_status_name(yvex_memory_plan_status_of(plan->memory)));
    yvex_cli_out_writef(fp, "backend: %s\n", plan->backend_name ? plan->backend_name : "");
    yvex_cli_out_writef(fp, "backend_status: %s\n", plan->backend_status ? plan->backend_status : "unknown");
    if (plan->backend_status && strcmp(plan->backend_status, "available") == 0) {
        yvex_cli_out_writef(fp, "backend_capabilities:\n");
        yvex_cli_out_writef(fp, "  tensor_alloc: %s\n", plan->backend_tensor_alloc ? "yes" : "no");
        yvex_cli_out_writef(fp, "  tensor_read_write: %s\n", plan->backend_tensor_read_write ? "yes" : "no");
        yvex_cli_out_writef(fp, "  op_embed: %s\n", plan->backend_op_embed ? "yes" : "no");
        yvex_cli_out_writef(fp, "  op_matmul: %s\n", plan->backend_op_matmul ? "yes" : "no");
        yvex_cli_out_writef(fp, "  op_mlp: %s\n", plan->backend_op_mlp ? "yes" : "no");
        yvex_cli_out_writef(fp, "  op_rms_norm: %s\n", plan->backend_op_rms_norm ? "yes" : "no");
        yvex_cli_out_writef(fp, "  op_rope: %s\n", plan->backend_op_rope ? "yes" : "no");
        yvex_cli_out_writef(fp, "  op_attention: %s\n", plan->backend_op_attention ? "yes" : "no");
    }
    yvex_cli_out_writef(fp, "architecture: %s\n", plan->graph->architecture ? plan->graph->architecture : "unknown");
    yvex_cli_out_writef(fp, "model_name: %s\n", plan->graph->model_name ? plan->graph->model_name : "");
    yvex_cli_out_writef(fp, "graph_status: %s\n", yvex_graph_status_name(plan->graph->status));
    yvex_cli_out_writef(fp, "ops: %llu\n", plan->graph->op_count);
    yvex_cli_out_writef(fp, "missing_required: %llu\n", plan->graph->missing_count);
    yvex_cli_out_writef(fp, "\n");

    if (yvex_memory_plan_dump(plan->memory, fp, err) != YVEX_OK) {
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_cli_out_writef(fp, "\n");
    yvex_cli_out_writef(fp, "execution_ready: false\n");
    if (plan->backend_status && strcmp(plan->backend_status, "unavailable") == 0) {
        yvex_cli_out_writef(fp, "reason: CUDA runtime/device not available\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_PARTIAL) {
        yvex_cli_out_writef(fp, "reason: graph partial; missing output_norm, output_head; backend lacks full graph ops\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_BUILT) {
        yvex_cli_out_writef(fp, "reason: session execution not implemented\n");
    } else {
        yvex_cli_out_writef(fp, "reason: graph unsupported; backend lacks full graph ops\n");
    }
    yvex_cli_out_writef(fp, "status: plan-only\n");

    yvex_error_clear(err);
    return YVEX_OK;
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

/* Graph guard and selected-artifact preflight helpers. */

static int cli_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void init_graph_guard_report(yvex_cli_graph_guard_report *report,
                                    const char *graph_kind,
                                    int needs_slice_range,
                                    const yvex_model_ref *model_ref)
{
    memset(report, 0, sizeof(*report));
    report->guard_status = "fail";
    report->phase = "preflight";
    report->graph_kind = graph_kind ? graph_kind : "unknown";
    report->integrity_status = "unchecked";
    report->identity_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->metadata_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->shape_status = "unchecked";
    report->range_status = "unchecked";
    report->slice_range_status = needs_slice_range ? "unchecked" : "not-needed";
    report->backend_status = "not-opened";
    report->backend_op_status = "unchecked";
    report->cleanup_status = "not-needed";
}

static const yvex_tensor_info *cli_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight",
        "blk.0.attention_norm.weight",
        "blk.0.input_layernorm.weight",
        "model.layers.0.input_layernorm.weight",
    };
    unsigned int i;
    unsigned long long count;
    unsigned long long index;

    if (!tensors) {
        return NULL;
    }
    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, preferred[i]);
        if (tensor) {
            return tensor;
        }
    }
    count = yvex_tensor_table_count(tensors);
    for (index = 0; index < count; ++index) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
        if (tensor && tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM) {
            return tensor;
        }
    }
    return NULL;
}

static int cli_has_rmsnorm_epsilon(const yvex_gguf *gguf)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon",
        "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, keys[i]);
        double epsilon = 0.0;
        if (value && yvex_gguf_value_as_f64(value, &epsilon) == YVEX_OK && epsilon > 0.0) {
            return 1;
        }
    }
    return 0;
}

void print_graph_guard_report(const yvex_cli_graph_guard_report *report)
{
    yvex_cli_out_writef(stdout, "graph_integrity_guard: %s\n", report->guard_status ? report->guard_status : "fail");
    yvex_cli_out_writef(stdout, "graph_execution_phase: %s\n", report->phase ? report->phase : "preflight");
    yvex_cli_out_writef(stdout, "graph_kind: %s\n", report->graph_kind ? report->graph_kind : "unknown");
    yvex_cli_out_writef(stdout, "integrity_status: %s\n", report->integrity_status ? report->integrity_status : "unchecked");
    yvex_cli_out_writef(stdout, "identity_status: %s\n", report->identity_status ? report->identity_status : "unregistered");
    yvex_cli_out_writef(stdout, "metadata_status: %s\n", report->metadata_status ? report->metadata_status : "unregistered");
    yvex_cli_out_writef(stdout, "shape_status: %s\n", report->shape_status ? report->shape_status : "unchecked");
    yvex_cli_out_writef(stdout, "range_status: %s\n", report->range_status ? report->range_status : "unchecked");
    yvex_cli_out_writef(stdout, "slice_range_status: %s\n", report->slice_range_status ? report->slice_range_status : "unchecked");
    yvex_cli_out_writef(stdout, "backend_status: %s\n", report->backend_status ? report->backend_status : "not-opened");
    yvex_cli_out_writef(stdout, "backend_op_status: %s\n", report->backend_op_status ? report->backend_op_status : "unchecked");
    yvex_cli_out_writef(stdout, "dispatch_attempted: %s\n", report->dispatch_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "reference_read_attempted: %s\n", report->reference_read_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_allocation_attempted: %s\n", report->output_allocation_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n", report->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n", report->cleanup_status ? report->cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "output_bytes_planned: %llu\n", report->output_bytes_planned);
    yvex_cli_out_writef(stdout, "output_bytes_allocated: %llu\n", report->output_bytes_allocated);
    yvex_cli_out_writef(stdout, "reference_bytes_planned: %llu\n", report->reference_bytes_planned);
}

static void init_standalone_graph_guard(yvex_cli_graph_guard_report *guard,
                                        const char *graph_kind,
                                        const char *slice_range_status)
{
    init_graph_guard_report(guard, graph_kind, 0, NULL);
    guard->integrity_status = "not-applicable";
    guard->identity_status = "unregistered";
    guard->metadata_status = "unregistered";
    guard->shape_status = "unchecked";
    guard->range_status = "not-applicable";
    guard->slice_range_status = slice_range_status ? slice_range_status : "not-needed";
}

static void graph_guard_mark_cleanup(yvex_cli_graph_guard_report *guard,
                                     const char *phase,
                                     int attempted)
{
    guard->phase = phase ? phase : "output";
    guard->cleanup_attempted = attempted ? 1 : 0;
    guard->cleanup_status = attempted ? "pass" : "not-needed";
}

/* Deterministic reference helpers for graph proof commands. */

static unsigned long long cli_checksum_bytes(const void *data, unsigned long long len)
{
    const unsigned char *bytes = (const unsigned char *)data;
    unsigned long long checksum = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; i < len; ++i) {
        checksum ^= (unsigned long long)bytes[i];
        checksum *= 1099511628211ull;
    }
    return checksum;
}

static float cli_abs_float(float x)
{
    return x < 0.0f ? -x : x;
}

static double cli_abs_double(double x)
{
    return x < 0.0 ? -x : x;
}

static double cli_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

static double cli_exp_double(double x)
{
    const double ln2 = 0.69314718055994530942;
    double term;
    double sum;
    int n = 0;
    unsigned int i;

    if (x < -60.0) {
        return 0.0;
    }
    if (x > 60.0) {
        x = 60.0;
    }
    while (x > 0.5) {
        x -= ln2;
        ++n;
    }
    while (x < -0.5) {
        x += ln2;
        --n;
    }
    term = 1.0;
    sum = 1.0;
    for (i = 1u; i <= 18u; ++i) {
        term *= x / (double)i;
        sum += term;
    }
    while (n > 0) {
        sum *= 2.0;
        --n;
    }
    while (n < 0) {
        sum *= 0.5;
        ++n;
    }
    return sum;
}

static double cli_nth_root_double(double x, unsigned long long n)
{
    double lo = 1.0;
    double hi = x > 1.0 ? x : 1.0;
    unsigned int iter;

    if (x <= 0.0 || n == 0) {
        return 0.0;
    }
    if (n == 1) {
        return x;
    }
    for (iter = 0; iter < 96u; ++iter) {
        double mid = 0.5 * (lo + hi);
        double acc = 1.0;
        unsigned long long i;

        for (i = 0; i < n; ++i) {
            acc *= mid;
            if (acc > x) {
                break;
            }
        }
        if (acc > x) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

static double cli_wrap_radians(double x)
{
    const double two_pi = 6.28318530717958647692;

    while (x > 3.14159265358979323846) {
        x -= two_pi;
    }
    while (x < -3.14159265358979323846) {
        x += two_pi;
    }
    return x;
}

static void cli_sincos_double(double x, double *sine, double *cosine)
{
    double x2;
    double s;
    double c;

    x = cli_wrap_radians(x);
    x2 = x * x;
    s = x * (1.0 -
             (x2 / 6.0) +
             ((x2 * x2) / 120.0) -
             ((x2 * x2 * x2) / 5040.0) +
             ((x2 * x2 * x2 * x2) / 362880.0));
    c = 1.0 -
        (x2 / 2.0) +
        ((x2 * x2) / 24.0) -
        ((x2 * x2 * x2) / 720.0) +
        ((x2 * x2 * x2 * x2) / 40320.0);
    if (cli_abs_double(s) < 0.000000000001) {
        s = 0.0;
    }
    if (cli_abs_double(c) < 0.000000000001) {
        c = 0.0;
    }
    if (sine) {
        *sine = s;
    }
    if (cosine) {
        *cosine = c;
    }
}

static void cli_rope_fill_input(float *values, unsigned long long head_dim)
{
    unsigned long long i;

    for (i = 0; i < head_dim; ++i) {
        values[i] = (float)((double)(i + 1ull) * 0.25);
    }
}

static void cli_rope_reference(const float *input,
                               unsigned long long head_dim,
                               unsigned long long position,
                               float rope_base,
                               float *out)
{
    unsigned long long pair_count = head_dim / 2ull;
    unsigned long long pair;
    double inverse_root = 1.0 / cli_nth_root_double((double)rope_base, pair_count);
    double frequency = 1.0;

    for (pair = 0; pair < pair_count; ++pair) {
        unsigned long long even_index = pair * 2ull;
        unsigned long long odd_index = even_index + 1ull;
        double sine;
        double cosine;
        double even = (double)input[even_index];
        double odd = (double)input[odd_index];
        double angle = (double)position * frequency;

        cli_sincos_double(angle, &sine, &cosine);
        out[even_index] = (float)((even * cosine) - (odd * sine));
        out[odd_index] = (float)((even * sine) + (odd * cosine));
        frequency *= inverse_root;
    }
}

static void cli_attention_fill_inputs(float *query,
                                      float *keys,
                                      float *values,
                                      unsigned long long seq_len,
                                      unsigned long long head_dim)
{
    unsigned long long i;
    unsigned long long d;

    for (d = 0; d < head_dim; ++d) {
        query[d] = (float)(0.02 + ((double)(d + 1ull) * 0.01));
    }
    for (i = 0; i < seq_len; ++i) {
        for (d = 0; d < head_dim; ++d) {
            keys[(i * head_dim) + d] =
                (float)(0.03 + ((double)(i + 1ull) * 0.04) + ((double)(d + 1ull) * 0.002));
            values[(i * head_dim) + d] =
                (float)(0.10 + ((double)(i + 1ull) * 0.08) + ((double)(d + 1ull) * 0.01));
        }
    }
}

static void cli_attention_reference(const float *query,
                                    const float *keys,
                                    const float *values,
                                    unsigned long long seq_len,
                                    unsigned long long position,
                                    unsigned long long head_dim,
                                    float scale,
                                    int causal,
                                    float *score_scratch,
                                    float *probability_scratch,
                                    float *out)
{
    unsigned long long visible_count = causal ? position + 1ull : seq_len;
    unsigned long long i;
    unsigned long long d;
    double max_score = 0.0;
    double sum_exp = 0.0;

    for (i = 0; i < seq_len; ++i) {
        double score = 0.0;
        if (causal && i > position) {
            score_scratch[i] = 0.0f;
            probability_scratch[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += (double)query[d] * (double)keys[(i * head_dim) + d];
        }
        score *= (double)scale;
        score_scratch[i] = (float)score;
        if (i == 0ull || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        double e = cli_exp_double((double)score_scratch[i] - max_score);
        probability_scratch[i] = (float)e;
        sum_exp += e;
    }
    if (sum_exp <= 0.0) {
        for (d = 0; d < head_dim; ++d) {
            out[d] = 0.0f;
        }
        return;
    }
    for (i = 0; i < visible_count; ++i) {
        probability_scratch[i] = (float)((double)probability_scratch[i] / sum_exp);
    }
    for (d = 0; d < head_dim; ++d) {
        double value = 0.0;
        for (i = 0; i < visible_count; ++i) {
            value += (double)probability_scratch[i] * (double)values[(i * head_dim) + d];
        }
        out[d] = (float)value;
    }
}

static void cli_matmul_fill_inputs(float *input,
                                   float *weight,
                                   unsigned long long m,
                                   unsigned long long k,
                                   unsigned long long n)
{
    unsigned long long row;
    unsigned long long inner;
    unsigned long long col;

    for (row = 0; row < m; ++row) {
        for (inner = 0; inner < k; ++inner) {
            input[(row * k) + inner] =
                (float)(0.05 + ((double)(row + 1ull) * 0.02) +
                        ((double)(inner + 1ull) * 0.01));
        }
    }
    for (inner = 0; inner < k; ++inner) {
        for (col = 0; col < n; ++col) {
            weight[(inner * n) + col] =
                (float)(0.03 + ((double)(inner + 1ull) * 0.004) +
                        ((double)(col + 1ull) * 0.002));
        }
    }
}

static void cli_matmul_reference(const float *input,
                                 const float *weight,
                                 unsigned long long m,
                                 unsigned long long k,
                                 unsigned long long n,
                                 float *out)
{
    unsigned long long row;
    unsigned long long col;

    for (row = 0; row < m; ++row) {
        for (col = 0; col < n; ++col) {
            double sum = 0.0;
            unsigned long long inner;
            for (inner = 0; inner < k; ++inner) {
                sum += (double)input[(row * k) + inner] *
                       (double)weight[(inner * n) + col];
            }
            out[(row * n) + col] = (float)sum;
        }
    }
}

static double cli_silu_double(double x)
{
    return x / (1.0 + cli_exp_double(-x));
}

static void cli_mlp_fill_inputs(float *input,
                                float *gate_weight,
                                float *up_weight,
                                float *down_weight,
                                unsigned long long batch,
                                unsigned long long hidden_dim,
                                unsigned long long ffn_dim,
                                unsigned long long expert_count,
                                int routed)
{
    unsigned long long row;
    unsigned long long h;
    unsigned long long j;
    unsigned long long e;
    unsigned long long actual_experts = routed ? expert_count : 1ull;

    for (row = 0; row < batch; ++row) {
        for (h = 0; h < hidden_dim; ++h) {
            input[(row * hidden_dim) + h] =
                (float)(0.04 + ((double)(row + 1ull) * 0.03) +
                        ((double)(h + 1ull) * 0.008));
        }
    }
    for (e = 0; e < actual_experts; ++e) {
        unsigned long long up_base = e * hidden_dim * ffn_dim;
        unsigned long long down_base = e * ffn_dim * hidden_dim;
        for (h = 0; h < hidden_dim; ++h) {
            for (j = 0; j < ffn_dim; ++j) {
                gate_weight[up_base + (h * ffn_dim) + j] =
                    (float)(0.015 + ((double)(e + 1ull) * 0.004) +
                            ((double)(h + 1ull) * 0.0015) +
                            ((double)(j + 1ull) * 0.0007));
                up_weight[up_base + (h * ffn_dim) + j] =
                    (float)(0.02 + ((double)(e + 1ull) * 0.005) +
                            ((double)(h + 1ull) * 0.0012) +
                            ((double)(j + 1ull) * 0.0009));
            }
        }
        for (j = 0; j < ffn_dim; ++j) {
            for (h = 0; h < hidden_dim; ++h) {
                down_weight[down_base + (j * hidden_dim) + h] =
                    (float)(0.012 + ((double)(e + 1ull) * 0.003) +
                            ((double)(j + 1ull) * 0.0008) +
                            ((double)(h + 1ull) * 0.0011));
            }
        }
    }
}

static void cli_mlp_reference(const float *input,
                              const float *gate_weight,
                              const float *up_weight,
                              const float *down_weight,
                              unsigned long long batch,
                              unsigned long long hidden_dim,
                              unsigned long long ffn_dim,
                              unsigned long long expert_id,
                              int routed,
                              float *intermediate,
                              float *out)
{
    unsigned long long gate_offset = routed ? expert_id * hidden_dim * ffn_dim : 0ull;
    unsigned long long up_offset = gate_offset;
    unsigned long long down_offset = routed ? expert_id * ffn_dim * hidden_dim : 0ull;
    unsigned long long row;
    unsigned long long h;
    unsigned long long j;

    for (row = 0; row < batch; ++row) {
        for (j = 0; j < ffn_dim; ++j) {
            double gate_sum = 0.0;
            double up_sum = 0.0;
            for (h = 0; h < hidden_dim; ++h) {
                double x = (double)input[(row * hidden_dim) + h];
                gate_sum += x * (double)gate_weight[gate_offset + (h * ffn_dim) + j];
                up_sum += x * (double)up_weight[up_offset + (h * ffn_dim) + j];
            }
            intermediate[(row * ffn_dim) + j] =
                (float)(cli_silu_double(gate_sum) * up_sum);
        }
        for (h = 0; h < hidden_dim; ++h) {
            double sum = 0.0;
            for (j = 0; j < ffn_dim; ++j) {
                sum += (double)intermediate[(row * ffn_dim) + j] *
                       (double)down_weight[down_offset + (j * hidden_dim) + h];
            }
            out[(row * hidden_dim) + h] = (float)sum;
        }
    }
}

static float cli_max_abs_diff_f32(const float *a,
                                  const float *b,
                                  unsigned long long count)
{
    unsigned long long i;
    float max_diff = 0.0f;

    for (i = 0; i < count; ++i) {
        float diff = cli_abs_float(a[i] - b[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
}

static void cli_print_float_values(const char *name,
                                   const float *values,
                                   unsigned long long count)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "%s:", name);
    for (i = 0; i < count; ++i) {
        yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)values[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
}

static void print_no_generation_readiness_fields(void)
{
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
}

static void print_rope_readiness_fields(void)
{
    yvex_cli_out_writef(stdout, "attention_ready: false\n");
    yvex_cli_out_writef(stdout, "transformer_block_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static void print_attention_readiness_fields(int primitive_executed)
{
    yvex_cli_out_writef(stdout, "attention_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "qkv_projection_ready: false\n");
    yvex_cli_out_writef(stdout, "transformer_block_ready: false\n");
    yvex_cli_out_writef(stdout, "full_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static void print_matmul_readiness_fields(int primitive_executed)
{
    yvex_cli_out_writef(stdout, "matmul_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "qkv_projection_ready: false\n");
    yvex_cli_out_writef(stdout, "transformer_block_ready: false\n");
    yvex_cli_out_writef(stdout, "full_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

static void print_mlp_readiness_fields(int primitive_executed)
{
    yvex_cli_out_writef(stdout, "mlp_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "router_logits_ready: false\n");
    yvex_cli_out_writef(stdout, "top_k_routing_ready: false\n");
    yvex_cli_out_writef(stdout, "transformer_block_ready: false\n");
    yvex_cli_out_writef(stdout, "full_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    print_no_generation_readiness_fields();
}

/* Controlled transformer-block fixture proof. */

typedef struct {
    yvex_device_tensor *input_states;
    yvex_device_tensor *attn_norm_weight;
    yvex_device_tensor *normalized_input;
    yvex_device_tensor *q_weight;
    yvex_device_tensor *k_weight;
    yvex_device_tensor *v_weight;
    yvex_device_tensor *o_weight;
    yvex_device_tensor *q;
    yvex_device_tensor *k;
    yvex_device_tensor *v;
    yvex_device_tensor *row_input;
    yvex_device_tensor *row_output;
    yvex_device_tensor *rope_q;
    yvex_device_tensor *rope_k;
    yvex_device_tensor *attention_output;
    yvex_device_tensor *attention_scores;
    yvex_device_tensor *attention_probabilities;
    yvex_device_tensor *attention_output_matrix;
    yvex_device_tensor *projected_attention;
    yvex_device_tensor *residual_after_attention;
    yvex_device_tensor *post_attn_norm_weight;
    yvex_device_tensor *post_attn_norm;
    yvex_device_tensor *mlp_gate_weight;
    yvex_device_tensor *mlp_up_weight;
    yvex_device_tensor *mlp_down_weight;
    yvex_device_tensor *mlp_intermediate;
    yvex_device_tensor *mlp_output;
    yvex_device_tensor *block_output;
} yvex_cli_block_tensors;

static void block_free_tensors(yvex_backend *backend, yvex_cli_block_tensors *t)
{
    if (!backend || !t) {
        return;
    }
    yvex_backend_tensor_free(backend, t->block_output);
    yvex_backend_tensor_free(backend, t->mlp_output);
    yvex_backend_tensor_free(backend, t->mlp_intermediate);
    yvex_backend_tensor_free(backend, t->mlp_down_weight);
    yvex_backend_tensor_free(backend, t->mlp_up_weight);
    yvex_backend_tensor_free(backend, t->mlp_gate_weight);
    yvex_backend_tensor_free(backend, t->post_attn_norm);
    yvex_backend_tensor_free(backend, t->post_attn_norm_weight);
    yvex_backend_tensor_free(backend, t->residual_after_attention);
    yvex_backend_tensor_free(backend, t->projected_attention);
    yvex_backend_tensor_free(backend, t->attention_output_matrix);
    yvex_backend_tensor_free(backend, t->attention_probabilities);
    yvex_backend_tensor_free(backend, t->attention_scores);
    yvex_backend_tensor_free(backend, t->attention_output);
    yvex_backend_tensor_free(backend, t->rope_k);
    yvex_backend_tensor_free(backend, t->rope_q);
    yvex_backend_tensor_free(backend, t->row_output);
    yvex_backend_tensor_free(backend, t->row_input);
    yvex_backend_tensor_free(backend, t->v);
    yvex_backend_tensor_free(backend, t->k);
    yvex_backend_tensor_free(backend, t->q);
    yvex_backend_tensor_free(backend, t->o_weight);
    yvex_backend_tensor_free(backend, t->v_weight);
    yvex_backend_tensor_free(backend, t->k_weight);
    yvex_backend_tensor_free(backend, t->q_weight);
    yvex_backend_tensor_free(backend, t->normalized_input);
    yvex_backend_tensor_free(backend, t->attn_norm_weight);
    yvex_backend_tensor_free(backend, t->input_states);
}

static int block_alloc_f32(yvex_backend *backend,
                           const char *name,
                           unsigned int rank,
                           unsigned long long d0,
                           unsigned long long d1,
                           yvex_device_tensor **out,
                           yvex_error *err)
{
    yvex_backend_tensor_desc desc;
    unsigned long long elements;

    memset(&desc, 0, sizeof(desc));
    desc.name = name;
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = rank;
    desc.dims[0] = d0;
    desc.dims[1] = rank > 1 ? d1 : 0ull;
    elements = rank > 1 ? d0 * d1 : d0;
    desc.bytes = elements * (unsigned long long)sizeof(float);
    return yvex_backend_tensor_alloc(backend, &desc, out, err);
}

static void block_fill_input(float *input,
                             unsigned long long seq_len,
                             unsigned long long hidden_dim)
{
    unsigned long long row;
    unsigned long long col;

    for (row = 0; row < seq_len; ++row) {
        for (col = 0; col < hidden_dim; ++col) {
            input[(row * hidden_dim) + col] =
                (float)(0.06 + ((double)(row + 1ull) * 0.017) +
                        ((double)(col + 1ull) * 0.004));
        }
    }
}

static void block_fill_norm_weight(float *weight,
                                   unsigned long long hidden_dim,
                                   double base)
{
    unsigned long long i;

    for (i = 0; i < hidden_dim; ++i) {
        weight[i] = (float)(base + ((double)(i + 1ull) * 0.002));
    }
}

static void block_fill_projection_weight(float *weight,
                                         unsigned long long hidden_dim,
                                         double base)
{
    unsigned long long row;
    unsigned long long col;

    for (row = 0; row < hidden_dim; ++row) {
        for (col = 0; col < hidden_dim; ++col) {
            weight[(row * hidden_dim) + col] =
                (float)(base + ((double)(row + 1ull) * 0.0013) +
                        ((double)(col + 1ull) * 0.0009));
        }
    }
}

static void block_fill_mlp_weights(float *gate,
                                   float *up,
                                   float *down,
                                   unsigned long long hidden_dim,
                                   unsigned long long ffn_dim)
{
    unsigned long long h;
    unsigned long long f;

    for (h = 0; h < hidden_dim; ++h) {
        for (f = 0; f < ffn_dim; ++f) {
            gate[(h * ffn_dim) + f] =
                (float)(0.009 + ((double)(h + 1ull) * 0.0008) +
                        ((double)(f + 1ull) * 0.0005));
            up[(h * ffn_dim) + f] =
                (float)(0.011 + ((double)(h + 1ull) * 0.0007) +
                        ((double)(f + 1ull) * 0.0006));
        }
    }
    for (f = 0; f < ffn_dim; ++f) {
        for (h = 0; h < hidden_dim; ++h) {
            down[(f * hidden_dim) + h] =
                (float)(0.010 + ((double)(f + 1ull) * 0.0004) +
                        ((double)(h + 1ull) * 0.0007));
        }
    }
}

static void block_rms_norm_reference(const float *input,
                                     const float *weight,
                                     unsigned long long hidden_dim,
                                     float epsilon,
                                     float *out)
{
    unsigned long long i;
    double sum_squares = 0.0;
    double inv_rms;

    for (i = 0; i < hidden_dim; ++i) {
        sum_squares += (double)input[i] * (double)input[i];
    }
    inv_rms = 1.0 / cli_sqrt_double((sum_squares / (double)hidden_dim) + (double)epsilon);
    for (i = 0; i < hidden_dim; ++i) {
        out[i] = (float)((double)input[i] * inv_rms * (double)weight[i]);
    }
}

static void block_residual_add(const float *a,
                               const float *b,
                               unsigned long long hidden_dim,
                               float *out)
{
    unsigned long long i;

    for (i = 0; i < hidden_dim; ++i) {
        out[i] = a[i] + b[i];
    }
}

static int block_reference(const float *input,
                           const float *attn_norm_weight,
                           const float *q_weight,
                           const float *k_weight,
                           const float *v_weight,
                           const float *o_weight,
                           const float *post_attn_norm_weight,
                           const float *mlp_gate_weight,
                           const float *mlp_up_weight,
                           const float *mlp_down_weight,
                           unsigned long long seq_len,
                           unsigned long long position,
                           unsigned long long hidden_dim,
                           unsigned long long ffn_dim,
                           int causal,
                           float *reference_out,
                           yvex_error *err)
{
    const float epsilon = 0.000001f;
    const float rope_base = 10000.0f;
    float scale = (float)(1.0 / cli_sqrt_double((double)hidden_dim));
    float *normalized = NULL;
    float *q = NULL;
    float *k = NULL;
    float *v = NULL;
    float *rope_q = NULL;
    float *rope_k = NULL;
    float *scores = NULL;
    float *probabilities = NULL;
    float *attention = NULL;
    float *projected = NULL;
    float *residual_attn = NULL;
    float *post_norm = NULL;
    float *mlp_intermediate = NULL;
    float *mlp_out = NULL;
    unsigned long long seq_hidden = seq_len * hidden_dim;
    unsigned long long row;
    int rc = YVEX_OK;

    normalized = (float *)calloc((size_t)seq_hidden, sizeof(float));
    q = (float *)calloc((size_t)seq_hidden, sizeof(float));
    k = (float *)calloc((size_t)seq_hidden, sizeof(float));
    v = (float *)calloc((size_t)seq_hidden, sizeof(float));
    rope_q = (float *)calloc((size_t)hidden_dim, sizeof(float));
    rope_k = (float *)calloc((size_t)seq_hidden, sizeof(float));
    scores = (float *)calloc((size_t)seq_len, sizeof(float));
    probabilities = (float *)calloc((size_t)seq_len, sizeof(float));
    attention = (float *)calloc((size_t)hidden_dim, sizeof(float));
    projected = (float *)calloc((size_t)hidden_dim, sizeof(float));
    residual_attn = (float *)calloc((size_t)hidden_dim, sizeof(float));
    post_norm = (float *)calloc((size_t)hidden_dim, sizeof(float));
    mlp_intermediate = (float *)calloc((size_t)ffn_dim, sizeof(float));
    mlp_out = (float *)calloc((size_t)hidden_dim, sizeof(float));
    if (!normalized || !q || !k || !v || !rope_q || !rope_k || !scores ||
        !probabilities || !attention || !projected || !residual_attn ||
        !post_norm || !mlp_intermediate || !mlp_out) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex graph block",
                       "failed to allocate controlled block reference scratch");
        rc = YVEX_ERR_NOMEM;
        goto done;
    }

    for (row = 0; row < seq_len; ++row) {
        block_rms_norm_reference(input + (row * hidden_dim), attn_norm_weight,
                                 hidden_dim, epsilon,
                                 normalized + (row * hidden_dim));
    }
    cli_matmul_reference(normalized, q_weight, seq_len, hidden_dim, hidden_dim, q);
    cli_matmul_reference(normalized, k_weight, seq_len, hidden_dim, hidden_dim, k);
    cli_matmul_reference(normalized, v_weight, seq_len, hidden_dim, hidden_dim, v);
    cli_rope_reference(q + (position * hidden_dim), hidden_dim, position, rope_base, rope_q);
    for (row = 0; row < seq_len; ++row) {
        cli_rope_reference(k + (row * hidden_dim), hidden_dim, row, rope_base,
                           rope_k + (row * hidden_dim));
    }
    cli_attention_reference(rope_q, rope_k, v, seq_len, position, hidden_dim,
                            scale, causal, scores, probabilities, attention);
    cli_matmul_reference(attention, o_weight, 1ull, hidden_dim, hidden_dim, projected);
    block_residual_add(input + (position * hidden_dim), projected, hidden_dim, residual_attn);
    block_rms_norm_reference(residual_attn, post_attn_norm_weight, hidden_dim,
                             epsilon, post_norm);
    cli_mlp_reference(post_norm, mlp_gate_weight, mlp_up_weight, mlp_down_weight,
                      1ull, hidden_dim, ffn_dim, 0ull, 0, mlp_intermediate, mlp_out);
    block_residual_add(residual_attn, mlp_out, hidden_dim, reference_out);

done:
    free(mlp_out);
    free(mlp_intermediate);
    free(post_norm);
    free(residual_attn);
    free(projected);
    free(attention);
    free(probabilities);
    free(scores);
    free(rope_k);
    free(rope_q);
    free(v);
    free(k);
    free(q);
    free(normalized);
    return rc;
}

static int block_write_f32(yvex_backend *backend,
                           yvex_device_tensor *tensor,
                           const float *values,
                           unsigned long long elements,
                           yvex_error *err)
{
    return yvex_backend_tensor_write(backend, tensor, values,
                                     elements * (unsigned long long)sizeof(float),
                                     err);
}

static int block_read_f32(yvex_backend *backend,
                          const yvex_device_tensor *tensor,
                          float *values,
                          unsigned long long elements,
                          yvex_error *err)
{
    return yvex_backend_tensor_read(backend, tensor, values,
                                    elements * (unsigned long long)sizeof(float),
                                    err);
}

typedef struct {
    const char *backend_name;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    const char *activation;
    int causal;
    int gated;
    const float *input_override_values;
    float *output_copy_values;
    float *reference_copy_values;
    int fail_after_backend_alloc;
} yvex_controlled_block_request;

typedef struct {
    float *input_values;
    float *attn_norm_weight_values;
    float *post_attn_norm_weight_values;
    float *q_weight_values;
    float *k_weight_values;
    float *v_weight_values;
    float *o_weight_values;
    float *mlp_gate_values;
    float *mlp_up_values;
    float *mlp_down_values;
    float *normalized_values;
    float *q_values;
    float *k_values;
    float *v_values;
    float *rope_q_values;
    float *rope_k_values;
    float *attention_values;
    float *projected_values;
    float *residual_values;
    float *post_norm_values;
    float *mlp_values;
    float *output_values;
    float *reference_values;
    float *row_values;
    float *mlp_intermediate_values;
} yvex_controlled_block_host_scratch;

typedef struct {
    yvex_backend *backend;
    yvex_cli_block_tensors t;
} yvex_controlled_block_backend_scratch;

typedef struct {
    const char *status;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *graph_kind;
    const char *backend_name;
    const char *backend_status;
    const char *backend_op_status;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    int causal;
    int gated;
    unsigned long long op_count;
    unsigned long long input_bytes;
    unsigned long long qkv_weight_bytes_each;
    unsigned long long mlp_weight_bytes;
    unsigned long long score_scratch_bytes;
    unsigned long long mlp_intermediate_bytes;
    unsigned long long scratch_planned_bytes;
    unsigned long long backend_allocated_bytes;
    unsigned long long output_bytes;
    unsigned long long output_checksum;
    unsigned long long reference_checksum;
    float max_abs_diff;
    int cleanup_attempted;
    const char *cleanup_status;
    int execution_ready;
    int graph_execution_ready;
    int generation_ready;
    int block_cuda_parity;
    unsigned long long sample_count;
    float output_sample_values[8];
    float reference_sample_values[8];
} yvex_controlled_block_result;

static void controlled_block_result_init(yvex_controlled_block_result *result,
                                         const yvex_controlled_block_request *request)
{
    memset(result, 0, sizeof(*result));
    result->status = "graph-block-fail";
    result->graph_integrity_guard = "fail";
    result->graph_execution_phase = "scratch-plan";
    result->graph_kind = "controlled-block-fixture";
    result->backend_name = request->backend_name;
    result->backend_status = "not-opened";
    result->backend_op_status = "unchecked";
    result->seq_len = request->seq_len;
    result->position = request->position;
    result->hidden_dim = request->hidden_dim;
    result->head_dim = request->head_dim;
    result->ffn_dim = request->ffn_dim;
    result->causal = request->causal;
    result->gated = request->gated;
    result->op_count = 12ull;
    result->cleanup_status = "not-needed";
    result->execution_ready = 0;
    result->graph_execution_ready = 0;
    result->generation_ready = 0;
}

static void controlled_block_host_scratch_free(yvex_controlled_block_host_scratch *scratch)
{
    if (!scratch) {
        return;
    }
    free(scratch->mlp_intermediate_values);
    free(scratch->row_values);
    free(scratch->reference_values);
    free(scratch->output_values);
    free(scratch->mlp_values);
    free(scratch->post_norm_values);
    free(scratch->residual_values);
    free(scratch->projected_values);
    free(scratch->attention_values);
    free(scratch->rope_k_values);
    free(scratch->rope_q_values);
    free(scratch->v_values);
    free(scratch->k_values);
    free(scratch->q_values);
    free(scratch->normalized_values);
    free(scratch->mlp_down_values);
    free(scratch->mlp_up_values);
    free(scratch->mlp_gate_values);
    free(scratch->o_weight_values);
    free(scratch->v_weight_values);
    free(scratch->k_weight_values);
    free(scratch->q_weight_values);
    free(scratch->post_attn_norm_weight_values);
    free(scratch->attn_norm_weight_values);
    free(scratch->input_values);
    memset(scratch, 0, sizeof(*scratch));
}

static int controlled_block_host_scratch_has_allocations(
    const yvex_controlled_block_host_scratch *scratch)
{
    return scratch &&
           (scratch->input_values ||
            scratch->attn_norm_weight_values ||
            scratch->post_attn_norm_weight_values ||
            scratch->q_weight_values ||
            scratch->k_weight_values ||
            scratch->v_weight_values ||
            scratch->o_weight_values ||
            scratch->mlp_gate_values ||
            scratch->mlp_up_values ||
            scratch->mlp_down_values ||
            scratch->normalized_values ||
            scratch->q_values ||
            scratch->k_values ||
            scratch->v_values ||
            scratch->rope_q_values ||
            scratch->rope_k_values ||
            scratch->attention_values ||
            scratch->projected_values ||
            scratch->residual_values ||
            scratch->post_norm_values ||
            scratch->mlp_values ||
            scratch->output_values ||
            scratch->reference_values ||
            scratch->row_values ||
            scratch->mlp_intermediate_values);
}

static void controlled_block_backend_scratch_close(
    yvex_controlled_block_backend_scratch *scratch)
{
    if (!scratch) {
        return;
    }
    if (scratch->backend) {
        block_free_tensors(scratch->backend, &scratch->t);
        yvex_backend_close(scratch->backend);
    }
    memset(scratch, 0, sizeof(*scratch));
}

static int controlled_block_backend_scratch_has_allocations(
    const yvex_controlled_block_backend_scratch *scratch)
{
    return scratch && scratch->backend;
}

static unsigned long long controlled_block_tensor_bytes(unsigned int rank,
                                                        unsigned long long d0,
                                                        unsigned long long d1)
{
    unsigned long long elements = rank > 1 ? d0 * d1 : d0;
    return elements * (unsigned long long)sizeof(float);
}

static int controlled_block_alloc_f32(yvex_backend *backend,
                                      const char *name,
                                      unsigned int rank,
                                      unsigned long long d0,
                                      unsigned long long d1,
                                      yvex_device_tensor **out,
                                      yvex_controlled_block_result *result,
                                      yvex_error *err)
{
    int rc = block_alloc_f32(backend, name, rank, d0, d1, out, err);
    if (rc == YVEX_OK) {
        result->backend_allocated_bytes += controlled_block_tensor_bytes(rank, d0, d1);
    }
    return rc;
}

static void controlled_block_result_print(const yvex_controlled_block_result *result)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "status: %s\n", result->status);
    yvex_cli_out_writef(stdout, "graph_integrity_guard: %s\n", result->graph_integrity_guard);
    yvex_cli_out_writef(stdout, "graph_execution_phase: %s\n", result->graph_execution_phase);
    yvex_cli_out_writef(stdout, "graph_kind: %s\n", result->graph_kind);
    yvex_cli_out_writef(stdout, "block: fixture\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", result->backend_name);
    yvex_cli_out_writef(stdout, "backend_status: %s\n", result->backend_status);
    yvex_cli_out_writef(stdout, "backend_op_status: %s\n", result->backend_op_status);
    yvex_cli_out_writef(stdout, "seq_len: %llu\n", result->seq_len);
    yvex_cli_out_writef(stdout, "position: %llu\n", result->position);
    yvex_cli_out_writef(stdout, "hidden_dim: %llu\n", result->hidden_dim);
    yvex_cli_out_writef(stdout, "head_dim: %llu\n", result->head_dim);
    yvex_cli_out_writef(stdout, "ffn_dim: %llu\n", result->ffn_dim);
    yvex_cli_out_writef(stdout, "causal: %s\n", result->causal ? "true" : "false");
    yvex_cli_out_writef(stdout, "gated: %s\n", result->gated ? "true" : "false");
    yvex_cli_out_writef(stdout, "op_count: %llu\n", result->op_count);
    yvex_cli_out_writef(stdout, "phase: input\n");
    yvex_cli_out_writef(stdout, "phase: attn_norm\n");
    yvex_cli_out_writef(stdout, "phase: q_projection\n");
    yvex_cli_out_writef(stdout, "phase: k_projection\n");
    yvex_cli_out_writef(stdout, "phase: v_projection\n");
    yvex_cli_out_writef(stdout, "phase: rope\n");
    yvex_cli_out_writef(stdout, "phase: attention\n");
    yvex_cli_out_writef(stdout, "phase: o_projection\n");
    yvex_cli_out_writef(stdout, "phase: residual_attn\n");
    yvex_cli_out_writef(stdout, "phase: post_attn_norm\n");
    yvex_cli_out_writef(stdout, "phase: mlp\n");
    yvex_cli_out_writef(stdout, "phase: residual_mlp\n");
    yvex_cli_out_writef(stdout, "phase: output\n");
    yvex_cli_out_writef(stdout, "input_bytes: %llu\n", result->input_bytes);
    yvex_cli_out_writef(stdout, "qkv_weight_bytes_each: %llu\n", result->qkv_weight_bytes_each);
    yvex_cli_out_writef(stdout, "mlp_weight_bytes: %llu\n", result->mlp_weight_bytes);
    yvex_cli_out_writef(stdout, "score_scratch_bytes: %llu\n", result->score_scratch_bytes);
    yvex_cli_out_writef(stdout, "mlp_intermediate_bytes: %llu\n", result->mlp_intermediate_bytes);
    yvex_cli_out_writef(stdout, "scratch_planned_bytes: %llu\n", result->scratch_planned_bytes);
    yvex_cli_out_writef(stdout, "backend_allocated_bytes: %llu\n", result->backend_allocated_bytes);
    yvex_cli_out_writef(stdout, "output_bytes: %llu\n", result->output_bytes);
    yvex_cli_out_writef(stdout, "checksum: %llu\n", result->output_checksum);
    yvex_cli_out_writef(stdout, "output_checksum: %llu\n", result->output_checksum);
    yvex_cli_out_writef(stdout, "reference_checksum: %llu\n", result->reference_checksum);
    yvex_cli_out_writef(stdout, "max_abs_diff: %.9g\n", (double)result->max_abs_diff);
    yvex_cli_out_writef(stdout, "phase: cleanup\n");
    yvex_cli_out_writef(stdout, "cleanup: %s\n",
           strcmp(result->cleanup_status, "fail") == 0 ? "fail" : "pass");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n", result->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n", result->cleanup_status);
    if (strcmp(result->backend_name, "cuda") == 0) {
        yvex_cli_out_writef(stdout, "block_cuda_parity: %s\n", result->block_cuda_parity ? "pass" : "fail");
    }
    yvex_cli_out_writef(stdout, "sample_count: %llu\n", result->sample_count);
    yvex_cli_out_writef(stdout, "output_sample_values:");
    for (i = 0; i < result->sample_count; ++i) {
        yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)result->output_sample_values[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "reference_sample_values:");
    for (i = 0; i < result->sample_count; ++i) {
        yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)result->reference_sample_values[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
}

static int controlled_block_execute_fixture(
    const yvex_controlled_block_request *request,
    yvex_controlled_block_result *result,
    yvex_error *err)
{
    const float epsilon = 0.000001f;
    const float rope_base = 10000.0f;
    yvex_backend_options backend_options;
    yvex_controlled_block_host_scratch host;
    yvex_controlled_block_backend_scratch device;
    yvex_mlp_options mlp_options;
    unsigned long long seq_hidden;
    unsigned long long hidden_hidden;
    unsigned long long hidden_ffn;
    unsigned long long hidden_bytes;
    unsigned long long seq_hidden_bytes;
    unsigned long long ffn_bytes;
    unsigned long long hidden_hidden_bytes;
    unsigned long long hidden_ffn_bytes;
    unsigned long long ffn_hidden_bytes;
    unsigned long long score_bytes;
    unsigned long long output_bytes;
    unsigned long long row;
    unsigned long long i;
    float scale;
    int rc = YVEX_OK;
    int exit_code = 1;

    memset(&backend_options, 0, sizeof(backend_options));
    memset(&host, 0, sizeof(host));
    memset(&device, 0, sizeof(device));
    memset(&mlp_options, 0, sizeof(mlp_options));
    controlled_block_result_init(result, request);

    seq_hidden = request->seq_len * request->hidden_dim;
    hidden_hidden = request->hidden_dim * request->hidden_dim;
    hidden_ffn = request->hidden_dim * request->ffn_dim;
    hidden_bytes = request->hidden_dim * (unsigned long long)sizeof(float);
    seq_hidden_bytes = seq_hidden * (unsigned long long)sizeof(float);
    ffn_bytes = request->ffn_dim * (unsigned long long)sizeof(float);
    hidden_hidden_bytes = hidden_hidden * (unsigned long long)sizeof(float);
    hidden_ffn_bytes = hidden_ffn * (unsigned long long)sizeof(float);
    ffn_hidden_bytes = request->ffn_dim * request->hidden_dim *
                       (unsigned long long)sizeof(float);
    score_bytes = request->seq_len * (unsigned long long)sizeof(float);
    output_bytes = hidden_bytes;
    scale = (float)(1.0 / cli_sqrt_double((double)request->head_dim));
    result->input_bytes = seq_hidden_bytes;
    result->qkv_weight_bytes_each = hidden_hidden_bytes;
    result->mlp_weight_bytes = hidden_ffn_bytes + hidden_ffn_bytes + ffn_hidden_bytes;
    result->score_scratch_bytes = score_bytes;
    result->mlp_intermediate_bytes = ffn_bytes;
    result->scratch_planned_bytes =
        (seq_hidden_bytes * 4ull) + (hidden_bytes * 10ull) +
        (score_bytes * 2ull) + ffn_bytes;
    result->output_bytes = output_bytes;

    result->graph_execution_phase = "host-allocation";
    host.input_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    host.attn_norm_weight_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.post_attn_norm_weight_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.q_weight_values = (float *)calloc((size_t)hidden_hidden, sizeof(float));
    host.k_weight_values = (float *)calloc((size_t)hidden_hidden, sizeof(float));
    host.v_weight_values = (float *)calloc((size_t)hidden_hidden, sizeof(float));
    host.o_weight_values = (float *)calloc((size_t)hidden_hidden, sizeof(float));
    host.mlp_gate_values = (float *)calloc((size_t)hidden_ffn, sizeof(float));
    host.mlp_up_values = (float *)calloc((size_t)hidden_ffn, sizeof(float));
    host.mlp_down_values = (float *)calloc((size_t)(request->ffn_dim * request->hidden_dim), sizeof(float));
    host.normalized_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    host.q_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    host.k_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    host.v_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    host.rope_q_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.rope_k_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    host.attention_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.projected_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.residual_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.post_norm_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.mlp_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.output_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.reference_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.row_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    host.mlp_intermediate_values = (float *)calloc((size_t)request->ffn_dim, sizeof(float));
    if (!host.input_values || !host.attn_norm_weight_values ||
        !host.post_attn_norm_weight_values || !host.q_weight_values ||
        !host.k_weight_values || !host.v_weight_values || !host.o_weight_values ||
        !host.mlp_gate_values || !host.mlp_up_values || !host.mlp_down_values ||
        !host.normalized_values || !host.q_values || !host.k_values ||
        !host.v_values || !host.rope_q_values || !host.rope_k_values ||
        !host.attention_values || !host.projected_values ||
        !host.residual_values || !host.post_norm_values || !host.mlp_values ||
        !host.output_values || !host.reference_values || !host.row_values ||
        !host.mlp_intermediate_values) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex graph block",
                       "failed to allocate host block buffers");
        exit_code = exit_for_status(YVEX_ERR_NOMEM);
        goto cleanup;
    }

    block_fill_input(host.input_values, request->seq_len, request->hidden_dim);
    block_fill_norm_weight(host.attn_norm_weight_values, request->hidden_dim, 1.0);
    block_fill_norm_weight(host.post_attn_norm_weight_values, request->hidden_dim, 0.95);
    block_fill_projection_weight(host.q_weight_values, request->hidden_dim, 0.006);
    block_fill_projection_weight(host.k_weight_values, request->hidden_dim, 0.007);
    block_fill_projection_weight(host.v_weight_values, request->hidden_dim, 0.008);
    block_fill_projection_weight(host.o_weight_values, request->hidden_dim, 0.005);
    block_fill_mlp_weights(host.mlp_gate_values, host.mlp_up_values,
                           host.mlp_down_values, request->hidden_dim,
                           request->ffn_dim);
    if (request->input_override_values) {
        memcpy(host.input_values, request->input_override_values,
               (size_t)seq_hidden_bytes);
    }

    result->graph_execution_phase = "backend-allocation";
    memset(&backend_options, 0, sizeof(backend_options));
    backend_options.kind = graph_backend_kind_from_name(request->backend_name);
    rc = yvex_backend_open(&device.backend, &backend_options, err);
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }
    result->backend_status = "ready";
    if (!yvex_backend_supports(device.backend, YVEX_BACKEND_CAP_OP_RMS_NORM) ||
        !yvex_backend_supports(device.backend, YVEX_BACKEND_CAP_OP_MATMUL) ||
        !yvex_backend_supports(device.backend, YVEX_BACKEND_CAP_OP_ROPE) ||
        !yvex_backend_supports(device.backend, YVEX_BACKEND_CAP_OP_ATTENTION) ||
        !yvex_backend_supports(device.backend, YVEX_BACKEND_CAP_OP_MLP)) {
        result->backend_op_status = "unsupported";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex graph block",
                       "graph block fixture requires RMSNorm, matmul, RoPE, attention, and MLP backend ops");
        exit_code = 5;
        goto cleanup;
    }
    result->backend_op_status = "supported";

    rc = controlled_block_alloc_f32(device.backend, "block.input_hidden_states",
                                    2, request->seq_len, request->hidden_dim,
                                    &device.t.input_states, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.attn_norm.weight", 1, request->hidden_dim, 0, &device.t.attn_norm_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.normalized_input", 2, request->seq_len, request->hidden_dim, &device.t.normalized_input, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.q.weight", 2, request->hidden_dim, request->hidden_dim, &device.t.q_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.k.weight", 2, request->hidden_dim, request->hidden_dim, &device.t.k_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.v.weight", 2, request->hidden_dim, request->hidden_dim, &device.t.v_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.o.weight", 2, request->hidden_dim, request->hidden_dim, &device.t.o_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.q", 2, request->seq_len, request->hidden_dim, &device.t.q, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.k", 2, request->seq_len, request->hidden_dim, &device.t.k, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.v", 2, request->seq_len, request->hidden_dim, &device.t.v, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.row_input", 1, request->hidden_dim, 0, &device.t.row_input, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.row_output", 1, request->hidden_dim, 0, &device.t.row_output, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.rope_q", 1, request->hidden_dim, 0, &device.t.rope_q, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.rope_k", 2, request->seq_len, request->hidden_dim, &device.t.rope_k, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.attention_output", 1, request->hidden_dim, 0, &device.t.attention_output, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.attention_scores", 1, request->seq_len, 0, &device.t.attention_scores, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.attention_probabilities", 1, request->seq_len, 0, &device.t.attention_probabilities, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.attention_output_matrix", 2, 1, request->hidden_dim, &device.t.attention_output_matrix, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.projected_attention", 2, 1, request->hidden_dim, &device.t.projected_attention, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.residual_after_attention", 2, 1, request->hidden_dim, &device.t.residual_after_attention, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.post_attn_norm.weight", 1, request->hidden_dim, 0, &device.t.post_attn_norm_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.post_attn_norm", 2, 1, request->hidden_dim, &device.t.post_attn_norm, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.mlp.gate", 2, request->hidden_dim, request->ffn_dim, &device.t.mlp_gate_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.mlp.up", 2, request->hidden_dim, request->ffn_dim, &device.t.mlp_up_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.mlp.down", 2, request->ffn_dim, request->hidden_dim, &device.t.mlp_down_weight, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.mlp.intermediate", 2, 1, request->ffn_dim, &device.t.mlp_intermediate, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.mlp.output", 2, 1, request->hidden_dim, &device.t.mlp_output, result, err);
    if (rc == YVEX_OK) rc = controlled_block_alloc_f32(device.backend, "block.output", 2, 1, request->hidden_dim, &device.t.block_output, result, err);
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }
    if (request->fail_after_backend_alloc ||
        cli_test_env_enabled("YVEX_TEST_FAIL_BLOCK_AFTER_BACKEND_ALLOC")) {
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex graph block",
                       "test block failure after backend allocation");
        exit_code = exit_for_status(YVEX_ERR_BACKEND);
        goto cleanup;
    }

    result->graph_execution_phase = "dispatch";
    rc = block_write_f32(device.backend, device.t.input_states,
                         host.input_values, seq_hidden, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.attn_norm_weight, host.attn_norm_weight_values, request->hidden_dim, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.q_weight, host.q_weight_values, hidden_hidden, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.k_weight, host.k_weight_values, hidden_hidden, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.v_weight, host.v_weight_values, hidden_hidden, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.o_weight, host.o_weight_values, hidden_hidden, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.post_attn_norm_weight, host.post_attn_norm_weight_values, request->hidden_dim, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.mlp_gate_weight, host.mlp_gate_values, hidden_ffn, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.mlp_up_weight, host.mlp_up_values, hidden_ffn, err);
    if (rc == YVEX_OK) rc = block_write_f32(device.backend, device.t.mlp_down_weight, host.mlp_down_values, request->ffn_dim * request->hidden_dim, err);
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }

    for (row = 0; row < request->seq_len; ++row) {
        rc = block_write_f32(device.backend, device.t.row_input,
                             host.input_values + (row * request->hidden_dim),
                             request->hidden_dim, err);
        if (rc == YVEX_OK) {
            rc = yvex_backend_op_rms_norm(device.backend, device.t.row_input,
                                          device.t.attn_norm_weight, epsilon,
                                          device.t.row_output, err);
        }
        if (rc == YVEX_OK) {
            rc = block_read_f32(device.backend, device.t.row_output,
                                host.normalized_values + (row * request->hidden_dim),
                                request->hidden_dim, err);
        }
        if (rc != YVEX_OK) {
            exit_code = exit_for_status(rc);
            goto cleanup;
        }
    }
    rc = block_write_f32(device.backend, device.t.normalized_input,
                         host.normalized_values, seq_hidden, err);
    if (rc == YVEX_OK) rc = yvex_backend_op_matmul(device.backend, device.t.normalized_input, device.t.q_weight, device.t.q, err);
    if (rc == YVEX_OK) rc = yvex_backend_op_matmul(device.backend, device.t.normalized_input, device.t.k_weight, device.t.k, err);
    if (rc == YVEX_OK) rc = yvex_backend_op_matmul(device.backend, device.t.normalized_input, device.t.v_weight, device.t.v, err);
    if (rc == YVEX_OK) rc = block_read_f32(device.backend, device.t.q, host.q_values, seq_hidden, err);
    if (rc == YVEX_OK) rc = block_read_f32(device.backend, device.t.k, host.k_values, seq_hidden, err);
    if (rc == YVEX_OK) rc = block_read_f32(device.backend, device.t.v, host.v_values, seq_hidden, err);
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }

    rc = block_write_f32(device.backend, device.t.row_input,
                         host.q_values + (request->position * request->hidden_dim),
                         request->hidden_dim, err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_op_rope(device.backend, device.t.row_input,
                                  request->position, rope_base, device.t.rope_q, err);
    }
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.rope_q,
                            host.rope_q_values, request->hidden_dim, err);
    }
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }
    for (row = 0; row < request->seq_len; ++row) {
        rc = block_write_f32(device.backend, device.t.row_input,
                             host.k_values + (row * request->hidden_dim),
                             request->hidden_dim, err);
        if (rc == YVEX_OK) {
            rc = yvex_backend_op_rope(device.backend, device.t.row_input,
                                      row, rope_base, device.t.row_output, err);
        }
        if (rc == YVEX_OK) {
            rc = block_read_f32(device.backend, device.t.row_output,
                                host.rope_k_values + (row * request->hidden_dim),
                                request->hidden_dim, err);
        }
        if (rc != YVEX_OK) {
            exit_code = exit_for_status(rc);
            goto cleanup;
        }
    }
    rc = block_write_f32(device.backend, device.t.rope_k,
                         host.rope_k_values, seq_hidden, err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_op_attention(device.backend, device.t.rope_q,
                                       device.t.rope_k, device.t.v,
                                       request->seq_len, request->position,
                                       scale, request->causal,
                                       device.t.attention_scores,
                                       device.t.attention_probabilities,
                                       device.t.attention_output, err);
    }
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.attention_output,
                            host.attention_values, request->hidden_dim, err);
    }
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }

    rc = block_write_f32(device.backend, device.t.attention_output_matrix,
                         host.attention_values, request->hidden_dim, err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_op_matmul(device.backend,
                                    device.t.attention_output_matrix,
                                    device.t.o_weight,
                                    device.t.projected_attention, err);
    }
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.projected_attention,
                            host.projected_values, request->hidden_dim, err);
    }
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }
    block_residual_add(host.input_values + (request->position * request->hidden_dim),
                       host.projected_values, request->hidden_dim,
                       host.residual_values);
    rc = block_write_f32(device.backend, device.t.residual_after_attention,
                         host.residual_values, request->hidden_dim, err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_op_rms_norm(device.backend,
                                      device.t.residual_after_attention,
                                      device.t.post_attn_norm_weight, epsilon,
                                      device.t.post_attn_norm, err);
    }
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.post_attn_norm,
                            host.post_norm_values, request->hidden_dim, err);
    }
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }

    mlp_options.batch = 1ull;
    mlp_options.hidden_dim = request->hidden_dim;
    mlp_options.ffn_dim = request->ffn_dim;
    mlp_options.activation = request->activation;
    mlp_options.gated = request->gated;
    rc = yvex_backend_op_mlp(device.backend, device.t.post_attn_norm,
                             device.t.mlp_gate_weight, device.t.mlp_up_weight,
                             device.t.mlp_down_weight, &mlp_options,
                             device.t.mlp_intermediate, device.t.mlp_output, err);
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.mlp_output,
                            host.mlp_values, request->hidden_dim, err);
    }
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.mlp_intermediate,
                            host.mlp_intermediate_values, request->ffn_dim, err);
    }
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }
    block_residual_add(host.residual_values, host.mlp_values,
                       request->hidden_dim, host.output_values);
    rc = block_write_f32(device.backend, device.t.block_output,
                         host.output_values, request->hidden_dim, err);
    if (rc == YVEX_OK) {
        rc = block_read_f32(device.backend, device.t.block_output,
                            host.output_values, request->hidden_dim, err);
    }
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }

    result->graph_execution_phase = "readback-reference";
    rc = block_reference(host.input_values, host.attn_norm_weight_values,
                         host.q_weight_values, host.k_weight_values,
                         host.v_weight_values, host.o_weight_values,
                         host.post_attn_norm_weight_values,
                         host.mlp_gate_values, host.mlp_up_values,
                         host.mlp_down_values, request->seq_len, request->position,
                         request->hidden_dim, request->ffn_dim, request->causal,
                         host.reference_values, err);
    if (rc != YVEX_OK) {
        exit_code = exit_for_status(rc);
        goto cleanup;
    }

    result->graph_execution_phase = "comparison";
    result->max_abs_diff = cli_max_abs_diff_f32(host.output_values,
                                                host.reference_values,
                                                request->hidden_dim);
    result->output_checksum = cli_checksum_bytes(host.output_values, output_bytes);
    result->reference_checksum = cli_checksum_bytes(host.reference_values, output_bytes);
    result->sample_count = request->hidden_dim < 8ull ? request->hidden_dim : 8ull;
    for (i = 0; i < result->sample_count; ++i) {
        result->output_sample_values[i] = host.output_values[i];
        result->reference_sample_values[i] = host.reference_values[i];
    }
    if (request->output_copy_values) {
        memcpy(request->output_copy_values, host.output_values,
               (size_t)output_bytes);
    }
    if (request->reference_copy_values) {
        memcpy(request->reference_copy_values, host.reference_values,
               (size_t)output_bytes);
    }
    if (result->max_abs_diff > 0.002f) {
        yvex_error_setf(err, YVEX_ERR_STATE, "yvex graph block",
                        "controlled block reference comparison failed: max_abs_diff %.9g",
                        (double)result->max_abs_diff);
        exit_code = exit_for_status(YVEX_ERR_STATE);
        goto cleanup;
    }
    result->status = "graph-block";
    result->graph_integrity_guard = "pass";
    result->graph_execution_phase = "complete";
    result->block_cuda_parity = 1;
    exit_code = 0;

cleanup:
    if (controlled_block_backend_scratch_has_allocations(&device) ||
        controlled_block_host_scratch_has_allocations(&host)) {
        result->cleanup_attempted = 1;
        result->cleanup_status = "pass";
    }
    controlled_block_backend_scratch_close(&device);
    controlled_block_host_scratch_free(&host);
    if (exit_code == 0) {
        yvex_error_clear(err);
    }
    return exit_code;
}

static int command_graph_execute_block_fixture(const char *backend_name,
                                               unsigned long long seq_len,
                                               unsigned long long position,
                                               unsigned long long hidden_dim,
                                               unsigned long long head_dim,
                                               unsigned long long ffn_dim,
                                               int causal,
                                               int gated)
{
    yvex_controlled_block_request request;
    yvex_controlled_block_result result;
    yvex_error err;
    unsigned long long seq_hidden;
    unsigned long long hidden_hidden;
    unsigned long long hidden_ffn;
    int exit_code;

    yvex_error_clear(&err);

    if (seq_len == 0 || hidden_dim == 0 || head_dim == 0 || ffn_dim == 0) {
        yvex_cli_out_writef(stderr, "yvex: block fixture dimensions must be positive\n");
        return 2;
    }
    if (position >= seq_len) {
        yvex_cli_out_writef(stderr, "yvex: block fixture position must be less than seq-len\n");
        return 2;
    }
    if (hidden_dim % head_dim != 0ull) {
        yvex_cli_out_writef(stderr, "yvex: block fixture hidden-dim must be divisible by head-dim\n");
        return 2;
    }
    if (hidden_dim != head_dim) {
        yvex_cli_out_writef(stderr, "yvex: block fixture currently supports one attention head\n");
        return 5;
    }
    if (!gated) {
        yvex_cli_out_writef(stderr, "yvex: block fixture requires gated MLP\n");
        return 2;
    }
    if (seq_len > ULLONG_MAX / hidden_dim ||
        hidden_dim > ULLONG_MAX / hidden_dim ||
        hidden_dim > ULLONG_MAX / ffn_dim ||
        ffn_dim > ULLONG_MAX / hidden_dim) {
        yvex_cli_out_writef(stderr, "yvex: block fixture dimension overflow\n");
        return 4;
    }
    seq_hidden = seq_len * hidden_dim;
    hidden_hidden = hidden_dim * hidden_dim;
    hidden_ffn = hidden_dim * ffn_dim;
    if (seq_hidden > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        hidden_hidden > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        hidden_ffn > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        (ffn_dim * hidden_dim) > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        yvex_cli_out_writef(stderr, "yvex: block fixture byte count overflow\n");
        return 4;
    }

    memset(&request, 0, sizeof(request));
    request.backend_name = backend_name;
    request.seq_len = seq_len;
    request.position = position;
    request.hidden_dim = hidden_dim;
    request.head_dim = head_dim;
    request.ffn_dim = ffn_dim;
    request.activation = "silu";
    request.causal = causal;
    request.gated = gated;

    exit_code = controlled_block_execute_fixture(&request, &result, &err);
    controlled_block_result_print(&result);
    if (exit_code != 0 && yvex_error_code(&err) != YVEX_OK) {
        return print_yvex_error(&err, exit_code);
    }
    return exit_code;
}

typedef struct {
    const char *backend_name;
    unsigned long long layers;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    int causal;
    int gated;
    const float *initial_position_values;
    unsigned long long initial_position_value_count;
} yvex_controlled_layer_scheduler_request;

typedef struct {
    const char *status;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *graph_kind;
    const char *backend_name;
    const char *backend_status;
    const char *backend_op_status;
    unsigned long long layers;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    int causal;
    int gated;
    unsigned long long op_count_per_layer;
    unsigned long long total_op_count;
    unsigned long long input_bytes_per_layer;
    unsigned long long scratch_planned_bytes_per_layer;
    unsigned long long backend_allocated_bytes_total;
    unsigned long long output_bytes;
    unsigned long long layer_output_checksums[16];
    unsigned long long layer_reference_checksums[16];
    float layer_max_abs_diffs[16];
    unsigned long long final_output_checksum;
    unsigned long long final_reference_checksum;
    float final_max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
    int layers_cuda_parity;
} yvex_controlled_layer_scheduler_result;

static void controlled_layer_scheduler_result_init(
    yvex_controlled_layer_scheduler_result *result,
    const yvex_controlled_layer_scheduler_request *request)
{
    memset(result, 0, sizeof(*result));
    result->status = "graph-layers-fail";
    result->graph_integrity_guard = "fail";
    result->graph_execution_phase = "scratch-plan";
    result->graph_kind = "controlled-layer-fixture";
    result->backend_name = request->backend_name;
    result->backend_status = "not-opened";
    result->backend_op_status = "unchecked";
    result->layers = request->layers;
    result->seq_len = request->seq_len;
    result->position = request->position;
    result->hidden_dim = request->hidden_dim;
    result->head_dim = request->head_dim;
    result->ffn_dim = request->ffn_dim;
    result->causal = request->causal;
    result->gated = request->gated;
    result->op_count_per_layer = 12ull;
    result->total_op_count = request->layers * 12ull;
    result->cleanup_status = "not-needed";
    result->layers_cuda_parity = 1;
}

static void controlled_layer_scheduler_print(
    const yvex_controlled_layer_scheduler_result *result)
{
    unsigned long long layer;

    yvex_cli_out_writef(stdout, "status: %s\n", result->status);
    yvex_cli_out_writef(stdout, "graph_integrity_guard: %s\n", result->graph_integrity_guard);
    yvex_cli_out_writef(stdout, "graph_execution_phase: %s\n", result->graph_execution_phase);
    yvex_cli_out_writef(stdout, "graph_kind: %s\n", result->graph_kind);
    yvex_cli_out_writef(stdout, "block: fixture\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", result->backend_name);
    yvex_cli_out_writef(stdout, "backend_status: %s\n", result->backend_status);
    yvex_cli_out_writef(stdout, "backend_op_status: %s\n", result->backend_op_status);
    yvex_cli_out_writef(stdout, "layers: %llu\n", result->layers);
    yvex_cli_out_writef(stdout, "seq_len: %llu\n", result->seq_len);
    yvex_cli_out_writef(stdout, "position: %llu\n", result->position);
    yvex_cli_out_writef(stdout, "hidden_dim: %llu\n", result->hidden_dim);
    yvex_cli_out_writef(stdout, "head_dim: %llu\n", result->head_dim);
    yvex_cli_out_writef(stdout, "ffn_dim: %llu\n", result->ffn_dim);
    yvex_cli_out_writef(stdout, "causal: %s\n", result->causal ? "true" : "false");
    yvex_cli_out_writef(stdout, "gated: %s\n", result->gated ? "true" : "false");
    yvex_cli_out_writef(stdout, "layer_handoff: selected-position-row\n");
    yvex_cli_out_writef(stdout, "sequence_rebuild: deterministic-with-previous-position-row\n");
    yvex_cli_out_writef(stdout, "op_count_per_layer: %llu\n", result->op_count_per_layer);
    yvex_cli_out_writef(stdout, "total_op_count: %llu\n", result->total_op_count);
    yvex_cli_out_writef(stdout, "input_bytes_per_layer: %llu\n", result->input_bytes_per_layer);
    yvex_cli_out_writef(stdout, "scratch_planned_bytes_per_layer: %llu\n",
           result->scratch_planned_bytes_per_layer);
    yvex_cli_out_writef(stdout, "backend_allocated_bytes_total: %llu\n",
           result->backend_allocated_bytes_total);
    yvex_cli_out_writef(stdout, "output_bytes: %llu\n", result->output_bytes);
    for (layer = 0; layer < result->layers && layer < 16ull; ++layer) {
        yvex_cli_out_writef(stdout, "layer_%llu_checksum: %llu\n", layer,
               result->layer_output_checksums[layer]);
        yvex_cli_out_writef(stdout, "layer_%llu_reference_checksum: %llu\n", layer,
               result->layer_reference_checksums[layer]);
        yvex_cli_out_writef(stdout, "layer_%llu_max_abs_diff: %.9g\n", layer,
               (double)result->layer_max_abs_diffs[layer]);
    }
    yvex_cli_out_writef(stdout, "final_output_checksum: %llu\n", result->final_output_checksum);
    yvex_cli_out_writef(stdout, "final_reference_checksum: %llu\n", result->final_reference_checksum);
    yvex_cli_out_writef(stdout, "final_max_abs_diff: %.9g\n", (double)result->final_max_abs_diff);
    if (strcmp(result->backend_name, "cuda") == 0) {
        yvex_cli_out_writef(stdout, "layers_cuda_parity: %s\n",
               result->layers_cuda_parity ? "pass" : "fail");
        yvex_cli_out_writef(stdout, "cuda_reference_max_abs_diff: %.9g\n",
               (double)result->final_max_abs_diff);
    } else {
        yvex_cli_out_writef(stdout, "cpu_reference_max_abs_diff: %.9g\n",
               (double)result->final_max_abs_diff);
    }
    yvex_cli_out_writef(stdout, "phase: cleanup\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n", result->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n", result->cleanup_status);
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", result->status);
}

static int controlled_layer_scheduler_execute(
    const yvex_controlled_layer_scheduler_request *request,
    yvex_controlled_layer_scheduler_result *result,
    yvex_error *err)
{
    yvex_controlled_block_request block_request;
    yvex_controlled_block_result block_result;
    float *sequence_values = NULL;
    float *previous_output_values = NULL;
    float *layer_output_values = NULL;
    float *layer_reference_values = NULL;
    unsigned long long seq_hidden;
    unsigned long long hidden_bytes;
    unsigned long long seq_hidden_bytes;
    unsigned long long layer;
    unsigned long long copy_count;
    int fail_after_layer_0;
    int fail_after_backend_alloc;
    int exit_code = 1;

    controlled_layer_scheduler_result_init(result, request);
    seq_hidden = request->seq_len * request->hidden_dim;
    hidden_bytes = request->hidden_dim * (unsigned long long)sizeof(float);
    seq_hidden_bytes = seq_hidden * (unsigned long long)sizeof(float);
    result->input_bytes_per_layer = seq_hidden_bytes;
    result->output_bytes = hidden_bytes;

    result->graph_execution_phase = "host-allocation";
    sequence_values = (float *)calloc((size_t)seq_hidden, sizeof(float));
    previous_output_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    layer_output_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    layer_reference_values = (float *)calloc((size_t)request->hidden_dim, sizeof(float));
    if (!sequence_values || !previous_output_values ||
        !layer_output_values || !layer_reference_values) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex graph layers",
                       "failed to allocate layer scheduler host buffers");
        exit_code = exit_for_status(YVEX_ERR_NOMEM);
        goto cleanup;
    }
    result->cleanup_attempted = 1;
    result->cleanup_status = "pass";

    fail_after_layer_0 = cli_test_env_enabled("YVEX_TEST_FAIL_LAYERS_AFTER_LAYER_0");
    fail_after_backend_alloc =
        cli_test_env_enabled("YVEX_TEST_FAIL_LAYERS_AFTER_BACKEND_ALLOC");

    for (layer = 0; layer < request->layers; ++layer) {
        block_fill_input(sequence_values, request->seq_len, request->hidden_dim);
        if (layer == 0ull && request->initial_position_values &&
            request->initial_position_value_count > 0ull) {
            copy_count = request->initial_position_value_count < request->hidden_dim
                             ? request->initial_position_value_count
                             : request->hidden_dim;
            memcpy(sequence_values + (request->position * request->hidden_dim),
                   request->initial_position_values,
                   (size_t)(copy_count * (unsigned long long)sizeof(float)));
        } else if (layer > 0ull) {
            memcpy(sequence_values + (request->position * request->hidden_dim),
                   previous_output_values, (size_t)hidden_bytes);
        }

        memset(&block_request, 0, sizeof(block_request));
        memset(&block_result, 0, sizeof(block_result));
        memset(layer_output_values, 0, (size_t)hidden_bytes);
        memset(layer_reference_values, 0, (size_t)hidden_bytes);
        block_request.backend_name = request->backend_name;
        block_request.seq_len = request->seq_len;
        block_request.position = request->position;
        block_request.hidden_dim = request->hidden_dim;
        block_request.head_dim = request->head_dim;
        block_request.ffn_dim = request->ffn_dim;
        block_request.activation = "silu";
        block_request.causal = request->causal;
        block_request.gated = request->gated;
        block_request.input_override_values = sequence_values;
        block_request.output_copy_values = layer_output_values;
        block_request.reference_copy_values = layer_reference_values;
        block_request.fail_after_backend_alloc =
            fail_after_backend_alloc && layer == 0ull;

        result->graph_execution_phase = "layer-dispatch";
        exit_code = controlled_block_execute_fixture(&block_request,
                                                     &block_result, err);
        result->backend_status = block_result.backend_status;
        result->backend_op_status = block_result.backend_op_status;
        result->scratch_planned_bytes_per_layer = block_result.scratch_planned_bytes;
        result->backend_allocated_bytes_total += block_result.backend_allocated_bytes;
        result->layer_output_checksums[layer] = block_result.output_checksum;
        result->layer_reference_checksums[layer] = block_result.reference_checksum;
        result->layer_max_abs_diffs[layer] = block_result.max_abs_diff;
        result->final_output_checksum = block_result.output_checksum;
        result->final_reference_checksum = block_result.reference_checksum;
        result->final_max_abs_diff = block_result.max_abs_diff;
        result->output_value_count =
            request->hidden_dim < YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES
                ? request->hidden_dim
                : YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES;
        for (copy_count = 0; copy_count < result->output_value_count; ++copy_count) {
            result->output_values[copy_count] = layer_output_values[copy_count];
        }
        if (strcmp(request->backend_name, "cuda") == 0 &&
            !block_result.block_cuda_parity) {
            result->layers_cuda_parity = 0;
        }
        if (block_result.cleanup_attempted) {
            result->cleanup_attempted = 1;
            result->cleanup_status = "pass";
        }
        if (exit_code != 0) {
            result->status = "graph-layers-failed-cleaned";
            result->graph_integrity_guard = "fail";
            result->graph_execution_phase = block_result.graph_execution_phase;
            goto cleanup;
        }
        memcpy(previous_output_values, layer_output_values, (size_t)hidden_bytes);
        if (fail_after_layer_0 && layer == 0ull) {
            yvex_error_set(err, YVEX_ERR_STATE, "yvex graph layers",
                           "test layer scheduler failure after layer 0");
            result->status = "graph-layers-failed-cleaned";
            result->graph_integrity_guard = "fail";
            result->graph_execution_phase = "layer-0-complete";
            exit_code = exit_for_status(YVEX_ERR_STATE);
            goto cleanup;
        }
    }

    result->status = "graph-layers";
    result->graph_integrity_guard = "pass";
    result->graph_execution_phase = "complete";
    exit_code = 0;

cleanup:
    if (sequence_values || previous_output_values ||
        layer_output_values || layer_reference_values) {
        result->cleanup_attempted = 1;
        result->cleanup_status = "pass";
    }
    free(layer_reference_values);
    free(layer_output_values);
    free(previous_output_values);
    free(sequence_values);
    if (exit_code == 0) {
        yvex_error_clear(err);
    }
    return exit_code;
}

int yvex_cli_graph_execute_layer_fixture(const yvex_cli_layer_fixture_options *options,
                                         yvex_cli_layer_fixture_result *out,
                                         yvex_error *err)
{
    yvex_controlled_layer_scheduler_request request;
    yvex_controlled_layer_scheduler_result result;
    unsigned long long seq_hidden;
    unsigned long long hidden_hidden;
    unsigned long long hidden_ffn;
    unsigned long long i;
    int exit_code;

    if (!options || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex graph layers",
                       "layer fixture options and output are required");
        return exit_for_status(YVEX_ERR_INVALID_ARG);
    }
    memset(out, 0, sizeof(*out));
    out->status = "graph-layers-fail";
    out->graph_integrity_guard = "fail";
    out->graph_execution_phase = "preflight";
    out->backend_status = "not-opened";
    out->backend_op_status = "unchecked";
    out->cleanup_status = "not-needed";

    if (!options->backend_name ||
        (strcmp(options->backend_name, "cpu") != 0 &&
         strcmp(options->backend_name, "cuda") != 0)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex graph layers",
                       "unknown backend kind");
        return exit_for_status(YVEX_ERR_INVALID_ARG);
    }
    if (options->layers == 0ull || options->layers > 16ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex graph layers",
                       "layer scheduler requires 1 <= layers <= 16");
        return exit_for_status(YVEX_ERR_INVALID_ARG);
    }
    if (options->seq_len == 0ull || options->hidden_dim == 0ull ||
        options->head_dim == 0ull || options->ffn_dim == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex graph layers",
                       "layer scheduler dimensions must be positive");
        return exit_for_status(YVEX_ERR_INVALID_ARG);
    }
    if (options->position >= options->seq_len) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex graph layers",
                       "layer scheduler position must be less than sequence length");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (options->hidden_dim % options->head_dim != 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex graph layers",
                       "layer scheduler hidden dimension must be divisible by head dimension");
        return exit_for_status(YVEX_ERR_INVALID_ARG);
    }
    if (options->hidden_dim != options->head_dim) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex graph layers",
                       "layer scheduler currently supports one attention head");
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    if (options->initial_position_value_count > options->hidden_dim) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex graph layers",
                       "initial selected-position row exceeds hidden dimension");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (options->seq_len > ULLONG_MAX / options->hidden_dim ||
        options->hidden_dim > ULLONG_MAX / options->hidden_dim ||
        options->hidden_dim > ULLONG_MAX / options->ffn_dim ||
        options->ffn_dim > ULLONG_MAX / options->hidden_dim ||
        options->layers > ULLONG_MAX / 12ull) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex graph layers",
                       "layer scheduler dimension overflow");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    seq_hidden = options->seq_len * options->hidden_dim;
    hidden_hidden = options->hidden_dim * options->hidden_dim;
    hidden_ffn = options->hidden_dim * options->ffn_dim;
    if (seq_hidden > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        hidden_hidden > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        hidden_ffn > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        (options->ffn_dim * options->hidden_dim) >
            (unsigned long long)(SIZE_MAX / sizeof(float))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex graph layers",
                       "layer scheduler byte count overflow");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    memset(&request, 0, sizeof(request));
    request.backend_name = options->backend_name;
    request.layers = options->layers;
    request.seq_len = options->seq_len;
    request.position = options->position;
    request.hidden_dim = options->hidden_dim;
    request.head_dim = options->head_dim;
    request.ffn_dim = options->ffn_dim;
    request.causal = 1;
    request.gated = 1;
    request.initial_position_values = options->initial_position_values;
    request.initial_position_value_count = options->initial_position_value_count;

    exit_code = controlled_layer_scheduler_execute(&request, &result, err);
    out->executed = exit_code == 0;
    out->status = result.status;
    out->graph_integrity_guard = result.graph_integrity_guard;
    out->graph_execution_phase = result.graph_execution_phase;
    out->backend_status = result.backend_status;
    out->backend_op_status = result.backend_op_status;
    out->layers = result.layers;
    out->total_op_count = result.total_op_count;
    out->output_bytes = result.output_bytes;
    out->scratch_bytes = result.scratch_planned_bytes_per_layer * result.layers;
    out->final_output_checksum = result.final_output_checksum;
    out->final_reference_checksum = result.final_reference_checksum;
    out->final_max_abs_diff = result.final_max_abs_diff;
    out->output_value_count = result.output_value_count;
    for (i = 0; i < result.output_value_count; ++i) {
        out->output_values[i] = result.output_values[i];
    }
    out->cleanup_attempted = result.cleanup_attempted;
    out->cleanup_status = result.cleanup_status;
    return exit_code;
}

static int command_graph_execute_layers_fixture(const char *backend_name,
                                                unsigned long long layers,
                                                unsigned long long seq_len,
                                                unsigned long long position,
                                                unsigned long long hidden_dim,
                                                unsigned long long head_dim,
                                                unsigned long long ffn_dim,
                                                int causal,
                                                int gated)
{
    yvex_controlled_layer_scheduler_request request;
    yvex_controlled_layer_scheduler_result result;
    yvex_error err;
    unsigned long long seq_hidden;
    unsigned long long hidden_hidden;
    unsigned long long hidden_ffn;
    int exit_code;

    yvex_error_clear(&err);
    if (layers == 0 || layers > 16ull) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler requires 1 <= --layers <= 16\n");
        return 2;
    }
    if (seq_len == 0 || hidden_dim == 0 || head_dim == 0 || ffn_dim == 0) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler dimensions must be positive\n");
        return 2;
    }
    if (position >= seq_len) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler position must be less than seq-len\n");
        return 2;
    }
    if (hidden_dim % head_dim != 0ull) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler hidden-dim must be divisible by head-dim\n");
        return 2;
    }
    if (hidden_dim != head_dim) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler currently supports one attention head\n");
        return 5;
    }
    if (!gated) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler requires gated MLP\n");
        return 2;
    }
    if (seq_len > ULLONG_MAX / hidden_dim ||
        hidden_dim > ULLONG_MAX / hidden_dim ||
        hidden_dim > ULLONG_MAX / ffn_dim ||
        ffn_dim > ULLONG_MAX / hidden_dim ||
        layers > ULLONG_MAX / 12ull) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler dimension overflow\n");
        return 4;
    }
    seq_hidden = seq_len * hidden_dim;
    hidden_hidden = hidden_dim * hidden_dim;
    hidden_ffn = hidden_dim * ffn_dim;
    if (seq_hidden > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        hidden_hidden > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        hidden_ffn > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        (ffn_dim * hidden_dim) > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        yvex_cli_out_writef(stderr, "yvex: layer scheduler byte count overflow\n");
        return 4;
    }

    memset(&request, 0, sizeof(request));
    request.backend_name = backend_name;
    request.layers = layers;
    request.seq_len = seq_len;
    request.position = position;
    request.hidden_dim = hidden_dim;
    request.head_dim = head_dim;
    request.ffn_dim = ffn_dim;
    request.causal = causal;
    request.gated = gated;

    exit_code = controlled_layer_scheduler_execute(&request, &result, &err);
    controlled_layer_scheduler_print(&result);
    if (exit_code != 0 && yvex_error_code(&err) != YVEX_OK) {
        return print_yvex_error(&err, exit_code);
    }
    return exit_code;
}

typedef struct {
    const char *backend_name;
    const char *suite_name;
    unsigned long long layers;
    int run_primitives;
    int run_block;
    int run_layers;
} yvex_graph_check_options;

typedef struct {
    const char *rope;
    const char *attention;
    const char *matmul;
    const char *mlp;
    const char *block;
    const char *layers;
    unsigned long long primitive_checks;
    unsigned long long block_checks;
    unsigned long long layer_checks;
    unsigned long long checks_passed;
    unsigned long long checks_failed;
    const char *failed_stage;
    int failed_exit_code;
    const char *final_status;
} yvex_graph_check_result;

static void graph_check_result_init(yvex_graph_check_result *result)
{
    memset(result, 0, sizeof(*result));
    result->rope = "skipped";
    result->attention = "skipped";
    result->matmul = "skipped";
    result->mlp = "skipped";
    result->block = "skipped";
    result->layers = "skipped";
    result->final_status = "graph-check-pass";
}

static void graph_check_note_stage(yvex_graph_check_result *result,
                                   const char **stage_status,
                                   const char *stage_name,
                                   int exit_code)
{
    if (exit_code == 0) {
        *stage_status = "pass";
        result->checks_passed++;
        return;
    }
    *stage_status = "fail";
    result->checks_failed++;
    if (!result->failed_stage) {
        result->failed_stage = stage_name;
        result->failed_exit_code = exit_code;
    }
}

static void graph_check_print_header(const yvex_graph_check_options *options)
{
    yvex_cli_out_writef(stdout, "status: graph-check\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", options->backend_name);
    yvex_cli_out_writef(stdout, "suite: %s\n", options->suite_name);
    yvex_cli_out_writef(stdout, "layers: %llu\n", options->layers);
    yvex_cli_out_writef(stdout, "graph_check_kind: fixture-proof-preset\n");
    yvex_cli_out_writef(stdout, "graph_check_scope: primitives-block-layers\n");
    yvex_cli_out_writef(stdout, "real_model_execution: false\n");
    yvex_cli_out_writef(stdout, "selected_artifact_execution: false\n");
    yvex_cli_out_writef(stdout, "prefill_execution: false\n");
}

static void graph_check_print_summary(const yvex_graph_check_result *result)
{
    yvex_cli_out_writef(stdout, "stage: rope %s\n", result->rope);
    yvex_cli_out_writef(stdout, "stage: attention %s\n", result->attention);
    yvex_cli_out_writef(stdout, "stage: matmul %s\n", result->matmul);
    yvex_cli_out_writef(stdout, "stage: mlp %s\n", result->mlp);
    yvex_cli_out_writef(stdout, "stage: block %s\n", result->block);
    yvex_cli_out_writef(stdout, "stage: layers %s\n", result->layers);
    yvex_cli_out_writef(stdout, "primitive_checks: %llu\n", result->primitive_checks);
    yvex_cli_out_writef(stdout, "block_checks: %llu\n", result->block_checks);
    yvex_cli_out_writef(stdout, "layer_checks: %llu\n", result->layer_checks);
    yvex_cli_out_writef(stdout, "checks_passed: %llu\n", result->checks_passed);
    yvex_cli_out_writef(stdout, "checks_failed: %llu\n", result->checks_failed);
    if (result->failed_stage) {
        yvex_cli_out_writef(stdout, "failed_stage: %s\n", result->failed_stage);
        yvex_cli_out_writef(stdout, "failed_exit_code: %d\n", result->failed_exit_code);
    }
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", result->final_status);
}

static int graph_check_backend_supported(const char *backend_name,
                                         const char **backend_status)
{
    yvex_backend_options options;
    yvex_backend *backend = NULL;
    yvex_error err;
    int supported;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.kind = graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc != YVEX_OK) {
        *backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unsupported" : "unavailable";
        yvex_error_clear(&err);
        return 0;
    }
    supported = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE) &&
                yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION) &&
                yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL) &&
                yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM) &&
                yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MLP);
    yvex_backend_close(backend);
    *backend_status = supported ? "ready" : "unsupported";
    return supported;
}

static int command_graph_execute_rope_op(const char *backend_name,
                                         unsigned long long position,
                                         unsigned long long head_dim);
static int command_graph_execute_attention_op(const char *backend_name,
                                              unsigned long long seq_len,
                                              unsigned long long position,
                                              unsigned long long head_dim,
                                              int causal);
static int command_graph_execute_matmul_op(const char *backend_name,
                                           unsigned long long m,
                                           unsigned long long k,
                                           unsigned long long n);
static int command_graph_execute_mlp_op(const char *backend_name,
                                        unsigned long long hidden_dim,
                                        unsigned long long ffn_dim,
                                        const char *activation,
                                        int gated,
                                        unsigned long long experts,
                                        unsigned long long expert_id,
                                        int use_expert);

static int command_graph_check(int argc, char **argv)
{
    yvex_graph_check_options options;
    yvex_graph_check_result result;
    const char *backend_status = "ready";
    int rc;
    int i;

    memset(&options, 0, sizeof(options));
    options.backend_name = "cpu";
    options.suite_name = "all";
    options.layers = 2ull;

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: graph check --backend requires cpu|cuda\n");
                return 2;
            }
            options.backend_name = argv[++i];
        } else if (strcmp(argv[i], "--suite") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: graph check --suite requires primitives, block, layers, or all\n");
                return 2;
            }
            options.suite_name = argv[++i];
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.layers)) {
                yvex_cli_out_writef(stderr, "yvex: graph check --layers requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--model") == 0) {
            yvex_cli_out_writef(stderr, "yvex: graph check preset is fixture-only in GRAPH.CHECK.0\n");
            return 2;
        } else if (argv[i][0] == '-') {
            yvex_cli_out_writef(stderr, "yvex: unknown graph check option: %s\n", argv[i]);
            return 2;
        } else {
            yvex_cli_out_writef(stderr, "yvex: graph check preset is fixture-only in GRAPH.CHECK.0\n");
            return 2;
        }
    }
    if (!graph_backend_valid(options.backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", options.backend_name);
        return 2;
    }
    if (strcmp(options.suite_name, "primitives") == 0) {
        options.run_primitives = 1;
    } else if (strcmp(options.suite_name, "block") == 0) {
        options.run_block = 1;
    } else if (strcmp(options.suite_name, "layers") == 0) {
        options.run_layers = 1;
    } else if (strcmp(options.suite_name, "all") == 0) {
        options.run_primitives = 1;
        options.run_block = 1;
        options.run_layers = 1;
    } else {
        yvex_cli_out_writef(stderr, "yvex: graph check --suite requires primitives, block, layers, or all\n");
        return 2;
    }
    if (options.layers == 0ull || options.layers > 16ull) {
        yvex_cli_out_writef(stderr, "yvex: graph check requires 1 <= --layers <= 16\n");
        return 2;
    }

    graph_check_result_init(&result);
    result.primitive_checks = options.run_primitives ? 4ull : 0ull;
    result.block_checks = options.run_block ? 1ull : 0ull;
    result.layer_checks = options.run_layers ? 1ull : 0ull;
    graph_check_print_header(&options);

    if (!graph_check_backend_supported(options.backend_name, &backend_status)) {
        yvex_cli_out_writef(stdout, "backend_status: %s\n", backend_status);
        result.final_status = "graph-check-unsupported";
        graph_check_print_summary(&result);
        return 5;
    }

    if (options.run_primitives) {
        rc = command_graph_execute_rope_op(options.backend_name, 7ull, 8ull);
        graph_check_note_stage(&result, &result.rope, "rope", rc);
        rc = command_graph_execute_attention_op(options.backend_name, 4ull, 3ull, 8ull, 1);
        graph_check_note_stage(&result, &result.attention, "attention", rc);
        rc = command_graph_execute_matmul_op(options.backend_name, 1ull, 8ull, 8ull);
        graph_check_note_stage(&result, &result.matmul, "matmul", rc);
        rc = command_graph_execute_mlp_op(options.backend_name, 8ull, 16ull,
                                          "silu", 1, 0ull, 0ull, 0);
        graph_check_note_stage(&result, &result.mlp, "mlp", rc);
    }
    if (options.run_block) {
        rc = command_graph_execute_block_fixture(options.backend_name,
                                                 4ull, 3ull, 8ull, 8ull,
                                                 16ull, 1, 1);
        graph_check_note_stage(&result, &result.block, "block", rc);
    }
    if (options.run_layers) {
        rc = command_graph_execute_layers_fixture(options.backend_name,
                                                  options.layers,
                                                  4ull, 3ull, 8ull, 8ull,
                                                  16ull, 1, 1);
        graph_check_note_stage(&result, &result.layers, "layers", rc);
    }

    result.final_status =
        result.checks_failed == 0ull ? "graph-check-pass" : "graph-check-fail";
    graph_check_print_summary(&result);
    return result.checks_failed == 0ull ? 0 : 1;
}

/* Standalone primitive graph proof executors. */

static int command_graph_execute_rope_op(const char *backend_name,
                                         unsigned long long position,
                                         unsigned long long head_dim)
{
    const float rope_base = 10000.0f;
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long bytes = 0ull;
    unsigned long long sample_count;
    float max_abs_diff = 0.0f;
    float input_output_max_abs_diff = 0.0f;
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_standalone_graph_guard(&guard, "rope-position-op", "not-needed");

    if (head_dim == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: rope-head-dim-zero\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if ((head_dim & 1ull) != 0ull) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: rope-head-dim-odd\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (cli_test_env_enabled("YVEX_TEST_ROPE_BYTE_OVERFLOW") ||
        head_dim > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        head_dim > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    bytes = head_dim * (unsigned long long)sizeof(float);
    guard.shape_status = "pass";
    guard.range_status = "not-applicable";
    guard.output_bytes_planned = bytes;
    guard.reference_bytes_planned = bytes;

    input_values = (float *)malloc((size_t)bytes);
    output_values = (float *)malloc((size_t)bytes);
    reference_values = (float *)malloc((size_t)bytes);
    if (!input_values || !output_values || !reference_values) {
        free(reference_values);
        free(output_values);
        free(input_values);
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph rope", "failed to allocate host buffers");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
    }
    cli_rope_fill_input(input_values, head_dim);
    cli_rope_reference(input_values, head_dim, position, rope_base, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(rc);
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE)) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: backend-op-rope-unsupported\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    guard.backend_op_status = "supported";

    desc.name = "rope.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 1;
    desc.dims[0] = head_dim;
    desc.bytes = bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc == YVEX_OK) {
        desc.name = "rope.output";
        rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    }
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", input || out);
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, bytes, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (cli_test_env_enabled("YVEX_TEST_FAIL_ROPE_AFTER_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        yvex_cli_out_writef(stdout, "input_bytes: %llu\n", bytes);
        yvex_cli_out_writef(stdout, "output_bytes: %llu\n", bytes);
        yvex_cli_out_writef(stdout, "reference_bytes: %llu\n", bytes);
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: injected-rope-after-alloc\n");
        yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(YVEX_ERR_STATE);
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_rope(backend, input, position, rope_base, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ROPE_AFTER_DISPATCH")) {
        graph_guard_mark_cleanup(&guard, "dispatch", 1);
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        yvex_cli_out_writef(stdout, "input_bytes: %llu\n", bytes);
        yvex_cli_out_writef(stdout, "output_bytes: %llu\n", bytes);
        yvex_cli_out_writef(stdout, "reference_bytes: %llu\n", bytes);
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "reason: %s\n",
               rc == YVEX_OK ? "injected-rope-after-dispatch" : yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return rc == YVEX_OK ? exit_for_status(YVEX_ERR_STATE) : exit_for_status(rc);
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, bytes, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "reference", 1);
        print_graph_guard_report(&guard);
        yvex_cli_out_writef(stdout, "op: rope\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        yvex_cli_out_writef(stdout, "position: %llu\n", position);
        yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
        yvex_cli_out_writef(stdout, "dtype: f32\n");
        print_rope_readiness_fields();
        yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, head_dim);
    input_output_max_abs_diff = cli_max_abs_diff_f32(output_values, input_values, head_dim);
    guard.guard_status = max_abs_diff <= 0.0005f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = head_dim < 8ull ? head_dim : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    yvex_cli_out_writef(stdout, "op: rope\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    yvex_cli_out_writef(stdout, "position: %llu\n", position);
    yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
    yvex_cli_out_writef(stdout, "rope_base: %.9g\n", (double)rope_base);
    yvex_cli_out_writef(stdout, "dtype: f32\n");
    yvex_cli_out_writef(stdout, "input_bytes: %llu\n", bytes);
    yvex_cli_out_writef(stdout, "output_bytes: %llu\n", bytes);
    yvex_cli_out_writef(stdout, "scratch_bytes: 0\n");
    yvex_cli_out_writef(stdout, "reference_bytes: %llu\n", bytes);
    yvex_cli_out_writef(stdout, "input_checksum: %llu\n", cli_checksum_bytes(input_values, bytes));
    yvex_cli_out_writef(stdout, "output_checksum: %llu\n", cli_checksum_bytes(output_values, bytes));
    yvex_cli_out_writef(stdout, "reference_checksum: %llu\n", cli_checksum_bytes(reference_values, bytes));
    yvex_cli_out_writef(stdout, "max_abs_diff: %.9g\n", (double)max_abs_diff);
    yvex_cli_out_writef(stdout, "input_output_max_abs_diff: %.9g\n", (double)input_output_max_abs_diff);
    yvex_cli_out_writef(stdout, "position_zero_identity: %s\n",
           position == 0ull && input_output_max_abs_diff <= 0.0000001f ? "true" : "false");
    yvex_cli_out_writef(stdout, "position_dependent_output: %s\n",
           position != 0ull && input_output_max_abs_diff > 0.0001f ? "true" : "false");
    yvex_cli_out_writef(stdout, "reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        yvex_cli_out_writef(stdout, "rope_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        yvex_cli_out_writef(stdout, "cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        yvex_cli_out_writef(stdout, "cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    yvex_cli_out_writef(stdout, "output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values, sample_count);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_rope_readiness_fields();
    yvex_cli_out_writef(stdout, "status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    free(reference_values);
    free(output_values);
    free(input_values);
    return exit_code;
}

static void print_attention_operation_fields(const char *backend_name,
                                             unsigned long long seq_len,
                                             unsigned long long position,
                                             unsigned long long head_dim,
                                             float scale,
                                             int causal,
                                             unsigned long long query_bytes,
                                             unsigned long long key_bytes,
                                             unsigned long long value_bytes,
                                             unsigned long long input_bytes,
                                             unsigned long long score_scratch_bytes,
                                             unsigned long long probability_scratch_bytes,
                                             unsigned long long output_bytes,
                                             unsigned long long reference_bytes)
{
    yvex_cli_out_writef(stdout, "op: attention\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    yvex_cli_out_writef(stdout, "dtype: f32\n");
    yvex_cli_out_writef(stdout, "seq_len: %llu\n", seq_len);
    yvex_cli_out_writef(stdout, "position: %llu\n", position);
    yvex_cli_out_writef(stdout, "head_dim: %llu\n", head_dim);
    yvex_cli_out_writef(stdout, "scale: %.9g\n", (double)scale);
    yvex_cli_out_writef(stdout, "mask: %s\n", causal ? "causal" : "none");
    yvex_cli_out_writef(stdout, "query_bytes: %llu\n", query_bytes);
    yvex_cli_out_writef(stdout, "key_bytes: %llu\n", key_bytes);
    yvex_cli_out_writef(stdout, "value_bytes: %llu\n", value_bytes);
    yvex_cli_out_writef(stdout, "input_bytes: %llu\n", input_bytes);
    yvex_cli_out_writef(stdout, "score_scratch_bytes: %llu\n", score_scratch_bytes);
    yvex_cli_out_writef(stdout, "probability_scratch_bytes: %llu\n", probability_scratch_bytes);
    yvex_cli_out_writef(stdout, "scratch_bytes: %llu\n", score_scratch_bytes + probability_scratch_bytes);
    yvex_cli_out_writef(stdout, "output_bytes: %llu\n", output_bytes);
    yvex_cli_out_writef(stdout, "reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_attention_op(const char *backend_name,
                                              unsigned long long seq_len,
                                              unsigned long long position,
                                              unsigned long long head_dim,
                                              int causal)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *query = NULL;
    yvex_device_tensor *keys = NULL;
    yvex_device_tensor *values = NULL;
    yvex_device_tensor *score_scratch = NULL;
    yvex_device_tensor *probability_scratch = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *query_values = NULL;
    float *key_values = NULL;
    float *value_values = NULL;
    float *score_values = NULL;
    float *probability_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    float *reference_scores = NULL;
    float *reference_probabilities = NULL;
    unsigned long long kv_elements = 0ull;
    unsigned long long query_bytes = 0ull;
    unsigned long long key_bytes = 0ull;
    unsigned long long value_bytes = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long score_scratch_bytes = 0ull;
    unsigned long long probability_scratch_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long visible_keys = 0ull;
    unsigned long long masked_keys = 0ull;
    unsigned long long sample_count = 0ull;
    float scale = 0.0f;
    float max_abs_diff = 0.0f;
    float probability_max_abs_diff = 0.0f;
    float masked_probability_max = 0.0f;
    float position_zero_value_diff = 0.0f;
    const char *reason = NULL;
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_standalone_graph_guard(&guard, "attention-primitive", "unchecked");

    if (head_dim == 0 || seq_len == 0) {
        guard.shape_status = "fail";
        reason = head_dim == 0 ? "attention-head-dim-zero" : "attention-seq-len-zero";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, 0.0f,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason);
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    scale = (float)(1.0 / cli_sqrt_double((double)head_dim));
    if (position >= seq_len) {
        guard.shape_status = "fail";
        guard.slice_range_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: position-out-of-range\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (cli_test_env_enabled("YVEX_TEST_ATTENTION_BYTE_OVERFLOW") ||
        seq_len > ULLONG_MAX / head_dim ||
        head_dim > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        seq_len > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    kv_elements = seq_len * head_dim;
    if (kv_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        kv_elements > ULLONG_MAX / (unsigned long long)sizeof(float)) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    query_bytes = head_dim * (unsigned long long)sizeof(float);
    key_bytes = kv_elements * (unsigned long long)sizeof(float);
    value_bytes = key_bytes;
    score_scratch_bytes = seq_len * (unsigned long long)sizeof(float);
    probability_scratch_bytes = score_scratch_bytes;
    output_bytes = query_bytes;
    reference_bytes = output_bytes;
    if (query_bytes > ULLONG_MAX - key_bytes ||
        query_bytes + key_bytes > ULLONG_MAX - value_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes, 0,
                                         score_scratch_bytes, probability_scratch_bytes,
                                         output_bytes, reference_bytes);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = query_bytes + key_bytes + value_bytes;
    visible_keys = causal ? position + 1ull : seq_len;
    masked_keys = seq_len - visible_keys;
    guard.shape_status = "pass";
    guard.slice_range_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    query_values = (float *)malloc((size_t)query_bytes);
    key_values = (float *)malloc((size_t)key_bytes);
    value_values = (float *)malloc((size_t)value_bytes);
    score_values = (float *)malloc((size_t)score_scratch_bytes);
    probability_values = (float *)malloc((size_t)probability_scratch_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    reference_scores = (float *)malloc((size_t)score_scratch_bytes);
    reference_probabilities = (float *)malloc((size_t)probability_scratch_bytes);
    if (!query_values || !key_values || !value_values || !score_values ||
        !probability_values || !output_values || !reference_values ||
        !reference_scores || !reference_probabilities) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph attention",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_attention_fill_inputs(query_values, key_values, value_values, seq_len, head_dim);
    cli_attention_reference(query_values, key_values, value_values, seq_len, position,
                            head_dim, scale, causal, reference_scores,
                            reference_probabilities, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes,
                                         input_bytes, score_scratch_bytes,
                                         probability_scratch_bytes, output_bytes,
                                         reference_bytes);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "visible_keys: %llu\n", visible_keys);
        yvex_cli_out_writef(stdout, "masked_keys: %llu\n", masked_keys);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason);
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION)) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes,
                                         input_bytes, score_scratch_bytes,
                                         probability_scratch_bytes, output_bytes,
                                         reference_bytes);
        print_attention_readiness_fields(0);
        yvex_cli_out_writef(stdout, "visible_keys: %llu\n", visible_keys);
        yvex_cli_out_writef(stdout, "masked_keys: %llu\n", masked_keys);
        yvex_cli_out_writef(stdout, "reason: backend-op-attention-unsupported\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "attention.query";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 1;
    desc.dims[0] = head_dim;
    desc.bytes = query_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &query, &err);
    if (rc == YVEX_OK) {
        desc.name = "attention.keys";
        desc.rank = 2;
        desc.dims[0] = seq_len;
        desc.dims[1] = head_dim;
        desc.bytes = key_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &keys, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.values";
        desc.bytes = value_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &values, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.scores";
        desc.rank = 1;
        desc.dims[0] = seq_len;
        desc.dims[1] = 0;
        desc.bytes = score_scratch_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &score_scratch, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.probabilities";
        desc.bytes = probability_scratch_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &probability_scratch, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.output";
        desc.dims[0] = head_dim;
        desc.bytes = output_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    }
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard,
                                 "output",
                                 query || keys || values || score_scratch ||
                                     probability_scratch || out);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, query, query_values, query_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, keys, key_values, key_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, values, value_values, value_bytes, &err);
    }
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }

    if (cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-attention-after-alloc";
        goto fail_cleaned;
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_attention(backend, query, keys, values, seq_len, position,
                                   scale, causal, score_scratch,
                                   probability_scratch, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_DISPATCH")) {
        graph_guard_mark_cleanup(&guard, "dispatch", 1);
        reason = rc == YVEX_OK ? "injected-attention-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, score_scratch, score_values, score_scratch_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, probability_scratch, probability_values,
                                      probability_scratch_bytes, &err);
    }
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_REFERENCE")) {
        graph_guard_mark_cleanup(&guard, "reference", 1);
        reason = rc == YVEX_OK ? "injected-attention-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, head_dim);
    probability_max_abs_diff = cli_max_abs_diff_f32(probability_values,
                                                    reference_probabilities,
                                                    seq_len);
    if (causal) {
        unsigned long long mask_index;
        for (mask_index = position + 1ull; mask_index < seq_len; ++mask_index) {
            float p = cli_abs_float(probability_values[mask_index]);
            if (p > masked_probability_max) {
                masked_probability_max = p;
            }
        }
    }
    position_zero_value_diff = cli_max_abs_diff_f32(output_values,
                                                    value_values,
                                                    head_dim);
    guard.guard_status = (max_abs_diff <= 0.001f && probability_max_abs_diff <= 0.001f)
                             ? "pass"
                             : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = head_dim < 8ull ? head_dim : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                     causal, query_bytes, key_bytes, value_bytes,
                                     input_bytes, score_scratch_bytes,
                                     probability_scratch_bytes, output_bytes,
                                     reference_bytes);
    yvex_cli_out_writef(stdout, "visible_keys: %llu\n", visible_keys);
    yvex_cli_out_writef(stdout, "masked_keys: %llu\n", masked_keys);
    yvex_cli_out_writef(stdout, "causal_prefix_keys: %llu\n", visible_keys);
    yvex_cli_out_writef(stdout, "causal_mask_future_prob_zero: %s\n",
           !causal || masked_probability_max <= 0.000001f ? "true" : "false");
    yvex_cli_out_writef(stdout, "masked_probability_max: %.9g\n", (double)masked_probability_max);
    yvex_cli_out_writef(stdout, "position_zero_single_key: %s\n",
           position == 0ull && position_zero_value_diff <= 0.000001f ? "true" : "false");
    yvex_cli_out_writef(stdout, "last_position_full_prefix: %s\n",
           causal && position + 1ull == seq_len ? "true" : "false");
    yvex_cli_out_writef(stdout, "query_checksum: %llu\n", cli_checksum_bytes(query_values, query_bytes));
    yvex_cli_out_writef(stdout, "key_checksum: %llu\n", cli_checksum_bytes(key_values, key_bytes));
    yvex_cli_out_writef(stdout, "value_checksum: %llu\n", cli_checksum_bytes(value_values, value_bytes));
    yvex_cli_out_writef(stdout, "score_checksum: %llu\n", cli_checksum_bytes(score_values, score_scratch_bytes));
    yvex_cli_out_writef(stdout, "probability_checksum: %llu\n", cli_checksum_bytes(probability_values, probability_scratch_bytes));
    yvex_cli_out_writef(stdout, "output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    yvex_cli_out_writef(stdout, "reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    yvex_cli_out_writef(stdout, "max_abs_diff: %.9g\n", (double)max_abs_diff);
    yvex_cli_out_writef(stdout, "softmax_max_abs_diff: %.9g\n", (double)probability_max_abs_diff);
    yvex_cli_out_writef(stdout, "reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        yvex_cli_out_writef(stdout, "attention_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        yvex_cli_out_writef(stdout, "cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        yvex_cli_out_writef(stdout, "cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    yvex_cli_out_writef(stdout, "output_sample_count: %llu\n", sample_count);
    cli_print_float_values("query_sample_values", query_values, sample_count);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_attention_readiness_fields(exit_code == 0);
    yvex_cli_out_writef(stdout, "status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                     causal, query_bytes, key_bytes, value_bytes,
                                     input_bytes, score_scratch_bytes,
                                     probability_scratch_bytes, output_bytes,
                                     reference_bytes);
    yvex_cli_out_writef(stdout, "visible_keys: %llu\n", visible_keys);
    yvex_cli_out_writef(stdout, "masked_keys: %llu\n", masked_keys);
    print_attention_readiness_fields(0);
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "attention-op-failed");
    yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, probability_scratch);
        yvex_backend_tensor_free(backend, score_scratch);
        yvex_backend_tensor_free(backend, values);
        yvex_backend_tensor_free(backend, keys);
        yvex_backend_tensor_free(backend, query);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_probabilities);
    free(reference_scores);
    free(reference_values);
    free(output_values);
    free(probability_values);
    free(score_values);
    free(value_values);
    free(key_values);
    free(query_values);
    return exit_code;
}

static void print_matmul_operation_fields(const char *backend_name,
                                          unsigned long long m,
                                          unsigned long long k,
                                          unsigned long long n,
                                          unsigned long long input_bytes,
                                          unsigned long long weight_bytes,
                                          unsigned long long output_bytes,
                                          unsigned long long reference_bytes)
{
    yvex_cli_out_writef(stdout, "op: matmul\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    yvex_cli_out_writef(stdout, "dtype: f32\n");
    yvex_cli_out_writef(stdout, "m: %llu\n", m);
    yvex_cli_out_writef(stdout, "k: %llu\n", k);
    yvex_cli_out_writef(stdout, "n: %llu\n", n);
    yvex_cli_out_writef(stdout, "projection_shape: %s\n", m == 1ull ? "true" : "false");
    yvex_cli_out_writef(stdout, "non_projection_shape: %s\n", m == 1ull ? "false" : "true");
    yvex_cli_out_writef(stdout, "input_bytes: %llu\n", input_bytes);
    yvex_cli_out_writef(stdout, "weight_bytes: %llu\n", weight_bytes);
    yvex_cli_out_writef(stdout, "output_bytes: %llu\n", output_bytes);
    yvex_cli_out_writef(stdout, "scratch_bytes: 0\n");
    yvex_cli_out_writef(stdout, "reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_matmul_op(const char *backend_name,
                                           unsigned long long m,
                                           unsigned long long k,
                                           unsigned long long n)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *weight = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *weight_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long input_elements = 0ull;
    unsigned long long weight_elements = 0ull;
    unsigned long long output_elements = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long weight_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long total_input_bytes = 0ull;
    unsigned long long sample_count = 0ull;
    float max_abs_diff = 0.0f;
    const char *reason = NULL;
    int rc = YVEX_OK;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_standalone_graph_guard(&guard,
                                m == 1ull ? "matmul-projection" : "matmul-matrix",
                                "not-needed");

    if (m == 0 || k == 0 || n == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: matmul-zero-dimension\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (cli_test_env_enabled("YVEX_TEST_MATMUL_BYTE_OVERFLOW") ||
        m > ULLONG_MAX / k ||
        k > ULLONG_MAX / n ||
        m > ULLONG_MAX / n) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_elements = m * k;
    weight_elements = k * n;
    output_elements = m * n;
    if (input_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        weight_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        output_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        input_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        weight_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        output_elements > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = input_elements * (unsigned long long)sizeof(float);
    weight_bytes = weight_elements * (unsigned long long)sizeof(float);
    output_bytes = output_elements * (unsigned long long)sizeof(float);
    reference_bytes = output_bytes;
    if (input_bytes > ULLONG_MAX - weight_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    total_input_bytes = input_bytes + weight_bytes;
    guard.shape_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    input_values = (float *)malloc((size_t)input_bytes);
    weight_values = (float *)malloc((size_t)weight_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    if (!input_values || !weight_values || !output_values || !reference_values) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph matmul",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_matmul_fill_inputs(input_values, weight_values, m, k, n);
    cli_matmul_reference(input_values, weight_values, m, k, n, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason);
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL) ||
        cli_test_env_enabled("YVEX_TEST_MATMUL_BACKEND_OP_UNSUPPORTED")) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: backend-op-matmul-unsupported\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "matmul.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 2;
    desc.dims[0] = m;
    desc.dims[1] = k;
    desc.bytes = input_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", input != NULL);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_INPUT_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-matmul-after-input-alloc";
        goto fail_cleaned;
    }

    desc.name = "matmul.weight";
    desc.dims[0] = k;
    desc.dims[1] = n;
    desc.bytes = weight_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_WEIGHT_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-matmul-after-weight-alloc";
        goto fail_cleaned;
    }

    desc.name = "matmul.output";
    desc.dims[0] = m;
    desc.dims[1] = n;
    desc.bytes = output_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, input_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, weight, weight_values, weight_bytes, &err);
    }
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_OUTPUT_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-matmul-after-output-alloc";
        goto fail_cleaned;
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_matmul(backend, input, weight, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_DISPATCH")) {
        graph_guard_mark_cleanup(&guard, "dispatch", 1);
        reason = rc == YVEX_OK ? "injected-matmul-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_REFERENCE")) {
        graph_guard_mark_cleanup(&guard, "reference", 1);
        reason = rc == YVEX_OK ? "injected-matmul-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, output_elements);
    guard.guard_status = max_abs_diff <= 0.001f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = output_elements < 8ull ? output_elements : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_matmul_operation_fields(backend_name, m, k, n,
                                  input_bytes, weight_bytes,
                                  output_bytes, reference_bytes);
    yvex_cli_out_writef(stdout, "input_elements: %llu\n", input_elements);
    yvex_cli_out_writef(stdout, "weight_elements: %llu\n", weight_elements);
    yvex_cli_out_writef(stdout, "output_elements: %llu\n", output_elements);
    yvex_cli_out_writef(stdout, "input_total_bytes: %llu\n", total_input_bytes);
    yvex_cli_out_writef(stdout, "input_checksum: %llu\n", cli_checksum_bytes(input_values, input_bytes));
    yvex_cli_out_writef(stdout, "weight_checksum: %llu\n", cli_checksum_bytes(weight_values, weight_bytes));
    yvex_cli_out_writef(stdout, "output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    yvex_cli_out_writef(stdout, "reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    yvex_cli_out_writef(stdout, "max_abs_diff: %.9g\n", (double)max_abs_diff);
    yvex_cli_out_writef(stdout, "reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        yvex_cli_out_writef(stdout, "matmul_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        yvex_cli_out_writef(stdout, "cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        yvex_cli_out_writef(stdout, "cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    yvex_cli_out_writef(stdout, "output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values,
                           input_elements < 8ull ? input_elements : 8ull);
    cli_print_float_values("weight_sample_values", weight_values,
                           weight_elements < 8ull ? weight_elements : 8ull);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_matmul_readiness_fields(exit_code == 0);
    yvex_cli_out_writef(stdout, "status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_matmul_operation_fields(backend_name, m, k, n,
                                  input_bytes, weight_bytes,
                                  output_bytes, reference_bytes);
    yvex_cli_out_writef(stdout, "input_elements: %llu\n", input_elements);
    yvex_cli_out_writef(stdout, "weight_elements: %llu\n", weight_elements);
    yvex_cli_out_writef(stdout, "output_elements: %llu\n", output_elements);
    yvex_cli_out_writef(stdout, "input_total_bytes: %llu\n", total_input_bytes);
    print_matmul_readiness_fields(0);
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "matmul-op-failed");
    yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, weight);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_values);
    free(output_values);
    free(weight_values);
    free(input_values);
    return exit_code;
}

static void print_mlp_operation_fields(const char *backend_name,
                                       unsigned long long batch,
                                       unsigned long long hidden_dim,
                                       unsigned long long ffn_dim,
                                       const char *activation,
                                       int gated,
                                       int routed,
                                       unsigned long long expert_count,
                                       unsigned long long expert_id,
                                       unsigned long long input_bytes,
                                       unsigned long long gate_bytes,
                                       unsigned long long up_bytes,
                                       unsigned long long down_bytes,
                                       unsigned long long intermediate_bytes,
                                       unsigned long long output_bytes,
                                       unsigned long long reference_bytes)
{
    yvex_cli_out_writef(stdout, "op: mlp\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    yvex_cli_out_writef(stdout, "dtype: f32\n");
    yvex_cli_out_writef(stdout, "batch: %llu\n", batch);
    yvex_cli_out_writef(stdout, "hidden_dim: %llu\n", hidden_dim);
    yvex_cli_out_writef(stdout, "ffn_dim: %llu\n", ffn_dim);
    yvex_cli_out_writef(stdout, "activation: %s\n", activation ? activation : "unknown");
    yvex_cli_out_writef(stdout, "gated: %s\n", gated ? "true" : "false");
    yvex_cli_out_writef(stdout, "routed_expert_mode: %s\n", routed ? "true" : "false");
    yvex_cli_out_writef(stdout, "expert_count: %llu\n", expert_count);
    yvex_cli_out_writef(stdout, "expert_id: %llu\n", expert_id);
    yvex_cli_out_writef(stdout, "input_bytes: %llu\n", input_bytes);
    yvex_cli_out_writef(stdout, "gate_weight_bytes: %llu\n", gate_bytes);
    yvex_cli_out_writef(stdout, "up_weight_bytes: %llu\n", up_bytes);
    yvex_cli_out_writef(stdout, "down_weight_bytes: %llu\n", down_bytes);
    yvex_cli_out_writef(stdout, "intermediate_bytes: %llu\n", intermediate_bytes);
    yvex_cli_out_writef(stdout, "output_bytes: %llu\n", output_bytes);
    yvex_cli_out_writef(stdout, "reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_mlp_op(const char *backend_name,
                                        unsigned long long hidden_dim,
                                        unsigned long long ffn_dim,
                                        const char *activation,
                                        int gated,
                                        unsigned long long expert_count,
                                        unsigned long long expert_id,
                                        int routed)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *gate = NULL;
    yvex_device_tensor *up = NULL;
    yvex_device_tensor *down = NULL;
    yvex_device_tensor *intermediate = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_mlp_options options;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *gate_values = NULL;
    float *up_values = NULL;
    float *down_values = NULL;
    float *intermediate_values = NULL;
    float *reference_intermediate_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long batch = 1ull;
    unsigned long long actual_experts = routed ? expert_count : 1ull;
    unsigned long long input_elements = 0ull;
    unsigned long long gate_elements = 0ull;
    unsigned long long up_elements = 0ull;
    unsigned long long down_elements = 0ull;
    unsigned long long intermediate_elements = 0ull;
    unsigned long long output_elements = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long gate_bytes = 0ull;
    unsigned long long up_bytes = 0ull;
    unsigned long long down_bytes = 0ull;
    unsigned long long intermediate_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long total_weight_bytes = 0ull;
    unsigned long long total_input_bytes = 0ull;
    unsigned long long sample_count = 0ull;
    float max_abs_diff = 0.0f;
    const char *reason = NULL;
    int rc = YVEX_OK;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    memset(&options, 0, sizeof(options));
    init_standalone_graph_guard(&guard,
                                routed ? "mlp-routed-expert" : "mlp-feed-forward",
                                "not-needed");

    if (hidden_dim == 0 || ffn_dim == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: mlp-zero-dimension\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (!gated || !activation || strcmp(activation, "silu") != 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: unsupported-activation\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    if (routed && (expert_count == 0 || expert_id >= expert_count)) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: expert-id-out-of-range\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (cli_test_env_enabled("YVEX_TEST_MLP_BYTE_OVERFLOW") ||
        hidden_dim > ULLONG_MAX / ffn_dim ||
        actual_experts > ULLONG_MAX / hidden_dim ||
        actual_experts * hidden_dim > ULLONG_MAX / ffn_dim ||
        actual_experts > ULLONG_MAX / ffn_dim ||
        actual_experts * ffn_dim > ULLONG_MAX / hidden_dim) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    input_elements = batch * hidden_dim;
    gate_elements = actual_experts * hidden_dim * ffn_dim;
    up_elements = gate_elements;
    down_elements = actual_experts * ffn_dim * hidden_dim;
    intermediate_elements = batch * ffn_dim;
    output_elements = input_elements;
    if (input_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        gate_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        up_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        down_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        intermediate_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        output_elements > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, 0, 0, 0, 0, 0, 0, 0);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = input_elements * (unsigned long long)sizeof(float);
    gate_bytes = gate_elements * (unsigned long long)sizeof(float);
    up_bytes = up_elements * (unsigned long long)sizeof(float);
    down_bytes = down_elements * (unsigned long long)sizeof(float);
    intermediate_bytes = intermediate_elements * (unsigned long long)sizeof(float);
    output_bytes = output_elements * (unsigned long long)sizeof(float);
    reference_bytes = output_bytes;
    if (gate_bytes > ULLONG_MAX - up_bytes ||
        gate_bytes + up_bytes > ULLONG_MAX - down_bytes ||
        input_bytes > ULLONG_MAX - gate_bytes ||
        input_bytes + gate_bytes > ULLONG_MAX - up_bytes ||
        input_bytes + gate_bytes + up_bytes > ULLONG_MAX - down_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, input_bytes, gate_bytes, up_bytes,
                                   down_bytes, intermediate_bytes, output_bytes,
                                   reference_bytes);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: byte-count-overflow\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    total_weight_bytes = gate_bytes + up_bytes + down_bytes;
    total_input_bytes = input_bytes + total_weight_bytes;
    guard.shape_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    input_values = (float *)malloc((size_t)input_bytes);
    gate_values = (float *)malloc((size_t)gate_bytes);
    up_values = (float *)malloc((size_t)up_bytes);
    down_values = (float *)malloc((size_t)down_bytes);
    intermediate_values = (float *)malloc((size_t)intermediate_bytes);
    reference_intermediate_values = (float *)malloc((size_t)intermediate_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    if (!input_values || !gate_values || !up_values || !down_values ||
        !intermediate_values || !reference_intermediate_values ||
        !output_values || !reference_values) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph mlp",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_mlp_fill_inputs(input_values, gate_values, up_values, down_values,
                        batch, hidden_dim, ffn_dim, actual_experts, routed);
    cli_mlp_reference(input_values, gate_values, up_values, down_values,
                      batch, hidden_dim, ffn_dim, expert_id, routed,
                      reference_intermediate_values, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, input_bytes, gate_bytes, up_bytes,
                                   down_bytes, intermediate_bytes, output_bytes,
                                   reference_bytes);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason);
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MLP) ||
        cli_test_env_enabled("YVEX_TEST_MLP_BACKEND_OP_UNSUPPORTED")) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                                   activation, gated, routed, expert_count,
                                   expert_id, input_bytes, gate_bytes, up_bytes,
                                   down_bytes, intermediate_bytes, output_bytes,
                                   reference_bytes);
        print_mlp_readiness_fields(0);
        yvex_cli_out_writef(stdout, "reason: backend-op-mlp-unsupported\n");
        yvex_cli_out_writef(stdout, "status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "mlp.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 2;
    desc.dims[0] = batch;
    desc.dims[1] = hidden_dim;
    desc.dims[2] = 0;
    desc.bytes = input_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", input != NULL);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_INPUT_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-mlp-after-input-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.gate_weight";
    desc.rank = routed ? 3 : 2;
    desc.dims[0] = routed ? actual_experts : hidden_dim;
    desc.dims[1] = routed ? hidden_dim : ffn_dim;
    desc.dims[2] = routed ? ffn_dim : 0;
    desc.bytes = gate_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &gate, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_GATE_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-mlp-after-gate-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.up_weight";
    desc.bytes = up_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &up, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_UP_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-mlp-after-up-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.down_weight";
    desc.rank = routed ? 3 : 2;
    desc.dims[0] = routed ? actual_experts : ffn_dim;
    desc.dims[1] = routed ? ffn_dim : hidden_dim;
    desc.dims[2] = routed ? hidden_dim : 0;
    desc.bytes = down_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &down, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_DOWN_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-mlp-after-down-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.intermediate";
    desc.rank = 2;
    desc.dims[0] = batch;
    desc.dims[1] = ffn_dim;
    desc.dims[2] = 0;
    desc.bytes = intermediate_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &intermediate, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_INTERMEDIATE_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-mlp-after-intermediate-alloc";
        goto fail_cleaned;
    }

    desc.name = "mlp.output";
    desc.dims[0] = batch;
    desc.dims[1] = hidden_dim;
    desc.bytes = output_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, input_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, gate, gate_values, gate_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, up, up_values, up_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, down, down_values, down_bytes, &err);
    }
    if (rc != YVEX_OK) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_OUTPUT_ALLOC")) {
        graph_guard_mark_cleanup(&guard, "output", 1);
        reason = "injected-mlp-after-output-alloc";
        goto fail_cleaned;
    }

    options.batch = batch;
    options.hidden_dim = hidden_dim;
    options.ffn_dim = ffn_dim;
    options.expert_count = routed ? expert_count : 0ull;
    options.expert_id = routed ? expert_id : 0ull;
    options.routed_expert_mode = routed;
    options.gated = gated;
    options.activation = activation;

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_mlp(backend, input, gate, up, down, &options,
                             intermediate, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_DISPATCH")) {
        graph_guard_mark_cleanup(&guard, "dispatch", 1);
        reason = rc == YVEX_OK ? "injected-mlp-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, intermediate, intermediate_values,
                                  intermediate_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    }
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MLP_AFTER_REFERENCE")) {
        graph_guard_mark_cleanup(&guard, "reference", 1);
        reason = rc == YVEX_OK ? "injected-mlp-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, output_elements);
    guard.guard_status = max_abs_diff <= 0.001f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = output_elements < 8ull ? output_elements : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                               activation, gated, routed, expert_count,
                               expert_id, input_bytes, gate_bytes, up_bytes,
                               down_bytes, intermediate_bytes, output_bytes,
                               reference_bytes);
    yvex_cli_out_writef(stdout, "input_elements: %llu\n", input_elements);
    yvex_cli_out_writef(stdout, "gate_weight_elements: %llu\n", gate_elements);
    yvex_cli_out_writef(stdout, "up_weight_elements: %llu\n", up_elements);
    yvex_cli_out_writef(stdout, "down_weight_elements: %llu\n", down_elements);
    yvex_cli_out_writef(stdout, "intermediate_elements: %llu\n", intermediate_elements);
    yvex_cli_out_writef(stdout, "output_elements: %llu\n", output_elements);
    yvex_cli_out_writef(stdout, "weight_total_bytes: %llu\n", total_weight_bytes);
    yvex_cli_out_writef(stdout, "input_total_bytes: %llu\n", total_input_bytes);
    yvex_cli_out_writef(stdout, "input_checksum: %llu\n", cli_checksum_bytes(input_values, input_bytes));
    yvex_cli_out_writef(stdout, "gate_weight_checksum: %llu\n", cli_checksum_bytes(gate_values, gate_bytes));
    yvex_cli_out_writef(stdout, "up_weight_checksum: %llu\n", cli_checksum_bytes(up_values, up_bytes));
    yvex_cli_out_writef(stdout, "down_weight_checksum: %llu\n", cli_checksum_bytes(down_values, down_bytes));
    yvex_cli_out_writef(stdout, "intermediate_checksum: %llu\n", cli_checksum_bytes(intermediate_values, intermediate_bytes));
    yvex_cli_out_writef(stdout, "reference_intermediate_checksum: %llu\n", cli_checksum_bytes(reference_intermediate_values, intermediate_bytes));
    yvex_cli_out_writef(stdout, "output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    yvex_cli_out_writef(stdout, "reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    yvex_cli_out_writef(stdout, "max_abs_diff: %.9g\n", (double)max_abs_diff);
    yvex_cli_out_writef(stdout, "reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        yvex_cli_out_writef(stdout, "mlp_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        yvex_cli_out_writef(stdout, "cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        yvex_cli_out_writef(stdout, "cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    yvex_cli_out_writef(stdout, "output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values,
                           input_elements < 8ull ? input_elements : 8ull);
    cli_print_float_values("intermediate_sample_values", intermediate_values,
                           intermediate_elements < 8ull ? intermediate_elements : 8ull);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_mlp_readiness_fields(exit_code == 0);
    yvex_cli_out_writef(stdout, "status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_mlp_operation_fields(backend_name, batch, hidden_dim, ffn_dim,
                               activation, gated, routed, expert_count,
                               expert_id, input_bytes, gate_bytes, up_bytes,
                               down_bytes, intermediate_bytes, output_bytes,
                               reference_bytes);
    yvex_cli_out_writef(stdout, "input_elements: %llu\n", input_elements);
    yvex_cli_out_writef(stdout, "gate_weight_elements: %llu\n", gate_elements);
    yvex_cli_out_writef(stdout, "up_weight_elements: %llu\n", up_elements);
    yvex_cli_out_writef(stdout, "down_weight_elements: %llu\n", down_elements);
    yvex_cli_out_writef(stdout, "intermediate_elements: %llu\n", intermediate_elements);
    yvex_cli_out_writef(stdout, "output_elements: %llu\n", output_elements);
    yvex_cli_out_writef(stdout, "weight_total_bytes: %llu\n", total_weight_bytes);
    yvex_cli_out_writef(stdout, "input_total_bytes: %llu\n", total_input_bytes);
    print_mlp_readiness_fields(0);
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "mlp-op-failed");
    yvex_cli_out_writef(stdout, "status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, intermediate);
        yvex_backend_tensor_free(backend, down);
        yvex_backend_tensor_free(backend, up);
        yvex_backend_tensor_free(backend, gate);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_values);
    free(output_values);
    free(reference_intermediate_values);
    free(intermediate_values);
    free(down_values);
    free(up_values);
    free(gate_values);
    free(input_values);
    return exit_code;
}

/* Selected fixture, partial, and segment graph preflight. */

int preflight_graph_guard(const yvex_model_ref *model_ref,
                                 const char *backend_name,
                                 int execute_fixture,
                                 int execute_segment,
                                 unsigned int token_id,
                                 yvex_cli_graph_guard_report *report,
                                 yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_artifact_integrity_report integrity_report;
    yvex_tensor_range tensor_range;
    yvex_tensor_slice_range slice_range;
    yvex_selected_embedding_shape embedding_shape;
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    const yvex_tensor_info *tensor;
    const yvex_tensor_info *rmsnorm_tensor = NULL;
    unsigned long long hidden_size;
    unsigned long long output_bytes;
    unsigned long long planned_bytes;
    int rc;

    init_graph_guard_report(report,
                            execute_fixture ? "fixture-embedding" :
                            (execute_segment ? "selected-embedding-rmsnorm"
                                             : "selected-embedding-partial"),
                            !execute_fixture,
                            model_ref);
    memset(&ctx, 0, sizeof(ctx));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&tensor_range, 0, sizeof(tensor_range));
    memset(&slice_range, 0, sizeof(slice_range));
    memset(&embedding_shape, 0, sizeof(embedding_shape));
    memset(&backend_options, 0, sizeof(backend_options));

    rc = open_model_context(model_ref->path, &ctx, err);
    if (rc != YVEX_OK) {
        report->integrity_status = "fail";
        return rc;
    }
    rc = yvex_artifact_integrity_validate(ctx.artifact,
                                          ctx.gguf,
                                          ctx.table,
                                          NULL,
                                          &integrity_report,
                                          err);
    report->integrity_status = (rc == YVEX_OK && integrity_report.passed) ? "pass" : "fail";
    report->shape_status =
        integrity_report.tensor_shapes_invalid == 0 &&
        integrity_report.tensor_dtypes_invalid == 0 &&
        integrity_report.tensor_byte_counts_invalid == 0 ? "pass" : "fail";
    report->range_status = integrity_report.tensor_ranges_invalid == 0 ? "pass" : "fail";
    if (rc != YVEX_OK || !integrity_report.passed) {
        close_model_context(&ctx);
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "graph_integrity_preflight",
                           "artifact integrity preflight failed");
        }
        return rc == YVEX_OK ? YVEX_ERR_STATE : rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (!tensor) {
        report->shape_status = "fail";
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, tensor, &tensor_range, err);
    if (rc != YVEX_OK) {
        report->range_status = "fail";
        close_model_context(&ctx);
        return rc;
    }
    report->range_status = "pass";

    if (execute_fixture) {
        if (tensor->dtype != YVEX_DTYPE_F32) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "fixture graph embed execution requires F32 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        if (tensor->rank != 2 || tensor->dims[0] == 0 || tensor->dims[1] == 0) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                           "fixture graph token embedding must be rank 2 with non-zero dims");
            return YVEX_ERR_FORMAT;
        }
        if ((unsigned long long)token_id >= tensor->dims[1]) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                            "fixture token id %u exceeds embedding vocab size %llu",
                            token_id, tensor->dims[1]);
            return YVEX_ERR_BOUNDS;
        }
        hidden_size = tensor->dims[0];
        if (hidden_size > (unsigned long long)(~(size_t)0 / sizeof(float))) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                           "fixture graph output is too large");
            return YVEX_ERR_BOUNDS;
        }
        report->shape_status = "pass";
        report->slice_range_status = "not-needed";
        report->output_bytes_planned = hidden_size * (unsigned long long)sizeof(float);
    } else {
        if (tensor->dtype != YVEX_DTYPE_F16) {
            report->shape_status = "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "real partial embedding segment requires F16 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        rc = yvex_selected_embedding_shape_validate(tensor, token_id, &embedding_shape, err);
        if (rc != YVEX_OK) {
            const char *msg = yvex_error_message(err);
            report->shape_status = msg && strstr(msg, "token-out-of-range") ? "pass" : "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            if (msg && strstr(msg, "token-out-of-range")) {
                yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                                "partial token out of range: %u >= %llu",
                                token_id, embedding_shape.vocab_size);
            }
            return rc;
        }
        rc = yvex_tensor_embedding_slice_range_validate(&tensor_range,
                                                        token_id,
                                                        &slice_range,
                                                        err);
        if (rc != YVEX_OK) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            return rc;
        }
        report->shape_status = "pass";
        report->slice_range_status = "pass";
        report->output_bytes_planned = embedding_shape.output_bytes;
        report->reference_bytes_planned = slice_range.slice_bytes;

        if (execute_segment) {
            rmsnorm_tensor = cli_find_first_rmsnorm_tensor(ctx.table);
            if (!rmsnorm_tensor) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-tensor-missing");
                return YVEX_ERR_UNSUPPORTED;
            }
            if (rmsnorm_tensor->rank != 1 || rmsnorm_tensor->dims[0] != embedding_shape.hidden_size) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-shape-invalid");
                return YVEX_ERR_FORMAT;
            }
            if (rmsnorm_tensor->dtype != YVEX_DTYPE_F16 &&
                rmsnorm_tensor->dtype != YVEX_DTYPE_F32) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-dtype-invalid");
                return YVEX_ERR_UNSUPPORTED;
            }
            rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, rmsnorm_tensor, &tensor_range, err);
            if (rc != YVEX_OK) {
                report->range_status = "fail";
                close_model_context(&ctx);
                return rc;
            }
            if (!cli_has_rmsnorm_epsilon(ctx.gguf)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-epsilon-missing");
                return YVEX_ERR_FORMAT;
            }
            if (embedding_shape.output_bytes > ULLONG_MAX / 2ull ||
                embedding_shape.output_bytes > (unsigned long long)(~(size_t)0)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                               "segment-memory-plan-overflow");
                return YVEX_ERR_BOUNDS;
            }
            output_bytes = embedding_shape.output_bytes;
            planned_bytes = output_bytes * 2ull;
            report->output_bytes_planned = planned_bytes;
            report->reference_bytes_planned = output_bytes;
        }
    }

    backend_options.kind = graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        report->backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        report->backend_op_status = "unsupported";
        close_model_context(&ctx);
        return rc;
    }
    report->backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        (execute_segment && !yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM)) ||
        cli_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        report->backend_op_status = "unsupported";
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "backend-op-unsupported");
        return YVEX_ERR_UNSUPPORTED;
    }
    report->backend_op_status = "supported";
    report->guard_status = "pass";
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return YVEX_OK;
}

static const char *graph_text_or(const char *value, const char *fallback)
{
    return value ? value : fallback;
}

static void graph_guard_apply_result_common(yvex_cli_graph_guard_report *guard,
                                            const char *guard_status,
                                            const char *phase,
                                            const char *shape_status,
                                            const char *range_status,
                                            const char *slice_range_status,
                                            const char *backend_status,
                                            const char *backend_op_status,
                                            int dispatch_attempted,
                                            int reference_read_attempted,
                                            int output_allocation_attempted,
                                            int cleanup_attempted,
                                            const char *cleanup_status,
                                            unsigned long long output_bytes_planned,
                                            unsigned long long output_bytes_allocated,
                                            unsigned long long reference_bytes_planned,
                                            const char *slice_success_default,
                                            int success)
{
    guard->guard_status = success ? graph_text_or(guard_status, "pass") : "fail";
    guard->phase = graph_text_or(phase, success ? "complete" : "dispatch");
    guard->shape_status = graph_text_or(shape_status, success ? "pass" : guard->shape_status);
    guard->range_status = graph_text_or(range_status, success ? "pass" : guard->range_status);
    guard->slice_range_status =
        graph_text_or(slice_range_status,
                      success ? slice_success_default : guard->slice_range_status);
    guard->backend_status = graph_text_or(backend_status, success ? "ready" : guard->backend_status);
    guard->backend_op_status =
        graph_text_or(backend_op_status, success ? "supported" : guard->backend_op_status);
    guard->dispatch_attempted = dispatch_attempted;
    guard->reference_read_attempted = reference_read_attempted;
    guard->output_allocation_attempted = output_allocation_attempted;
    guard->cleanup_attempted = cleanup_attempted;
    guard->cleanup_status = graph_text_or(cleanup_status,
                                          success ? "not-needed" : guard->cleanup_status);
    guard->output_bytes_planned = output_bytes_planned;
    guard->output_bytes_allocated = output_bytes_allocated;
    guard->reference_bytes_planned = reference_bytes_planned;
}

static void graph_guard_apply_fixture_result(yvex_cli_graph_guard_report *guard,
                                             const yvex_fixture_graph_result *result,
                                             int success)
{
    graph_guard_apply_result_common(guard,
                                    result->graph_integrity_guard,
                                    result->graph_execution_phase,
                                    result->shape_status,
                                    result->range_status,
                                    result->slice_range_status,
                                    result->backend_status,
                                    result->backend_op_status,
                                    result->dispatch_attempted,
                                    result->reference_read_attempted,
                                    result->output_allocation_attempted,
                                    result->cleanup_attempted,
                                    result->cleanup_status,
                                    result->output_bytes_planned,
                                    result->output_bytes_allocated,
                                    result->reference_bytes_planned,
                                    "not-needed",
                                    success);
}

static void graph_guard_apply_partial_result(yvex_cli_graph_guard_report *guard,
                                             const yvex_partial_graph_result *result,
                                             int success)
{
    graph_guard_apply_result_common(guard,
                                    result->graph_integrity_guard,
                                    result->graph_execution_phase,
                                    result->shape_status,
                                    result->range_status,
                                    result->slice_range_status,
                                    result->backend_status,
                                    result->backend_op_status,
                                    result->dispatch_attempted,
                                    result->reference_read_attempted,
                                    result->output_allocation_attempted,
                                    result->cleanup_attempted,
                                    result->cleanup_status,
                                    result->output_bytes_planned,
                                    result->output_bytes_allocated,
                                    result->reference_bytes_planned,
                                    "pass",
                                    success);
}

static void graph_guard_apply_segment_result(yvex_cli_graph_guard_report *guard,
                                             const yvex_segment_graph_result *result,
                                             int success)
{
    graph_guard_apply_result_common(guard,
                                    result->graph_integrity_guard,
                                    result->graph_execution_phase,
                                    result->shape_status,
                                    result->range_status,
                                    result->slice_range_status,
                                    result->backend_status,
                                    result->backend_op_status,
                                    result->dispatch_attempted,
                                    result->reference_read_attempted,
                                    result->output_allocation_attempted,
                                    result->cleanup_attempted,
                                    result->cleanup_status,
                                    result->output_bytes_planned,
                                    result->output_bytes_allocated,
                                    result->reference_bytes_planned,
                                    "pass",
                                    success);
}

static int graph_mode_count(int execute_fixture,
                            int execute_partial,
                            int execute_segment,
                            int execute_op,
                            int execute_block,
                            int execute_layers)
{
    return (execute_fixture ? 1 : 0) +
           (execute_partial ? 1 : 0) +
           (execute_segment ? 1 : 0) +
           (execute_op ? 1 : 0) +
           (execute_block ? 1 : 0) +
           (execute_layers ? 1 : 0);
}

static const char *graph_guard_kind_for_mode(int execute_fixture, int execute_segment)
{
    if (execute_fixture) {
        return "fixture-embedding";
    }
    return execute_segment ? "selected-embedding-rmsnorm" : "selected-embedding-partial";
}

static const char *graph_execution_label_for_mode(int execute_fixture, int execute_segment)
{
    if (execute_fixture) {
        return "graph-fixture";
    }
    return execute_segment ? "graph-segment" : "graph-partial";
}

static const char *graph_backend_label_for_mode(int execute_fixture, int execute_segment)
{
    if (execute_fixture) {
        return "fixture";
    }
    return execute_segment ? "segment" : "partial";
}

static unsigned int graph_token_for_mode(int execute_fixture,
                                         int execute_segment,
                                         const yvex_fixture_graph_options *fixture_options,
                                         const yvex_partial_graph_options *partial_options,
                                         const yvex_segment_graph_options *segment_options)
{
    if (execute_fixture) {
        return fixture_options->token_id;
    }
    return execute_segment ? segment_options->token_id : partial_options->token_id;
}

/* Graph command parser and output surface. */

static int command_graph(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_graph *graph = NULL;
    yvex_graph_build_options options;
    yvex_engine_options engine_options;
    yvex_fixture_graph_options fixture_options;
    yvex_fixture_graph_result fixture_result;
    yvex_partial_graph_options partial_options;
    yvex_partial_graph_result partial_result;
    yvex_segment_graph_options segment_options;
    yvex_segment_graph_result segment_result;
    yvex_cli_graph_guard_report graph_guard;
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine *engine = NULL;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = "cpu";
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    const char *op_name = NULL;
    const char *block_name = NULL;
    unsigned int token_index = 0u;
    unsigned int selected_token_id = 0u;
    unsigned long long token_vocab_size = 0ull;
    unsigned long long rope_position = 0ull;
    unsigned long long rope_head_dim = 0ull;
    unsigned long long attention_seq_len = 0ull;
    unsigned long long matmul_m = 0ull;
    unsigned long long matmul_k = 0ull;
    unsigned long long matmul_n = 0ull;
    unsigned long long mlp_hidden_dim = 0ull;
    unsigned long long mlp_ffn_dim = 0ull;
    unsigned long long mlp_experts = 0ull;
    unsigned long long mlp_expert_id = 0ull;
    unsigned long long layer_count = 0ull;
    int execute_fixture = 0;
    int execute_partial = 0;
    int execute_segment = 0;
    int execute_op = 0;
    int execute_block = 0;
    int execute_layers = 0;
    int attention_causal = 0;
    int mlp_gated = 0;
    int block_name_provided = 0;
    int fixture_token_provided = 0;
    int partial_token_provided = 0;
    int token_input_provided = 0;
    int token_index_provided = 0;
    int rope_position_provided = 0;
    int rope_head_dim_provided = 0;
    int attention_seq_len_provided = 0;
    int matmul_m_provided = 0;
    int matmul_k_provided = 0;
    int matmul_n_provided = 0;
    int mlp_hidden_dim_provided = 0;
    int mlp_ffn_dim_provided = 0;
    int mlp_activation_provided = 0;
    int mlp_experts_provided = 0;
    int mlp_expert_id_provided = 0;
    int layers_provided = 0;
    const char *mlp_activation = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&fixture_options, 0, sizeof(fixture_options));
    memset(&fixture_result, 0, sizeof(fixture_result));
    memset(&partial_options, 0, sizeof(partial_options));
    memset(&partial_result, 0, sizeof(partial_result));
    memset(&segment_options, 0, sizeof(segment_options));
    memset(&segment_result, 0, sizeof(segment_result));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    options.sequence_length = 1;
    options.include_prefill_path = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_graph_help(stdout);
        return argc >= 3 ? 0 : 2;
    }
    if (strcmp(argv[2], "check") == 0) {
        return command_graph_check(argc, argv);
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.sequence_length)) {
                yvex_cli_out_writef(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.context_length)) {
                yvex_cli_out_writef(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--execute-fixture") == 0) {
            execute_fixture = 1;
        } else if (strcmp(argv[i], "--fixture-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &fixture_options.token_id)) {
                yvex_cli_out_writef(stderr, "yvex: --fixture-token requires a non-negative integer\n");
                return 2;
            }
            fixture_token_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--execute-partial") == 0) {
            execute_partial = 1;
        } else if (strcmp(argv[i], "--execute-segment") == 0) {
            execute_segment = 1;
        } else if (strcmp(argv[i], "--execute-op") == 0) {
            execute_op = 1;
        } else if (strcmp(argv[i], "--execute-block") == 0) {
            execute_block = 1;
        } else if (strcmp(argv[i], "--execute-layers") == 0) {
            execute_layers = 1;
        } else if (strcmp(argv[i], "--op") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --op requires rope, attention, matmul, or mlp\n");
                return 2;
            }
            op_name = argv[++i];
        } else if (strcmp(argv[i], "--block") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --block requires fixture\n");
                return 2;
            }
            block_name = argv[++i];
            block_name_provided = 1;
        } else if (strcmp(argv[i], "--m") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_m)) {
                yvex_cli_out_writef(stderr, "yvex: --m requires a positive integer\n");
                return 2;
            }
            matmul_m_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--k") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_k)) {
                yvex_cli_out_writef(stderr, "yvex: --k requires a positive integer\n");
                return 2;
            }
            matmul_k_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--n") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_n)) {
                yvex_cli_out_writef(stderr, "yvex: --n requires a positive integer\n");
                return 2;
            }
            matmul_n_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--hidden-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &mlp_hidden_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --hidden-dim requires a positive integer\n");
                return 2;
            }
            mlp_hidden_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--ffn-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &mlp_ffn_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --ffn-dim requires a positive integer\n");
                return 2;
            }
            mlp_ffn_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--activation") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --activation requires silu\n");
                return 2;
            }
            mlp_activation = argv[++i];
            mlp_activation_provided = 1;
        } else if (strcmp(argv[i], "--gated") == 0) {
            mlp_gated = 1;
        } else if (strcmp(argv[i], "--experts") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &mlp_experts)) {
                yvex_cli_out_writef(stderr, "yvex: --experts requires a positive integer\n");
                return 2;
            }
            mlp_experts_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--expert-id") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &mlp_expert_id)) {
                yvex_cli_out_writef(stderr, "yvex: --expert-id requires a non-negative integer\n");
                return 2;
            }
            mlp_expert_id_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            layers_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--seq-len") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &attention_seq_len)) {
                yvex_cli_out_writef(stderr, "yvex: --seq-len requires a positive integer\n");
                return 2;
            }
            attention_seq_len_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &rope_position)) {
                yvex_cli_out_writef(stderr, "yvex: --position requires a non-negative integer\n");
                return 2;
            }
            rope_position_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &rope_head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            rope_head_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--causal") == 0) {
            attention_causal = 1;
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--partial-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &partial_options.token_id)) {
                yvex_cli_out_writef(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            segment_options.token_id = partial_options.token_id;
            partial_token_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: --tokens requires comma-separated token IDs\n");
                return 2;
            }
            tokens_text = argv[++i];
            token_input_provided = 1;
        } else if (strcmp(argv[i], "--token-index") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &token_index)) {
                yvex_cli_out_writef(stderr, "yvex: --token-index requires a non-negative integer\n");
                return 2;
            }
            token_index_provided = 1;
            i += 1;
        } else if (!model_arg) {
            model_arg = argv[i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown graph option: %s\n", argv[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help graph' for usage.\n");
            return 2;
        }
    }

    if (!model_arg && !execute_op && !execute_block && !execute_layers) {
        yvex_cli_out_writef(stderr, "yvex: graph requires FILE_OR_ALIAS\n");
        yvex_cli_out_writef(stderr, "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda] [--execute-fixture] [--execute-partial] [--execute-segment --segment embedding-rmsnorm] [--partial-token N] [--tokens IDS --token-index N] | yvex graph --backend cpu|cuda --execute-block --block fixture --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated] | yvex graph --backend cpu|cuda --execute-layers --layers N --block fixture --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]\n");
        return 2;
    }
    if (!graph_backend_valid(backend_name)) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (graph_mode_count(execute_fixture, execute_partial, execute_segment,
                         execute_op, execute_block, execute_layers) > 1) {
        yvex_cli_out_writef(stderr, "yvex: --execute-fixture, --execute-partial, --execute-segment, --execute-op, --execute-block, and --execute-layers are mutually exclusive\n");
        return 2;
    }
    if (execute_layers) {
        if (model_arg) {
            yvex_cli_out_writef(stderr, "yvex: --execute-layers does not take a model artifact\n");
            return 2;
        }
        if (!block_name_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-layers requires --block fixture\n");
            return 2;
        }
        if (strcmp(block_name, "fixture") != 0) {
            yvex_cli_out_writef(stderr, "yvex: unsupported block: %s\n", block_name);
            return 2;
        }
        if (!layers_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-layers requires --layers N\n");
            return 2;
        }
        if (!attention_seq_len_provided || !rope_position_provided ||
            !mlp_hidden_dim_provided || !rope_head_dim_provided ||
            !mlp_ffn_dim_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-layers requires --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N\n");
            return 2;
        }
        if (op_name || matmul_m_provided || matmul_k_provided ||
            matmul_n_provided || mlp_activation_provided ||
            mlp_experts_provided || mlp_expert_id_provided ||
            fixture_token_provided || partial_token_provided ||
            token_input_provided || token_index_provided || segment_name) {
            yvex_cli_out_writef(stderr, "yvex: --execute-layers cannot be combined with model graph, segment, token, or standalone op options\n");
            return 2;
        }
        return command_graph_execute_layers_fixture(backend_name,
                                                    layer_count,
                                                    attention_seq_len,
                                                    rope_position,
                                                    mlp_hidden_dim,
                                                    rope_head_dim,
                                                    mlp_ffn_dim,
                                                    1,
                                                    1);
    }
    if (execute_block) {
        if (model_arg) {
            yvex_cli_out_writef(stderr, "yvex: --execute-block does not take a model artifact\n");
            return 2;
        }
        if (!block_name_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-block requires --block fixture\n");
            return 2;
        }
        if (strcmp(block_name, "fixture") != 0) {
            yvex_cli_out_writef(stderr, "yvex: unsupported block: %s\n", block_name);
            return 2;
        }
        if (!attention_seq_len_provided || !rope_position_provided ||
            !mlp_hidden_dim_provided || !rope_head_dim_provided ||
            !mlp_ffn_dim_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-block requires --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N\n");
            return 2;
        }
        if (op_name || matmul_m_provided || matmul_k_provided ||
            matmul_n_provided || mlp_activation_provided ||
            mlp_experts_provided || mlp_expert_id_provided ||
            fixture_token_provided || partial_token_provided ||
            token_input_provided || token_index_provided || segment_name) {
            yvex_cli_out_writef(stderr, "yvex: --execute-block cannot be combined with model graph, segment, or standalone op options\n");
            return 2;
        }
        return command_graph_execute_block_fixture(backend_name,
                                                   attention_seq_len,
                                                   rope_position,
                                                   mlp_hidden_dim,
                                                   rope_head_dim,
                                                   mlp_ffn_dim,
                                                   1,
                                                   1);
    }
    if (execute_op) {
        if (model_arg) {
            yvex_cli_out_writef(stderr, "yvex: --execute-op does not take a model artifact\n");
            return 2;
        }
        if (!op_name ||
            (strcmp(op_name, "rope") != 0 &&
             strcmp(op_name, "attention") != 0 &&
             strcmp(op_name, "matmul") != 0 &&
             strcmp(op_name, "mlp") != 0)) {
            yvex_cli_out_writef(stderr, "yvex: --execute-op requires --op rope, --op attention, --op matmul, or --op mlp\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") == 0) {
            if (!matmul_m_provided || !matmul_k_provided || !matmul_n_provided) {
                yvex_cli_out_writef(stderr, "yvex: --execute-op --op matmul requires --m M --k K --n N\n");
                return 2;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || attention_causal) {
                yvex_cli_out_writef(stderr, "yvex: --position, --head-dim, --seq-len, and --causal require --op rope or --op attention\n");
                return 2;
            }
        } else if (strcmp(op_name, "mlp") == 0) {
            if (!mlp_hidden_dim_provided || !mlp_ffn_dim_provided ||
                !mlp_activation_provided || !mlp_gated) {
                yvex_cli_out_writef(stderr, "yvex: --execute-op --op mlp requires --hidden-dim N --ffn-dim N --activation silu --gated\n");
                return 2;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || attention_causal ||
                matmul_m_provided || matmul_k_provided || matmul_n_provided) {
                yvex_cli_out_writef(stderr, "yvex: --op mlp cannot use --position, --head-dim, --seq-len, --causal, --m, --k, or --n\n");
                return 2;
            }
            if (mlp_experts_provided != mlp_expert_id_provided) {
                yvex_cli_out_writef(stderr, "yvex: --experts and --expert-id must be provided together\n");
                return 2;
            }
        } else if (!rope_position_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-op requires --position N\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") != 0 && strcmp(op_name, "mlp") != 0 &&
            !rope_head_dim_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-op requires --head-dim N\n");
            return 2;
        }
        if (strcmp(op_name, "attention") == 0 && !attention_seq_len_provided) {
            yvex_cli_out_writef(stderr, "yvex: --execute-op --op attention requires --seq-len N\n");
            return 2;
        }
        if (strcmp(op_name, "rope") == 0 && (attention_seq_len_provided || attention_causal)) {
            yvex_cli_out_writef(stderr, "yvex: --seq-len and --causal require --op attention\n");
            return 2;
        }
        if (strcmp(op_name, "rope") != 0 && strcmp(op_name, "attention") != 0 &&
            (rope_position_provided || rope_head_dim_provided)) {
            yvex_cli_out_writef(stderr, "yvex: --position and --head-dim require --op rope or --op attention\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") != 0 &&
            (matmul_m_provided || matmul_k_provided || matmul_n_provided)) {
            yvex_cli_out_writef(stderr, "yvex: --m, --k, and --n require --op matmul\n");
            return 2;
        }
        if (strcmp(op_name, "mlp") != 0 &&
            (mlp_hidden_dim_provided || mlp_ffn_dim_provided ||
             mlp_activation_provided || mlp_gated || mlp_experts_provided ||
             mlp_expert_id_provided)) {
            yvex_cli_out_writef(stderr, "yvex: --hidden-dim, --ffn-dim, --activation, --gated, --experts, and --expert-id require --op mlp\n");
            return 2;
        }
        if (fixture_token_provided || partial_token_provided || token_input_provided ||
            token_index_provided || segment_name) {
            yvex_cli_out_writef(stderr, "yvex: --execute-op cannot be combined with model graph token or segment options\n");
            return 2;
        }
        if (strcmp(op_name, "mlp") == 0) {
            return command_graph_execute_mlp_op(backend_name, mlp_hidden_dim,
                                                mlp_ffn_dim, mlp_activation,
                                                mlp_gated, mlp_experts,
                                                mlp_expert_id,
                                                mlp_experts_provided);
        }
        if (strcmp(op_name, "matmul") == 0) {
            return command_graph_execute_matmul_op(backend_name, matmul_m, matmul_k, matmul_n);
        }
        if (strcmp(op_name, "attention") == 0) {
            return command_graph_execute_attention_op(backend_name, attention_seq_len,
                                                      rope_position, rope_head_dim,
                                                      attention_causal);
        }
        return command_graph_execute_rope_op(backend_name, rope_position, rope_head_dim);
    }
    if (block_name_provided) {
        yvex_cli_out_writef(stderr, "yvex: --block requires --execute-block or --execute-layers\n");
        return 2;
    }
    if (op_name || rope_position_provided || rope_head_dim_provided ||
        attention_seq_len_provided || attention_causal ||
        matmul_m_provided || matmul_k_provided || matmul_n_provided ||
        mlp_hidden_dim_provided || mlp_ffn_dim_provided ||
        mlp_activation_provided || mlp_gated || mlp_experts_provided ||
        mlp_expert_id_provided || layers_provided) {
        yvex_cli_out_writef(stderr, "yvex: --op and standalone op options require --execute-op\n");
        return 2;
    }
    if (execute_segment) {
        if (!segment_name) {
            yvex_cli_out_writef(stderr, "yvex: --execute-segment requires --segment embedding-rmsnorm\n");
            return 2;
        }
        if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
            yvex_cli_out_writef(stderr, "yvex: unsupported segment: %s\n", segment_name);
            return 2;
        }
        segment_options.segment_name = segment_name;
    } else if (segment_name) {
        yvex_cli_out_writef(stderr, "yvex: --segment requires --execute-segment\n");
        return 2;
    }
    if (token_index_provided && !token_input_provided) {
        yvex_cli_out_writef(stderr, "yvex: --token-index requires --tokens\n");
        return 2;
    }
    if (token_input_provided && !(execute_fixture || execute_partial || execute_segment)) {
        yvex_cli_out_writef(stderr, "yvex: --tokens is only supported with graph execution flags\n");
        return 2;
    }
    if (token_input_provided && (partial_token_provided || fixture_token_provided)) {
        yvex_cli_out_writef(stderr, "yvex: --tokens cannot be combined with --partial-token or --fixture-token\n");
        return 2;
    }

    if (execute_fixture || execute_partial || execute_segment) {
        rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = enforce_registered_identity_cli(&model_ref,
                                             graph_execution_label_for_mode(execute_fixture,
                                                                           execute_segment));
        if (rc != YVEX_OK) {
            init_graph_guard_report(&graph_guard,
                                    graph_guard_kind_for_mode(execute_fixture, execute_segment),
                                    !execute_fixture,
                                    &model_ref);
            graph_guard.identity_status = "fail";
            graph_guard.metadata_status = "fail";
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return exit_for_status(rc);
        }

        if (token_input_provided) {
            rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
            if (rc == YVEX_OK) {
                rc = cli_token_input_vocab_from_model(model_ref.path, &token_vocab_size, &err);
            }
            if (rc == YVEX_OK) {
                rc = yvex_token_input_validate_bounds(&token_input, token_vocab_size, &err);
            }
            if (rc == YVEX_OK) {
                rc = yvex_token_input_select(&token_input,
                                             (unsigned long long)token_index,
                                             &selected_token_id,
                                             &err);
            }
            if (rc != YVEX_OK) {
                init_graph_guard_report(&graph_guard,
                                        graph_guard_kind_for_mode(execute_fixture,
                                                                  execute_segment),
                                        !execute_fixture,
                                        &model_ref);
                graph_guard.slice_range_status =
                    token_input.token_bounds_checked && token_input.token_bounds_valid
                        ? "pass"
                        : "fail";
                print_token_input_summary(&token_input,
                                          "fail",
                                          token_input.token_bounds_checked
                                              ? (token_input.token_bounds_valid ? "pass" : "fail")
                                              : "not-checked",
                                          (unsigned long long)token_index,
                                          selected_token_id,
                                          0);
                yvex_cli_out_writef(stdout, "vocab_size: %llu\n", token_vocab_size);
                print_graph_guard_report(&graph_guard);
                yvex_cli_out_writef(stdout, "status: graph-integrity-fail\n");
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }

            if (execute_fixture) {
                fixture_options.token_id = selected_token_id;
            } else if (execute_segment) {
                segment_options.token_id = selected_token_id;
                partial_options.token_id = selected_token_id;
            } else {
                partial_options.token_id = selected_token_id;
                segment_options.token_id = selected_token_id;
            }
        }

        rc = preflight_graph_guard(&model_ref,
                                   backend_name,
                                   execute_fixture,
                                   execute_segment,
                                   graph_token_for_mode(execute_fixture,
                                                        execute_segment,
                                                        &fixture_options,
                                                        &partial_options,
                                                        &segment_options),
                                   &graph_guard,
                                   &err);
        if (rc != YVEX_OK) {
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }

        if (token_input_provided) {
            print_token_input_summary(&token_input,
                                      "pass",
                                      "pass",
                                      (unsigned long long)token_index,
                                      selected_token_id,
                                      1);
            yvex_cli_out_writef(stdout, "vocab_size: %llu\n", token_vocab_size);
        }

        engine_options.model_path = model_ref.path;
        engine_options.load_tokenizer = 0;
        engine_options.build_descriptor = 1;
        engine_options.build_default_graph = 1;
        engine_options.attach_weights = 1;
        engine_options.backend_name = backend_name;
        engine_options.require_all_weights = 1;

        rc = yvex_engine_open(&engine, &engine_options, &err);
        if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
            graph_guard.guard_status = "fail";
            graph_guard.phase = "preflight";
            graph_guard.backend_status = "unavailable";
            graph_guard.backend_op_status = "unsupported";
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "%s_backend: cuda\n", graph_backend_label_for_mode(execute_fixture,
                                                                       execute_segment));
            yvex_cli_out_writef(stdout, "%s_backend_status: unsupported\n",
                   graph_backend_label_for_mode(execute_fixture, execute_segment));
            yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
            yvex_cli_out_writef(stdout, "status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return 5;
        }
        if (rc == YVEX_OK && execute_fixture) {
            rc = yvex_engine_execute_fixture_graph(engine, &fixture_options, &fixture_result, &err);
        } else if (rc == YVEX_OK && execute_segment) {
            rc = yvex_engine_execute_segment_graph(engine, &segment_options, &segment_result, &err);
        } else if (rc == YVEX_OK) {
            rc = yvex_engine_execute_partial_graph(engine, &partial_options, &partial_result, &err);
        }
        if (rc != YVEX_OK) {
            if (execute_fixture) {
                graph_guard_apply_fixture_result(&graph_guard, &fixture_result, 0);
            } else if (execute_segment) {
                graph_guard_apply_segment_result(&graph_guard, &segment_result, 0);
            } else {
                graph_guard_apply_partial_result(&graph_guard, &partial_result, 0);
            }
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "status: %s\n",
                   graph_guard.cleanup_attempted ? "graph-failed-cleaned" : "graph-integrity-fail");
            yvex_engine_close(engine);
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }

        if (execute_fixture) {
            graph_guard_apply_fixture_result(&graph_guard, &fixture_result, 1);
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "fixture_graph_executed: true\n");
            yvex_cli_out_writef(stdout, "fixture_backend: %s\n", fixture_result.backend_name);
            yvex_cli_out_writef(stdout, "fixture_op: %s\n", fixture_result.op_name);
            yvex_cli_out_writef(stdout, "fixture_weight: %s\n", fixture_result.weight_name);
            yvex_cli_out_writef(stdout, "fixture_token_id: %u\n", fixture_result.token_id);
            yvex_cli_out_writef(stdout, "fixture_node_count: %llu\n", fixture_result.node_count);
            yvex_cli_out_writef(stdout, "fixture_output_count: %llu\n", fixture_result.output_count);
            yvex_cli_out_writef(stdout, "fixture_output_bytes: %llu\n", fixture_result.output_bytes);
            yvex_cli_out_writef(stdout, "fixture_output_checksum: %llu\n", fixture_result.output_checksum);
            yvex_cli_out_writef(stdout, "fixture_output_values:");
            for (i = 0; (unsigned long long)i < fixture_result.output_value_count; ++i) {
                yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)fixture_result.output_values[i]);
            }
            yvex_cli_out_writef(stdout, "\n");
            yvex_cli_out_writef(stdout, "execution_ready: false\n");
            yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
            yvex_cli_out_writef(stdout, "status: fixture-graph-executed\n");
        } else if (execute_segment) {
            graph_guard_apply_segment_result(&graph_guard, &segment_result, 1);
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "segment_graph_executed: true\n");
            yvex_cli_out_writef(stdout, "segment_backend: %s\n", segment_result.backend_name);
            yvex_cli_out_writef(stdout, "segment_name: %s\n", segment_result.segment_name);
            yvex_cli_out_writef(stdout, "segment_ops: %llu\n", segment_result.segment_ops);
            yvex_cli_out_writef(stdout, "segment_op_0: embed\n");
            yvex_cli_out_writef(stdout, "segment_op_1: rms_norm\n");
            yvex_cli_out_writef(stdout, "partial_token: %u\n", segment_result.token_id);
            yvex_cli_out_writef(stdout, "token_tensor: %s\n", segment_result.token_tensor_name);
            yvex_cli_out_writef(stdout, "token_tensor_dtype: %s\n", segment_result.token_tensor_dtype);
            yvex_cli_out_writef(stdout, "rmsnorm_tensor: %s\n", segment_result.rmsnorm_tensor_name);
            yvex_cli_out_writef(stdout, "rmsnorm_tensor_dtype: %s\n", segment_result.rmsnorm_tensor_dtype);
            yvex_cli_out_writef(stdout, "hidden_size: %llu\n", segment_result.hidden_size);
            yvex_cli_out_writef(stdout, "vocab_size: %llu\n", segment_result.vocab_size);
            yvex_cli_out_writef(stdout, "rmsnorm_epsilon_key: %s\n", segment_result.rmsnorm_epsilon_key);
            yvex_cli_out_writef(stdout, "rmsnorm_epsilon: %.9g\n", segment_result.rmsnorm_epsilon);
            yvex_cli_out_writef(stdout, "segment_memory_plan: explicit\n");
            yvex_cli_out_writef(stdout, "segment_intermediate_count: %llu\n", segment_result.segment_intermediate_count);
            yvex_cli_out_writef(stdout, "segment_intermediate_bytes: %llu\n", segment_result.segment_intermediate_bytes);
            yvex_cli_out_writef(stdout, "segment_output_count: %llu\n", segment_result.segment_output_count);
            yvex_cli_out_writef(stdout, "segment_output_bytes: %llu\n", segment_result.segment_output_bytes);
            yvex_cli_out_writef(stdout, "segment_scratch_bytes: %llu\n", segment_result.segment_scratch_bytes);
            yvex_cli_out_writef(stdout, "segment_reference_bytes: %llu\n", segment_result.segment_reference_bytes);
            yvex_cli_out_writef(stdout, "segment_output_checksum: %llu\n", segment_result.output_checksum);
            yvex_cli_out_writef(stdout, "segment_reference_checksum: %llu\n", segment_result.reference_checksum);
            yvex_cli_out_writef(stdout, "segment_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            if (strcmp(segment_result.backend_name, "cuda") == 0) {
                yvex_cli_out_writef(stdout, "segment_cuda_parity: pass\n");
                yvex_cli_out_writef(stdout, "cuda_reference_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            } else {
                yvex_cli_out_writef(stdout, "cpu_reference_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            }
            yvex_cli_out_writef(stdout, "segment_output_sample_count: %llu\n", segment_result.output_value_count);
            yvex_cli_out_writef(stdout, "segment_output_sample_values:");
            for (i = 0; (unsigned long long)i < segment_result.output_value_count; ++i) {
                yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)segment_result.output_values[i]);
            }
            yvex_cli_out_writef(stdout, "\n");
            yvex_cli_out_writef(stdout, "execution_ready: false\n");
            yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
            yvex_cli_out_writef(stdout, "prefill_ready: false\n");
            yvex_cli_out_writef(stdout, "logits_ready: false\n");
            yvex_cli_out_writef(stdout, "generation: unsupported\n");
            yvex_cli_out_writef(stdout, "status: real-segment-graph-executed\n");
        } else {
            graph_guard_apply_partial_result(&graph_guard, &partial_result, 1);
            print_graph_guard_report(&graph_guard);
            yvex_cli_out_writef(stdout, "real_partial_graph_executed: true\n");
            yvex_cli_out_writef(stdout, "partial_graph_kind: %s\n", partial_result.segment_name);
            yvex_cli_out_writef(stdout, "partial_backend: %s\n", partial_result.backend_name);
            yvex_cli_out_writef(stdout, "partial_weight: %s\n", partial_result.weight_name);
            yvex_cli_out_writef(stdout, "partial_weight_dtype: %s\n", partial_result.weight_dtype);
            yvex_cli_out_writef(stdout, "partial_token: %u\n", partial_result.token_id);
            yvex_cli_out_writef(stdout, "partial_node_count: %llu\n", partial_result.node_count);
            yvex_cli_out_writef(stdout, "partial_output_dtype: %s\n", partial_result.output_dtype);
            yvex_cli_out_writef(stdout, "partial_output_count: %llu\n", partial_result.output_count);
            yvex_cli_out_writef(stdout, "partial_output_bytes: %llu\n", partial_result.output_bytes);
            yvex_cli_out_writef(stdout, "partial_output_checksum: %llu\n", partial_result.output_checksum);
            yvex_cli_out_writef(stdout, "partial_reference_checksum: %llu\n", partial_result.reference_checksum);
            yvex_cli_out_writef(stdout, "partial_max_abs_diff: %.9g\n", partial_result.max_abs_diff);
            yvex_cli_out_writef(stdout, "partial_output_sample_count: %llu\n", partial_result.output_value_count);
            yvex_cli_out_writef(stdout, "partial_output_sample_values:");
            for (i = 0; (unsigned long long)i < partial_result.output_value_count; ++i) {
                yvex_cli_out_writef(stdout, "%s%.9g", i == 0 ? " " : ",", (double)partial_result.output_values[i]);
            }
            yvex_cli_out_writef(stdout, "\n");
            yvex_cli_out_writef(stdout, "execution_ready: false\n");
            yvex_cli_out_writef(stdout, "graph_execution_ready: false\n");
            yvex_cli_out_writef(stdout, "prefill_ready: false\n");
            yvex_cli_out_writef(stdout, "logits_ready: false\n");
            yvex_cli_out_writef(stdout, "generation: unsupported\n");
            yvex_cli_out_writef(stdout, "status: real-partial-graph-executed\n");
        }

        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return 0;
    }

    rc = open_model_context(model_arg, &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_graph_build_for_model(&graph, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_graph_dump(graph, stdout, &err);
    }

    yvex_graph_close(graph);
    close_model_context(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

int yvex_graph_command(int argc, char **argv)
{
    return command_graph(argc, argv);
}

void yvex_graph_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda] [--execute-fixture] [--execute-partial] [--execute-segment --segment embedding-rmsnorm] [--partial-token N] [--tokens IDS --token-index N]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-op --op mlp --hidden-dim N --ffn-dim N --activation silu --gated [--experts N --expert-id N]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-block --block fixture --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]\n");
    yvex_cli_out_writef(fp, "       yvex graph --backend cpu|cuda --execute-layers --layers N --block fixture --seq-len N --position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]\n");
    yvex_cli_out_writef(fp, "       yvex graph check [--backend cpu|cuda] [--suite primitives|block|layers|all] [--layers N]\n");
    yvex_cli_out_writef(fp, "\nGraph commands run descriptor diagnostics, bounded fixture and selected real graph proofs, standalone primitive probes, one controlled transformer-block fixture, or a controlled layer scheduler fixture. They do not run inference or text generation.\n");
    yvex_cli_out_writef(fp, "Layer execution is a controlled fixture scheduler over repeated diagnostic blocks; it is not model prefill, decode, logits, sampling, or generation.\n");
    yvex_cli_out_writef(fp, "Graph check composes existing fixture proof commands only; it does not execute real model layers, prefill, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
}
