/* Owner: DeepSeek graph-family recipe.
 * Owns: schedule, role lowering, compressor recurrence, and CPU/CUDA composition.
 * Does not own: generic numerics, state transactions, backend kernels, KV, or generation.
 * Invariants: planning reads no payload; incomplete attention remains unsupported.
 * Boundary: family execution evidence cannot promote KV, transformer, or generation.
 * Purpose: compose admitted generic graph mechanisms for the DeepSeek-V4 schedule.
 * Inputs: immutable architecture, plan, materialization, descriptor, and history facts.
 * Effects: bounded execution mutates only owned transactions, traces, and results.
 * Failure: typed refusal unwinds scratch, state, loaded roles, and unpublished output. */
#include "src/graph/private.h"
#include <yvex/internal/families/deepseek_v4.h>
static const yvex_attention_cpu_options cpu_options_template = {
    .token_count = 1ull,
    .local_history_tokens = 4ull,
    .compressed_history_tokens = 8ull,
    .max_q_b_rows = 128ull,
    .max_kv_rows = 512ull,
    .max_compressor_rows = 32ull,
    .max_indexer_rows = 64ull,
    .scratch_limit_bytes = 64ull * 1024ull * 1024ull
};
/* Purpose: default the bounded CPU probe policy.
 * Inputs: optional caller-owned options; Effects: initializes options; Failure: none; Boundary: no execution. */
static void graph_cpu_options_default(yvex_attention_cpu_options *options)
{
    if (!options) return;
    *options = cpu_options_template;
}
/* Purpose: project the exact logical-model identity into the generic plan recipe. */
static int graph_recipe_identity(const void *context, char output[65])
{
    return yvex_model_register_deepseek_v4()->transform.architecture_identity(
        (const yvex_deepseek_v4_ir *)context, output);
}
// Purpose: Project the admitted DeepSeek recipe layer into the generic graph recipe.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_recipe_layer(const void *context, unsigned long long index,
    yvex_attention_layer_plan *out)
{
    const yvex_deepseek_v4_layer_spec *layer =
        yvex_model_register_deepseek_v4()->ir.layer_at(
            (const yvex_deepseek_v4_ir *)context, index);
    if (!layer || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->layer_index = layer->layer_index;
    out->attention_class = layer->attention_class;
    out->compression_ratio = layer->compression_ratio;
    out->sliding_window = layer->kv.sliding_window;
    out->query_heads = layer->query_heads;
    out->kv_heads = layer->kv_heads;
    out->head_dimension = layer->head_dimension;
    out->rope_head_dimension = layer->rope_head_dimension;
    out->query_lora_rank = layer->query_lora_rank;
    out->output_lora_rank = layer->output_lora_rank;
    out->output_groups = layer->output_groups;
    out->hidden_dimension = layer->tensors.q_a_columns;
    out->indexer_heads = layer->indexer_heads;
    out->indexer_head_dimension = layer->indexer_head_dimension;
    out->indexer_topk = layer->indexer_topk;
    out->compressor_ape_columns = layer->tensors.compressor_ape_columns;
    out->indexer_ape_columns = layer->tensors.indexer_ape_columns;
    out->rms_norm_epsilon = layer->rms_norm_epsilon;
    out->compressor_required = layer->compressor_required;
    out->indexer_required = layer->indexer_required;
    out->position = layer->position;
    out->attention_kv_activation = layer->attention_kv_activation;
    out->compressor_activation = layer->compressor_activation;
    out->compressor_rotated_activation = layer->compressor_rotated_activation;
    out->indexer_query_activation = layer->indexer_query_activation;
    out->sparse_topk = layer->sparse_topk;
    return 1;
}
// Purpose: Compose the DeepSeek plan build recipe without redefining generic numerics.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_plan_build(yvex_attention_plan **out, const void *family_ir,
    const yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure, yvex_error *err)
{
    const yvex_deepseek_v4_ir *ir = (const yvex_deepseek_v4_ir *)family_ir;
    const yvex_deepseek_v4_model_spec *model =
        ir ? yvex_model_register_deepseek_v4()->ir.model(ir) : NULL;
    yvex_attention_recipe recipe;
    if (!model) return yvex_attention_plan_build(out, NULL, session, descriptor, failure, err);
    recipe = (yvex_attention_recipe){
        ir, yvex_model_register_deepseek_v4()->ir.layer_count(ir),
        model->auxiliary_layer_count, model->swa_layer_count, model->csa_layer_count,
        model->hca_layer_count,
        graph_recipe_identity, graph_recipe_layer
    };
    return yvex_attention_plan_build(out, &recipe, session, descriptor,
                                     failure, err);
}
// Purpose: Admit and prepare the DeepSeek context validate phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_context_validate(const yvex_attention_plan *plan, const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure, yvex_error *err)
{
    char identity[65];
    if (!ir || !graph_recipe_identity(ir, identity)) identity[0] = '\0';
    return yvex_attention_context_validate(plan, identity, session, descriptor,
                                           failure, err);
}
typedef struct {
    const yvex_attention_plan *plan;
    const yvex_deepseek_v4_ir *ir;
    yvex_materialization_session *session;
    const yvex_runtime_descriptor *descriptor;
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts;
    yvex_attention_cpu_result *result;
    yvex_attention_failure *failure;
    yvex_error *err;
    const yvex_attention_layer_plan *layer_plan;
    const yvex_deepseek_v4_layer_spec *layer;
    const yvex_runtime_tensor_binding *q_a, *q_a_norm, *q_b, *kv, *kv_norm;
    yvex_attention_state_delta delta;
    unsigned long long layer_index, hidden_width, q_rank;
    unsigned long long q_a_rows, q_b_rows, kv_rows;
    size_t scratch_bytes;
    float *hidden, *q_low, *q_norm_weights, *query;
    float *kv_values, *kv_norm_weights, *tmp;
} cpu_probe_context;
// Purpose: Admit and prepare the DeepSeek probe prepare phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_probe_prepare(cpu_probe_context *context)
{
    yvex_attention_result_reset(context->result);
    if (!context->opts) {
        graph_cpu_options_default(&context->defaults);
        context->opts = &context->defaults;
    }
    if (!context->plan || !context->ir || !context->session ||
        !context->descriptor || !context->result)
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            NULL, YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, context->err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention CPU probe requires plan, IR, session, descriptor, and result");
    int rc = graph_context_validate(
        context->plan, context->ir, context->session, context->descriptor,
        context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    if (context->opts->trace && context->opts->trace->owned)
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            NULL, context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            1ull, context->err, YVEX_ERR_STATE,
            "DeepSeek attention execution trace must be released before reuse");
    context->layer_index = context->opts->layer_index;
    context->layer_plan = yvex_attention_plan_layer_at(
        context->plan, context->layer_index);
    context->layer = yvex_model_register_deepseek_v4()->ir.layer_at(
        context->ir, context->layer_index);
    if (!context->layer_plan || !context->layer)
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE,
            NULL, context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            context->err, YVEX_ERR_BOUNDS,
            "DeepSeek attention CPU probe layer is absent");
    context->q_a = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A,
        context->layer_index);
    context->q_a_norm = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM,
        context->layer_index);
    context->q_b = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
        context->layer_index);
    context->kv = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV,
        context->layer_index);
    context->kv_norm = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM,
        context->layer_index);
    if (!context->q_a || !context->q_a_norm || !context->q_b ||
        !context->kv || !context->kv_norm)
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
            NULL, context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 5ull, 0ull,
            context->err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU probe requires q_a/q_norm/q_b/kv/kv_norm bindings");
    context->hidden_width = context->q_a->binding->row_width;
    context->q_rank = context->q_a->binding->row_count;
    if (context->q_rank != context->q_b->binding->row_width)
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            context->q_b, context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
            context->q_rank, context->q_b->binding->row_width, context->err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU probe q_b input width must match q_a rank");
    context->q_b_rows = yvex_attention_min_u64(
        context->opts->max_q_b_rows, context->q_b->binding->row_count);
    context->kv_rows = yvex_attention_min_u64(
        context->opts->max_kv_rows, context->kv->binding->row_count);
    if (!context->q_b_rows || !context->kv_rows)
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            context->err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU probe requires q_b and kv rows");
    unsigned long long elements = context->hidden_width + context->q_rank * 2ull +
        context->q_b_rows + context->kv_rows * 2ull +
        context->opts->max_compressor_rows + context->opts->max_indexer_rows;
    if (!yvex_attention_checked_size(elements, sizeof(float),
                                     &context->scratch_bytes) ||
        (context->opts->scratch_limit_bytes &&
         context->scratch_bytes > context->opts->scratch_limit_bytes))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            NULL, context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            context->opts->scratch_limit_bytes, context->scratch_bytes,
            context->err, YVEX_ERR_BOUNDS,
            "DeepSeek attention CPU probe scratch budget exceeded");
    context->hidden = yvex_attention_calloc_array(
        context->hidden_width, sizeof(float));
    context->q_low = yvex_attention_calloc_array(context->q_rank, sizeof(float));
    context->q_norm_weights = yvex_attention_calloc_array(
        context->q_rank, sizeof(float));
    context->query = yvex_attention_calloc_array(
        context->q_b_rows, sizeof(float));
    context->kv_values = yvex_attention_calloc_array(
        context->kv_rows, sizeof(float));
    context->kv_norm_weights = yvex_attention_calloc_array(
        context->kv_rows, sizeof(float));
    context->tmp = yvex_attention_calloc_array(
        yvex_attention_min_u64(
            context->opts->max_compressor_rows + context->opts->max_indexer_rows,
            1ull + context->opts->max_compressor_rows +
                context->opts->max_indexer_rows),
        sizeof(float));
    if (context->hidden && context->q_low && context->q_norm_weights &&
        context->query && context->kv_values && context->kv_norm_weights &&
        context->tmp)
        return YVEX_OK;
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->scratch_bytes,
        0ull, context->err, YVEX_ERR_NOMEM,
        "failed to allocate DeepSeek CPU probe scratch");
}
// Purpose: Compose generic graph mechanisms for the DeepSeek probe project phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_probe_project(cpu_probe_context *context)
{
    int rc;
    yvex_attention_fill_activation(
        context->hidden, context->hidden_width, context->layer_index,
        context->opts->token_position);
    rc = yvex_attention_dot_rows(
        context->session, context->q_a, context->hidden,
        context->hidden_width, context->q_rank, context->q_low,
        &context->q_a_rows, context->opts->collect_reference_metrics,
        context->result, context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_attention_decode_row(
        context->session, context->q_a_norm, 0ull, context->q_norm_weights,
        context->q_rank, context->result, context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_attention_rms_norm(
            context->q_low, context->q_rank, context->q_norm_weights,
            context->layer->rms_norm_epsilon))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
            context->q_a_norm, context->layer_index,
            YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, context->q_rank, 0ull,
            context->err, YVEX_ERR_FORMAT,
            "DeepSeek q_a RMS norm produced invalid values");
    rc = yvex_attention_dot_rows(
        context->session, context->q_b, context->q_low, context->q_rank,
        context->q_b_rows, context->query, &context->q_b_rows,
        context->opts->collect_reference_metrics, context->result,
        context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    if (context->q_b_rows >= context->layer_plan->head_dimension &&
        !yvex_attention_rope_apply(
            context->query, context->layer_plan->head_dimension,
            context->layer->rope_head_dimension, context->opts->token_position,
            &context->layer->position, 0))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
            context->q_b, context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
            1ull, 0ull, context->err, YVEX_ERR_FORMAT,
            "DeepSeek probe RoPE/YaRN application failed");
    rc = yvex_attention_dot_rows(
        context->session, context->kv, context->hidden, context->hidden_width,
        context->kv_rows, context->kv_values, &context->kv_rows,
        context->opts->collect_reference_metrics, context->result,
        context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_attention_decode_row(
        context->session, context->kv_norm, 0ull, context->kv_norm_weights,
        context->kv_rows, context->result, context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_attention_rms_norm(
            context->kv_values, context->kv_rows, context->kv_norm_weights,
            context->layer->rms_norm_epsilon))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
            context->kv_norm, context->layer_index,
            YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, context->kv_rows, 0ull,
            context->err, YVEX_ERR_FORMAT,
            "DeepSeek kv RMS norm produced invalid values");
    rc = yvex_attention_compressor_probe(
        context->layer_plan, context->session,
        context->descriptor, context->hidden, context->hidden_width,
        context->opts->token_position, context->result, context->failure,
        context->err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_attention_softmax_probe(
            context->query, context->q_b_rows, context->kv_values,
            context->kv_rows,
            context->opts->local_history_tokens
                ? context->opts->local_history_tokens : 1ull,
            context->layer->attention_class == YVEX_ATTENTION_CLASS_SWA
                ? 0ull : context->opts->compressed_history_tokens,
            context->layer->attention_class, context->layer_plan->sparse_topk.k,
            context->result, context->failure, context->err))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            context->err, YVEX_ERR_FORMAT,
            "DeepSeek attention softmax probe failed");
    rc = yvex_attention_state_delta_begin(
        context->layer_plan, context->opts->token_position, &context->delta,
        context->failure, context->err);
    if (rc != YVEX_OK) return rc;
    return yvex_attention_state_delta_commit(
        &context->delta, context->failure, context->err);
}
// Purpose: Publish the complete DeepSeek probe publish facts only after transactional success.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static void cpu_probe_publish(cpu_probe_context *context)
{
    yvex_attention_cpu_result *result = context->result;
    result->executed = 1;
    result->full_attention = 0;
    result->reference_independent = result->reference_comparisons ? 1 : 0;
    result->cuda_executed = 0;
    result->layer_index = context->layer_index;
    result->attention_class = context->layer->attention_class;
    result->token_position = context->opts->token_position;
    result->q_a_rows = context->q_a_rows;
    result->q_b_rows = context->q_b_rows;
    result->kv_rows = context->kv_rows;
    result->state_raw_entries = context->delta.raw_kv_entries;
    result->state_compressed_entries = context->delta.compressed_kv_entries;
    result->state_indexer_entries = context->delta.indexer_entries;
    result->q_projection_checksum = yvex_attention_checksum(
        context->q_low, context->q_rank);
    result->kv_projection_checksum = yvex_attention_checksum(
        context->kv_values, context->kv_rows);
    result->rope_checksum = yvex_attention_checksum(
        context->query, context->q_b_rows);
    if (result->reference_comparisons)
        result->rmse = sqrt(result->rmse / (double)result->reference_comparisons);
    yvex_error_clear(context->err);
    if (context->failure) memset(context->failure, 0, sizeof(*context->failure));
}
// Purpose: Compose generic graph mechanisms for the DeepSeek probe execute phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_cpu_probe_execute(const yvex_attention_plan *plan, const void *family_ir,
    yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options, yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err)
{
    cpu_probe_context context;
    int rc;
    memset(&context, 0, sizeof(context));
    context.plan = plan;
    context.ir = (const yvex_deepseek_v4_ir *)family_ir;
    context.session = session;
    context.descriptor = descriptor;
    context.opts = options;
    context.result = result;
    context.failure = failure;
    context.err = err;
    rc = cpu_probe_prepare(&context);
    if (rc == YVEX_OK) rc = cpu_probe_project(&context);
    if (rc == YVEX_OK) cpu_probe_publish(&context);
    free(context.hidden);
    free(context.q_low);
    free(context.q_norm_weights);
    free(context.query);
    free(context.kv_values);
    free(context.kv_norm_weights);
    free(context.tmp);
    return rc;
}
typedef struct {
    const yvex_attention_plan *plan;
    const yvex_deepseek_v4_ir *ir;
    yvex_materialization_session *session;
    const yvex_runtime_descriptor *descriptor;
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts;
    yvex_attention_cpu_result *result;
    yvex_attention_failure *failure;
    yvex_error *err;
    const yvex_attention_layer_plan *layer_plan;
    const yvex_deepseek_v4_layer_spec *layer;
    const yvex_runtime_tensor_binding *q_a, *q_a_norm, *q_b, *kv, *kv_norm;
    const yvex_runtime_tensor_binding *sinks, *out_a, *out_b;
    const yvex_runtime_tensor_binding *wkv, *wgate, *ape, *norm;
    const yvex_runtime_tensor_binding *index_wkv, *index_wgate;
    const yvex_runtime_tensor_binding *index_ape_binding, *index_norm_binding;
    const yvex_runtime_tensor_binding *index_q_binding, *index_weight_binding;
    yvex_attention_memory_sink sink;
    yvex_attention_state_transaction transaction;
    yvex_attention_history_view history;
    yvex_attention_component_span output_span, raw_kv_span;
    yvex_attention_component_span main_kv_span, main_score_span, compressed_span;
    yvex_attention_component_span index_kv_span, index_score_span, index_emit_span;
    yvex_attention_rolling_state_view main_before, main_current;
    yvex_attention_rolling_state_view index_before, index_current;
    yvex_attention_rolling_state_output main_after, index_after;
    const yvex_attention_component_span *committed_output, *committed_raw;
    const yvex_attention_component_span *committed_compressed, *committed_indexer;
    const yvex_attention_component_span *committed_main_kv_state;
    const yvex_attention_component_span *committed_main_score_state;
    const yvex_attention_component_span *committed_index_kv_state;
    const yvex_attention_component_span *committed_index_score_state;
    const char *committed_identity;
    unsigned long long layer_index, token_count, hidden_width, q_rank;
    unsigned long long query_width, kv_width, rows, token;
    unsigned long long emitted_count, index_emitted_count;
    unsigned long long hidden_elements, q_low_elements, query_elements, kv_elements;
    unsigned long long scratch_elements, scratch_term;
    size_t scratch_bytes;
    float *hidden, *q_low, *q_norm_weights, *query, *kv_norm_weights;
    float *sink_values, *attention_values;
    float *main_state_kv, *main_state_score, *main_kv, *main_score;
    float *main_ape, *main_norm;
    float *index_state_kv, *index_state_score, *index_kv, *index_score;
    float *index_ape, *index_norm, *index_query, *index_weights;
    unsigned long long *compressed_positions, *index_positions;
    unsigned long long *trace_topk_counts, *trace_topk_positions;
    unsigned long long trace_topk_stride;
    int rc;
} cpu_chunk_context;
// Purpose: Admit and prepare the DeepSeek chunk admit phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_admit(cpu_chunk_context *context)
{
yvex_attention_result_reset(context->result);
if (!context->opts) {
    graph_cpu_options_default(&context->defaults);
    context->opts = &context->defaults;
}
context->token_count = context->opts->token_count ? context->opts->token_count : 1ull;
if (!context->plan || !context->ir || !context->session || !context->descriptor || !context->result)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
        YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
        0ull, context->err, YVEX_ERR_INVALID_ARG,
        "DeepSeek attention CPU chunk requires plan, IR, session, descriptor, "
        "and result");
context->rc = graph_context_validate(
    context->plan, context->ir, context->session, context->descriptor, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
if (context->opts->trace && context->opts->trace->owned)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
        NULL, context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
        context->err, YVEX_ERR_STATE,
        "DeepSeek attention execution trace must be released before reuse");
context->layer_index = context->opts->layer_index;
context->layer_plan = yvex_attention_plan_layer_at(context->plan, context->layer_index);
context->layer = yvex_model_register_deepseek_v4()->ir.layer_at(context->ir, context->layer_index);
if (!context->layer_plan || !context->layer)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
        YVEX_ERR_BOUNDS, "DeepSeek attention CPU chunk layer is absent");
context->q_a = yvex_attention_binding_find(context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A,
                             context->layer_index);
context->q_a_norm = yvex_attention_binding_find(
    context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, context->layer_index);
context->q_b = yvex_attention_binding_find(context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
                             context->layer_index);
context->kv = yvex_attention_binding_find(context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV,
                            context->layer_index);
context->kv_norm = yvex_attention_binding_find(
    context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, context->layer_index);
context->sinks = yvex_attention_binding_find(context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_SINKS,
                               context->layer_index);
context->out_a = yvex_attention_binding_find(context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
                               context->layer_index);
context->out_b = yvex_attention_binding_find(context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
                               context->layer_index);
if (!context->q_a || !context->q_a_norm || !context->q_b || !context->kv ||
    !context->kv_norm || !context->sinks || !context->out_a ||
    !context->out_b)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 8ull, 0ull, context->err,
        YVEX_ERR_FORMAT,
        "DeepSeek attention CPU chunk requires complete Q/KV/sink/output bindings");
context->hidden_width = context->q_a->binding->row_width;
context->q_rank = context->q_a->binding->row_count;
context->query_width = context->q_b->binding->row_count;
context->kv_width = context->kv->binding->row_count;
if (context->q_rank != context->q_b->binding->row_width ||
    context->query_width != context->layer_plan->query_heads * context->layer_plan->head_dimension ||
    context->kv_width != context->layer_plan->head_dimension ||
    context->hidden_width != context->layer_plan->hidden_dimension)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, context->q_b,
        context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
        context->layer_plan->query_heads * context->layer_plan->head_dimension,
        context->query_width, context->err, YVEX_ERR_FORMAT,
        "DeepSeek attention CPU chunk tensor dimensions do not match plan");
if (!yvex_core_u64_mul(context->token_count, context->hidden_width,
                               &context->hidden_elements) ||
    !yvex_core_u64_mul(context->token_count, context->q_rank, &context->q_low_elements) ||
    !yvex_core_u64_mul(context->token_count, context->query_width,
                               &context->query_elements) ||
    !yvex_core_u64_mul(context->token_count, context->kv_width, &context->kv_elements) ||
    !yvex_core_u64_add(context->hidden_elements, context->q_low_elements,
                               &context->scratch_elements) ||
    !yvex_core_u64_add(context->scratch_elements, context->query_elements,
                               &context->scratch_elements) ||
    !yvex_core_u64_add(context->scratch_elements, context->kv_elements,
                               &context->scratch_elements) ||
    !yvex_core_u64_add(context->scratch_elements, context->query_elements,
                               &context->scratch_elements) ||
    !yvex_core_u64_add(context->q_rank, context->kv_width, &context->scratch_term) ||
    !yvex_core_u64_add(context->scratch_term, context->layer_plan->query_heads,
                               &context->scratch_term) ||
    !yvex_core_u64_add(context->scratch_elements, context->scratch_term,
                               &context->scratch_elements) ||
    !yvex_attention_checked_size(context->scratch_elements, sizeof(float),
                            &context->scratch_bytes) ||
    (context->opts->scratch_limit_bytes && context->scratch_bytes > context->opts->scratch_limit_bytes))
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->opts->scratch_limit_bytes,
        context->scratch_bytes, context->err, YVEX_ERR_BOUNDS,
        "DeepSeek attention CPU chunk scratch budget exceeded");
if (context->opts->history) {
    context->history = *context->opts->history;
    if (context->history.token_count != context->opts->token_position) {
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->opts->token_position,
            context->history.token_count, context->err, YVEX_ERR_STATE,
            "DeepSeek attention CPU chunk history is not contiguous");
    }
    context->main_before = context->history.main_rolling_state;
    context->index_before = context->history.indexer_rolling_state;
}
if (!context->opts->history &&
    context->layer_plan->attention_class != YVEX_ATTENTION_CLASS_SWA) {
    context->rc = yvex_attention_rolling_storage_allocate(
        context->layer_plan, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
        context->opts->token_position, &context->main_state_kv, &context->main_state_score,
        &context->main_before, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->history.main_rolling_state = context->main_before;
    if (context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_CSA) {
        context->rc = yvex_attention_rolling_storage_allocate(
            context->layer_plan, YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER,
            context->opts->token_position, &context->index_state_kv, &context->index_state_score,
            &context->index_before, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        context->history.indexer_rolling_state = context->index_before;
    }
}
context->history.immutable = 1;
context->history.token_count = context->opts->token_position;
context->rc = yvex_attention_memory_sink_init(&context->sink, NULL, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_state_transaction_begin(
    &context->sink, context->layer_plan, &context->history, context->opts->token_position, context->token_count,
    &context->transaction, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_state_transaction_acquire(
    &context->transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
    &context->output_span, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_state_transaction_acquire(
    &context->transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
    &context->raw_kv_span, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek chunk project phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_project(cpu_chunk_context *context)
{
context->hidden = (float *)yvex_attention_calloc_array(context->hidden_elements,
                                         sizeof(float));
context->q_low = (float *)yvex_attention_calloc_array(context->q_low_elements,
                                        sizeof(float));
context->q_norm_weights = (float *)yvex_attention_calloc_array(context->q_rank, sizeof(float));
context->query = (float *)yvex_attention_calloc_array(context->query_elements,
                                        sizeof(float));
context->kv_norm_weights = (float *)yvex_attention_calloc_array(context->kv_width, sizeof(float));
context->sink_values = (float *)yvex_attention_calloc_array(context->layer_plan->query_heads,
                                              sizeof(float));
context->attention_values = (float *)yvex_attention_calloc_array(context->query_elements,
                                                    sizeof(float));
if (!context->hidden || !context->q_low || !context->q_norm_weights || !context->query || !context->kv_norm_weights ||
    !context->sink_values || !context->attention_values) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->scratch_bytes, 0ull, context->err,
        YVEX_ERR_NOMEM,
        "failed to allocate DeepSeek CPU chunk scratch");
    return context->rc;
}
if (context->opts->input && context->opts->input_stride < context->hidden_width) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->hidden_width,
        context->opts->input_stride, context->err, YVEX_ERR_BOUNDS,
        "DeepSeek attention input stride is shorter than hidden width");
    return context->rc;
}
for (context->token = 0ull; context->token < context->token_count; ++context->token) {
    if (context->opts->input) {
        unsigned long long lane;
        const float *source = context->opts->input + context->token * context->opts->input_stride;
        for (lane = 0ull; lane < context->hidden_width; ++lane) {
            if (!isfinite(source[lane])) {
                context->rc = yvex_attention_reject(
                    context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    NULL, context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    context->hidden_width, lane, context->err, YVEX_ERR_FORMAT,
                    "DeepSeek attention input contains non-finite values");
                return context->rc;
            }
        }
        memcpy(context->hidden + context->token * context->hidden_width, source,
               (size_t)context->hidden_width * sizeof(*context->hidden));
    } else {
        yvex_attention_fill_activation(
            context->hidden + context->token * context->hidden_width, context->hidden_width, context->layer_index,
            context->opts->token_position + context->token);
    }
}
context->rc = yvex_attention_dot_batch(
    context->session, context->q_a, 0ull, context->hidden,
    context->token_count, context->hidden_width, context->hidden_width,
    context->q_rank, context->q_low, context->q_rank, &context->rows, context->opts->collect_reference_metrics,
    context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
if (context->rows != context->q_rank) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, context->q_a,
        context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_A, context->q_rank, context->rows, context->err,
        YVEX_ERR_FORMAT,
        "DeepSeek attention CPU chunk Q-A projection is incomplete");
    return context->rc;
}
context->rc = yvex_attention_decode_row(context->session, context->q_a_norm, 0ull, context->q_norm_weights,
                          context->q_rank, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
for (context->token = 0ull; context->token < context->token_count; ++context->token) {
    if (!yvex_attention_rms_norm(context->q_low + context->token * context->q_rank, context->q_rank,
                                  context->q_norm_weights,
                                  context->layer->rms_norm_epsilon)) {
        context->rc = yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->q_a_norm,
            context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, context->q_rank,
            context->token, context->err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk Q norm produced invalid values");
        return context->rc;
    }
}
context->rc = yvex_attention_dot_batch(
    context->session, context->q_b, 0ull, context->q_low,
    context->token_count, context->q_rank, context->q_rank,
    context->query_width,
    context->query, context->query_width, &context->rows, context->opts->collect_reference_metrics, context->result,
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_dot_batch(
    context->session, context->kv, 0ull, context->hidden,
    context->token_count, context->hidden_width, context->hidden_width,
    context->kv_width, (float *)context->raw_kv_span.data, context->raw_kv_span.stride, &context->rows,
    context->opts->collect_reference_metrics, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_decode_row(context->session, context->kv_norm, 0ull, context->kv_norm_weights,
                          context->kv_width, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
for (context->token = 0ull; context->token < context->token_count; ++context->token) {
    unsigned long long head;
    for (head = 0ull; head < context->layer_plan->query_heads; ++head) {
        if (!yvex_attention_unit_rms_norm(
                context->query + context->token * context->query_width +
                    head * context->layer_plan->head_dimension,
                context->layer_plan->head_dimension, context->layer->rms_norm_epsilon)) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->q_b,
                context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
                context->layer_plan->head_dimension, head, context->err, YVEX_ERR_FORMAT,
                "DeepSeek attention query head RMS norm failed");
            return context->rc;
        }
        if (!yvex_attention_rope_apply(
                context->query + context->token * context->query_width +
                    head * context->layer_plan->head_dimension,
                context->layer_plan->head_dimension, context->layer->rope_head_dimension,
                context->opts->token_position + context->token, &context->layer->position, 0)) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->q_b,
                context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_Q_B, 1ull, head,
                context->err, YVEX_ERR_FORMAT,
                "DeepSeek attention query RoPE/YaRN application failed");
            return context->rc;
        }
    }
    if (!yvex_attention_rms_norm(
            (float *)context->raw_kv_span.data + context->token * context->raw_kv_span.stride,
            context->kv_width, context->kv_norm_weights, context->layer->rms_norm_epsilon)) {
        context->rc = yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->kv_norm,
            context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, context->kv_width,
            context->token, context->err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk KV norm produced invalid values");
        return context->rc;
    }
    if (!yvex_attention_rope_apply(
            (float *)context->raw_kv_span.data + context->token * context->raw_kv_span.stride,
            context->kv_width, context->layer->rope_head_dimension,
            context->opts->token_position + context->token, &context->layer->position, 0)) {
        context->rc = yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->kv,
            context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_KV, 1ull, context->token, context->err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention KV RoPE/YaRN application failed");
        return context->rc;
    }
    if (context->kv_width > context->layer->rope_head_dimension) {
        context->rc = yvex_attention_activation_apply(
            &context->layer_plan->attention_kv_activation,
            (float *)context->raw_kv_span.data + context->token * context->raw_kv_span.stride,
            context->kv_width - context->layer->rope_head_dimension, context->layer_index,
            YVEX_TENSOR_ROLE_ATTENTION_KV, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
}
context->rc = yvex_attention_decode_flat(context->session, context->sinks, context->sink_values,
                           context->layer_plan->query_heads, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Admit and prepare the DeepSeek chunk main prepare phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_main_prepare(cpu_chunk_context *context)
{
    if (context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_SWA)
        return YVEX_OK;
    context->wkv = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
        context->layer_index);
    context->wgate = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
        context->layer_index);
    context->ape = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
        context->layer_index);
    context->norm = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
        context->layer_index);
    if (!context->wkv || !context->wgate || !context->ape || !context->norm) {
        context->rc = yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
            NULL, context->layer_index, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
            4ull, 0ull, context->err, YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk requires compressor bindings");
        return context->rc;
    }
    context->rc = yvex_attention_state_transaction_acquire(
        &context->transaction,
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
        &context->main_kv_span, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->rc = yvex_attention_state_transaction_acquire(
        &context->transaction,
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
        &context->main_score_span, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->main_kv = (float *)yvex_attention_calloc_array(
        context->token_count * context->main_before.state_width, sizeof(float));
    context->main_score = (float *)yvex_attention_calloc_array(
        context->token_count * context->main_before.state_width, sizeof(float));
    context->main_ape = (float *)yvex_attention_calloc_array(context->main_before.state_width,
                                               sizeof(float));
    context->main_norm = (float *)yvex_attention_calloc_array(context->layer_plan->head_dimension,
                                                sizeof(float));
    if (!context->main_kv || !context->main_score || !context->main_ape || !context->main_norm) {
        context->rc = yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            context->token_count * context->main_before.state_width, 0ull, context->err,
            YVEX_ERR_NOMEM,
            "DeepSeek attention CPU chunk compressor scratch allocation failed");
        return context->rc;
    }
    context->rc = yvex_attention_dot_batch(
        context->session, context->wkv, 0ull, context->hidden, context->token_count, context->hidden_width,
        context->hidden_width, context->main_before.state_width, context->main_kv,
        context->main_before.state_width, &context->rows, 0, context->result, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->rc = yvex_attention_dot_batch(
        context->session, context->wgate, 0ull, context->hidden, context->token_count, context->hidden_width,
        context->hidden_width, context->main_before.state_width, context->main_score,
        context->main_before.state_width, &context->rows, 0, context->result, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->rc = yvex_attention_decode_row(context->session, context->norm, 0ull, context->main_norm,
                              context->layer_plan->head_dimension, context->result,
                              context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    if (context->transaction.components[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV]
            .required) {
        context->rc = yvex_attention_state_transaction_acquire(
            &context->transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
            &context->compressed_span, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        context->compressed_positions = (unsigned long long *)yvex_attention_calloc_array(
            context->compressed_span.dims[0], sizeof(*context->compressed_positions));
        if (!context->compressed_positions) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                NULL, context->layer_index,
                YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
                context->compressed_span.dims[0], 0ull, context->err, YVEX_ERR_NOMEM,
                "DeepSeek attention compressed-position allocation failed");
            return context->rc;
        }
    }
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek chunk main step phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_main_step(cpu_chunk_context *context)
{
    if (context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_SWA)
        return YVEX_OK;
    context->main_current = context->main_before;
    for (context->token = 0ull; context->token < context->token_count; ++context->token) {
        int emitted = 0;
        float *compressed_out =
            context->compressed_span.data
                ? (float *)context->compressed_span.data +
                      context->emitted_count * context->compressed_span.stride
                : context->main_kv + context->token * context->main_before.state_width;
        context->rc = yvex_attention_decode_row(
            context->session, context->ape,
            (context->opts->token_position + context->token) % context->layer_plan->compression_ratio,
            context->main_ape, context->main_before.state_width, context->result, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        memset(&context->main_after, 0, sizeof(context->main_after));
        context->main_after.kv_state = (float *)context->main_kv_span.data;
        context->main_after.score_state = (float *)context->main_score_span.data;
        context->main_after.kv_state_stride = context->main_before.kv_state_stride;
        context->main_after.score_state_stride = context->main_before.score_state_stride;
        context->main_after.kv_state_extent = context->main_before.kv_state_extent;
        context->main_after.score_state_extent = context->main_before.score_state_extent;
        context->rc = yvex_attention_rolling_state_step_cpu(
            context->layer_plan, &context->main_current,
            context->main_kv + context->token * context->main_before.state_width,
            context->main_score + context->token * context->main_before.state_width, context->main_ape,
            &context->main_after, compressed_out, context->layer_plan->head_dimension,
            &emitted, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (emitted) {
            unsigned long long emission_position =
                context->opts->token_position + context->token + 1ull -
                context->layer_plan->compression_ratio;
            if (!yvex_attention_rms_norm(
                    compressed_out, context->layer_plan->head_dimension,
                    context->main_norm, context->layer->rms_norm_epsilon)) {
                context->rc = yvex_attention_reject(
                    context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    context->norm, context->layer_index,
                    YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
                    context->layer_plan->head_dimension, context->emitted_count, context->err,
                    YVEX_ERR_FORMAT,
                    "DeepSeek attention CPU chunk compressor emission invalid");
                return context->rc;
            }
            if (!yvex_attention_rope_apply(
                    compressed_out, context->layer_plan->head_dimension,
                    context->layer->rope_head_dimension,
                    emission_position,
                    &context->layer->position, 0)) {
                context->rc = yvex_attention_reject(
                    context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    NULL, context->layer_index,
                    YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, 1ull,
                    context->token, context->err, YVEX_ERR_FORMAT,
                    "DeepSeek attention compressor RoPE/YaRN failed");
                return context->rc;
            }
            if (context->layer_plan->head_dimension > context->layer->rope_head_dimension) {
                context->rc = yvex_attention_activation_apply(
                    &context->layer_plan->compressor_activation, compressed_out,
                    context->layer_plan->head_dimension -
                        context->layer->rope_head_dimension,
                    context->layer_index,
                    YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, context->failure,
                    context->err);
                if (context->rc != YVEX_OK) return context->rc;
            }
            if (!context->compressed_positions ||
                context->emitted_count >= context->compressed_span.dims[0]) {
                context->rc = yvex_attention_reject(
                    context->failure,
                    YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                    context->layer_index,
                    YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
                    context->compressed_span.dims[0], context->emitted_count, context->err,
                    YVEX_ERR_BOUNDS,
                    "DeepSeek attention compressor emitted beyond planned positions");
                return context->rc;
            }
            context->compressed_positions[context->emitted_count] = emission_position;
            context->emitted_count++;
        }
        yvex_attention_rolling_output_bind(&context->main_after, &context->main_current);
    }
    if (context->compressed_span.data) {
        context->rc = yvex_attention_state_transaction_seal(
            &context->transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
            context->compressed_span.expected_elements, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
    context->rc = yvex_attention_state_transaction_seal(
        &context->transaction,
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
        context->main_kv_span.expected_elements, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->rc = yvex_attention_state_transaction_seal(
        &context->transaction,
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
        context->main_score_span.expected_elements, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Admit and prepare the DeepSeek chunk index prepare phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_index_prepare(cpu_chunk_context *context)
{
    if (context->layer_plan->attention_class != YVEX_ATTENTION_CLASS_CSA)
        return YVEX_OK;
    context->index_wkv = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
        context->layer_index);
    context->index_wgate = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
        context->layer_index);
    context->index_ape_binding = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
        context->layer_index);
    context->index_norm_binding = yvex_attention_binding_find(
        context->descriptor, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
        context->layer_index);
        if (!context->index_wkv || !context->index_wgate || !context->index_ape_binding ||
            !context->index_norm_binding) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                NULL, context->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, 4ull, 0ull, context->err,
                YVEX_ERR_FORMAT,
                "DeepSeek attention CPU chunk requires indexer compressor bindings");
            return context->rc;
        }
        context->rc = yvex_attention_state_transaction_acquire(
            &context->transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
            &context->index_kv_span, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        context->rc = yvex_attention_state_transaction_acquire(
            &context->transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
            &context->index_score_span, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (context->transaction.components[YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV]
                .required) {
            context->rc = yvex_attention_state_transaction_acquire(
                &context->transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                &context->index_emit_span, context->failure, context->err);
            if (context->rc != YVEX_OK) return context->rc;
            context->index_positions =
                (unsigned long long *)yvex_attention_calloc_array(
                    context->index_emit_span.dims[0],
                    sizeof(*context->index_positions));
            if (!context->index_positions) {
                context->rc = yvex_attention_reject(
                    context->failure,
                    YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                    context->layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                    context->index_emit_span.dims[0], 0ull, context->err,
                    YVEX_ERR_NOMEM,
                    "DeepSeek attention indexer-position allocation failed");
                return context->rc;
            }
        }
        context->index_kv = (float *)yvex_attention_calloc_array(
            context->token_count * context->index_before.state_width, sizeof(float));
        context->index_score = (float *)yvex_attention_calloc_array(
            context->token_count * context->index_before.state_width, sizeof(float));
        context->index_ape = (float *)yvex_attention_calloc_array(
            context->index_before.state_width, sizeof(float));
        context->index_norm = (float *)yvex_attention_calloc_array(
            context->layer_plan->indexer_head_dimension, sizeof(float));
        if (!context->index_kv || !context->index_score || !context->index_ape || !context->index_norm) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                NULL, context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                context->token_count * context->index_before.state_width, 0ull, context->err,
                YVEX_ERR_NOMEM,
                "DeepSeek attention CPU chunk indexer scratch allocation failed");
            return context->rc;
        }
        context->rc = yvex_attention_dot_batch(
            context->session, context->index_wkv, 0ull, context->hidden, context->token_count, context->hidden_width,
            context->hidden_width, context->index_before.state_width, context->index_kv,
            context->index_before.state_width, &context->rows, 0, context->result, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        context->rc = yvex_attention_dot_batch(
            context->session, context->index_wgate, 0ull, context->hidden, context->token_count, context->hidden_width,
            context->hidden_width, context->index_before.state_width, context->index_score,
            context->index_before.state_width, &context->rows, 0, context->result, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        context->rc = yvex_attention_decode_row(
            context->session, context->index_norm_binding, 0ull, context->index_norm,
            context->layer_plan->indexer_head_dimension, context->result, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek chunk index step phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_index_step(cpu_chunk_context *context)
{
    if (context->layer_plan->attention_class != YVEX_ATTENTION_CLASS_CSA)
        return YVEX_OK;
    context->index_current = context->index_before;
        for (context->token = 0ull; context->token < context->token_count; ++context->token) {
            int emitted = 0;
            float *index_out =
                context->index_emit_span.data
                    ? (float *)context->index_emit_span.data +
                          context->index_emitted_count * context->index_emit_span.stride
                    : context->index_kv + context->token * context->index_before.state_width;
            context->rc = yvex_attention_decode_row(
                context->session, context->index_ape_binding,
                (context->opts->token_position + context->token) %
                    context->layer_plan->compression_ratio,
                context->index_ape, context->index_before.state_width, context->result, context->failure,
                context->err);
            if (context->rc != YVEX_OK) return context->rc;
            memset(&context->index_after, 0, sizeof(context->index_after));
            context->index_after.kv_state = (float *)context->index_kv_span.data;
            context->index_after.score_state = (float *)context->index_score_span.data;
            context->index_after.kv_state_stride = context->index_before.kv_state_stride;
            context->index_after.score_state_stride = context->index_before.score_state_stride;
            context->index_after.kv_state_extent = context->index_before.kv_state_extent;
            context->index_after.score_state_extent = context->index_before.score_state_extent;
            context->rc = yvex_attention_rolling_state_step_cpu(
                context->layer_plan, &context->index_current,
                context->index_kv + context->token * context->index_before.state_width,
                context->index_score + context->token * context->index_before.state_width,
                context->index_ape, &context->index_after, index_out,
                context->layer_plan->indexer_head_dimension, &emitted, context->failure,
                context->err);
            if (context->rc != YVEX_OK) return context->rc;
            if (emitted) {
                unsigned long long emission_position =
                    context->opts->token_position + context->token + 1ull -
                    context->layer_plan->compression_ratio;
                if (!yvex_attention_rms_norm(
                        index_out, context->layer_plan->indexer_head_dimension,
                        context->index_norm, context->layer->rms_norm_epsilon)) {
                    context->rc = yvex_attention_reject(
                        context->failure,
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        context->index_norm_binding, context->layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
                        context->layer_plan->indexer_head_dimension,
                        context->index_emitted_count, context->err, YVEX_ERR_FORMAT,
                        "DeepSeek attention CPU chunk indexer emission invalid");
                    return context->rc;
                }
                if (!yvex_attention_rope_apply(
                        index_out, context->layer_plan->indexer_head_dimension,
                        context->layer->rope_head_dimension,
                        emission_position,
                        &context->layer->position, 0)) {
                    context->rc = yvex_attention_reject(
                        context->failure,
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                        context->layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, 1ull,
                        context->token, context->err, YVEX_ERR_FORMAT,
                        "DeepSeek indexer compressor RoPE/YaRN failed");
                    return context->rc;
                }
                context->rc = yvex_attention_activation_apply(
                    &context->layer_plan->compressor_rotated_activation, index_out,
                    context->layer_plan->indexer_head_dimension, context->layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, context->failure,
                    context->err);
                if (context->rc != YVEX_OK) return context->rc;
                if (!context->index_positions ||
                    context->index_emitted_count >= context->index_emit_span.dims[0]) {
                    context->rc = yvex_attention_reject(
                        context->failure,
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA,
                        NULL, context->layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                        context->index_emit_span.dims[0], context->index_emitted_count,
                        context->err, YVEX_ERR_BOUNDS,
                        "DeepSeek attention indexer emitted beyond planned positions");
                    return context->rc;
                }
                context->index_positions[context->index_emitted_count] = emission_position;
                context->index_emitted_count++;
            }
            yvex_attention_rolling_output_bind(&context->index_after,
                                             &context->index_current);
        }
        if (context->index_emit_span.data) {
            context->rc = yvex_attention_state_transaction_seal(
                &context->transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                context->index_emit_span.expected_elements, context->failure, context->err);
            if (context->rc != YVEX_OK) return context->rc;
        }
        context->rc = yvex_attention_state_transaction_seal(
            &context->transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
            context->index_kv_span.expected_elements, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        context->rc = yvex_attention_state_transaction_seal(
            &context->transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
            context->index_score_span.expected_elements, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek chunk index query phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_index_query(cpu_chunk_context *context)
{
    if (context->layer_plan->attention_class != YVEX_ATTENTION_CLASS_CSA)
        return YVEX_OK;
    context->index_q_binding = yvex_attention_binding_find(
            context->descriptor, YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
            context->layer_index);
        context->index_weight_binding = yvex_attention_binding_find(
            context->descriptor, YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
            context->layer_index);
        if (!context->index_q_binding || !context->index_weight_binding ||
            context->index_q_binding->binding->row_width != context->q_rank ||
            context->index_q_binding->binding->row_count !=
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension ||
            context->index_weight_binding->binding->row_width != context->hidden_width ||
            context->index_weight_binding->binding->row_count !=
                context->layer_plan->indexer_heads) {
            context->rc = yvex_attention_reject(
                context->failure,
                YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                context->index_q_binding, context->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension,
                context->index_q_binding ? context->index_q_binding->binding->row_count
                                : 0ull,
                context->err, YVEX_ERR_FORMAT,
                "DeepSeek CSA index query/weight bindings do not match the plan");
            return context->rc;
        }
        if (!yvex_core_u64_mul(
                context->token_count,
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension,
                &context->scratch_term)) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                context->index_q_binding, context->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, ULLONG_MAX,
                context->token_count, context->err, YVEX_ERR_BOUNDS,
                "DeepSeek CSA index query extent overflowed");
            return context->rc;
        }
        context->index_query = (float *)yvex_attention_calloc_array(
            context->scratch_term, sizeof(*context->index_query));
        context->index_weights = (float *)yvex_attention_calloc_array(
            context->token_count * context->layer_plan->indexer_heads,
            sizeof(*context->index_weights));
        if (!context->index_query || !context->index_weights) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                context->index_q_binding, context->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, context->scratch_term,
                0ull, context->err, YVEX_ERR_NOMEM,
                "DeepSeek CSA index query/weight allocation failed");
            return context->rc;
        }
        context->rc = yvex_attention_dot_batch(
            context->session, context->index_q_binding, 0ull, context->q_low, context->token_count, context->q_rank,
            context->q_rank,
            context->layer_plan->indexer_heads *
                context->layer_plan->indexer_head_dimension,
            context->index_query,
            context->layer_plan->indexer_heads *
                context->layer_plan->indexer_head_dimension,
            &context->rows, 0, context->result, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (context->rows != context->layer_plan->indexer_heads *
                        context->layer_plan->indexer_head_dimension) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                context->index_q_binding, context->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension,
                context->rows, context->err, YVEX_ERR_FORMAT,
                "DeepSeek CSA index query projection is incomplete");
            return context->rc;
        }
        context->rc = yvex_attention_dot_batch(
            context->session, context->index_weight_binding, 0ull, context->hidden, context->token_count,
            context->hidden_width, context->hidden_width, context->layer_plan->indexer_heads,
            context->index_weights, context->layer_plan->indexer_heads, &context->rows, 0, context->result,
            context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (context->rows != context->layer_plan->indexer_heads) {
            context->rc = yvex_attention_reject(
                context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                context->index_weight_binding, context->layer_index,
                YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
                context->layer_plan->indexer_heads, context->rows, context->err, YVEX_ERR_FORMAT,
                "DeepSeek CSA index weight projection is incomplete");
            return context->rc;
        }
        for (context->token = 0ull; context->token < context->token_count; ++context->token) {
            unsigned long long head;
            for (head = 0ull; head < context->layer_plan->indexer_heads; ++head) {
                float *head_query = context->index_query +
                    context->token * context->layer_plan->indexer_heads *
                        context->layer_plan->indexer_head_dimension +
                    head * context->layer_plan->indexer_head_dimension;
                if (!yvex_attention_rope_apply(
                        head_query,
                        context->layer_plan->indexer_head_dimension,
                        context->layer->rope_head_dimension,
                        context->opts->token_position + context->token,
                        &context->layer->position, 0)) {
                    context->rc = yvex_attention_reject(
                        context->failure,
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        context->index_q_binding, context->layer_index,
                        YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, 1ull,
                        head, context->err, YVEX_ERR_FORMAT,
                        "DeepSeek CSA index query RoPE/YaRN failed");
                    return context->rc;
                }
                context->rc = yvex_attention_activation_apply(
                    &context->layer_plan->indexer_query_activation, head_query,
                    context->layer_plan->indexer_head_dimension, context->layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, context->failure,
                    context->err);
                if (context->rc != YVEX_OK) return context->rc;
            }
        }
    return YVEX_OK;
}
// Purpose: Admit and prepare the DeepSeek chunk trace prepare phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_trace_prepare(cpu_chunk_context *context)
{
if (context->opts->trace &&
    context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_CSA) {
    unsigned long long compressed_total;
    unsigned long long topk_extent;
    if (!yvex_core_u64_add(context->history.compressed_entry_count,
                                   context->emitted_count,
                                   &compressed_total) ||
        !yvex_core_u64_mul(
            context->token_count,
            yvex_attention_min_u64(compressed_total,
                               context->layer_plan->sparse_topk.k),
            &topk_extent)) {
        context->rc = yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            context->emitted_count, context->err, YVEX_ERR_BOUNDS,
            "DeepSeek CSA trace top-k extent overflowed");
        return context->rc;
    }
    context->trace_topk_stride = yvex_attention_min_u64(
        compressed_total, context->layer_plan->sparse_topk.k);
    if (context->trace_topk_stride) {
        context->trace_topk_counts =
            (unsigned long long *)yvex_attention_calloc_array(
                context->token_count, sizeof(*context->trace_topk_counts));
        context->trace_topk_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                topk_extent, sizeof(*context->trace_topk_positions));
        if (!context->trace_topk_counts || !context->trace_topk_positions) {
            context->rc = yvex_attention_reject(
                context->failure,
                YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, topk_extent,
                0ull, context->err, YVEX_ERR_NOMEM,
                "DeepSeek CSA trace top-k allocation failed");
            return context->rc;
        }
    }
}
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek chunk reduce commit phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_reduce_commit(cpu_chunk_context *context)
{
context->rc = yvex_attention_reduce_chunk(
    context->layer_plan, context->query, &context->history,
    (const float *)context->raw_kv_span.data, context->raw_kv_span.stride,
    context->compressed_span.data ? (const float *)context->compressed_span.data : NULL,
    context->emitted_count, context->compressed_span.stride, context->compressed_positions,
    context->index_emit_span.data ? (const float *)context->index_emit_span.data : NULL,
    context->index_emitted_count, context->index_emit_span.stride, context->index_positions,
    context->index_query,
    context->layer_plan->indexer_heads * context->layer_plan->indexer_head_dimension,
    context->index_weights, context->layer_plan->indexer_heads, context->sink_values, context->token_count,
    context->opts->token_position, context->attention_values, context->trace_topk_counts,
    context->trace_topk_positions, context->trace_topk_stride, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_output_project(
    context->session, context->out_a, context->out_b,
    context->attention_values, context->token_count, context->query_width,
    context->layer->output_groups, context->layer->output_group_input_width,
    context->layer->output_lora_rank, context->hidden_width, (float *)context->output_span.data,
    context->output_span.stride, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_state_transaction_seal(
    &context->transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
    context->output_span.expected_elements, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_state_transaction_seal(
    &context->transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
    context->raw_kv_span.expected_elements, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_state_transaction_commit(
    &context->transaction, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Publish the complete DeepSeek chunk capture facts only after transactional success.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cpu_chunk_capture(cpu_chunk_context *context)
{
context->committed_output = yvex_attention_memory_sink_committed_component(
    &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT);
context->committed_raw = yvex_attention_memory_sink_committed_component(
    &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV);
context->committed_compressed =
    yvex_attention_memory_sink_committed_component(
        &context->sink,
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV);
context->committed_indexer =
    yvex_attention_memory_sink_committed_component(
        &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV);
context->committed_main_kv_state =
    yvex_attention_memory_sink_committed_component(
        &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE);
context->committed_main_score_state =
    yvex_attention_memory_sink_committed_component(
        &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE);
context->committed_index_kv_state =
    yvex_attention_memory_sink_committed_component(
        &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE);
context->committed_index_score_state =
    yvex_attention_memory_sink_committed_component(
        &context->sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE);
context->committed_identity = yvex_attention_memory_sink_identity(&context->sink);
if (!context->committed_output || !context->committed_output->data || !context->committed_raw ||
    !context->committed_raw->data || !context->committed_identity) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
        YVEX_ERR_STATE,
        "DeepSeek attention CPU chunk commit did not publish output identity");
    return context->rc;
}
if (context->opts->trace &&
    !yvex_attention_trace_capture(
        context->opts->trace, context->layer_index, context->layer->attention_class,
        context->opts->token_position, context->token_count, context->hidden_width, context->q_rank,
        context->query_width, context->kv_width, context->hidden, context->q_low, context->query,
        (const float *)context->committed_raw->data,
        context->committed_compressed
            ? (const float *)context->committed_compressed->data
            : NULL,
        context->emitted_count,
        context->committed_compressed ? context->committed_compressed->stride : 0ull,
        context->compressed_positions,
        context->committed_indexer ? (const float *)context->committed_indexer->data
                          : NULL,
        context->index_emitted_count,
        context->committed_indexer ? context->committed_indexer->stride : 0ull,
        context->index_positions,
        context->index_query,
        context->layer_plan->indexer_heads *
            context->layer_plan->indexer_head_dimension,
        context->index_weights, context->layer_plan->indexer_heads, context->attention_values,
        (const float *)context->committed_output->data, context->trace_topk_counts,
        context->trace_topk_positions, context->trace_topk_stride,
        context->main_after.present ? &context->main_after : NULL,
        context->committed_main_kv_state
            ? (const float *)context->committed_main_kv_state->data : NULL,
        context->committed_main_score_state
            ? (const float *)context->committed_main_score_state->data : NULL,
        context->index_after.present ? &context->index_after : NULL,
        context->committed_index_kv_state
            ? (const float *)context->committed_index_kv_state->data : NULL,
        context->committed_index_score_state
            ? (const float *)context->committed_index_score_state->data : NULL)) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
        YVEX_ERR_NOMEM,
        "DeepSeek attention execution trace capture failed");
    return context->rc;
}
    return YVEX_OK;
}
// Purpose: Publish the complete DeepSeek chunk publish facts only after transactional success.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static void cpu_chunk_publish(cpu_chunk_context *context)
{
context->result->executed = 1;
context->result->full_attention = 1;
context->result->reference_independent = context->result->reference_comparisons ? 1 : 0;
context->result->cuda_executed = 0;
context->result->layer_index = context->layer_index;
context->result->attention_class = context->layer->attention_class;
context->result->token_position = context->opts->token_position;
context->result->q_a_rows = context->q_rank;
context->result->q_b_rows = context->query_width;
context->result->kv_rows = context->kv_width;
context->result->local_entries = context->history.local_tail_count + context->token_count;
context->result->compressed_entries = context->emitted_count;
context->result->deduplicated_entries = context->history.local_tail_count + context->token_count;
context->result->state_raw_entries = context->token_count;
context->result->state_compressed_entries = context->emitted_count;
context->result->state_indexer_entries = context->index_emitted_count;
context->result->q_projection_checksum = yvex_attention_checksum(context->q_low,
                                                   context->token_count * context->q_rank);
context->result->kv_projection_checksum =
    yvex_attention_checksum((const float *)context->committed_raw->data,
                       context->committed_raw->expected_elements);
context->result->rope_checksum = yvex_attention_checksum(context->query, context->token_count * context->query_width);
context->result->attention_checksum =
    yvex_attention_checksum(context->attention_values, context->token_count * context->query_width);
context->result->output_checksum =
    yvex_attention_checksum((const float *)context->committed_output->data,
                       context->committed_output->expected_elements);
(void)snprintf(context->result->output_identity, sizeof(context->result->output_identity),
               "%s", context->committed_identity);
if (context->result->reference_comparisons)
    context->result->rmse =
        sqrt(context->result->rmse / (double)context->result->reference_comparisons);
yvex_error_clear(context->err);
if (context->failure) memset(context->failure, 0, sizeof(*context->failure));
}
// Purpose: Compose generic graph mechanisms for the DeepSeek chunk execute phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_cpu_chunk_execute(const yvex_attention_plan *plan, const void *family_ir,
    yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options, yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err)
{
    cpu_chunk_context context;
    int rc;
    memset(&context, 0, sizeof(context));
    context.plan = plan;
    context.ir = (const yvex_deepseek_v4_ir *)family_ir;
    context.session = session;
    context.descriptor = descriptor;
    context.opts = options;
    context.result = result;
    context.failure = failure;
    context.err = err;
    rc = cpu_chunk_admit(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_project(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_main_prepare(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_main_step(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_index_prepare(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_index_step(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_index_query(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_trace_prepare(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_reduce_commit(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_capture(&context);
    if (rc == YVEX_OK) cpu_chunk_publish(&context);
    if (rc != YVEX_OK &&
        context.transaction.status == YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN)
        (void)yvex_attention_state_transaction_abort(
            &context.transaction, context.failure, context.err);
    yvex_attention_memory_sink_release(&context.sink);
    free(context.hidden);
    free(context.q_low);
    free(context.q_norm_weights);
    free(context.query);
    free(context.kv_norm_weights);
    free(context.sink_values);
    free(context.attention_values);
    free(context.main_state_kv);
    free(context.main_state_score);
    free(context.main_kv);
    free(context.main_score);
    free(context.main_ape);
    free(context.main_norm);
    free(context.index_state_kv);
    free(context.index_state_score);
    free(context.index_kv);
    free(context.index_score);
    free(context.index_ape);
    free(context.index_norm);
    free(context.index_query);
    free(context.index_weights);
    free(context.compressed_positions);
    free(context.index_positions);
    free(context.trace_topk_counts);
    free(context.trace_topk_positions);
    return rc;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek first token execute phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_cpu_first_token_execute(const yvex_attention_plan *plan, const void *family_ir,
    yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options, yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err)
{
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts = options;
    if (!opts) {
        graph_cpu_options_default(&defaults);
        opts = &defaults;
    } else {
        defaults = *opts;
        opts = &defaults;
    }
    if (opts->token_position != 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            opts->token_position, err, YVEX_ERR_UNSUPPORTED,
            "DeepSeek first-token attention executor requires empty history at position zero");
    defaults.token_position = 0ull;
    defaults.token_count = 1ull;
    return graph_cpu_chunk_execute(
        plan, family_ir, session, descriptor, &defaults, result, failure, err);
}
#include <yvex/backend.h>
typedef struct {
    yvex_tensor_role role;
    yvex_backend_attention_weight_slot slot;
} attention_cuda_role;
static const attention_cuda_role cuda_base_roles[] = {
    {YVEX_TENSOR_ROLE_ATTENTION_Q_A, YVEX_BACKEND_ATTENTION_WEIGHT_Q_A},
    {YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM},
    {YVEX_TENSOR_ROLE_ATTENTION_Q_B, YVEX_BACKEND_ATTENTION_WEIGHT_Q_B},
    {YVEX_TENSOR_ROLE_ATTENTION_KV, YVEX_BACKEND_ATTENTION_WEIGHT_KV},
    {YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM},
    {YVEX_TENSOR_ROLE_ATTENTION_SINKS, YVEX_BACKEND_ATTENTION_WEIGHT_SINKS},
    {YVEX_TENSOR_ROLE_ATTENTION_OUT_A, YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A},
    {YVEX_TENSOR_ROLE_ATTENTION_OUT_B, YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B},
};
static const attention_cuda_role cuda_compressor_roles[] = {
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM},
};
static const attention_cuda_role cuda_index_roles[] = {
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM},
    {YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q},
    {YVEX_TENSOR_ROLE_INDEXER_PROJECTION, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION},
};
typedef struct {
    const yvex_attention_plan *plan;
    const yvex_deepseek_v4_ir *ir;
    yvex_materialization_session *session;
    const yvex_runtime_descriptor *descriptor;
    yvex_backend *backend;
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts;
    yvex_attention_cpu_result *result;
    yvex_attention_failure *failure;
    yvex_error *err;
    const yvex_attention_layer_plan *layer;
    const yvex_deepseek_v4_layer_spec *architecture;
    yvex_attention_history_view empty_history;
    const yvex_attention_history_view *history;
    attention_cuda_weights weights;
    yvex_backend_attention_job job;
    yvex_backend_attention_output cuda_output;
    yvex_backend_attention_failure cuda_failure;
    yvex_attention_execution_trace trace;
    unsigned int i;
    int rc;
} cuda_token_context;
// Purpose: Admit and prepare the DeepSeek token prepare phase from immutable plan facts.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cuda_token_prepare(cuda_token_context *context)
{
if (context->result) memset(context->result, 0, sizeof(*context->result));
memset(&context->weights, 0, sizeof(context->weights));
memset(&context->job, 0, sizeof(context->job));
memset(&context->cuda_output, 0, sizeof(context->cuda_output));
memset(&context->trace, 0, sizeof(context->trace));
memset(&context->empty_history, 0, sizeof(context->empty_history));
if (!context->opts) {
    graph_cpu_options_default(&context->defaults);
    context->opts = &context->defaults;
}
if (!context->plan || !context->ir || !context->session ||
    !context->descriptor || !context->backend || !context->result ||
    !context->opts->input || context->opts->input_stride == 0ull ||
    (context->opts->token_count && context->opts->token_count != 1ull))
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
        context->opts ? context->opts->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
        YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err, YVEX_ERR_INVALID_ARG,
        "CUDA attention requires one explicit input token and backend");
context->rc = graph_context_validate(
    context->plan, context->ir, context->session, context->descriptor, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
if (context->opts->trace && context->opts->trace->owned)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull, context->err,
        YVEX_ERR_STATE,
        "CUDA attention trace must be released before reuse");
context->layer = yvex_attention_plan_layer_at(context->plan, context->opts->layer_index);
context->architecture = yvex_model_register_deepseek_v4()->ir.layer_at(context->ir, context->opts->layer_index);
if (!context->layer || !context->architecture || context->opts->input_stride < context->layer->hidden_dimension)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->layer ? context->layer->hidden_dimension : 1ull, context->opts->input_stride, context->err,
        YVEX_ERR_BOUNDS,
        "CUDA attention layer or input stride is invalid");
context->history = context->opts->history ? context->opts->history : &context->empty_history;
if (context->history->token_count != context->opts->token_position)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->opts->token_position, context->history->token_count, context->err, YVEX_ERR_STATE,
        "CUDA attention history is not contiguous");
if (context->opts->history) {
    context->rc = yvex_attention_history_validate(
        context->layer, context->history, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
} else if (context->layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
        YVEX_ERR_STATE,
        "compressed CUDA attention requires explicit rolling history");
}
context->rc = yvex_attention_cuda_trace_allocate(
    &context->trace, context->layer, context->history,
    context->opts->token_position, context->opts->input, context->failure,
    context->err);
if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek token project phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cuda_token_project(cuda_token_context *context)
{
context->job.attention_class = (unsigned int)context->layer->attention_class;
context->job.token_position = context->opts->token_position;
context->job.hidden_width = context->layer->hidden_dimension;
context->job.q_rank = context->layer->query_lora_rank;
context->job.query_heads = context->layer->query_heads;
context->job.head_dimension = context->layer->head_dimension;
context->job.kv_width = context->layer->head_dimension;
context->job.sliding_window = context->layer->sliding_window;
context->job.compression_ratio = context->layer->compression_ratio;
context->job.output_groups = context->layer->output_groups;
context->job.output_group_input_width = context->architecture->output_group_input_width;
context->job.output_rank = context->layer->output_lora_rank;
context->job.indexer_heads = context->layer->indexer_heads;
context->job.indexer_head_dimension = context->layer->indexer_head_dimension;
context->job.indexer_topk = context->layer->sparse_topk.k;
context->job.rms_epsilon = context->architecture->rms_norm_epsilon;
context->job.position.theta = context->architecture->position.theta;
context->job.position.scaling_factor = context->architecture->position.scaling_factor;
context->job.position.original_context = context->architecture->position.original_context;
context->job.position.beta_fast = context->architecture->position.beta_fast;
context->job.position.beta_slow = context->architecture->position.beta_slow;
context->job.position.rope_dimensions = context->architecture->rope_head_dimension;
yvex_attention_cuda_activation_project(
    &context->layer->attention_kv_activation, &context->job.attention_kv_activation);
yvex_attention_cuda_activation_project(
    &context->layer->compressor_activation, &context->job.compressor_activation);
yvex_attention_cuda_activation_project(
    &context->layer->compressor_rotated_activation,
    &context->job.compressor_rotated_activation);
yvex_attention_cuda_activation_project(
    &context->layer->indexer_query_activation, &context->job.indexer_query_activation);
context->job.input = context->opts->input;
context->job.local_kv = context->history->local_kv;
context->job.local_positions = context->history->local_positions;
context->job.local_count = context->history->local_tail_count;
context->job.local_stride = context->history->local_kv_stride;
context->job.compressed_kv = context->history->compressed_kv;
context->job.compressed_positions = context->history->compressed_positions;
context->job.compressed_count = context->history->compressed_entry_count;
context->job.compressed_stride = context->history->compressed_kv_stride;
context->job.indexer_kv = context->history->indexer_kv;
context->job.indexer_positions = context->history->indexer_positions;
context->job.indexer_count = context->history->indexer_entry_count;
context->job.indexer_stride = context->history->indexer_kv_stride;
if (context->history->main_rolling_state.present)
    (void)yvex_attention_cuda_rolling_project(
        &context->history->main_rolling_state, &context->job.main_rolling);
if (context->history->indexer_rolling_state.present)
    (void)yvex_attention_cuda_rolling_project(
        &context->history->indexer_rolling_state, &context->job.indexer_rolling);
context->job.max_device_bytes = 1024ull * 1024ull * 1024ull;
for (context->i = 0u; context->i < sizeof(cuda_base_roles) / sizeof(cuda_base_roles[0]); ++context->i) {
    context->rc = yvex_attention_cuda_role_load(
        context->session, context->descriptor, context->layer->layer_index, cuda_base_roles[context->i].role,
        cuda_base_roles[context->i].slot, &context->weights, &context->job, context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
}
if (context->layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
    for (context->i = 0u; context->i < sizeof(cuda_compressor_roles) /
                          sizeof(cuda_compressor_roles[0]); ++context->i) {
        context->rc = yvex_attention_cuda_role_load(
            context->session, context->descriptor, context->layer->layer_index,
            cuda_compressor_roles[context->i].role, cuda_compressor_roles[context->i].slot,
            &context->weights, &context->job, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
}
if (context->layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
    for (context->i = 0u; context->i < sizeof(cuda_index_roles) / sizeof(cuda_index_roles[0]); ++context->i) {
        context->rc = yvex_attention_cuda_role_load(
            context->session, context->descriptor, context->layer->layer_index, cuda_index_roles[context->i].role,
            cuda_index_roles[context->i].slot, &context->weights, &context->job, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
}
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek token dispatch phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cuda_token_dispatch(cuda_token_context *context)
{
context->cuda_output.q_low = context->trace.q_low;
context->cuda_output.query = context->trace.query;
context->cuda_output.raw_kv = context->trace.raw_kv;
context->cuda_output.compressed_kv = context->trace.compressed_kv;
context->cuda_output.indexer_kv = context->trace.indexer_kv;
context->cuda_output.index_query = context->trace.index_query;
context->cuda_output.index_weights = context->trace.index_weights;
context->cuda_output.attention_values = context->trace.attention_values;
context->cuda_output.output = context->trace.output;
context->cuda_output.compressed_positions = context->trace.compressed_positions;
context->cuda_output.indexer_positions = context->trace.indexer_positions;
context->cuda_output.topk_positions = context->trace.topk_positions;
context->cuda_output.main_kv_state = context->trace.next_main_rolling_state.kv_state;
context->cuda_output.main_score_state = context->trace.next_main_rolling_state.score_state;
context->cuda_output.indexer_kv_state = context->trace.next_indexer_rolling_state.kv_state;
context->cuda_output.indexer_score_state = context->trace.next_indexer_rolling_state.score_state;
context->rc = yvex_backend_attention_execute(
    context->backend, &context->job, &context->cuda_output, &context->cuda_failure, context->err);
if (context->rc != YVEX_OK) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, NULL,
        context->layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->cuda_failure.expected, context->cuda_failure.actual, context->err,
        (yvex_status)context->rc,
        context->cuda_failure.stage ? context->cuda_failure.stage :
            "CUDA attention backend execution failed");
    return context->rc;
}
    return YVEX_OK;
}
// Purpose: Publish the complete DeepSeek token publish facts only after transactional success.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int cuda_token_publish(cuda_token_context *context)
{
context->trace.compressed_count = context->cuda_output.compressed_count;
context->trace.indexer_count = context->cuda_output.indexer_count;
context->trace.compressed_stride = context->trace.compressed_count
    ? context->layer->head_dimension : 0ull;
context->trace.indexer_stride = context->trace.indexer_count
    ? context->layer->indexer_head_dimension : 0ull;
if (context->trace.topk_counts) context->trace.topk_counts[0] = context->cuda_output.topk_count;
if (context->history->main_rolling_state.present)
    yvex_attention_cuda_rolling_commit(
        &context->history->main_rolling_state, context->opts->token_position,
        &context->trace.next_main_rolling_state);
if (context->history->indexer_rolling_state.present)
    yvex_attention_cuda_rolling_commit(
        &context->history->indexer_rolling_state, context->opts->token_position,
        &context->trace.next_indexer_rolling_state);
context->trace.complete = 1;
context->result->executed = 1;
context->result->full_attention = 1;
context->result->reference_independent = 0;
context->result->cuda_executed = 1;
context->result->layer_index = context->layer->layer_index;
context->result->attention_class = context->layer->attention_class;
context->result->token_position = context->opts->token_position;
context->result->q_a_rows = context->layer->query_lora_rank;
context->result->q_b_rows = context->layer->query_heads * context->layer->head_dimension;
context->result->kv_rows = context->layer->head_dimension;
context->result->topk_candidates = context->cuda_output.valid_candidate_count;
context->result->topk_selected = context->cuda_output.topk_count;
context->result->local_entries = context->history->local_tail_count + 1ull;
context->result->compressed_entries = context->cuda_output.compressed_count;
context->result->deduplicated_entries = context->history->local_tail_count + 1ull;
context->result->payload_bytes_read = context->weights.payload_bytes_read;
context->result->state_raw_entries = 1ull;
context->result->state_compressed_entries = context->cuda_output.compressed_count;
context->result->state_indexer_entries = context->cuda_output.indexer_count;
context->result->cuda_kernel_launches = context->cuda_output.kernel_launches;
context->result->cuda_peak_device_bytes = context->cuda_output.peak_device_bytes;
context->result->q_projection_checksum = yvex_attention_cuda_checksum(
    context->trace.q_low, context->trace.q_rank);
context->result->kv_projection_checksum = yvex_attention_cuda_checksum(
    context->trace.raw_kv, context->trace.kv_width);
context->result->rope_checksum = yvex_attention_cuda_checksum(
    context->trace.query, context->trace.query_width);
context->result->attention_checksum = yvex_attention_cuda_checksum(
    context->trace.attention_values, context->trace.query_width);
context->result->output_checksum = yvex_attention_cuda_checksum(
    context->trace.output, context->trace.hidden_width);
if (!yvex_attention_cuda_output_identity(context->plan, &context->trace,
                                    context->result->output_identity)) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
        context->layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
        YVEX_ERR_STATE,
        "CUDA attention output identity construction failed");
    return context->rc;
}
if (context->opts->trace) {
    *context->opts->trace = context->trace;
    memset(&context->trace, 0, sizeof(context->trace));
}
if (context->failure) memset(context->failure, 0, sizeof(*context->failure));
yvex_error_clear(context->err);
    return YVEX_OK;
}
// Purpose: Compose generic graph mechanisms for the DeepSeek token execute phase.
// Inputs: admitted plan, family recipe, materialization, descriptor, and caller-owned state.
// Effects: mutates only owned scratch, transactions, traces, or result facts.
// Failure: typed refusal unwinds owned state and publishes no partial output.
// Boundary: family composition cannot promote attention, KV, or generation support.
static int graph_cuda_token_execute(const yvex_attention_plan *plan, const void *family_ir,
    yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    yvex_backend *backend, const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *result, yvex_attention_failure *failure, yvex_error *err)
{
    cuda_token_context context;
    int rc;
    memset(&context, 0, sizeof(context));
    context.plan = plan;
    context.ir = (const yvex_deepseek_v4_ir *)family_ir;
    context.session = session;
    context.descriptor = descriptor;
    context.backend = backend;
    context.opts = options;
    context.result = result;
    context.failure = failure;
    context.err = err;
    rc = cuda_token_prepare(&context);
    if (rc == YVEX_OK) rc = cuda_token_project(&context);
    if (rc == YVEX_OK) rc = cuda_token_dispatch(&context);
    if (rc == YVEX_OK) rc = cuda_token_publish(&context);
    yvex_attention_cuda_weights_release(&context.weights);
    yvex_attention_execution_trace_release(&context.trace);
    if (rc != YVEX_OK && context.result)
        memset(context.result, 0, sizeof(*context.result));
    return rc;
}
/* Purpose: publish the process-lifetime immutable DeepSeek graph recipe.
 * Inputs: none.
 * Effects: none.
 * Failure: none.
 * Boundary: registration does not promote execution support. */
const yvex_graph_family_api *yvex_graph_lower_deepseek_v4(void)
{
    static const yvex_graph_family_api api = {
        graph_plan_build,
        yvex_attention_plan_close,
        yvex_attention_plan_summary,
        yvex_attention_plan_layer_count,
        yvex_attention_plan_layer_at,
        yvex_attention_history_validate,
        yvex_attention_rolling_state_validate,
        yvex_attention_rolling_state_step_cpu,
        graph_cpu_options_default,
        yvex_attention_execution_trace_release,
        graph_cpu_probe_execute,
        graph_cpu_first_token_execute,
        graph_cuda_token_execute,
        graph_cpu_chunk_execute
    };
    return &api;
}
