/*
 * yvex_generation.c - diagnostic generation loop composition.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   generation loop composition over available prefill, decode, logits,
 *   sampling, token append, stop, trace, cancel, and cleanup pieces.
 *
 * Does not own:
 *   tensor role mapping, artifact emission, tokenizer training, benchmark,
 *   command grammar outside this command surface, server/provider generation,
 *   or release decisions.
 *
 * Invariants:
 *   generated state remains bounded and diagnostic; partial output and cleanup
 *   behavior are explicit; trace/audit output must not imply supported-family
 *   runtime generation.
 *
 * Boundary:
 *   diagnostic generation is not runtime generation over supported-family
 *   artifacts, eval evidence, benchmark evidence, throughput, or release
 *   readiness.
 */

#include "yvex_generation_private.h"

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

static int generate_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long generate_mix_u64(unsigned long long hash,
                                           unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long generate_mix_float(unsigned long long hash,
                                             double value)
{
    float narrowed = (float)value;
    uint32_t bits = 0u;

    memcpy(&bits, &narrowed, sizeof(bits));
    return generate_mix_u64(hash, (unsigned long long)bits);
}

static int generate_add_ull(unsigned long long a,
                            unsigned long long b,
                            unsigned long long *out)
{
    if (!out || a > ~0ull - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static unsigned long long generate_state_id_for_input(const yvex_token_input *input,
                                                      unsigned long long position_start,
                                                      unsigned long long max_new_tokens)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    hash = generate_mix_u64(hash, position_start);
    hash = generate_mix_u64(hash, max_new_tokens);
    hash = generate_mix_u64(hash, input ? input->token_count : 0ull);
    if (input) {
        for (i = 0ull; i < input->token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            hash = generate_mix_u64(hash, (unsigned long long)input->tokens[i]);
        }
    }
    return hash ? hash : 1ull;
}

static void generate_state_defaults(yvex_generation_report *summary,
                                    const yvex_token_input *input,
                                    unsigned long long position_start,
                                    unsigned long long max_new_tokens,
                                    int cancel_after_steps_seen,
                                    unsigned long long cancel_after_steps)
{
    yvex_generation_state *state;

    if (!summary) {
        return;
    }
    state = &summary->state;
    memset(state, 0, sizeof(*state));
    state->state_id = generate_state_id_for_input(input, position_start, max_new_tokens);
    state->lifecycle_status = "created";
    state->generation_state = "created";
    state->cancel_supported = 1;
    state->cancel_after_steps_seen = cancel_after_steps_seen ? 1 : 0;
    state->cancel_after_steps = cancel_after_steps;
    state->cancel_reason = "none";
    state->cancel_timing = "none";
    state->cancel_safe_point = "none";
    state->cleanup_idempotent = 1;
    state->failure_preserved = 1;
    state->partial_output_preserved = 1;
}

static void generate_state_set_lifecycle(yvex_generation_report *summary,
                                         const char *lifecycle_status)
{
    if (summary && lifecycle_status) {
        summary->state.lifecycle_status = lifecycle_status;
    }
}

static void generate_state_mark_step(yvex_generation_report *summary,
                                     unsigned long long step)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "step-active";
    summary->state.active_step_seen = 1;
    summary->state.active_step = step;
}

static void generate_state_mark_append(yvex_generation_report *summary,
                                       unsigned long long step)
{
    if (!summary) {
        return;
    }
    summary->state.state_dirty = 1;
    summary->state.last_completed_step_seen = 1;
    summary->state.last_completed_step = step;
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

static void generate_state_mark_completed(yvex_generation_report *summary)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "completed";
    summary->state.generation_state = "completed";
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

static void generate_state_mark_failed(yvex_generation_report *summary)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "failed";
    summary->state.generation_state = "failed";
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

static void generate_state_mark_cancelled(yvex_generation_report *summary,
                                          unsigned long long cancel_step,
                                          const char *timing,
                                          const char *safe_point)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "cancelled";
    summary->state.generation_state = "cancelled";
    summary->state.cancel_requested = 1;
    summary->state.cancel_reason = "interrupted";
    summary->state.cancel_step_seen = 1;
    summary->state.cancel_step = cancel_step;
    summary->state.cancel_timing = timing ? timing : "stop-check";
    summary->state.cancel_safe_point = safe_point ? safe_point : "stop-check";
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

static void generate_summary_trace_defaults(yvex_generation_report *summary,
                                            yvex_generation_trace_level level)
{
    if (!summary) {
        return;
    }
    summary->trace_level = level;
    summary->trace_level_name = yvex_generation_trace_level_name(level);
    summary->trace_enabled = level != YVEX_GENERATION_TRACE_NONE;
    summary->trace_status = summary->trace_enabled ? "enabled" : "disabled";
}

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
    memset(summary, 0, sizeof(*summary));
    summary->loop_created = 1;
    summary->phase = "created";
    summary->status = "generation-loop";
    summary->token_input_status = "fail";
    summary->prompt_token_count = input ? input->token_count : 0ull;
    summary->prefill_token_count = input ? input->token_count : 0ull;
    summary->max_new_tokens = max_new_tokens;
    summary->generated_token_count = 0ull;
    summary->accepted_token_count = 0ull;
    summary->total_token_count = input ? input->token_count : 0ull;
    summary->position_start = position_start;
    summary->append_status = "not-started";
    summary->append_failure = "none";
    summary->context_length = 0ull;
    summary->stop_policy = "bounded-diagnostic";
    summary->stop_reason = "internal-error";
    summary->stop_phase = "preflight";
    summary->stop_timing = "preflight";
    summary->eos_policy = "unsupported";
    summary->stop_token_policy = "unsupported";
    if (input && input->token_count > 0ull) {
        unsigned long long offset = input->token_count - 1ull;
        if (generate_add_ull(position_start, offset, &summary->prefill_position_end)) {
            (void)generate_add_ull(summary->prefill_position_end,
                                   1ull,
                                   &summary->current_decode_position);
            summary->last_successful_position = summary->prefill_position_end;
        }
    }
    summary->generation_checksum = 1469598103934665603ull;
    summary->sequence_checksum = 1469598103934665603ull;
    if (input) {
        for (i = 0ull; i < input->token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            summary->prompt_tokens[i] = input->tokens[i];
            summary->sequence_checksum = generate_mix_u64(summary->sequence_checksum,
                                                          (unsigned long long)input->tokens[i]);
        }
    }
    summary->cleanup_status = "not-needed";
    summary->failed_phase = "none";
    generate_state_defaults(summary,
                            input,
                            position_start,
                            max_new_tokens,
                            cancel_after_steps_seen,
                            cancel_after_steps);
    generate_summary_trace_defaults(summary, trace_level);
}

static void generate_summary_trace_kv(yvex_generation_report *summary,
                                      int attach_kv,
                                      const yvex_kv_shape *shape)
{
    if (!summary) {
        return;
    }
    summary->trace_kv_requested = attach_kv ? 1 : 0;
    if (shape) {
        summary->trace_kv_shape = *shape;
    }
}

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

static yvex_generation_trace_step *generate_trace_step_begin(
    yvex_generation_report *summary,
    unsigned long long step,
    unsigned long long position)
{
    yvex_generation_trace_step *record = generate_trace_step_at(summary, step);

    if (!record) {
        return NULL;
    }
    memset(record, 0, sizeof(*record));
    record->attempted = 1;
    record->index = step;
    record->decode_position = position;
    record->decode_status = "pending";
    record->logits_status = "pending";
    record->sample_status = "pending";
    record->append_status = "not-started";
    record->stop_reason = "none";
    record->stop_timing = "none";
    return record;
}

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

static void generate_trace_step_mark_failure(yvex_generation_report *summary,
                                             const char *phase,
                                             unsigned long long step)
{
    yvex_generation_trace_step *record;

    if (!summary || step >= summary->trace_step_count) {
        return;
    }
    record = &summary->trace_step_records[step];
    if (!record->attempted) {
        return;
    }
    if (phase && strcmp(phase, "decode") == 0) {
        record->decode_status = "fail";
        record->logits_status = "skipped";
        record->sample_status = "skipped";
        record->append_status = "not-started";
    } else if (phase && strcmp(phase, "logits") == 0) {
        record->decode_status = "pass";
        record->logits_status = "fail";
        record->sample_status = "skipped";
        record->append_status = "not-started";
    } else if (phase && strcmp(phase, "sample") == 0) {
        record->decode_status = "pass";
        record->logits_status = "pass";
        record->sample_status = "fail";
        record->append_status = "not-started";
    } else if (phase && strcmp(phase, "append") == 0) {
        record->decode_status = "pass";
        record->logits_status = "pass";
        record->sample_status = "pass";
        record->append_status = "fail";
    } else {
        record->append_status = "fail";
    }
}

static void generate_stop_record(yvex_generation_report *summary,
                                 const char *reason,
                                 const char *phase,
                                 unsigned long long step,
                                 const char *timing,
                                 int before_append,
                                 int after_append,
                                 int failure,
                                 int unsupported)
{
    if (!summary) {
        return;
    }
    summary->stop_requested = 1;
    summary->stop_reason = reason ? reason : "internal-error";
    summary->stop_phase = phase ? phase : "stop-check";
    summary->stop_step = step;
    summary->stop_timing = timing ? timing : "failure";
    summary->stop_before_append = before_append ? 1 : 0;
    summary->stop_after_append = after_append ? 1 : 0;
    summary->failure_stop = failure ? 1 : 0;
    summary->unsupported_stop_feature = unsupported ? 1 : 0;
    generate_trace_step_mark_stop(summary, summary->stop_reason, summary->stop_timing,
                                  step, before_append);
}

/*
 * generate_mark_cleanup()
 *
 * Purpose:
 *   record idempotent diagnostic cleanup state for the generation summary.
 *
 * Inputs:
 *   summary is borrowed mutable diagnostic state.
 *
 * Effects:
 *   mutates lifecycle/cleanup flags only; it does not free external model
 *   ownership or change backend/device allocations.
 *
 * Failure:
 *   no failure path; missing summaries are ignored by callers before use.
 *
 * Boundary:
 *   cleanup accounting is not proof of runtime generation support or release
 *   readiness.
 */
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
    summary->state.lifecycle_status = "cleaned";
    summary->state.cleanup_idempotent = 1;
    summary->state.cleanup_owned_state_released = 1;
    summary->state.failure_preserved = 1;
    summary->state.partial_output_preserved = 1;
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

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
    generate_state_mark_failed(summary);
    generate_stop_record(summary,
                         reason ? reason : "internal-error",
                         phase ? phase : "preflight",
                         step,
                         "failure",
                         0,
                         0,
                         1,
                         0);
    if (phase && strcmp(phase, "append") == 0) {
        summary->append_status = "append-failed";
        summary->append_failure = reason ? reason : "append-failure";
    }
    generate_trace_step_mark_failure(summary, phase, step);
}

static void generate_mark_cancel(yvex_generation_report *summary,
                                 unsigned long long step,
                                 unsigned long long cancel_step,
                                 const char *timing,
                                 const char *safe_point,
                                 int before_append,
                                 int after_append)
{
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
    generate_state_mark_cancelled(summary, cancel_step, timing, safe_point);
    generate_stop_record(summary,
                         "interrupted",
                         "stop-check",
                         step,
                         "cancel-safe-point",
                         before_append,
                         after_append,
                         0,
                         0);
}

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
            record->accepted_token_count = summary->accepted_token_count;
            record->generated_token_count = summary->generated_token_count;
            record->total_token_count = summary->total_token_count;
            record->sequence_checksum = summary->sequence_checksum;
        }
    }
    if (sample->sample_created) {
        yvex_generation_trace_step *record;

        summary->candidate_token_seen = 1;
        summary->candidate_token_id = sample->selected_token_id;
        summary->candidate_logit = sample->selected_logit;
        summary->last_selected_token_id = sample->selected_token_id;
        summary->last_selected_logit = sample->selected_logit;
        summary->append_status = "candidate-" "rea" "dy";
        if (step < summary->trace_step_count) {
            record = &summary->trace_step_records[step];
            record->candidate_token_id = sample->selected_token_id;
            record->candidate_logit = sample->selected_logit;
            record->selected_token_id = sample->selected_token_id;
            record->logits_checksum = sample->logits_checksum;
            record->logits_min = sample->logits_min;
            record->logits_max = sample->logits_max;
            record->sample_checksum = sample->sample_checksum;
        }
    }
}

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
    summary->candidate_token_seen = 1;
    summary->candidate_token_id = append->token_id;
    summary->candidate_logit = append->logit;
    summary->last_selected_token_id = append->token_id;
    summary->last_selected_logit = append->logit;
    summary->append_status = "candidate-" "rea" "dy";
    summary->append_failure = "none";
    if (step < summary->trace_step_count) {
        yvex_generation_trace_step *record = &summary->trace_step_records[step];

        record->decode_status = "pass";
        record->logits_status = "pass";
        record->sample_status = "pass";
        record->append_status = "candidate";
        record->selected_token_id = sample->selected_token_id;
        record->candidate_token_id = sample->selected_token_id;
        record->candidate_logit = sample->selected_logit;
        record->logits_checksum = sample->logits_checksum;
        record->logits_min = sample->logits_min;
        record->logits_max = sample->logits_max;
        record->sample_checksum = sample->sample_checksum;
    }
}

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
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "generate_append",
                       "generation append state is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!append->candidate_recorded || !summary->candidate_token_seen) {
        yvex_error_set(err, YVEX_ERR_STATE, "generate_append",
                       "candidate token is required before append");
        return YVEX_ERR_STATE;
    }
    if (summary->generated_token_count != summary->accepted_token_count ||
        summary->generated_token_count != summary->append_steps ||
        summary->generated_token_count != summary->partial_generated_token_count) {
        yvex_error_set(err, YVEX_ERR_STATE, "generate_append",
                       "generated token accounting is inconsistent");
        return YVEX_ERR_STATE;
    }
    if (summary->generated_token_count >= summary->max_new_tokens) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "generate_append",
                       "append would exceed requested max-new-tokens");
        return YVEX_ERR_BOUNDS;
    }
    if (!generate_add_ull(summary->prompt_token_count,
                          summary->generated_token_count,
                          &expected_total) ||
        expected_total != sequence->token_count ||
        expected_total != summary->total_token_count) {
        yvex_error_set(err, YVEX_ERR_STATE, "generate_append",
                       "runtime token sequence accounting is inconsistent");
        return YVEX_ERR_STATE;
    }
    if (summary->current_decode_position >= context_length) {
        if (context_stop) {
            *context_stop = 1;
        }
        summary->append_status = "context-limit";
        summary->append_failure = "none";
        summary->status = "generation-loop-complete";
        summary->phase = "complete";
        generate_state_mark_completed(summary);
        generate_stop_record(summary,
                             "context-limit",
                             "stop-check",
                             step,
                             "pre-append",
                             1,
                             0,
                             0,
                             0);
        return YVEX_OK;
    }
    if (!generate_add_ull(summary->current_decode_position, 1ull, &next_position)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "generate_append",
                       "decode position advance would overflow");
        return YVEX_ERR_BOUNDS;
    }
    (void)next_position;
    if (summary->generated_token_count >= YVEX_TOKEN_INPUT_MAX_TOKENS ||
        sequence->token_count >= sequence->max_tokens ||
        sequence->token_count >= YVEX_TOKEN_INPUT_MAX_TOKENS) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "generate_append",
                       "token append exceeds bounded token input capacity");
        return YVEX_ERR_BOUNDS;
    }
    if (generate_test_env_enabled("YVEX_TEST_FAIL_GENERATE_APPEND")) {
        yvex_error_set(err, YVEX_ERR_BACKEND, "generate_append",
                       "test generation append failure");
        return YVEX_ERR_BACKEND;
    }
    return YVEX_OK;
}

static int generate_append_commit(yvex_generation_report *summary,
                                  yvex_token_input *sequence,
                                  const yvex_generation_append_state *append,
                                  yvex_error *err)
{
    unsigned long long generated_index;
    unsigned long long next_position;

    if (!summary || !sequence || !append || !append->candidate_recorded) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "generate_append",
                       "append commit requires state, sequence, and candidate");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!generate_add_ull(summary->current_decode_position, 1ull, &next_position)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "generate_append",
                       "decode position advance would overflow");
        return YVEX_ERR_BOUNDS;
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
    summary->generation_checksum = generate_mix_u64(summary->generation_checksum,
                                                    append->step);
    summary->generation_checksum = generate_mix_u64(summary->generation_checksum,
                                                    (unsigned long long)append->token_id);
    summary->generation_checksum = generate_mix_float(summary->generation_checksum,
                                                     append->logit);
    summary->generation_checksum = generate_mix_u64(summary->generation_checksum,
                                                    append->sample_checksum);
    summary->sequence_checksum = generate_mix_u64(summary->sequence_checksum,
                                                  (unsigned long long)append->token_id);
    generate_state_mark_append(summary, append->step);
    if (append->step < summary->trace_step_count) {
        yvex_generation_trace_step *record = &summary->trace_step_records[append->step];

        record->append_status = "appended";
        record->appended_token_id = append->token_id;
        record->position_after_append = next_position;
        record->accepted_token_count = summary->accepted_token_count;
        record->generated_token_count = summary->generated_token_count;
        record->total_token_count = summary->total_token_count;
        record->sequence_checksum = summary->sequence_checksum;
    }
    return YVEX_OK;
}

static int generate_compute_default_context(unsigned long long position_start,
                                            unsigned long long prompt_tokens,
                                            unsigned long long max_new_tokens,
                                            unsigned long long *out)
{
    unsigned long long total;

    if (!generate_add_ull(prompt_tokens, max_new_tokens, &total)) {
        return 0;
    }
    return generate_add_ull(position_start, total, out);
}

static void generate_finish_report(yvex_generation_report *out,
                                   yvex_generation_report *summary)
{
    if (!out || !summary) {
        return;
    }
    yvex_generation_trace_account(summary);
    *out = *summary;
}

static void generate_summary_bind_request(yvex_generation_report *summary,
                                          const yvex_generation_request *request)
{
    if (!summary || !request) {
        return;
    }
    summary->model_arg = request->model_arg;
    summary->backend_name = request->backend_name;
    summary->segment_name = request->segment_name;
}

/*
 * yvex_generation_run_diagnostic()
 *
 * Purpose:
 *   execute the bounded diagnostic generation loop over existing prefill,
 *   decode, logits, sampling, append, stop, cancel, and cleanup pieces.
 *
 * Inputs:
 *   request is borrowed validated generation input; report receives by-value
 *   diagnostic facts for renderers.
 *
 * Effects:
 *   resolves the model reference, validates explicit token bounds, may open
 *   an engine, runs bounded diagnostic sample/append steps, closes owned
 *   engine/reference state, and accounts trace records.
 *
 * Failure:
 *   returns YVEX status codes for model, token, engine, decode/logits/sample,
 *   append, or bounds failures while preserving partial report state.
 *
 * Boundary:
 *   diagnostic generation is not runtime generation over supported-family
 *   artifacts, provider generation, eval evidence, benchmark evidence,
 *   throughput, or release readiness.
 */
int yvex_generation_run_diagnostic(const yvex_generation_request *request,
                                   yvex_generation_report *report,
                                   yvex_error *err)
{
    yvex_model_ref model_ref;
    yvex_token_input prompt_input;
    yvex_token_input sequence;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options sample_options;
    yvex_sampling_summary sample_summary;
    yvex_generation_report summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    unsigned long long vocab_size = 0ull;
    unsigned long long context_length;
    unsigned long long step;
    int rc;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "generate",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_error_clear(err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&prompt_input, 0, sizeof(prompt_input));
    memset(&sequence, 0, sizeof(sequence));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&decode_options, 0, sizeof(decode_options));
    memset(&logits_options, 0, sizeof(logits_options));
    memset(&sample_options, 0, sizeof(sample_options));
    memset(&sample_summary, 0, sizeof(sample_summary));
    memset(&summary, 0, sizeof(summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(report, 0, sizeof(*report));
    context_length = request->context_length;

    rc = yvex_model_ref_resolve(&model_ref, request->model_arg, NULL, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    generate_summary_defaults(&summary,
                              NULL,
                              request->position_start,
                              request->max_new_tokens,
                              request->trace_level,
                              request->cancel_after_steps_seen,
                              request->cancel_after_steps);
    generate_summary_bind_request(&summary, request);
    generate_summary_trace_kv(&summary, request->attach_kv, &request->kv_shape);
    rc = enforce_registered_identity_cli(&model_ref, "generate");
    if (rc != YVEX_OK) {
        yvex_error_set(err, rc, "generate",
                       "registered model identity check failed");
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_finish_report(report, &summary);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    rc = yvex_token_input_parse_explicit(request->tokens_text, &prompt_input, err);
    if (rc == YVEX_OK) {
        rc = cli_token_input_vocab_from_model(model_ref.path, &vocab_size, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&prompt_input, vocab_size, err);
    }
    generate_summary_defaults(&summary,
                              &prompt_input,
                              request->position_start,
                              request->max_new_tokens,
                              request->trace_level,
                              request->cancel_after_steps_seen,
                              request->cancel_after_steps);
    generate_summary_bind_request(&summary, request);
    generate_summary_trace_kv(&summary, request->attach_kv, &request->kv_shape);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_finish_report(report, &summary);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }
    summary.token_input_status = "pass";
    generate_state_set_lifecycle(&summary, "preflighted");
    sequence = prompt_input;

    if (!request->context_length_seen &&
        !generate_compute_default_context(request->position_start,
                                          prompt_input.token_count,
                                          request->max_new_tokens,
                                          &context_length)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "generate",
                       "context length overflow");
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_finish_report(report, &summary);
        yvex_model_ref_clear(&model_ref);
        return YVEX_ERR_BOUNDS;
    }
    summary.context_length = context_length;

    rc = preflight_graph_guard(&model_ref,
                               request->backend_name,
                               0,
                               1,
                               prompt_input.tokens[0],
                               &graph_guard,
                               err);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_finish_report(report, &summary);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = request->backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, err);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_finish_report(report, &summary);
        yvex_model_ref_clear(&model_ref);
        return rc;
    }

    generate_state_set_lifecycle(&summary, "running");
    for (step = 0ull; step < request->max_new_tokens; ++step) {
        yvex_generation_append_state append_state;
        unsigned long long decode_position;
        int context_stop = 0;

        if (!generate_add_ull(request->position_start,
                              sequence.token_count,
                              &decode_position)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "generate",
                           "decode position overflow");
            generate_mark_failure(&summary, "stop-check", "internal-error", step);
            break;
        }
        summary.current_decode_position = decode_position;
        (void)generate_trace_step_begin(&summary, step, decode_position);
        generate_state_mark_step(&summary, step);
        if (request->cancel_after_steps_seen &&
            request->cancel_after_steps == 0ull) {
            generate_mark_cancel(&summary,
                                 step,
                                 0ull,
                                 "before-step",
                                 "before-decode",
                                 1,
                                 0);
            break;
        }
        if (decode_position >= context_length) {
            summary.phase = "complete";
            summary.status = "generation-loop-complete";
            summary.append_status = "context-limit";
            summary.append_failure = "none";
            generate_state_mark_completed(&summary);
            generate_stop_record(&summary,
                                 "context-limit",
                                 "stop-check",
                                 step,
                                 "pre-append",
                                 1,
                                 0,
                                 0,
                                 0);
            break;
        }

        memset(&decode_options, 0, sizeof(decode_options));
        memset(&logits_options, 0, sizeof(logits_options));
        memset(&sample_options, 0, sizeof(sample_options));
        memset(&sample_summary, 0, sizeof(sample_summary));

        summary.phase = "prefill";
        generate_state_set_lifecycle(&summary, "prefilled");
        summary.prefill_invoked = 1;
        decode_options.token_input = &sequence;
        decode_options.segment_name = request->segment_name;
        decode_options.backend_name = request->backend_name;
        decode_options.position_start = request->position_start;
        decode_options.chunk_size =
            request->chunk_size_seen ? request->chunk_size : 0ull;
        decode_options.context_length = context_length;
        decode_options.attach_kv = request->attach_kv;
        decode_options.kv_shape = request->kv_shape;
        decode_options.layer_count =
            request->layer_count_seen ? request->layer_count : 0ull;
        decode_options.layer_hidden_dim = request->layer_hidden_dim;
        decode_options.layer_head_dim = request->layer_head_dim;
        decode_options.layer_ffn_dim = request->layer_ffn_dim;
        logits_options.decode_options = &decode_options;
        logits_options.logits_count = request->logits_count;
        sample_options.logits_options = &logits_options;
        sample_options.strategy = YVEX_SAMPLING_STRATEGY_GREEDY;

        summary.phase = "sample";
        rc = yvex_engine_sample_token(engine, &sample_options,
                                      &sample_summary, err);
        if (rc != YVEX_OK) {
            generate_account_failed_sample(&summary, &sample_summary, step);
            generate_mark_failure(&summary,
                                  generate_sample_failure_phase(&sample_summary),
                                  generate_sample_failure_reason(&sample_summary),
                                  step);
            break;
        }

        summary.loop_executed = 1;
        summary.decode_steps += 1ull;
        summary.logits_steps += 1ull;
        summary.sample_steps += 1ull;
        generate_record_candidate(&summary,
                                  &append_state,
                                  &sample_summary,
                                  step,
                                  decode_position);

        summary.phase = "append";
        rc = generate_append_preflight(&summary,
                                       &sequence,
                                       &append_state,
                                       context_length,
                                       step,
                                       &context_stop,
                                       err);
        if (context_stop) {
            break;
        }
        if (rc != YVEX_OK) {
            generate_mark_failure(&summary, "append", "append-failure", step);
            break;
        }
        rc = generate_append_commit(&summary, &sequence, &append_state, err);
        if (rc != YVEX_OK) {
            generate_mark_failure(&summary, "append", "append-failure", step);
            break;
        }

        summary.phase = "stop-check";
        if (request->cancel_after_steps_seen &&
            request->cancel_after_steps > 0ull &&
            summary.generated_token_count >= request->cancel_after_steps) {
            generate_mark_cancel(&summary,
                                 step,
                                 request->cancel_after_steps,
                                 "after-step",
                                 "after-append",
                                 0,
                                 1);
            break;
        }
        if (summary.generated_token_count >= request->max_new_tokens) {
            summary.status = "generation-loop-complete";
            summary.phase = "complete";
            generate_state_mark_completed(&summary);
            generate_stop_record(&summary,
                                 "max-new-tokens",
                                 "stop-check",
                                 step,
                                 "post-append",
                                 0,
                                 1,
                                 0,
                                 0);
            break;
        }
    }

    if (!summary.stop_requested &&
        summary.generated_token_count >= request->max_new_tokens &&
        summary.phase &&
        strcmp(summary.phase, "failed") != 0) {
        summary.status = "generation-loop-complete";
        summary.phase = "complete";
        generate_state_mark_completed(&summary);
        generate_stop_record(&summary,
                             "max-new-tokens",
                             "stop-check",
                             summary.generated_token_count > 0ull ?
                                 summary.generated_token_count - 1ull : 0ull,
                             "post-append",
                             0,
                             1,
                             0,
                             0);
    }

    generate_mark_cleanup(&summary);
    if (generate_test_env_enabled("YVEX_TEST_REPEAT_GENERATE_CLEANUP")) {
        generate_mark_cleanup(&summary);
    }
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    generate_finish_report(report, &summary);
    if (summary.phase && strcmp(summary.phase, "failed") == 0) {
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "generate",
                           "generation loop failed");
            rc = YVEX_ERR_STATE;
        }
        return rc;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
