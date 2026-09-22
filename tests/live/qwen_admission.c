/* Exact Qwen artifact/binding execution and common hybrid-prefix qualification. */
#include <yvex/internal/backend.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/decoder_execution.h>
#include <yvex/internal/generation.h>
#include <yvex/internal/graph_state.h>
#include <yvex/internal/logits.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/runtime_prefix.h>
#include <yvex/internal/sequence_state.h>
#include <yvex/tokenizer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QWEN_ARTIFACT_ID \
    "1fce07008eaa78e04eedd1a031144f48eb6af617f2b5c508811ba91dca7e00f1"
#define QWEN_TENSOR_COUNT 851ull
#define QWEN_LAYER_COUNT 64ull
#define QWEN_ATTENTION_LAYERS 16ull
#define QWEN_RECURRENT_LAYERS 48ull
#define QWEN_TOKENIZER_VOCABULARY 248077ull
#define QWEN_OUTPUT_VOCABULARY 248320ull
#define QWEN_PREFIX_BUDGET (4ull << 30u)

typedef struct {
    yvex_runtime_execution_session *session;
    yvex_runtime_decoder_execution_context *decoder;
    yvex_runtime_logits_context *logits;
    yvex_runtime_execution_profile profile;
    float *values;
    unsigned long long value_count;
    yvex_runtime_decoder_execution_result execution;
    yvex_runtime_logits_row_result row;
} qwen_run;

static int qwen_refuse(yvex_error *err, const char *reason)
{
    yvex_error_set(err, YVEX_ERR_STATE, "test.qwen-admission", reason);
    return YVEX_ERR_STATE;
}

static int qwen_legacy_binding_refusal(const char *path, yvex_error *err)
{
    yvex_runtime_binding *binding = NULL;
    yvex_runtime_binding_summary summary = {0};
    yvex_complete_artifact_admission admission = {0};
    yvex_runtime_binding_failure failure = {0};
    int rc = yvex_runtime_binding_open(
        &binding, path, &summary, &admission, &failure, err);

    yvex_runtime_binding_close(binding);
    if (rc == YVEX_OK || failure.code == YVEX_RUNTIME_BINDING_FAILURE_NONE ||
        !failure.field[0])
        return qwen_refuse(
            err, "retained malformed legacy binding did not fail closed");
    printf("qwen_legacy_binding path=%s status=%d failure_code=%u field=%s "
           "result=REFUSED\n",
           path, rc, (unsigned int)failure.code, failure.field);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int qwen_input_identity(unsigned int token, unsigned long long position,
                               char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.test.qwen-admission.input.v1") ||
        !yvex_sha256_update_u64(&hash, token) ||
        !yvex_sha256_update_u64(&hash, position) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int qwen_profile_reseal(
    yvex_runtime_execution_profile *profile, yvex_error *err)
{
    yvex_runtime_execution_profile source = *profile;
    yvex_runtime_execution_profile_request request = {
        .schema_version = source.schema_version,
        .engine_generation = source.engine_generation,
        .engine_specialization_identity = source.engine_specialization_identity,
        .kernel_bundle_identity = source.kernel_bundle_identity,
        .workload_profile_identity = source.workload_profile_identity,
        .generation_mode = source.generation_mode,
        .evidence = source.evidence,
        .execution_class = source.execution_class,
        .attention_resolution = source.attention_resolution,
        .moe_resolution = source.moe_resolution,
        .sampling_resolution = source.sampling_resolution};
    return yvex_runtime_execution_profile_seal(&request, profile, err);
}

static int qwen_profile(qwen_run *run, yvex_model_engine *model,
                        yvex_error *err)
{
    yvex_execution_workload_profile workload = {
        .schema_version = YVEX_EXECUTION_WORKLOAD_PROFILE_SCHEMA_V1,
        .kind = YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY,
        .minimum_session_context = 2ull,
        .requested_session_context = 4ull,
        .concurrent_sequences = 1ull,
        .logical_batch_tokens = 1ull,
        .prefill_chunk_tokens = 1ull,
        .attention_microbatch_rows = 1ull,
        .moe_row_tile = 1ull,
        .output_head_rows = 1ull,
        .system_reserve_bytes = YVEX_EXECUTION_MINIMUM_SYSTEM_RESERVE,
        .latency_priority = 1};
    yvex_runtime_execution_profile_derivation derivation = {0};
    yvex_runtime_execution_profile contradicted = {0};
    int rc;

    yvex_core_text_copy(workload.name, sizeof(workload.name),
                        "qwen-hybrid-prefix-admission");
    rc = yvex_execution_workload_profile_seal(&workload, err);
    if (rc != YVEX_OK) return rc;
    derivation.schema_version = YVEX_RUNTIME_EXECUTION_PROFILE_SCHEMA_V1;
    derivation.model = model;
    derivation.session = run->session;
    derivation.workload = &workload;
    derivation.backend = YVEX_BACKEND_KIND_CUDA;
    derivation.generation_mode = YVEX_EXECUTION_GENERATION_TARGET_ONLY;
    derivation.evidence = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
    derivation.sampling_requirement = YVEX_EXECUTION_SAMPLING_NOT_INVOKED;
    rc = yvex_runtime_execution_profile_derive(
        &derivation, &run->profile, err);
    if (rc == YVEX_OK &&
        (run->profile.execution_class != YVEX_EXECUTION_CLASS_DEVICE_NATIVE ||
         run->profile.generation_mode !=
             YVEX_EXECUTION_GENERATION_TARGET_ONLY ||
         run->profile.attention_resolution !=
             YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED ||
         run->profile.moe_resolution !=
             YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED ||
         run->profile.sampling_resolution != YVEX_EXECUTION_RESOLUTION_EXACT))
        return qwen_refuse(
            err, "derived Qwen CUDA execution posture is not admitted");
    contradicted = run->profile;
    contradicted.attention_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    if (rc == YVEX_OK &&
        (qwen_profile_reseal(&contradicted, err) != YVEX_OK ||
         yvex_runtime_execution_profile_admit(
             &contradicted, model, run->session, err) != YVEX_ERR_STATE))
        return qwen_refuse(err, "false exact Qwen attention was admitted");
    contradicted = run->profile;
    contradicted.moe_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    if (rc == YVEX_OK &&
        (qwen_profile_reseal(&contradicted, err) != YVEX_OK ||
         yvex_runtime_execution_profile_admit(
             &contradicted, model, run->session, err) != YVEX_ERR_STATE))
        return qwen_refuse(err, "false exact Qwen MoE was admitted");
    if (rc == YVEX_OK) yvex_error_clear(err);
    if (rc == YVEX_OK)
        printf("qwen_profile identity=%s kernel_bundle=%s class=%u "
               "attention=%u moe=%u sampling=%u result=PASS\n",
               run->profile.identity, run->profile.kernel_bundle_identity,
               (unsigned int)run->profile.execution_class,
               (unsigned int)run->profile.attention_resolution,
               (unsigned int)run->profile.moe_resolution,
               (unsigned int)run->profile.sampling_resolution);
    return rc;
}

static int qwen_capacity_configure(qwen_run *run, yvex_model_engine *model,
                                   yvex_error *err)
{
    yvex_runtime_generation_context *capacity_owner = NULL;
    yvex_runtime_generation_options options = {
        .schema_version = YVEX_RUNTIME_GENERATION_SCHEMA_V6,
        .backend = YVEX_BACKEND_KIND_CUDA,
        .mode = YVEX_GENERATION_MODE_TARGET_ONLY,
        .workload_kind = YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY,
        .context_capacity = 4ull,
        .prefill_chunk_tokens = 1ull,
        .maximum_new_tokens = 1ull,
        .maximum_output_bytes = 16ull,
        .evidence_profile = YVEX_EXECUTION_EVIDENCE_PRODUCTION,
        .sampling_policy = {
            .schema_version = YVEX_RUNTIME_SAMPLING_SCHEMA_V1,
            .strategy = YVEX_SAMPLING_STRATEGY_GREEDY,
            .temperature = 1.0,
            .top_p = 1.0,
            .typical_p = 1.0}};
    int rc = yvex_runtime_generation_context_open(
        &capacity_owner, model, run->session, &options, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_generation_context_close(&capacity_owner, err);
    return rc;
}

static int qwen_run_open(qwen_run *run, yvex_model_engine *model,
                         yvex_error *err)
{
    yvex_runtime_session_open_request session = {
        .backend = YVEX_BACKEND_KIND_CUDA};
    yvex_runtime_decoder_execution_options decoder = {
        .context_capacity = 4ull, .token_capacity = 1ull};
    yvex_runtime_logits_options logits = {
        .maximum_rows = 1ull,
        .evidence_profile = YVEX_EXECUTION_EVIDENCE_PRODUCTION};
    yvex_model_engine_failure failure = {0};
    const yvex_runtime_logits_plan_summary *plan;
    int rc = yvex_runtime_session_open(
        &run->session, model, &session, &failure, err);
    if (rc == YVEX_OK) rc = qwen_capacity_configure(run, model, err);
    if (rc == YVEX_OK) rc = qwen_profile(run, model, err);
    decoder.execution_profile = &run->profile;
    logits.execution_profile = &run->profile;
    if (rc == YVEX_OK)
        rc = yvex_runtime_decoder_execution_context_open(
            &run->decoder, model, run->session, &decoder, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_context_open_program(
            &run->logits, model, run->session, &logits, err);
    plan = yvex_runtime_logits_plan_summary_get(run->logits);
    if (rc == YVEX_OK &&
        (!plan || plan->vocabulary_size != QWEN_OUTPUT_VOCABULARY))
        rc = qwen_refuse(err, "complete Qwen output vocabulary is unavailable");
    if (rc == YVEX_OK) {
        run->value_count = plan->vocabulary_size;
        run->values = malloc((size_t)run->value_count * sizeof(*run->values));
        if (!run->values)
            rc = qwen_refuse(err, "full-logits allocation failed");
    }
    return rc;
}

static int qwen_run_close(qwen_run *run, yvex_error *err)
{
    int rc = yvex_runtime_logits_context_close(&run->logits, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_decoder_execution_context_close(&run->decoder, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_close(&run->session, err);
    free(run->values);
    run->values = NULL;
    return rc;
}

static int qwen_run_token(qwen_run *run, unsigned int token,
                          unsigned long long position, yvex_error *err)
{
    yvex_runtime_decoder_execution_request request = {
        .token_ids = &token, .token_start = position, .token_count = 1ull};
    yvex_runtime_logits_source source = {0};
    char input_identity[YVEX_SHA256_HEX_CAP];
    unsigned long long index;
    int rc;

    memset(&run->execution, 0, sizeof(run->execution));
    memset(&run->row, 0, sizeof(run->row));
    if (!qwen_input_identity(token, position, input_identity))
        return qwen_refuse(err, "input identity could not seal");
    request.input_identity = input_identity;
    rc = yvex_runtime_decoder_execution_execute(
        run->decoder, &request, &run->execution, err);
    if (rc == YVEX_OK &&
        (!run->execution.completed ||
         run->execution.layers_executed != QWEN_LAYER_COUNT ||
         run->execution.attention_layers != QWEN_ATTENTION_LAYERS ||
         run->execution.recurrent_layers != QWEN_RECURRENT_LAYERS ||
         run->execution.position_after != position + 1ull))
        rc = qwen_refuse(err, "compiled hybrid forward coverage changed");
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_source_from_decoder(
            run->logits, &source, &run->execution,
            position ? YVEX_LOGITS_SOURCE_DECODE
                     : YVEX_LOGITS_SOURCE_PREFILL,
            0ull, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_project(
            run->logits, &source, YVEX_BACKEND_KIND_CUDA, run->values,
            run->value_count, &run->row, err);
    if (rc == YVEX_OK &&
        (!run->row.completed ||
         run->row.vocabulary_size != QWEN_OUTPUT_VOCABULARY ||
         run->row.logits_count != QWEN_OUTPUT_VOCABULARY ||
         !run->row.finite_count_available ||
         run->row.finite_count != QWEN_OUTPUT_VOCABULARY ||
         !yvex_sha256_hex_valid(run->row.output_head_plan_identity) ||
         !yvex_sha256_hex_valid(run->row.raw_logits_digest)))
        rc = qwen_refuse(err, "complete finite Qwen logits were not published");
    for (index = 0ull; rc == YVEX_OK && index < run->value_count; ++index)
        if (!isfinite(run->values[index]))
            rc = qwen_refuse(err, "Qwen logits contain a non-finite value");
    return rc;
}

static int qwen_tokenizer(const yvex_model_engine_view *view,
                          yvex_error *err)
{
    static const unsigned char text[] = "hello world";
    yvex_tokenizer_encode_options options = {
        .allow_special_tokens = 1, .maximum_tokens = 16ull};
    yvex_tokenizer_decode_options decode_options = {
        .require_complete_utf8 = 1};
    yvex_tokenizer_encode_result encoded = {0}, replay = {0};
    yvex_tokenizer_decode_result decoded = {0};
    const yvex_tokenizer_plan_summary *plan =
        view ? yvex_tokenizer_plan_summary_get(view->tokenizer) : NULL;
    int rc = YVEX_OK;

    if (!plan || !plan->runtime_bound ||
        plan->vocabulary_size != QWEN_TOKENIZER_VOCABULARY ||
        plan->bos_present ||
        !plan->eos_present || plan->eos_token_id != 248046u ||
        !plan->pad_present || plan->pad_token_id != 248044u ||
        plan->unk_present ||
        strcmp(plan->artifact_identity, QWEN_ARTIFACT_ID))
        return qwen_refuse(err, "exact Qwen tokenizer policy is unavailable");
    rc = yvex_tokenizer_encode(view->tokenizer, text, sizeof(text) - 1u,
                               &options, &encoded, err);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_decode(view->tokenizer, encoded.tokens.ids,
                                   encoded.tokens.len, &decode_options,
                                   &decoded, err);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_encode(view->tokenizer, decoded.bytes,
                                   decoded.byte_count, &options, &replay, err);
    if (rc == YVEX_OK &&
        (!encoded.completed || !encoded.tokens.len || !replay.completed ||
         encoded.tokens.len != replay.tokens.len ||
         memcmp(encoded.tokens.ids, replay.tokens.ids,
                (size_t)encoded.tokens.len * sizeof(*encoded.tokens.ids)) ||
         strcmp(encoded.tokenizer_plan_identity,
                replay.tokenizer_plan_identity)))
        rc = qwen_refuse(err, "Qwen tokenizer roundtrip changed token identity");
    if (rc == YVEX_OK)
        printf("qwen_tokenizer plan=%s vocabulary=%llu bos=absent eos=%u "
               "pad=%u unk=absent tokens=%llu roundtrip=exact\n",
               plan->tokenizer_plan_identity, plan->vocabulary_size,
               plan->eos_token_id, plan->pad_token_id, encoded.tokens.len);
    yvex_tokenizer_encode_result_clear(&replay);
    yvex_tokenizer_decode_result_clear(&decoded);
    yvex_tokenizer_encode_result_clear(&encoded);
    return rc;
}

static int qwen_state_identity(
    const qwen_run *run, yvex_graph_attention_state_summary *attention,
    yvex_sequence_state_summary *sequence,
    char recurrent_identity[YVEX_SHA256_HEX_CAP], yvex_error *err)
{
    const yvex_runtime_session_view *view =
        yvex_runtime_session_view_get(run->session);
    int rc;
    if (!view || !view->attention_state_provider || !view->sequence_state)
        return qwen_refuse(err, "hybrid attention and recurrent state are required");
    rc = view->attention_state_provider->summary(
        view->attention_state_provider->context, attention, err);
    if (rc == YVEX_OK)
        rc = yvex_sequence_state_summary_copy(
            view->sequence_state, sequence, err);
    if (rc == YVEX_OK)
        rc = yvex_sequence_state_committed_identity(
            view->sequence_state, recurrent_identity, err);
    return rc;
}

static int qwen_resource_evidence(const qwen_run *run, yvex_error *err)
{
    yvex_runtime_session_summary summary = {0};
    int rc = yvex_runtime_session_summary_copy(run->session, &summary, err);
    if (rc == YVEX_OK)
        printf("qwen_resources host_resident=%llu device_resident=%llu "
               "peak_host=%llu peak_device=%llu workspace=%llu "
               "device_workspace=%llu workspace_peak=%llu "
               "host_workspace=%llu host_workspace_peak=%llu "
               "attention_allocated=%llu attention_resident=%llu "
               "sequence_committed=%llu sequence_candidate=%llu "
               "sequence_host=%llu sequence_device=%llu "
               "sequence_recurrent=%llu sequence_convolution=%llu "
               "logits_bytes=%llu executions=%llu failures=%llu\n",
               summary.host_resident_bytes, summary.device_resident_bytes,
               summary.peak_host_bytes, summary.peak_device_bytes,
               summary.workspace_bytes, summary.device_workspace_bytes,
               summary.workspace_peak_bytes, summary.host_workspace_bytes,
               summary.host_workspace_peak_bytes,
               summary.attention_state_allocated_bytes,
               summary.attention_state_resident_bytes,
               summary.sequence_committed_state_bytes,
               summary.sequence_candidate_state_bytes,
               summary.sequence_host_state_bytes,
               summary.sequence_device_state_bytes,
               summary.sequence_recurrent_state_bytes,
               summary.sequence_convolution_state_bytes,
               run->value_count * sizeof(*run->values),
               summary.execution_count, summary.failure_count);
    return rc;
}

static int qwen_compare(const qwen_run *direct, const qwen_run *attached,
                        double *maximum, yvex_error *err)
{
    unsigned long long index;
    *maximum = 0.0;
    if (direct->value_count != attached->value_count)
        return qwen_refuse(err, "direct and attached vocabulary changed");
    for (index = 0ull; index < direct->value_count; ++index) {
        double difference = fabs((double)direct->values[index] -
                                 (double)attached->values[index]);
        if (difference > *maximum) *maximum = difference;
    }
    if (*maximum != 0.0)
        return qwen_refuse(err, "attached hybrid prefix changed full logits");
    return YVEX_OK;
}

static int qwen_hybrid_prefix(yvex_model_engine *model, yvex_error *err)
{
    qwen_run direct = {0}, attached = {0}, nonempty = {0};
    yvex_runtime_session_prefix *prefix = NULL;
    yvex_runtime_session_prefix_summary captured = {0}, attached_summary = {0};
    yvex_model_engine_failure failure = {0};
    yvex_graph_attention_state_summary source_attention = {0};
    yvex_graph_attention_state_summary source_after_branch = {0};
    yvex_graph_attention_state_summary direct_attention = {0};
    yvex_graph_attention_state_summary attached_attention = {0};
    yvex_sequence_state_summary source_sequence = {0};
    yvex_sequence_state_summary source_sequence_after_branch = {0};
    yvex_sequence_state_summary direct_sequence = {0};
    yvex_sequence_state_summary attached_sequence = {0};
    char source_recurrent[YVEX_SHA256_HEX_CAP] = {0};
    char source_recurrent_after_branch[YVEX_SHA256_HEX_CAP] = {0};
    char direct_recurrent[YVEX_SHA256_HEX_CAP] = {0};
    char attached_recurrent[YVEX_SHA256_HEX_CAP] = {0};
    double maximum = 0.0;
    int refusal, rc = qwen_run_open(&direct, model, err);

    if (rc == YVEX_OK) rc = qwen_run_token(&direct, 1u, 0ull, err);
    if (rc == YVEX_OK) rc = qwen_resource_evidence(&direct, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_prefix_capture(
            direct.session, QWEN_PREFIX_BUDGET, &prefix, &captured,
            &failure, err);
    if (rc == YVEX_OK &&
        (captured.schema_version != YVEX_RUNTIME_SESSION_PREFIX_SCHEMA_V2 ||
         captured.scope_count != 2ull ||
         captured.committed_sequence_length != 1ull ||
         !captured.target_prefix_identity[0] ||
         !captured.sequence_prefix_identity[0] ||
         !captured.sequence_state_binding_count))
        rc = qwen_refuse(err, "captured prefix is not hybrid attention plus recurrent state");
    if (rc == YVEX_OK) rc = qwen_run_open(&attached, model, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_prefix_attach(
            attached.session, prefix, &attached_summary, &failure, err);
    if (rc == YVEX_OK &&
        strcmp(captured.prefix_identity, attached_summary.prefix_identity))
        rc = qwen_refuse(err, "attached hybrid prefix identity changed");
    if (rc == YVEX_OK)
        rc = qwen_state_identity(&direct, &source_attention,
                                 &source_sequence, source_recurrent, err);
    if (rc == YVEX_OK) rc = qwen_run_token(&attached, 2u, 1ull, err);
    if (rc == YVEX_OK)
        rc = qwen_state_identity(&direct, &source_after_branch,
                                 &source_sequence_after_branch,
                                 source_recurrent_after_branch, err);
    if (rc == YVEX_OK &&
        (source_attention.committed_sequence_length != 1ull ||
         source_sequence.committed_position != 1ull ||
         strcmp(source_attention.state_content_identity,
                source_after_branch.state_content_identity) ||
         strcmp(source_recurrent, source_recurrent_after_branch) ||
         source_sequence_after_branch.committed_position != 1ull))
        rc = qwen_refuse(err, "attached branch mutated the source prefix state");
    if (rc == YVEX_OK) rc = qwen_run_token(&direct, 2u, 1ull, err);
    if (rc == YVEX_OK)
        rc = qwen_compare(&direct, &attached, &maximum, err);
    if (rc == YVEX_OK)
        rc = qwen_state_identity(&direct, &direct_attention,
                                 &direct_sequence, direct_recurrent, err);
    if (rc == YVEX_OK)
        rc = qwen_state_identity(&attached, &attached_attention,
                                 &attached_sequence, attached_recurrent, err);
    if (rc == YVEX_OK &&
        (direct_attention.committed_sequence_length != 2ull ||
         attached_attention.committed_sequence_length != 2ull ||
         direct_sequence.committed_position != 2ull ||
         attached_sequence.committed_position != 2ull ||
         strcmp(direct_attention.state_content_identity,
                attached_attention.state_content_identity) ||
         strcmp(direct_recurrent, attached_recurrent) ||
         strcmp(direct.execution.persistent_state_identity,
                attached.execution.persistent_state_identity)))
        rc = qwen_refuse(err, "hybrid continuation state changed after attach");
    if (rc == YVEX_OK) rc = qwen_resource_evidence(&direct, err);
    if (rc == YVEX_OK) rc = qwen_run_open(&nonempty, model, err);
    if (rc == YVEX_OK) rc = qwen_run_token(&nonempty, 1u, 0ull, err);
    if (rc == YVEX_OK) {
        yvex_runtime_session_prefix_summary rejected = {0};
        yvex_error expected = {0};
        refusal = yvex_runtime_session_prefix_attach(
            nonempty.session, prefix, &rejected, &failure, &expected);
        if (refusal != YVEX_ERR_STATE || rejected.prefix_identity[0])
            rc = qwen_refuse(err, "non-empty prefix destination was admitted");
    }
    if (rc == YVEX_OK)
        printf("qwen_prefix schema=%u identity=%s scopes=%llu position=%llu "
               "attention_identity=%s recurrent_identity=%s shared_bytes=%llu "
               "mapped_bytes=%llu sequence_bytes=%llu bindings=%llu "
               "output_head=%s logits_digest=%s replay_values=%llu "
               "finite_logits=%llu max_abs=%.17g tolerance=0 result=PASS\n",
               captured.schema_version, captured.prefix_identity,
               captured.scope_count, captured.committed_sequence_length,
               captured.target_prefix_identity,
               captured.sequence_prefix_identity, captured.shared_bytes,
               captured.mapped_bytes, captured.sequence_state_bytes,
               captured.sequence_state_binding_count,
               direct.row.output_head_plan_identity,
               direct.row.raw_logits_digest, direct.value_count,
               direct.row.finite_count, maximum);
    yvex_runtime_session_prefix_close(&prefix);
    {
        yvex_error cleanup = {0};
        int closed = qwen_run_close(&nonempty, &cleanup);
        if (rc == YVEX_OK && closed != YVEX_OK) { rc = closed; *err = cleanup; }
        closed = qwen_run_close(&attached, &cleanup);
        if (rc == YVEX_OK && closed != YVEX_OK) { rc = closed; *err = cleanup; }
        closed = qwen_run_close(&direct, &cleanup);
        if (rc == YVEX_OK && closed != YVEX_OK) { rc = closed; *err = cleanup; }
    }
    return rc;
}

int main(int argc, char **argv)
{
    yvex_model_engine_open_request request = {0};
    yvex_model_engine *model = NULL;
    yvex_model_engine_failure failure = {0};
    yvex_model_engine_summary summary = {0};
    const yvex_model_engine_view *view;
    const yvex_program_physical_summary *forward, *output;
    yvex_error err = {0};
    int rc;

    if (argc != 5) {
        fprintf(stderr,
                "usage: %s ARTIFACT RUNTIME_BINDING TARGET LEGACY_BINDING\n",
                argv[0]);
        return 2;
    }
    rc = qwen_legacy_binding_refusal(argv[4], &err);
    if (rc != YVEX_OK) goto done;
    request.artifact_path = argv[1];
    request.runtime_binding_path = argv[2];
    request.target_id = argv[3];
    request.residency_backend = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_model_engine_open(&model, &request, &failure, &err);
    if (rc == YVEX_OK)
        rc = yvex_model_engine_summary_copy(model, &summary, &err);
    view = yvex_model_engine_view_get(model);
    forward = view ? yvex_program_physical_summary_get(
                         yvex_compiled_model_plan_forward(view->compiled_plan))
                   : NULL;
    output = view ? yvex_program_physical_summary_get(
                        yvex_compiled_model_plan_output(view->compiled_plan))
                  : NULL;
    if (rc == YVEX_OK &&
        (!view || !view->binding ||
         view->binding->schema_version != YVEX_RUNTIME_BINDING_SCHEMA_CURRENT ||
         strcmp(summary.artifact_identity, QWEN_ARTIFACT_ID) ||
         summary.tensor_count != QWEN_TENSOR_COUNT ||
         summary.attention_layer_count != QWEN_ATTENTION_LAYERS ||
         view->binding->decoder_layer_count != QWEN_LAYER_COUNT ||
         view->binding->recurrent_layer_count != QWEN_RECURRENT_LAYERS ||
         !forward || !output))
        rc = qwen_refuse(&err, "exact current Qwen engine/program truth is unavailable");
    if (rc == YVEX_OK)
        printf("qwen_engine artifact=%s binding=%s model=%s generation=%llu "
               "binding_schema=%u tensors=%llu layers=%llu attention=%llu "
               "recurrent=%llu forward=%s forward_steps=%zu output=%s "
               "output_steps=%zu artifact_bytes=%llu mapped_bytes=%llu "
               "prepared_bytes=%llu host_bytes=%llu device_bytes=%llu\n",
               summary.artifact_identity, summary.runtime_binding_identity,
               summary.runtime_model_identity, summary.engine_generation,
               view->binding->schema_version, summary.tensor_count,
               view->binding->decoder_layer_count,
               summary.attention_layer_count,
               view->binding->recurrent_layer_count, forward->identity,
               forward->step_count, output->identity, output->step_count,
               summary.artifact_bytes, summary.mapped_package_bytes,
               summary.prepared_bytes, summary.resident_host_bytes,
               summary.resident_device_bytes);
    if (rc == YVEX_OK) rc = qwen_tokenizer(view, &err);
    if (rc == YVEX_OK) rc = qwen_hybrid_prefix(model, &err);
done:
    yvex_model_engine_close(&model);
    if (rc != YVEX_OK)
        fprintf(stderr, "qwen_admission status=%d where=%s reason=%s\n",
                rc, yvex_error_where(&err), yvex_error_message(&err));
    else
        puts("qwen_admission backend=cuda strategy=target-only "
             "sampling=not-invoked decision_readout=not-invoked cleanup=complete result=PASS");
    return rc == YVEX_OK ? 0 : 1;
}
