/*
 * attention_deepseek.c - selected-artifact DeepSeek attention live-plan proof.
 *
 * Owner:
 *   tests/live
 *
 * Owns:
 *   proof that the admitted selected DeepSeek GGUF can flow through
 *   materialization, runtime descriptor projection, complete production CPU
 *   execution, independent scalar-reference comparison, and device-complete
 *   CUDA execution for the three release attention classes.
 *
 * Does not own:
 *   persistent KV, prefill, decode, logits, sampling, generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   this runner commits materialization for binding truth and compares CUDA
 *   output with a separately linked scalar oracle over identical state.
 *
 * Boundary:
 *   live-plan and scoped CPU proof are graph evidence only.
 */
#define _POSIX_C_SOURCE 200809L

#include <yvex/internal/artifact.h>
#include "src/graph/private.h"
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/runtime.h>
#include "tests/reference/deepseek_attention.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/api.h>

static int path_join_selected(char *out, size_t out_size,
                              const char *models_root)
{
    int written;

    if (!out || !out_size || !models_root) return 0;
    written = snprintf(out, out_size, "%s/deepseek/%s", models_root,
                       YVEX_SELECTED_DEEPSEEK_ARTIFACT_FILENAME);
    return written >= 0 && (size_t)written < out_size;
}

static void print_materialization_failure(
    const char *phase,
    const yvex_materialization_failure *failure,
    const yvex_error *err)
{
    fprintf(stderr,
            "%s_failure=%s tensor=%llu name=%s expected=%llu actual=%llu offset=%llu where=%s message=%s\n",
            phase, yvex_materialization_failure_name(failure->code),
            failure->tensor_index, failure->tensor_name, failure->expected,
            failure->actual, failure->offset, yvex_error_where(err),
            yvex_error_message(err));
}

static void print_descriptor_failure(
    const char *phase,
    const yvex_runtime_descriptor_failure *failure,
    const yvex_error *err)
{
    fprintf(stderr,
            "%s_failure=%s tensor=%llu name=%s expected=%llu actual=%llu where=%s message=%s\n",
            phase, yvex_runtime_descriptor_failure_name(failure->code),
            failure->tensor_index, failure->tensor_name, failure->expected,
            failure->actual, yvex_error_where(err), yvex_error_message(err));
}

static void print_attention_failure(
    const yvex_attention_failure *failure,
    const yvex_error *err)
{
    fprintf(stderr,
            "attention_failure=%s layer=%llu role=%u tensor=%s expected=%llu actual=%llu where=%s message=%s\n",
            yvex_attention_failure_name(failure->code),
            failure->layer_index, (unsigned int)failure->role,
            failure->tensor_name, failure->expected, failure->actual,
            yvex_error_where(err), yvex_error_message(err));
}

static void print_architecture_failure(
    const yvex_deepseek_v4_ir_failure *failure,
    const yvex_error *err)
{
    fprintf(stderr,
            "architecture_failure=%s component=%s field=%s layer=%llu expected=%llu actual=%llu where=%s message=%s\n",
            yvex_model_register_deepseek_v4()->ir.failure_name(failure->code),
            yvex_model_register_deepseek_v4()->ir.component_name(failure->component),
            failure->field ? failure->field : "", failure->layer_index,
            failure->expected, failure->actual, yvex_error_where(err),
            yvex_error_message(err));
}

static void fill_history_values(float *values,
                                unsigned long long count,
                                unsigned long long salt)
{
    unsigned long long i;

    for (i = 0ull; i < count; ++i) {
        int lane = (int)(((i + 5ull) * 23ull + salt * 41ull) % 193ull) -
                   96;
        values[i] = (float)lane / 96.0f;
    }
}

/* Contract: executes production and the separate scalar oracle over identical
 * immutable inputs, compares every stage, and releases both owned traces. */
static int run_reference_compare(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *production_result,
    yvex_test_attention_reference_metrics *metrics,
    yvex_attention_execution_trace *preserved_production,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_cpu_options production_options = *options;
    yvex_attention_execution_trace production;
    yvex_attention_execution_trace reference;
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP];
    int rc;

    memset(&production, 0, sizeof(production));
    memset(&reference, 0, sizeof(reference));
    memset(metrics, 0, sizeof(*metrics));
    production_options.trace = &production;
    rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
        plan, ir, session, descriptor, &production_options,
        production_result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!yvex_test_attention_reference_execute(
            plan, ir, session, descriptor, options, &reference, reason)) {
        fprintf(stderr, "attention_reference_failure=%s\n", reason);
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (!yvex_test_attention_reference_compare(
            &production, &reference, 2.0e-4, 2.0e-4, metrics)) {
        fprintf(stderr,
                "attention_reference_mismatch stage=%s values=%llu max_abs=%.17g max_rel=%.17g\n",
                metrics->first_failed_stage ? metrics->first_failed_stage : "",
                metrics->compared_values, metrics->maximum_absolute_error,
                metrics->maximum_relative_error);
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (preserved_production) {
        if (preserved_production->owned) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
        *preserved_production = production;
        memset(&production, 0, sizeof(production));
    }
    rc = YVEX_OK;

cleanup:
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&production);
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&reference);
    return rc;
}

/* Contract: executes every production numerical stage on CUDA, then compares
 * the resulting owned trace with the separately linked full-equation oracle. */
static int run_cuda_reference_compare(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_backend *backend,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *production_result,
    yvex_test_attention_reference_metrics *metrics,
    yvex_attention_execution_trace *preserved_production,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_cpu_options production_options = *options;
    yvex_attention_execution_trace production;
    yvex_attention_execution_trace reference;
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP];
    int rc;

    memset(&production, 0, sizeof(production));
    memset(&reference, 0, sizeof(reference));
    memset(metrics, 0, sizeof(*metrics));
    production_options.trace = &production;
    rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
        plan, ir, session, descriptor, backend, &production_options,
        production_result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!production_result->cuda_executed ||
        production_result->cuda_kernel_launches == 0ull) {
        fprintf(stderr, "attention_cuda_device_path_missing=1\n");
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    if (!yvex_test_attention_reference_execute(
            plan, ir, session, descriptor, options, &reference, reason)) {
        fprintf(stderr, "attention_cuda_reference_failure=%s\n", reason);
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (!yvex_test_attention_reference_compare(
            &production, &reference, 5.0e-4, 5.0e-4, metrics)) {
        fprintf(stderr,
                "attention_cuda_reference_mismatch stage=%s values=%llu max_abs=%.17g max_rel=%.17g\n",
                metrics->first_failed_stage ? metrics->first_failed_stage : "",
                metrics->compared_values, metrics->maximum_absolute_error,
                metrics->maximum_relative_error);
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (preserved_production) {
        if (preserved_production->owned) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
        *preserved_production = production;
        memset(&production, 0, sizeof(production));
    }
    rc = YVEX_OK;

cleanup:
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&production);
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&reference);
    return rc;
}

typedef struct {
    yvex_attention_history_view view;
    float *local_kv;
    unsigned long long *local_positions;
    float *compressed_kv;
    unsigned long long *compressed_positions;
    float *indexer_kv;
    unsigned long long *indexer_positions;
    float *main_kv;
    float *main_score;
    float *index_kv;
    float *index_score;
} live_attention_history;

static void live_history_release(live_attention_history *history)
{
    if (!history) return;
    free(history->local_kv);
    free(history->local_positions);
    free(history->compressed_kv);
    free(history->compressed_positions);
    free(history->indexer_kv);
    free(history->indexer_positions);
    free(history->main_kv);
    free(history->main_score);
    free(history->index_kv);
    free(history->index_score);
    memset(history, 0, sizeof(*history));
}

static int live_rolling_init(
    yvex_attention_rolling_state_view *state,
    float **kv_out,
    float **score_out,
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long token_position,
    const char *plan_identity)
{
    unsigned long long head_dimension =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? layer->indexer_head_dimension : layer->head_dimension;
    int overlap = kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER ||
                  layer->attention_class == YVEX_ATTENTION_CLASS_CSA;
    unsigned long long width = head_dimension * (overlap ? 2ull : 1ull);
    unsigned long long slots = layer->compression_ratio *
                               (overlap ? 2ull : 1ull);
    unsigned long long extent = slots * width;
    unsigned long long slot;
    unsigned long long active;

    *kv_out = (float *)calloc((size_t)extent, sizeof(**kv_out));
    *score_out = (float *)malloc((size_t)extent * sizeof(**score_out));
    if (!*kv_out || !*score_out) return 0;
    for (slot = 0ull; slot < extent; ++slot) (*score_out)[slot] = -INFINITY;
    memset(state, 0, sizeof(*state));
    state->present = 1;
    state->schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1;
    state->kind = kind;
    state->attention_class = layer->attention_class;
    state->layer_index = layer->layer_index;
    state->next_token_position = token_position;
    state->ratio = layer->compression_ratio;
    state->head_dimension = head_dimension;
    state->state_width = width;
    state->state_slots = slots;
    state->cursor = token_position % layer->compression_ratio;
    state->current_fill = state->cursor;
    state->previous_fill = overlap && token_position >= layer->compression_ratio
        ? layer->compression_ratio : 0ull;
    state->kv_state_stride = width;
    state->score_state_stride = width;
    state->kv_state_extent = extent;
    state->score_state_extent = extent;
    state->kv_state = *kv_out;
    state->score_state = *score_out;
    state->overlap = overlap;
    state->rotated = kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER;
    (void)snprintf(state->attention_plan_identity,
                   sizeof(state->attention_plan_identity), "%s",
                   plan_identity);
    if (state->previous_fill) {
        for (slot = 0ull; slot < layer->compression_ratio; ++slot) {
            unsigned long long lane;
            for (lane = 0ull; lane < width; ++lane) {
                unsigned long long offset = slot * width + lane;
                (*kv_out)[offset] =
                    (float)((int)((offset * 17ull + 3ull) % 97ull) - 48) /
                    128.0f;
                (*score_out)[offset] =
                    (float)((int)((offset * 13ull + 5ull) % 67ull) - 33) /
                    64.0f;
            }
        }
    }
    active = state->cursor;
    for (slot = 0ull; slot < active; ++slot) {
        unsigned long long target = overlap ? layer->compression_ratio + slot
                                            : slot;
        unsigned long long lane;
        for (lane = 0ull; lane < width; ++lane) {
            unsigned long long offset = target * width + lane;
            (*kv_out)[offset] =
                (float)((int)((offset * 19ull + 7ull) % 101ull) - 50) /
                128.0f;
            (*score_out)[offset] =
                (float)((int)((offset * 11ull + 9ull) % 71ull) - 35) /
                64.0f;
        }
    }
    return 1;
}

/* Contract: creates deterministic external history including real-size local,
 * compressed, indexer, and rolling-state facts. */
static int live_history_init(
    live_attention_history *history,
    const yvex_attention_layer_plan *layer,
    const yvex_attention_summary *summary,
    unsigned long long token_position,
    unsigned long long local_count,
    unsigned long long compressed_count)
{
    unsigned long long i;

    memset(history, 0, sizeof(*history));
    history->view.immutable = 1;
    history->view.token_count = token_position;
    history->view.local_tail_count = local_count;
    history->view.compressed_entry_count = compressed_count;
    history->view.indexer_entry_count =
        layer->attention_class == YVEX_ATTENTION_CLASS_CSA
            ? compressed_count : 0ull;
    if (local_count) {
        history->local_kv = (float *)calloc(
            (size_t)(local_count * layer->head_dimension), sizeof(float));
        history->local_positions = (unsigned long long *)calloc(
            (size_t)local_count, sizeof(*history->local_positions));
        if (!history->local_kv || !history->local_positions) goto fail;
        fill_history_values(history->local_kv,
                            local_count * layer->head_dimension,
                            layer->layer_index + 101ull);
        for (i = 0ull; i < local_count; ++i)
            history->local_positions[i] = token_position - local_count + i;
        history->view.local_kv = history->local_kv;
        history->view.local_positions = history->local_positions;
        history->view.local_kv_stride = layer->head_dimension;
    }
    if (compressed_count) {
        if (compressed_count > token_position / layer->compression_ratio)
            goto fail;
        history->compressed_kv = (float *)calloc(
            (size_t)(compressed_count * layer->head_dimension), sizeof(float));
        history->compressed_positions = (unsigned long long *)calloc(
            (size_t)compressed_count,
            sizeof(*history->compressed_positions));
        if (!history->compressed_kv || !history->compressed_positions)
            goto fail;
        fill_history_values(history->compressed_kv,
                            compressed_count * layer->head_dimension,
                            layer->layer_index + 211ull);
        for (i = 0ull; i < compressed_count; ++i)
            history->compressed_positions[i] =
                i * layer->compression_ratio;
        history->view.compressed_kv = history->compressed_kv;
        history->view.compressed_positions = history->compressed_positions;
        history->view.compressed_kv_stride = layer->head_dimension;
    }
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        compressed_count) {
        history->indexer_kv = (float *)calloc(
            (size_t)(compressed_count * layer->indexer_head_dimension),
            sizeof(float));
        history->indexer_positions = (unsigned long long *)calloc(
            (size_t)compressed_count, sizeof(*history->indexer_positions));
        if (!history->indexer_kv || !history->indexer_positions) goto fail;
        fill_history_values(
            history->indexer_kv,
            compressed_count * layer->indexer_head_dimension,
            layer->layer_index + 307ull);
        memcpy(history->indexer_positions, history->compressed_positions,
               (size_t)compressed_count *
                   sizeof(*history->indexer_positions));
        history->view.indexer_kv = history->indexer_kv;
        history->view.indexer_positions = history->indexer_positions;
        history->view.indexer_kv_stride = layer->indexer_head_dimension;
    }
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA &&
        !live_rolling_init(&history->view.main_rolling_state,
                           &history->main_kv, &history->main_score, layer,
                           YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
                           token_position, summary->attention_plan_identity))
        goto fail;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        !live_rolling_init(&history->view.indexer_rolling_state,
                           &history->index_kv, &history->index_score, layer,
                           YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER,
                           token_position, summary->attention_plan_identity))
        goto fail;
    return 1;

fail:
    live_history_release(history);
    return 0;
}

static void live_history_bind_next_state(
    live_attention_history *history,
    const yvex_attention_execution_trace *trace)
{
    const yvex_attention_rolling_state_output *main_state =
        &trace->next_main_rolling_state;
    const yvex_attention_rolling_state_output *index_state =
        &trace->next_indexer_rolling_state;

    history->view.token_count = trace->token_position + trace->token_count;
    if (main_state->present) {
        history->view.main_rolling_state =
            (yvex_attention_rolling_state_view){
                .present = main_state->present,
                .schema_version = main_state->schema_version,
                .kind = main_state->kind,
                .attention_class = main_state->attention_class,
                .layer_index = main_state->layer_index,
                .next_token_position = main_state->next_token_position,
                .ratio = main_state->ratio,
                .head_dimension = main_state->head_dimension,
                .state_width = main_state->state_width,
                .state_slots = main_state->state_slots,
                .previous_fill = main_state->previous_fill,
                .current_fill = main_state->current_fill,
                .cursor = main_state->cursor,
                .kv_state_stride = main_state->kv_state_stride,
                .score_state_stride = main_state->score_state_stride,
                .kv_state_extent = main_state->kv_state_extent,
                .score_state_extent = main_state->score_state_extent,
                .kv_state = main_state->kv_state,
                .score_state = main_state->score_state,
                .overlap = main_state->overlap,
                .rotated = main_state->rotated,
            };
        memcpy(history->view.main_rolling_state.attention_plan_identity,
               main_state->attention_plan_identity,
               sizeof(main_state->attention_plan_identity));
    }
    if (index_state->present) {
        history->view.indexer_rolling_state =
            (yvex_attention_rolling_state_view){
                .present = index_state->present,
                .schema_version = index_state->schema_version,
                .kind = index_state->kind,
                .attention_class = index_state->attention_class,
                .layer_index = index_state->layer_index,
                .next_token_position = index_state->next_token_position,
                .ratio = index_state->ratio,
                .head_dimension = index_state->head_dimension,
                .state_width = index_state->state_width,
                .state_slots = index_state->state_slots,
                .previous_fill = index_state->previous_fill,
                .current_fill = index_state->current_fill,
                .cursor = index_state->cursor,
                .kv_state_stride = index_state->kv_state_stride,
                .score_state_stride = index_state->score_state_stride,
                .kv_state_extent = index_state->kv_state_extent,
                .score_state_extent = index_state->score_state_extent,
                .kv_state = index_state->kv_state,
                .score_state = index_state->score_state,
                .overlap = index_state->overlap,
                .rotated = index_state->rotated,
            };
        memcpy(history->view.indexer_rolling_state.attention_plan_identity,
               index_state->attention_plan_identity,
               sizeof(index_state->attention_plan_identity));
    }
}

/* Contract: proves the generated-PTX path against the independent oracle for
 * SWA, real 513-candidate CSA, and ratio-128 HCA boundary state. */
static int run_cuda_live_suite(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_summary *summary,
    unsigned long long swa_layer,
    unsigned long long csa_layer,
    unsigned long long hca_layer,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_backend_options backend_options;
    yvex_backend_capability_result capability;
    yvex_backend *backend = NULL;
    yvex_attention_cpu_options options;
    yvex_attention_cpu_result result;
    yvex_test_attention_reference_metrics metrics;
    yvex_attention_execution_trace failed_trace;
    live_attention_history history;
    const yvex_attention_layer_plan *layer;
    float *input = NULL;
    unsigned long long input_extent = 0ull;
    unsigned long long classes = 0ull;
    unsigned long long launches = 0ull;
    unsigned long long peak_device_bytes = 0ull;
    unsigned long long compared_values = 0ull;
    double max_abs = 0.0;
    double max_rel = 0.0;
    char repeat_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    static const char *fault_cases[] = {
        "allocation",
        "copy-input",
        "cuda.deepseek_attention.q_a",
        "copy-output"
    };
    unsigned int fault_index;
    int rc;

    memset(&backend_options, 0, sizeof(backend_options));
    memset(&capability, 0, sizeof(capability));
    memset(&failed_trace, 0, sizeof(failed_trace));
    memset(&history, 0, sizeof(history));
    memset(repeat_identity, 0, sizeof(repeat_identity));
    backend_options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = yvex_backend_query_capability(
        backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &capability, err);
    if (rc != YVEX_OK ||
        capability.state != YVEX_BACKEND_CAPABILITY_SUPPORTED) {
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED,
                           "attention.cuda.capability",
                           "device-complete DeepSeek attention is not admitted");
            rc = YVEX_ERR_UNSUPPORTED;
        }
        goto cleanup;
    }
    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(plan, swa_layer);
    if (!layer) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    input_extent = layer->hidden_dimension;
    input = (float *)malloc((size_t)input_extent * sizeof(*input));
    if (!input) {
        rc = YVEX_ERR_NOMEM;
        goto cleanup;
    }
    fill_history_values(input, input_extent, 401ull);
    yvex_graph_lower_deepseek_v4()->cpu_options_default(&options);
    options.layer_index = swa_layer;
    options.token_position = 0ull;
    options.token_count = 1ull;
    options.input = input;
    options.input_stride = input_extent;
    rc = run_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &result, &metrics,
        NULL, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    memcpy(repeat_identity, result.output_identity,
           sizeof(repeat_identity));
    classes++;
    launches += result.cuda_kernel_launches;
    if (result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = result.cuda_peak_device_bytes;
    compared_values += metrics.compared_values;
    if (metrics.maximum_absolute_error > max_abs)
        max_abs = metrics.maximum_absolute_error;
    if (metrics.maximum_relative_error > max_rel)
        max_rel = metrics.maximum_relative_error;

    for (fault_index = 0u;
         fault_index < sizeof(fault_cases) / sizeof(fault_cases[0]);
         ++fault_index) {
        if (setenv("YVEX_TEST_CUDA_ATTENTION_FAILURE",
                   fault_cases[fault_index], 1) != 0) {
            rc = YVEX_ERR_IO;
            goto cleanup;
        }
        options.trace = &failed_trace;
        rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
            plan, ir, session, descriptor, backend, &options, &result,
            failure, err);
        (void)unsetenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
        options.trace = NULL;
        if (rc == YVEX_OK || failed_trace.owned || result.executed ||
            failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
    }
    rc = run_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &result, &metrics,
        NULL, failure, err);
    if (rc != YVEX_OK || strcmp(repeat_identity, result.output_identity) != 0)
        goto cleanup;
    launches += result.cuda_kernel_launches;
    if (result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = result.cuda_peak_device_bytes;

    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(plan, csa_layer);
    if (!layer || layer->hidden_dimension != input_extent ||
        !live_history_init(&history, layer, summary, 2052ull, 4ull, 513ull)) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    fill_history_values(input, input_extent, 503ull);
    yvex_graph_lower_deepseek_v4()->cpu_options_default(&options);
    options.layer_index = csa_layer;
    options.token_position = 2052ull;
    options.token_count = 1ull;
    options.input = input;
    options.input_stride = input_extent;
    options.history = &history.view;
    rc = run_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &result, &metrics,
        NULL, failure, err);
    if (rc != YVEX_OK || result.topk_candidates != 513ull ||
        result.topk_selected != 512ull)
        goto cleanup;
    classes++;
    launches += result.cuda_kernel_launches;
    if (result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = result.cuda_peak_device_bytes;
    compared_values += metrics.compared_values;
    if (metrics.maximum_absolute_error > max_abs)
        max_abs = metrics.maximum_absolute_error;
    if (metrics.maximum_relative_error > max_rel)
        max_rel = metrics.maximum_relative_error;
    live_history_release(&history);

    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(plan, hca_layer);
    if (!layer || layer->hidden_dimension != input_extent ||
        !live_history_init(&history, layer, summary, 127ull, 32ull, 0ull)) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    fill_history_values(input, input_extent, 607ull);
    yvex_graph_lower_deepseek_v4()->cpu_options_default(&options);
    options.layer_index = hca_layer;
    options.token_position = 127ull;
    options.token_count = 1ull;
    options.input = input;
    options.input_stride = input_extent;
    options.history = &history.view;
    rc = run_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &result, &metrics,
        NULL, failure, err);
    if (rc != YVEX_OK || result.compressed_entries != 1ull)
        goto cleanup;
    classes++;
    launches += result.cuda_kernel_launches;
    if (result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = result.cuda_peak_device_bytes;
    compared_values += metrics.compared_values;
    if (metrics.maximum_absolute_error > max_abs)
        max_abs = metrics.maximum_absolute_error;
    if (metrics.maximum_relative_error > max_rel)
        max_rel = metrics.maximum_relative_error;
    live_history_release(&history);

    printf("attention_cuda_classes_executed=%llu\n", classes);
    printf("attention_cuda_kernel_launches=%llu\n", launches);
    printf("attention_cuda_peak_device_bytes=%llu\n", peak_device_bytes);
    printf("attention_cuda_reference_values=%llu\n", compared_values);
    printf("attention_cuda_reference_max_abs=%.17g\n", max_abs);
    printf("attention_cuda_reference_max_rel=%.17g\n", max_rel);
    printf("attention_cuda_swa_repeat_deterministic=1\n");
    printf("attention_cuda_fault_cases=%zu\n",
           sizeof(fault_cases) / sizeof(fault_cases[0]));
    printf("attention_cuda_fault_cleanup=1\n");
    printf("attention_cuda_csa_topk_candidates=513\n");
    printf("attention_cuda_csa_topk_selected=512\n");
    printf("attention_cuda_hca_ratio128_boundary=1\n");
    rc = YVEX_OK;

cleanup:
    (void)unsetenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&failed_trace);
    live_history_release(&history);
    free(input);
    yvex_backend_close(backend);
    return rc;
}

int main(int argc, char **argv)
{
    const char *source_path;
    const char *models_root;
    const char *manifest_path;
    int plan_only = 0;
    char artifact_path[YVEX_ARTIFACT_PATH_CAP];
    yvex_deepseek_payload_handoff_options handoff_options;
    yvex_deepseek_payload_handoff *handoff = NULL;
    yvex_deepseek_payload_failure handoff_failure;
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_complete_artifact_admission admission;
    yvex_artifact_admission_failure admission_failure;
    yvex_materialization_options materialization_options;
    yvex_materialization_plan *materialization_plan = NULL;
    yvex_materialization_session *session = NULL;
    yvex_materialization_failure materialization_failure;
    yvex_deepseek_v4_ir *architecture_ir = NULL;
    yvex_deepseek_v4_ir_failure architecture_failure;
    yvex_runtime_descriptor *descriptor = NULL;
    yvex_runtime_descriptor_failure descriptor_failure;
    yvex_attention_plan *attention_plan = NULL;
    yvex_attention_failure attention_failure;
    const yvex_materialization_summary *materialization_summary;
    const yvex_runtime_descriptor_summary *descriptor_summary;
    const yvex_attention_summary *attention_summary;
    const char *execution_reason = NULL;
    yvex_error err;
    int rc;

    if (argc == 5 && strcmp(argv[1], "--plan-only") == 0) {
        plan_only = 1;
        source_path = argv[2];
        models_root = argv[3];
        manifest_path = argv[4];
    } else if (argc == 4) {
        source_path = argv[1];
        models_root = argv[2];
        manifest_path = argv[3];
    } else {
        fprintf(stderr,
                "usage: attention_deepseek [--plan-only] SOURCE MODELS_ROOT SOURCE_MANIFEST\n");
        return 2;
    }

    if (!path_join_selected(artifact_path, sizeof(artifact_path),
                            models_root)) {
        fprintf(stderr, "artifact_path_build=fail\n");
        return 1;
    }

    memset(&handoff_options, 0, sizeof(handoff_options));
    handoff_options.source_path = source_path;
    handoff_options.models_root = models_root;
    handoff_options.manifest_path = manifest_path;
    yvex_source_payload_budget_default(&handoff_options.budget);
    handoff_options.budget.maximum_open_handles = 32u;
    handoff_options.budget.maximum_streams = 16u;
    handoff_options.budget.maximum_inflight_host_bytes =
        handoff_options.budget.chunk_bytes *
        handoff_options.budget.maximum_streams;
    handoff_options.chunk_bytes = handoff_options.budget.chunk_bytes;
    handoff_options.page_bytes = handoff_options.budget.page_bytes;

    rc = yvex_model_register_deepseek_v4()->payload.open(
        &handoff, &handoff_options, &handoff_failure, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "handoff_failure=%d where=%s message=%s\n",
                handoff_failure.code, yvex_error_where(&err),
                yvex_error_message(&err));
        return 1;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = artifact_path;
    artifact_options.readonly = 1;
    artifact_options.map = 0;
    rc = yvex_artifact_open(&artifact, &artifact_options, &err);
    if (rc == YVEX_OK &&
        yvex_artifact_size(artifact) != YVEX_SELECTED_DEEPSEEK_FILE_BYTES) {
        fprintf(stderr, "artifact_size_mismatch expected=%llu actual=%llu\n",
                YVEX_SELECTED_DEEPSEEK_FILE_BYTES,
                yvex_artifact_size(artifact));
        rc = YVEX_ERR_FORMAT;
    }
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "artifact_open_failure where=%s message=%s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        goto cleanup_fail;
    }

    rc = yvex_artifact_admit_deepseek(
        artifact, &admission, &admission_failure, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr,
                "admission_failure=%s field=%s expected=%llu actual=%llu message=%s\n",
                yvex_artifact_admission_code_name(admission_failure.code),
                admission_failure.field, admission_failure.expected,
                admission_failure.actual, yvex_error_message(&err));
        goto cleanup_fail;
    }

    yvex_materialization_options_default(&materialization_options);
    materialization_options.require_deepseek_map = 1;
    materialization_options.max_chunk_bytes = 16ull * 1024ull * 1024ull;
    materialization_options.cache_budget_bytes = 256ull * 1024ull * 1024ull;
    materialization_options.backend_resident_budget_bytes = 0ull;
    materialization_options.future_graph_scratch_reserve_bytes =
        2ull * 1024ull * 1024ull * 1024ull;
    materialization_options.future_kv_reserve_bytes =
        2ull * 1024ull * 1024ull * 1024ull;

    rc = yvex_materialization_plan_build(
        &materialization_plan, &admission, artifact, gguf, tensors,
        yvex_model_register_deepseek_v4()->payload.map(handoff), &materialization_options,
        &materialization_failure, &err);
    if (rc != YVEX_OK) {
        print_materialization_failure("materialization-plan",
                                      &materialization_failure, &err);
        goto cleanup_fail;
    }
    rc = yvex_materialization_session_open(
        &session, materialization_plan, artifact, &materialization_options,
        &materialization_failure, &err);
    if (rc == YVEX_OK)
        rc = yvex_materialization_session_commit(
            session, &materialization_failure, &err);
    if (rc != YVEX_OK) {
        print_materialization_failure("materialization-commit",
                                      &materialization_failure, &err);
        goto cleanup_fail;
    }

    rc = yvex_model_register_deepseek_v4()->ir.build(
        &architecture_ir, yvex_model_register_deepseek_v4()->payload.verification(handoff),
        &architecture_failure, &err);
    if (rc != YVEX_OK) {
        print_architecture_failure(&architecture_failure, &err);
        goto cleanup_fail;
    }

    rc = yvex_runtime_descriptor_build_deepseek(
        &descriptor, &admission, session,
        yvex_model_register_deepseek_v4()->payload.map(handoff), architecture_ir,
        &descriptor_failure, &err);
    if (rc != YVEX_OK) {
        print_descriptor_failure("descriptor", &descriptor_failure, &err);
        goto cleanup_fail;
    }

    rc = yvex_graph_lower_deepseek_v4()->plan_build(
        &attention_plan, architecture_ir, session, descriptor,
        &attention_failure, &err);
    if (rc != YVEX_OK) {
        print_attention_failure(&attention_failure, &err);
        goto cleanup_fail;
    }

    {
        yvex_runtime_descriptor *mutated_descriptor = NULL;
        yvex_attention_plan *mutated_plan = NULL;
        yvex_deepseek_v4_layer_spec *mutable_layer =
            (yvex_deepseek_v4_layer_spec *)
                yvex_model_register_deepseek_v4()->ir.layer_at(architecture_ir, 2ull);
        const yvex_runtime_descriptor_summary *canonical_runtime =
            yvex_runtime_descriptor_summary_get(descriptor);
        const yvex_attention_summary *canonical_attention =
            yvex_graph_lower_deepseek_v4()->plan_summary(attention_plan);
        const yvex_materialization_summary *canonical_materialization =
            yvex_materialization_session_summary(session);
        yvex_attention_cpu_options stale_options;
        yvex_attention_cpu_result stale_result;
        unsigned long long original_topk;
        unsigned long long bytes_before;
        char canonical_logical[YVEX_TRANSFORM_IR_IDENTITY_CAP];
        char mutated_logical[YVEX_TRANSFORM_IR_IDENTITY_CAP];
        char repeated_logical[YVEX_TRANSFORM_IR_IDENTITY_CAP];

        if (!mutable_layer || !canonical_runtime || !canonical_attention ||
            !canonical_materialization ||
            !yvex_model_register_deepseek_v4()->transform.architecture_identity(
                architecture_ir, canonical_logical))
            goto cleanup_fail;
        original_topk = mutable_layer->sparse_topk.k;
        mutable_layer->sparse_topk.k = original_topk - 1ull;
        if (!yvex_model_register_deepseek_v4()->transform.architecture_identity(
                architecture_ir, mutated_logical) ||
            !yvex_model_register_deepseek_v4()->transform.architecture_identity(
                architecture_ir, repeated_logical) ||
            strcmp(canonical_logical, mutated_logical) == 0 ||
            strcmp(mutated_logical, repeated_logical) != 0)
            goto identity_mutation_fail;
        rc = yvex_runtime_descriptor_build_deepseek(
            &mutated_descriptor, &admission, session,
            yvex_model_register_deepseek_v4()->payload.map(handoff), architecture_ir,
            &descriptor_failure, &err);
        if (rc != YVEX_OK) goto identity_mutation_fail;
        rc = yvex_graph_lower_deepseek_v4()->plan_build(
            &mutated_plan, architecture_ir, session, mutated_descriptor,
            &attention_failure, &err);
        if (rc != YVEX_OK ||
            strcmp(canonical_runtime->runtime_numeric_identity,
                   yvex_runtime_descriptor_summary_get(mutated_descriptor)
                       ->runtime_numeric_identity) == 0 ||
            strcmp(canonical_runtime->runtime_descriptor_identity,
                   yvex_runtime_descriptor_summary_get(mutated_descriptor)
                       ->runtime_descriptor_identity) == 0 ||
            strcmp(canonical_attention->attention_plan_identity,
                   yvex_graph_lower_deepseek_v4()->plan_summary(mutated_plan)
                       ->attention_plan_identity) == 0)
            goto identity_mutation_fail;
        bytes_before = canonical_materialization->payload_bytes_accessed;
        memset(&stale_result, 0, sizeof(stale_result));
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&stale_options);
        stale_options.layer_index = 2ull;
        stale_options.token_position = 0ull;
        rc = yvex_graph_lower_deepseek_v4()->cpu_probe_execute(
            attention_plan, architecture_ir, session, descriptor,
            &stale_options, &stale_result, &attention_failure, &err);
        if (rc == YVEX_OK ||
            attention_failure.code !=
                YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR ||
            stale_result.executed ||
            canonical_materialization->payload_bytes_accessed != bytes_before)
            goto identity_mutation_fail;
        mutable_layer->sparse_topk.k = original_topk;
        if (!yvex_model_register_deepseek_v4()->transform.architecture_identity(
                architecture_ir, repeated_logical) ||
            strcmp(canonical_logical, repeated_logical) != 0 ||
            strcmp(admission.artifact_identity,
                   canonical_attention->artifact_identity) != 0 ||
            strcmp(canonical_materialization->plan_identity,
                   canonical_attention->materialization_plan_identity) != 0)
            goto identity_mutation_fail;
        printf("attention_identity_canonical_logical=%s\n", canonical_logical);
        printf("attention_identity_mutated_logical=%s\n", mutated_logical);
        printf("attention_identity_mutation_deterministic=1\n");
        printf("attention_identity_runtime_propagated=1\n");
        printf("attention_identity_plan_propagated=1\n");
        printf("attention_identity_stale_refused_before_payload=1\n");
        printf("attention_identity_artifact_unchanged=1\n");
        printf("attention_identity_materialization_unchanged=1\n");
        yvex_graph_lower_deepseek_v4()->plan_close(mutated_plan);
        yvex_runtime_descriptor_close(mutated_descriptor);
        yvex_error_clear(&err);
        memset(&attention_failure, 0, sizeof(attention_failure));
        goto identity_mutation_done;

identity_mutation_fail:
        mutable_layer->sparse_topk.k = original_topk;
        yvex_graph_lower_deepseek_v4()->plan_close(mutated_plan);
        yvex_runtime_descriptor_close(mutated_descriptor);
        goto cleanup_fail;
identity_mutation_done:
        ;
    }

    materialization_summary =
        yvex_materialization_session_summary(session);
    descriptor_summary = yvex_runtime_descriptor_summary_get(descriptor);
    attention_summary = yvex_graph_lower_deepseek_v4()->plan_summary(attention_plan);
    (void)yvex_attention_execute_supported(&execution_reason);

    printf("mode=%s\n", plan_only ? "plan-only" : "cpu-probe");
    printf("artifact_path=%s\n", artifact_path);
    printf("artifact_identity=%s\n", admission.artifact_identity);
    printf("materialization_plan_identity=%s\n",
           materialization_summary->plan_identity);
    printf("runtime_descriptor_identity=%s\n",
           descriptor_summary->runtime_descriptor_identity);
    printf("runtime_numeric_identity=%s\n",
           descriptor_summary->runtime_numeric_identity);
    printf("runtime_numeric_schema_version=%u\n",
           descriptor_summary->runtime_numeric_schema_version);
    printf("runtime_hadamard_revision=%s\n",
           descriptor_summary->runtime_hadamard_revision);
    printf("attention_plan_status=%s\n",
           yvex_attention_status_name(attention_summary->status));
    printf("attention_plan_identity=%s\n",
           attention_summary->attention_plan_identity);
    printf("attention_layers=%llu\n", attention_summary->layer_count);
    printf("attention_swa_layers=%llu\n", attention_summary->swa_layer_count);
    printf("attention_csa_layers=%llu\n", attention_summary->csa_layer_count);
    printf("attention_hca_layers=%llu\n", attention_summary->hca_layer_count);
    printf("attention_required_bindings=%llu\n",
           attention_summary->required_binding_count);
    printf("attention_payload_bytes_bound=%llu\n",
           attention_summary->payload_bytes_bound);
    printf("attention_history_contract_ready=%d\n",
           attention_summary->history_contract_ready);
    printf("attention_state_delta_contract_ready=%d\n",
           attention_summary->state_delta_contract_ready);
    printf("attention_cpu_reference_ready=%d\n",
           attention_summary->cpu_reference_ready);
    printf("attention_cuda_execution_ready=%d\n",
           attention_summary->cuda_execution_ready);
    printf("attention_full_execution_ready=%d\n",
           attention_summary->full_execution_ready);
    printf("attention_execution_supported=%d\n",
           yvex_attention_execute_supported(NULL));
    printf("attention_execution_refusal=%s\n",
           execution_reason ? execution_reason : "");
    printf("runtime_generation_ready=%d\n",
           descriptor_summary->generation_ready);
    printf("payload_bytes_accessed=%llu\n",
           materialization_summary->payload_bytes_accessed);

    if (!plan_only) {
        yvex_attention_cpu_options exec_options;
        yvex_attention_cpu_result exec_result;
        yvex_test_attention_reference_metrics reference_metrics;
        unsigned long long first_swa = ~0ull;
        unsigned long long first_csa = ~0ull;
        unsigned long long first_hca = ~0ull;
        unsigned long long executed = 0ull;
        unsigned long long payload_read = 0ull;
        unsigned long long first_token_payload_read = 0ull;
        unsigned long long first_token_executed = 0ull;
        unsigned long long chunk_payload_read = 0ull;
        unsigned long long chunk_executed = 0ull;
        unsigned long long chunk_repeat_payload_read = 0ull;
        unsigned long long chunk_repeat_executed = 0ull;
        unsigned long long history_chunk_payload_read = 0ull;
        unsigned long long history_chunk_executed = 0ull;
        unsigned long long i;
        double checksum = 0.0;
        double first_token_checksum = 0.0;
        double chunk_checksum = 0.0;
        double history_chunk_checksum = 0.0;
        char chunk_identity[3][YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
        int cuda_only = getenv("YVEX_ATTENTION_CUDA_ONLY") != NULL;

        memset(chunk_identity, 0, sizeof(chunk_identity));

        for (i = 0ull; i < yvex_graph_lower_deepseek_v4()->plan_layer_count(attention_plan); ++i) {
            const yvex_attention_layer_plan *layer =
                yvex_graph_lower_deepseek_v4()->plan_layer_at(attention_plan, i);
            if (!layer) continue;
            if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA &&
                first_swa == ~0ull)
                first_swa = i;
            if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
                first_csa == ~0ull)
                first_csa = i;
            if (layer->attention_class == YVEX_ATTENTION_CLASS_HCA &&
                first_hca == ~0ull)
                first_hca = i;
        }
        if (!cuda_only) {
#define RUN_PROBE(layer_id, token_id) do {                                      \
    if ((layer_id) != ~0ull) {                                                  \
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);             \
        exec_options.layer_index = (layer_id);                                  \
        exec_options.token_position = (token_id);                               \
        exec_options.collect_reference_metrics = 1;                             \
        rc = yvex_graph_lower_deepseek_v4()->cpu_probe_execute(                         \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &attention_failure, &err);             \
        if (rc != YVEX_OK) {                                                    \
            print_attention_failure(&attention_failure, &err);                  \
            goto cleanup_fail;                                                  \
        }                                                                       \
        printf("attention_cpu_probe.layer.%llu.class=%s\n", executed,           \
               yvex_model_register_deepseek_v4()->ir.attention_name(exec_result.attention_class));   \
        printf("attention_cpu_probe.layer.%llu.index=%llu\n", executed,         \
               exec_result.layer_index);                                        \
        printf("attention_cpu_probe.layer.%llu.q_a_rows=%llu\n", executed,      \
               exec_result.q_a_rows);                                           \
        printf("attention_cpu_probe.layer.%llu.q_b_rows=%llu\n", executed,      \
               exec_result.q_b_rows);                                           \
        printf("attention_cpu_probe.layer.%llu.kv_rows=%llu\n", executed,       \
               exec_result.kv_rows);                                            \
        printf("attention_cpu_probe.layer.%llu.compressor_rows=%llu\n",         \
               executed, exec_result.compressor_rows);                          \
        printf("attention_cpu_probe.layer.%llu.indexer_rows=%llu\n", executed,  \
               exec_result.indexer_rows);                                       \
        printf("attention_cpu_probe.layer.%llu.state_raw_entries=%llu\n",       \
               executed, exec_result.state_raw_entries);                        \
        printf("attention_cpu_probe.layer.%llu.state_compressed_entries=%llu\n", \
               executed, exec_result.state_compressed_entries);                 \
        printf("attention_cpu_probe.layer.%llu.state_indexer_entries=%llu\n",   \
               executed, exec_result.state_indexer_entries);                    \
        printf("attention_cpu_probe.layer.%llu.topk_candidates=%llu\n",         \
               executed, exec_result.topk_candidates);                          \
        printf("attention_cpu_probe.layer.%llu.topk_selected=%llu\n", executed, \
               exec_result.topk_selected);                                      \
        printf("attention_cpu_probe.layer.%llu.payload_bytes_read=%llu\n",      \
               executed, exec_result.payload_bytes_read);                       \
        printf("attention_cpu_probe.layer.%llu.q_checksum=%.17g\n", executed,   \
               exec_result.q_projection_checksum);                              \
        printf("attention_cpu_probe.layer.%llu.kv_checksum=%.17g\n", executed,  \
               exec_result.kv_projection_checksum);                             \
        printf("attention_cpu_probe.layer.%llu.attention_checksum=%.17g\n",     \
               executed, exec_result.attention_checksum);                       \
        printf("attention_cpu_probe.layer.%llu.reference_comparisons=%llu\n",   \
               executed, exec_result.reference_comparisons);                    \
        printf("attention_cpu_probe.layer.%llu.max_abs_error=%.17g\n",          \
               executed, exec_result.max_abs_error);                            \
        printf("attention_cpu_probe.layer.%llu.max_relative_error=%.17g\n",     \
               executed, exec_result.max_relative_error);                       \
        printf("attention_cpu_probe.layer.%llu.rmse=%.17g\n", executed,         \
               exec_result.rmse);                                               \
        printf("attention_cpu_probe.layer.%llu.full_attention=%d\n", executed,  \
               exec_result.full_attention);                                     \
        payload_read += exec_result.payload_bytes_read;                         \
        checksum += exec_result.output_checksum;                                \
        executed++;                                                             \
    }                                                                           \
} while (0)
        RUN_PROBE(first_swa, 7ull);
        RUN_PROBE(first_csa, 515ull);
        RUN_PROBE(first_hca, 4095ull);
#undef RUN_PROBE
#define RUN_FIRST_TOKEN(layer_id) do {                                          \
    if ((layer_id) != ~0ull) {                                                  \
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);             \
        exec_options.layer_index = (layer_id);                                  \
        exec_options.token_position = 0ull;                                     \
        exec_options.collect_reference_metrics = 0;                             \
        exec_options.token_count = 1ull;                                         \
        rc = run_reference_compare(                                              \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &reference_metrics, NULL,               \
            &attention_failure, &err);                                           \
        if (rc != YVEX_OK) {                                                    \
            print_attention_failure(&attention_failure, &err);                  \
            goto cleanup_fail;                                                  \
        }                                                                       \
        printf("attention_cpu_first_token.layer.%llu.class=%s\n",              \
               first_token_executed,                                           \
               yvex_model_register_deepseek_v4()->ir.attention_name(exec_result.attention_class));  \
        printf("attention_cpu_first_token.layer.%llu.index=%llu\n",            \
               first_token_executed, exec_result.layer_index);                 \
        printf("attention_cpu_first_token.layer.%llu.q_a_rows=%llu\n",         \
               first_token_executed, exec_result.q_a_rows);                    \
        printf("attention_cpu_first_token.layer.%llu.q_b_rows=%llu\n",         \
               first_token_executed, exec_result.q_b_rows);                    \
        printf("attention_cpu_first_token.layer.%llu.kv_rows=%llu\n",          \
               first_token_executed, exec_result.kv_rows);                     \
        printf("attention_cpu_first_token.layer.%llu.compressor_rows=%llu\n",  \
               first_token_executed, exec_result.compressor_rows);             \
        printf("attention_cpu_first_token.layer.%llu.indexer_rows=%llu\n",     \
               first_token_executed, exec_result.indexer_rows);                \
        printf("attention_cpu_first_token.layer.%llu.local_entries=%llu\n",    \
               first_token_executed, exec_result.local_entries);               \
        printf("attention_cpu_first_token.layer.%llu.payload_bytes_read=%llu\n", \
               first_token_executed, exec_result.payload_bytes_read);          \
        printf("attention_cpu_first_token.layer.%llu.q_checksum=%.17g\n",      \
               first_token_executed, exec_result.q_projection_checksum);       \
        printf("attention_cpu_first_token.layer.%llu.kv_checksum=%.17g\n",     \
               first_token_executed, exec_result.kv_projection_checksum);      \
        printf("attention_cpu_first_token.layer.%llu.attention_checksum=%.17g\n", \
               first_token_executed, exec_result.attention_checksum);          \
        printf("attention_cpu_first_token.layer.%llu.output_checksum=%.17g\n", \
               first_token_executed, exec_result.output_checksum);             \
        printf("attention_cpu_first_token.layer.%llu.output_identity=%s\n",   \
               first_token_executed, exec_result.output_identity);             \
        printf("attention_cpu_first_token.layer.%llu.full_attention=%d\n",     \
               first_token_executed, exec_result.full_attention);              \
        printf("attention_cpu_first_token.layer.%llu.reference_values=%llu\n", \
               first_token_executed, reference_metrics.compared_values);        \
        printf("attention_cpu_first_token.layer.%llu.reference_max_abs=%.17g\n", \
               first_token_executed, reference_metrics.maximum_absolute_error); \
        printf("attention_cpu_first_token.layer.%llu.reference_max_rel=%.17g\n", \
               first_token_executed, reference_metrics.maximum_relative_error); \
        first_token_payload_read += exec_result.payload_bytes_read;             \
        first_token_checksum += exec_result.output_checksum;                    \
        first_token_executed++;                                                 \
    }                                                                           \
} while (0)
        RUN_FIRST_TOKEN(first_swa);
        RUN_FIRST_TOKEN(first_csa);
        RUN_FIRST_TOKEN(first_hca);
#undef RUN_FIRST_TOKEN
#define RUN_CHUNK(layer_id, token_start, tokens) do {                          \
    if ((layer_id) != ~0ull) {                                                  \
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);             \
        exec_options.layer_index = (layer_id);                                  \
        exec_options.token_position = (token_start);                            \
        exec_options.token_count = (tokens);                                    \
        exec_options.collect_reference_metrics = 0;                             \
        rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(                         \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &attention_failure, &err);             \
        if (rc != YVEX_OK) {                                                    \
            print_attention_failure(&attention_failure, &err);                  \
            goto cleanup_fail;                                                  \
        }                                                                       \
        printf("attention_cpu_chunk.layer.%llu.class=%s\n", chunk_executed,    \
               yvex_model_register_deepseek_v4()->ir.attention_name(exec_result.attention_class));  \
        printf("attention_cpu_chunk.layer.%llu.index=%llu\n", chunk_executed,  \
               exec_result.layer_index);                                        \
        printf("attention_cpu_chunk.layer.%llu.token_start=%llu\n",            \
               chunk_executed, exec_result.token_position);                     \
        printf("attention_cpu_chunk.layer.%llu.token_count=%llu\n",            \
               chunk_executed, exec_result.local_entries);                      \
        printf("attention_cpu_chunk.layer.%llu.q_a_rows=%llu\n",               \
               chunk_executed, exec_result.q_a_rows);                           \
        printf("attention_cpu_chunk.layer.%llu.q_b_rows=%llu\n",               \
               chunk_executed, exec_result.q_b_rows);                           \
        printf("attention_cpu_chunk.layer.%llu.kv_rows=%llu\n",                \
               chunk_executed, exec_result.kv_rows);                            \
        printf("attention_cpu_chunk.layer.%llu.compressed_entries=%llu\n",      \
               chunk_executed, exec_result.compressed_entries);                 \
        printf("attention_cpu_chunk.layer.%llu.indexer_entries=%llu\n",        \
               chunk_executed, exec_result.state_indexer_entries);              \
        printf("attention_cpu_chunk.layer.%llu.payload_bytes_read=%llu\n",      \
               chunk_executed, exec_result.payload_bytes_read);                 \
        printf("attention_cpu_chunk.layer.%llu.attention_checksum=%.17g\n",    \
               chunk_executed, exec_result.attention_checksum);                 \
        printf("attention_cpu_chunk.layer.%llu.output_checksum=%.17g\n",       \
               chunk_executed, exec_result.output_checksum);                    \
        printf("attention_cpu_chunk.layer.%llu.output_identity=%s\n",          \
               chunk_executed, exec_result.output_identity);                    \
        printf("attention_cpu_chunk.layer.%llu.full_attention=%d\n",           \
               chunk_executed, exec_result.full_attention);                     \
        if (chunk_executed < 3ull)                                              \
            snprintf(chunk_identity[chunk_executed],                            \
                     sizeof(chunk_identity[chunk_executed]), "%s",             \
                     exec_result.output_identity);                              \
        chunk_payload_read += exec_result.payload_bytes_read;                   \
        chunk_checksum += exec_result.output_checksum;                          \
        chunk_executed++;                                                       \
    }                                                                           \
} while (0)
        RUN_CHUNK(first_swa, 0ull, 4ull);
        RUN_CHUNK(first_csa, 0ull, 4ull);
        RUN_CHUNK(first_hca, 0ull, 4ull);
#undef RUN_CHUNK
#define RUN_CHUNK_REPEAT(layer_id, token_start, tokens) do {                   \
    if ((layer_id) != ~0ull) {                                                  \
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);             \
        exec_options.layer_index = (layer_id);                                  \
        exec_options.token_position = (token_start);                            \
        exec_options.token_count = (tokens);                                    \
        exec_options.collect_reference_metrics = 0;                             \
        rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(                         \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &attention_failure, &err);             \
        if (rc != YVEX_OK) {                                                    \
            print_attention_failure(&attention_failure, &err);                  \
            goto cleanup_fail;                                                  \
        }                                                                       \
        if (chunk_repeat_executed >= 3ull ||                                    \
            strcmp(chunk_identity[chunk_repeat_executed],                       \
                   exec_result.output_identity) != 0) {                         \
            fprintf(stderr,                                                     \
                    "attention chunk repeat identity mismatch at %llu\n",       \
                    chunk_repeat_executed);                                     \
            goto cleanup_fail;                                                  \
        }                                                                       \
        printf("attention_cpu_chunk_repeat.layer.%llu.identity_match=1\n",     \
               chunk_repeat_executed);                                          \
        printf("attention_cpu_chunk_repeat.layer.%llu.output_identity=%s\n",   \
               chunk_repeat_executed, exec_result.output_identity);             \
        chunk_repeat_payload_read += exec_result.payload_bytes_read;            \
        chunk_repeat_executed++;                                                \
    }                                                                           \
} while (0)
        RUN_CHUNK_REPEAT(first_swa, 0ull, 4ull);
        RUN_CHUNK_REPEAT(first_csa, 0ull, 4ull);
        RUN_CHUNK_REPEAT(first_hca, 0ull, 4ull);
#undef RUN_CHUNK_REPEAT
        if (first_swa != ~0ull) {
            const yvex_attention_layer_plan *layer =
                yvex_graph_lower_deepseek_v4()->plan_layer_at(attention_plan,
                                                      first_swa);
            yvex_attention_history_view history;
            float *history_values = NULL;
            unsigned long long history_positions[2] = {0ull, 1ull};
            if (!layer) goto cleanup_fail;
            history_values = (float *)calloc(
                2ull * (size_t)layer->head_dimension, sizeof(float));
            if (!history_values) goto cleanup_fail;
            fill_history_values(history_values, 2ull * layer->head_dimension,
                                first_swa);
            memset(&history, 0, sizeof(history));
            history.immutable = 1;
            history.token_count = 2ull;
            history.local_tail_count = 2ull;
            history.local_kv = history_values;
            history.local_kv_stride = layer->head_dimension;
            history.local_positions = history_positions;
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_swa;
            exec_options.token_position = 2ull;
            exec_options.token_count = 2ull;
            exec_options.history = &history;
            exec_options.collect_reference_metrics = 0;
            rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &attention_failure, &err);
            free(history_values);
            if (rc != YVEX_OK) {
                print_attention_failure(&attention_failure, &err);
                goto cleanup_fail;
            }
            printf("attention_cpu_history_chunk.layer.0.class=%s\n",
                   yvex_model_register_deepseek_v4()->ir.attention_name(
                       exec_result.attention_class));
            printf("attention_cpu_history_chunk.layer.0.index=%llu\n",
                   exec_result.layer_index);
            printf("attention_cpu_history_chunk.layer.0.token_start=%llu\n",
                   exec_result.token_position);
            printf("attention_cpu_history_chunk.layer.0.local_entries=%llu\n",
                   exec_result.local_entries);
            printf("attention_cpu_history_chunk.layer.0.payload_bytes_read=%llu\n",
                   exec_result.payload_bytes_read);
            printf("attention_cpu_history_chunk.layer.0.output_checksum=%.17g\n",
                   exec_result.output_checksum);
            printf("attention_cpu_history_chunk.layer.0.output_identity=%s\n",
                   exec_result.output_identity);
            printf("attention_cpu_history_chunk.layer.0.full_attention=%d\n",
                   exec_result.full_attention);
            history_chunk_payload_read += exec_result.payload_bytes_read;
            history_chunk_checksum += exec_result.output_checksum;
            history_chunk_executed++;
        }
        if (first_csa != ~0ull) {
            const yvex_attention_layer_plan *layer =
                yvex_graph_lower_deepseek_v4()->plan_layer_at(attention_plan,
                                                      first_csa);
            live_attention_history history;
            yvex_attention_cpu_result changed_result;
            double baseline_checksum;
            unsigned long long j;

            if (!layer || !live_history_init(
                    &history, layer, attention_summary, 2052ull, 4ull,
                    513ull))
                goto cleanup_fail;
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_csa;
            exec_options.token_position = 2052ull;
            exec_options.token_count = 1ull;
            exec_options.history = &history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics, NULL,
                &attention_failure, &err);
            if (rc != YVEX_OK || exec_result.topk_candidates != 513ull ||
                exec_result.topk_selected != 512ull) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                live_history_release(&history);
                goto cleanup_fail;
            }
            baseline_checksum = exec_result.output_checksum;
            history.view.compressed_entry_count = 7ull;
            history.view.indexer_entry_count = 7ull;
            rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &attention_failure, &err);
            if (rc != YVEX_OK || changed_result.topk_candidates != 7ull ||
                changed_result.topk_selected != 7ull) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            history.view.compressed_entry_count = 512ull;
            history.view.indexer_entry_count = 512ull;
            rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &attention_failure, &err);
            if (rc != YVEX_OK || changed_result.topk_candidates != 512ull ||
                changed_result.topk_selected != 512ull) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            history.view.compressed_entry_count = 513ull;
            history.view.indexer_entry_count = 513ull;
            for (j = 0ull; j < 513ull * layer->head_dimension; ++j)
                history.compressed_kv[j] = -history.compressed_kv[j];
            rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &attention_failure, &err);
            if (rc != YVEX_OK ||
                changed_result.output_checksum == baseline_checksum) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                live_history_release(&history);
                goto cleanup_fail;
            }
            printf("attention_csa_real_candidates=513\n");
            printf("attention_csa_fewer_than_topk=7\n");
            printf("attention_csa_exact_topk=512\n");
            printf("attention_csa_topk_selected=512\n");
            printf("attention_csa_selected_kv_affects_output=1\n");
            printf("attention_csa_reference_values=%llu\n",
                   reference_metrics.compared_values);
            live_history_release(&history);
        }
        if (first_hca != ~0ull) {
            const yvex_attention_layer_plan *layer =
                yvex_graph_lower_deepseek_v4()->plan_layer_at(attention_plan,
                                                      first_hca);
            live_attention_history boundary_history;
            live_attention_history multi_history;
            yvex_attention_execution_trace boundary_trace;
            yvex_attention_cpu_result after_result;
            yvex_attention_cpu_result changed_result;
            float *next_local;
            unsigned long long *next_positions;

            memset(&boundary_trace, 0, sizeof(boundary_trace));
            if (!layer || !live_history_init(
                    &boundary_history, layer, attention_summary, 127ull,
                    32ull, 0ull))
                goto cleanup_fail;
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_hca;
            exec_options.token_position = 127ull;
            exec_options.token_count = 1ull;
            exec_options.history = &boundary_history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics,
                &boundary_trace, &attention_failure, &err);
            if (rc != YVEX_OK || boundary_trace.compressed_count != 1ull ||
                !boundary_trace.next_main_rolling_state.present) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                yvex_graph_lower_deepseek_v4()->execution_trace_release(
                    &boundary_trace);
                live_history_release(&boundary_history);
                goto cleanup_fail;
            }
            boundary_history.compressed_kv = (float *)malloc(
                (size_t)layer->head_dimension * sizeof(float));
            boundary_history.compressed_positions =
                (unsigned long long *)malloc(sizeof(unsigned long long));
            next_local = (float *)realloc(
                boundary_history.local_kv,
                (size_t)(33ull * layer->head_dimension) * sizeof(float));
            next_positions = (unsigned long long *)realloc(
                boundary_history.local_positions,
                33ull * sizeof(unsigned long long));
            if (!boundary_history.compressed_kv ||
                !boundary_history.compressed_positions || !next_local ||
                !next_positions) {
                yvex_graph_lower_deepseek_v4()->execution_trace_release(
                    &boundary_trace);
                live_history_release(&boundary_history);
                goto cleanup_fail;
            }
            boundary_history.local_kv = next_local;
            boundary_history.local_positions = next_positions;
            memcpy(boundary_history.compressed_kv,
                   boundary_trace.compressed_kv,
                   (size_t)layer->head_dimension * sizeof(float));
            boundary_history.compressed_positions[0] =
                boundary_trace.compressed_positions[0];
            memcpy(boundary_history.local_kv +
                       32ull * layer->head_dimension,
                   boundary_trace.raw_kv,
                   (size_t)layer->head_dimension * sizeof(float));
            boundary_history.local_positions[32] = 127ull;
            boundary_history.view.local_tail_count = 33ull;
            boundary_history.view.local_kv = boundary_history.local_kv;
            boundary_history.view.local_positions =
                boundary_history.local_positions;
            boundary_history.view.compressed_entry_count = 1ull;
            boundary_history.view.compressed_kv =
                boundary_history.compressed_kv;
            boundary_history.view.compressed_positions =
                boundary_history.compressed_positions;
            boundary_history.view.compressed_kv_stride =
                layer->head_dimension;
            live_history_bind_next_state(&boundary_history, &boundary_trace);
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_hca;
            exec_options.token_position = 128ull;
            exec_options.token_count = 1ull;
            exec_options.history = &boundary_history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &after_result, &reference_metrics, NULL,
                &attention_failure, &err);
            if (rc != YVEX_OK || after_result.compressed_entries != 0ull) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                yvex_graph_lower_deepseek_v4()->execution_trace_release(
                    &boundary_trace);
                live_history_release(&boundary_history);
                goto cleanup_fail;
            }
            printf("attention_hca_fill_before_boundary=127\n");
            printf("attention_hca_boundary_emissions=1\n");
            printf("attention_hca_first_after_boundary=1\n");
            printf("attention_hca_external_compressed_used=%d\n",
                   after_result.topk_candidates == 1ull);
            yvex_graph_lower_deepseek_v4()->execution_trace_release(&boundary_trace);
            live_history_release(&boundary_history);

            if (!live_history_init(&multi_history, layer, attention_summary,
                                   384ull, 128ull, 3ull))
                goto cleanup_fail;
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_hca;
            exec_options.token_position = 384ull;
            exec_options.token_count = 1ull;
            exec_options.history = &multi_history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics, NULL,
                &attention_failure, &err);
            if (rc != YVEX_OK || exec_result.topk_candidates != 3ull ||
                exec_result.local_entries != 129ull) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                live_history_release(&multi_history);
                goto cleanup_fail;
            }
            {
                double baseline = exec_result.output_checksum;
                unsigned long long j;
                for (j = 0ull; j < 3ull * layer->head_dimension; ++j)
                    multi_history.compressed_kv[j] =
                        -multi_history.compressed_kv[j];
                rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &changed_result, &attention_failure, &err);
                if (rc != YVEX_OK ||
                    changed_result.output_checksum == baseline) {
                    live_history_release(&multi_history);
                    goto cleanup_fail;
                }
                for (j = 0ull; j < 3ull * layer->head_dimension; ++j)
                    multi_history.compressed_kv[j] =
                        -multi_history.compressed_kv[j];
                for (j = 0ull; j < 128ull * layer->head_dimension; ++j)
                    multi_history.local_kv[j] =
                        -multi_history.local_kv[j];
                rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &changed_result, &attention_failure, &err);
                if (rc != YVEX_OK ||
                    changed_result.output_checksum == baseline) {
                    live_history_release(&multi_history);
                    goto cleanup_fail;
                }
            }
            printf("attention_hca_multiple_compressed_groups=3\n");
            printf("attention_hca_local_window_entries=128\n");
            printf("attention_hca_full_output_reference=1\n");
            printf("attention_hca_compressed_affects_output=1\n");
            printf("attention_hca_local_affects_output=1\n");
            live_history_release(&multi_history);
        }
        {
            unsigned long long full_layer_count = first_token_executed;
            unsigned long long full_layer_payload = first_token_payload_read;
            double full_layer_checksum = first_token_checksum;

            for (i = 0ull;
                 i < yvex_graph_lower_deepseek_v4()->plan_layer_count(attention_plan);
                 ++i) {
                if (i == first_swa || i == first_csa || i == first_hca)
                    continue;
                yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
                exec_options.layer_index = i;
                exec_options.token_position = 0ull;
                exec_options.token_count = 1ull;
                rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &exec_result, &attention_failure, &err);
                if (rc != YVEX_OK || !exec_result.full_attention) {
                    if (rc != YVEX_OK)
                        print_attention_failure(&attention_failure, &err);
                    goto cleanup_fail;
                }
                full_layer_count++;
                full_layer_payload += exec_result.payload_bytes_read;
                full_layer_checksum += exec_result.output_checksum;
            }
            if (full_layer_count !=
                yvex_graph_lower_deepseek_v4()->plan_layer_count(attention_plan))
                goto cleanup_fail;
            printf("attention_full_release_layers_executed=%llu\n",
                   full_layer_count);
            printf("attention_full_release_layers_payload_bytes=%llu\n",
                   full_layer_payload);
            printf("attention_full_release_layers_checksum=%.17g\n",
                   full_layer_checksum);
        }
        printf("attention_cpu_probe_executed=%llu\n", executed);
        printf("attention_cpu_probe_payload_bytes_read=%llu\n", payload_read);
        printf("attention_cpu_probe_execution_checksum=%.17g\n", checksum);
        printf("attention_cpu_probe_full_attention=0\n");
        printf("attention_cpu_first_token_executed=%llu\n",
               first_token_executed);
        printf("attention_cpu_first_token_payload_bytes_read=%llu\n",
               first_token_payload_read);
        printf("attention_cpu_first_token_execution_checksum=%.17g\n",
               first_token_checksum);
        printf("attention_cpu_chunk_executed=%llu\n", chunk_executed);
        printf("attention_cpu_chunk_payload_bytes_read=%llu\n",
               chunk_payload_read);
        printf("attention_cpu_chunk_execution_checksum=%.17g\n",
               chunk_checksum);
        printf("attention_cpu_chunk_repeat_executed=%llu\n",
               chunk_repeat_executed);
        printf("attention_cpu_chunk_repeat_payload_bytes_read=%llu\n",
               chunk_repeat_payload_read);
        printf("attention_cpu_history_chunk_executed=%llu\n",
               history_chunk_executed);
        printf("attention_cpu_history_chunk_payload_bytes_read=%llu\n",
               history_chunk_payload_read);
        printf("attention_cpu_history_chunk_execution_checksum=%.17g\n",
               history_chunk_checksum);
        }
        rc = run_cuda_live_suite(
            attention_plan, architecture_ir, session, descriptor,
            attention_summary, first_swa, first_csa, first_hca,
            &attention_failure, &err);
        if (rc != YVEX_OK) {
            print_attention_failure(&attention_failure, &err);
            goto cleanup_fail;
        }
        printf("attention_cuda_evidence_exercised=1\n");
    }

    yvex_graph_lower_deepseek_v4()->plan_close(attention_plan);
    yvex_model_register_deepseek_v4()->ir.close(architecture_ir);
    yvex_runtime_descriptor_close(descriptor);
    yvex_materialization_session_close(session);
    yvex_materialization_plan_close(materialization_plan);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    yvex_model_register_deepseek_v4()->payload.close(handoff);
    return 0;

cleanup_fail:
    yvex_graph_lower_deepseek_v4()->plan_close(attention_plan);
    yvex_model_register_deepseek_v4()->ir.close(architecture_ir);
    yvex_runtime_descriptor_close(descriptor);
    yvex_materialization_session_close(session);
    yvex_materialization_plan_close(materialization_plan);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    yvex_model_register_deepseek_v4()->payload.close(handoff);
    return 1;
}
