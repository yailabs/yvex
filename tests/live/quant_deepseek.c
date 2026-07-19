/*
 * quant_deepseek.c - target-scale DeepSeek quantization plan/execution proof.
 *
 * Owner: tests/live.
 * Owns: read-only handoff construction, exact plan assertions, and structured
 *   proof output consumed by the V010.QUANT.2 validation targets.
 * Does not own: profile policy, codecs, payload access, output bytes, or claims.
 * Invariants: plan mode reads zero payload bytes and never writes an artifact.
 * Boundary: execution mode is added only through the canonical discard sink.
 */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/compilation.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/families/deepseek_v4.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static unsigned long long quant_elapsed_ns(const struct timespec *begin,
                                           const struct timespec *end)
{
    if (end->tv_nsec >= begin->tv_nsec)
        return (unsigned long long)(end->tv_sec - begin->tv_sec) *
                   1000000000ull +
               (unsigned long long)(end->tv_nsec - begin->tv_nsec);
    return (unsigned long long)(end->tv_sec - begin->tv_sec - 1) *
               1000000000ull + 1000000000ull -
           (unsigned long long)(begin->tv_nsec - end->tv_nsec);
}

static int quant_plan_invariants(const yvex_quant_plan_summary *summary)
{
    return summary && summary->complete &&
           summary->state == YVEX_QUANT_PLAN_SEALED &&
           summary->schema_version == YVEX_QUANT_PROFILE_SCHEMA_VERSION &&
           summary->terminal_count == 1360u &&
           summary->decision_count == 1360u &&
           summary->source_value_count == 69187u &&
           summary->mapping_identity ==
               YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY &&
           summary->payload_bytes_read == 0u &&
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_Q8_0] != 0u &&
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_Q2_K] == 132u &&
           summary->calibration_required == 0;
}

/* Prints typed terminal/lowering facts for a live refusal without payload data. */
static void quant_print_terminal_context(
    const yvex_deepseek_payload_handoff *handoff,
    unsigned long long ordinal)
{
    const yvex_transform_value *terminal;
    const yvex_deepseek_gguf_descriptor *descriptor;

    if (!handoff || ordinal == ULLONG_MAX) return;
    terminal = yvex_transform_ir_terminal_at(
        yvex_model_register_deepseek_v4()->payload.transform_ir(handoff), ordinal);
    descriptor = yvex_model_register_deepseek_v4()->lowering.at(
        yvex_model_register_deepseek_v4()->payload.map(handoff), ordinal);
    if (!terminal || !descriptor) return;
    fprintf(stderr,
            "terminal_context role=%u scope=%u layer=%llu aux=%llu rank=%u dims=%llu,%llu,%llu descriptor_role=%u descriptor_scope=%u descriptor_layer=%llu predictor=%llu descriptor_rank=%u descriptor_dims=%llu,%llu,%llu axes=%u,%u,%u\n",
            terminal->logical_key.role, terminal->logical_key.scope,
            terminal->logical_key.layer_index,
            terminal->logical_key.auxiliary_index,
            terminal->shape.rank, terminal->shape.dims[0],
            terminal->shape.dims[1], terminal->shape.dims[2],
            descriptor->role, descriptor->scope,
            descriptor->layer_index, descriptor->predictor_index,
            descriptor->logical_rank, descriptor->logical_dims[0],
            descriptor->logical_dims[1], descriptor->logical_dims[2],
            descriptor->source_axis_for_logical[0],
            descriptor->source_axis_for_logical[1],
            descriptor->source_axis_for_logical[2]);
}

int main(int argc, char **argv)
{
    yvex_deepseek_payload_handoff_options options;
    yvex_deepseek_payload_handoff *handoff = NULL;
    yvex_deepseek_payload_failure handoff_failure;
    yvex_quant_plan *plan = NULL;
    yvex_quant_digest_sink *digest_sink = NULL;
    yvex_quant_output_sink output_sink;
    yvex_quant_digest_summary digest_summary;
    yvex_quant_execution_summary execution;
    yvex_quant_executor_options executor_options;
    const yvex_quant_plan_summary *summary;
    const yvex_transform_ir_summary *transform_summary;
    const yvex_source_verification *verification;
    yvex_quant_failure failure;
    yvex_error error;
    yvex_source_payload_session_facts source_facts;
    struct timespec begin;
    struct timespec end;
    unsigned long long elapsed = 0u;
    unsigned long long nonfinite_count = 0u;
    unsigned int operation;
    unsigned int role;
    unsigned int qtype;
    int argument = 1;
    int plan_only = 0;
    int rc;

    if (argc > 1 && strcmp(argv[1], "--plan-only") == 0) {
        plan_only = 1;
        argument++;
    }
    if (argc - argument != 3) {
        fprintf(stderr,
                "usage: %s [--plan-only] SOURCE MODELS_ROOT MANIFEST\n",
                argv[0]);
        return 2;
    }
    memset(&options, 0, sizeof(options));
    options.source_path = argv[argument];
    options.models_root = argv[argument + 1];
    options.manifest_path = argv[argument + 2];
    yvex_source_payload_budget_default(&options.budget);
    options.budget.maximum_open_handles = 32u;
    options.budget.maximum_streams = 16u;
    options.budget.maximum_inflight_host_bytes =
        options.budget.chunk_bytes * options.budget.maximum_streams;
    options.chunk_bytes = options.budget.chunk_bytes;
    options.page_bytes = options.budget.page_bytes;
    yvex_error_clear(&error);
    rc = yvex_model_register_deepseek_v4()->payload.open(
        &handoff, &options, &handoff_failure, &error);
    if (rc != YVEX_OK) {
        fprintf(stderr, "handoff_failure=%s status=%s where=%s\n",
                yvex_model_register_deepseek_v4()->payload.failure_name(handoff_failure.code),
                yvex_status_name(yvex_error_code(&error)),
                yvex_error_where(&error));
        return 1;
    }
    rc = yvex_quant_plan_build_deepseek_profile(
        &plan, yvex_model_register_deepseek_v4()->payload.transform_ir(handoff),
        yvex_model_register_deepseek_v4()->payload.binding(handoff),
        yvex_model_register_deepseek_v4()->payload.map(handoff),
        YVEX_QUANT_PROFILE_RELEASE_Q8_Q2, NULL, &failure, &error);
    if (rc != YVEX_OK) {
        quant_print_terminal_context(handoff, failure.terminal_ordinal);
        fprintf(stderr,
                "quant_plan_failure=%s terminal=%llu source=%llu qtype=%u operation=%u expected=%llu actual=%llu status=%s where=%s\n",
                yvex_quant_failure_name(failure.code),
                failure.terminal_ordinal, failure.source_index, failure.qtype,
                (unsigned int)failure.operation, failure.expected,
                failure.actual,
                yvex_status_name(yvex_error_code(&error)),
                yvex_error_where(&error));
        yvex_model_register_deepseek_v4()->payload.close(handoff);
        return 1;
    }
    summary = yvex_quant_plan_summary_get(plan);
    transform_summary = yvex_transform_ir_summary_get(
        yvex_model_register_deepseek_v4()->payload.transform_ir(handoff));
    verification = yvex_model_register_deepseek_v4()->payload.verification(handoff);
    if (!quant_plan_invariants(summary) || !transform_summary ||
        !verification ||
        strcmp(summary->transform_identity,
               transform_summary->transform_identity) != 0 ||
        strcmp(summary->required_payload_identity,
               transform_summary->required_payload_identity) != 0) {
        fprintf(stderr, "quant_plan_invariant=failed\n");
        yvex_quant_plan_release(&plan);
        yvex_model_register_deepseek_v4()->payload.close(handoff);
        return 1;
    }
    memset(&execution, 0, sizeof(execution));
    memset(&digest_summary, 0, sizeof(digest_summary));
    memset(&source_facts, 0, sizeof(source_facts));
    if (!plan_only) {
        rc = yvex_quant_digest_sink_create(
            &digest_sink, plan, summary->required_payload_identity,
            &failure, &error);
        if (rc != YVEX_OK) {
            fprintf(stderr, "digest_sink_failure=%s status=%s\n",
                    yvex_quant_failure_name(failure.code),
                    yvex_status_name(yvex_error_code(&error)));
            yvex_quant_plan_release(&plan);
            yvex_model_register_deepseek_v4()->payload.close(handoff);
            return 1;
        }
        yvex_quant_digest_sink_adapter(digest_sink, &output_sink);
        yvex_quant_executor_options_default(&executor_options);
        executor_options.worker_count = options.budget.maximum_streams;
        executor_options.maximum_owned_bytes = 64u * 1024u * 1024u;
        clock_gettime(CLOCK_MONOTONIC, &begin);
        rc = yvex_quant_execute(
            plan, &output_sink, &executor_options, &execution,
            &failure, &error);
        clock_gettime(CLOCK_MONOTONIC, &end);
        elapsed = quant_elapsed_ns(&begin, &end);
        if (rc == YVEX_OK)
            rc = yvex_quant_digest_sink_finalize(
                digest_sink, &digest_summary, &failure, &error);
        if (rc != YVEX_OK) {
            quant_print_terminal_context(handoff, failure.terminal_ordinal);
            fprintf(stderr,
                    "quant_execution_failure=%s terminal=%llu source=%llu row=%llu block=%llu expected=%llu actual=%llu qtype=%u operation=%u status=%s where=%s\n",
                    yvex_quant_failure_name(failure.code),
                    failure.terminal_ordinal, failure.source_index,
                    failure.row_index, failure.block_index,
                    failure.expected, failure.actual, failure.qtype,
                    (unsigned int)failure.operation,
                    yvex_status_name(yvex_error_code(&error)),
                    yvex_error_where(&error));
            yvex_quant_digest_sink_release(&digest_sink);
            yvex_quant_plan_release(&plan);
            yvex_model_register_deepseek_v4()->payload.close(handoff);
            return 1;
        }
        if (!execution.complete || !digest_summary.complete ||
            execution.terminals_executed != summary->decision_count ||
            execution.source_values_consumed != summary->source_value_count ||
            execution.encoded_output_bytes != summary->encoded_bytes ||
            digest_summary.encoded_bytes != summary->encoded_bytes ||
            digest_summary.committed_terminals != summary->decision_count ||
            digest_summary.aborted_terminals != 0u) {
            fprintf(stderr, "quant_execution_invariant=failed\n");
            yvex_quant_digest_sink_release(&digest_sink);
            yvex_quant_plan_release(&plan);
            yvex_model_register_deepseek_v4()->payload.close(handoff);
            return 1;
        }
    }
    if (yvex_source_payload_session_facts_get(
            yvex_model_register_deepseek_v4()->payload.session(handoff), &source_facts,
            &error) != YVEX_OK) {
        yvex_quant_digest_sink_release(&digest_sink);
        yvex_quant_plan_release(&plan);
        yvex_model_register_deepseek_v4()->payload.close(handoff);
        return 1;
    }
    printf("mode=%s\n", plan_only ? "plan-only" : "execute-discard");
    printf("source_snapshot_identity=%016llx\n",
           summary->source_snapshot_identity);
    printf("required_payload_identity=%s\n",
           summary->required_payload_identity);
    printf("payload_identity=%s\n", source_facts.payload_identity);
    printf("payload_trust_class=%s\n",
           yvex_source_payload_trust_class_name(source_facts.trust_class));
    printf("transform_identity=%s\n", summary->transform_identity);
    printf("mapping_identity=%016llx\n", summary->mapping_identity);
    printf("profile_name=%s\n", summary->profile_name);
    printf("profile_schema=%u\n", summary->schema_version);
    printf("profile_identity=%s\n", summary->profile_identity);
    printf("terminal_decisions=%llu\n", summary->decision_count);
    printf("source_values=%llu\n", summary->source_value_count);
    printf("source_shards=%llu\n", verification->shard_count);
    printf("source_footprint_bytes=%llu\n", verification->shard_bytes);
    printf("transform_nodes=%llu\n", transform_summary->node_count);
    printf("transform_edges=%llu\n", transform_summary->edge_count);
    printf("transform_maximum_fan_in=%llu\n",
           transform_summary->maximum_fan_in);
    printf("transform_maximum_depth=%llu\n",
           transform_summary->maximum_depth);
    for (operation = 0u; operation < YVEX_TRANSFORM_OP_COUNT; ++operation)
        printf("operation_%s=%llu\n",
               yvex_transform_operation_name(
                   (yvex_transform_operation_kind)operation),
               transform_summary->operation_counts[operation]);
    printf("encoded_output_bytes=%llu\n", summary->encoded_bytes);
    printf("exact_scalar_bytes=%llu\n", summary->exact_scalar_bytes);
    printf("q8_0_bytes=%llu\n", summary->q8_0_bytes);
    printf("q2_k_bytes=%llu\n", summary->q2_k_bytes);
    printf("mxfp4_bytes=%llu\n", summary->mxfp4_bytes);
    printf("f32_tensors=%llu\n",
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_F32]);
    printf("f16_tensors=%llu\n",
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_F16]);
    printf("bf16_tensors=%llu\n",
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_BF16]);
    printf("i32_tensors=%llu\n",
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_I32]);
    printf("q8_0_tensors=%llu\n",
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_Q8_0]);
    printf("q2_k_tensors=%llu\n",
           summary->qtype_tensor_counts[YVEX_GGUF_QTYPE_Q2_K]);
    printf("reference_profile_bytes=%llu\n",
           summary->candidates[0].encoded_bytes);
    printf("release_profile_bytes=%llu\n",
           summary->candidates[1].encoded_bytes);
    printf("calibration_required=%d\n", summary->calibration_required);
    printf("calibration_identity=%s\n", summary->calibration_identity);
    printf("backend_compute_contract=%s\n",
           summary->backend_compute_contract);
    printf("reference_profile_numeric_admissible=%d\n",
           summary->candidates[0].numerically_admissible);
    printf("reference_profile_compute_admissible=%d\n",
           summary->candidates[0].compute_admissible);
    printf("release_profile_numeric_admissible=%d\n",
           summary->candidates[1].numerically_admissible);
    printf("release_profile_compute_admissible=%d\n",
           summary->candidates[1].compute_admissible);
    printf("owned_bytes=%zu\n", summary->owned_bytes);
    printf("peak_builder_bytes=%zu\n", summary->peak_builder_bytes);
    printf("header_scans=%llu\n", verification->header_scan_count);
    printf("payload_bytes_read=%llu\n", summary->payload_bytes_read);
    printf("terminal_lowering_bijection=complete\n");
    printf("aggregate_execution_identity=%s\n",
           digest_summary.complete ? digest_summary.execution_identity
                                   : "not-executed");
    printf("terminals_executed=%llu\n", execution.terminals_executed);
    printf("source_values_consumed=%llu\n",
           execution.source_values_consumed);
    printf("source_ranges_read=%llu\n", execution.source_ranges_read);
    printf("execution_payload_bytes_read=%llu\n",
           execution.payload_bytes_read);
    printf("execution_encoded_bytes=%llu\n",
           execution.encoded_output_bytes);
    for (qtype = 0u; qtype <= YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID;
         ++qtype) {
        if (!summary->qtype_tensor_counts[qtype]) continue;
        printf("qtype_%u_plan_tensors=%llu\n", qtype,
               summary->qtype_tensor_counts[qtype]);
        printf("qtype_%u_executed_tensors=%llu\n", qtype,
               execution.qtype_tensor_counts[qtype]);
        printf("qtype_%u_output_bytes=%llu\n", qtype,
               execution.qtype_output_bytes[qtype]);
        printf("qtype_%u_max_absolute_error=%.17g\n", qtype,
               execution.qtype_metrics[qtype].maximum_absolute_error);
        printf("qtype_%u_rmse=%.17g\n", qtype,
               yvex_quant_metrics_rmse(&execution.qtype_metrics[qtype]));
        if (ULLONG_MAX - nonfinite_count <
                execution.qtype_metrics[qtype].nonfinite_count) {
            fprintf(stderr, "quant_nonfinite_accounting=overflow\n");
            yvex_quant_digest_sink_release(&digest_sink);
            yvex_quant_plan_release(&plan);
            yvex_model_register_deepseek_v4()->payload.close(handoff);
            return 1;
        }
        nonfinite_count += execution.qtype_metrics[qtype].nonfinite_count;
    }
    for (role = 1u; role < YVEX_TENSOR_ROLE_COUNT; ++role) {
        if (!summary->role_tensor_counts[role]) continue;
        printf("role_%u_name=%s\n", role,
               yvex_tensor_role_name((yvex_tensor_role)role));
        printf("role_%u_plan_tensors=%llu\n", role,
               summary->role_tensor_counts[role]);
        printf("role_%u_executed_tensors=%llu\n", role,
               execution.role_tensor_counts[role]);
        printf("role_%u_max_absolute_error=%.17g\n", role,
               execution.role_metrics[role].maximum_absolute_error);
        printf("role_%u_rmse=%.17g\n", role,
               yvex_quant_metrics_rmse(&execution.role_metrics[role]));
    }
    printf("execution_source_chunks=%llu\n", execution.source_chunks);
    printf("execution_output_chunks=%llu\n", execution.output_chunks);
    printf("reference_decode_elements=%llu\n",
           execution.reference_decode_elements);
    printf("q8_0_max_absolute_error=%.17g\n",
           execution.qtype_metrics[YVEX_GGUF_QTYPE_Q8_0]
               .maximum_absolute_error);
    printf("q8_0_rmse=%.17g\n",
           yvex_quant_metrics_rmse(
               &execution.qtype_metrics[YVEX_GGUF_QTYPE_Q8_0]));
    printf("q2_k_max_absolute_error=%.17g\n",
           execution.qtype_metrics[YVEX_GGUF_QTYPE_Q2_K]
               .maximum_absolute_error);
    printf("q2_k_rmse=%.17g\n",
           yvex_quant_metrics_rmse(
               &execution.qtype_metrics[YVEX_GGUF_QTYPE_Q2_K]));
    printf("numeric_bound_violations=%llu\n",
           execution.numeric_bound_violations);
    printf("nonfinite_count=%llu\n", nonfinite_count);
    printf("committed_terminals=%llu\n",
           execution.committed_terminals);
    printf("aborted_terminals=%llu\n", execution.aborted_terminals);
    printf("peak_owned_bytes=%zu\n", execution.peak_owned_bytes);
    printf("configured_memory_budget=%zu\n",
           execution.configured_memory_budget);
    printf("worker_count=%u\n", execution.configured_workers);
    printf("workers_started=%u\n", execution.workers_started);
    printf("short_reads=%llu\n", source_facts.short_reads);
    printf("payload_drift=%llu\n", source_facts.identity_drifts);
    printf("sink_protocol_failures=%llu\n", execution.sink_failures);
    printf("cancellations=%llu\n", execution.cancellations);
    printf("worker_failures=%llu\n", execution.worker_failures);
    printf("incomplete_terminals=%llu\n",
           execution.terminal_decisions - execution.terminals_executed);
    printf("elapsed_nanoseconds=%llu\n", elapsed);
    printf("source_bytes_per_second=%.3f\n",
           elapsed ? (double)execution.payload_bytes_read * 1e9 /
                         (double)elapsed : 0.0);
    printf("encoded_bytes_per_second=%.3f\n",
           elapsed ? (double)execution.encoded_output_bytes * 1e9 /
                         (double)elapsed : 0.0);
    yvex_quant_digest_sink_release(&digest_sink);
    yvex_quant_plan_release(&plan);
    yvex_model_register_deepseek_v4()->payload.close(handoff);
    return 0;
}
