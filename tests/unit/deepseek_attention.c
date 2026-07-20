/*
 * deepseek_attention.c - DeepSeek attention graph-boundary tests.
 *
 * Owner:
 *   tests/unit
 *
 * Owns:
 *   fixture coverage for DeepSeek attention admission refusal, immutable
 *   history-view validation, transactional state-delta lifecycle, runtime
 *   activation numerics, deterministic top-k, and reference mutation checks.
 *
 * Does not own:
 *   target-scale artifact access, full real-weight equations, CUDA execution,
 *   persistent KV storage, generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   tests use synthetic layer plans only and never read model payload bytes.
 *
 * Boundary:
 *   fixture numerics prove bounded primitives, not full-model execution.
 */
#include "tests/test.h"

#include "src/graph/private.h"
#include "tests/reference/deepseek_attention.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yvex_test_attention_reference_codec_selftest(void);

static yvex_attention_layer_plan layer_fixture(
    unsigned long long layer_index,
    yvex_attention_class attention_class,
    unsigned long long compression_ratio)
{
    yvex_attention_layer_plan layer;

    memset(&layer, 0, sizeof(layer));
    layer.layer_index = layer_index;
    layer.attention_class = attention_class;
    layer.compression_ratio = compression_ratio;
    layer.sliding_window = 128ull;
    layer.query_heads = 64ull;
    layer.kv_heads = 1ull;
    layer.head_dimension = 512ull;
    layer.rope_head_dimension = 64ull;
    layer.query_lora_rank = 1024ull;
    layer.output_lora_rank = 1024ull;
    layer.output_groups = 8ull;
    layer.hidden_dimension = 4096ull;
    if (attention_class == YVEX_ATTENTION_CLASS_CSA) {
        layer.compressor_required = 1;
        layer.indexer_required = 1;
        layer.indexer_heads = 64ull;
        layer.indexer_head_dimension = 128ull;
        layer.indexer_topk = 512ull;
        layer.sparse_topk.required = 1;
        layer.sparse_topk.k = 512ull;
    } else if (attention_class == YVEX_ATTENTION_CLASS_HCA) {
        layer.compressor_required = 1;
    }
    return layer;
}

typedef struct {
    unsigned int samples;
    unsigned int cancel_at;
} cancellation_fixture;

/* Purpose: request cancellation at one deterministic cooperative sample. */
static int cancellation_requested(void *opaque)
{
    cancellation_fixture *fixture = (cancellation_fixture *)opaque;
    fixture->samples++;
    return fixture->samples >= fixture->cancel_at;
}

static int test_cpu_resource_guards(void)
{
    yvex_attention_layer_plan layer =
        layer_fixture(0ull, YVEX_ATTENTION_CLASS_SWA, 0ull);
    yvex_attention_history_view history;
    yvex_attention_execution_trace trace;
    yvex_attention_scratch_budget scratch;
    yvex_materialized_tensor_binding binding;
    yvex_attention_cpu_result result;
    yvex_attention_failure failure;
    yvex_error err;
    unsigned long long row_bytes = 0ull;
    unsigned long long row_count = 0ull;
    unsigned long long trace_bytes = 0ull;
    float input[4096] = {0.0f};
    size_t reserved = 0u;
    int rc;

    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 31ull;
    YVEX_TEST_ASSERT(
        !yvex_attention_scratch_reserve(&scratch, 8ull, sizeof(float),
                                        &reserved) &&
            scratch.live_bytes == 0u,
        "scratch budget refuses one byte beyond the limit atomically");
    scratch.limit_bytes = 32ull;
    YVEX_TEST_ASSERT(
        yvex_attention_scratch_reserve(&scratch, 8ull, sizeof(float),
                                       &reserved) &&
            reserved == 32u && scratch.live_bytes == 32u &&
            scratch.peak_bytes == 32u,
        "scratch budget admits the exact limit");
    yvex_attention_scratch_release(&scratch, reserved);
    YVEX_TEST_ASSERT(scratch.live_bytes == 0u && scratch.peak_bytes == 32u,
                     "scratch release preserves peak accounting");

    memset(&history, 0, sizeof(history));
    rc = yvex_attention_cuda_trace_allocate(
        &trace, &layer, &history, 0ull, input, ULLONG_MAX, &trace_bytes,
        &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && trace_bytes != 0ull,
                     "CUDA trace reports its complete owned bytes");
    yvex_graph_lower_deepseek_v4()->publication_release(&trace);
    yvex_graph_lower_deepseek_v4()->publication_release(&trace);
    YVEX_TEST_ASSERT(!trace.owned && !trace.complete && !trace.output,
                     "attention publication release is idempotent");
    rc = yvex_attention_cuda_trace_allocate(
        &trace, &layer, &history, 0ull, input, trace_bytes - 1ull,
        &row_bytes, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_ERR_BOUNDS && row_bytes == 0ull && !trace.owned &&
            failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH,
        "CUDA trace refuses budget minus one before allocation");
    rc = yvex_attention_cuda_trace_allocate(
        &trace, &layer, &history, 0ull, input, trace_bytes, &row_bytes,
        &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && row_bytes == trace_bytes && trace.owned,
                     "CUDA trace admits its exact host budget");
    yvex_graph_lower_deepseek_v4()->publication_release(&trace);

    memset(&binding, 0, sizeof(binding));
    binding.role = YVEX_TENSOR_ROLE_ATTENTION_Q_A;
    binding.layer_index = 1ull;
    binding.row_width = 32ull;
    binding.row_count = 2ull;
    binding.block_size = 32ull;
    binding.bytes_per_block = 34ull;
    binding.encoded_bytes = 68ull;
    yvex_error_clear(&err);
    rc = yvex_attention_row_geometry(
        &binding, &row_bytes, &row_count, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && row_bytes == 34ull && row_count == 2ull,
                     "exact encoded row geometry is admitted");
    binding.encoded_bytes++;
    rc = yvex_attention_row_geometry(
        &binding, &row_bytes, &row_count, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_ERR_BOUNDS &&
            failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
        "trailing encoded tensor bytes refuse");
    binding.row_width = 1ull;
    binding.row_count = ULLONG_MAX;
    binding.block_size = 1ull;
    binding.bytes_per_block = 2ull;
    binding.encoded_bytes = 0ull;
    rc = yvex_attention_row_geometry(
        &binding, &row_bytes, &row_count, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS,
                     "encoded tensor byte multiplication overflow refuses");

    memset(&result, 0, sizeof(result));
    result.payload_bytes_read = ULLONG_MAX;
    rc = yvex_attention_payload_account(
        &result, 1ull, NULL, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_ERR_BOUNDS && result.payload_bytes_read == ULLONG_MAX,
        "payload evidence overflow refuses without counter mutation");
    return 0;
}

static void fill_vector(float *values,
                        unsigned long long count,
                        unsigned long long token,
                        unsigned long long salt)
{
    unsigned long long i;

    for (i = 0ull; i < count; ++i) {
        int lane = (int)(((i + 1ull) * 17ull + token * 31ull + salt) % 127ull) -
                   63;
        values[i] = (float)lane / 32.0f;
    }
}

static void fill_scores(float *values,
                        unsigned long long count,
                        unsigned long long token,
                        unsigned long long salt)
{
    unsigned long long i;

    for (i = 0ull; i < count; ++i) {
        int lane = (int)(((i + 3ull) * 13ull + token * 19ull + salt) % 89ull) -
                   44;
        values[i] = (float)lane / 64.0f;
    }
}

static void init_rolling_view(
    yvex_attention_rolling_state_view *state,
    yvex_attention_rolling_kind kind,
    const yvex_attention_layer_plan *layer,
    unsigned long long next_token_position,
    float *kv,
    float *score)
{
    unsigned long long ratio = layer->compression_ratio;
    int overlap = kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER ||
                  layer->attention_class == YVEX_ATTENTION_CLASS_CSA;
    unsigned long long head_dim =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? layer->indexer_head_dimension
            : layer->head_dimension;
    unsigned long long width = head_dim * (overlap ? 2ull : 1ull);
    unsigned long long slots = ratio * (overlap ? 2ull : 1ull);
    unsigned long long i;

    for (i = 0ull; i < slots * width; ++i) {
        kv[i] = 0.0f;
        score[i] = -INFINITY;
    }
    memset(state, 0, sizeof(*state));
    state->present = 1;
    state->schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1;
    state->kind = kind;
    state->attention_class = layer->attention_class;
    state->layer_index = layer->layer_index;
    state->next_token_position = next_token_position;
    state->ratio = ratio;
    state->head_dimension = head_dim;
    state->state_width = width;
    state->state_slots = slots;
    state->cursor = next_token_position % ratio;
    state->current_fill = state->cursor;
    state->previous_fill = overlap && next_token_position >= ratio
        ? ratio : 0ull;
    state->kv_state_stride = width;
    state->score_state_stride = width;
    state->kv_state_extent = slots * width;
    state->score_state_extent = slots * width;
    state->kv_state = kv;
    state->score_state = score;
    state->overlap = overlap;
    state->rotated = kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER;
    for (i = 0ull; i < state->previous_fill; ++i) {
        unsigned long long lane;
        for (lane = 0ull; lane < head_dim; ++lane) {
            kv[i * width + lane] = 0.0f;
            score[i * width + lane] = 0.0f;
        }
    }
    for (i = 0ull; i < state->current_fill; ++i) {
        unsigned long long lane;
        unsigned long long base = overlap ? ratio + i : i;
        unsigned long long offset = overlap ? head_dim : 0ull;
        for (lane = 0ull; lane < head_dim; ++lane) {
            kv[base * width + offset + lane] = 0.0f;
            score[base * width + offset + lane] = 0.0f;
        }
    }
}

static void init_rolling_output(
    yvex_attention_rolling_state_output *state,
    const yvex_attention_rolling_state_view *view,
    float *kv,
    float *score)
{
    memset(state, 0, sizeof(*state));
    state->kv_state = kv;
    state->score_state = score;
    state->kv_state_stride = view->kv_state_stride;
    state->score_state_stride = view->score_state_stride;
    state->kv_state_extent = view->kv_state_extent;
    state->score_state_extent = view->score_state_extent;
}

static void output_to_view(
    yvex_attention_rolling_state_view *view,
    const yvex_attention_rolling_state_output *out)
{
    memset(view, 0, sizeof(*view));
    view->present = out->present;
    view->schema_version = out->schema_version;
    view->kind = out->kind;
    view->attention_class = out->attention_class;
    view->layer_index = out->layer_index;
    view->next_token_position = out->next_token_position;
    view->ratio = out->ratio;
    view->head_dimension = out->head_dimension;
    view->state_width = out->state_width;
    view->state_slots = out->state_slots;
    view->previous_fill = out->previous_fill;
    view->current_fill = out->current_fill;
    view->cursor = out->cursor;
    view->kv_state_stride = out->kv_state_stride;
    view->score_state_stride = out->score_state_stride;
    view->kv_state_extent = out->kv_state_extent;
    view->score_state_extent = out->score_state_extent;
    view->kv_state = out->kv_state;
    view->score_state = out->score_state;
    view->overlap = out->overlap;
    view->rotated = out->rotated;
    memcpy(view->attention_plan_identity, out->attention_plan_identity,
           sizeof(view->attention_plan_identity));
}

static void output_to_committed_view(
    yvex_attention_rolling_state_view *view,
    const yvex_attention_rolling_state_output *out,
    const yvex_attention_component_span *kv,
    const yvex_attention_component_span *score)
{
    output_to_view(view, out);
    view->kv_state = (const float *)kv->data;
    view->score_state = (const float *)score->data;
}

static int seal_zero_component(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_component_span span;

    if (yvex_attention_state_transaction_acquire(
            transaction, kind, &span, failure, err) != YVEX_OK)
        return 0;
    if (yvex_attention_state_transaction_seal(
            transaction, kind, span.expected_elements, failure, err) != YVEX_OK)
        return 0;
    return 1;
}

static void memory_sink_options_reset(
    yvex_attention_memory_sink_options *options)
{
    memset(options, 0, sizeof(*options));
    options->fail_acquire_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
    options->fail_seal_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
}

static int compare_float_ranges(const float *left,
                                const float *right,
                                unsigned long long count,
                                const char *label);

static int test_execution_gate_is_admitted(void)
{
    const char *reason = NULL;

    YVEX_TEST_ASSERT(yvex_attention_execute_supported(&reason) == 1,
                     "attention executor is admitted");
    YVEX_TEST_ASSERT(reason == NULL,
                     "admitted attention has no refusal reason");
    YVEX_TEST_ASSERT_STREQ(
        yvex_test_attention_status_name(
            YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_READY),
        "execution-ready", "execution-ready status name");
    YVEX_TEST_ASSERT_STREQ(
        yvex_test_attention_failure_name(
            YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY),
        "history", "history failure name");
    return 0;
}

/* Purpose: prove exact class geometry and borrowed cancellation refuse before execution. */
static int test_execution_geometry_and_cancellation(void)
{
    yvex_attention_layer_plan swa =
        layer_fixture(0ull, YVEX_ATTENTION_CLASS_SWA, 0ull);
    yvex_attention_layer_plan csa =
        layer_fixture(2ull, YVEX_ATTENTION_CLASS_CSA, 4ull);
    yvex_attention_layer_plan hca =
        layer_fixture(3ull, YVEX_ATTENTION_CLASS_HCA, 128ull);
    cancellation_fixture fixture = {0u, 2u};
    yvex_attention_cancellation cancellation = {
        cancellation_requested, &fixture
    };
    yvex_attention_cancellation malformed = {NULL, NULL};
    yvex_attention_failure failure;
    yvex_error err;

    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(
        yvex_attention_class_geometry_validate(
            &swa, 4ull, 128ull, &failure, &err) == YVEX_OK &&
        yvex_attention_class_geometry_validate(
            &csa, 4ull, 128ull, &failure, &err) == YVEX_OK &&
        yvex_attention_class_geometry_validate(
            &hca, 4ull, 128ull, &failure, &err) == YVEX_OK,
        "SWA, CSA, and ratio-128 HCA geometry admit exactly");
    hca.compression_ratio = 127ull;
    YVEX_TEST_ASSERT(
        yvex_attention_class_geometry_validate(
            &hca, 4ull, 128ull, &failure, &err) == YVEX_ERR_FORMAT &&
        failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
        "HCA ratio other than 128 refuses");
    csa.sparse_topk.k = 0ull;
    YVEX_TEST_ASSERT(
        yvex_attention_class_geometry_validate(
            &csa, 4ull, 128ull, &failure, &err) == YVEX_ERR_FORMAT,
        "CSA without its exact sparse selection contract refuses");
    swa.compressor_required = 1;
    YVEX_TEST_ASSERT(
        yvex_attention_class_geometry_validate(
            &swa, 4ull, 128ull, &failure, &err) == YVEX_ERR_FORMAT,
        "SWA compressor state refuses");
    YVEX_TEST_ASSERT(
        yvex_attention_cancel_check(
            &malformed, 0ull, "malformed cancellation", &failure, &err) ==
            YVEX_ERR_INVALID_ARG,
        "cancellation without predicate refuses");
    YVEX_TEST_ASSERT(
        yvex_attention_cancel_check(
            &cancellation, 0ull, "first safe point", &failure, &err) ==
            YVEX_OK,
        "clear cancellation continues at first safe point");
    YVEX_TEST_ASSERT(
        yvex_attention_cancel_check(
            &cancellation, 0ull, "second safe point", &failure, &err) ==
            YVEX_ERR_CANCELLED &&
        failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED &&
        strcmp(yvex_test_attention_failure_name(failure.code), "cancelled") == 0,
        "requested cancellation returns its typed result");
    YVEX_TEST_ASSERT_STREQ(
        yvex_test_attention_failure_name(YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH),
        "scratch", "scratch budget has a distinct typed result");
    YVEX_TEST_ASSERT_STREQ(
        yvex_test_attention_failure_name(YVEX_DEEPSEEK_ATTENTION_FAILURE_CLEANUP),
        "cleanup", "cleanup failure has a distinct typed result");
    return 0;
}

static int test_plan_requires_committed_inputs(void)
{
    yvex_attention_plan *plan = NULL;
    yvex_attention_failure failure;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_graph_lower_deepseek_v4()->plan_build(
        &plan, NULL, NULL, NULL, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG,
                     "attention plan refuses missing inputs");
    YVEX_TEST_ASSERT(plan == NULL, "refused plan publishes nothing");
    YVEX_TEST_ASSERT(
        failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
        "invalid plan refusal code");
    return 0;
}

static int test_history_contracts(void)
{
    yvex_attention_layer_plan swa =
        layer_fixture(0ull, YVEX_ATTENTION_CLASS_SWA, 0ull);
    yvex_attention_layer_plan csa =
        layer_fixture(2ull, YVEX_ATTENTION_CLASS_CSA, 4ull);
    yvex_attention_layer_plan hca =
        layer_fixture(3ull, YVEX_ATTENTION_CLASS_HCA, 128ull);
    yvex_attention_history_view history;
    yvex_attention_failure failure;
    yvex_error err;
    float local_kv[128ull * 512ull];
    float compressed_kv[3ull * 512ull];
    float indexer_kv[3ull * 128ull];
    unsigned long long local_positions[128ull];
    unsigned long long compressed_positions[3ull] = {0ull, 4ull, 8ull};
    unsigned long long indexer_positions[3ull] = {0ull, 4ull, 8ull};
    float main_state_kv[8ull * 1024ull];
    float main_state_score[8ull * 1024ull];
    float indexer_state_kv[8ull * 256ull];
    float indexer_state_score[8ull * 256ull];
    unsigned long long i;
    int rc;

    memset(local_kv, 0, sizeof(local_kv));
    memset(compressed_kv, 0, sizeof(compressed_kv));
    memset(indexer_kv, 0, sizeof(indexer_kv));
    memset(main_state_kv, 0, sizeof(main_state_kv));
    memset(main_state_score, 0, sizeof(main_state_score));
    memset(indexer_state_kv, 0, sizeof(indexer_state_kv));
    memset(indexer_state_score, 0, sizeof(indexer_state_score));
    for (i = 0ull; i < 128ull; ++i) local_positions[i] = i;
    memset(&history, 0, sizeof(history));
    history.immutable = 1;
    history.local_tail_count = 127ull;
    history.token_count = 128ull;
    history.local_kv = local_kv + 512ull;
    history.local_kv_stride = 512ull;
    history.local_positions = &local_positions[1];
    yvex_error_clear(&err);
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &swa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "SWA accepts immutable local history");

    history.local_kv = NULL;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &swa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT,
                     "history count without raw KV storage refuses");
    history.local_kv = local_kv + 512ull;

    history.local_tail_count = 129ull;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS,
                     "history over sliding window refuses");
    YVEX_TEST_ASSERT(failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                     "history bounds failure code");

    history.local_tail_count = 127ull;
    history.compressed_entry_count = 1ull;
    history.indexer_entry_count = 0ull;
    history.compressed_kv = compressed_kv;
    history.compressed_kv_stride = 512ull;
    history.compressed_positions = compressed_positions;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &swa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT,
                     "SWA refuses compressed history");

    history.compressed_entry_count = 1ull;
    history.indexer_entry_count = 1ull;
    history.indexer_kv = indexer_kv;
    history.indexer_kv_stride = 128ull;
    history.indexer_positions = indexer_positions;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &hca, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT,
                     "HCA refuses CSA indexer history");

    history.compressed_entry_count = 3ull;
    history.indexer_entry_count = 3ull;
    history.token_count = 12ull;
    history.local_tail_count = 12ull;
    history.local_kv = local_kv;
    history.local_positions = local_positions;
    init_rolling_view(&history.main_rolling_state,
                      YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN, &csa, 12ull,
                      main_state_kv, main_state_score);
    init_rolling_view(&history.indexer_rolling_state,
                      YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, &csa, 12ull,
                      indexer_state_kv, indexer_state_score);
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "CSA accepts compressed and indexer history");
    history.local_positions = &local_positions[1];
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT,
                     "history missing the local suffix head refuses");
    history.local_positions = local_positions;
    history.compressed_entry_count = 2ull;
    history.indexer_entry_count = 2ull;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT,
                     "history missing the latest compressed group refuses");
    history.compressed_entry_count = 3ull;
    history.indexer_entry_count = 3ull;
    history.main_rolling_state.previous_fill = 3ull;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "partially filled prior rolling group refuses");
    history.main_rolling_state.previous_fill = 4ull;
    history.main_rolling_state.current_fill = 1ull;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "stale rolling current fill refuses");
    history.main_rolling_state.current_fill = 0ull;
    history.main_rolling_state.cursor = 1ull;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "stale rolling cursor refuses");
    history.main_rolling_state.cursor = 0ull;
    history.main_rolling_state.previous_fill = 0ull;
    history.main_rolling_state.next_token_position = 0ull;
    main_state_score[0] = 0.0f;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT,
                     "active first overlap emission refuses finite inactive history");
    main_state_score[0] = -INFINITY;
    history.main_rolling_state.cursor = 0ull;
    history.main_rolling_state.next_token_position = 1ull;
    rc = yvex_graph_lower_deepseek_v4()->history_validate(
        &csa, &history, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "stale rolling state position refuses");
    return 0;
}

/* Purpose: prove CSA candidate/ranking allocations obey one exact shared
 * scratch budget and unwind their reservations on refusal and success. */
static int test_csa_selection_scratch_budget(void)
{
    yvex_attention_layer_plan layer =
        layer_fixture(2ull, YVEX_ATTENTION_CLASS_CSA, 4ull);
    yvex_attention_history_view history;
    yvex_attention_scratch_budget scratch;
    yvex_attention_failure failure;
    yvex_error err;
    float key[2] = {1.0f, 0.0f};
    float query[2] = {1.0f, 0.0f};
    float weights[1] = {1.0f};
    unsigned long long position = 0ull;
    unsigned long long selected = ULLONG_MAX;
    unsigned long long selected_count = 0ull;
    unsigned long long valid_count = 0ull;
    int rc;

    memset(&history, 0, sizeof(history));
    layer.indexer_heads = 1ull;
    layer.indexer_head_dimension = 2ull;
    layer.sparse_topk.k = 1ull;
    history.indexer_entry_count = 1ull;
    history.indexer_kv = key;
    history.indexer_kv_stride = 2ull;
    history.indexer_positions = &position;
    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 27ull;
    yvex_error_clear(&err);
    rc = yvex_attention_csa_select(
        &layer, &history, NULL, 0ull, 0ull, NULL, query, weights, 4ull,
        &selected, &selected_count, &valid_count, &scratch, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_ERR_BOUNDS &&
            failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH &&
            scratch.live_bytes == 0u && scratch.peak_bytes == 20u &&
            selected_count == 0ull,
        "CSA ranking refuses budget minus one and releases base scratch");
    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 28ull;
    selected = ULLONG_MAX;
    rc = yvex_attention_csa_select(
        &layer, &history, NULL, 0ull, 0ull, NULL, query, weights, 4ull,
        &selected, &selected_count, &valid_count, &scratch, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_OK && selected == 0ull && selected_count == 1ull &&
            valid_count == 1ull && scratch.live_bytes == 0u &&
            scratch.peak_bytes == 28u,
        "CSA ranking admits the exact complete scratch budget");
    return 0;
}

static int test_transactional_memory_sink(void)
{
    yvex_attention_layer_plan swa =
        layer_fixture(0ull, YVEX_ATTENTION_CLASS_SWA, 0ull);
    yvex_attention_history_view history;
    yvex_attention_memory_sink sink;
    yvex_attention_memory_sink sink2;
    yvex_attention_memory_sink_options options;
    yvex_attention_state_transaction transaction;
    yvex_attention_component_span output;
    yvex_attention_component_span raw;
    const yvex_attention_component_span *committed;
    const char *identity;
    char first_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    char second_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    yvex_attention_failure failure;
    yvex_error err;
    cancellation_fixture cancel_fixture = {0u, 1u};
    yvex_attention_cancellation cancellation = {
        cancellation_requested, &cancel_fixture
    };
    float local_kv[512] = {0.0f};
    unsigned long long local_position = 0ull;
    float *copy = NULL;
    unsigned long long i;
    int rc;

    memset(&history, 0, sizeof(history));
    memset(&sink2, 0, sizeof(sink2));
    memset(first_identity, 0, sizeof(first_identity));
    memset(second_identity, 0, sizeof(second_identity));
    history.immutable = 1;
    yvex_error_clear(&err);
    rc = yvex_attention_memory_sink_init(
        &sink, NULL, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "memory sink initializes");
    rc = yvex_attention_state_transaction_begin(
        &sink, &swa, &history, 0ull, 1ull, &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "memory sink transaction begins");
    YVEX_TEST_ASSERT(
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT) == NULL,
        "component is invisible before commit");
    YVEX_TEST_ASSERT(yvex_attention_memory_sink_identity(&sink) ==
                         NULL,
                     "sink identity is invisible before commit");
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        &output, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "attention output span acquired");
    copy = (float *)calloc((size_t)output.expected_elements, sizeof(float));
    YVEX_TEST_ASSERT(copy != NULL, "test output copy allocation");
    for (i = 0ull; i < output.expected_elements; ++i)
        copy[i] = (float)(i % 17ull) / 17.0f;
    YVEX_TEST_ASSERT(
        output.byte_extent == output.expected_elements * sizeof(*copy),
        "attention output span exposes its exact byte extent");
    memcpy(output.data, copy, (size_t)output.byte_extent);
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        output.expected_elements - 1ull, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_ERR_BOUNDS &&
            !transaction.components[
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT].sealed,
        "attention output refuses a short direct write before sealing");
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        output.expected_elements, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "attention output component seals");
    rc = yvex_attention_state_transaction_commit(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "incomplete transaction cannot commit");
    YVEX_TEST_ASSERT(yvex_attention_memory_sink_identity(&sink) ==
                         NULL,
                     "incomplete commit publishes no identity");
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
        &raw, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "raw local KV span acquired");
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
        raw.expected_elements, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "raw local KV component seals");
    rc = yvex_attention_state_transaction_commit(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "complete transaction commits");
    committed = yvex_attention_memory_sink_committed_component(
        &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT);
    YVEX_TEST_ASSERT(committed != NULL && committed->data != NULL,
                     "committed attention output is visible");
    YVEX_TEST_ASSERT(compare_float_ranges(
                         (const float *)committed->data, copy,
                         output.expected_elements, "committed-output"),
                     "committed output matches staged data");
    identity = yvex_attention_memory_sink_identity(&sink);
    YVEX_TEST_ASSERT(identity != NULL,
                     "committed sink has semantic identity");
    strncpy(first_identity, identity, sizeof(first_identity) - 1u);
    YVEX_TEST_ASSERT(
        transaction.components[YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]
                .data == NULL &&
            transaction.components[YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV]
                    .data == NULL,
        "commit releases all transaction staging");

    history.token_count = 1ull;
    history.local_tail_count = 1ull;
    history.local_kv = local_kv;
    history.local_kv_stride = 512ull;
    history.local_positions = &local_position;
    rc = yvex_attention_state_transaction_begin(
        &sink, &swa, &history, 1ull, 1ull, &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "second transaction begins");
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        &output, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "abort transaction output acquired");
    ((float *)output.data)[0] = 99.0f;
    rc = yvex_attention_cancel_check(
        &cancellation, swa.layer_index, "cancel before commit", &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_CANCELLED &&
                         failure.code ==
                             YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED,
                     "transaction observes cancellation before commit");
    rc = yvex_attention_state_transaction_abort(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "aborted transaction succeeds");
    YVEX_TEST_ASSERT(
        transaction.components[YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]
                .data == NULL,
        "abort releases transaction staging");
    committed = yvex_attention_memory_sink_committed_component(
        &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT);
    YVEX_TEST_ASSERT(((const float *)committed->data)[0] == copy[0],
                     "abort preserves previous committed output");
    YVEX_TEST_ASSERT(strcmp(yvex_attention_memory_sink_identity(&sink),
                            first_identity) == 0,
                     "abort preserves previous committed identity");
    yvex_attention_memory_sink_release(&sink);

    memory_sink_options_reset(&options);
    options.fail_acquire_kind =
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV;
    rc = yvex_attention_memory_sink_init(
        &sink, &options, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "failure-injection sink initializes");
    history.token_count = 0ull;
    history.local_tail_count = 0ull;
    history.local_kv = NULL;
    history.local_positions = NULL;
    rc = yvex_attention_state_transaction_begin(
        &sink, &swa, &history, 0ull, 1ull, &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "failure-injection transaction begins");
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
        &raw, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "injected acquire failure is reported");
    rc = yvex_attention_state_transaction_abort(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "transaction can abort after acquire failure");
    yvex_attention_memory_sink_release(&sink);

    memory_sink_options_reset(&options);
    options.fail_commit = 1;
    rc = yvex_attention_memory_sink_init(
        &sink, &options, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "commit-failure sink initializes");
    rc = yvex_attention_state_transaction_begin(
        &sink, &swa, &history, 0ull, 1ull, &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "commit-failure transaction begins");
    YVEX_TEST_ASSERT(
        seal_zero_component(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
            &failure, &err) &&
            seal_zero_component(
                &transaction,
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
                &failure, &err),
        "commit-failure transaction components seal");
    rc = yvex_attention_state_transaction_commit(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE,
                     "injected commit failure is reported");
    YVEX_TEST_ASSERT(
        yvex_attention_memory_sink_committed_component(
            &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT) == NULL,
        "failed commit publishes nothing");
    YVEX_TEST_ASSERT(yvex_attention_memory_sink_identity(&sink) ==
                         NULL,
                     "failed commit publishes no identity");
    rc = yvex_attention_state_transaction_abort(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "commit-failure transaction aborts cleanly");
    YVEX_TEST_ASSERT(
        transaction.components[YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT]
                .data == NULL,
        "commit-failure abort releases staging");
    yvex_attention_memory_sink_release(&sink);

    rc = yvex_attention_memory_sink_init(
        &sink2, NULL, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "determinism sink initializes");
    history.token_count = 0ull;
    rc = yvex_attention_state_transaction_begin(
        &sink2, &swa, &history, 0ull, 1ull, &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "determinism transaction begins");
    rc = yvex_attention_state_transaction_acquire(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        &output, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "determinism output span acquired");
    YVEX_TEST_ASSERT(
        output.byte_extent == output.expected_elements * sizeof(*copy),
        "determinism span preserves its exact byte extent");
    memcpy(output.data, copy, (size_t)output.byte_extent);
    rc = yvex_attention_state_transaction_seal(
        &transaction, YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
        output.expected_elements, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "determinism output component seals");
    YVEX_TEST_ASSERT(
        seal_zero_component(
            &transaction,
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
            &failure, &err),
        "determinism raw component seals");
    rc = yvex_attention_state_transaction_commit(
        &transaction, &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "determinism transaction commits");
    identity = yvex_attention_memory_sink_identity(&sink2);
    YVEX_TEST_ASSERT(identity != NULL,
                     "determinism sink has committed identity");
    strncpy(second_identity, identity, sizeof(second_identity) - 1u);
    YVEX_TEST_ASSERT(strcmp(first_identity, second_identity) == 0,
                     "same committed semantic content has stable identity");
    yvex_attention_memory_sink_release(&sink2);
    free(copy);
    return 0;
}

static int compare_float_ranges(const float *left,
                                const float *right,
                                unsigned long long count,
                                const char *label)
{
    unsigned long long i;

    for (i = 0ull; i < count; ++i) {
        double diff = fabs((double)left[i] - (double)right[i]);
        if (diff > 1.0e-6) {
            (void)label;
            return 0;
        }
    }
    return 1;
}

static int run_rolling_sequence(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    const unsigned long long *partition,
    unsigned long long partition_count,
    unsigned long long token_count,
    float *final_kv,
    float *final_score,
    float *emissions,
    unsigned long long *emission_count)
{
    static float state_a_kv[128ull * 1024ull];
    static float state_a_score[128ull * 1024ull];
    static float index_a_kv[8ull * 256ull];
    static float index_a_score[8ull * 256ull];
    static float history_local_kv[127ull * 512ull];
    static float history_compressed_kv[32ull * 512ull];
    static float history_indexer_kv[32ull * 128ull];
    unsigned long long history_local_positions[127ull];
    unsigned long long history_compressed_positions[32ull];
    float token_kv[1024ull];
    float token_score[1024ull];
    float ape_row[1024ull];
    float index_token_kv[256ull];
    float index_token_score[256ull];
    float index_ape_row[256ull];
    yvex_attention_rolling_state_view before;
    yvex_attention_rolling_state_view index_before;
    yvex_attention_rolling_state_output after;
    yvex_attention_rolling_state_output index_after;
    yvex_attention_memory_sink sink;
    yvex_attention_state_transaction transaction;
    yvex_attention_failure failure;
    yvex_error err;
    unsigned long long head_dim =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? layer->indexer_head_dimension
            : layer->head_dimension;
    unsigned long long width =
        head_dim * ((kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER ||
                     layer->attention_class == YVEX_ATTENTION_CLASS_CSA)
                        ? 2ull
                        : 1ull);
    unsigned long long slots =
        layer->compression_ratio *
        ((kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER ||
          layer->attention_class == YVEX_ATTENTION_CLASS_CSA)
             ? 2ull
             : 1ull);
    unsigned long long token = 0ull;
    unsigned long long part;
    int result = 0;

    *emission_count = 0ull;
    memset(final_kv, 0, sizeof(float) * slots * width);
    memset(final_score, 0, sizeof(float) * slots * width);
    memset(emissions, 0, sizeof(float) * token_count * head_dim);
    memset(&transaction, 0, sizeof(transaction));
    init_rolling_view(&before, kind, layer, 0ull, state_a_kv, state_a_score);
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA)
        init_rolling_view(&index_before,
                          YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, layer,
                          0ull, index_a_kv, index_a_score);
    if (yvex_attention_memory_sink_init(
            &sink, NULL, &failure, &err) != YVEX_OK)
        return 0;
    for (part = 0ull; part < partition_count && token < token_count; ++part) {
        unsigned long long limit = token + partition[part];
        if (limit > token_count) limit = token_count;
        while (token < limit) {
            yvex_attention_history_view history;
            yvex_attention_component_span main_kv_span;
            yvex_attention_component_span main_score_span;
            yvex_attention_component_span compressed_span;
            yvex_attention_component_span index_kv_span;
            yvex_attention_component_span index_score_span;
            yvex_attention_component_span index_emit_span;
            const yvex_attention_component_span *committed_kv;
            const yvex_attention_component_span *committed_score;
            int emitted = 0;
            int index_emitted = 0;

            fill_vector(token_kv, width, token, 11ull);
            fill_scores(token_score, width, token, 23ull);
            fill_scores(ape_row, width, token % layer->compression_ratio, 41ull);
            memset(&history, 0, sizeof(history));
            history.immutable = 1;
            history.token_count = token;
            history.local_tail_count = token < 127ull ? token : 127ull;
            history.local_kv = history_local_kv;
            history.local_kv_stride = layer->head_dimension;
            history.local_positions = history_local_positions;
            {
                unsigned long long position;
                unsigned long long start =
                    token - history.local_tail_count;
                for (position = 0ull;
                     position < history.local_tail_count; ++position)
                    history_local_positions[position] = start + position;
            }
            history.compressed_entry_count =
                token / layer->compression_ratio;
            history.compressed_kv = history_compressed_kv;
            history.compressed_kv_stride = layer->head_dimension;
            history.compressed_positions = history_compressed_positions;
            {
                unsigned long long group;
                for (group = 0ull;
                     group < history.compressed_entry_count; ++group)
                    history_compressed_positions[group] =
                        group * layer->compression_ratio;
            }
            if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
                history.indexer_entry_count =
                    history.compressed_entry_count;
                history.indexer_kv = history_indexer_kv;
                history.indexer_kv_stride = layer->indexer_head_dimension;
                history.indexer_positions = history_compressed_positions;
            }
            history.main_rolling_state = before;
            if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA)
                history.indexer_rolling_state = index_before;
            yvex_error_clear(&err);
            if (yvex_attention_state_transaction_begin(
                    &sink, layer, &history, token, 1ull, &transaction,
                    &failure, &err) != YVEX_OK)
                goto cleanup;
            if (!seal_zero_component(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT,
                    &failure, &err) ||
                !seal_zero_component(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
                    &failure, &err))
                goto cleanup;
            if (yvex_attention_state_transaction_acquire(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
                    &main_kv_span, &failure, &err) != YVEX_OK ||
                yvex_attention_state_transaction_acquire(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
                    &main_score_span, &failure, &err) != YVEX_OK)
                goto cleanup;
            init_rolling_output(&after, &before,
                                (float *)main_kv_span.data,
                                (float *)main_score_span.data);
            memset(&compressed_span, 0, sizeof(compressed_span));
            if (((token + 1ull) % layer->compression_ratio) == 0ull &&
                yvex_attention_state_transaction_acquire(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
                    &compressed_span, &failure, &err) != YVEX_OK)
                goto cleanup;
            if (yvex_graph_lower_deepseek_v4()->rolling_state_step_cpu(
                    layer, &before, token_kv, token_score, ape_row, &after,
                    compressed_span.data ? (float *)compressed_span.data
                                         : token_kv,
                    head_dim, &emitted, &failure, &err) != YVEX_OK)
                goto cleanup;
            if (emitted) {
                if (yvex_attention_state_transaction_seal(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
                        compressed_span.expected_elements, &failure,
                        &err) != YVEX_OK)
                    goto cleanup;
            }
            if (yvex_attention_state_transaction_seal(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
                    main_kv_span.expected_elements, &failure, &err) !=
                    YVEX_OK ||
                yvex_attention_state_transaction_seal(
                    &transaction,
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
                    main_score_span.expected_elements, &failure, &err) !=
                    YVEX_OK)
                goto cleanup;
            if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
                fill_vector(index_token_kv, index_before.state_width, token,
                            71ull);
                fill_scores(index_token_score, index_before.state_width, token,
                            83ull);
                fill_scores(index_ape_row, index_before.state_width,
                            token % layer->compression_ratio, 97ull);
                if (yvex_attention_state_transaction_acquire(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
                        &index_kv_span, &failure, &err) != YVEX_OK ||
                    yvex_attention_state_transaction_acquire(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
                        &index_score_span, &failure, &err) != YVEX_OK)
                    goto cleanup;
                init_rolling_output(&index_after, &index_before,
                                    (float *)index_kv_span.data,
                                    (float *)index_score_span.data);
                memset(&index_emit_span, 0, sizeof(index_emit_span));
                if (emitted &&
                    yvex_attention_state_transaction_acquire(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                        &index_emit_span, &failure, &err) != YVEX_OK)
                    goto cleanup;
                if (yvex_graph_lower_deepseek_v4()->rolling_state_step_cpu(
                        layer, &index_before, index_token_kv,
                        index_token_score, index_ape_row, &index_after,
                        index_emit_span.data ? (float *)index_emit_span.data
                                             : index_token_kv,
                        layer->indexer_head_dimension, &index_emitted,
                        &failure, &err) != YVEX_OK)
                    goto cleanup;
                if (index_emitted &&
                    yvex_attention_state_transaction_seal(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
                        index_emit_span.expected_elements, &failure,
                        &err) != YVEX_OK)
                    goto cleanup;
                if (yvex_attention_state_transaction_seal(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
                        index_kv_span.expected_elements, &failure, &err) !=
                        YVEX_OK ||
                    yvex_attention_state_transaction_seal(
                        &transaction,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
                        index_score_span.expected_elements, &failure, &err) !=
                        YVEX_OK)
                    goto cleanup;
            }
            if (yvex_attention_state_transaction_commit(
                    &transaction, &failure, &err) != YVEX_OK)
                goto cleanup;
            committed_kv =
                yvex_attention_memory_sink_committed_component(
                    &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE);
            committed_score =
                yvex_attention_memory_sink_committed_component(
                    &sink, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE);
            if (!committed_kv || !committed_score) goto cleanup;
            if (emitted) {
                const yvex_attention_component_span *committed_emit =
                    yvex_attention_memory_sink_committed_component(
                        &sink,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV);
                if (!committed_emit) goto cleanup;
                memcpy(emissions + (*emission_count * head_dim),
                       committed_emit->data,
                       (size_t)(head_dim * sizeof(float)));
                (*emission_count)++;
            }
            output_to_committed_view(&before, &after, committed_kv,
                                     committed_score);
            if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
                const yvex_attention_component_span *ikv =
                    yvex_attention_memory_sink_committed_component(
                        &sink,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE);
                const yvex_attention_component_span *iscore =
                    yvex_attention_memory_sink_committed_component(
                        &sink,
                        YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE);
                if (!ikv || !iscore) goto cleanup;
                output_to_committed_view(&index_before, &index_after, ikv,
                                         iscore);
            }
            token++;
        }
    }
    if (token != token_count)
        goto cleanup;
    memcpy(final_kv, before.kv_state, (size_t)(slots * width * sizeof(float)));
    memcpy(final_score, before.score_state,
           (size_t)(slots * width * sizeof(float)));
    result = 1;

cleanup:
    if (!result && transaction.status ==
                       YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN)
        (void)yvex_attention_state_transaction_abort(
            &transaction, &failure, &err);
    yvex_attention_memory_sink_release(&sink);
    return result;
}

static int test_rolling_state_chunk_invariance(void)
{
    yvex_attention_layer_plan csa =
        layer_fixture(2ull, YVEX_ATTENTION_CLASS_CSA, 4ull);
    yvex_attention_layer_plan hca =
        layer_fixture(3ull, YVEX_ATTENTION_CLASS_HCA, 128ull);
    unsigned long long one_chunk[] = {10ull};
    unsigned long long irregular[] = {1ull, 3ull, 2ull, 4ull};
    unsigned long long hca_one[] = {130ull};
    unsigned long long hca_irregular[] = {17ull, 110ull, 1ull, 2ull};
    static float a_kv[128ull * 1024ull];
    static float a_score[128ull * 1024ull];
    static float b_kv[128ull * 1024ull];
    static float b_score[128ull * 1024ull];
    static float a_emit[130ull * 512ull];
    static float b_emit[130ull * 512ull];
    unsigned long long a_count;
    unsigned long long b_count;

    YVEX_TEST_ASSERT(
        run_rolling_sequence(&csa, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
                             one_chunk, 1ull, 10ull, a_kv, a_score, a_emit,
                             &a_count),
        "CSA rolling state one-chunk execution succeeds");
    YVEX_TEST_ASSERT(
        run_rolling_sequence(&csa, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
                             irregular, 4ull, 10ull, b_kv, b_score, b_emit,
                             &b_count),
        "CSA rolling state irregular execution succeeds");
    YVEX_TEST_ASSERT(a_count == b_count && a_count == 2ull,
                     "CSA emits the same compressed group count");
    YVEX_TEST_ASSERT(compare_float_ranges(a_emit, b_emit, a_count * 512ull,
                                          "csa-emissions"),
                     "CSA compressed emissions are chunk invariant");
    YVEX_TEST_ASSERT(compare_float_ranges(a_kv, b_kv, 8ull * 1024ull,
                                          "csa-kv-state") &&
                         compare_float_ranges(a_score, b_score,
                                              8ull * 1024ull,
                                              "csa-score-state"),
                     "CSA final rolling state is chunk invariant");

    YVEX_TEST_ASSERT(
        run_rolling_sequence(&hca, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
                             hca_one, 1ull, 130ull, a_kv, a_score, a_emit,
                             &a_count),
        "HCA rolling state one-chunk execution succeeds");
    YVEX_TEST_ASSERT(
        run_rolling_sequence(&hca, YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
                             hca_irregular, 4ull, 130ull, b_kv, b_score,
                             b_emit, &b_count),
        "HCA rolling state irregular execution succeeds");
    YVEX_TEST_ASSERT(a_count == b_count && a_count == 1ull,
                     "HCA emits the same compressed group count");
    YVEX_TEST_ASSERT(compare_float_ranges(a_emit, b_emit, a_count * 512ull,
                                          "hca-emissions"),
                     "HCA compressed emissions are chunk invariant");
    YVEX_TEST_ASSERT(compare_float_ranges(a_kv, b_kv, 128ull * 512ull,
                                          "hca-kv-state") &&
                         compare_float_ranges(a_score, b_score,
                                              128ull * 512ull,
                                              "hca-score-state"),
                     "HCA final rolling state is chunk invariant");
    return 0;
}

static int test_runtime_hadamard_policy(void)
{
    const float input[] = {1.0f, -2.0f, 0.5f};
    float reference[3];
    float production[3];
    yvex_attention_failure failure;
    yvex_attention_scratch_budget scratch;
    yvex_error err;
    unsigned long long i;

    yvex_error_clear(&err);
    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 16ull;
    YVEX_TEST_ASSERT(
        yvex_test_attention_reference_hadamard(
            input, 3ull, 0.5f, 1, reference),
        "Hadamard reference accepts finite non-power-of-two input");
    YVEX_TEST_ASSERT(
        yvex_attention_hadamard_cpu(
            input, 3ull, 0.5f, 1, production, &scratch, &failure, &err) ==
            YVEX_OK,
        "Hadamard CPU accepts finite non-power-of-two input");
    YVEX_TEST_ASSERT(scratch.live_bytes == 0u && scratch.peak_bytes == 16u,
                     "Hadamard CPU accounts padded scratch at exact limit");
    for (i = 0ull; i < 3ull; ++i) {
        char label[32];
        snprintf(label, sizeof(label), "hadamard-lane-%llu", i);
        YVEX_TEST_ASSERT(fabs((double)reference[i] -
                              (double)production[i]) < 1.0e-6,
                         label);
    }
    YVEX_TEST_ASSERT(fabs((double)production[0] - -0.25) < 1.0e-6 &&
                     fabs((double)production[1] - 1.75) < 1.0e-6 &&
                     fabs((double)production[2] - -0.75) < 1.0e-6,
                     "Hadamard CPU follows padded Dao FHT row contract");

    production[0] = 0.0f;
    {
        const float bad[] = {1.0f, NAN};
        YVEX_TEST_ASSERT(
            yvex_attention_hadamard_cpu(
                bad, 2ull, 1.0f, 1, production, &scratch, &failure,
                &err) != YVEX_OK,
            "Hadamard CPU refuses non-finite input");
        YVEX_TEST_ASSERT(production[0] == 0.0f,
                         "Hadamard failure does not publish output");
    }
    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 15ull;
    YVEX_TEST_ASSERT(
        yvex_attention_hadamard_cpu(
            input, 3ull, 0.5f, 1, production, &scratch, &failure, &err) ==
            YVEX_ERR_BOUNDS &&
            failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH &&
            scratch.live_bytes == 0u && scratch.peak_bytes == 0u,
        "Hadamard CPU refuses budget minus one before allocation");
    return 0;
}

static int test_runtime_activation_scratch_budget(void)
{
    yvex_attention_activation_policy policy;
    yvex_attention_scratch_budget scratch;
    yvex_attention_failure failure;
    yvex_error err;
    float values[] = {1.0f, -2.0f, 0.5f, 3.0f};
    float original[4];
    int rc;

    memset(&policy, 0, sizeof(policy));
    policy.required = 1;
    policy.stage = YVEX_ATTENTION_ACTIVATION_COMPRESSOR_ROTATED;
    policy.quantization =
        YVEX_ATTENTION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT;
    policy.block_axis = YVEX_ATTENTION_AXIS_FINAL_DIMENSION;
    policy.block_width = 2ull;
    policy.scale_format = YVEX_ATTENTION_SCALE_UE8M0;
    policy.scale_dtype = YVEX_NATIVE_DTYPE_F8_E8M0;
    policy.pre_transform =
        YVEX_ATTENTION_TRANSFORM_DAO_FHT_V1_1_0_POST2;
    policy.tail_policy = YVEX_ATTENTION_TAIL_EXACT_OR_SHORT_FINAL_BLOCK;
    policy.nonfinite_policy = YVEX_ATTENTION_NONFINITE_REFUSE;
    policy.fake_quant_inplace = 1;
    policy.zero_pad_hadamard_to_power_of_two = 1;
    memcpy(original, values, sizeof(values));

    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 31ull;
    rc = yvex_attention_activation_apply(
        &policy, values, 4ull, 0ull,
        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, &scratch, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_ERR_BOUNDS &&
            failure.code == YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH &&
            scratch.live_bytes == 0u && scratch.peak_bytes == 16u &&
            memcmp(values, original, sizeof(values)) == 0,
        "activation fake quant refuses budget minus one without publication");

    memset(&scratch, 0, sizeof(scratch));
    scratch.limit_bytes = 32ull;
    rc = yvex_attention_activation_apply(
        &policy, values, 4ull, 0ull,
        YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV, &scratch, &failure, &err);
    YVEX_TEST_ASSERT(
        rc == YVEX_OK && scratch.live_bytes == 0u &&
            scratch.peak_bytes == 32u,
        "activation fake quant accounts nested Hadamard at exact limit");
    return 0;
}

static int test_runtime_topk_policy(void)
{
    const float scores[] = {1.0f, 2.0f, 2.0f, -0.0f, 0.0f, 3.0f};
    const unsigned long long ordinals[] = {9ull, 4ull, 2ull, 7ull, 6ull, 5ull};
    unsigned long long selected[6];
    unsigned long long selected_count = 0ull;
    yvex_attention_failure failure;
    yvex_error err;

    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(
        yvex_attention_topk_select(
            scores, ordinals, 6ull, 4ull, selected, &selected_count,
            &failure, &err) == YVEX_OK,
        "top-k deterministic selection succeeds");
    YVEX_TEST_ASSERT(selected_count == 4ull &&
                     selected[0] == 5ull &&
                     selected[1] == 2ull &&
                     selected[2] == 1ull &&
                     selected[3] == 0ull,
                     "top-k uses score desc then ordinal asc");
    selected_count = 99ull;
    YVEX_TEST_ASSERT(
        yvex_attention_topk_select(
            scores, ordinals, 6ull, 0ull, selected, &selected_count,
            &failure, &err) == YVEX_OK && selected_count == 0ull,
        "top-k zero selects no candidate");
    YVEX_TEST_ASSERT(
        yvex_attention_topk_select(
            scores, ordinals, 6ull, 9ull, selected, &selected_count,
            &failure, &err) == YVEX_OK && selected_count == 6ull,
        "top-k above the available count selects every candidate");

    {
        const float zero_scores[] = {-0.0f, 0.0f};
        const unsigned long long zero_ordinals[] = {20ull, 10ull};
        selected_count = 0ull;
        YVEX_TEST_ASSERT(
            yvex_attention_topk_select(
                zero_scores, zero_ordinals, 2ull, 2ull, selected,
                &selected_count, &failure, &err) == YVEX_OK,
            "top-k accepts signed zeros");
        YVEX_TEST_ASSERT(selected_count == 2ull &&
                         selected[0] == 1ull &&
                         selected[1] == 0ull,
                         "top-k treats signed zeros equal and orders ordinal");
    }
    {
        const float bad_scores[] = {1.0f, INFINITY};
        const unsigned long long bad_ordinals[] = {1ull, 2ull};
        YVEX_TEST_ASSERT(
            yvex_attention_topk_select(
                bad_scores, bad_ordinals, 2ull, 1ull, selected,
                &selected_count, &failure, &err) != YVEX_OK,
            "top-k refuses non-finite scores");
    }
    {
        const float dup_scores[] = {1.0f, 0.5f};
        const unsigned long long dup_ordinals[] = {3ull, 3ull};
        YVEX_TEST_ASSERT(
            yvex_attention_topk_select(
                dup_scores, dup_ordinals, 2ull, 1ull, selected,
                &selected_count, &failure, &err) != YVEX_OK,
            "top-k refuses duplicate ordinals");
    }
    return 0;
}

static int test_runtime_fp4_fake_quant_policy(void)
{
    const float input[] = {0.0f, 0.49f, 1.01f, -1.51f, 3.99f, -8.0f};
    float dequant[6];
    unsigned char codes[6];
    unsigned char scale_code = 0u;
    yvex_attention_failure failure;
    yvex_error err;

    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(
        yvex_attention_fp4_e2m1_encode(0.0f) == 0u &&
        yvex_attention_fp4_e2m1_encode(0.6f) == 1u &&
        yvex_attention_fp4_e2m1_encode(1.75f) == 4u &&
        yvex_attention_fp4_e2m1_encode(-6.5f) == 15u,
        "FP4 E2M1 uses the pinned codebook and ties-to-even");
    YVEX_TEST_ASSERT(
        yvex_attention_fp4_e2m1_decode(15u) == -6.0f,
        "FP4 E2M1 decode table exposes signed max value");
    YVEX_TEST_ASSERT(
        yvex_attention_fp4_fake_quant_block(
            input, 6ull, dequant, codes, &scale_code, &failure, &err) ==
            YVEX_OK,
        "FP4 fake quant block succeeds for finite input");
    YVEX_TEST_ASSERT(scale_code != 0u, "FP4 fake quant publishes scale");
    YVEX_TEST_ASSERT(codes[0] == 0u && dequant[0] == 0.0f,
                     "FP4 fake quant preserves exact zero");
    YVEX_TEST_ASSERT(
        dequant[1] == yvex_quant_bf16_decode(yvex_quant_bf16_encode(
                          dequant[1])),
        "FP4 fake quant publishes BF16-rounded activation values");
    YVEX_TEST_ASSERT(isfinite(dequant[5]) && dequant[5] < 0.0f,
                     "FP4 fake quant dequantizes signed values");
    {
        const float zeros[] = {0.0f, -0.0f};
        float zero_out[2] = {1.0f, 1.0f};
        unsigned char zero_codes[2] = {7u, 7u};
        scale_code = 9u;
        YVEX_TEST_ASSERT(
            yvex_attention_fp4_fake_quant_block(
                zeros, 2ull, zero_out, zero_codes, &scale_code, &failure,
                &err) == YVEX_OK,
            "FP4 fake quant accepts all-zero block");
        YVEX_TEST_ASSERT(scale_code != 0u && zero_codes[0] == 0u &&
                         zero_codes[1] == 8u && zero_out[0] == 0.0f &&
                         zero_out[1] == 0.0f && signbit(zero_out[1]),
                         "FP4 fake quant preserves signed zero at kernel min scale");
    }
    {
        const float bad[] = {0.0f, NAN};
        YVEX_TEST_ASSERT(
            yvex_attention_fp4_fake_quant_block(
                bad, 2ull, dequant, codes, &scale_code, &failure, &err) !=
                YVEX_OK,
            "FP4 fake quant refuses non-finite input");
    }
    return 0;
}

static int test_runtime_fp8_fake_quant_policy(void)
{
    const float input[] = {0.0f, 1.0f, -2.0f, 448.0f, -512.0f};
    float dequant[5];
    unsigned char codes[5];
    unsigned char scale_code = 0u;
    yvex_attention_failure failure;
    yvex_error err;

    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(
        yvex_attention_fp8_e4m3fn_decode(
            yvex_attention_fp8_e4m3fn_encode(448.0f)) == 448.0f,
        "FP8 E4M3FN exposes finite max 448");
    YVEX_TEST_ASSERT(
        yvex_attention_fp8_fake_quant_block(
            input, 5ull, dequant, codes, &scale_code, &failure, &err) ==
            YVEX_OK,
        "FP8 fake quant block succeeds for finite input");
    YVEX_TEST_ASSERT(scale_code != 0u, "FP8 fake quant publishes UE8M0 scale");
    YVEX_TEST_ASSERT(codes[0] == 0u && dequant[0] == 0.0f,
                     "FP8 fake quant preserves exact zero");
    YVEX_TEST_ASSERT(
        dequant[1] == yvex_quant_bf16_decode(yvex_quant_bf16_encode(
                          dequant[1])),
        "FP8 fake quant publishes BF16-rounded activation values");
    YVEX_TEST_ASSERT(isfinite(dequant[4]) && dequant[4] < 0.0f,
                     "FP8 fake quant clamps and dequantizes signed values");
    {
        const float zeros[] = {0.0f, -0.0f};
        float zero_out[2] = {1.0f, 1.0f};
        unsigned char zero_codes[2] = {9u, 9u};
        scale_code = 0u;
        YVEX_TEST_ASSERT(
            yvex_attention_fp8_fake_quant_block(
                zeros, 2ull, zero_out, zero_codes, &scale_code, &failure,
                &err) == YVEX_OK,
            "FP8 fake quant accepts all-zero block");
        YVEX_TEST_ASSERT(scale_code != 0u && zero_codes[0] == 0u &&
                         zero_codes[1] == 0x80u && zero_out[0] == 0.0f &&
                         zero_out[1] == 0.0f && signbit(zero_out[1]),
                         "FP8 fake quant preserves signed zero at kernel min scale");
    }
    {
        const float bad[] = {0.0f, INFINITY};
        YVEX_TEST_ASSERT(
            yvex_attention_fp8_fake_quant_block(
                bad, 2ull, dequant, codes, &scale_code, &failure, &err) !=
                YVEX_OK,
            "FP8 fake quant refuses non-finite input");
    }
    return 0;
}

/* Purpose: prove production and independent-oracle scalar codec edge contracts. */
static int test_runtime_activation_codec_edges(void)
{
    unsigned int code;

    YVEX_TEST_ASSERT(yvex_quant_e8m0_decode(0u) == ldexpf(1.0f, -127) &&
                         isnan(yvex_quant_e8m0_decode(0xffu)),
                     "UE8M0 code zero is minimum finite and 255 is NaN");
    for (code = 0u; code <= 0xffu; ++code) {
        float scale = yvex_quant_e8m0_decode((unsigned char)code);
        if (code == 0xffu)
            YVEX_TEST_ASSERT(isnan(scale),
                             "UE8M0 special code decodes as NaN");
        else
            YVEX_TEST_ASSERT(isfinite(scale) && scale > 0.0f,
                             "every admitted UE8M0 code is positive finite");
    }
    for (code = 0u; code <= 0xffu; ++code) {
        float value = yvex_attention_fp8_e4m3fn_decode(
            (unsigned char)code);
        if ((code & 0x7fu) == 0x7fu) {
            YVEX_TEST_ASSERT(isnan(value),
                             "E4M3FN special magnitudes decode as NaN");
        } else {
            YVEX_TEST_ASSERT(isfinite(value) &&
                                 yvex_attention_fp8_e4m3fn_encode(value) ==
                                     (unsigned char)code,
                             "every finite E4M3FN code roundtrips exactly");
        }
    }
    for (code = 0u; code < 0x7eu; ++code) {
        float lower = yvex_attention_fp8_e4m3fn_decode(
            (unsigned char)code);
        float upper = yvex_attention_fp8_e4m3fn_decode(
            (unsigned char)(code + 1u));
        float midpoint = (lower + upper) * 0.5f;
        unsigned char expected = (unsigned char)((code & 1u)
            ? code + 1u : code);
        YVEX_TEST_ASSERT(
            yvex_attention_fp8_e4m3fn_encode(midpoint) == expected &&
                yvex_attention_fp8_e4m3fn_encode(-midpoint) ==
                    (unsigned char)(expected | 0x80u),
            "E4M3FN midpoint encoding rounds to the even code");
    }
    YVEX_TEST_ASSERT(
        yvex_attention_fp8_e4m3fn_encode(1.1875f) == 0x3au &&
            yvex_attention_fp8_e4m3fn_encode(-0.0f) == 0x80u &&
            yvex_quant_bf16_decode(yvex_quant_bf16_encode(1.00390625f)) ==
                1.0f &&
            yvex_quant_bf16_decode(yvex_quant_bf16_encode(1.01171875f)) ==
                1.015625f,
        "E4M3FN and BF16 explicit midpoint cases are canonical");
    for (code = 0u; code < 16u; ++code)
        YVEX_TEST_ASSERT(
            yvex_attention_fp4_e2m1_encode(
                yvex_attention_fp4_e2m1_decode((unsigned char)code)) == code,
            "every E2M1 code roundtrips exactly");
    for (code = 0u; code < 7u; ++code) {
        float lower = yvex_attention_fp4_e2m1_decode((unsigned char)code);
        float upper = yvex_attention_fp4_e2m1_decode(
            (unsigned char)(code + 1u));
        float midpoint = (lower + upper) * 0.5f;
        unsigned char expected = (unsigned char)((code & 1u)
            ? code + 1u : code);
        YVEX_TEST_ASSERT(
            yvex_attention_fp4_e2m1_encode(midpoint) == expected &&
                yvex_attention_fp4_e2m1_encode(-midpoint) ==
                    (unsigned char)(expected | 0x8u),
            "E2M1 midpoint encoding rounds to the even code");
    }
    YVEX_TEST_ASSERT(yvex_test_attention_reference_codec_selftest(),
                     "independent oracle proves its own codec edge contract");
    return 0;
}

static int test_independent_reference_detects_stage_mutations(void)
{
    yvex_attention_execution_trace production;
    yvex_attention_execution_trace reference;
    yvex_test_attention_reference_metrics metrics;
    float input[2] = {0.25f, -0.5f};
    float q_low[2] = {0.5f, -0.25f};
    float query[2] = {0.75f, 0.125f};
    float raw_kv[2] = {-0.75f, 0.25f};
    float compressed[2] = {0.375f, -0.625f};
    float indexer[2] = {0.5f, 0.75f};
    float index_query[2] = {-0.125f, 0.875f};
    float index_weights[1] = {0.5f};
    float attention[2] = {0.0625f, -0.1875f};
    float output[2] = {0.03125f, 0.09375f};
    unsigned long long compressed_positions[1] = {4ull};
    unsigned long long indexer_positions[1] = {4ull};
    unsigned long long topk_counts[1] = {1ull};
    unsigned long long topk_positions[1] = {4ull};
    float reference_input[2];
    float reference_q_low[2];
    float reference_query[2];
    float reference_raw_kv[2];
    float reference_compressed[2];
    float reference_indexer[2];
    float reference_index_query[2];
    float reference_index_weights[1];
    float reference_attention[2];
    float reference_output[2];
    unsigned long long reference_compressed_positions[1];
    unsigned long long reference_indexer_positions[1];
    unsigned long long reference_topk_counts[1];
    unsigned long long reference_topk_positions[1];
    float rolling_kv[1] = {0.4375f};
    float rolling_score[1] = {-0.125f};
    float reference_rolling_kv[1] = {0.4375f};
    float reference_rolling_score[1] = {-0.125f};
    float *stages[] = {
        input, q_low, query, raw_kv, compressed, indexer,
        index_query, index_weights, attention, output
    };
    const char *names[] = {
        "input", "q-low", "query", "raw-kv", "compressed-kv",
        "indexer-kv", "index-query", "index-weights", "attention",
        "output"
    };
    unsigned long long i;

    memset(&production, 0, sizeof(production));
    production.owned = 1;
    production.complete = 1;
    production.layer_index = 2ull;
    production.attention_class = YVEX_ATTENTION_CLASS_CSA;
    production.token_position = 8ull;
    production.token_count = 1ull;
    production.hidden_width = 2ull;
    production.q_rank = 2ull;
    production.query_width = 2ull;
    production.kv_width = 2ull;
    production.compressed_count = 1ull;
    production.compressed_stride = 2ull;
    production.indexer_count = 1ull;
    production.indexer_stride = 2ull;
    production.index_query_stride = 2ull;
    production.index_weight_stride = 1ull;
    production.topk_stride = 1ull;
    production.input = input;
    production.q_low = q_low;
    production.query = query;
    production.raw_kv = raw_kv;
    production.compressed_kv = compressed;
    production.indexer_kv = indexer;
    production.index_query = index_query;
    production.index_weights = index_weights;
    production.attention_values = attention;
    production.output = output;
    production.compressed_positions = compressed_positions;
    production.indexer_positions = indexer_positions;
    production.topk_counts = topk_counts;
    production.topk_positions = topk_positions;
    production.next_main_rolling_state.present = 1;
    production.next_main_rolling_state.schema_version =
        YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1;
    production.next_main_rolling_state.kind =
        YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN;
    production.next_main_rolling_state.attention_class =
        YVEX_ATTENTION_CLASS_CSA;
    production.next_main_rolling_state.layer_index = 2ull;
    production.next_main_rolling_state.next_token_position = 9ull;
    production.next_main_rolling_state.ratio = 4ull;
    production.next_main_rolling_state.head_dimension = 1ull;
    production.next_main_rolling_state.state_width = 1ull;
    production.next_main_rolling_state.state_slots = 1ull;
    production.next_main_rolling_state.kv_state_stride = 1ull;
    production.next_main_rolling_state.score_state_stride = 1ull;
    production.next_main_rolling_state.kv_state_extent = 1ull;
    production.next_main_rolling_state.score_state_extent = 1ull;
    production.next_main_rolling_state.kv_state = rolling_kv;
    production.next_main_rolling_state.score_state = rolling_score;
    (void)snprintf(
        production.next_main_rolling_state.attention_plan_identity,
        sizeof(production.next_main_rolling_state.attention_plan_identity),
        "%s", "reference-mutation-fixture");
    reference = production;
    memcpy(reference_input, input, sizeof(input));
    memcpy(reference_q_low, q_low, sizeof(q_low));
    memcpy(reference_query, query, sizeof(query));
    memcpy(reference_raw_kv, raw_kv, sizeof(raw_kv));
    memcpy(reference_compressed, compressed, sizeof(compressed));
    memcpy(reference_indexer, indexer, sizeof(indexer));
    memcpy(reference_index_query, index_query, sizeof(index_query));
    memcpy(reference_index_weights, index_weights, sizeof(index_weights));
    memcpy(reference_attention, attention, sizeof(attention));
    memcpy(reference_output, output, sizeof(output));
    memcpy(reference_compressed_positions, compressed_positions,
           sizeof(compressed_positions));
    memcpy(reference_indexer_positions, indexer_positions,
           sizeof(indexer_positions));
    memcpy(reference_topk_counts, topk_counts, sizeof(topk_counts));
    memcpy(reference_topk_positions, topk_positions, sizeof(topk_positions));
    reference.input = reference_input;
    reference.q_low = reference_q_low;
    reference.query = reference_query;
    reference.raw_kv = reference_raw_kv;
    reference.compressed_kv = reference_compressed;
    reference.indexer_kv = reference_indexer;
    reference.index_query = reference_index_query;
    reference.index_weights = reference_index_weights;
    reference.attention_values = reference_attention;
    reference.output = reference_output;
    reference.compressed_positions = reference_compressed_positions;
    reference.indexer_positions = reference_indexer_positions;
    reference.topk_counts = reference_topk_counts;
    reference.topk_positions = reference_topk_positions;
    reference.next_main_rolling_state.kv_state = reference_rolling_kv;
    reference.next_main_rolling_state.score_state = reference_rolling_score;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 1,
                     "independent reference accepts identical stage facts");
    for (i = 0ull; i < sizeof(stages) / sizeof(stages[0]); ++i) {
        float saved = stages[i][0];
        stages[i][0] = saved + 1.0f;
        YVEX_TEST_ASSERT(
            yvex_test_attention_reference_compare(
                &production, &reference, 0.0, 0.0, &metrics) == 0,
            "independent reference detects numeric stage mutation");
        YVEX_TEST_ASSERT_STREQ(metrics.first_failed_stage, names[i],
                               "reference names mutated stage");
        stages[i][0] = saved;
    }
    compressed_positions[0]++;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 0 &&
                         strcmp(metrics.first_failed_stage,
                                "compressed-positions") == 0,
                     "reference detects compressed-position mutation");
    compressed_positions[0]--;
    indexer_positions[0]++;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 0 &&
                         strcmp(metrics.first_failed_stage,
                                "indexer-positions") == 0,
                     "reference detects indexer-position mutation");
    indexer_positions[0]--;
    topk_positions[0]++;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 0 &&
                         strcmp(metrics.first_failed_stage,
                                "topk-order") == 0,
                     "reference detects sparse selection mutation");
    topk_positions[0]--;
    rolling_kv[0] += 1.0f;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 0 &&
                         strcmp(metrics.first_failed_stage,
                                "main-rolling-state") == 0,
                     "reference detects rolling KV mutation");
    rolling_kv[0] -= 1.0f;
    rolling_score[0] += 1.0f;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 0 &&
                         strcmp(metrics.first_failed_stage,
                                "main-rolling-state") == 0,
                     "reference detects rolling score mutation");
    rolling_score[0] -= 1.0f;
    production.next_main_rolling_state.cursor++;
    YVEX_TEST_ASSERT(yvex_test_attention_reference_compare(
                         &production, &reference, 0.0, 0.0, &metrics) == 0 &&
                         strcmp(metrics.first_failed_stage,
                                "main-rolling-state") == 0,
                     "reference detects rolling geometry mutation");
    return 0;
}

/* Proves the production comparison contract distinguishes numeric admission from object bytes. */
static int test_f32_comparison_contract(void)
{
    const float identical[] = {0.0f, 1.0f, -2.0f};
    const float signed_zero[] = {-0.0f, 1.0f, -2.0f};
    const float near_left[] = {1000.0f};
    const float near_right[] = {1000.4f};
    const float far_right[] = {1001.0f};
    const float nonfinite[] = {NAN, INFINITY};
    const float finite_mixed[] = {NAN, 1.0f};
    const float delayed_left[] = {1.0f, 2.0f, 3.0f};
    const float delayed_right[] = {1.0f, 2.0f, 4.0f};
    const uint32_t nan_bits[] = {UINT32_C(0x7fc00001), UINT32_C(0x7fc00002)};
    float distinct_nan[2];
    yvex_graph_f32_comparison comparison;
    yvex_graph_f32_comparison unchanged;
    yvex_error error;

    memcpy(distinct_nan, nan_bits, sizeof(distinct_nan));
    yvex_error_clear(&error);
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(identical, identical, 3ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_OK &&
                         comparison.within_tolerance && comparison.bitwise_equal &&
                         comparison.finite_value_count == 3ull &&
                         comparison.nonfinite_value_count == 0ull,
                     "identical finite F32 values compare exactly");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(identical, signed_zero, 3ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_OK &&
                         comparison.within_tolerance && !comparison.bitwise_equal,
                     "signed zero is numerically equal but not bitwise equal");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(near_left, near_right, 1ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_OK &&
                         comparison.within_tolerance,
                     "combined absolute-relative tolerance admits its finite bound");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(near_left, far_right, 1ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_OK &&
                         !comparison.within_tolerance &&
                         comparison.first_failing_coordinate == 0ull,
                     "comparison reports the first finite tolerance failure");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(nonfinite, nonfinite, 2ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_OK &&
                         !comparison.within_tolerance && comparison.bitwise_equal &&
                         comparison.finite_value_count == 0ull &&
                         comparison.nonfinite_value_count == comparison.value_count &&
                         comparison.first_failing_coordinate == 0ull &&
                         isfinite(comparison.squared_error_sum),
                     "matching non-finite bytes still fail numeric admission");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(distinct_nan, distinct_nan + 1, 1ull, 5.0e-4,
                                            5.0e-4, &comparison, &error) == YVEX_OK &&
                         !comparison.within_tolerance && !comparison.bitwise_equal &&
                         comparison.nonfinite_value_count == 1ull,
                     "different NaN payloads fail bitwise and numeric admission");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(nonfinite, finite_mixed, 2ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_OK &&
                         !comparison.within_tolerance &&
                         comparison.finite_value_count + comparison.nonfinite_value_count ==
                             comparison.value_count,
                     "mixed finite and non-finite pairs retain exact accounting");
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(delayed_left, delayed_right, 3ull, 0.0, 0.0,
                                            &comparison, &error) == YVEX_OK &&
                         !comparison.within_tolerance &&
                         comparison.first_failing_coordinate == 2ull &&
                         comparison.maximum_absolute_error == 1.0 &&
                         comparison.maximum_relative_error == 0.25 &&
                         comparison.squared_error_sum == 1.0,
                     "finite metrics and first delayed failure are exact");
    memset(&unchanged, 0x5a, sizeof(unchanged));
    comparison = unchanged;
    YVEX_TEST_ASSERT(yvex_graph_f32_compare(NULL, identical, 3ull, 5.0e-4, 5.0e-4,
                                            &comparison, &error) == YVEX_ERR_INVALID_ARG &&
                         memcmp(&comparison, &unchanged, sizeof(comparison)) == 0,
                     "invalid comparison arguments preserve caller output");
    return 0;
}

int yvex_test_deepseek_attention(void)
{
    if (test_execution_gate_is_admitted() != 0) return 1;
    if (test_cpu_resource_guards() != 0) return 1;
    if (test_execution_geometry_and_cancellation() != 0) return 1;
    if (test_plan_requires_committed_inputs() != 0) return 1;
    if (test_history_contracts() != 0) return 1;
    if (test_csa_selection_scratch_budget() != 0) return 1;
    if (test_transactional_memory_sink() != 0) return 1;
    if (test_rolling_state_chunk_invariance() != 0) return 1;
    if (test_runtime_hadamard_policy() != 0) return 1;
    if (test_runtime_activation_scratch_budget() != 0) return 1;
    if (test_runtime_topk_policy() != 0) return 1;
    if (test_runtime_fp4_fake_quant_policy() != 0) return 1;
    if (test_runtime_fp8_fake_quant_policy() != 0) return 1;
    if (test_runtime_activation_codec_edges() != 0) return 1;
    if (test_f32_comparison_contract() != 0) return 1;
    if (test_independent_reference_detects_stage_mutations() != 0) return 1;
    return 0;
}
