/*
 * yvex_graph.c - Graph values, shapes, memory plans, and planner output.
 *
 * This file owns graph construction and planning data structures. It does not
 * execute graph operations.
 */

#include <yvex/yvex.h>
#include <stdlib.h>
#include <limits.h>
#include <yvex/op.h>
#include <string.h>
#include <yvex/graph.h>
#include <stdint.h>

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

    fprintf(fp, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            fprintf(fp, ",");
        }
        fprintf(fp, "%llu", dims[i]);
    }
    fprintf(fp, "]");
}

static void dump_edge_list(FILE *fp, const unsigned int *ids, unsigned int count)
{
    unsigned int i;

    fprintf(fp, "[");
    if (!ids) {
        fprintf(fp, "]");
        return;
    }
    for (i = 0; i < count; ++i) {
        if (i > 0) {
            fprintf(fp, ",");
        }
        fprintf(fp, "%u", ids[i]);
    }
    fprintf(fp, "]");
}

int yvex_graph_dump(const yvex_graph *graph, FILE *fp, yvex_error *err)
{
    unsigned long long i;

    if (!graph || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_dump", "graph and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "graph status: %s\n", yvex_graph_status_name(graph->status));
    fprintf(fp, "architecture: %s\n", graph->architecture ? graph->architecture : "unknown");
    fprintf(fp, "model_name: %s\n", graph->model_name ? graph->model_name : "");
    fprintf(fp, "values: %llu\n", graph->value_count);
    fprintf(fp, "ops: %llu\n", graph->op_count);
    fprintf(fp, "missing_required: %llu\n", graph->missing_count);
    fprintf(fp, "\n");

    for (i = 0; i < graph->value_count; ++i) {
        const yvex_graph_value_info *value = &graph->values[i];
        fprintf(fp, "value %u %s kind=%s shape=",
                value->id,
                value->name ? value->name : "",
                yvex_value_kind_name(value->kind));
        dump_shape(fp, value->dims, value->rank);
        fprintf(fp, " dtype=%s residency=%s",
                yvex_dtype_name(value->dtype),
                yvex_residency_name(value->residency));
        if (value->source_tensor_name && value->source_tensor_name[0] != '\0') {
            fprintf(fp, " source=%s", value->source_tensor_name);
        }
        fprintf(fp, "\n");
    }
    if (graph->value_count > 0 || graph->op_count > 0 || graph->missing_count > 0) {
        fprintf(fp, "\n");
    }

    for (i = 0; i < graph->op_count; ++i) {
        const yvex_graph_op_info *op = &graph->ops[i];
        const yvex_graph_op_edges *edges = yvex_graph_op_edges_at(graph, i);
        fprintf(fp, "op %u %s status=%s inputs=",
                op->id,
                op->name ? op->name : yvex_op_kind_name(op->kind),
                yvex_op_status_name(op->status));
        dump_edge_list(fp, edges ? edges->input_ids : NULL, op->input_count);
        fprintf(fp, " outputs=");
        dump_edge_list(fp, edges ? edges->output_ids : NULL, op->output_count);
        if (op->reason && op->reason[0] != '\0') {
            fprintf(fp, " reason=\"%s\"", op->reason);
        }
        fprintf(fp, "\n");
    }
    if (graph->op_count > 0 && graph->missing_count > 0) {
        fprintf(fp, "\n");
    }

    for (i = 0; i < graph->missing_count; ++i) {
        const yvex_graph_missing_required *missing = &graph->missing[i];
        fprintf(fp, "missing %s reason=\"%s\"\n",
                missing->role_name ? missing->role_name : yvex_tensor_role_name(missing->role),
                missing->reason ? missing->reason : "");
    }
    fprintf(fp, "status: graph-%s\n", yvex_graph_status_name(graph->status));

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

    fprintf(fp, "memory:\n");
    fprintf(fp, "  model_tensor_bytes_known: %llu\n", plan->summary.model_tensor_bytes_known);
    fprintf(fp, "  model_tensor_bytes_unknown_count: %llu\n",
            plan->summary.model_tensor_bytes_unknown_count);
    fprintf(fp, "  activation_peak_bytes: %llu\n", plan->summary.activation_peak_bytes);
    fprintf(fp, "  kv_cache_bytes: %llu\n", plan->summary.kv_cache_bytes);
    fprintf(fp, "  scratch_peak_bytes: %llu\n", plan->summary.scratch_peak_bytes);
    fprintf(fp, "  total_known_bytes: %llu\n", plan->summary.total_known_bytes);

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

static int fill_backend_status(yvex_plan *plan, const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options backend_options;
    int rc;

    if (strcmp(backend_name, "cpu") == 0 || strcmp(backend_name, "cuda") == 0) {
        memset(&backend_options, 0, sizeof(backend_options));
        backend_options.kind = strcmp(backend_name, "cuda") == 0
                                   ? YVEX_BACKEND_KIND_CUDA
                                   : YVEX_BACKEND_KIND_CPU;
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

    fprintf(fp, "plan status: %s\n", yvex_memory_plan_status_name(yvex_memory_plan_status_of(plan->memory)));
    fprintf(fp, "backend: %s\n", plan->backend_name ? plan->backend_name : "");
    fprintf(fp, "backend_status: %s\n", plan->backend_status ? plan->backend_status : "unknown");
    if (plan->backend_status && strcmp(plan->backend_status, "available") == 0) {
        fprintf(fp, "backend_capabilities:\n");
        fprintf(fp, "  tensor_alloc: %s\n", plan->backend_tensor_alloc ? "yes" : "no");
        fprintf(fp, "  tensor_read_write: %s\n", plan->backend_tensor_read_write ? "yes" : "no");
        fprintf(fp, "  op_embed: %s\n", plan->backend_op_embed ? "yes" : "no");
        fprintf(fp, "  op_matmul: %s\n", plan->backend_op_matmul ? "yes" : "no");
        fprintf(fp, "  op_mlp: %s\n", plan->backend_op_mlp ? "yes" : "no");
        fprintf(fp, "  op_rms_norm: %s\n", plan->backend_op_rms_norm ? "yes" : "no");
        fprintf(fp, "  op_rope: %s\n", plan->backend_op_rope ? "yes" : "no");
        fprintf(fp, "  op_attention: %s\n", plan->backend_op_attention ? "yes" : "no");
    }
    fprintf(fp, "architecture: %s\n", plan->graph->architecture ? plan->graph->architecture : "unknown");
    fprintf(fp, "model_name: %s\n", plan->graph->model_name ? plan->graph->model_name : "");
    fprintf(fp, "graph_status: %s\n", yvex_graph_status_name(plan->graph->status));
    fprintf(fp, "ops: %llu\n", plan->graph->op_count);
    fprintf(fp, "missing_required: %llu\n", plan->graph->missing_count);
    fprintf(fp, "\n");

    if (yvex_memory_plan_dump(plan->memory, fp, err) != YVEX_OK) {
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(fp, "\n");
    fprintf(fp, "execution_ready: false\n");
    if (plan->backend_status && strcmp(plan->backend_status, "unavailable") == 0) {
        fprintf(fp, "reason: CUDA runtime/device not available\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_PARTIAL) {
        fprintf(fp, "reason: graph partial; missing output_norm, output_head; backend lacks full graph ops\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_BUILT) {
        fprintf(fp, "reason: session execution not implemented\n");
    } else {
        fprintf(fp, "reason: graph unsupported; backend lacks full graph ops\n");
    }
    fprintf(fp, "status: plan-only\n");

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
