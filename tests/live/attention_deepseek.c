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
 *   CUDA execution for all 43 release attention descriptors.
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/api.h>

#define ATTENTION_CPU_ABSOLUTE_TOLERANCE 2.0e-4
#define ATTENTION_CPU_RELATIVE_TOLERANCE 2.0e-4
#define ATTENTION_CUDA_ABSOLUTE_TOLERANCE 5.0e-4
#define ATTENTION_CUDA_RELATIVE_TOLERANCE 5.0e-4

typedef struct {
    yvex_sha256 hash;
    yvex_sha256 oracle_trace_hash;
    yvex_sha256 oracle_output_hash;
    yvex_sha256 fixture_hash;
    yvex_sha256 history_hash;
    yvex_sha256 comparison_contract_hash;
    unsigned long long layer_count;
    unsigned long long swa_count;
    unsigned long long csa_count;
    unsigned long long hca_count;
    unsigned long long qtype_paths;
    unsigned long long cpu_reference_values;
    unsigned long long cuda_reference_values;
    unsigned long long cpu_cuda_values;
    double cpu_squared_error;
    double cuda_squared_error;
    double cpu_cuda_squared_error;
    double cpu_maximum_absolute_error;
    double cpu_maximum_relative_error;
    double cuda_maximum_absolute_error;
    double cuda_maximum_relative_error;
    double cpu_cuda_maximum_absolute_error;
    double cpu_cuda_maximum_relative_error;
    unsigned long long exact_topk_positions;
    char oracle_trace_identity[YVEX_SHA256_HEX_CAP];
    char oracle_output_identity[YVEX_SHA256_HEX_CAP];
    char fixture_identity[YVEX_SHA256_HEX_CAP];
    char history_identity[YVEX_SHA256_HEX_CAP];
    char comparison_contract_identity[YVEX_SHA256_HEX_CAP];
} attention_live_evidence;

static int attention_reference_contract_init(
    yvex_test_attention_reference_contract *contract,
    const yvex_attention_summary *summary,
    const char *path_name,
    double absolute_tolerance,
    double relative_tolerance)
{
    unsigned int stage;

    if (!contract || !summary || !path_name ||
        !isfinite(absolute_tolerance) || !isfinite(relative_tolerance) ||
        absolute_tolerance < 0.0 || relative_tolerance < 0.0)
        return 0;
    memset(contract, 0, sizeof(*contract));
    contract->schema_version = YVEX_TEST_ATTENTION_EVIDENCE_SCHEMA_V2;
    contract->path_name = path_name;
    /* Both paths consume the same decoded qtype values. One bound covers a
     * projection; composed transforms allow additional double-accumulation
     * ordering error before each required F32 publication. */
    for (stage = 0u; stage < YVEX_TEST_ATTENTION_STAGE_COUNT; ++stage) {
        double factor = 1.0;

        if (stage == YVEX_TEST_ATTENTION_STAGE_QUERY ||
            stage == YVEX_TEST_ATTENTION_STAGE_COMPRESSED_KV ||
            stage == YVEX_TEST_ATTENTION_STAGE_INDEXER_KV ||
            stage == YVEX_TEST_ATTENTION_STAGE_INDEX_QUERY ||
            stage == YVEX_TEST_ATTENTION_STAGE_INDEX_WEIGHTS ||
            stage == YVEX_TEST_ATTENTION_STAGE_MAIN_STATE ||
            stage == YVEX_TEST_ATTENTION_STAGE_INDEXER_STATE)
            factor = 2.0;
        else if (stage == YVEX_TEST_ATTENTION_STAGE_ATTENTION)
            factor = 4.0;
        else if (stage == YVEX_TEST_ATTENTION_STAGE_OUTPUT)
            factor = 6.0;
        contract->absolute[stage] = absolute_tolerance * factor;
        contract->relative[stage] = relative_tolerance * factor;
    }
    contract->absolute[YVEX_TEST_ATTENTION_STAGE_INPUT] = 0.0;
    contract->relative[YVEX_TEST_ATTENTION_STAGE_INPUT] = 0.0;
    memcpy(contract->qtype_binding_counts, summary->qtype_binding_counts,
           sizeof(contract->qtype_binding_counts));
    contract->compressed_positions_exact = 1;
    contract->indexer_positions_exact = 1;
    contract->topk_positions_exact = 1;
    return 1;
}

static double reference_metrics_rmse(
    const yvex_test_attention_reference_metrics *metrics)
{
    if (!metrics || !metrics->compared_values) return 0.0;
    return sqrt(metrics->squared_error_sum /
                (double)metrics->compared_values);
}

static int reference_metrics_merge(
    unsigned long long *values,
    double *squared_error,
    double *maximum_absolute_error,
    double *maximum_relative_error,
    const yvex_test_attention_reference_metrics *metrics)
{
    if (!values || !squared_error || !maximum_absolute_error ||
        !maximum_relative_error || !metrics)
        return 0;
    if (!yvex_core_u64_add(
            *values, metrics->compared_values, values) ||
        !isfinite(metrics->squared_error_sum) ||
        !isfinite(*squared_error + metrics->squared_error_sum))
        return 0;
    *squared_error += metrics->squared_error_sum;
    if (metrics->maximum_absolute_error > *maximum_absolute_error)
        *maximum_absolute_error = metrics->maximum_absolute_error;
    if (metrics->maximum_relative_error > *maximum_relative_error)
        *maximum_relative_error = metrics->maximum_relative_error;
    return 1;
}

static int attention_evidence_init(
    attention_live_evidence *evidence,
    const yvex_attention_summary *summary)
{
    unsigned long long binding_total = 0ull;
    unsigned int qtype;

    if (!evidence || !summary) return 0;
    memset(evidence, 0, sizeof(*evidence));
    yvex_sha256_init(&evidence->hash);
    yvex_sha256_init(&evidence->oracle_trace_hash);
    yvex_sha256_init(&evidence->oracle_output_hash);
    yvex_sha256_init(&evidence->fixture_hash);
    yvex_sha256_init(&evidence->history_hash);
    yvex_sha256_init(&evidence->comparison_contract_hash);
    if (!yvex_sha256_update_text(
            &evidence->hash, "yvex.attention.execution.evidence.v2") ||
        !yvex_sha256_update_text(
            &evidence->oracle_trace_hash,
            "yvex.attention.oracle.trace.aggregate.v2") ||
        !yvex_sha256_update_text(
            &evidence->oracle_output_hash,
            "yvex.attention.oracle.output.aggregate.v2") ||
        !yvex_sha256_update_text(
            &evidence->fixture_hash,
            "yvex.attention.fixture.aggregate.v2") ||
        !yvex_sha256_update_text(
            &evidence->history_hash,
            "yvex.attention.history.aggregate.v2") ||
        !yvex_sha256_update_text(
            &evidence->comparison_contract_hash,
            "yvex.attention.comparison.aggregate.v2") ||
        !yvex_sha256_update_text(
            &evidence->hash, summary->attention_plan_identity) ||
        !yvex_sha256_update_text(
            &evidence->hash, summary->runtime_descriptor_identity) ||
        !yvex_sha256_update_text(
            &evidence->hash, summary->runtime_numeric_identity) ||
        !yvex_sha256_update_u64(
            &evidence->hash, summary->required_binding_count) ||
        !yvex_sha256_update_u64(&evidence->hash, summary->layer_count))
        return 0;
    for (qtype = 0u; qtype < YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP; ++qtype) {
        unsigned long long count = summary->qtype_binding_counts[qtype];

        if (!count) continue;
        if (!yvex_core_u64_add(binding_total, count, &binding_total) ||
            !yvex_core_u64_add(
                evidence->qtype_paths, 1ull, &evidence->qtype_paths))
            return 0;
        if (!yvex_sha256_update_u64(&evidence->hash, qtype) ||
            !yvex_sha256_update_u64(&evidence->hash, count))
            return 0;
    }
    return binding_total == summary->required_binding_count;
}

static int attention_evidence_add_case(
    attention_live_evidence *evidence,
    const char *case_name,
    const yvex_attention_cpu_result *cpu,
    const yvex_attention_cpu_result *cuda,
    const yvex_test_attention_reference_metrics *cpu_reference,
    const yvex_test_attention_reference_metrics *cuda_reference,
    const yvex_test_attention_reference_metrics *cpu_cuda,
    const yvex_test_attention_reference_evidence *cpu_oracle,
    const yvex_test_attention_reference_evidence *cuda_oracle)
{
    if (!evidence || !case_name || !cpu || !cuda || !cpu_reference ||
        !cuda_reference || !cpu_cuda || !cpu_oracle || !cuda_oracle ||
        cpu_oracle->schema_version != YVEX_TEST_ATTENTION_EVIDENCE_SCHEMA_V2 ||
        cuda_oracle->schema_version != YVEX_TEST_ATTENTION_EVIDENCE_SCHEMA_V2 ||
        cpu->layer_index != cuda->layer_index ||
        cpu->attention_class != cuda->attention_class ||
        strcmp(cpu_oracle->oracle_trace_identity,
               cuda_oracle->oracle_trace_identity) != 0 ||
        strcmp(cpu_oracle->oracle_output_identity,
               cuda_oracle->oracle_output_identity) != 0 ||
        strcmp(cpu_oracle->fixture_identity,
               cuda_oracle->fixture_identity) != 0 ||
        strcmp(cpu_oracle->history_identity,
               cuda_oracle->history_identity) != 0 ||
        cpu_oracle->exact_topk_positions !=
            cuda_oracle->exact_topk_positions)
        return 0;
    if (!yvex_core_u64_add(
            evidence->exact_topk_positions,
            cpu_oracle->exact_topk_positions,
            &evidence->exact_topk_positions) ||
        !reference_metrics_merge(
        &evidence->cpu_reference_values, &evidence->cpu_squared_error,
        &evidence->cpu_maximum_absolute_error,
        &evidence->cpu_maximum_relative_error, cpu_reference) ||
        !reference_metrics_merge(
        &evidence->cuda_reference_values, &evidence->cuda_squared_error,
        &evidence->cuda_maximum_absolute_error,
        &evidence->cuda_maximum_relative_error, cuda_reference) ||
        !reference_metrics_merge(
        &evidence->cpu_cuda_values, &evidence->cpu_cuda_squared_error,
        &evidence->cpu_cuda_maximum_absolute_error,
        &evidence->cpu_cuda_maximum_relative_error, cpu_cuda))
        return 0;
    return yvex_sha256_update_text(&evidence->hash, case_name) &&
           yvex_sha256_update_u64(&evidence->hash, cpu->layer_index) &&
           yvex_sha256_update_u64(
               &evidence->hash, (unsigned long long)cpu->attention_class) &&
           yvex_sha256_update_text(&evidence->hash, cpu->output_identity) &&
           yvex_sha256_update_text(&evidence->hash, cuda->output_identity) &&
           yvex_sha256_update_text(
               &evidence->hash, cpu_oracle->oracle_trace_identity) &&
           yvex_sha256_update_text(
               &evidence->hash, cpu_oracle->oracle_output_identity) &&
           yvex_sha256_update_text(
               &evidence->hash, cpu_oracle->fixture_identity) &&
           yvex_sha256_update_text(
               &evidence->hash, cpu_oracle->history_identity) &&
           yvex_sha256_update_text(
               &evidence->hash,
               cpu_oracle->comparison_contract_identity) &&
           yvex_sha256_update_text(
               &evidence->hash,
               cuda_oracle->comparison_contract_identity) &&
           yvex_sha256_update_u64(
               &evidence->hash, cpu_oracle->exact_topk_positions) &&
           yvex_sha256_update_u64(
               &evidence->hash, cpu_reference->compared_values) &&
           yvex_sha256_update_u64(
               &evidence->hash, cuda_reference->compared_values) &&
           yvex_sha256_update_u64(
               &evidence->hash, cpu_cuda->compared_values) &&
           yvex_sha256_update_text(
               &evidence->oracle_trace_hash, case_name) &&
           yvex_sha256_update_text(
               &evidence->oracle_trace_hash,
               cpu_oracle->oracle_trace_identity) &&
           yvex_sha256_update_text(
               &evidence->oracle_output_hash, case_name) &&
           yvex_sha256_update_text(
               &evidence->oracle_output_hash,
               cpu_oracle->oracle_output_identity) &&
           yvex_sha256_update_text(&evidence->fixture_hash, case_name) &&
           yvex_sha256_update_text(
               &evidence->fixture_hash, cpu_oracle->fixture_identity) &&
           yvex_sha256_update_text(&evidence->history_hash, case_name) &&
           yvex_sha256_update_text(
               &evidence->history_hash, cpu_oracle->history_identity) &&
           yvex_sha256_update_text(
               &evidence->comparison_contract_hash, case_name) &&
           yvex_sha256_update_text(
               &evidence->comparison_contract_hash,
               cpu_oracle->comparison_contract_identity) &&
           yvex_sha256_update_text(
               &evidence->comparison_contract_hash,
               cuda_oracle->comparison_contract_identity);
}

static int attention_evidence_add(
    attention_live_evidence *evidence,
    const yvex_attention_cpu_result *cpu,
    const yvex_attention_cpu_result *cuda,
    const yvex_test_attention_reference_metrics *cpu_reference,
    const yvex_test_attention_reference_metrics *cuda_reference,
    const yvex_test_attention_reference_metrics *cpu_cuda,
    const yvex_test_attention_reference_evidence *cpu_oracle,
    const yvex_test_attention_reference_evidence *cuda_oracle)
{
    if (!evidence || !cpu) return 0;
    if (!yvex_core_u64_add(
            evidence->layer_count, 1ull, &evidence->layer_count))
        return 0;
    if (cpu->attention_class == YVEX_ATTENTION_CLASS_SWA) {
        if (!yvex_core_u64_add(
                evidence->swa_count, 1ull, &evidence->swa_count))
            return 0;
    } else if (cpu->attention_class == YVEX_ATTENTION_CLASS_CSA) {
        if (!yvex_core_u64_add(
                evidence->csa_count, 1ull, &evidence->csa_count))
            return 0;
    } else if (cpu->attention_class == YVEX_ATTENTION_CLASS_HCA) {
        if (!yvex_core_u64_add(
                evidence->hca_count, 1ull, &evidence->hca_count))
            return 0;
    }
    else
        return 0;
    return attention_evidence_add_case(
        evidence, "descriptor-layer", cpu, cuda, cpu_reference,
        cuda_reference, cpu_cuda, cpu_oracle, cuda_oracle);
}

static int attention_evidence_final(
    attention_live_evidence *evidence,
    char identity[YVEX_SHA256_HEX_CAP])
{
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];

    if (!evidence || !identity ||
        !yvex_sha256_update_u64(&evidence->hash, evidence->layer_count) ||
        !yvex_sha256_update_u64(&evidence->hash, evidence->swa_count) ||
        !yvex_sha256_update_u64(&evidence->hash, evidence->csa_count) ||
        !yvex_sha256_update_u64(&evidence->hash, evidence->hca_count) ||
        !yvex_sha256_update_u64(
            &evidence->hash, evidence->exact_topk_positions) ||
        !yvex_sha256_final(&evidence->hash, digest))
        return 0;
    yvex_sha256_hex(digest, identity);
    if (!yvex_sha256_final(&evidence->oracle_trace_hash, digest)) return 0;
    yvex_sha256_hex(digest, evidence->oracle_trace_identity);
    if (!yvex_sha256_final(&evidence->oracle_output_hash, digest)) return 0;
    yvex_sha256_hex(digest, evidence->oracle_output_identity);
    if (!yvex_sha256_final(&evidence->fixture_hash, digest)) return 0;
    yvex_sha256_hex(digest, evidence->fixture_identity);
    if (!yvex_sha256_final(&evidence->history_hash, digest)) return 0;
    yvex_sha256_hex(digest, evidence->history_identity);
    if (!yvex_sha256_final(&evidence->comparison_contract_hash, digest))
        return 0;
    yvex_sha256_hex(digest, evidence->comparison_contract_identity);
    return 1;
}

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
            yvex_test_attention_failure_name(failure->code),
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

/* Contract: proves production and oracle selection agree for zero/full-k and
 * independently refuse non-finite scores and duplicate candidate positions. */
static int run_topk_contract_cases(void)
{
    float scores[] = {2.0f, 2.0f, 1.0f};
    unsigned long long positions[] = {9ull, 4ull, 7ull};
    unsigned long long duplicate_positions[] = {9ull, 9ull, 7ull};
    unsigned long long production[3] = {~0ull, ~0ull, ~0ull};
    unsigned long long reference[3] = {~0ull, ~0ull, ~0ull};
    unsigned long long production_count = ~0ull;
    unsigned long long reference_count = ~0ull;
    yvex_attention_failure failure;
    yvex_error err;
    int rc;

    memset(&failure, 0, sizeof(failure));
    yvex_error_clear(&err);
    rc = yvex_attention_topk_select(
        scores, positions, 3ull, 0ull, production, &production_count,
        &failure, &err);
    if (rc != YVEX_OK || production_count != 0ull ||
        !yvex_test_attention_reference_topk(
            scores, positions, 3ull, 0ull, reference, &reference_count) ||
        reference_count != 0ull)
        return 0;
    rc = yvex_attention_topk_select(
        scores, positions, 3ull, 4ull, production, &production_count,
        &failure, &err);
    if (rc != YVEX_OK || production_count != 3ull ||
        !yvex_test_attention_reference_topk(
            scores, positions, 3ull, 4ull, reference, &reference_count) ||
        reference_count != 3ull ||
        memcmp(production, reference, sizeof(production)) != 0 ||
        production[0] != 1ull || production[1] != 0ull ||
        production[2] != 2ull)
        return 0;
    scores[1] = INFINITY;
    rc = yvex_attention_topk_select(
        scores, positions, 3ull, 1ull, production, &production_count,
        &failure, &err);
    if (rc == YVEX_OK ||
        failure.code != YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC ||
        yvex_test_attention_reference_topk(
            scores, positions, 3ull, 1ull, reference, &reference_count))
        return 0;
    scores[1] = 2.0f;
    memset(&failure, 0, sizeof(failure));
    yvex_error_clear(&err);
    rc = yvex_attention_topk_select(
        scores, duplicate_positions, 3ull, 1ull, production,
        &production_count, &failure, &err);
    if (rc == YVEX_OK ||
        failure.code != YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC ||
        yvex_test_attention_reference_topk(
            scores, duplicate_positions, 3ull, 1ull, reference,
            &reference_count))
        return 0;
    printf("attention_topk_zero_exact=1\n");
    printf("attention_topk_full_count_exact=1\n");
    printf("attention_topk_nonfinite_refused=1\n");
    printf("attention_topk_duplicate_refused=1\n");
    return 1;
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
    yvex_test_attention_reference_evidence *oracle_evidence,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_attention_summary *summary;
    const yvex_attention_layer_plan *layer;
    yvex_test_attention_reference_contract contract;
    yvex_attention_cpu_options effective_options = *options;
    yvex_attention_cpu_options production_options;
    yvex_attention_execution_trace production;
    yvex_attention_execution_trace reference;
    float *owned_input = NULL;
    unsigned long long input_count = 0ull;
    unsigned long long token_count;
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP];
    int rc;

    memset(&production, 0, sizeof(production));
    memset(&reference, 0, sizeof(reference));
    memset(metrics, 0, sizeof(*metrics));
    summary = yvex_graph_lower_deepseek_v4()->plan_summary(plan);
    if (!attention_reference_contract_init(
            &contract, summary, "cpu-reference",
            ATTENTION_CPU_ABSOLUTE_TOLERANCE,
            ATTENTION_CPU_RELATIVE_TOLERANCE))
        return YVEX_ERR_STATE;
    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(
        plan, effective_options.layer_index);
    token_count = effective_options.token_count
        ? effective_options.token_count : 1ull;
    if (!effective_options.input) {
        if (!layer ||
            !yvex_core_u64_mul(token_count, layer->hidden_dimension,
                               &input_count) ||
            input_count > (unsigned long long)(SIZE_MAX / sizeof(*owned_input)))
            return YVEX_ERR_BOUNDS;
        owned_input = (float *)malloc((size_t)input_count * sizeof(*owned_input));
        if (!owned_input) return YVEX_ERR_NOMEM;
        fill_history_values(
            owned_input, input_count,
            1009ull + effective_options.layer_index * 131ull +
                effective_options.token_position);
        effective_options.input = owned_input;
        effective_options.input_stride = layer->hidden_dimension;
    }
    production_options = effective_options;
    production_options.publication = &production;
    production_options.trace = NULL;
    rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
        plan, ir, session, descriptor, &production_options,
        production_result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!production.owned || !production.complete || !production.input ||
        !production.raw_kv || !production.output ||
        (effective_options.history &&
         effective_options.history->main_rolling_state.present &&
         !production.next_main_rolling_state.present)) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    if (!yvex_test_attention_reference_execute(
            plan, ir, session, descriptor, &effective_options, &reference,
            reason)) {
        fprintf(stderr, "attention_reference_failure=%s\n", reason);
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (!yvex_test_attention_reference_compare_contract(
            &production, &reference, &contract, metrics)) {
        fprintf(stderr,
                "attention_reference_mismatch layer=%llu class=%u stage=%s "
                "coordinate=%llu values=%llu max_abs=%.17g "
                "max_rel=%.17g rmse=%.17g\n",
                options->layer_index,
                (unsigned int)production.attention_class,
                metrics->first_failed_stage ? metrics->first_failed_stage : "",
                metrics->first_failed_index,
                metrics->compared_values, metrics->maximum_absolute_error,
                metrics->maximum_relative_error,
                reference_metrics_rmse(metrics));
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (oracle_evidence &&
        !yvex_test_attention_reference_evidence_build(
            &reference, effective_options.history,
            summary->attention_plan_identity,
            &contract, oracle_evidence)) {
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
    yvex_graph_lower_deepseek_v4()->publication_release(&production);
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&reference);
    free(owned_input);
    return rc;
}

/* Contract: perturb one real production float stage and require the separate
 * oracle comparison to identify the changed logical coordinate. */
static int real_float_mutation_detected(
    yvex_attention_execution_trace *production,
    const yvex_attention_execution_trace *reference,
    const yvex_test_attention_reference_contract *contract,
    float *values,
    unsigned long long count)
{
    yvex_test_attention_reference_metrics metrics;
    float original;
    int detected;

    if (!production || !reference || !contract || !values || !count)
        return 0;
    original = values[0];
    values[0] = isfinite(original) ? original + 64.0f : 64.0f;
    detected = !yvex_test_attention_reference_compare_contract(
        production, reference, contract, &metrics);
    values[0] = original;
    return detected;
}

/* Contract: perturb one real production discrete stage and require the
 * independent oracle to reject the changed position or selection. */
static int real_position_mutation_detected(
    yvex_attention_execution_trace *production,
    const yvex_attention_execution_trace *reference,
    const yvex_test_attention_reference_contract *contract,
    unsigned long long *positions,
    unsigned long long count)
{
    yvex_test_attention_reference_metrics metrics;
    unsigned long long original;
    int detected;

    if (!production || !reference || !contract || !positions || !count)
        return 0;
    original = positions[0];
    positions[0] = original == ULLONG_MAX ? 0ull : original + 1ull;
    detected = !yvex_test_attention_reference_compare_contract(
        production, reference, contract, &metrics);
    positions[0] = original;
    return detected;
}

/* Contract: execute real selected-artifact weights and prove that corrupting
 * each available production stage is observable to the independent oracle. */
static int run_real_mutation_proof(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    unsigned long long *detected_count,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_attention_summary *summary;
    yvex_test_attention_reference_contract contract;
    yvex_test_attention_reference_metrics metrics;
    yvex_attention_cpu_options production_options;
    yvex_attention_cpu_result result;
    yvex_attention_execution_trace production;
    yvex_attention_execution_trace reference;
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP];
    unsigned long long detected = 0ull;
    int rc;

    if (!options || !options->input || !detected_count) return YVEX_ERR_INVALID_ARG;
    memset(&production, 0, sizeof(production));
    memset(&reference, 0, sizeof(reference));
    production_options = *options;
    production_options.publication = &production;
    production_options.trace = NULL;
    summary = yvex_graph_lower_deepseek_v4()->plan_summary(plan);
    if (!attention_reference_contract_init(
            &contract, summary, "real-stage-mutation",
            ATTENTION_CPU_ABSOLUTE_TOLERANCE,
            ATTENTION_CPU_RELATIVE_TOLERANCE))
        return YVEX_ERR_STATE;
    rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
        plan, ir, session, descriptor, &production_options, &result,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!yvex_test_attention_reference_execute(
            plan, ir, session, descriptor, options, &reference, reason) ||
        !yvex_test_attention_reference_compare_contract(
            &production, &reference, &contract, &metrics)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
#define MUTATE_FLOAT(field, count) do {                                        \
    unsigned long long mutation_count_ = (unsigned long long)(count);           \
    if (mutation_count_ != 0ull && !real_float_mutation_detected(               \
            &production, &reference, &contract, production.field,               \
            mutation_count_)) {                                                 \
        rc = YVEX_ERR_FORMAT;                                                   \
        goto cleanup;                                                           \
    }                                                                           \
    if (mutation_count_ != 0ull) detected++;                                    \
} while (0)
    MUTATE_FLOAT(input, production.token_count * production.hidden_width);
    MUTATE_FLOAT(q_low, production.token_count * production.q_rank);
    MUTATE_FLOAT(query, production.token_count * production.query_width);
    MUTATE_FLOAT(raw_kv, production.token_count * production.kv_width);
    MUTATE_FLOAT(compressed_kv,
                 production.compressed_count * production.compressed_stride);
    MUTATE_FLOAT(indexer_kv,
                 production.indexer_count * production.indexer_stride);
    MUTATE_FLOAT(index_query,
                 production.token_count * production.index_query_stride);
    MUTATE_FLOAT(index_weights,
                 production.token_count * production.index_weight_stride);
    MUTATE_FLOAT(attention_values,
                 production.token_count * production.query_width);
    MUTATE_FLOAT(output, production.token_count * production.hidden_width);
    MUTATE_FLOAT(next_main_rolling_state.kv_state,
                 production.next_main_rolling_state.kv_state_extent);
    MUTATE_FLOAT(next_indexer_rolling_state.kv_state,
                 production.next_indexer_rolling_state.kv_state_extent);
#undef MUTATE_FLOAT
    if (production.compressed_count) {
        if (!real_position_mutation_detected(
                &production, &reference, &contract,
                production.compressed_positions,
                production.compressed_count)) {
            rc = YVEX_ERR_FORMAT;
            goto cleanup;
        }
        detected++;
    }
    if (production.topk_counts && production.topk_counts[0]) {
        if (!real_position_mutation_detected(
                &production, &reference, &contract,
                production.topk_positions, production.topk_counts[0])) {
            rc = YVEX_ERR_FORMAT;
            goto cleanup;
        }
        detected++;
    }
    *detected_count += detected;
    rc = YVEX_OK;
cleanup:
    yvex_graph_lower_deepseek_v4()->publication_release(&production);
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
    yvex_test_attention_reference_evidence *oracle_evidence,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_attention_summary *summary;
    yvex_test_attention_reference_contract contract;
    yvex_attention_cpu_options production_options = *options;
    yvex_attention_execution_trace production;
    yvex_attention_execution_trace reference;
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP];
    int rc;

    memset(&production, 0, sizeof(production));
    memset(&reference, 0, sizeof(reference));
    memset(metrics, 0, sizeof(*metrics));
    summary = yvex_graph_lower_deepseek_v4()->plan_summary(plan);
    if (!attention_reference_contract_init(
            &contract, summary, "cuda-reference",
            ATTENTION_CUDA_ABSOLUTE_TOLERANCE,
            ATTENTION_CUDA_RELATIVE_TOLERANCE))
        return YVEX_ERR_STATE;
    production_options.publication = &production;
    production_options.trace = NULL;
    rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
        plan, ir, session, descriptor, backend, &production_options,
        production_result, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!production.owned || !production.complete || !production.input ||
        !production.raw_kv || !production.output ||
        (options->history && options->history->main_rolling_state.present &&
         !production.next_main_rolling_state.present) ||
        !production_result->cuda_executed ||
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
    if (!yvex_test_attention_reference_compare_contract(
            &production, &reference, &contract, metrics)) {
        fprintf(stderr,
                "attention_cuda_reference_mismatch layer=%llu class=%u "
                "stage=%s coordinate=%llu values=%llu max_abs=%.17g "
                "max_rel=%.17g rmse=%.17g\n",
                options->layer_index,
                (unsigned int)production.attention_class,
                metrics->first_failed_stage ? metrics->first_failed_stage : "",
                metrics->first_failed_index,
                metrics->compared_values, metrics->maximum_absolute_error,
                metrics->maximum_relative_error,
                reference_metrics_rmse(metrics));
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (oracle_evidence &&
        !yvex_test_attention_reference_evidence_build(
            &reference, options->history, summary->attention_plan_identity,
            &contract, oracle_evidence)) {
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
    yvex_graph_lower_deepseek_v4()->publication_release(&production);
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&reference);
    return rc;
}

/* Contract: compares CPU and CUDA independently with the oracle, then
 * compares both production traces directly before releasing owned state. */
static int run_cpu_cuda_reference_compare(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_backend *backend,
    const yvex_attention_cpu_options *options,
    yvex_attention_cpu_result *cpu_result,
    yvex_attention_cpu_result *cuda_result,
    yvex_test_attention_reference_metrics *cpu_reference,
    yvex_test_attention_reference_metrics *cuda_reference,
    yvex_test_attention_reference_metrics *cpu_cuda,
    yvex_test_attention_reference_evidence *cpu_oracle,
    yvex_test_attention_reference_evidence *cuda_oracle,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_attention_summary *summary;
    yvex_test_attention_reference_contract contract;
    yvex_attention_execution_trace cpu_trace;
    yvex_attention_execution_trace cuda_trace;
    int rc;

    memset(&cpu_trace, 0, sizeof(cpu_trace));
    memset(&cuda_trace, 0, sizeof(cuda_trace));
    rc = run_reference_compare(
        plan, ir, session, descriptor, options, cpu_result, cpu_reference,
        &cpu_trace, cpu_oracle, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = run_cuda_reference_compare(
        plan, ir, session, descriptor, backend, options, cuda_result,
        cuda_reference, &cuda_trace, cuda_oracle, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    summary = yvex_graph_lower_deepseek_v4()->plan_summary(plan);
    if (!attention_reference_contract_init(
            &contract, summary, "cpu-cuda",
            ATTENTION_CUDA_ABSOLUTE_TOLERANCE,
            ATTENTION_CUDA_RELATIVE_TOLERANCE) ||
        !yvex_test_attention_reference_compare_contract(
            &cpu_trace, &cuda_trace, &contract, cpu_cuda)) {
        fprintf(stderr,
                "attention_cpu_cuda_mismatch layer=%llu class=%u stage=%s "
                "coordinate=%llu values=%llu max_abs=%.17g "
                "max_rel=%.17g rmse=%.17g\n",
                options->layer_index,
                (unsigned int)cpu_trace.attention_class,
                cpu_cuda->first_failed_stage ?
                    cpu_cuda->first_failed_stage : "",
                cpu_cuda->first_failed_index, cpu_cuda->compared_values,
                cpu_cuda->maximum_absolute_error,
                cpu_cuda->maximum_relative_error,
                reference_metrics_rmse(cpu_cuda));
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    rc = YVEX_OK;

cleanup:
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&cpu_trace);
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&cuda_trace);
    return rc;
}

typedef struct {
    unsigned long long calls;
    unsigned long long cancel_on_call;
} attention_live_cancellation;

static int attention_live_cancel_requested(void *context)
{
    attention_live_cancellation *cancellation =
        (attention_live_cancellation *)context;

    if (!cancellation) return 0;
    cancellation->calls++;
    return cancellation->calls >= cancellation->cancel_on_call;
}

/* Contract: runs one failure seam on a fresh backend and proves no result or
 * trace becomes observable after rollback. */
static int run_cuda_fault_case(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *base_options,
    const char *environment,
    const char *value,
    unsigned long long cancel_on_call,
    yvex_attention_failure_code expected_failure,
    yvex_error *err)
{
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    yvex_attention_cpu_options options;
    yvex_attention_cpu_result result;
    yvex_attention_execution_trace trace;
    yvex_attention_failure failure;
    attention_live_cancellation cancel_state;
    yvex_attention_cancellation cancellation;
    int rc;
    int passed;

    memset(&backend_options, 0, sizeof(backend_options));
    memset(&result, 0, sizeof(result));
    memset(&trace, 0, sizeof(trace));
    memset(&failure, 0, sizeof(failure));
    memset(&cancel_state, 0, sizeof(cancel_state));
    memset(&cancellation, 0, sizeof(cancellation));
    backend_options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) return 0;
    options = *base_options;
    options.trace = &trace;
    if (cancel_on_call) {
        cancel_state.cancel_on_call = cancel_on_call;
        cancellation.requested = attention_live_cancel_requested;
        cancellation.context = &cancel_state;
        options.cancellation = &cancellation;
    }
    if (environment && setenv(environment, value, 1) != 0) {
        yvex_backend_close(backend);
        return 0;
    }
    rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
        plan, ir, session, descriptor, backend, &options, &result, &failure,
        err);
    if (environment) (void)unsetenv(environment);
    passed = rc != YVEX_OK && !result.executed && !trace.owned &&
             failure.code == expected_failure;
    yvex_graph_lower_deepseek_v4()->execution_trace_release(&trace);
    yvex_backend_close(backend);
    return passed;
}

/* Contract: one missing exact encoded-attention symbol rejects the entire
 * variant atomically while leaving Driver memory capability available. */
static int run_cuda_bundle_refusal(yvex_error *err)
{
    yvex_backend_options options;
    yvex_backend_capability_result capability;
    yvex_backend *backend = NULL;
    int rc;
    int passed;

    memset(&options, 0, sizeof(options));
    memset(&capability, 0, sizeof(capability));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    if (setenv("YVEX_TEST_CUDA_BUNDLE_FAILURE", "yvex_deepseek_topk", 1) !=
        0)
        return 0;
    rc = yvex_backend_open(&backend, &options, err);
    (void)unsetenv("YVEX_TEST_CUDA_BUNDLE_FAILURE");
    if (rc != YVEX_OK || !backend) {
        yvex_backend_close(backend);
        return 0;
    }
    rc = yvex_backend_query_capability(
        backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &capability, err);
    passed = rc == YVEX_OK &&
             capability.state == YVEX_BACKEND_CAPABILITY_FAILED &&
             capability.reason ==
                 YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING &&
             !capability.kernel_bundle_available &&
             !capability.function_available;
    yvex_backend_close(backend);
    return passed;
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
    unsigned long long token_position)
{
    unsigned long long local_capacity;
    unsigned long long local_count;
    unsigned long long compressed_count = 0ull;
    unsigned long long i;

    if (!history || !layer || !summary || !layer->sliding_window) return 0;
    local_capacity = layer->sliding_window - 1ull;
    local_count = token_position < local_capacity
        ? token_position : local_capacity;
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        if (!layer->compression_ratio) return 0;
        compressed_count = token_position / layer->compression_ratio;
    }
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

/* Contract: proves every release layer on generated PTX against both CPU and
 * the independent oracle, then exercises deep CSA/HCA state boundaries. */
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
    yvex_attention_cpu_result cpu_result;
    yvex_attention_cpu_result cuda_result;
    yvex_test_attention_reference_metrics cpu_reference;
    yvex_test_attention_reference_metrics cuda_reference;
    yvex_test_attention_reference_metrics cpu_cuda;
    yvex_test_attention_reference_evidence cpu_oracle;
    yvex_test_attention_reference_evidence cuda_oracle;
    yvex_attention_execution_trace failed_trace;
    attention_live_evidence evidence;
    live_attention_history history;
    const yvex_attention_layer_plan *layer;
    float *input = NULL;
    unsigned long long input_extent = 0ull;
    unsigned long long layer_index;
    unsigned long long launches = 0ull;
    unsigned long long peak_host_bytes = 0ull;
    unsigned long long peak_device_bytes = 0ull;
    unsigned long long peak_encoded_weight_staging_bytes = 0ull;
    unsigned long long configured_host_scratch_bytes = 0ull;
    unsigned long long peak_total_host_owned_bytes = 0ull;
    unsigned long long mutation_detected = 0ull;
    char repeat_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    char evidence_identity[YVEX_SHA256_HEX_CAP];
    static const struct {
        const char *seam;
        yvex_attention_failure_code expected_failure;
    } fault_cases[] = {
        {"allocation", YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION},
        {"host-staging", YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION},
        {"copy-input", YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND},
        {"cuda.deepseek_attention.q_a",
         YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND},
        {"cuda.deepseek_attention.copy.output",
         YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND}
    };
    unsigned int fault_index;
    unsigned int qtype;
    int rc;

    memset(&backend_options, 0, sizeof(backend_options));
    memset(&capability, 0, sizeof(capability));
    memset(&failed_trace, 0, sizeof(failed_trace));
    memset(&evidence, 0, sizeof(evidence));
    memset(&history, 0, sizeof(history));
    memset(repeat_identity, 0, sizeof(repeat_identity));
    memset(evidence_identity, 0, sizeof(evidence_identity));
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
    if (!attention_evidence_init(&evidence, summary)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    for (layer_index = 0ull;
         layer_index <
             yvex_graph_lower_deepseek_v4()->plan_layer_count(plan);
         ++layer_index) {
        layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(
            plan, layer_index);
        if (!layer || layer->hidden_dimension != input_extent) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
        fill_history_values(input, input_extent, 401ull + layer_index);
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&options);
        configured_host_scratch_bytes = options.scratch_limit_bytes;
        options.layer_index = layer_index;
        options.token_position = 0ull;
        options.token_count = 1ull;
        options.input = input;
        options.input_stride = input_extent;
        if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
            if (!live_history_init(&history, layer, summary, 0ull)) {
                rc = YVEX_ERR_NOMEM;
                goto cleanup;
            }
            options.history = &history.view;
        }
        rc = run_cpu_cuda_reference_compare(
            plan, ir, session, descriptor, backend, &options, &cpu_result,
            &cuda_result, &cpu_reference, &cuda_reference, &cpu_cuda,
            &cpu_oracle, &cuda_oracle, failure, err);
        if (rc != YVEX_OK ||
            !attention_evidence_add(
                &evidence, &cpu_result, &cuda_result, &cpu_reference,
                &cuda_reference, &cpu_cuda, &cpu_oracle, &cuda_oracle)) {
            live_history_release(&history);
            goto cleanup;
        }
        live_history_release(&history);
        if (layer_index == swa_layer)
            memcpy(repeat_identity, cuda_result.output_identity,
                   sizeof(repeat_identity));
        if (!yvex_core_u64_add(
                launches, cuda_result.cuda_kernel_launches, &launches)) {
            rc = YVEX_ERR_FORMAT;
            goto cleanup;
        }
        if (cuda_result.cuda_peak_device_bytes > peak_device_bytes)
            peak_device_bytes = cuda_result.cuda_peak_device_bytes;
        if (cuda_result.cuda_peak_host_bytes > peak_host_bytes)
            peak_host_bytes = cuda_result.cuda_peak_host_bytes;
        if (cuda_result.payload_bytes_read >
            peak_encoded_weight_staging_bytes)
            peak_encoded_weight_staging_bytes =
                cuda_result.payload_bytes_read;
    }

    fill_history_values(input, input_extent, 401ull + swa_layer);
    yvex_graph_lower_deepseek_v4()->cpu_options_default(&options);
    options.layer_index = swa_layer;
    options.token_position = 0ull;
    options.token_count = 1ull;
    options.input = input;
    options.input_stride = input_extent;

    rc = run_real_mutation_proof(
        plan, ir, session, descriptor, &options, &mutation_detected,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;

    options.input = NULL;
    rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
        plan, ir, session, descriptor, &options, &cpu_result, failure, err);
    if (rc != YVEX_ERR_INVALID_ARG || cpu_result.executed ||
        failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    options.input = input;
    options.token_position = 1ull;
    rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
        plan, ir, session, descriptor, &options, &cpu_result, failure, err);
    if (rc != YVEX_ERR_STATE || cpu_result.executed ||
        failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
        plan, ir, session, descriptor, backend, &options, &cuda_result,
        failure, err);
    if (rc != YVEX_ERR_STATE || cuda_result.executed ||
        failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    options.token_position = 0ull;
    printf("attention_explicit_input_refusal=1\n");
    printf("attention_missing_history_refusal=1\n");

    for (fault_index = 0u;
         fault_index < sizeof(fault_cases) / sizeof(fault_cases[0]);
         ++fault_index) {
        if (setenv("YVEX_TEST_CUDA_ATTENTION_FAILURE",
                   fault_cases[fault_index].seam, 1) != 0) {
            rc = YVEX_ERR_IO;
            goto cleanup;
        }
        options.trace = &failed_trace;
        rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
            plan, ir, session, descriptor, backend, &options, &cuda_result,
            failure, err);
        (void)unsetenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
        options.trace = NULL;
        if (rc == YVEX_OK || failed_trace.owned || cuda_result.executed ||
            failure->code != fault_cases[fault_index].expected_failure) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
    }
    rc = run_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &cuda_result,
        &cuda_reference, NULL, NULL, failure, err);
    if (rc != YVEX_OK ||
        strcmp(repeat_identity, cuda_result.output_identity) != 0)
        goto cleanup;
    if (!yvex_core_u64_add(
            launches, cuda_result.cuda_kernel_launches, &launches)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (cuda_result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = cuda_result.cuda_peak_device_bytes;
    if (cuda_result.cuda_peak_host_bytes > peak_host_bytes)
        peak_host_bytes = cuda_result.cuda_peak_host_bytes;
    if (cuda_result.payload_bytes_read > peak_encoded_weight_staging_bytes)
        peak_encoded_weight_staging_bytes = cuda_result.payload_bytes_read;
    if (!run_cuda_fault_case(
            plan, ir, session, descriptor, &options,
            "YVEX_TEST_CUDA_SYNC_FAILURE", "encoded-attention", 0ull,
            YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, err) ||
        !run_cuda_fault_case(
            plan, ir, session, descriptor, &options,
            "YVEX_TEST_CUDA_CLEANUP_FAILURE", "encoded-attention", 0ull,
            YVEX_DEEPSEEK_ATTENTION_FAILURE_CLEANUP, err) ||
        !run_cuda_fault_case(
            plan, ir, session, descriptor, &options, NULL, NULL, 1ull,
            YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED, err) ||
        !run_cuda_fault_case(
            plan, ir, session, descriptor, &options, NULL, NULL, 9ull,
            YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED, err) ||
        !run_cuda_bundle_refusal(err)) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }

    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(plan, csa_layer);
    if (!layer || layer->hidden_dimension != input_extent ||
        !live_history_init(&history, layer, summary, 2052ull)) {
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
    {
        yvex_attention_history_view stale = history.view;
        stale.main_rolling_state.current_fill++;
        options.history = &stale;
        rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
            plan, ir, session, descriptor, &options, &cpu_result, failure,
            err);
        if (rc != YVEX_ERR_STATE || cpu_result.executed ||
            failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
        rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
            plan, ir, session, descriptor, backend, &options, &cuda_result,
            failure, err);
        if (rc != YVEX_ERR_STATE || cuda_result.executed ||
            failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
        stale = history.view;
        stale.main_rolling_state.previous_fill--;
        options.history = &stale;
        rc = yvex_graph_lower_deepseek_v4()->cuda_token_execute(
            plan, ir, session, descriptor, backend, &options, &cuda_result,
            failure, err);
        if (rc != YVEX_ERR_STATE || cuda_result.executed ||
            failure->code != YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY) {
            rc = YVEX_ERR_STATE;
            goto cleanup;
        }
        options.history = &history.view;
        printf("attention_stale_rolling_refusal=1\n");
    }
    rc = run_cpu_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &cpu_result,
        &cuda_result, &cpu_reference, &cuda_reference, &cpu_cuda,
        &cpu_oracle, &cuda_oracle, failure, err);
    if (rc != YVEX_OK || cuda_result.topk_candidates != 513ull ||
        cuda_result.topk_selected != 512ull)
        goto cleanup;
    if (!attention_evidence_add_case(
            &evidence, "csa-topk-513", &cpu_result, &cuda_result,
            &cpu_reference, &cuda_reference, &cpu_cuda, &cpu_oracle,
            &cuda_oracle)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    rc = run_real_mutation_proof(
        plan, ir, session, descriptor, &options, &mutation_detected,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!yvex_core_u64_add(
            launches, cuda_result.cuda_kernel_launches, &launches)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (cuda_result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = cuda_result.cuda_peak_device_bytes;
    if (cuda_result.cuda_peak_host_bytes > peak_host_bytes)
        peak_host_bytes = cuda_result.cuda_peak_host_bytes;
    if (cuda_result.payload_bytes_read > peak_encoded_weight_staging_bytes)
        peak_encoded_weight_staging_bytes = cuda_result.payload_bytes_read;
    live_history_release(&history);

    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(plan, hca_layer);
    if (!layer || layer->hidden_dimension != input_extent ||
        !live_history_init(&history, layer, summary, 127ull)) {
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
    rc = run_cpu_cuda_reference_compare(
        plan, ir, session, descriptor, backend, &options, &cpu_result,
        &cuda_result, &cpu_reference, &cuda_reference, &cpu_cuda,
        &cpu_oracle, &cuda_oracle, failure, err);
    if (rc != YVEX_OK || cuda_result.compressed_entries != 1ull)
        goto cleanup;
    if (!attention_evidence_add_case(
            &evidence, "hca-ratio-128", &cpu_result, &cuda_result,
            &cpu_reference, &cuda_reference, &cpu_cuda, &cpu_oracle,
            &cuda_oracle)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    rc = run_real_mutation_proof(
        plan, ir, session, descriptor, &options, &mutation_detected,
        failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!yvex_core_u64_add(
            launches, cuda_result.cuda_kernel_launches, &launches)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    if (cuda_result.cuda_peak_device_bytes > peak_device_bytes)
        peak_device_bytes = cuda_result.cuda_peak_device_bytes;
    if (cuda_result.cuda_peak_host_bytes > peak_host_bytes)
        peak_host_bytes = cuda_result.cuda_peak_host_bytes;
    if (cuda_result.payload_bytes_read > peak_encoded_weight_staging_bytes)
        peak_encoded_weight_staging_bytes = cuda_result.payload_bytes_read;
    live_history_release(&history);

    if (evidence.layer_count != summary->layer_count ||
        evidence.swa_count != summary->swa_layer_count ||
        evidence.csa_count != summary->csa_layer_count ||
        evidence.hca_count != summary->hca_layer_count ||
        !attention_evidence_final(&evidence, evidence_identity)) {
        rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    if (!yvex_core_u64_add(
            peak_host_bytes, peak_encoded_weight_staging_bytes,
            &peak_total_host_owned_bytes)) {
        rc = YVEX_ERR_FORMAT;
        goto cleanup;
    }
    printf("attention_cuda_layers_executed=%llu\n", evidence.layer_count);
    printf("attention_publication_abi_consumed=1\n");
    printf("attention_real_stage_mutations_detected=%llu\n",
           mutation_detected);
    printf("attention_cuda_swa_layers=%llu\n", evidence.swa_count);
    printf("attention_cuda_csa_layers=%llu\n", evidence.csa_count);
    printf("attention_cuda_hca_layers=%llu\n", evidence.hca_count);
    printf("attention_cuda_classes_executed=3\n");
    printf("attention_cuda_kernel_launches=%llu\n", launches);
    printf("attention_cuda_peak_host_bytes=%llu\n", peak_host_bytes);
    printf("attention_cuda_peak_device_bytes=%llu\n", peak_device_bytes);
    printf("attention_cuda_peak_encoded_weight_staging_bytes=%llu\n",
           peak_encoded_weight_staging_bytes);
    printf("attention_cuda_peak_total_host_owned_bytes=%llu\n",
           peak_total_host_owned_bytes);
    printf("attention_cuda_configured_host_scratch_bytes=%llu\n",
           configured_host_scratch_bytes);
    printf("attention_required_bindings_executed=%llu\n",
           summary->required_binding_count);
    printf("attention_qtype_compute_refusals=%llu\n",
           summary->qtype_compute_refusal_count);
    printf("attention_qtype_paths=%llu\n", evidence.qtype_paths);
    for (qtype = 0u; qtype < YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP; ++qtype) {
        if (!summary->qtype_binding_counts[qtype]) continue;
        printf("attention_qtype_bindings.%s=%llu\n",
               yvex_gguf_qtype_name(qtype),
               summary->qtype_binding_counts[qtype]);
    }
    printf("attention_cpu_reference_values=%llu\n",
           evidence.cpu_reference_values);
    printf("attention_cpu_reference_max_abs=%.17g\n",
           evidence.cpu_maximum_absolute_error);
    printf("attention_cpu_reference_max_rel=%.17g\n",
           evidence.cpu_maximum_relative_error);
    printf("attention_cpu_reference_rmse=%.17g\n",
           evidence.cpu_reference_values ?
               sqrt(evidence.cpu_squared_error /
                    (double)evidence.cpu_reference_values) : 0.0);
    printf("attention_cuda_reference_values=%llu\n",
           evidence.cuda_reference_values);
    printf("attention_cuda_reference_max_abs=%.17g\n",
           evidence.cuda_maximum_absolute_error);
    printf("attention_cuda_reference_max_rel=%.17g\n",
           evidence.cuda_maximum_relative_error);
    printf("attention_cuda_reference_rmse=%.17g\n",
           evidence.cuda_reference_values ?
               sqrt(evidence.cuda_squared_error /
                    (double)evidence.cuda_reference_values) : 0.0);
    printf("attention_cpu_cuda_values=%llu\n", evidence.cpu_cuda_values);
    printf("attention_cpu_cuda_max_abs=%.17g\n",
           evidence.cpu_cuda_maximum_absolute_error);
    printf("attention_cpu_cuda_max_rel=%.17g\n",
           evidence.cpu_cuda_maximum_relative_error);
    printf("attention_cpu_cuda_rmse=%.17g\n",
           evidence.cpu_cuda_values ?
               sqrt(evidence.cpu_cuda_squared_error /
                    (double)evidence.cpu_cuda_values) : 0.0);
    printf("attention_execution_evidence_identity=%s\n", evidence_identity);
    printf("attention_reference_oracle_trace_identity=%s\n",
           evidence.oracle_trace_identity);
    printf("attention_reference_oracle_output_identity=%s\n",
           evidence.oracle_output_identity);
    printf("attention_reference_fixture_identity=%s\n",
           evidence.fixture_identity);
    printf("attention_reference_history_identity=%s\n",
           evidence.history_identity);
    printf("attention_reference_comparison_contract_identity=%s\n",
           evidence.comparison_contract_identity);
    printf("attention_reference_evidence_schema=%u\n",
           YVEX_TEST_ATTENTION_EVIDENCE_SCHEMA_V2);
    printf("attention_reference_exact_topk_positions=%llu\n",
           evidence.exact_topk_positions);
    printf("attention_cuda_swa_repeat_deterministic=1\n");
    printf("attention_cuda_fault_cases=%zu\n",
           sizeof(fault_cases) / sizeof(fault_cases[0]) + 5u);
    printf("attention_cuda_fault_cleanup=1\n");
    printf("attention_cuda_cancel_before_dispatch=1\n");
    printf("attention_cuda_cancel_before_publish=1\n");
    printf("attention_cuda_missing_symbol_refused=1\n");
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
    if (!run_topk_contract_cases()) {
        fprintf(stderr, "attention_topk_contract=fail\n");
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
        const yvex_attention_layer_plan *canonical_layer =
            yvex_graph_lower_deepseek_v4()->plan_layer_at(attention_plan, 2ull);
        const yvex_materialization_summary *canonical_materialization =
            yvex_materialization_session_summary(session);
        yvex_attention_cpu_options stale_options;
        yvex_attention_cpu_result stale_result;
        float *stale_input = NULL;
        unsigned long long original_topk;
        unsigned long long bytes_before;
        char canonical_logical[YVEX_TRANSFORM_IR_IDENTITY_CAP];
        char mutated_logical[YVEX_TRANSFORM_IR_IDENTITY_CAP];
        char repeated_logical[YVEX_TRANSFORM_IR_IDENTITY_CAP];

        if (!mutable_layer || !canonical_runtime || !canonical_attention ||
            !canonical_layer ||
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
        stale_input = (float *)calloc(
            (size_t)canonical_layer->hidden_dimension, sizeof(*stale_input));
        if (!stale_input) goto identity_mutation_fail;
        stale_options.input = stale_input;
        stale_options.input_stride = canonical_layer->hidden_dimension;
        rc = yvex_graph_lower_deepseek_v4()->cpu_chunk_execute(
            attention_plan, architecture_ir, session, descriptor,
            &stale_options, &stale_result, &attention_failure, &err);
        if (rc == YVEX_OK ||
            attention_failure.code !=
                YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR ||
            stale_result.executed ||
            canonical_materialization->payload_bytes_accessed != bytes_before)
            goto identity_mutation_fail;
        free(stale_input);
        stale_input = NULL;
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
        free(stale_input);
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

    printf("mode=%s\n",
           plan_only ? "plan-only" :
           (getenv("YVEX_ATTENTION_CUDA_ONLY") ?
                "cuda-execution" : "cpu-reference-cuda"));
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
           yvex_test_attention_status_name(attention_summary->status));
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
        unsigned long long first_token_payload_read = 0ull;
        unsigned long long first_token_executed = 0ull;
        unsigned long long chunk_payload_read = 0ull;
        unsigned long long chunk_executed = 0ull;
        unsigned long long chunk_repeat_payload_read = 0ull;
        unsigned long long chunk_repeat_executed = 0ull;
        unsigned long long history_chunk_payload_read = 0ull;
        unsigned long long history_chunk_executed = 0ull;
        unsigned long long i;
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
#define RUN_FIRST_TOKEN(layer_id) do {                                          \
    if ((layer_id) != ~0ull) {                                                  \
        yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);             \
        exec_options.layer_index = (layer_id);                                  \
        exec_options.token_position = 0ull;                                     \
        exec_options.token_count = 1ull;                                         \
        rc = run_reference_compare(                                              \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &reference_metrics, NULL,               \
            NULL, &attention_failure, &err);                                     \
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
        rc = run_reference_compare(                                             \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &reference_metrics, NULL, NULL,         \
            &attention_failure, &err);                                           \
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
        rc = run_reference_compare(                                             \
            attention_plan, architecture_ir, session, descriptor,               \
            &exec_options, &exec_result, &reference_metrics, NULL, NULL,         \
            &attention_failure, &err);                                           \
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
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics, NULL, NULL,
                &attention_failure, &err);
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
            yvex_attention_execution_trace selection_trace;
            double baseline_checksum;
            char baseline_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
            unsigned long long selected_index = ~0ull;
            unsigned long long unselected_index = ~0ull;
            unsigned long long csa_reference_values = 0ull;
            unsigned long long j;

            memset(&selection_trace, 0, sizeof(selection_trace));
            memset(baseline_identity, 0, sizeof(baseline_identity));
            if (!layer || !live_history_init(
                    &history, layer, attention_summary, 2052ull))
                goto cleanup_fail;
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_csa;
            exec_options.token_position = 2052ull;
            exec_options.token_count = 1ull;
            exec_options.history = &history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics,
                &selection_trace, NULL,
                &attention_failure, &err);
            if (rc != YVEX_OK || exec_result.topk_candidates != 513ull ||
                exec_result.topk_selected != 512ull ||
                !selection_trace.topk_counts ||
                selection_trace.topk_counts[0] != 512ull) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                yvex_graph_lower_deepseek_v4()->execution_trace_release(
                    &selection_trace);
                live_history_release(&history);
                goto cleanup_fail;
            }
            baseline_checksum = exec_result.output_checksum;
            csa_reference_values = reference_metrics.compared_values;
            snprintf(baseline_identity, sizeof(baseline_identity), "%s",
                     exec_result.output_identity);
            for (j = 0ull; j < 513ull; ++j) {
                unsigned long long selected;
                int chosen = 0;

                for (selected = 0ull; selected < 512ull; ++selected) {
                    if (selection_trace.topk_positions[selected] ==
                        history.compressed_positions[j]) {
                        chosen = 1;
                        break;
                    }
                }
                if (chosen && selected_index == ~0ull)
                    selected_index = j;
                if (!chosen) unselected_index = j;
            }
            yvex_graph_lower_deepseek_v4()->execution_trace_release(
                &selection_trace);
            if (selected_index == ~0ull || unselected_index == ~0ull) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            for (j = 0ull; j < layer->head_dimension; ++j)
                history.compressed_kv[
                    selected_index * layer->head_dimension + j] *= -1.0f;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &reference_metrics, NULL,
                NULL, &attention_failure, &err);
            if (rc != YVEX_OK ||
                changed_result.output_checksum == baseline_checksum) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            for (j = 0ull; j < layer->head_dimension; ++j)
                history.compressed_kv[
                    selected_index * layer->head_dimension + j] *= -1.0f;
            for (j = 0ull; j < layer->head_dimension; ++j)
                history.compressed_kv[
                    unselected_index * layer->head_dimension + j] *= -1.0f;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &reference_metrics, NULL,
                NULL, &attention_failure, &err);
            if (rc != YVEX_OK ||
                strcmp(changed_result.output_identity, baseline_identity) != 0) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            for (j = 0ull; j < layer->head_dimension; ++j)
                history.compressed_kv[
                    unselected_index * layer->head_dimension + j] *= -1.0f;
            live_history_release(&history);
            if (!live_history_init(
                    &history, layer, attention_summary, 28ull))
                goto cleanup_fail;
            exec_options.token_position = 28ull;
            exec_options.history = &history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &reference_metrics, NULL,
                NULL, &attention_failure, &err);
            if (rc != YVEX_OK || changed_result.topk_candidates != 7ull ||
                changed_result.topk_selected != 7ull) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            live_history_release(&history);
            if (!live_history_init(
                    &history, layer, attention_summary, 2048ull))
                goto cleanup_fail;
            exec_options.token_position = 2048ull;
            exec_options.history = &history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &changed_result, &reference_metrics, NULL,
                NULL, &attention_failure, &err);
            if (rc != YVEX_OK || changed_result.topk_candidates != 512ull ||
                changed_result.topk_selected != 512ull) {
                live_history_release(&history);
                goto cleanup_fail;
            }
            live_history_release(&history);
            {
                yvex_attention_execution_trace masked_trace;

                memset(&masked_trace, 0, sizeof(masked_trace));
                if (!live_history_init(
                        &history, layer, attention_summary, 2056ull))
                    goto cleanup_fail;
                history.compressed_positions[513] = 2055ull;
                history.indexer_positions[513] = 2055ull;
                for (j = 0ull; j < layer->indexer_head_dimension; ++j)
                    history.indexer_kv[
                        513ull * layer->indexer_head_dimension + j] = 32.0f;
                exec_options.token_position = 2056ull;
                exec_options.history = &history.view;
                rc = run_reference_compare(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &exec_result, &reference_metrics,
                    &masked_trace, NULL, &attention_failure, &err);
                if (rc == YVEX_OK ||
                    attention_failure.code !=
                        YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY ||
                    masked_trace.owned || exec_result.executed) {
                    yvex_graph_lower_deepseek_v4()->execution_trace_release(
                        &masked_trace);
                    live_history_release(&history);
                    goto cleanup_fail;
                }
                yvex_graph_lower_deepseek_v4()->execution_trace_release(
                    &masked_trace);
            }
            printf("attention_csa_real_candidates=513\n");
            printf("attention_csa_fewer_than_topk=7\n");
            printf("attention_csa_exact_topk=512\n");
            printf("attention_csa_topk_selected=512\n");
            printf("attention_csa_selected_kv_affects_output=1\n");
            printf("attention_csa_unselected_kv_invariant=1\n");
            printf("attention_csa_incomplete_candidate_refused=1\n");
            printf("attention_csa_reference_values=%llu\n",
                   csa_reference_values);
            live_history_release(&history);
        }
        if (first_hca != ~0ull) {
            const yvex_attention_layer_plan *layer =
                yvex_graph_lower_deepseek_v4()->plan_layer_at(attention_plan,
                                                      first_hca);
            yvex_attention_layer_plan wrong_ratio;
            live_attention_history boundary_history;
            live_attention_history multi_history;
            yvex_attention_execution_trace boundary_trace;
            yvex_attention_cpu_result after_result;
            yvex_attention_cpu_result changed_result;

            memset(&boundary_trace, 0, sizeof(boundary_trace));
            if (!layer) goto cleanup_fail;
            wrong_ratio = *layer;
            wrong_ratio.compression_ratio = 127ull;
            rc = yvex_attention_class_geometry_validate(
                &wrong_ratio, 4ull, 128ull, &attention_failure, &err);
            if (rc == YVEX_OK ||
                attention_failure.code !=
                    YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION)
                goto cleanup_fail;
            printf("attention_hca_wrong_ratio_refused=1\n");
            if (!layer || !live_history_init(
                    &boundary_history, layer, attention_summary, 127ull))
                goto cleanup_fail;
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_hca;
            exec_options.token_position = 127ull;
            exec_options.token_count = 1ull;
            exec_options.history = &boundary_history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics,
                &boundary_trace, NULL, &attention_failure, &err);
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
            if (!boundary_history.compressed_kv ||
                !boundary_history.compressed_positions) {
                yvex_graph_lower_deepseek_v4()->execution_trace_release(
                    &boundary_trace);
                live_history_release(&boundary_history);
                goto cleanup_fail;
            }
            memcpy(boundary_history.compressed_kv,
                   boundary_trace.compressed_kv,
                   (size_t)layer->head_dimension * sizeof(float));
            boundary_history.compressed_positions[0] =
                boundary_trace.compressed_positions[0];
            memmove(boundary_history.local_kv,
                    boundary_history.local_kv + layer->head_dimension,
                    (size_t)(126ull * layer->head_dimension) * sizeof(float));
            memmove(boundary_history.local_positions,
                    boundary_history.local_positions + 1ull,
                    126ull * sizeof(*boundary_history.local_positions));
            memcpy(boundary_history.local_kv +
                       126ull * layer->head_dimension,
                   boundary_trace.raw_kv,
                   (size_t)layer->head_dimension * sizeof(float));
            boundary_history.local_positions[126] = 127ull;
            boundary_history.view.local_tail_count = 127ull;
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
                NULL, &attention_failure, &err);
            if (rc != YVEX_OK || after_result.compressed_entries != 0ull ||
                after_result.topk_candidates != 1ull) {
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
                                   384ull)) {
                fprintf(stderr, "attention_hca_multi_history_init=fail\n");
                goto cleanup_fail;
            }
            yvex_graph_lower_deepseek_v4()->cpu_options_default(&exec_options);
            exec_options.layer_index = first_hca;
            exec_options.token_position = 384ull;
            exec_options.token_count = 1ull;
            exec_options.history = &multi_history.view;
            rc = run_reference_compare(
                attention_plan, architecture_ir, session, descriptor,
                &exec_options, &exec_result, &reference_metrics, NULL,
                NULL, &attention_failure, &err);
            if (rc != YVEX_OK || exec_result.topk_candidates != 3ull ||
                exec_result.local_entries != 128ull) {
                if (rc != YVEX_OK)
                    print_attention_failure(&attention_failure, &err);
                else
                    fprintf(stderr,
                            "attention_hca_multi_result candidates=%llu local=%llu\n",
                            exec_result.topk_candidates,
                            exec_result.local_entries);
                live_history_release(&multi_history);
                goto cleanup_fail;
            }
            {
                double baseline = exec_result.output_checksum;
                unsigned long long overlap = 2ull;
                unsigned long long group_start =
                    multi_history.compressed_positions[overlap];
                unsigned long long group_end =
                    group_start + layer->compression_ratio - 1ull;
                unsigned long long raw_inside =
                    multi_history.local_positions[1];
                unsigned long long j;

                if (raw_inside <= group_start || raw_inside > group_end) {
                    fprintf(stderr,
                            "attention_hca_overlap_interval_mismatch=%llu,%llu,%llu\n",
                            group_start, group_end, raw_inside);
                    live_history_release(&multi_history);
                    goto cleanup_fail;
                }
                for (j = 0ull; j < layer->head_dimension; ++j) {
                    unsigned long long offset =
                        overlap * layer->head_dimension + j;
                    multi_history.compressed_kv[offset] =
                        -multi_history.compressed_kv[offset];
                }
                rc = run_reference_compare(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &changed_result, &reference_metrics, NULL,
                    NULL, &attention_failure, &err);
                if (rc != YVEX_OK ||
                    changed_result.output_checksum == baseline) {
                    if (rc != YVEX_OK)
                        print_attention_failure(&attention_failure, &err);
                    else
                        fprintf(stderr,
                                "attention_hca_compressed_perturbation_dead=1\n");
                    live_history_release(&multi_history);
                    goto cleanup_fail;
                }
                for (j = 0ull; j < layer->head_dimension; ++j) {
                    unsigned long long offset =
                        overlap * layer->head_dimension + j;
                    multi_history.compressed_kv[offset] =
                        -multi_history.compressed_kv[offset];
                }
                for (j = 0ull; j < layer->head_dimension; ++j)
                    multi_history.local_kv[layer->head_dimension + j] =
                        -multi_history.local_kv[
                            layer->head_dimension + j];
                rc = run_reference_compare(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &changed_result, &reference_metrics, NULL,
                    NULL, &attention_failure, &err);
                if (rc != YVEX_OK ||
                    changed_result.output_checksum == baseline) {
                    if (rc != YVEX_OK)
                        print_attention_failure(&attention_failure, &err);
                    else
                        fprintf(stderr,
                                "attention_hca_local_perturbation_dead=1\n");
                    live_history_release(&multi_history);
                    goto cleanup_fail;
                }
            }
            printf("attention_hca_multiple_compressed_groups=3\n");
            printf("attention_hca_local_window_entries=128\n");
            printf("attention_hca_full_output_reference=1\n");
            printf("attention_hca_compressed_affects_output=1\n");
            printf("attention_hca_local_affects_output=1\n");
            printf("attention_hca_raw_compressed_overlap_distinct=1\n");
            printf("attention_hca_raw_inside_compressed_interval=1\n");
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
                rc = run_reference_compare(
                    attention_plan, architecture_ir, session, descriptor,
                    &exec_options, &exec_result, &reference_metrics, NULL,
                    NULL, &attention_failure, &err);
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
