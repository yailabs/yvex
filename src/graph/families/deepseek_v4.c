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
enum { DEEPSEEK_ATTENTION_CSA_RATIO = 4, DEEPSEEK_ATTENTION_HCA_RATIO = 128 };
static const yvex_attention_cpu_options cpu_options_template = {
    .token_count = 1ull,
    .local_history_tokens = 4ull, .compressed_history_tokens = 8ull,
    .max_q_b_rows = 128ull, .max_kv_rows = 512ull,
    .max_compressor_rows = 32ull, .max_indexer_rows = 64ull,
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
/* Purpose: project one admitted DeepSeek layer into the generic graph recipe.
 * Inputs: immutable family IR and output recipe.
 * Effects: writes only the output recipe.
 * Failure: absent inputs refuse.
 * Boundary: projection does not admit execution. */
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
    out->compute_contract = layer->compute_contract;
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
/* Purpose: compose the DeepSeek plan recipe without redefining generic numerics.
 * Inputs: family IR, materialization, and descriptor.
 * Effects: delegates immutable plan construction.
 * Failure: propagates typed build refusal.
 * Boundary: planning reads no payload. */
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
/* Purpose: validate that the DeepSeek execution context matches immutable plan identity.
 * Inputs: plan, family IR, session, and descriptor.
 * Effects: none.
 * Failure: propagates typed context refusal.
 * Boundary: validation performs no execution. */
static int graph_context_validate(const yvex_attention_plan *plan, const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure, yvex_error *err)
{
    char identity[65];
    if (!ir || !graph_recipe_identity(ir, identity)) identity[0] = '\0';
    return yvex_attention_context_validate(plan, identity, session, descriptor,
                                           failure, err);
}
enum { CPU_ROLLING_MAIN = 0, CPU_ROLLING_INDEXER, CPU_ROLLING_COUNT };
typedef struct {
    const yvex_runtime_tensor_binding *weights[4];
    yvex_attention_component_span state[2], emission;
    yvex_attention_rolling_state_view before, current;
    yvex_attention_rolling_state_output after;
    float *initial_state[2], *projected[2], *ape, *norm;
    unsigned long long *positions, emitted;
} cpu_rolling_stage;
typedef struct {
    yvex_attention_rolling_kind kind;
    yvex_tensor_role weight_roles[4];
    yvex_tensor_role output_role, norm_role;
    yvex_attention_component_kind state_components[2], emission_component;
    const char *binding_failure, *geometry_failure, *token_budget_failure;
    const char *ape_budget_failure, *norm_budget_failure, *allocation_failure;
    const char *input_round_failure, *norm_failure, *norm_round_failure;
    const char *rope_failure, *rope_round_failure, *position_failure;
    int activate_complete_output;
} cpu_rolling_recipe;
static const cpu_rolling_recipe cpu_rolling_recipes[CPU_ROLLING_COUNT] = {
    {YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
         YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM},
        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
        {YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE},
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
        "DeepSeek attention CPU chunk requires compressor bindings",
        "attention compressor scratch geometry overflowed", "attention compressor token scratch exceeds its budget",
        "attention compressor APE scratch exceeds its budget", "attention compressor norm scratch exceeds its budget",
        "DeepSeek attention CPU chunk compressor scratch allocation failed",
        "DeepSeek compressor output could not enter BF16 before normalization",
        "DeepSeek attention CPU chunk compressor emission invalid",
        "DeepSeek compressor norm could not publish BF16 values", "DeepSeek attention compressor RoPE/YaRN failed",
        "DeepSeek compressor RoPE could not publish BF16 values",
        "DeepSeek attention compressor emitted beyond planned positions", 0
    },
    {YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER,
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
         YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM},
        YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
        {YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
         YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE},
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
        "DeepSeek attention CPU chunk requires indexer compressor bindings",
        "attention indexer scratch geometry overflowed", "attention indexer token scratch exceeds its budget",
        "attention indexer APE scratch exceeds its budget", "attention indexer norm scratch exceeds its budget",
        "DeepSeek attention CPU chunk indexer scratch allocation failed",
        "DeepSeek indexer compressor output could not enter BF16 before normalization",
        "DeepSeek attention CPU chunk indexer emission invalid",
        "DeepSeek indexer compressor norm could not publish BF16 values",
        "DeepSeek indexer compressor RoPE/YaRN failed",
        "DeepSeek indexer compressor RoPE could not publish BF16 values",
        "DeepSeek attention indexer emitted beyond planned positions", 1
    }
};
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
    const yvex_runtime_tensor_binding *index_q_binding, *index_weight_binding;
    yvex_attention_memory_sink sink;
    yvex_attention_state_transaction transaction;
    yvex_attention_history_view history;
    yvex_attention_component_span output_span, raw_kv_span;
    cpu_rolling_stage rolling[CPU_ROLLING_COUNT];
    const yvex_attention_component_span *committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
    const char *committed_identity;
    unsigned long long layer_index, token_count, hidden_width, q_rank;
    unsigned long long query_width, kv_width, rows, token;
    unsigned long long hidden_elements, q_low_elements, query_elements, kv_elements;
    unsigned long long scratch_elements, scratch_term;
    yvex_attention_scratch_budget scratch;
    float *hidden, *q_low, *q_norm_weights, *query, *kv_norm_weights;
    float *sink_values, *attention_values;
    float *index_query, *index_weights;
    unsigned long long *trace_topk_counts, *trace_topk_positions;
    unsigned long long trace_topk_stride;
    int rc;
} cpu_chunk_context;
/* Purpose: publish one CPU-family refusal with the current layer and error owners. */
static int cpu_chunk_reject(cpu_chunk_context *context,
                            yvex_attention_failure_code code,
                            const yvex_runtime_tensor_binding *binding, yvex_tensor_role role,
                            unsigned long long expected,
                            unsigned long long actual, yvex_status status,
                            const char *reason)
{
    return yvex_attention_reject(context->failure, code, binding,
                                 context->layer_index, role, expected, actual,
                                 context->err, status, reason);
}

/* Purpose: publish one canonical BF16 model-visible numeric boundary.
 * Inputs: active chunk, mutable values, element count, and diagnostic stage.
 * Effects: rounds the supplied working values in place only.
 * Failure: unsupported contracts or non-finite values return typed refusal.
 * Boundary: compressor F32 projections call this only after compression. */
static int cpu_chunk_round(cpu_chunk_context *context, float *values, unsigned long long count, const char *stage)
{
    if (yvex_attention_compute_round(context->layer_plan->compute_contract,
                                     values, count))
        return YVEX_OK;
    return cpu_chunk_reject(
        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        YVEX_TENSOR_ROLE_UNKNOWN, count, 0ull, YVEX_ERR_FORMAT, stage);
}
/* Purpose: reserve one checked family-owned scratch extent before allocation.
 * Inputs: mutable execution accounting, element geometry, and a stable refusal reason.
 * Effects: advances owned scratch bytes only when the complete extent is admitted.
 * Failure: overflow or budget exhaustion returns a typed scratch refusal.
 * Boundary: accounts family execution memory without allocating it. */
static int cpu_chunk_scratch_reserve(cpu_chunk_context *context,
                                     unsigned long long count, size_t element_size, const char *reason)
{
    size_t bytes;
    if (!yvex_attention_scratch_reserve(
            &context->scratch, count, element_size, &bytes))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            context->opts->scratch_limit_bytes,
            (unsigned long long)context->scratch.live_bytes,
            context->err, YVEX_ERR_BOUNDS, reason);
    return YVEX_OK;
}
/* Purpose: account for one pair of empty rolling buffers before allocation.
 * Inputs: admitted layer geometry and one main or indexer rolling-state kind.
 * Effects: reserves both candidate KV and score buffers in owned scratch accounting.
 * Failure: invalid geometry, overflow, or budget exhaustion refuses before allocation.
 * Boundary: derives memory only; rolling state semantics remain graph-owned. */
static int cpu_chunk_rolling_scratch_reserve(cpu_chunk_context *context, yvex_attention_rolling_kind kind)
{
    unsigned long long ratio, head_dimension, state_width, state_slots, extent;
    int overlap, rotated;
    if (!yvex_attention_rolling_geometry(
            context->layer_plan, kind, &ratio, &head_dimension, &state_width,
            &state_slots, &overlap, &rotated) ||
        !yvex_core_u64_mul(state_width, state_slots, &extent) ||
        !yvex_core_u64_mul(extent, 2ull, &extent))
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            state_slots, context->err, YVEX_ERR_BOUNDS,
            "attention rolling scratch geometry overflowed");
    return cpu_chunk_scratch_reserve(context, extent, sizeof(float),
        "attention rolling scratch budget exceeded");
}
/* Purpose: account for all transaction-owned staging before it is allocated.
 * Inputs: admitted token, layer, output, rolling, and emission geometry.
 * Effects: reserves the exact required transaction components in scratch accounting.
 * Failure: geometry overflow or budget exhaustion refuses before transaction begin.
 * Boundary: mirrors transaction storage geometry without owning publication. */
static int cpu_chunk_transaction_scratch_reserve(cpu_chunk_context *context)
{
    unsigned long long elements, term, emitted, end;
    if (!yvex_core_u64_mul(context->token_count, context->hidden_width,
                           &elements) ||
        !yvex_core_u64_mul(context->token_count,
                           context->layer_plan->head_dimension, &term) ||
        !yvex_core_u64_add(elements, term, &elements))
        goto overflow;
    if (context->layer_plan->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        if (!yvex_core_u64_mul(
                context->rolling[CPU_ROLLING_MAIN].before.state_slots,
                context->rolling[CPU_ROLLING_MAIN].before.state_width, &term) ||
            !yvex_core_u64_mul(term, 2ull, &term) ||
            !yvex_core_u64_add(elements, term, &elements) ||
            !yvex_core_u64_add(context->opts->token_position,
                               context->token_count, &end))
            goto overflow;
        emitted = end / context->layer_plan->compression_ratio -
                  context->opts->token_position /
                      context->layer_plan->compression_ratio;
        if (!yvex_core_u64_mul(emitted, context->layer_plan->head_dimension,
                               &term) ||
            !yvex_core_u64_add(elements, term, &elements))
            goto overflow;
        if (context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_CSA) {
            if (!yvex_core_u64_mul(
                    context->rolling[CPU_ROLLING_INDEXER].before.state_slots,
                    context->rolling[CPU_ROLLING_INDEXER].before.state_width,
                    &term) ||
                !yvex_core_u64_mul(term, 2ull, &term) ||
                !yvex_core_u64_add(elements, term, &elements) ||
                !yvex_core_u64_mul(
                    emitted, context->layer_plan->indexer_head_dimension,
                    &term) ||
                !yvex_core_u64_add(elements, term, &elements))
                goto overflow;
        }
    }
    return cpu_chunk_scratch_reserve(context, elements, sizeof(float),
        "attention transaction staging exceeds the scratch budget");
overflow:
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX, 0ull,
        context->err, YVEX_ERR_BOUNDS,
        "attention transaction staging geometry overflowed");
}
/* Purpose: identify whether one rolling recipe participates in the layer class. */
static int cpu_chunk_rolling_active(const cpu_chunk_context *context, unsigned int index)
{
    return index == CPU_ROLLING_MAIN
        ? context->layer_plan->attention_class != YVEX_ATTENTION_CLASS_SWA
        : context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_CSA;
}
/* Purpose: project one rolling recipe onto its canonical history component. */
static yvex_attention_rolling_state_view *cpu_chunk_history_state(cpu_chunk_context *context, unsigned int index)
{
    return index == CPU_ROLLING_MAIN
        ? &context->history.main_rolling_state
        : &context->history.indexer_rolling_state;
}
/* Purpose: allocate one absent rolling state using the recipe's admitted geometry.
 * Inputs: admitted chunk and main/indexer recipe index.
 * Effects: owns initial state buffers and publishes their immutable history view.
 * Failure: scratch or allocation refusal leaves the transaction unopened.
 * Boundary: initializes family recurrence; it does not execute compression. */
static int cpu_chunk_rolling_initialize(cpu_chunk_context *context, unsigned int index)
{
    const cpu_rolling_recipe *recipe = &cpu_rolling_recipes[index];
    cpu_rolling_stage *stage = &context->rolling[index];
    int rc;
    if (!cpu_chunk_rolling_active(context, index)) return YVEX_OK;
    rc = cpu_chunk_rolling_scratch_reserve(context, recipe->kind);
    if (rc != YVEX_OK) return rc;
    rc = yvex_attention_rolling_storage_allocate(
        context->layer_plan, recipe->kind, context->opts->token_position,
        &stage->initial_state[0], &stage->initial_state[1], &stage->before,
        context->failure, context->err);
    if (rc == YVEX_OK) *cpu_chunk_history_state(context, index) = stage->before;
    return rc;
}
/* Purpose: admit one bounded CPU chunk from immutable plan and history facts.
 * Inputs: execution context.
 * Effects: binds roles and begins private state staging.
 * Failure: typed refusal leaves no published state.
 * Boundary: admission executes no attention math. */
static int cpu_chunk_admit(cpu_chunk_context *context)
{
yvex_attention_result_reset(context->result);
if (!context->opts) {
    graph_cpu_options_default(&context->defaults);
    context->opts = &context->defaults;
}
context->token_count = context->opts->token_count ? context->opts->token_count : 1ull;
context->scratch.limit_bytes = context->opts->scratch_limit_bytes;
if (!context->plan || !context->ir || !context->session ||
    !context->descriptor || !context->result || !context->opts->input)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
        YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
        0ull, context->err, YVEX_ERR_INVALID_ARG,
        "DeepSeek attention CPU chunk requires plan, IR, session, descriptor, "
        "explicit input, and result");
context->rc = graph_context_validate(
    context->plan, context->ir, context->session, context->descriptor, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
if ((context->opts->publication && context->opts->trace) ||
    (context->opts->publication && context->opts->publication->owned) ||
    (context->opts->trace && context->opts->trace->owned))
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
        NULL, context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
        context->err, YVEX_ERR_STATE,
        "DeepSeek attention publication must be singular and released before reuse");
context->layer_index = context->opts->layer_index;
context->layer_plan = yvex_attention_plan_layer_at(context->plan, context->layer_index);
context->layer = yvex_model_register_deepseek_v4()->ir.layer_at(context->ir, context->layer_index);
if (!context->layer_plan || !context->layer)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, context->err,
        YVEX_ERR_BOUNDS, "DeepSeek attention CPU chunk layer is absent");
if (context->opts->input_stride < context->layer_plan->hidden_dimension)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->layer_plan->hidden_dimension, context->opts->input_stride,
        context->err, YVEX_ERR_BOUNDS,
        "DeepSeek attention input stride is shorter than hidden width");
context->rc = yvex_attention_class_geometry_validate(
    context->layer_plan, DEEPSEEK_ATTENTION_CSA_RATIO,
    DEEPSEEK_ATTENTION_HCA_RATIO, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_cancel_check(
    context->opts->cancellation, context->layer_index,
    "attention CPU execution cancelled before mutation",
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
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
                      &context->scratch_elements))
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->opts->scratch_limit_bytes,
        ULLONG_MAX, context->err, YVEX_ERR_BOUNDS,
        "DeepSeek attention CPU chunk scratch geometry overflowed");
context->rc = cpu_chunk_scratch_reserve(
    context, context->scratch_elements, sizeof(float),
    "DeepSeek attention CPU chunk scratch budget exceeded");
if (context->rc != YVEX_OK) return context->rc;
if (context->opts->history) {
    context->history = *context->opts->history;
    if (context->history.token_count != context->opts->token_position) {
        return yvex_attention_reject(
            context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, context->opts->token_position,
            context->history.token_count, context->err, YVEX_ERR_STATE,
            "DeepSeek attention CPU chunk history is not contiguous");
    }
    context->rolling[CPU_ROLLING_MAIN].before =
        context->history.main_rolling_state;
    context->rolling[CPU_ROLLING_INDEXER].before =
        context->history.indexer_rolling_state;
}
if (!context->opts->history && context->opts->token_position != 0ull)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
        context->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->opts->token_position, 0ull, context->err, YVEX_ERR_STATE,
        "DeepSeek attention requires complete history after the first token");
if (!context->opts->history) {
    unsigned int rolling_index;
    for (rolling_index = 0u; rolling_index < CPU_ROLLING_COUNT;
         ++rolling_index) {
        context->rc = cpu_chunk_rolling_initialize(context, rolling_index);
        if (context->rc != YVEX_OK) return context->rc;
    }
}
context->history.immutable = 1;
context->history.token_count = context->opts->token_position;
context->rc = yvex_attention_memory_sink_init(&context->sink, NULL, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = cpu_chunk_transaction_scratch_reserve(context);
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
/* Purpose: allocate project buffers and import finite caller activations at
 * the model-visible BF16 boundary before any encoded weight access.
 * Inputs: admitted chunk geometry and explicit caller activation rows.
 * Effects: allocates chunk-owned staging and copies rounded input rows.
 * Failure: cancellation, allocation, or non-finite input publishes no state.
 * Boundary: imports activation state but performs no attention projection. */
static int cpu_chunk_input_prepare(cpu_chunk_context *context)
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
    context->rc = cpu_chunk_reject(
        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        YVEX_TENSOR_ROLE_UNKNOWN, context->scratch.live_bytes, 0ull,
        YVEX_ERR_NOMEM, "failed to allocate DeepSeek CPU chunk scratch");
    return context->rc;
}
for (context->token = 0ull; context->token < context->token_count; ++context->token) {
    unsigned long long lane;
    const float *source = context->opts->input +
        context->token * context->opts->input_stride;
    for (lane = 0ull; lane < context->hidden_width; ++lane) {
        if (!isfinite(source[lane])) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_TENSOR_ROLE_UNKNOWN, context->hidden_width, lane, YVEX_ERR_FORMAT,
                "DeepSeek attention input contains non-finite values");
            return context->rc;
        }
    }
    memcpy(context->hidden + context->token * context->hidden_width, source,
           (size_t)context->hidden_width * sizeof(*context->hidden));
}
context->rc = cpu_chunk_round(
    context, context->hidden, context->hidden_elements,
    "DeepSeek attention input could not enter the BF16 compute contract");
return context->rc;
}
/* Purpose: execute admitted Q/KV projections and model-visible numeric boundaries.
 * Inputs: bound chunk context.
 * Effects: fills private query, KV, and sink staging.
 * Failure: typed numeric/read refusal publishes nothing.
 * Boundary: output reduction remains separate. */
static int cpu_chunk_project(cpu_chunk_context *context)
{
context->rc = yvex_attention_cancel_check(
    context->opts->cancellation, context->layer_index,
    "attention CPU execution cancelled before payload access",
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = cpu_chunk_input_prepare(context);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_dot_batch(
    context->session, context->q_a, 0ull, context->hidden,
    context->token_count, context->hidden_width, context->hidden_width,
    context->q_rank, context->q_low, context->q_rank, &context->rows,
    &context->scratch, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = cpu_chunk_round(
    context, context->q_low, context->q_low_elements,
    "DeepSeek attention Q-A projection could not publish BF16 values");
if (context->rc != YVEX_OK) return context->rc;
if (context->rows != context->q_rank) {
    context->rc = cpu_chunk_reject(
        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, context->q_a,
        YVEX_TENSOR_ROLE_ATTENTION_Q_A, context->q_rank, context->rows, YVEX_ERR_FORMAT,
        "DeepSeek attention CPU chunk Q-A projection is incomplete");
    return context->rc;
}
context->rc = yvex_attention_decode_row(
    context->session, context->q_a_norm, 0ull, context->q_norm_weights,
    context->q_rank, &context->scratch, context->result, context->failure,
    context->err);
if (context->rc != YVEX_OK) return context->rc;
for (context->token = 0ull; context->token < context->token_count; ++context->token) {
    if (!yvex_attention_rms_norm(context->q_low + context->token * context->q_rank, context->q_rank,
                                  context->q_norm_weights,
                                  context->layer->rms_norm_epsilon)) {
        context->rc = cpu_chunk_reject(
            context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->q_a_norm,
            YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, context->q_rank, context->token,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk Q norm produced invalid values");
        return context->rc;
    }
    context->rc = cpu_chunk_round(
        context, context->q_low + context->token * context->q_rank,
        context->q_rank,
        "DeepSeek attention Q norm could not publish BF16 values");
    if (context->rc != YVEX_OK) return context->rc;
}
context->rc = yvex_attention_dot_batch(
    context->session, context->q_b, 0ull, context->q_low,
    context->token_count, context->q_rank, context->q_rank,
    context->query_width,
    context->query, context->query_width, &context->rows,
    &context->scratch, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = cpu_chunk_round(
    context, context->query, context->query_elements,
    "DeepSeek attention Q-B projection could not publish BF16 values");
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_dot_batch(
    context->session, context->kv, 0ull, context->hidden,
    context->token_count, context->hidden_width, context->hidden_width,
    context->kv_width, (float *)context->raw_kv_span.data,
    context->raw_kv_span.stride, &context->rows,
    &context->scratch, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = cpu_chunk_round(
    context, (float *)context->raw_kv_span.data,
    context->raw_kv_span.expected_elements,
    "DeepSeek attention KV projection could not publish BF16 values");
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_decode_row(
    context->session, context->kv_norm, 0ull, context->kv_norm_weights,
    context->kv_width, &context->scratch, context->result, context->failure,
    context->err);
if (context->rc != YVEX_OK) return context->rc;
for (context->token = 0ull; context->token < context->token_count; ++context->token) {
    unsigned long long head;
    for (head = 0ull; head < context->layer_plan->query_heads; ++head) {
        if (!yvex_attention_unit_rms_norm(
                context->query + context->token * context->query_width +
                    head * context->layer_plan->head_dimension,
                context->layer_plan->head_dimension, context->layer->rms_norm_epsilon)) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->q_b,
                YVEX_TENSOR_ROLE_ATTENTION_Q_B, context->layer_plan->head_dimension,
                head, YVEX_ERR_FORMAT,
                "DeepSeek attention query head RMS norm failed");
            return context->rc;
        }
        context->rc = cpu_chunk_round(
            context,
            context->query + context->token * context->query_width +
                head * context->layer_plan->head_dimension,
            context->layer_plan->head_dimension,
            "DeepSeek attention query normalization could not publish BF16 values");
        if (context->rc != YVEX_OK) return context->rc;
        if (!yvex_attention_rope_apply(
                context->query + context->token * context->query_width +
                    head * context->layer_plan->head_dimension,
                context->layer_plan->head_dimension, context->layer->rope_head_dimension,
                context->opts->token_position + context->token, &context->layer->position, 0)) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->q_b,
                YVEX_TENSOR_ROLE_ATTENTION_Q_B, 1ull, head, YVEX_ERR_FORMAT,
                "DeepSeek attention query RoPE/YaRN application failed");
            return context->rc;
        }
        context->rc = cpu_chunk_round(
            context,
            context->query + context->token * context->query_width +
                head * context->layer_plan->head_dimension,
            context->layer_plan->head_dimension,
            "DeepSeek attention query RoPE could not publish BF16 values");
        if (context->rc != YVEX_OK) return context->rc;
    }
    if (!yvex_attention_rms_norm(
            (float *)context->raw_kv_span.data + context->token * context->raw_kv_span.stride,
            context->kv_width, context->kv_norm_weights, context->layer->rms_norm_epsilon)) {
        context->rc = cpu_chunk_reject(
            context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->kv_norm,
            YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, context->kv_width, context->token,
            YVEX_ERR_FORMAT,
            "DeepSeek attention CPU chunk KV norm produced invalid values");
        return context->rc;
    }
    context->rc = cpu_chunk_round(
        context,
        (float *)context->raw_kv_span.data +
            context->token * context->raw_kv_span.stride,
        context->kv_width,
        "DeepSeek attention KV normalization could not publish BF16 values");
    if (context->rc != YVEX_OK) return context->rc;
    if (!yvex_attention_rope_apply(
            (float *)context->raw_kv_span.data + context->token * context->raw_kv_span.stride,
            context->kv_width, context->layer->rope_head_dimension,
            context->opts->token_position + context->token, &context->layer->position, 0)) {
        context->rc = cpu_chunk_reject(
            context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, context->kv,
            YVEX_TENSOR_ROLE_ATTENTION_KV, 1ull, context->token, YVEX_ERR_FORMAT,
            "DeepSeek attention KV RoPE/YaRN application failed");
        return context->rc;
    }
    context->rc = cpu_chunk_round(
        context,
        (float *)context->raw_kv_span.data +
            context->token * context->raw_kv_span.stride,
        context->kv_width,
        "DeepSeek attention KV RoPE could not publish BF16 values");
    if (context->rc != YVEX_OK) return context->rc;
    if (context->kv_width > context->layer->rope_head_dimension) {
        context->rc = yvex_attention_activation_apply(
            &context->layer_plan->attention_kv_activation,
            (float *)context->raw_kv_span.data + context->token * context->raw_kv_span.stride,
            context->kv_width - context->layer->rope_head_dimension, context->layer_index,
            YVEX_TENSOR_ROLE_ATTENTION_KV, &context->scratch,
            context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
}
context->rc = yvex_attention_decode_flat(
    context->session, context->sinks, context->sink_values,
    context->layer_plan->query_heads, &context->scratch, context->result,
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
    return YVEX_OK;
}
/* Purpose: acquire and allocate one recipe's optional emission-position staging.
 * Inputs: active rolling stage, recipe, and begun state transaction.
 * Effects: acquires only the recipe emission span and its bounded positions.
 * Failure: preserves the main/indexer scratch and allocation refusal stages.
 * Boundary: prepares evidence storage without executing recurrence. */
static int cpu_chunk_emission_prepare(cpu_chunk_context *context, unsigned int index)
{
    const cpu_rolling_recipe *recipe = &cpu_rolling_recipes[index];
    cpu_rolling_stage *stage = &context->rolling[index];
    if (!context->transaction.components[recipe->emission_component].required)
        return YVEX_OK;
    context->rc = yvex_attention_state_transaction_acquire(
        &context->transaction, recipe->emission_component, &stage->emission,
        context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
    context->rc = cpu_chunk_scratch_reserve(
        context, stage->emission.dims[0], sizeof(*stage->positions),
        index == CPU_ROLLING_MAIN
            ? "attention compressed-position scratch exceeds its budget"
            : "attention indexer-position scratch exceeds its budget");
    if (context->rc != YVEX_OK) return context->rc;
    stage->positions = yvex_attention_calloc_array(
        stage->emission.dims[0], sizeof(*stage->positions));
    if (stage->positions) return YVEX_OK;
    return cpu_chunk_reject(
        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        recipe->output_role, stage->emission.dims[0], 0ull, YVEX_ERR_NOMEM,
        index == CPU_ROLLING_MAIN
            ? "DeepSeek attention compressed-position allocation failed"
            : "DeepSeek attention indexer-position allocation failed");
}
/* Purpose: prepare one main or indexer rolling compressor from its typed recipe.
 * Inputs: admitted chunk, rolling index, descriptor bindings, and transaction.
 * Effects: acquires candidate spans and allocates bounded projection scratch.
 * Failure: preserves binding, scratch, allocation, and decode refusal stages.
 * Boundary: shares recurrence mechanics without merging family policies. */
static int cpu_chunk_rolling_prepare(cpu_chunk_context *context, unsigned int index)
{
    const cpu_rolling_recipe *recipe = &cpu_rolling_recipes[index];
    cpu_rolling_stage *stage = &context->rolling[index];
    unsigned int item;
    if (!cpu_chunk_rolling_active(context, index)) return YVEX_OK;
    for (item = 0u; item < 4u; ++item) {
        stage->weights[item] = yvex_attention_binding_find(
            context->descriptor, recipe->weight_roles[item],
            context->layer_index);
        if (!stage->weights[item])
            return cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                NULL, recipe->output_role, 4ull, 0ull, YVEX_ERR_FORMAT,
                recipe->binding_failure);
    }
    for (item = 0u; item < 2u; ++item) {
        context->rc = yvex_attention_state_transaction_acquire(
            &context->transaction, recipe->state_components[item],
            &stage->state[item], context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
    if (index == CPU_ROLLING_INDEXER) {
        context->rc = cpu_chunk_emission_prepare(context, index);
        if (context->rc != YVEX_OK) return context->rc;
    }
    if (!yvex_core_u64_mul(context->token_count, stage->before.state_width,
                           &context->scratch_term))
        return cpu_chunk_reject(
            context, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
            YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX, context->token_count,
            YVEX_ERR_BOUNDS, recipe->geometry_failure);
    context->rc = cpu_chunk_scratch_reserve(
        context, context->scratch_term, 2u * sizeof(float),
        recipe->token_budget_failure);
    if (context->rc == YVEX_OK)
        context->rc = cpu_chunk_scratch_reserve(
            context, stage->before.state_width, sizeof(float),
            recipe->ape_budget_failure);
    if (context->rc == YVEX_OK)
        context->rc = cpu_chunk_scratch_reserve(
            context, stage->before.head_dimension, sizeof(float),
            recipe->norm_budget_failure);
    if (context->rc != YVEX_OK) return context->rc;
    stage->projected[0] = yvex_attention_calloc_array(
        context->scratch_term, sizeof(float));
    stage->projected[1] = yvex_attention_calloc_array(
        context->scratch_term, sizeof(float));
    stage->ape = yvex_attention_calloc_array(
        stage->before.state_width, sizeof(float));
    stage->norm = yvex_attention_calloc_array(
        stage->before.head_dimension, sizeof(float));
    if (!stage->projected[0] || !stage->projected[1] || !stage->ape ||
        !stage->norm)
        return cpu_chunk_reject(
            context, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_TENSOR_ROLE_UNKNOWN, context->scratch_term, 0ull,
            YVEX_ERR_NOMEM, recipe->allocation_failure);
    for (item = 0u; item < 2u; ++item) {
        context->rc = yvex_attention_dot_batch(
            context->session, stage->weights[item], 0ull, context->hidden,
            context->token_count, context->hidden_width,
            context->hidden_width, stage->before.state_width,
            stage->projected[item], stage->before.state_width, &context->rows,
            &context->scratch, context->result, context->failure,
            context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
    context->rc = yvex_attention_decode_row(
        context->session, stage->weights[3], 0ull, stage->norm,
        stage->before.head_dimension, &context->scratch, context->result,
        context->failure, context->err);
    if (context->rc != YVEX_OK || index == CPU_ROLLING_INDEXER)
        return context->rc;
    return cpu_chunk_emission_prepare(context, index);
}
/* Purpose: execute and seal one main or indexer rolling-compressor recurrence.
 * Inputs: prepared rolling stage, immutable recipe, token projections, and APE.
 * Effects: advances only candidate recurrence state and ordered emissions.
 * Failure: preserves every numeric, bounds, and transaction refusal stage.
 * Boundary: common recurrence does not merge main and indexer activation policy. */
static int cpu_chunk_rolling_step(cpu_chunk_context *context, unsigned int index)
{
    const cpu_rolling_recipe *recipe = &cpu_rolling_recipes[index];
    cpu_rolling_stage *stage = &context->rolling[index];
    const yvex_attention_activation_policy *activation =
        index == CPU_ROLLING_MAIN
            ? &context->layer_plan->compressor_activation
            : &context->layer_plan->compressor_rotated_activation;
    unsigned long long width = stage->before.head_dimension;
    unsigned int item;
    if (!cpu_chunk_rolling_active(context, index)) return YVEX_OK;
    stage->current = stage->before;
    for (context->token = 0ull; context->token < context->token_count;
         ++context->token) {
        unsigned long long position;
        int emitted = 0;
        float *output = stage->emission.data
            ? (float *)stage->emission.data +
                  stage->emitted * stage->emission.stride
            : stage->projected[0] +
                  context->token * stage->before.state_width;
        context->rc = yvex_attention_decode_row(
            context->session, stage->weights[2],
            (context->opts->token_position + context->token) %
                context->layer_plan->compression_ratio,
            stage->ape, stage->before.state_width, &context->scratch,
            context->result, context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        memset(&stage->after, 0, sizeof(stage->after));
        stage->after.kv_state = (float *)stage->state[0].data;
        stage->after.score_state = (float *)stage->state[1].data;
        stage->after.kv_state_stride = stage->before.kv_state_stride;
        stage->after.score_state_stride = stage->before.score_state_stride;
        stage->after.kv_state_extent = stage->before.kv_state_extent;
        stage->after.score_state_extent = stage->before.score_state_extent;
        context->rc = yvex_attention_rolling_state_step_cpu(
            context->layer_plan, &stage->current,
            stage->projected[0] +
                context->token * stage->before.state_width,
            stage->projected[1] +
                context->token * stage->before.state_width,
            stage->ape, &stage->after, output, width, &emitted,
            context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (emitted) {
            position = context->opts->token_position + context->token + 1ull -
                       context->layer_plan->compression_ratio;
            context->rc = cpu_chunk_round(
                context, output, width, recipe->input_round_failure);
            if (context->rc != YVEX_OK) return context->rc;
            if (!yvex_attention_rms_norm(
                    output, width, stage->norm,
                    context->layer->rms_norm_epsilon))
                return cpu_chunk_reject(
                    context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                    stage->weights[3], recipe->norm_role, width,
                    stage->emitted, YVEX_ERR_FORMAT, recipe->norm_failure);
            context->rc = cpu_chunk_round(
                context, output, width, recipe->norm_round_failure);
            if (context->rc != YVEX_OK) return context->rc;
            if (!yvex_attention_rope_apply(
                    output, width, context->layer->rope_head_dimension,
                    position, &context->layer->position, 0))
                return cpu_chunk_reject(
                    context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                    recipe->output_role, 1ull, context->token,
                    YVEX_ERR_FORMAT, recipe->rope_failure);
            context->rc = cpu_chunk_round(
                context, output, width, recipe->rope_round_failure);
            if (context->rc != YVEX_OK) return context->rc;
            if (recipe->activate_complete_output ||
                width > context->layer->rope_head_dimension) {
                unsigned long long active = recipe->activate_complete_output
                    ? width : width - context->layer->rope_head_dimension;
                context->rc = yvex_attention_activation_apply(
                    activation, output, active, context->layer_index,
                    recipe->output_role, &context->scratch, context->failure,
                    context->err);
                if (context->rc != YVEX_OK) return context->rc;
            }
            if (!stage->positions ||
                stage->emitted >= stage->emission.dims[0])
                return cpu_chunk_reject(
                    context, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA,
                    NULL, recipe->output_role, stage->emission.dims[0],
                    stage->emitted, YVEX_ERR_BOUNDS,
                    recipe->position_failure);
            stage->positions[stage->emitted++] = position;
        }
        yvex_attention_rolling_output_bind(&stage->after, &stage->current);
    }
    if (stage->emission.data) {
        context->rc = yvex_attention_state_transaction_seal(
            &context->transaction, recipe->emission_component,
            stage->emission.expected_elements, context->failure,
            context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
    for (item = 0u; item < 2u; ++item) {
        context->rc = yvex_attention_state_transaction_seal(
            &context->transaction, recipe->state_components[item],
            stage->state[item].expected_elements, context->failure,
            context->err);
        if (context->rc != YVEX_OK) return context->rc;
    }
    return YVEX_OK;
}
/* Purpose: derive CSA index queries and weights from admitted projections.
 * Inputs: prepared CSA chunk.
 * Effects: fills private index-query and index-weight staging.
 * Failure: typed binding, scratch, or numeric refusal.
 * Boundary: selection remains reduction-owned. */
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
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
                context->index_q_binding,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension,
                context->index_q_binding ? context->index_q_binding->binding->row_count
                                : 0ull,
                YVEX_ERR_FORMAT,
                "DeepSeek CSA index query/weight bindings do not match the plan");
            return context->rc;
        }
        if (!yvex_core_u64_mul(
                context->token_count,
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension,
                &context->scratch_term)) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                context->index_q_binding,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, ULLONG_MAX,
                context->token_count, YVEX_ERR_BOUNDS,
                "DeepSeek CSA index query extent overflowed");
            return context->rc;
        }
        context->rc = cpu_chunk_scratch_reserve(
            context, context->scratch_term, sizeof(float),
            "attention index-query scratch exceeds its budget");
        if (context->rc != YVEX_OK) return context->rc;
        context->index_query = (float *)yvex_attention_calloc_array(
            context->scratch_term, sizeof(*context->index_query));
        if (!yvex_core_u64_mul(context->token_count,
                               context->layer_plan->indexer_heads,
                               &context->scratch_term))
            return cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH,
                context->index_weight_binding,
                YVEX_TENSOR_ROLE_INDEXER_PROJECTION, ULLONG_MAX,
                context->token_count, YVEX_ERR_BOUNDS,
                "attention index-weight scratch geometry overflowed");
        context->rc = cpu_chunk_scratch_reserve(
            context, context->scratch_term, sizeof(float),
            "attention index-weight scratch exceeds its budget");
        if (context->rc != YVEX_OK) return context->rc;
        context->index_weights = (float *)yvex_attention_calloc_array(
            context->scratch_term,
            sizeof(*context->index_weights));
        if (!context->index_query || !context->index_weights) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                context->index_q_binding,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, context->scratch_term,
                0ull, YVEX_ERR_NOMEM,
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
            &context->rows, &context->scratch, context->result,
            context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (context->rows != context->layer_plan->indexer_heads *
                        context->layer_plan->indexer_head_dimension) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                context->index_q_binding,
                YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                context->layer_plan->indexer_heads *
                    context->layer_plan->indexer_head_dimension,
                context->rows, YVEX_ERR_FORMAT,
                "DeepSeek CSA index query projection is incomplete");
            return context->rc;
        }
        context->rc = cpu_chunk_round(
            context, context->index_query,
            context->token_count * context->layer_plan->indexer_heads *
                context->layer_plan->indexer_head_dimension,
            "DeepSeek index query projection could not publish BF16 values");
        if (context->rc != YVEX_OK) return context->rc;
        context->rc = yvex_attention_dot_batch(
            context->session, context->index_weight_binding, 0ull, context->hidden, context->token_count,
            context->hidden_width, context->hidden_width, context->layer_plan->indexer_heads,
            context->index_weights, context->layer_plan->indexer_heads,
            &context->rows, &context->scratch, context->result,
            context->failure, context->err);
        if (context->rc != YVEX_OK) return context->rc;
        if (context->rows != context->layer_plan->indexer_heads) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                context->index_weight_binding,
                YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
                context->layer_plan->indexer_heads, context->rows, YVEX_ERR_FORMAT,
                "DeepSeek CSA index weight projection is incomplete");
            return context->rc;
        }
        context->rc = cpu_chunk_round(
            context, context->index_weights,
            context->token_count * context->layer_plan->indexer_heads,
            "DeepSeek index weight projection could not publish BF16 values");
        if (context->rc != YVEX_OK) return context->rc;
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
                    context->rc = cpu_chunk_reject(
                        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                        context->index_q_binding,
                        YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, 1ull,
                        head, YVEX_ERR_FORMAT,
                        "DeepSeek CSA index query RoPE/YaRN failed");
                    return context->rc;
                }
                context->rc = cpu_chunk_round(
                    context, head_query,
                    context->layer_plan->indexer_head_dimension,
                    "DeepSeek index query RoPE could not publish BF16 values");
                if (context->rc != YVEX_OK) return context->rc;
                context->rc = yvex_attention_activation_apply(
                    &context->layer_plan->indexer_query_activation, head_query,
                    context->layer_plan->indexer_head_dimension, context->layer_index,
                    YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                    &context->scratch, context->failure, context->err);
                if (context->rc != YVEX_OK) return context->rc;
            }
        }
    return YVEX_OK;
}
/* Purpose: reserve optional CSA top-k trace evidence before reduction.
 * Inputs: admitted chunk and rolling emission counts.
 * Effects: owns bounded trace arrays.
 * Failure: overflow or allocation refusal.
 * Boundary: trace storage cannot affect selection. */
static int cpu_chunk_trace_prepare(cpu_chunk_context *context)
{
if ((context->opts->publication || context->opts->trace) &&
    context->layer_plan->attention_class == YVEX_ATTENTION_CLASS_CSA) {
    unsigned long long compressed_total;
    unsigned long long topk_extent;
    if (!yvex_core_u64_add(context->history.compressed_entry_count,
                                   context->rolling[CPU_ROLLING_MAIN].emitted,
                                   &compressed_total) ||
        !yvex_core_u64_mul(
            context->token_count,
            yvex_attention_min_u64(compressed_total,
                               context->layer_plan->sparse_topk.k),
            &topk_extent)) {
        context->rc = cpu_chunk_reject(
            context, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            context->rolling[CPU_ROLLING_MAIN].emitted,
            YVEX_ERR_BOUNDS,
            "DeepSeek CSA trace top-k extent overflowed");
        return context->rc;
    }
    context->trace_topk_stride = yvex_attention_min_u64(
        compressed_total, context->layer_plan->sparse_topk.k);
    if (context->trace_topk_stride) {
        context->rc = cpu_chunk_scratch_reserve(
            context, context->token_count,
            sizeof(*context->trace_topk_counts),
            "attention top-k count scratch exceeds its budget");
        if (context->rc == YVEX_OK)
            context->rc = cpu_chunk_scratch_reserve(
                context, topk_extent,
                sizeof(*context->trace_topk_positions),
                "attention top-k position scratch exceeds its budget");
        if (context->rc != YVEX_OK) return context->rc;
        context->trace_topk_counts =
            (unsigned long long *)yvex_attention_calloc_array(
                context->token_count, sizeof(*context->trace_topk_counts));
        context->trace_topk_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                topk_extent, sizeof(*context->trace_topk_positions));
        if (!context->trace_topk_counts || !context->trace_topk_positions) {
            context->rc = cpu_chunk_reject(
                context, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                YVEX_TENSOR_ROLE_UNKNOWN, topk_extent, 0ull, YVEX_ERR_NOMEM,
                "DeepSeek CSA trace top-k allocation failed");
            return context->rc;
        }
    }
}
    return YVEX_OK;
}
/* Purpose: reduce complete attention, project output, and atomically commit candidate state.
 * Inputs: projected chunk and prepared history.
 * Effects: seals and commits the private transaction.
 * Failure: typed reduction or commit refusal publishes nothing.
 * Boundary: capture follows commit. */
static int cpu_chunk_reduce_commit(cpu_chunk_context *context)
{
context->rc = yvex_attention_reduce_chunk(
    context->layer_plan, context->query, &context->history,
    (const float *)context->raw_kv_span.data, context->raw_kv_span.stride,
    context->rolling[CPU_ROLLING_MAIN].emission.data
        ? (const float *)context->rolling[CPU_ROLLING_MAIN].emission.data : NULL,
    context->rolling[CPU_ROLLING_MAIN].emitted,
    context->rolling[CPU_ROLLING_MAIN].emission.stride,
    context->rolling[CPU_ROLLING_MAIN].positions,
    context->rolling[CPU_ROLLING_INDEXER].emission.data
        ? (const float *)context->rolling[CPU_ROLLING_INDEXER].emission.data : NULL,
    context->rolling[CPU_ROLLING_INDEXER].emitted,
    context->rolling[CPU_ROLLING_INDEXER].emission.stride,
    context->rolling[CPU_ROLLING_INDEXER].positions,
    context->index_query,
    context->layer_plan->indexer_heads * context->layer_plan->indexer_head_dimension,
    context->index_weights, context->layer_plan->indexer_heads, context->sink_values, context->token_count,
    context->opts->token_position, context->attention_values, context->trace_topk_counts,
    context->trace_topk_positions, context->trace_topk_stride,
    &context->scratch, context->result, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_output_project(
    context->session, context->out_a, context->out_b,
    context->attention_values, context->token_count, context->query_width,
    context->layer->output_groups, context->layer->output_group_input_width,
    context->layer->output_lora_rank, context->hidden_width,
    context->layer_plan->compute_contract, (float *)context->output_span.data,
    context->output_span.stride, &context->scratch, context->result,
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_cancel_check(
    context->opts->cancellation, context->layer_index,
    "attention CPU execution cancelled before state commit",
    context->failure, context->err);
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
/* Purpose: capture committed components and optional execution trace after transaction success.
 * Inputs: committed memory sink and private evidence.
 * Effects: owns the requested trace publication.
 * Failure: missing commit or trace allocation refuses.
 * Boundary: never reconstructs attention facts. */
static int cpu_chunk_capture(cpu_chunk_context *context)
{
const yvex_attention_component_span *const *committed = context->committed;
unsigned int component;
for (component = 0u; component < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
     ++component)
    context->committed[component] =
        yvex_attention_memory_sink_committed_component(
            &context->sink, (yvex_attention_component_kind)component);
context->committed_identity = yvex_attention_memory_sink_identity(&context->sink);
if (!committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT] ||
    !committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]->data ||
    !committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV] ||
    !committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV]->data ||
    !context->committed_identity) {
    context->rc = cpu_chunk_reject(
        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
        YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, YVEX_ERR_STATE,
        "DeepSeek attention CPU chunk commit did not publish output identity");
    return context->rc;
}
if ((context->opts->publication || context->opts->trace) &&
    !yvex_attention_trace_capture(
        context->opts->publication ? context->opts->publication
                                   : context->opts->trace,
        context->layer_index, context->layer->attention_class,
        context->opts->token_position, context->token_count, context->hidden_width, context->q_rank,
        context->query_width, context->kv_width, context->hidden, context->q_low, context->query,
        (const float *)committed[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV]->data,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV]
            ? (const float *)committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV]->data
            : NULL,
        context->rolling[CPU_ROLLING_MAIN].emitted,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV]
            ? committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV]->stride : 0ull,
        context->rolling[CPU_ROLLING_MAIN].positions,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV]
            ? (const float *)committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV]->data : NULL,
        context->rolling[CPU_ROLLING_INDEXER].emitted,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV]
            ? committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV]->stride : 0ull,
        context->rolling[CPU_ROLLING_INDEXER].positions,
        context->index_query,
        context->layer_plan->indexer_heads *
            context->layer_plan->indexer_head_dimension,
        context->index_weights, context->layer_plan->indexer_heads, context->attention_values,
        (const float *)committed[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]->data,
        context->trace_topk_counts,
        context->trace_topk_positions, context->trace_topk_stride,
        context->rolling[CPU_ROLLING_MAIN].after.present
            ? &context->rolling[CPU_ROLLING_MAIN].after : NULL,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE]
            ? (const float *)committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE]->data : NULL,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE]
            ? (const float *)committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE]->data : NULL,
        context->rolling[CPU_ROLLING_INDEXER].after.present
            ? &context->rolling[CPU_ROLLING_INDEXER].after : NULL,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE]
            ? (const float *)committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE]->data : NULL,
        committed[YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE]
            ? (const float *)committed[
                  YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE]->data : NULL)) {
    context->rc = cpu_chunk_reject(
        context, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, YVEX_ERR_NOMEM,
        "DeepSeek attention execution trace capture failed");
    return context->rc;
}
    return YVEX_OK;
}
/* Purpose: publish CPU result counters and identities after complete commit and capture.
 * Inputs: successful chunk context.
 * Effects: writes caller result and clears failure state.
 * Failure: none after admission.
 * Boundary: result evidence does not promote KV or generation. */
static void cpu_chunk_publish(cpu_chunk_context *context)
{
const yvex_attention_component_span *const *committed = context->committed;
context->result->executed = 1;
context->result->full_attention = 1;
context->result->cuda_executed = 0;
context->result->layer_index = context->layer_index;
context->result->attention_class = context->layer->attention_class;
context->result->token_position = context->opts->token_position;
context->result->q_a_rows = context->q_rank;
context->result->q_b_rows = context->query_width;
context->result->kv_rows = context->kv_width;
context->result->local_entries = context->history.local_tail_count + context->token_count;
context->result->compressed_entries =
    context->rolling[CPU_ROLLING_MAIN].emitted;
context->result->state_raw_entries = context->token_count;
context->result->state_compressed_entries =
    context->rolling[CPU_ROLLING_MAIN].emitted;
context->result->state_indexer_entries =
    context->rolling[CPU_ROLLING_INDEXER].emitted;
context->result->q_projection_checksum = yvex_attention_checksum(context->q_low,
                                                   context->token_count * context->q_rank);
context->result->kv_projection_checksum =
    yvex_attention_checksum(
        (const float *)committed[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV]->data,
        committed[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV]->expected_elements);
context->result->rope_checksum = yvex_attention_checksum(context->query, context->token_count * context->query_width);
context->result->attention_checksum =
    yvex_attention_checksum(context->attention_values, context->token_count * context->query_width);
context->result->output_checksum =
    yvex_attention_checksum(
        (const float *)committed[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]->data,
        committed[
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]->expected_elements);
(void)snprintf(context->result->output_identity, sizeof(context->result->output_identity),
               "%s", context->committed_identity);
yvex_error_clear(context->err);
if (context->failure) memset(context->failure, 0, sizeof(*context->failure));
}
/* Purpose: orchestrate one complete transactional DeepSeek CPU attention chunk.
 * Inputs: admitted owners and caller options.
 * Effects: publishes only fully committed result state.
 * Failure: aborts transaction and releases all scratch.
 * Boundary: owns no persistent runtime KV. */
static int graph_cpu_chunk_execute(const yvex_attention_plan *plan, const void *family_ir,
    yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options, yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err)
{
    cpu_chunk_context context;
    unsigned int rolling_index, item;
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
    if (rc == YVEX_OK) rc = cpu_chunk_rolling_prepare(&context, CPU_ROLLING_MAIN);
    if (rc == YVEX_OK) rc = cpu_chunk_rolling_step(&context, CPU_ROLLING_MAIN);
    if (rc == YVEX_OK) rc = cpu_chunk_rolling_prepare(&context, CPU_ROLLING_INDEXER);
    if (rc == YVEX_OK) rc = cpu_chunk_rolling_step(&context, CPU_ROLLING_INDEXER);
    if (rc == YVEX_OK) rc = cpu_chunk_index_query(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_trace_prepare(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_reduce_commit(&context);
    if (rc == YVEX_OK) rc = cpu_chunk_capture(&context);
    if (rc == YVEX_OK) cpu_chunk_publish(&context);
    if (rc != YVEX_OK &&
        context.transaction.status == YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN) {
        yvex_attention_failure cleanup_failure;
        yvex_error cleanup_error;
        yvex_error_clear(&cleanup_error);
        if (yvex_attention_state_transaction_abort(
                &context.transaction, &cleanup_failure, &cleanup_error) != YVEX_OK)
            rc = yvex_attention_reject(
                context.failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_CLEANUP, NULL,
                context.layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
                context.err, YVEX_ERR_STATE,
                "attention CPU transaction rollback failed");
    }
    yvex_attention_memory_sink_release(&context.sink);
    free(context.hidden);
    free(context.q_low);
    free(context.q_norm_weights);
    free(context.query);
    free(context.kv_norm_weights);
    free(context.sink_values);
    free(context.attention_values);
    for (rolling_index = 0u; rolling_index < CPU_ROLLING_COUNT;
         ++rolling_index) {
        for (item = 0u; item < 2u; ++item) {
            free(context.rolling[rolling_index].initial_state[item]);
            free(context.rolling[rolling_index].projected[item]);
        }
        free(context.rolling[rolling_index].ape);
        free(context.rolling[rolling_index].norm);
        free(context.rolling[rolling_index].positions);
    }
    free(context.index_query);
    free(context.index_weights);
    free(context.trace_topk_counts);
    free(context.trace_topk_positions);
    if (rc != YVEX_OK) yvex_attention_result_reset(result);
    return rc;
}
#include <yvex/backend.h>
typedef struct {
    yvex_tensor_role role;
    yvex_backend_attention_weight_slot slot;
    unsigned int class_mask;
} attention_cuda_role;
enum {
    ROLE_CSA = 2u, ROLE_COMPRESSED = 6u, ROLE_ALL = 7u
};
static const attention_cuda_role cuda_roles[] = {
    {YVEX_TENSOR_ROLE_ATTENTION_Q_A, YVEX_BACKEND_ATTENTION_WEIGHT_Q_A, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_Q_B, YVEX_BACKEND_ATTENTION_WEIGHT_Q_B, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_KV, YVEX_BACKEND_ATTENTION_WEIGHT_KV, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_KV_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_SINKS, YVEX_BACKEND_ATTENTION_WEIGHT_SINKS, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_OUT_A, YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_OUT_B, YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B, ROLE_ALL},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV, ROLE_COMPRESSED},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE, ROLE_COMPRESSED},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE, ROLE_COMPRESSED},
    {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM, ROLE_COMPRESSED},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV, ROLE_CSA},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE, ROLE_CSA},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE, ROLE_CSA},
    {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM, ROLE_CSA},
    {YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q, ROLE_CSA},
    {YVEX_TENSOR_ROLE_INDEXER_PROJECTION, YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION, ROLE_CSA},
};
static const yvex_attention_failure_code cuda_failure_map[] = {
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_CLEANUP
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
    yvex_backend_cancellation cancellation;
    yvex_backend_attention_job job;
    yvex_backend_attention_output cuda_output;
    yvex_backend_attention_failure cuda_failure;
    yvex_attention_execution_trace trace;
    unsigned long long trace_bytes;
    unsigned int i, role_mask;
    int rc;
} cuda_token_context;
/* Purpose: admit one CUDA token and allocate result-evidence staging.
 * Inputs: immutable owners, explicit token, backend, and history.
 * Effects: owns a private trace.
 * Failure: typed refusal occurs before dispatch.
 * Boundary: no host numerical completion. */
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
if ((context->opts->publication && context->opts->trace) ||
    (context->opts->publication && context->opts->publication->owned) ||
    (context->opts->trace && context->opts->trace->owned))
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull, context->err,
        YVEX_ERR_STATE,
        "CUDA attention publication must be singular and released before reuse");
context->layer = yvex_attention_plan_layer_at(context->plan, context->opts->layer_index);
context->architecture = yvex_model_register_deepseek_v4()->ir.layer_at(context->ir, context->opts->layer_index);
if (!context->layer || !context->architecture || context->opts->input_stride < context->layer->hidden_dimension)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->layer ? context->layer->hidden_dimension : 1ull, context->opts->input_stride, context->err,
        YVEX_ERR_BOUNDS,
        "CUDA attention layer or input stride is invalid");
context->rc = yvex_attention_class_geometry_validate(
    context->layer, DEEPSEEK_ATTENTION_CSA_RATIO,
    DEEPSEEK_ATTENTION_HCA_RATIO, context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->rc = yvex_attention_cancel_check(
    context->opts->cancellation, context->opts->layer_index,
    "CUDA attention cancelled before graph dispatch",
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
context->history = context->opts->history ? context->opts->history : &context->empty_history;
if (context->history->token_count != context->opts->token_position)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->opts->token_position, context->history->token_count, context->err, YVEX_ERR_STATE,
        "CUDA attention history is not contiguous");
if (!context->opts->history && context->opts->token_position != 0ull)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->opts->token_position, 0ull, context->err, YVEX_ERR_STATE,
        "CUDA attention requires complete history after the first token");
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
    context->opts->token_position, context->opts->input,
    context->opts->scratch_limit_bytes, &context->trace_bytes,
    context->failure, context->err);
if (context->rc != YVEX_OK) return context->rc;
if (!yvex_attention_compute_round(
        context->layer->compute_contract, context->trace.input,
        context->trace.hidden_width)) {
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->trace.hidden_width, 0ull, context->err, YVEX_ERR_FORMAT,
        "CUDA attention input trace could not enter the BF16 compute contract");
}
    return YVEX_OK;
}
/* Purpose: project DeepSeek plan, history, policies, and weights into the CUDA job ABI.
 * Inputs: admitted token context.
 * Effects: loads bounded weights and fills private job facts.
 * Failure: typed contract or role-load refusal.
 * Boundary: performs no host attention math. */
static int cuda_token_project(cuda_token_context *context)
{
if (context->layer->compute_contract !=
    YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1)
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1,
        context->layer->compute_contract, context->err, YVEX_ERR_UNSUPPORTED,
        "CUDA attention compute contract has no backend projection");
context->job.compute_contract =
    YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1;
switch (context->layer->attention_class) {
case YVEX_ATTENTION_CLASS_SWA:
    context->job.attention_class = YVEX_BACKEND_ATTENTION_SWA;
    break;
case YVEX_ATTENTION_CLASS_CSA:
    context->job.attention_class = YVEX_BACKEND_ATTENTION_CSA;
    break;
case YVEX_ATTENTION_CLASS_HCA:
    context->job.attention_class = YVEX_BACKEND_ATTENTION_HCA;
    break;
default:
    return yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
        context->opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
        context->layer->attention_class, context->err, YVEX_ERR_FORMAT,
        "CUDA attention class has no backend representation");
}
context->role_mask = 1u << context->layer->attention_class;
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
if (context->opts->cancellation) {
    context->cancellation.requested = context->opts->cancellation->requested;
    context->cancellation.context = context->opts->cancellation->context;
    context->job.cancellation = &context->cancellation;
}
context->job.max_host_bytes = context->opts->scratch_limit_bytes -
                              context->trace_bytes;
context->job.max_device_bytes = 1024ull * 1024ull * 1024ull;
for (context->i = 0u; context->i < sizeof(cuda_roles) / sizeof(cuda_roles[0]);
     ++context->i) {
    const attention_cuda_role *role = &cuda_roles[context->i];
    if (!(role->class_mask & context->role_mask)) continue;
    context->rc = yvex_attention_cuda_role_load(
        context->session, context->descriptor, context->layer->layer_index,
        role->role, role->slot, &context->weights, &context->job,
        context->failure, context->err);
    if (context->rc != YVEX_OK) return context->rc;
}
    return YVEX_OK;
}
/* Purpose: dispatch one complete admitted CUDA attention job into private output spans.
 * Inputs: projected job and trace storage.
 * Effects: invokes only the backend attention ABI.
 * Failure: maps backend failure without fallback.
 * Boundary: host code performs no numeric work. */
static int cuda_token_dispatch(cuda_token_context *context)
{
yvex_attention_failure_code graph_failure =
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND;
context->cuda_output.q_low = (yvex_backend_float_span){context->trace.q_low, context->trace.q_rank};
context->cuda_output.query = (yvex_backend_float_span){context->trace.query, context->trace.query_width};
context->cuda_output.raw_kv = (yvex_backend_float_span){context->trace.raw_kv, context->trace.kv_width};
context->cuda_output.compressed_kv =
    (yvex_backend_float_span){context->trace.compressed_kv, context->trace.compressed_stride};
context->cuda_output.indexer_kv =
    (yvex_backend_float_span){context->trace.indexer_kv, context->trace.indexer_stride};
context->cuda_output.index_query =
    (yvex_backend_float_span){context->trace.index_query, context->trace.index_query_stride};
context->cuda_output.index_weights =
    (yvex_backend_float_span){context->trace.index_weights, context->trace.index_weight_stride};
context->cuda_output.attention_values =
    (yvex_backend_float_span){context->trace.attention_values, context->trace.query_width};
context->cuda_output.output =
    (yvex_backend_float_span){context->trace.output, context->trace.hidden_width};
context->cuda_output.compressed_positions =
    (yvex_backend_u64_span){context->trace.compressed_positions,
                            context->trace.compressed_positions ? 1ull : 0ull};
context->cuda_output.indexer_positions =
    (yvex_backend_u64_span){context->trace.indexer_positions,
                            context->trace.indexer_positions ? 1ull : 0ull};
context->cuda_output.topk_positions =
    (yvex_backend_u64_span){context->trace.topk_positions, context->trace.topk_stride};
context->cuda_output.main_kv_state =
    (yvex_backend_float_span){context->trace.next_main_rolling_state.kv_state,
                              context->history->main_rolling_state.kv_state_extent};
context->cuda_output.main_score_state =
    (yvex_backend_float_span){context->trace.next_main_rolling_state.score_state,
                              context->history->main_rolling_state.score_state_extent};
context->cuda_output.indexer_kv_state =
    (yvex_backend_float_span){context->trace.next_indexer_rolling_state.kv_state,
                              context->history->indexer_rolling_state.kv_state_extent};
context->cuda_output.indexer_score_state =
    (yvex_backend_float_span){context->trace.next_indexer_rolling_state.score_state,
                              context->history->indexer_rolling_state.score_state_extent};
context->rc = yvex_backend_attention_execute(
    context->backend, &context->job, &context->cuda_output, &context->cuda_failure, context->err);
if (context->rc != YVEX_OK) {
    if ((size_t)context->cuda_failure.code <
        sizeof(cuda_failure_map) / sizeof(cuda_failure_map[0]))
        graph_failure = cuda_failure_map[context->cuda_failure.code];
    context->rc = yvex_attention_reject(
        context->failure, graph_failure, NULL,
        context->layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->cuda_failure.expected, context->cuda_failure.actual, context->err,
        (yvex_status)context->rc,
        context->cuda_failure.stage ? context->cuda_failure.stage :
            "CUDA attention backend execution failed");
    return context->rc;
}
    return YVEX_OK;
}
/* Purpose: publish CUDA evidence and rolling state only after successful device completion.
 * Inputs: completed backend output and private trace.
 * Effects: commits result and requested publication.
 * Failure: accounting or identity refusal publishes nothing.
 * Boundary: persistent KV remains external. */
static int cuda_token_publish(cuda_token_context *context)
{
context->trace.compressed_count = context->cuda_output.compressed_count;
context->trace.indexer_count = context->cuda_output.indexer_count;
context->trace.compressed_stride = context->trace.compressed_count
    ? context->layer->head_dimension : 0ull;
context->trace.indexer_stride = context->trace.indexer_count
    ? context->layer->indexer_head_dimension : 0ull;
context->trace.topk_stride = context->cuda_output.topk_count;
if (context->trace.topk_counts) context->trace.topk_counts[0] = context->cuda_output.topk_count;
context->trace.complete = 1;
context->result->executed = 1;
context->result->full_attention = 1;
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
context->result->payload_bytes_read = context->weights.payload_bytes_read;
context->result->state_raw_entries = 1ull;
context->result->state_compressed_entries = context->cuda_output.compressed_count;
context->result->state_indexer_entries = context->cuda_output.indexer_count;
context->result->cuda_kernel_launches = context->cuda_output.kernel_launches;
if (!yvex_core_u64_add(context->trace_bytes,
                       context->cuda_output.peak_host_bytes,
                       &context->result->cuda_peak_host_bytes)) {
    context->rc = yvex_attention_reject(
        context->failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
        context->layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
        context->opts->scratch_limit_bytes, ULLONG_MAX, context->err,
        YVEX_ERR_BOUNDS, "CUDA attention peak host accounting overflowed");
    return context->rc;
}
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
if (context->history->main_rolling_state.present)
    yvex_attention_cuda_rolling_commit(
        &context->history->main_rolling_state, context->opts->token_position,
        &context->trace.next_main_rolling_state);
if (context->history->indexer_rolling_state.present)
    yvex_attention_cuda_rolling_commit(
        &context->history->indexer_rolling_state, context->opts->token_position,
        &context->trace.next_indexer_rolling_state);
if (context->opts->publication || context->opts->trace) {
    yvex_attention_publication *publication = context->opts->publication
        ? context->opts->publication : context->opts->trace;
    *publication = context->trace;
    memset(&context->trace, 0, sizeof(context->trace));
}
if (context->failure) memset(context->failure, 0, sizeof(*context->failure));
yvex_error_clear(context->err);
    return YVEX_OK;
}
/* Purpose: orchestrate one fail-closed DeepSeek CUDA attention token.
 * Inputs: admitted owners, backend, and token options.
 * Effects: publishes only complete device evidence.
 * Failure: releases weights and trace without fallback.
 * Boundary: owns no persistent runtime KV. */
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
        yvex_attention_rolling_state_step_cpu,
        graph_cpu_options_default,
        yvex_attention_execution_trace_release,
        yvex_attention_execution_trace_release,
        graph_cuda_token_execute,
        graph_cpu_chunk_execute
    };
    return &api;
}
