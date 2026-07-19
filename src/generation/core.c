/* Owner: src/generation
 * Owns: generation loop composition over available prefill, decode, logits, sampling, token append, stop, trace,
 *   cancel, and cleanup pieces.
 * Does not own: tensor role mapping, artifact emission, tokenizer training, benchmark, command grammar outside this
 *   command surface, server/provider generation, or release decisions.
 * Invariants: generated state remains bounded and diagnostic; partial output and cleanup behavior are explicit;
 *   trace/audit output must not imply supported-family runtime generation.
 * Boundary: diagnostic generation is not runtime generation over supported-family artifacts, eval evidence,
 *   benchmark evidence, throughput, or release readiness.
 * Purpose: Compose the bounded diagnostic generation protocol from admitted lower-stage components.
 * Inputs: A diagnostic engine, prompt tokens, sampling policy, limits, and report storage.
 * Effects: Advances only session-owned diagnostic state and an explicit trace transaction.
 * Failure: Failure or cancellation records a terminal refusal and releases all temporary state. */

#include <yvex/internal/core.h>
#include <yvex/internal/generation.h>
#include <yvex/internal/runtime.h>
#include <yvex/artifact.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int candidate_recorded;
    unsigned int token_id;
    double logit;
    unsigned long long sample_checksum;
    unsigned long long step;
    unsigned long long append_position;
} yvex_generation_append_state;

typedef struct {
    const char *phase;
    const char *decode;
    const char *logits;
    const char *sample;
    const char *append;
} generation_failure_projection;

typedef struct {
    const char *reason;
    const char *phase;
    const char *timing;
    int before_append;
    int after_append;
    int failure;
    int unsupported;
} generation_stop_policy;

static const generation_failure_projection generation_failure_projections[] = {
    {"decode", "fail", "skipped", "skipped", "not-started"},
    {"logits", "pass", "fail", "skipped", "not-started"},
    {"sample", "pass", "pass", "fail", "not-started"},
    {"append", "pass", "pass", "pass", "fail"},
};

static const generation_stop_policy generation_stop_failure = {
    "internal-error", "preflight", "failure", 0, 0, 1, 0,
};
static const generation_stop_policy generation_stop_cancel = {
    "interrupted", "stop-check", "cancel-safe-point", 0, 0, 0, 0,
};
static const generation_stop_policy generation_stop_context = {
    "context-limit", "stop-check", "pre-append", 1, 0, 0, 0,
};
static const generation_stop_policy generation_stop_max = {
    "max-new-tokens", "stop-check", "post-append", 0, 1, 0, 0,
};

static const yvex_generation_trace_step generation_trace_step_default = {
    .attempted = 1,
    .decode_status = "pending",
    .logits_status = "pending",
    .sample_status = "pending",
    .append_status = "not-started",
    .stop_reason = "none",
    .stop_timing = "none",
};

static const yvex_generation_trace_step generation_trace_candidate = {
    .decode_status = "pass",
    .logits_status = "pass",
    .sample_status = "pass",
    .append_status = "candidate",
};

static const yvex_generation_state generation_state_cancelled = {
    .lifecycle_status = "cancelled",
    .generation_state = "cancelled",
    .cancel_requested = 1,
    .cancel_reason = "interrupted",
    .cancel_step_seen = 1,
};

static const yvex_generation_state generation_state_cleaned = {
    .lifecycle_status = "cleaned",
    .cleanup_idempotent = 1,
    .cleanup_owned_state_released = 1,
    .failure_preserved = 1,
    .partial_output_preserved = 1,
};

static const yvex_engine_options generation_engine_default = {
    .load_tokenizer = 0,
    .build_descriptor = 1,
    .build_default_graph = 1,
    .attach_weights = 1,
    .require_all_weights = 1,
};

#define GENERATION_REQUEST_FIELD(field_)                                                \
    {                                                                                   \
        offsetof(yvex_decode_step_options, field_),                                     \
        offsetof(yvex_generation_request, field_),                                      \
        sizeof(((yvex_generation_request *)0)->field_) +                                \
            0u * sizeof(char[sizeof(((yvex_decode_step_options *)0)->field_) ==          \
                                    sizeof(((yvex_generation_request *)0)->field_)       \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection generation_request_fields[] = {
    GENERATION_REQUEST_FIELD(segment_name),
    GENERATION_REQUEST_FIELD(backend_name),
    GENERATION_REQUEST_FIELD(position_start),
    GENERATION_REQUEST_FIELD(chunk_size),
    GENERATION_REQUEST_FIELD(attach_kv),
    GENERATION_REQUEST_FIELD(kv_shape),
    GENERATION_REQUEST_FIELD(layer_count),
    GENERATION_REQUEST_FIELD(layer_hidden_dim),
    GENERATION_REQUEST_FIELD(layer_head_dim),
    GENERATION_REQUEST_FIELD(layer_ffn_dim),
};

#undef GENERATION_REQUEST_FIELD

#define SAMPLE_REPORT_FIELD(destination_, source_)                                      \
    {                                                                                   \
        offsetof(yvex_generation_report, destination_),                                 \
        offsetof(yvex_sampling_summary, source_),                                       \
        sizeof(((yvex_sampling_summary *)0)->source_) +                                 \
            0u * sizeof(char[sizeof(((yvex_generation_report *)0)->destination_) ==      \
                                    sizeof(((yvex_sampling_summary *)0)->source_)        \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection generation_candidate_fields[] = {
    SAMPLE_REPORT_FIELD(candidate_token_id, selected_token_id),
    SAMPLE_REPORT_FIELD(candidate_logit, selected_logit),
    SAMPLE_REPORT_FIELD(last_selected_token_id, selected_token_id),
    SAMPLE_REPORT_FIELD(last_selected_logit, selected_logit),
};

#undef SAMPLE_REPORT_FIELD

#define SAMPLE_TRACE_FIELD(destination_, source_)                                       \
    {                                                                                   \
        offsetof(yvex_generation_trace_step, destination_),                             \
        offsetof(yvex_sampling_summary, source_),                                       \
        sizeof(((yvex_sampling_summary *)0)->source_) +                                 \
            0u * sizeof(char[sizeof(((yvex_generation_trace_step *)0)->destination_) ==  \
                                    sizeof(((yvex_sampling_summary *)0)->source_)        \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection generation_trace_candidate_fields[] = {
    SAMPLE_TRACE_FIELD(selected_token_id, selected_token_id),
    SAMPLE_TRACE_FIELD(candidate_token_id, selected_token_id),
    SAMPLE_TRACE_FIELD(candidate_logit, selected_logit),
    SAMPLE_TRACE_FIELD(logits_checksum, logits_checksum),
    SAMPLE_TRACE_FIELD(logits_min, logits_min),
    SAMPLE_TRACE_FIELD(logits_max, logits_max),
    SAMPLE_TRACE_FIELD(sample_checksum, sample_checksum),
};

#undef SAMPLE_TRACE_FIELD

#define SAME_FIELD(type_, field_)                                                       \
    {offsetof(type_, field_), offsetof(type_, field_), sizeof(((type_ *)0)->field_)}

static const yvex_generation_field_projection generation_cancelled_fields[] = {
    SAME_FIELD(yvex_generation_state, lifecycle_status),
    SAME_FIELD(yvex_generation_state, generation_state),
    SAME_FIELD(yvex_generation_state, cancel_requested),
    SAME_FIELD(yvex_generation_state, cancel_reason),
    SAME_FIELD(yvex_generation_state, cancel_step_seen),
};

static const yvex_generation_field_projection generation_cleaned_fields[] = {
    SAME_FIELD(yvex_generation_state, lifecycle_status),
    SAME_FIELD(yvex_generation_state, cleanup_idempotent),
    SAME_FIELD(yvex_generation_state, cleanup_owned_state_released),
    SAME_FIELD(yvex_generation_state, failure_preserved),
    SAME_FIELD(yvex_generation_state, partial_output_preserved),
};

static const yvex_generation_field_projection generation_candidate_status_fields[] = {
    SAME_FIELD(yvex_generation_trace_step, decode_status),
    SAME_FIELD(yvex_generation_trace_step, logits_status),
    SAME_FIELD(yvex_generation_trace_step, sample_status),
    SAME_FIELD(yvex_generation_trace_step, append_status),
};

#undef SAME_FIELD

#define SUMMARY_TRACE_FIELD(field_)                                                     \
    {                                                                                   \
        offsetof(yvex_generation_trace_step, field_),                                   \
        offsetof(yvex_generation_report, field_),                                       \
        sizeof(((yvex_generation_report *)0)->field_) +                                 \
            0u * sizeof(char[sizeof(((yvex_generation_trace_step *)0)->field_) ==        \
                                    sizeof(((yvex_generation_report *)0)->field_)        \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection generation_trace_account_fields[] = {
    SUMMARY_TRACE_FIELD(accepted_token_count),
    SUMMARY_TRACE_FIELD(generated_token_count),
    SUMMARY_TRACE_FIELD(total_token_count),
    SUMMARY_TRACE_FIELD(sequence_checksum),
};

#undef SUMMARY_TRACE_FIELD

/* Purpose: enforce the sampling report's exact registered-alias snapshot rule.
 * Inputs: resolved immutable model reference and caller error storage.
 * Effects: reads only the referenced artifact identity and updates error state.
 * Failure: missing or drifted alias identity returns the historical typed refusal.
 * Boundary: sampling-report admission; other callers use canonical model-ref identity. */
static int generation_sampling_identity(const yvex_model_ref *ref,
                                        yvex_error *err)
{
    yvex_artifact_file_identity identity;
    int status;

    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) return YVEX_OK;
    memset(&identity, 0, sizeof(identity));
    status = yvex_artifact_identity_read(ref->path, &identity, err);
    if (status != YVEX_OK) return status;
    if (!ref->sha256 || !ref->sha256[0] || !yvex_sha256_hex_is_valid(ref->sha256)) {
        return yvex_generation_refuse(
            err, YVEX_ERR_STATE, "sampling_identity",
            "registered alias lacks digest identity; re-add model");
    }
    if (strcmp(ref->sha256, identity.sha256) != 0 ||
        (ref->registered_file_size != 0ull &&
         ref->registered_file_size != identity.file_size)) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "sampling_identity",
                                      "digest mismatch for registered alias");
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: admit one shared diagnostic generation execution session.
 * Inputs: model/backend/token facts and the caller's exact identity policy.
 * Effects: resolves a model, validates input, performs graph admission, and owns an engine.
 * Failure: stage identifies the first failed gate; acquired owners remain closeable.
 * Boundary: shared diagnostic admission only; it does not execute generation stages. */
int yvex_generation_admission_prepare(
    yvex_generation_admission *admission,
    const char *model_arg,
    const char *tokens_text,
    yvex_generation_identity_policy identity_policy,
    yvex_error *err)
{
    yvex_model_ref_identity_result identity_result;
    unsigned long long vocab_size = 0ull;
    int status;

    if (!admission) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "generation_admission", "admission is required");
    }
    memset(admission, 0, sizeof(*admission));
    admission->stage = YVEX_GENERATION_ADMISSION_RESOLVE;
    status = yvex_model_ref_resolve(&admission->model_ref, model_arg, NULL, err);
    if (status != YVEX_OK) return status;
    admission->stage = YVEX_GENERATION_ADMISSION_IDENTITY;
    if (identity_policy == YVEX_GENERATION_IDENTITY_SAMPLING_ALIAS) {
        status = generation_sampling_identity(&admission->model_ref, err);
    } else {
        memset(&identity_result, 0, sizeof(identity_result));
        status = yvex_model_ref_identity_validate(
            &admission->model_ref, &identity_result, err);
        if (status != YVEX_OK) {
            yvex_error_set(err, status, "generate",
                           "registered model identity check failed");
        }
    }
    if (status != YVEX_OK) return status;
    admission->stage = YVEX_GENERATION_ADMISSION_TOKENS;
    status = yvex_token_input_parse_explicit(tokens_text, &admission->token_input, err);
    if (status == YVEX_OK) {
        status = yvex_model_context_vocab_size(
            admission->model_ref.path, &vocab_size, err);
    }
    if (status == YVEX_OK) {
        status = yvex_token_input_validate_bounds(&admission->token_input, vocab_size, err);
    }
    if (status != YVEX_OK) return status;
    return status;
}

/* Purpose: complete graph and engine admission after caller-specific preflight facts.
 * Inputs: prepared admission and requested backend.
 * Effects: performs graph admission and owns one engine on success.
 * Failure: stage distinguishes graph from engine refusal and remains closeable.
 * Boundary: shared diagnostic resource admission; no token execution. */
int yvex_generation_admission_engine_open(
    yvex_generation_admission *admission,
    const char *backend_name,
    yvex_error *err)
{
    yvex_engine_options options = generation_engine_default;
    yvex_cli_graph_guard_report guard;
    int status;

    if (!admission || admission->stage != YVEX_GENERATION_ADMISSION_TOKENS) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "generation_admission",
                                      "prepared admission is required");
    }
    admission->stage = YVEX_GENERATION_ADMISSION_GRAPH;
    memset(&guard, 0, sizeof(guard));
    status = yvex_graph_preflight(
        &admission->model_ref, backend_name, 0, 1, admission->token_input.tokens[0],
        &guard, err);
    admission->graph_guard_status = guard.guard_status;
    admission->graph_guard_phase = guard.phase;
    if (status != YVEX_OK) return status;
    admission->stage = YVEX_GENERATION_ADMISSION_ENGINE;
    options.model_path = admission->model_ref.path;
    options.backend_name = backend_name;
    status = yvex_engine_open(&admission->engine, &options, err);
    if (status == YVEX_OK) admission->stage = YVEX_GENERATION_ADMISSION_READY;
    return status;
}

/* Purpose: release every owner acquired by one shared generation admission.
 * Inputs: optional admission in any partial construction stage.
 * Effects: closes engine/model reference and resets the caller-owned envelope.
 * Failure: idempotent for null, empty, and already-closed admissions.
 * Boundary: shared diagnostic admission lifecycle only. */
void yvex_generation_admission_close(yvex_generation_admission *admission)
{
    if (!admission) return;
    yvex_engine_close(admission->engine);
    yvex_model_ref_clear(&admission->model_ref);
    memset(admission, 0, sizeof(*admission));
}

/* Purpose: Fold hash float into the canonical deterministic evidence identity.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
unsigned long long yvex_generation_hash_float(unsigned long long hash,
                                              double value)
{
    float narrowed = (float)value;
    uint32_t bits = 0u;

    memcpy(&bits, &narrowed, sizeof(bits));
    return yvex_core_hash_mix_u64(hash, (unsigned long long)bits);
}

/* Purpose: Implement the canonical state id for input mechanism owned by the generation boundary. */
static unsigned long long generate_state_id_for_input(const yvex_token_input *input,
                                                      unsigned long long position_start,
                                                      unsigned long long max_new_tokens)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    hash = yvex_core_hash_mix_u64(hash, position_start);
    hash = yvex_core_hash_mix_u64(hash, max_new_tokens);
    hash = yvex_core_hash_mix_u64(hash, input ? input->token_count : 0ull);
    if (input) {
        for (i = 0ull; i < input->token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            hash = yvex_core_hash_mix_u64(hash, (unsigned long long)input->tokens[i]);
        }
    }
    return hash ? hash : 1ull;
}

static const yvex_generation_state generation_state_default = {
    .lifecycle_status = "created",
    .generation_state = "created",
    .cancel_supported = 1,
    .cancel_reason = "none",
    .cancel_timing = "none",
    .cancel_safe_point = "none",
    .cleanup_idempotent = 1,
    .failure_preserved = 1,
    .partial_output_preserved = 1
};

static const yvex_generation_report generation_report_default = {
    .loop_created = 1,
    .phase = "created",
    .status = "generation-loop",
    .token_input_status = "fail",
    .append_status = "not-started",
    .append_failure = "none",
    .stop_policy = "bounded-diagnostic",
    .stop_reason = "internal-error",
    .stop_phase = "preflight",
    .stop_timing = "preflight",
    .eos_policy = "unsupported",
    .stop_token_policy = "unsupported",
    .generation_checksum = 1469598103934665603ull,
    .sequence_checksum = 1469598103934665603ull,
    .cleanup_status = "not-needed",
    .failed_phase = "none"
};

/* Purpose: Publish state set lifecycle only within its admitted destination range. */
static void generate_state_set_lifecycle(yvex_generation_report *summary,
                                         const char *lifecycle_status)
{
    if (summary && lifecycle_status) {
        summary->state.lifecycle_status = lifecycle_status;
    }
}

/* Purpose: Implement the canonical state mark terminal mechanism owned by the generation boundary. */
static void generate_state_mark_terminal(yvex_generation_report *summary,
                                         const char *state)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = state;
    summary->state.generation_state = state;
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

/* Purpose: Implement the canonical summary defaults mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_summary_defaults(yvex_generation_report *summary,
                                      const yvex_token_input *input,
                                      unsigned long long position_start,
                                      unsigned long long max_new_tokens,
                                      yvex_generation_trace_level trace_level,
                                      int cancel_after_steps_seen,
                                      unsigned long long cancel_after_steps)
{
    unsigned long long i;

    if (!summary) {
        return;
    }
    *summary = generation_report_default;
    summary->prompt_token_count = input ? input->token_count : 0ull;
    summary->prefill_token_count = input ? input->token_count : 0ull;
    summary->max_new_tokens = max_new_tokens;
    summary->total_token_count = input ? input->token_count : 0ull;
    summary->position_start = position_start;
    if (input && input->token_count > 0ull) {
        unsigned long long offset = input->token_count - 1ull;
        if (yvex_core_u64_add(position_start, offset,
                                    &summary->prefill_position_end)) {
            (void)yvex_core_u64_add(summary->prefill_position_end, 1ull,
                                          &summary->current_decode_position);
            summary->last_successful_position = summary->prefill_position_end;
        }
    }
    if (input) {
        for (i = 0ull; i < input->token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            summary->prompt_tokens[i] = input->tokens[i];
            summary->sequence_checksum = yvex_core_hash_mix_u64(summary->sequence_checksum,
                                                          (unsigned long long)input->tokens[i]);
        }
    }
    summary->state = generation_state_default;
    summary->state.state_id =
        generate_state_id_for_input(input, position_start, max_new_tokens);
    summary->state.cancel_after_steps_seen = cancel_after_steps_seen ? 1 : 0;
    summary->state.cancel_after_steps = cancel_after_steps;
    summary->trace_level = trace_level;
    summary->trace_level_name = yvex_generation_trace_level_name(trace_level);
    summary->trace_enabled = trace_level != YVEX_GENERATION_TRACE_NONE;
    summary->trace_status = summary->trace_enabled ? "enabled" : "disabled";
}

/* Purpose: Implement the canonical trace step at mechanism owned by the generation boundary. */
static yvex_generation_trace_step *generate_trace_step_at(
    yvex_generation_report *summary,
    unsigned long long step)
{
    if (!summary || step >= YVEX_TOKEN_INPUT_MAX_TOKENS) {
        return NULL;
    }
    if (step >= summary->trace_step_count) {
        summary->trace_step_count = step + 1ull;
    }
    return &summary->trace_step_records[step];
}

/* Purpose: Implement the canonical trace step begin mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static yvex_generation_trace_step *generate_trace_step_begin(
    yvex_generation_report *summary,
    unsigned long long step,
    unsigned long long position)
{
    yvex_generation_trace_step *record = generate_trace_step_at(summary, step);

    if (!record) {
        return NULL;
    }
    *record = generation_trace_step_default;
    record->index = step;
    record->decode_position = position;
    return record;
}

/* Purpose: Implement the canonical trace step mark stop mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_trace_step_mark_stop(yvex_generation_report *summary,
                                          const char *reason,
                                          const char *timing,
                                          unsigned long long step,
                                          int before_append)
{
    yvex_generation_trace_step *record;

    if (!summary || step >= summary->trace_step_count) {
        return;
    }
    record = &summary->trace_step_records[step];
    if (!record->attempted) {
        return;
    }
    record->stop_reason = reason ? reason : "internal-error";
    record->stop_timing = timing ? timing : "failure";
    if (before_append && record->decode_status &&
        strcmp(record->decode_status, "pending") == 0) {
        record->decode_status = "skipped";
        record->logits_status = "skipped";
        record->sample_status = "skipped";
    }
    if (before_append && (!record->append_status ||
                          strcmp(record->append_status, "not-started") == 0 ||
                          strcmp(record->append_status, "candidate") == 0)) {
        record->append_status = reason && strcmp(reason, "interrupted") == 0 ?
            "cancelled" : "context-limit";
    }
}

/* Purpose: Implement the canonical trace step mark failure mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_trace_step_mark_failure(yvex_generation_report *summary,
                                             const char *phase,
                                             unsigned long long step)
{
    size_t index;
    yvex_generation_trace_step *record;

    if (!summary || step >= summary->trace_step_count) {
        return;
    }
    record = &summary->trace_step_records[step];
    if (!record->attempted) {
        return;
    }
    for (index = 0;
         index < sizeof(generation_failure_projections) /
                     sizeof(generation_failure_projections[0]);
         ++index) {
        const generation_failure_projection *projection =
            &generation_failure_projections[index];

        if (phase && strcmp(phase, projection->phase) == 0) {
            record->decode_status = projection->decode;
            record->logits_status = projection->logits;
            record->sample_status = projection->sample;
            record->append_status = projection->append;
            return;
        }
    }
    record->append_status = "fail";
}

/* Purpose: Execute the typed stop record operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_stop_record(yvex_generation_report *summary,
                                 const generation_stop_policy *policy,
                                 const char *reason,
                                 const char *phase,
                                 unsigned long long step)
{
    if (!summary || !policy) {
        return;
    }
    summary->stop_requested = 1;
    summary->stop_reason = reason ? reason : policy->reason;
    summary->stop_phase = phase ? phase : policy->phase;
    summary->stop_step = step;
    summary->stop_timing = policy->timing;
    summary->stop_before_append = policy->before_append;
    summary->stop_after_append = policy->after_append;
    summary->failure_stop = policy->failure;
    summary->unsupported_stop_feature = policy->unsupported;
    generate_trace_step_mark_stop(summary, summary->stop_reason, summary->stop_timing,
                                  step, policy->before_append);
}

/* Purpose: publish one admitted terminal completion and its immutable stop policy. */
static void generate_complete(yvex_generation_report *summary,
                              const generation_stop_policy *policy,
                              unsigned long long step)
{
    summary->status = "generation-loop-complete";
    summary->phase = "complete";
    generate_state_mark_terminal(summary, "completed");
    generate_stop_record(summary, policy, NULL, NULL, step);
}

/* Purpose: record idempotent diagnostic cleanup state for the generation summary.
 * Inputs: summary is borrowed mutable diagnostic state.
 * Effects: mutates lifecycle/cleanup flags only; it does not free external model
 * ownership or change backend/device allocations.
 * Failure: no failure path; missing summaries are ignored by callers before use.
 * Boundary: cleanup accounting is not proof of runtime generation support or release readiness. */
static void generate_mark_cleanup(yvex_generation_report *summary)
{
    if (!summary) {
        return;
    }
    if (summary->cleanup_attempted) {
        summary->state.cleanup_repeated = 1;
        summary->cleanup_status = "al" "rea" "dy-cleaned";
        summary->state.lifecycle_status = "cleaned";
        return;
    }
    summary->cleanup_attempted = 1;
    summary->cleanup_status = "pass";
    yvex_generation_project_fields(
        &summary->state, &generation_state_cleaned, generation_cleaned_fields,
        sizeof(generation_cleaned_fields) / sizeof(generation_cleaned_fields[0]));
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

/* Purpose: Implement the canonical mark failure mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_mark_failure(yvex_generation_report *summary,
                                  const char *phase,
                                  const char *reason,
                                  unsigned long long step)
{
    if (!summary) {
        return;
    }
    summary->phase = "failed";
    summary->status = "generation-loop-failed";
    summary->failed_phase = phase ? phase : "internal-error";
    summary->failed_step = step;
    summary->partial_generated_token_count = summary->generated_token_count;
    generate_state_mark_terminal(summary, "failed");
    generate_stop_record(summary, &generation_stop_failure, reason, phase, step);
    if (phase && strcmp(phase, "append") == 0) {
        summary->append_status = "append-failed";
        summary->append_failure = reason ? reason : "append-failure";
    }
    generate_trace_step_mark_failure(summary, phase, step);
}

/* Purpose: Implement the canonical mark cancel mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_mark_cancel(yvex_generation_report *summary,
                                 unsigned long long step,
                                 unsigned long long cancel_step,
                                 const char *timing,
                                 const char *safe_point,
                                 int before_append,
                                 int after_append)
{
    generation_stop_policy policy = generation_stop_cancel;

    if (!summary) {
        return;
    }
    summary->phase = "cancelled";
    summary->status = "generation-loop-cancelled";
    summary->partial_generated_token_count = summary->generated_token_count;
    if (before_append) {
        summary->append_status = "cancelled";
        summary->append_failure = "none";
    }
    yvex_generation_project_fields(
        &summary->state, &generation_state_cancelled, generation_cancelled_fields,
        sizeof(generation_cancelled_fields) / sizeof(generation_cancelled_fields[0]));
    summary->state.cancel_step = cancel_step;
    summary->state.cancel_timing = timing ? timing : "stop-check";
    summary->state.cancel_safe_point = safe_point ? safe_point : "stop-check";
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
    policy.before_append = before_append ? 1 : 0;
    policy.after_append = after_append ? 1 : 0;
    generate_stop_record(summary, &policy, NULL, NULL, step);
}

/* Purpose: Select sample failure phase deterministically from admitted logits and sampling policy. */
static const char *generate_sample_failure_phase(const yvex_sampling_summary *sample)
{
    if (!sample) {
        return "sample";
    }
    if (sample->logits_phase && strcmp(sample->logits_phase, "decode") == 0) {
        return "decode";
    }
    if (sample->sampling_phase &&
        (strcmp(sample->sampling_phase, "after-logits") == 0 ||
         strcmp(sample->sampling_phase, "select") == 0 ||
         strcmp(sample->sampling_phase, "after-select") == 0)) {
        return "sample";
    }
    return "logits";
}

/* Purpose: Select sample failure reason deterministically from admitted logits and sampling policy. */
static const char *generate_sample_failure_reason(const yvex_sampling_summary *sample)
{
    const char *phase = generate_sample_failure_phase(sample);

    if (strcmp(phase, "decode") == 0) {
        return "decode-failure";
    }
    if (strcmp(phase, "sample") == 0) {
        return "sampler-failure";
    }
    return "logits-failure";
}

/* Purpose: project one sampled candidate into summary and trace facts.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_project_candidate(yvex_generation_report *summary,
                                       const yvex_sampling_summary *sample,
                                       unsigned long long step)
{
    yvex_generation_trace_step *record;

    yvex_generation_project_fields(
        summary, sample, generation_candidate_fields,
        sizeof(generation_candidate_fields) / sizeof(generation_candidate_fields[0]));
    summary->candidate_token_seen = 1;
    summary->append_status = "candidate-" "rea" "dy";
    if (step >= summary->trace_step_count) {
        return;
    }
    record = &summary->trace_step_records[step];
    yvex_generation_project_fields(
        record, sample, generation_trace_candidate_fields,
        sizeof(generation_trace_candidate_fields) /
            sizeof(generation_trace_candidate_fields[0]));
}

/* Purpose: Select account failed sample deterministically from admitted logits and sampling policy.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_account_failed_sample(yvex_generation_report *summary,
                                           const yvex_sampling_summary *sample,
                                           unsigned long long step)
{
    if (!summary || !sample) {
        return;
    }
    if (sample->logits_phase &&
        (strcmp(sample->logits_phase, "after-decode") == 0 ||
         strcmp(sample->logits_phase, "allocation") == 0 ||
         strcmp(sample->logits_phase, "fill") == 0 ||
         strcmp(sample->logits_phase, "complete") == 0)) {
        summary->decode_steps += 1ull;
    }
    if (sample->logits_buffer_created) {
        yvex_generation_trace_step *record;

        if (summary->decode_steps == 0ull) {
            summary->decode_steps += 1ull;
        }
        summary->logits_steps += 1ull;
        if (step < summary->trace_step_count) {
            record = &summary->trace_step_records[step];
            record->logits_checksum = sample->logits_checksum;
            record->logits_min = sample->logits_min;
            record->logits_max = sample->logits_max;
            yvex_generation_project_fields(
                record, summary, generation_trace_account_fields,
                sizeof(generation_trace_account_fields) /
                    sizeof(generation_trace_account_fields[0]));
        }
    }
    if (sample->sample_created) {
        generate_project_candidate(summary, sample, step);
    }
}

/* Purpose: Implement the canonical record candidate mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void generate_record_candidate(yvex_generation_report *summary,
                                      yvex_generation_append_state *append,
                                      const yvex_sampling_summary *sample,
                                      unsigned long long step,
                                      unsigned long long append_position)
{
    if (!summary || !append || !sample) {
        return;
    }
    memset(append, 0, sizeof(*append));
    append->candidate_recorded = 1;
    append->token_id = sample->selected_token_id;
    append->logit = sample->selected_logit;
    append->sample_checksum = sample->sample_checksum;
    append->step = step;
    append->append_position = append_position;
    generate_project_candidate(summary, sample, step);
    summary->append_failure = "none";
    if (step < summary->trace_step_count) {
        yvex_generation_trace_step *record = &summary->trace_step_records[step];

        yvex_generation_project_fields(
            record, &generation_trace_candidate, generation_candidate_status_fields,
            sizeof(generation_candidate_status_fields) /
                sizeof(generation_candidate_status_fields[0]));
    }
}

/* Purpose: Append append preflight while preserving checked capacity and deterministic order.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int generate_append_preflight(yvex_generation_report *summary,
                                     const yvex_token_input *sequence,
                                     const yvex_generation_append_state *append,
                                     unsigned long long context_length,
                                     unsigned long long step,
                                     int *context_stop,
                                     yvex_error *err)
{
    unsigned long long expected_total;
    unsigned long long next_position;

    if (context_stop) {
        *context_stop = 0;
    }
    if (!summary || !sequence || !append) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "generate_append",
                                      "generation append state is required");
    }
    if (!append->candidate_recorded || !summary->candidate_token_seen) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "generate_append",
                                      "candidate token is required before append");
    }
    if (summary->generated_token_count != summary->accepted_token_count ||
        summary->generated_token_count != summary->append_steps ||
        summary->generated_token_count != summary->partial_generated_token_count) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "generate_append",
                                      "generated token accounting is inconsistent");
    }
    if (summary->generated_token_count >= summary->max_new_tokens) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "generate_append",
                                      "append would exceed requested max-new-tokens");
    }
    if (!yvex_core_u64_add(summary->prompt_token_count,
                                 summary->generated_token_count, &expected_total) ||
        expected_total != sequence->token_count ||
        expected_total != summary->total_token_count) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "generate_append",
                                      "runtime token sequence accounting is inconsistent");
    }
    if (summary->current_decode_position >= context_length) {
        if (context_stop) {
            *context_stop = 1;
        }
        summary->append_status = "context-limit";
        summary->append_failure = "none";
        generate_complete(summary, &generation_stop_context, step);
        return YVEX_OK;
    }
    if (!yvex_core_u64_add(summary->current_decode_position, 1ull, &next_position)) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "generate_append",
                                      "decode position advance would overflow");
    }
    (void)next_position;
    if (summary->generated_token_count >= YVEX_TOKEN_INPUT_MAX_TOKENS ||
        sequence->token_count >= sequence->max_tokens ||
        sequence->token_count >= YVEX_TOKEN_INPUT_MAX_TOKENS) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "generate_append",
                                      "token append exceeds bounded token input capacity");
    }
    return yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_GENERATE_APPEND", err, YVEX_ERR_BACKEND,
        "generate_append", "test generation append failure");
}

/* Purpose: Append append commit while preserving checked capacity and deterministic order.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int generate_append_commit(yvex_generation_report *summary,
                                  yvex_token_input *sequence,
                                  const yvex_generation_append_state *append,
                                  yvex_error *err)
{
    unsigned long long generated_index;
    unsigned long long next_position;

    if (!summary || !sequence || !append || !append->candidate_recorded) {
        return yvex_generation_refuse(
            err, YVEX_ERR_INVALID_ARG, "generate_append",
            "append commit requires state, sequence, and candidate");
    }
    if (!yvex_core_u64_add(summary->current_decode_position, 1ull, &next_position)) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "generate_append",
                                      "decode position advance would overflow");
    }
    generated_index = summary->generated_token_count;
    sequence->tokens[sequence->token_count++] = append->token_id;
    summary->generated_tokens[generated_index] = append->token_id;
    summary->generated_token_count += 1ull;
    summary->accepted_token_count += 1ull;
    summary->append_steps += 1ull;
    summary->partial_generated_token_count = summary->generated_token_count;
    summary->total_token_count = sequence->token_count;
    summary->last_appended_token_seen = 1;
    summary->last_appended_token_id = append->token_id;
    summary->last_successful_position = append->append_position;
    summary->current_decode_position = next_position;
    summary->append_status = "appended";
    summary->append_failure = "none";
    summary->generation_checksum = yvex_core_hash_mix_u64(summary->generation_checksum,
                                                    append->step);
    summary->generation_checksum = yvex_core_hash_mix_u64(summary->generation_checksum,
                                                    (unsigned long long)append->token_id);
    summary->generation_checksum = yvex_generation_hash_float(summary->generation_checksum,
                                                              append->logit);
    summary->generation_checksum = yvex_core_hash_mix_u64(summary->generation_checksum,
                                                    append->sample_checksum);
    summary->sequence_checksum = yvex_core_hash_mix_u64(summary->sequence_checksum,
                                                  (unsigned long long)append->token_id);
    summary->state.state_dirty = 1;
    summary->state.last_completed_step_seen = 1;
    summary->state.last_completed_step = append->step;
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
    if (append->step < summary->trace_step_count) {
        yvex_generation_trace_step *record = &summary->trace_step_records[append->step];

        record->append_status = "appended";
        record->appended_token_id = append->token_id;
        record->position_after_append = next_position;
        yvex_generation_project_fields(
            record, summary, generation_trace_account_fields,
            sizeof(generation_trace_account_fields) /
                sizeof(generation_trace_account_fields[0]));
    }
    return YVEX_OK;
}

/* Purpose: Project immutable finish report facts into the typed reporting surface. */
static void generate_finish_report(yvex_generation_report *out,
                                   yvex_generation_report *summary)
{
    if (!out || !summary) {
        return;
    }
    yvex_generation_trace_account(summary);
    *out = *summary;
}

typedef struct {
    const yvex_generation_request *request;
    yvex_generation_report *report;
    yvex_error *err;
    yvex_generation_admission admission;
    yvex_token_input sequence;
    yvex_generation_report summary;
    unsigned long long context_length;
    int status;
} generation_run_ctx;

/* Publishes a failed preflight and releases only the model reference acquired
 * before engine construction. */
/* Purpose: Implement the canonical preflight fail mechanism owned by the generation boundary. */
static int generation_preflight_fail(generation_run_ctx *ctx, int status)
{
    generate_mark_failure(&ctx->summary, "preflight", "internal-error", 0ull);
    generate_mark_cleanup(&ctx->summary);
    generate_finish_report(ctx->report, &ctx->summary);
    yvex_generation_admission_close(&ctx->admission);
    return status;
}

/* Resolves and validates the model, prompt, context, graph guard, and engine
 * required by the bounded loop without executing a generation step. */
/* Purpose: Implement the canonical preflight mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int generation_preflight(generation_run_ctx *ctx)
{
    const yvex_token_input *summary_input;
    unsigned long long context_span;
    int status;

    status = yvex_generation_admission_prepare(
        &ctx->admission, ctx->request->model_arg, ctx->request->tokens_text,
        YVEX_GENERATION_IDENTITY_MODEL_REF, ctx->err);
    if (status != YVEX_OK &&
        ctx->admission.stage == YVEX_GENERATION_ADMISSION_RESOLVE) {
        return status;
    }
    summary_input = ctx->admission.stage == YVEX_GENERATION_ADMISSION_IDENTITY
                        ? NULL
                        : &ctx->admission.token_input;
    generate_summary_defaults(&ctx->summary, summary_input,
                              ctx->request->position_start,
                              ctx->request->max_new_tokens,
                              ctx->request->trace_level,
                              ctx->request->cancel_after_steps_seen,
                              ctx->request->cancel_after_steps);
    ctx->summary.model_arg = ctx->request->model_arg;
    ctx->summary.backend_name = ctx->request->backend_name;
    ctx->summary.segment_name = ctx->request->segment_name;
    ctx->summary.trace_kv_requested = ctx->request->attach_kv ? 1 : 0;
    ctx->summary.trace_kv_shape = ctx->request->kv_shape;
    if (status != YVEX_OK) {
        return generation_preflight_fail(ctx, status);
    }
    ctx->summary.token_input_status = "pass";
    generate_state_set_lifecycle(&ctx->summary, "preflighted");
    ctx->sequence = ctx->admission.token_input;
    if (!ctx->request->context_length_seen &&
        (!yvex_core_u64_add(ctx->admission.token_input.token_count,
                           ctx->request->max_new_tokens, &context_span) ||
         !yvex_core_u64_add(ctx->request->position_start, context_span,
                           &ctx->context_length))) {
        yvex_error_set(ctx->err, YVEX_ERR_BOUNDS, "generate",
                       "context length overflow");
        return generation_preflight_fail(ctx, YVEX_ERR_BOUNDS);
    }
    ctx->summary.context_length = ctx->context_length;
    status = yvex_generation_admission_engine_open(
        &ctx->admission, ctx->request->backend_name, ctx->err);
    return status == YVEX_OK ? YVEX_OK : generation_preflight_fail(ctx, status);
}

/* Executes one bounded sample/append step.  A zero return means the caller
 * must stop because a refusal, cancellation, or terminal condition was
 * recorded in the summary. */
/* Purpose: Implement the canonical step mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int generation_step(generation_run_ctx *ctx, unsigned long long step)
{
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options sample_options;
    yvex_sampling_summary sample_summary;
    yvex_generation_append_state append_state;
    unsigned long long decode_position;
    int context_stop = 0;

    if (!yvex_core_u64_add(ctx->request->position_start,
                                 ctx->sequence.token_count, &decode_position)) {
        yvex_error_set(ctx->err, YVEX_ERR_BOUNDS, "generate",
                       "decode position overflow");
        generate_mark_failure(&ctx->summary, "stop-check", "internal-error", step);
        ctx->status = YVEX_ERR_BOUNDS;
        return 0;
    }
    ctx->summary.current_decode_position = decode_position;
    (void)generate_trace_step_begin(&ctx->summary, step, decode_position);
    ctx->summary.state.lifecycle_status = "step-active";
    ctx->summary.state.active_step_seen = 1;
    ctx->summary.state.active_step = step;
    if (ctx->request->cancel_after_steps_seen &&
        ctx->request->cancel_after_steps == 0ull) {
        generate_mark_cancel(&ctx->summary, step, 0ull, "before-step",
                             "before-decode", 1, 0);
        return 0;
    }
    if (decode_position >= ctx->context_length) {
        ctx->summary.append_status = "context-limit";
        ctx->summary.append_failure = "none";
        generate_complete(&ctx->summary, &generation_stop_context, step);
        return 0;
    }
    memset(&decode_options, 0, sizeof(decode_options));
    memset(&logits_options, 0, sizeof(logits_options));
    memset(&sample_options, 0, sizeof(sample_options));
    memset(&sample_summary, 0, sizeof(sample_summary));
    ctx->summary.phase = "prefill";
    generate_state_set_lifecycle(&ctx->summary, "prefilled");
    ctx->summary.prefill_invoked = 1;
    decode_options.token_input = &ctx->sequence;
    yvex_generation_project_fields(
        &decode_options, ctx->request, generation_request_fields,
        sizeof(generation_request_fields) / sizeof(generation_request_fields[0]));
    if (!ctx->request->chunk_size_seen) decode_options.chunk_size = 0ull;
    decode_options.context_length = ctx->context_length;
    if (!ctx->request->layer_count_seen) decode_options.layer_count = 0ull;
    logits_options.decode_options = &decode_options;
    logits_options.logits_count = ctx->request->logits_count;
    sample_options.logits_options = &logits_options;
    sample_options.strategy = YVEX_SAMPLING_STRATEGY_GREEDY;
    ctx->summary.phase = "sample";
    ctx->status = yvex_engine_sample_token(
        ctx->admission.engine, &sample_options, &sample_summary, ctx->err);
    if (ctx->status != YVEX_OK) {
        generate_account_failed_sample(&ctx->summary, &sample_summary, step);
        generate_mark_failure(
            &ctx->summary, generate_sample_failure_phase(&sample_summary),
            generate_sample_failure_reason(&sample_summary), step);
        return 0;
    }
    ctx->summary.loop_executed = 1;
    ctx->summary.decode_steps++;
    ctx->summary.logits_steps++;
    ctx->summary.sample_steps++;
    generate_record_candidate(&ctx->summary, &append_state,
                              &sample_summary, step, decode_position);
    ctx->summary.phase = "append";
    ctx->status = generate_append_preflight(
        &ctx->summary, &ctx->sequence, &append_state,
        ctx->context_length, step, &context_stop, ctx->err);
    if (context_stop)
        return 0;
    if (ctx->status == YVEX_OK)
        ctx->status = generate_append_commit(
            &ctx->summary, &ctx->sequence, &append_state, ctx->err);
    if (ctx->status != YVEX_OK) {
        generate_mark_failure(&ctx->summary, "append", "append-failure", step);
        return 0;
    }
    ctx->summary.phase = "stop-check";
    if (ctx->request->cancel_after_steps_seen &&
        ctx->request->cancel_after_steps > 0ull &&
        ctx->summary.generated_token_count >= ctx->request->cancel_after_steps) {
        generate_mark_cancel(&ctx->summary, step,
                             ctx->request->cancel_after_steps,
                             "after-step", "after-append", 0, 1);
        return 0;
    }
    if (ctx->summary.generated_token_count >= ctx->request->max_new_tokens) {
        generate_complete(&ctx->summary, &generation_stop_max, step);
        return 0;
    }
    return 1;
}

/* Finalizes the bounded loop, releases all owned engine/model state, and maps
 * a recorded failed phase to a typed error without promoting capability. */
/* Purpose: Implement the canonical finish mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int generation_finish(generation_run_ctx *ctx)
{
    if (!ctx->summary.stop_requested &&
        ctx->summary.generated_token_count >= ctx->request->max_new_tokens &&
        ctx->summary.phase && strcmp(ctx->summary.phase, "failed") != 0) {
        generate_complete(
            &ctx->summary, &generation_stop_max,
            ctx->summary.generated_token_count > 0ull
                ? ctx->summary.generated_token_count - 1ull : 0ull);
    }
    generate_mark_cleanup(&ctx->summary);
    if (yvex_core_test_flag("YVEX_TEST_REPEAT_GENERATE_CLEANUP"))
        generate_mark_cleanup(&ctx->summary);
    yvex_generation_admission_close(&ctx->admission);
    generate_finish_report(ctx->report, &ctx->summary);
    if (ctx->summary.phase && strcmp(ctx->summary.phase, "failed") == 0) {
        if (ctx->status == YVEX_OK) {
            yvex_error_set(ctx->err, YVEX_ERR_STATE, "generate",
                           "generation loop failed");
            ctx->status = YVEX_ERR_STATE;
        }
        return ctx->status;
    }
    yvex_error_clear(ctx->err);
    return YVEX_OK;
}

/* Purpose: execute the bounded diagnostic generation loop over existing prefill, decode,
 * logits, sampling, append, stop, and cleanup boundaries.
 * Inputs: request is borrowed; report receives by-value diagnostic facts.
 * Effects: opens and closes an engine, mutates only local loop state, and publishes the final diagnostic report.
 * Failure: returns typed preflight or step failures with partial report state and deterministic resource cleanup.
 * Boundary: diagnostic generation is not supported-family generation, evaluation,
 * benchmark evidence, throughput, or release readiness. */
int yvex_generation_run_diagnostic(const yvex_generation_request *request,
                                   yvex_generation_report *report,
                                   yvex_error *err)
{
    generation_run_ctx ctx;
    unsigned long long step;
    int status;

    if (!request || !report) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "generate", "request and report are required");
    }
    memset(&ctx, 0, sizeof(ctx));
    memset(report, 0, sizeof(*report));
    yvex_error_clear(err);
    ctx.request = request;
    ctx.report = report;
    ctx.err = err;
    ctx.context_length = request->context_length;
    status = generation_preflight(&ctx);
    if (status != YVEX_OK)
        return status;
    ctx.status = YVEX_OK;
    generate_state_set_lifecycle(&ctx.summary, "running");
    for (step = 0ull; step < request->max_new_tokens; ++step)
        if (!generation_step(&ctx, step))
            break;
    return generation_finish(&ctx);
}
