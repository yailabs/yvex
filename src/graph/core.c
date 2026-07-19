/* Owner: graph core and encoded attention operations.
 * Owns: graph lifecycle, value/op tables, shapes, encoded reads/dots, reduction, and output projection.
 * Does not own: planning, family schedules, transactions, reports, backend kernels, or generation.
 * Invariants: graph facts and positioned reads preserve admitted descriptor geometry.
 * Boundary: graph construction and bounded operations are not transformer or generation support.
 * Purpose: centralize reusable graph state and payload-bound attention computation.
 * Inputs: model descriptors, tensor tables, admitted bindings, vectors, and history views.
 * Effects: mutates owned graph tables and caller-provided bounded numeric outputs.
 * Failure: checked allocation, range, numeric, or payload refusal leaves outputs unpromoted. */
#include "src/graph/private.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static void graph_value_clear(yvex_graph_value_info *value);
static void graph_op_clear(yvex_graph_op_info *op);
static void graph_missing_clear(yvex_graph_missing_required *missing);
static int graph_add_value(yvex_graph *graph, yvex_value_kind kind,
                                const char *name, unsigned int rank,
                                const unsigned long long *dims,
                                yvex_dtype dtype, yvex_residency residency,
                                const char *source_tensor_name,
                                unsigned int *out_id, yvex_error *err);
static int graph_add_op(yvex_graph *graph, yvex_op_kind kind,
                             yvex_op_status status, const char *name,
                             const unsigned int *inputs,
                             unsigned int input_count,
                             const unsigned int *outputs,
                             unsigned int output_count, const char *reason,
                             yvex_error *err);
static int graph_add_missing(yvex_graph *graph, yvex_tensor_role role,
                                  const char *reason, yvex_error *err);
typedef struct {
    const yvex_attention_layer_plan *layer;
    const yvex_attention_history_view *history;
    const float *query;
    const float *current_kv;
    const float *current_compressed;
    const float *current_indexer;
    const float *index_query;
    const float *index_weights;
    const float *sinks;
    const unsigned long long *current_compressed_positions;
    const unsigned long long *current_indexer_positions;
    float *out;
    unsigned long long current_kv_stride;
    unsigned long long current_compressed_count;
    unsigned long long current_compressed_stride;
    unsigned long long current_indexer_count;
    unsigned long long current_indexer_stride;
    unsigned long long index_query_stride;
    unsigned long long index_weight_stride;
    unsigned long long token_count;
    unsigned long long token_position;
    unsigned long long compressed_total;
    unsigned long long *trace_topk_counts;
    unsigned long long *trace_topk_positions;
    unsigned long long trace_topk_stride;
    yvex_attention_cpu_result *result;
    yvex_attention_failure *failure;
    yvex_error *err;
    double scale;
} attention_reduce_context;
// Purpose: Execute the bounded reduce visit row transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int reduce_visit_row(const attention_reduce_context *context,
                            const float *query, const float *row,
                            double maximum, double *score_or_denominator,
                            float *destination, int accumulate)
{
    unsigned long long lane;
    double score = 0.0;
    if (!context || !query || !row || !score_or_denominator) return 0;
    for (lane = 0ull; lane < context->layer->head_dimension; ++lane) {
        if (!isfinite(query[lane]) || !isfinite(row[lane])) return 0;
        score += (double)query[lane] * (double)row[lane];
    }
    score *= context->scale;
    if (!isfinite(score)) return 0;
    if (!accumulate) {
        if (score > *score_or_denominator) *score_or_denominator = score;
        return 1;
    }
    score = exp(score - maximum);
    if (!isfinite(score) || !destination) return 0;
    *score_or_denominator += score;
    for (lane = 0ull; lane < context->layer->head_dimension; ++lane)
        destination[lane] += (float)(score * row[lane]);
    return 1;
}
// Purpose: Execute the bounded reduce local rows transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int reduce_local_rows(const attention_reduce_context *context,
                             unsigned long long token, const float *query,
                             double maximum, double *state,
                             float *destination, int accumulate)
{
    unsigned long long absolute = context->token_position + token;
    unsigned long long first =
        absolute + 1ull > context->layer->sliding_window
            ? absolute + 1ull - context->layer->sliding_window : 0ull;
    unsigned long long candidate;
    for (candidate = 0ull;
         candidate < context->history->local_tail_count; ++candidate) {
        unsigned long long position =
            context->history->local_positions[candidate];
        const float *row;
        if (position < first || position > absolute) continue;
        row = context->history->local_kv +
              candidate * context->history->local_kv_stride;
        if (!reduce_visit_row(context, query, row, maximum, state,
                              destination, accumulate))
            return 0;
    }
    for (candidate = 0ull; candidate <= token; ++candidate) {
        unsigned long long position = context->token_position + candidate;
        const float *row = context->current_kv +
                           candidate * context->current_kv_stride;
        if (position >= first &&
            !reduce_visit_row(context, query, row, maximum, state,
                              destination, accumulate))
            return 0;
    }
    return 1;
}
// Purpose: Validate reduce hca candidate valid against the admitted graph invariants.
static int reduce_hca_candidate_valid(const attention_reduce_context *context,
                                      unsigned long long candidate,
                                      unsigned long long absolute)
{
    unsigned long long position = yvex_attention_segment_position(
        context->history->compressed_positions,
        context->history->compressed_entry_count,
        context->current_compressed_positions,
        context->current_compressed_count, candidate);
    return position != ULLONG_MAX && position <= absolute &&
           position <= ULLONG_MAX - context->layer->compression_ratio + 1ull &&
           position + context->layer->compression_ratio - 1ull <= absolute;
}
// Purpose: Execute the bounded reduce compressed rows transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int reduce_compressed_rows(const attention_reduce_context *context,
                                  unsigned long long absolute,
                                  const unsigned long long *selected,
                                  unsigned long long selected_count,
                                  const float *query, double maximum,
                                  double *state, float *destination,
                                  int accumulate)
{
    unsigned long long count =
        context->layer->attention_class == YVEX_ATTENTION_CLASS_HCA
            ? context->compressed_total : selected_count;
    unsigned long long candidate;
    for (candidate = 0ull; candidate < count; ++candidate) {
        unsigned long long index =
            context->layer->attention_class == YVEX_ATTENTION_CLASS_HCA
                ? candidate : selected[candidate];
        const float *row;
        if (context->layer->attention_class == YVEX_ATTENTION_CLASS_HCA &&
            !reduce_hca_candidate_valid(context, index, absolute))
            continue;
        row = yvex_attention_segment_row(
            context->history->compressed_kv,
            context->history->compressed_entry_count,
            context->history->compressed_kv_stride,
            context->current_compressed,
            context->current_compressed_count,
            context->current_compressed_stride, index);
        if (!reduce_visit_row(context, query, row, maximum, state,
                              destination, accumulate))
            return 0;
    }
    return 1;
}
// Purpose: Execute the bounded reduce head transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int reduce_head(const attention_reduce_context *context,
                       unsigned long long token, unsigned long long head,
                       const unsigned long long *selected,
                       unsigned long long selected_count)
{
    unsigned long long absolute = context->token_position + token;
    const float *query = context->query +
        token * context->layer->query_heads * context->layer->head_dimension +
        head * context->layer->head_dimension;
    float *destination = context->out +
        token * context->layer->query_heads * context->layer->head_dimension +
        head * context->layer->head_dimension;
    double maximum = (double)context->sinks[head];
    double denominator;
    unsigned long long lane;
    if (!isfinite(maximum) ||
        !reduce_local_rows(context, token, query, maximum, &maximum, NULL, 0) ||
        !reduce_compressed_rows(context, absolute, selected, selected_count,
                                query, maximum, &maximum, NULL, 0))
        return 0;
    denominator = exp((double)context->sinks[head] - maximum);
    if (!isfinite(denominator)) return 0;
    memset(destination, 0,
           (size_t)context->layer->head_dimension * sizeof(*destination));
    if (!reduce_local_rows(context, token, query, maximum, &denominator,
                           destination, 1) ||
        !reduce_compressed_rows(context, absolute, selected, selected_count,
                                query, maximum, &denominator, destination, 1) ||
        !isfinite(denominator) || denominator <= 0.0)
        return 0;
    for (lane = 0ull; lane < context->layer->head_dimension; ++lane)
        destination[lane] = (float)((double)destination[lane] / denominator);
    return yvex_attention_rope_apply(
        destination, context->layer->head_dimension,
        context->layer->rope_head_dimension, absolute,
        &context->layer->position, 1);
}
// Purpose: Execute the bounded reduce select transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int reduce_select(const attention_reduce_context *context,
                         unsigned long long token,
                         unsigned long long *selected,
                         unsigned long long *selected_count)
{
    unsigned long long absolute = context->token_position + token;
    unsigned long long valid_count = 0ull;
    unsigned long long candidate;
    int rc = YVEX_OK;
    *selected_count = 0ull;
    if (context->layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        context->compressed_total) {
        rc = yvex_attention_csa_select(
            context->layer, context->history, context->current_indexer,
            context->current_indexer_count, context->current_indexer_stride,
            context->current_indexer_positions,
            context->index_query + token * context->index_query_stride,
            context->index_weights + token * context->index_weight_stride,
            absolute, selected, selected_count, &valid_count,
            context->failure, context->err);
        if (rc != YVEX_OK) return rc;
    } else if (context->layer->attention_class ==
                   YVEX_ATTENTION_CLASS_HCA) {
        for (candidate = 0ull; candidate < context->compressed_total; ++candidate)
            if (reduce_hca_candidate_valid(context, candidate, absolute))
                valid_count++;
        *selected_count = valid_count;
    }
    if (context->result) {
        if (valid_count > context->result->topk_candidates)
            context->result->topk_candidates = valid_count;
        if (*selected_count > context->result->topk_selected)
            context->result->topk_selected = *selected_count;
    }
    if (!context->trace_topk_counts) return YVEX_OK;
    if (*selected_count > context->trace_topk_stride ||
        (*selected_count && (!selected || !context->trace_topk_positions)))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA,
            NULL, context->layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            context->trace_topk_stride, *selected_count, context->err,
            YVEX_ERR_BOUNDS, "CSA trace top-k extent is smaller than selection");
    context->trace_topk_counts[token] = *selected_count;
    for (candidate = 0ull; candidate < *selected_count; ++candidate)
        context->trace_topk_positions[
            token * context->trace_topk_stride + candidate] =
            yvex_attention_segment_position(
                context->history->compressed_positions,
                context->history->compressed_entry_count,
                context->current_compressed_positions,
                context->current_compressed_count, selected[candidate]);
    return YVEX_OK;
}
// Purpose: Validate reduce context validate against the admitted graph invariants.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int reduce_context_validate(attention_reduce_context *context)
{
    if (!context->layer || !context->query ||
        !context->history || !context->current_kv || !context->sinks ||
        !context->out || !context->token_count)
        return yvex_attention_reject(
            context->failure,
            YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            context->layer ? context->layer->layer_index
                           : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
            YVEX_ERR_INVALID_ARG,
            "sparse attention requires plan, history, query, KV, and output");
    if (!yvex_core_u64_add(
            context->history->compressed_entry_count,
            context->current_compressed_count, &context->compressed_total))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            NULL, context->layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            ULLONG_MAX, context->history->compressed_entry_count,
            context->err, YVEX_ERR_BOUNDS,
            "sparse attention compressed candidate count overflowed");
    if (context->current_compressed_count &&
        (!context->current_compressed ||
         !context->current_compressed_positions ||
         context->current_compressed_stride <
             context->layer->head_dimension))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
            NULL, context->layer->layer_index,
            YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
            context->layer->head_dimension,
            context->current_compressed_stride, context->err,
            YVEX_ERR_FORMAT,
            "current compressed attention entries are incomplete");
    if (context->layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        (context->current_indexer_count != context->current_compressed_count ||
         (context->current_indexer_count &&
          (!context->current_indexer || !context->current_indexer_positions ||
           !context->index_query || !context->index_weights ||
           context->current_indexer_stride <
               context->layer->indexer_head_dimension))))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
            NULL, context->layer->layer_index,
            YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
            context->current_compressed_count,
            context->current_indexer_count, context->err, YVEX_ERR_FORMAT,
            "CSA current compressed and indexer entries are not paired");
    context->scale = 1.0 / sqrt((double)context->layer->head_dimension);
    return YVEX_OK;
}
// Purpose: Return the admitted reduce chunk fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_reduce_chunk(
    const yvex_attention_layer_plan *layer,
    const float *query, const yvex_attention_history_view *history,
    const float *current_kv, unsigned long long current_kv_stride,
    const float *current_compressed,
    unsigned long long current_compressed_count,
    unsigned long long current_compressed_stride,
    const unsigned long long *current_compressed_positions,
    const float *current_indexer, unsigned long long current_indexer_count,
    unsigned long long current_indexer_stride,
    const unsigned long long *current_indexer_positions,
    const float *index_query, unsigned long long index_query_stride,
    const float *index_weights, unsigned long long index_weight_stride,
    const float *sinks, unsigned long long token_count,
    unsigned long long token_position, float *out,
    unsigned long long *trace_topk_counts,
    unsigned long long *trace_topk_positions,
    unsigned long long trace_topk_stride,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err)
{
    attention_reduce_context context = {
        layer, history, query, current_kv, current_compressed,
        current_indexer, index_query, index_weights, sinks,
        current_compressed_positions, current_indexer_positions, out,
        current_kv_stride, current_compressed_count,
        current_compressed_stride, current_indexer_count,
        current_indexer_stride, index_query_stride, index_weight_stride,
        token_count, token_position, 0ull, trace_topk_counts,
        trace_topk_positions, trace_topk_stride, result, failure, err, 0.0
    };
    unsigned long long *selected = NULL;
    unsigned long long token;
    int rc = reduce_context_validate(&context);
    if (rc != YVEX_OK) return rc;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        context.compressed_total) {
        selected = (unsigned long long *)yvex_attention_calloc_array(
            yvex_attention_min_u64(context.compressed_total,
                                   layer->sparse_topk.k),
            sizeof(*selected));
        if (!selected)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                context.compressed_total, 0ull, err, YVEX_ERR_NOMEM,
                "sparse attention selection allocation failed");
    }
    for (token = 0ull; token < token_count; ++token) {
        unsigned long long selected_count = 0ull;
        unsigned long long head;
        rc = reduce_select(&context, token, selected, &selected_count);
        if (rc != YVEX_OK) break;
        for (head = 0ull; head < layer->query_heads; ++head) {
            if (!reduce_head(&context, token, head, selected,
                             selected_count)) {
                rc = yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    NULL, layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    1ull, token, err, YVEX_ERR_FORMAT,
                    "sparse attention score, softmax, or reduction became non-finite");
                break;
            }
        }
        if (rc != YVEX_OK) break;
    }
    free(selected);
    return rc;
}
// Purpose: Return the admitted output project fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_output_project(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *out_a,
    const yvex_runtime_tensor_binding *out_b,
    const float *attention_values,
    unsigned long long token_count,
    unsigned long long attention_stride,
    unsigned long long group_count,
    unsigned long long group_input_width,
    unsigned long long rank,
    unsigned long long hidden_width,
    float *out,
    unsigned long long output_stride,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    float *low = NULL;
    unsigned long long group;
    unsigned long long rows;
    int rc;
    if (!session || !out_a || !out_b || !attention_values || !out ||
        !token_count || attention_stride < group_count * group_input_width ||
        !group_count || !group_input_width || !rank || !hidden_width ||
        output_stride < hidden_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            out_a, YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_ATTENTION_OUT_A, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "attention batch output projection requires bindings and buffers");
    low = (float *)yvex_attention_calloc_array(token_count * group_count * rank,
                                          sizeof(float));
    if (!low)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, out_a,
            out_a->layer_index, YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
            token_count * group_count * rank, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention batch output scratch");
    for (group = 0ull; group < group_count; ++group) {
        rc = yvex_attention_dot_batch(
            session, out_a, group * rank,
            attention_values + group * group_input_width, token_count,
            attention_stride, group_input_width, rank, low + group * rank,
            group_count * rank, &rows, 0, result, failure, err);
        if (rc != YVEX_OK) {
            free(low);
            return rc;
        }
        if (rows != rank) {
            free(low);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, out_a,
                out_a->layer_index, YVEX_TENSOR_ROLE_ATTENTION_OUT_A, rank,
                rows, err, YVEX_ERR_FORMAT,
                "attention batch output A projection did not produce full group rank");
        }
    }
    rc = yvex_attention_dot_batch(
        session, out_b, 0ull, low, token_count, group_count * rank,
        group_count * rank, hidden_width, out, output_stride, &rows, 0,
        result, failure, err);
    free(low);
    if (rc != YVEX_OK) return rc;
    if (rows != hidden_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, out_b,
            out_b->layer_index, YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
            hidden_width, rows, err, YVEX_ERR_FORMAT,
            "attention batch output B projection did not produce full hidden width");
    return YVEX_OK;
}
// Purpose: Return the admitted fill activation fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_fill_activation(float *out, unsigned long long count,
                                      unsigned long long layer_index,
                                      unsigned long long token_position)
{
    unsigned long long i;
    for (i = 0ull; i < count; ++i) {
        unsigned long long mixed =
            (i * 1103515245ull) ^ (layer_index * 2654435761ull) ^
            (token_position * 97ull);
        int lane = (int)(mixed % 257ull) - 128;
        out[i] = (float)lane / 128.0f;
    }
}
// Purpose: Return the admitted decode flat fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_decode_flat(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    float *out,
    unsigned long long expected_elements,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row;
    unsigned long long total;
    int rc;
    if (!session || !runtime_binding || !runtime_binding->binding || !out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_ATTENTION_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention flat decode requires session, binding, and output");
    binding = runtime_binding->binding;
    if (!yvex_core_u64_mul(binding->row_width, binding->row_count,
                                   &total) ||
        total != expected_elements)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            expected_elements, total, err, YVEX_ERR_BOUNDS,
            "attention flat decode expected element count mismatch");
    for (row = 0ull; row < binding->row_count; ++row) {
        rc = yvex_attention_decode_row(session, runtime_binding, row,
                                  out + row * binding->row_width,
                                  binding->row_width, result, failure, err);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}
// Purpose: Return the admitted activation apply fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_activation_apply(
    const yvex_attention_activation_policy *policy,
    float *values,
    unsigned long long count,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    float *scratch = NULL;
    float *block_out = NULL;
    unsigned char *codes = NULL;
    unsigned long long block_width;
    unsigned long long offset;
    int rc = YVEX_OK;
    if (!policy || !policy->required) return YVEX_OK;
    if (!values || count == 0ull || policy->block_width == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "runtime activation policy requires values and block width");
    if (policy->block_axis != YVEX_ATTENTION_AXIS_FINAL_DIMENSION)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer_index, role,
            YVEX_ATTENTION_AXIS_FINAL_DIMENSION,
            (unsigned long long)policy->block_axis, err, YVEX_ERR_FORMAT,
            "runtime activation policy axis is unsupported");
    if (policy->pre_transform ==
        YVEX_ATTENTION_TRANSFORM_DAO_FHT_V1_1_0_POST2) {
        float scale = 1.0f / sqrtf((float)count);
        scratch = (float *)yvex_attention_calloc_array(count, sizeof(*scratch));
        if (!scratch)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer_index, role, count, 0ull, err, YVEX_ERR_NOMEM,
                "runtime activation Hadamard scratch allocation failed");
        rc = yvex_attention_hadamard_cpu(
            values, count, scale, 1, scratch, failure, err);
        if (rc != YVEX_OK) goto cleanup;
        memcpy(values, scratch, (size_t)count * sizeof(*values));
    } else if (policy->pre_transform !=
               YVEX_ATTENTION_TRANSFORM_NONE) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer_index, role, 0ull,
            (unsigned long long)policy->pre_transform, err,
            YVEX_ERR_FORMAT,
            "runtime activation transform is unsupported");
        goto cleanup;
    }
    block_width = policy->block_width;
    if (count % block_width != 0ull) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer_index, role, block_width, count, err, YVEX_ERR_BOUNDS,
            "runtime activation policy requires exact block divisibility");
        goto cleanup;
    }
    block_out = (float *)yvex_attention_calloc_array(block_width,
                                                sizeof(*block_out));
    codes = (unsigned char *)yvex_attention_calloc_array(block_width,
                                                    sizeof(*codes));
    if (!block_out || !codes) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer_index, role, block_width, 0ull, err, YVEX_ERR_NOMEM,
            "runtime activation codec scratch allocation failed");
        goto cleanup;
    }
    for (offset = 0ull; offset < count; offset += block_width) {
        unsigned char scale_code = 0u;
        if (policy->quantization ==
            YVEX_ATTENTION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT) {
            rc = yvex_attention_fp8_fake_quant_block(
                values + offset, block_width, block_out, codes, &scale_code,
                failure, err);
        } else if (policy->quantization ==
                   YVEX_ATTENTION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT) {
            rc = yvex_attention_fp4_fake_quant_block(
                values + offset, block_width, block_out, codes, &scale_code,
                failure, err);
        } else {
            rc = yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer_index, role, 0ull,
                (unsigned long long)policy->quantization, err,
                YVEX_ERR_FORMAT,
                "runtime activation quantization is unsupported");
        }
        if (rc != YVEX_OK) goto cleanup;
        memcpy(values + offset, block_out,
               (size_t)block_width * sizeof(*values));
    }
    yvex_error_clear(err);
cleanup:
    free(scratch);
    free(block_out);
    free(codes);
    return rc;
}
static int execute_update_reference_error(
    yvex_attention_cpu_result *result,
    double production,
    double reference);
static int execute_dot_row_range(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long start_row,
    const float *vector,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err);
// Purpose: Return the admitted result reset fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_result_reset(yvex_attention_cpu_result *result)
{
    if (result) memset(result, 0, sizeof(*result));
}
// Purpose: Return the admitted binding find fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_runtime_tensor_binding *yvex_attention_binding_find(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    unsigned long long layer_index)
{
    return yvex_runtime_descriptor_find_role(
        descriptor, role, YVEX_TENSOR_SCOPE_MAIN_LAYER,
        layer_index, YVEX_ATTENTION_NO_TENSOR_INDEX);
}
// Purpose: Implement the graph-local row bytes semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_row_bytes(
    const yvex_materialized_tensor_binding *binding,
    unsigned long long *row_bytes_out,
    unsigned long long *row_count_out,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long blocks;
    unsigned long long row_bytes;
    if (!binding || !row_bytes_out || !row_count_out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "attention row-byte calculation requires binding and outputs");
    if (!binding->row_width || !binding->row_count ||
        !binding->block_size || !binding->bytes_per_block ||
        binding->row_width % binding->block_size != 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            binding->layer_index, binding->role, binding->block_size,
            binding->row_width, err, YVEX_ERR_FORMAT,
            "attention binding row geometry is invalid");
    blocks = binding->row_width / binding->block_size;
    if (!yvex_core_u64_mul(blocks, binding->bytes_per_block,
                                   &row_bytes) ||
        row_bytes == 0ull ||
        row_bytes > (unsigned long long)SIZE_MAX)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            binding->layer_index, binding->role, ULLONG_MAX, row_bytes,
            err, YVEX_ERR_BOUNDS,
            "attention binding row byte size overflowed");
    if (binding->encoded_bytes / row_bytes < binding->row_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            binding->layer_index, binding->role,
            binding->row_count * row_bytes, binding->encoded_bytes,
            err, YVEX_ERR_BOUNDS,
            "attention binding encoded bytes do not cover declared rows");
    *row_bytes_out = row_bytes;
    *row_count_out = binding->row_count;
    return YVEX_OK;
}
// Purpose: Return the admitted decode row fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_decode_row(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long row_index,
    float *out,
    unsigned long long out_elements,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long row_count;
    unsigned long long block_count;
    unsigned long long block;
    unsigned char *encoded = NULL;
    int rc;
    if (!session || !runtime_binding || !runtime_binding->binding || !out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_ATTENTION_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention row decode requires session, binding, and output");
    binding = runtime_binding->binding;
    rc = attention_row_bytes(binding, &row_bytes, &row_count, failure, err);
    if (rc != YVEX_OK) return rc;
    if (row_index >= row_count || out_elements != binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->row_width, out_elements, err, YVEX_ERR_BOUNDS,
            "attention row decode output shape does not match tensor row");
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (!encoded)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            row_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention encoded row scratch");
    rc = yvex_materialization_session_read(
        session, binding, row_index * row_bytes, encoded, (size_t)row_bytes,
        NULL, err);
    if (rc != YVEX_OK) {
        free(encoded);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ, runtime_binding,
            binding->layer_index, binding->role, row_bytes, 0ull, err, rc,
            "failed to read attention encoded row");
    }
    if (result) result->payload_bytes_read += row_bytes;
    block_count = binding->row_width / binding->block_size;
    for (block = 0ull; block < block_count; ++block) {
        yvex_quant_failure qfailure;
        rc = yvex_quant_decode_block(
            binding->qtype,
            encoded + (size_t)(block * binding->bytes_per_block),
            (size_t)binding->bytes_per_block,
            out + (block * binding->block_size),
            binding->block_size, &qfailure, err);
        if (rc != YVEX_OK) {
            free(encoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                runtime_binding, binding->layer_index, binding->role,
                binding->block_size, qfailure.actual, err, rc,
                "failed to decode attention qtype row");
        }
    }
    free(encoded);
    return YVEX_OK;
}
// Purpose: Execute the bounded execute dot row range transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int execute_dot_row_range(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long start_row,
    const float *vector,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long row_count;
    unsigned long long rows;
    unsigned long long row;
    unsigned char *encoded = NULL;
    float *decoded = NULL;
    int rc;
    if (!session || !runtime_binding || !runtime_binding->binding ||
        !vector || !out || !rows_out)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_ATTENTION_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention dot rows require session, binding, vector, and output");
    binding = runtime_binding->binding;
    rc = attention_row_bytes(binding, &row_bytes, &row_count, failure, err);
    if (rc != YVEX_OK) return rc;
    if (vector_len != binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->row_width, vector_len, err, YVEX_ERR_BOUNDS,
            "attention dot vector length does not match tensor row width");
    if (start_row >= row_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            row_count, start_row, err, YVEX_ERR_BOUNDS,
            "attention dot row range starts beyond tensor rows");
    rows = yvex_attention_min_u64(max_rows ? max_rows : row_count - start_row,
                             row_count - start_row);
    if (rows == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            1ull, 0ull, err, YVEX_ERR_BOUNDS,
            "attention dot requires at least one output row");
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (collect_reference_metrics)
        decoded = (float *)yvex_attention_calloc_array(vector_len, sizeof(float));
    if (!encoded || (collect_reference_metrics && !decoded)) {
        free(encoded);
        free(decoded);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            row_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention dot row scratch");
    }
    for (row = 0ull; row < rows; ++row) {
        yvex_quant_failure qfailure;
        unsigned long long block_count;
        unsigned long long block;
        unsigned long long lane;
        double reference = 0.0;
        rc = yvex_materialization_session_read(
            session, binding, (start_row + row) * row_bytes, encoded,
            (size_t)row_bytes, NULL, err);
        if (rc != YVEX_OK) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ,
                runtime_binding, binding->layer_index, binding->role,
                row_bytes, 0ull, err, rc,
                "failed to read attention dot encoded row");
        }
        if (result) result->payload_bytes_read += row_bytes;
        rc = yvex_quant_cpu_dot(binding->qtype, encoded, (size_t)row_bytes,
                                vector, vector_len, &out[row], &qfailure, err);
        if (rc != YVEX_OK) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                runtime_binding, binding->layer_index, binding->role,
                vector_len, qfailure.actual, err, rc,
                "attention encoded row dot failed");
        }
        if (!isfinite(out[row])) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                runtime_binding, binding->layer_index, binding->role,
                1ull, row, err, YVEX_ERR_FORMAT,
                "attention encoded row dot produced non-finite output");
        }
        if (collect_reference_metrics) {
            block_count = binding->row_width / binding->block_size;
            for (block = 0ull; block < block_count; ++block) {
                rc = yvex_quant_decode_block(
                    binding->qtype,
                    encoded + (size_t)(block * binding->bytes_per_block),
                    (size_t)binding->bytes_per_block,
                    decoded + (block * binding->block_size),
                    binding->block_size, &qfailure, err);
                if (rc != YVEX_OK) {
                    free(encoded);
                    free(decoded);
                    return yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        runtime_binding, binding->layer_index, binding->role,
                        binding->block_size, qfailure.actual, err, rc,
                        "attention reference decode failed");
                }
            }
            for (lane = 0ull; lane < vector_len; ++lane)
                reference += (double)decoded[lane] * (double)vector[lane];
            if (!execute_update_reference_error(result, (double)out[row],
                                                  reference)) {
                free(encoded);
                free(decoded);
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    runtime_binding, binding->layer_index, binding->role,
                    1ull, row, err, YVEX_ERR_FORMAT,
                    "attention reference comparison produced non-finite metrics");
            }
        }
    }
    free(encoded);
    free(decoded);
    *rows_out = rows;
    return YVEX_OK;
}
// Purpose: Return the admitted dot rows fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_dot_rows(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    const float *vector,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    return execute_dot_row_range(
        session, runtime_binding, 0ull, vector, vector_len, max_rows, out,
        rows_out, collect_reference_metrics, result, failure, err);
}
// Purpose: Return the admitted dot batch fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_dot_batch(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    unsigned long long start_row,
    const float *vectors,
    unsigned long long token_count,
    unsigned long long vector_stride,
    unsigned long long vector_len,
    unsigned long long max_rows,
    float *out,
    unsigned long long output_stride,
    unsigned long long *rows_out,
    int collect_reference_metrics,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long row_count;
    unsigned long long rows;
    unsigned long long row;
    unsigned char *encoded = NULL;
    float *decoded = NULL;
    int rc;
    if (!session || !runtime_binding || !runtime_binding->binding ||
        !vectors || !out || !rows_out || token_count == 0ull ||
        vector_stride < vector_len || output_stride < max_rows)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_ATTENTION_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "attention batch dot rows require session, binding, vectors, and output");
    binding = runtime_binding->binding;
    rc = attention_row_bytes(binding, &row_bytes, &row_count, failure, err);
    if (rc != YVEX_OK) return rc;
    if (vector_len != binding->row_width)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->row_width, vector_len, err, YVEX_ERR_BOUNDS,
            "attention batch dot vector length does not match tensor row width");
    if (start_row >= row_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            row_count, start_row, err, YVEX_ERR_BOUNDS,
            "attention batch dot row range starts beyond tensor rows");
    rows = yvex_attention_min_u64(max_rows ? max_rows : row_count - start_row,
                             row_count - start_row);
    if (rows == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            1ull, 0ull, err, YVEX_ERR_BOUNDS,
            "attention batch dot requires at least one output row");
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (collect_reference_metrics)
        decoded = (float *)yvex_attention_calloc_array(vector_len, sizeof(float));
    if (!encoded || (collect_reference_metrics && !decoded)) {
        free(encoded);
        free(decoded);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            row_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "failed to allocate attention batch dot row scratch");
    }
    for (row = 0ull; row < rows; ++row) {
        yvex_quant_failure qfailure;
        unsigned long long token;
        unsigned long long block_count = 0ull;
        unsigned long long block;
        rc = yvex_materialization_session_read(
            session, binding, (start_row + row) * row_bytes, encoded,
            (size_t)row_bytes, NULL, err);
        if (rc != YVEX_OK) {
            free(encoded);
            free(decoded);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ,
                runtime_binding, binding->layer_index, binding->role,
                row_bytes, 0ull, err, rc,
                "failed to read attention batch dot encoded row");
        }
        if (result) result->payload_bytes_read += row_bytes;
        if (collect_reference_metrics) {
            block_count = binding->row_width / binding->block_size;
            for (block = 0ull; block < block_count; ++block) {
                rc = yvex_quant_decode_block(
                    binding->qtype,
                    encoded + (size_t)(block * binding->bytes_per_block),
                    (size_t)binding->bytes_per_block,
                    decoded + (block * binding->block_size),
                    binding->block_size, &qfailure, err);
                if (rc != YVEX_OK) {
                    free(encoded);
                    free(decoded);
                    return yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        runtime_binding, binding->layer_index, binding->role,
                        binding->block_size, qfailure.actual, err, rc,
                        "attention batch reference decode failed");
                }
            }
        }
        for (token = 0ull; token < token_count; ++token) {
            const float *vector = vectors + token * vector_stride;
            float *dst = out + token * output_stride + row;
            rc = yvex_quant_cpu_dot(binding->qtype, encoded,
                                    (size_t)row_bytes, vector, vector_len,
                                    dst, &qfailure, err);
            if (rc != YVEX_OK) {
                free(encoded);
                free(decoded);
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    runtime_binding, binding->layer_index, binding->role,
                    vector_len, qfailure.actual, err, rc,
                    "attention batch encoded row dot failed");
            }
            if (!isfinite(*dst)) {
                free(encoded);
                free(decoded);
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    runtime_binding, binding->layer_index, binding->role,
                    1ull, row, err, YVEX_ERR_FORMAT,
                    "attention batch encoded row dot produced non-finite output");
            }
            if (collect_reference_metrics) {
                unsigned long long lane;
                double reference = 0.0;
                for (lane = 0ull; lane < vector_len; ++lane)
                    reference +=
                        (double)decoded[lane] * (double)vector[lane];
                if (!execute_update_reference_error(
                        result, (double)*dst, reference)) {
                    free(encoded);
                    free(decoded);
                    return yvex_attention_reject(
                        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        runtime_binding, binding->layer_index, binding->role,
                        1ull, row, err, YVEX_ERR_FORMAT,
                        "attention batch reference comparison produced non-finite metrics");
                }
            }
        }
    }
    free(encoded);
    free(decoded);
    *rows_out = rows;
    return YVEX_OK;
}
// Purpose: Return the admitted checksum fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
double yvex_attention_checksum(const float *values,
                                 unsigned long long count)
{
    unsigned long long i;
    double sum = 0.0;
    if (!values) return 0.0;
    for (i = 0ull; i < count; ++i) {
        double weight = (double)((i % 29ull) + 1ull) / 29.0;
        sum += (double)values[i] * weight;
    }
    return sum;
}
// Purpose: Execute the bounded execute update reference error transformation over admitted inputs.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int execute_update_reference_error(
    yvex_attention_cpu_result *result,
    double production,
    double reference)
{
    double diff;
    double abs_diff;
    double denom;
    if (!result || !isfinite(production) || !isfinite(reference))
        return 0;
    diff = production - reference;
    abs_diff = fabs(diff);
    denom = fabs(reference);
    if (denom < 1e-12) denom = 1.0;
    if (!isfinite(abs_diff) || !isfinite(abs_diff / denom))
        return 0;
    if (abs_diff > result->max_abs_error)
        result->max_abs_error = abs_diff;
    if ((abs_diff / denom) > result->max_relative_error)
        result->max_relative_error = abs_diff / denom;
    result->rmse += diff * diff;
    result->reference_comparisons++;
    return 1;
}
// Purpose: Release graph-owned resources held by close.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_graph_close(yvex_graph *graph)
{
    unsigned long long i;
    if (!graph) {
        return;
    }
    free(graph->architecture);
    free(graph->model_name);
    for (i = 0; i < graph->value_count; ++i) {
        graph_value_clear(&graph->values[i]);
    }
    for (i = 0; i < graph->op_count; ++i) {
        graph_op_clear(&graph->ops[i]);
    }
    for (i = 0; i < graph->missing_count; ++i) {
        graph_missing_clear(&graph->missing[i]);
    }
    free(graph->values);
    free(graph->ops);
    free(graph->edges);
    free(graph->missing);
    free(graph);
}
// Purpose: Return the admitted status of fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
yvex_graph_status yvex_graph_status_of(const yvex_graph *graph)
{
    return graph ? graph->status : YVEX_GRAPH_STATUS_EMPTY;
}
// Purpose: Project the stable textual ABI label for a graph construction status.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Return the admitted value count fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_graph_value_count(const yvex_graph *graph)
{
    return graph ? graph->value_count : 0;
}
// Purpose: Return the admitted op count fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_graph_op_count(const yvex_graph *graph)
{
    return graph ? graph->op_count : 0;
}
// Purpose: Return the admitted missing required count fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_graph_missing_required_count(const yvex_graph *graph)
{
    return graph ? graph->missing_count : 0;
}
// Purpose: Return the admitted value at fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_graph_value_info *yvex_graph_value_at(const yvex_graph *graph,
                                                 unsigned long long index)
{
    if (!graph || index >= graph->value_count) {
        return NULL;
    }
    return &graph->values[index];
}
// Purpose: Return the admitted op at fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_graph_op_info *yvex_graph_op_at(const yvex_graph *graph,
                                           unsigned long long index)
{
    if (!graph || index >= graph->op_count) {
        return NULL;
    }
    return &graph->ops[index];
}
// Purpose: Return the admitted missing required at fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_graph_missing_required *yvex_graph_missing_required_at(const yvex_graph *graph,
                                                                  unsigned long long index)
{
    if (!graph || index >= graph->missing_count) {
        return NULL;
    }
    return &graph->missing[index];
}
// Purpose: Implement the graph-local find role semantic operation.
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
// Purpose: Apply the checked graph-local add required diagnostics invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int add_required_diagnostics(yvex_graph *graph,
                                    const yvex_tensor_info *token_embedding,
                                    const yvex_tensor_info *output_norm,
                                    const yvex_tensor_info *output_head,
                                    yvex_error *err)
{
    int rc;
    if (!token_embedding) {
        rc = graph_add_missing(graph, YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
                                    "required for token embedding", err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (!output_norm) {
        rc = graph_add_missing(graph, YVEX_TENSOR_ROLE_OUTPUT_NORM,
                                    "required for final normalization", err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (!output_head) {
        rc = graph_add_missing(graph, YVEX_TENSOR_ROLE_OUTPUT_HEAD,
                                    "required for logits", err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    return YVEX_OK;
}
// Purpose: Apply the checked graph-local add embedding path invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
    rc = graph_add_value(graph, YVEX_VALUE_TOKEN_IDS, "token_ids", 1,
                              token_shape, YVEX_DTYPE_I32, YVEX_RESIDENCY_HOST,
                              NULL, &token_ids_value, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = graph_add_value(graph, YVEX_VALUE_WEIGHT, token_embedding->name,
                              token_embedding->rank, token_embedding->dims,
                              token_embedding->dtype, YVEX_RESIDENCY_HOST,
                              token_embedding->name, &weight_value, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    hidden_shape[0] = sequence_length;
    hidden_shape[1] = token_embedding->dims[0];
    rc = graph_add_value(graph, YVEX_VALUE_ACTIVATION, "hidden", 2,
                              hidden_shape, token_embedding->dtype,
                              YVEX_RESIDENCY_HOST, NULL, &hidden_value, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    inputs[0] = token_ids_value;
    inputs[1] = weight_value;
    outputs[0] = hidden_value;
    return graph_add_op(graph, YVEX_OP_EMBED, YVEX_OP_STATUS_PLANNED,
                             "embed", inputs, 2, outputs, 1, "", err);
}
// Purpose: Construct build for model with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
    graph->architecture = yvex_core_strdup(yvex_arch_name(yvex_model_arch(model)));
    graph->model_name = yvex_core_strdup(yvex_model_name(model));
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
// Purpose: Project the stable textual ABI label for op kind name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Project the stable textual ABI label for op status name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Project the stable textual ABI label for value kind name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Project the stable textual ABI label for residency name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const char *yvex_residency_name(yvex_residency residency)
{
    switch (residency) {
    case YVEX_RESIDENCY_HOST: return "host";
    case YVEX_RESIDENCY_DEVICE: return "device";
    case YVEX_RESIDENCY_BACKEND_DECIDES: return "backend_decides";
    }
    return "unknown";
}
// Purpose: Apply the checked graph-local shape product invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Apply the checked graph-local shape equal invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Apply the checked graph-local shape copy invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Release graph-owned resources held by value clear.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void graph_value_clear(yvex_graph_value_info *value)
{
    if (!value) {
        return;
    }
    free((char *)value->name);
    free((char *)value->source_tensor_name);
    memset(value, 0, sizeof(*value));
}
// Purpose: Release graph-owned resources held by op clear.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void graph_op_clear(yvex_graph_op_info *op)
{
    if (!op) {
        return;
    }
    free((char *)op->name);
    free((char *)op->reason);
    memset(op, 0, sizeof(*op));
}
// Purpose: Release graph-owned resources held by missing clear.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void graph_missing_clear(yvex_graph_missing_required *missing)
{
    if (!missing) {
        return;
    }
    free((char *)missing->role_name);
    free((char *)missing->reason);
    memset(missing, 0, sizeof(*missing));
}
// Purpose: Construct reserve values with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Construct reserve ops with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Construct reserve missing with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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
// Purpose: Apply the checked graph-local add value invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int graph_add_value(yvex_graph *graph,
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
    value->name = yvex_core_strdup(name);
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
        value->source_tensor_name = yvex_core_strdup(source_tensor_name);
        if (!value->source_tensor_name) {
            graph_value_clear(value);
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
// Purpose: Apply the checked graph-local add op invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int graph_add_op(yvex_graph *graph,
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
    op->name = yvex_core_strdup(name);
    op->reason = yvex_core_strdup(reason ? reason : "");
    if (!op->name || !op->reason) {
        graph_op_clear(op);
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
// Purpose: Apply the checked graph-local add missing invariant.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int graph_add_missing(yvex_graph *graph,
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
    missing->role_name = yvex_core_strdup(yvex_tensor_role_name(role));
    missing->reason = yvex_core_strdup(reason ? reason : "");
    if (!missing->role_name || !missing->reason) {
        graph_missing_clear(missing);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "failed to copy missing diagnostic");
        return YVEX_ERR_NOMEM;
    }
    graph->missing_count += 1u;
    return YVEX_OK;
}
// Purpose: Return the admitted op edges at fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index)
{
    if (!graph || index >= graph->op_count) {
        return NULL;
    }
    return &graph->edges[index];
}
