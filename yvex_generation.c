/*
 * yvex_generation.c - Runtime generation loop boundary.
 *
 * This file owns the first bounded diagnostic generation loop. It composes
 * prefill, decode, logits, greedy selection, token append, stop checks, and
 * cleanup without claiming full model output quality or server/provider output.
 */

#include <yvex/yvex.h>
#include "yvex_console_private.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int loop_created;
    int loop_executed;
    const char *phase;
    const char *status;
    const char *token_input_status;
    unsigned int prompt_tokens[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned int generated_tokens[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned long long prompt_token_count;
    unsigned long long prefill_token_count;
    unsigned long long max_new_tokens;
    unsigned long long generated_token_count;
    unsigned long long accepted_token_count;
    unsigned long long total_token_count;
    unsigned long long position_start;
    unsigned long long prefill_position_end;
    unsigned long long current_decode_position;
    int prefill_invoked;
    unsigned long long decode_steps;
    unsigned long long logits_steps;
    unsigned long long sample_steps;
    unsigned long long append_steps;
    int candidate_token_seen;
    unsigned int candidate_token_id;
    double candidate_logit;
    unsigned int last_selected_token_id;
    double last_selected_logit;
    int last_appended_token_seen;
    unsigned int last_appended_token_id;
    const char *append_status;
    const char *append_failure;
    unsigned long long context_length;
    const char *stop_policy;
    int stop_requested;
    const char *stop_reason;
    const char *stop_phase;
    unsigned long long stop_step;
    const char *stop_timing;
    int stop_after_append;
    int stop_before_append;
    int failure_stop;
    int unsupported_stop_feature;
    const char *eos_policy;
    const char *stop_token_policy;
    unsigned long long generation_checksum;
    unsigned long long sequence_checksum;
    int cleanup_attempted;
    const char *cleanup_status;
    const char *failed_phase;
    unsigned long long failed_step;
    unsigned long long partial_generated_token_count;
    unsigned long long last_successful_position;
} yvex_generate_summary;

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

static int generate_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

static int generate_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

static void generate_summary_defaults(yvex_generate_summary *summary,
                                      const yvex_token_input *input,
                                      unsigned long long position_start,
                                      unsigned long long max_new_tokens)
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
}

static void generate_stop_record(yvex_generate_summary *summary,
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
}

static void generate_print_token_list(const char *label,
                                      const unsigned int *tokens,
                                      unsigned long long count)
{
    unsigned long long i;

    printf("%s: ", label ? label : "tokens");
    for (i = 0ull; tokens && i < count; ++i) {
        if (i > 0ull) {
            printf(",");
        }
        printf("%u", tokens[i]);
    }
    printf("\n");
}

static void generate_print_runtime_sequence(const yvex_generate_summary *summary)
{
    unsigned long long i;
    unsigned long long emitted = 0ull;

    printf("runtime_token_sequence: ");
    if (summary) {
        for (i = 0ull; i < summary->prompt_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            if (emitted > 0ull) {
                printf(",");
            }
            printf("%u", summary->prompt_tokens[i]);
            emitted += 1ull;
        }
        for (i = 0ull; i < summary->generated_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            if (emitted > 0ull) {
                printf(",");
            }
            printf("%u", summary->generated_tokens[i]);
            emitted += 1ull;
        }
    }
    printf("\n");
}

static void generate_print_summary(const yvex_generate_summary *summary,
                                   const char *model_arg,
                                   const char *backend_name,
                                   const char *segment_name)
{
    printf("generate: loop\n");
    printf("status: %s\n", summary && summary->status ? summary->status : "generation-loop-fail");
    printf("model: %s\n", model_arg ? model_arg : "");
    printf("backend: %s\n", backend_name ? backend_name : "cpu");
    printf("segment: %s\n", segment_name ? segment_name : "embedding-rmsnorm");
    printf("token_input_status: %s\n",
           summary && summary->token_input_status ? summary->token_input_status : "fail");
    printf("prompt_token_count: %llu\n", summary ? summary->prompt_token_count : 0ull);
    printf("prefill_token_count: %llu\n", summary ? summary->prefill_token_count : 0ull);
    printf("max_new_tokens: %llu\n", summary ? summary->max_new_tokens : 0ull);
    printf("context_length: %llu\n", summary ? summary->context_length : 0ull);
    printf("generated_token_count: %llu\n", summary ? summary->generated_token_count : 0ull);
    printf("accepted_token_count: %llu\n", summary ? summary->accepted_token_count : 0ull);
    printf("partial_generated_token_count: %llu\n",
           summary ? summary->partial_generated_token_count : 0ull);
    printf("total_token_count: %llu\n", summary ? summary->total_token_count : 0ull);
    printf("position_start: %llu\n", summary ? summary->position_start : 0ull);
    printf("prefill_position_end: %llu\n", summary ? summary->prefill_position_end : 0ull);
    printf("current_decode_position: %llu\n",
           summary ? summary->current_decode_position : 0ull);
    printf("last_successful_position: %llu\n",
           summary ? summary->last_successful_position : 0ull);
    printf("generation_loop_kind: bounded-diagnostic\n");
    printf("generation_mode: diagnostic-runtime\n");
    printf("decode_mode: bounded-diagnostic\n");
    printf("logits_mode: bounded-diagnostic\n");
    printf("sampling_strategy: greedy\n");
    printf("bounded_generation: %s\n",
           summary && summary->loop_executed ? "true" : "false");
    printf("full_model_generation: false\n");
    printf("real_deepseek_generation: false\n");
    printf("prefill_invoked: %s\n", summary && summary->prefill_invoked ? "true" : "false");
    printf("decode_steps: %llu\n", summary ? summary->decode_steps : 0ull);
    printf("logits_steps: %llu\n", summary ? summary->logits_steps : 0ull);
    printf("sample_steps: %llu\n", summary ? summary->sample_steps : 0ull);
    printf("append_steps: %llu\n", summary ? summary->append_steps : 0ull);
    printf("candidate_token_id: %u\n",
           summary && summary->candidate_token_seen ? summary->candidate_token_id : 0u);
    printf("candidate_logit: %.9g\n",
           summary && summary->candidate_token_seen ? summary->candidate_logit : 0.0);
    printf("last_selected_token_id: %u\n", summary ? summary->last_selected_token_id : 0u);
    printf("last_selected_logit: %.9g\n", summary ? summary->last_selected_logit : 0.0);
    printf("last_appended_token_id: %u\n",
           summary && summary->last_appended_token_seen ? summary->last_appended_token_id : 0u);
    printf("append_status: %s\n",
           summary && summary->append_status ? summary->append_status : "not-started");
    printf("append_failure: %s\n",
           summary && summary->append_failure ? summary->append_failure : "none");
    printf("stop_policy: %s\n",
           summary && summary->stop_policy ? summary->stop_policy : "bounded-diagnostic");
    printf("stop_requested: %s\n",
           summary && summary->stop_requested ? "true" : "false");
    printf("stop_reason: %s\n", summary && summary->stop_reason ? summary->stop_reason : "internal-error");
    printf("stop_phase: %s\n",
           summary && summary->stop_phase ? summary->stop_phase : "preflight");
    printf("stop_step: %llu\n", summary ? summary->stop_step : 0ull);
    printf("stop_timing: %s\n",
           summary && summary->stop_timing ? summary->stop_timing : "preflight");
    printf("stop_after_append: %s\n",
           summary && summary->stop_after_append ? "true" : "false");
    printf("stop_before_append: %s\n",
           summary && summary->stop_before_append ? "true" : "false");
    printf("failure_stop: %s\n", summary && summary->failure_stop ? "true" : "false");
    printf("unsupported_stop_feature: %s\n",
           summary && summary->unsupported_stop_feature ? "true" : "false");
    printf("eos_policy: %s\n", summary && summary->eos_policy ? summary->eos_policy : "unsupported");
    printf("stop_token_policy: %s\n",
           summary && summary->stop_token_policy ? summary->stop_token_policy : "unsupported");
    printf("generation_checksum: %llu\n", summary ? summary->generation_checksum : 0ull);
    printf("sequence_checksum: %llu\n", summary ? summary->sequence_checksum : 0ull);
    generate_print_token_list("generated_token_ids",
                              summary ? summary->generated_tokens : NULL,
                              summary ? summary->generated_token_count : 0ull);
    generate_print_runtime_sequence(summary);
    printf("cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    printf("generation_" "rea" "dy: false\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("failed_phase: %s\n",
           summary && summary->failed_phase ? summary->failed_phase : "none");
    printf("failed_step: %llu\n", summary ? summary->failed_step : 0ull);
}

static void generate_mark_cleanup(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    summary->cleanup_attempted = 1;
    summary->cleanup_status = "pass";
}

static void generate_mark_failure(yvex_generate_summary *summary,
                                  const char *phase,
                                  const char *reason,
                                  unsigned long long step)
{
    if (!summary) {
        return;
    }
    summary->phase = "failed";
    summary->status = "generation-loop-failed-cleaned";
    summary->failed_phase = phase ? phase : "internal-error";
    summary->failed_step = step;
    summary->partial_generated_token_count = summary->generated_token_count;
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

static void generate_account_failed_sample(yvex_generate_summary *summary,
                                           const yvex_sampling_summary *sample)
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
        if (summary->decode_steps == 0ull) {
            summary->decode_steps += 1ull;
        }
        summary->logits_steps += 1ull;
    }
    if (sample->sample_created) {
        summary->candidate_token_seen = 1;
        summary->candidate_token_id = sample->selected_token_id;
        summary->candidate_logit = sample->selected_logit;
        summary->last_selected_token_id = sample->selected_token_id;
        summary->last_selected_logit = sample->selected_logit;
        summary->append_status = "candidate-" "rea" "dy";
    }
}

static void generate_record_candidate(yvex_generate_summary *summary,
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
}

static int generate_append_preflight(yvex_generate_summary *summary,
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

static int generate_append_commit(yvex_generate_summary *summary,
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

static int generate_parse_strategy(const char *text)
{
    return text && strcmp(text, "greedy") == 0;
}

int yvex_generate_command(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input prompt_input;
    yvex_token_input sequence;
    yvex_engine_options engine_options;
    yvex_decode_step_options decode_options;
    yvex_logits_buffer_options logits_options;
    yvex_sampling_options sample_options;
    yvex_sampling_summary sample_summary;
    yvex_generate_summary summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    yvex_kv_shape kv_shape;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    unsigned long long vocab_size = 0ull;
    unsigned long long layer_count = 0ull;
    unsigned long long layer_hidden_dim = 0ull;
    unsigned long long layer_head_dim = 0ull;
    unsigned long long layer_ffn_dim = 0ull;
    unsigned long long chunk_size = 0ull;
    unsigned long long position_start = 0ull;
    unsigned long long context_length = 0ull;
    unsigned long long logits_count = 16ull;
    unsigned long long max_new_tokens = 0ull;
    unsigned long long step;
    int max_new_tokens_seen = 0;
    int attach_kv = 0;
    int kv_shape_seen = 0;
    int layer_count_seen = 0;
    int layer_hidden_seen = 0;
    int layer_head_seen = 0;
    int layer_ffn_seen = 0;
    int chunk_size_seen = 0;
    int context_length_seen = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
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
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_generate_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--max-new-tokens") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &max_new_tokens)) {
                fprintf(stderr, "yvex: --max-new-tokens requires a positive integer\n");
                return 2;
            }
            max_new_tokens_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc || !generate_parse_strategy(argv[i + 1])) {
                fprintf(stderr, "yvex: --strategy supports only greedy\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &logits_count) ||
                !generate_logits_count_valid(logits_count)) {
                fprintf(stderr, "yvex: --logits-count requires 1 <= N <= 256\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.layer_count)) {
                fprintf(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.kv_head_count)) {
                fprintf(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.head_dim)) {
                fprintf(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.capacity)) {
                fprintf(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_count)) {
                fprintf(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            layer_count_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_hidden_dim)) {
                fprintf(stderr, "yvex: --layer-hidden-dim requires a positive integer\n");
                return 2;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_head_dim)) {
                fprintf(stderr, "yvex: --layer-head-dim requires a positive integer\n");
                return 2;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &layer_ffn_dim)) {
                fprintf(stderr, "yvex: --layer-ffn-dim requires a positive integer\n");
                return 2;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &chunk_size)) {
                fprintf(stderr, "yvex: --chunk-size requires a positive integer\n");
                return 2;
            }
            chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position-start") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &position_start)) {
                fprintf(stderr, "yvex: --position-start requires a non-negative integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --context-length requires a positive integer\n");
                return 2;
            }
            context_length_seen = 1;
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown generate option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help generate' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !backend_name || !tokens_text || !segment_name || !max_new_tokens_seen) {
        fprintf(stderr, "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N] [--strategy greedy]\n");
        return 2;
    }
    if (!generate_backend_name_valid(backend_name)) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        fprintf(stderr, "yvex: unsupported generate segment: %s\n", segment_name);
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        fprintf(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        fprintf(stderr, "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }
    if (!layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        fprintf(stderr, "yvex: --layer-* options require --layers N\n");
        return 2;
    }
    if (layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        fprintf(stderr, "yvex: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim\n");
        return 2;
    }
    if (layer_count_seen && (layer_count == 0ull || layer_count > 16ull)) {
        fprintf(stderr, "yvex: --layers requires 1 <= N <= 16\n");
        return 2;
    }
    if (layer_count_seen && !layer_hidden_seen) {
        layer_hidden_dim = 8ull;
        layer_head_dim = 8ull;
        layer_ffn_dim = 16ull;
    }
    if (layer_count_seen && layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        fprintf(stderr, "yvex: --layer-hidden-dim cannot exceed %u for sampled segment handoff\n",
                (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    generate_summary_defaults(&summary, NULL, position_start, max_new_tokens);
    rc = enforce_registered_identity_cli(&model_ref, "generate");
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_summary(&summary, model_arg, backend_name, segment_name);
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(tokens_text, &prompt_input, &err);
    if (rc == YVEX_OK) {
        rc = cli_token_input_vocab_from_model(model_ref.path, &vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&prompt_input, vocab_size, &err);
    }
    generate_summary_defaults(&summary, &prompt_input, position_start, max_new_tokens);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_summary(&summary, model_arg, backend_name, segment_name);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    summary.token_input_status = "pass";
    sequence = prompt_input;

    if (!context_length_seen &&
        !generate_compute_default_context(position_start,
                                          prompt_input.token_count,
                                          max_new_tokens,
                                          &context_length)) {
        yvex_error_set(&err, YVEX_ERR_BOUNDS, "generate", "context length overflow");
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_summary(&summary, model_arg, backend_name, segment_name);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_BOUNDS));
    }
    summary.context_length = context_length;

    rc = preflight_graph_guard(&model_ref,
                               backend_name,
                               0,
                               1,
                               prompt_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_summary(&summary, model_arg, backend_name, segment_name);
        print_graph_guard_report(&graph_guard);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_summary(&summary, model_arg, backend_name, segment_name);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    for (step = 0ull; step < max_new_tokens; ++step) {
        yvex_generation_append_state append_state;
        unsigned long long decode_position;
        int context_stop = 0;

        if (!generate_add_ull(position_start, sequence.token_count, &decode_position)) {
            yvex_error_set(&err, YVEX_ERR_BOUNDS, "generate", "decode position overflow");
            generate_mark_failure(&summary, "stop-check", "internal-error", step);
            break;
        }
        summary.current_decode_position = decode_position;
        if (decode_position >= context_length) {
            summary.phase = "complete";
            summary.status = "generation-loop-complete";
            summary.append_status = "context-limit";
            summary.append_failure = "none";
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
        summary.prefill_invoked = 1;
        decode_options.token_input = &sequence;
        decode_options.segment_name = segment_name;
        decode_options.backend_name = backend_name;
        decode_options.position_start = position_start;
        decode_options.chunk_size = chunk_size_seen ? chunk_size : 0ull;
        decode_options.context_length = context_length;
        decode_options.attach_kv = attach_kv;
        decode_options.kv_shape = kv_shape;
        decode_options.layer_count = layer_count_seen ? layer_count : 0ull;
        decode_options.layer_hidden_dim = layer_hidden_dim;
        decode_options.layer_head_dim = layer_head_dim;
        decode_options.layer_ffn_dim = layer_ffn_dim;
        logits_options.decode_options = &decode_options;
        logits_options.logits_count = logits_count;
        sample_options.logits_options = &logits_options;
        sample_options.strategy = YVEX_SAMPLING_STRATEGY_GREEDY;

        summary.phase = "sample";
        rc = yvex_engine_sample_token(engine, &sample_options, &sample_summary, &err);
        if (rc != YVEX_OK) {
            generate_account_failed_sample(&summary, &sample_summary);
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
                                       &err);
        if (context_stop) {
            break;
        }
        if (rc != YVEX_OK) {
            generate_mark_failure(&summary, "append", "append-failure", step);
            break;
        }
        rc = generate_append_commit(&summary, &sequence, &append_state, &err);
        if (rc != YVEX_OK) {
            generate_mark_failure(&summary, "append", "append-failure", step);
            break;
        }

        summary.phase = "stop-check";
        if (summary.generated_token_count >= max_new_tokens) {
            summary.status = "generation-loop-complete";
            summary.phase = "complete";
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
        summary.generated_token_count >= max_new_tokens &&
        summary.phase &&
        strcmp(summary.phase, "failed") != 0) {
        summary.status = "generation-loop-complete";
        summary.phase = "complete";
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
    generate_print_summary(&summary, model_arg, backend_name, segment_name);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    if (summary.phase && strcmp(summary.phase, "failed") == 0) {
        return print_yvex_error(&err, exit_for_status(rc != YVEX_OK ? rc : YVEX_ERR_STATE));
    }
    yvex_error_clear(&err);
    return 0;
}

void yvex_generate_help(FILE *fp)
{
    fprintf(fp, "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [--layers N [--layer-hidden-dim N --layer-head-dim N --layer-ffn-dim N]] [--chunk-size N] [--position-start N] [--context-length N] [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N] [--logits-count N] [--strategy greedy]\n\nGenerate runs a bounded diagnostic loop over the existing prefill, decode, logits, greedy selection, and token-append path. It uses a bounded diagnostic stop policy, stops on max-new-tokens or context-length, reports decode/logits/sample/append failure stop reasons, and keeps EOS and stop-token policies unsupported. It does not claim full model generation, DeepSeek generation, provider/server output, evaluation, or benchmark measurement.\n");
}
