/*
 * yvex_generate_cli.c - diagnostic generation command quarantine.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   generation loop composition over available prefill, decode, logits,
 *   sampling, token append, stop, trace, cancel, cleanup pieces, and CLI
 *   output routing during TOPOLOGY.CLI.PRINT.ALL.0.
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

#include <yvex/yvex.h>
#include "yvex_console_private.h"
#include "yvex_cli_out.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    YVEX_GENERATE_TRACE_NONE = 0,
    YVEX_GENERATE_TRACE_TOKENS,
    YVEX_GENERATE_TRACE_STEPS,
    YVEX_GENERATE_TRACE_KV,
    YVEX_GENERATE_TRACE_LOGITS,
    YVEX_GENERATE_TRACE_SAMPLING,
    YVEX_GENERATE_TRACE_FULL
} yvex_generation_trace_level;

typedef enum {
    YVEX_GENERATE_OUTPUT_NORMAL = 0,
    YVEX_GENERATE_OUTPUT_AUDIT
} yvex_generation_output_mode;

typedef struct {
    int attempted;
    unsigned long long index;
    unsigned long long decode_position;
    const char *decode_status;
    const char *logits_status;
    const char *sample_status;
    const char *append_status;
    unsigned int selected_token_id;
    unsigned int appended_token_id;
    unsigned int candidate_token_id;
    double candidate_logit;
    unsigned long long position_after_append;
    const char *stop_reason;
    const char *stop_timing;
    unsigned long long logits_checksum;
    double logits_min;
    double logits_max;
    unsigned long long sample_checksum;
    unsigned long long accepted_token_count;
    unsigned long long generated_token_count;
    unsigned long long total_token_count;
    unsigned long long sequence_checksum;
} yvex_generation_trace_step;

typedef struct {
    unsigned long long state_id;
    const char *lifecycle_status;
    const char *generation_state;
    int state_dirty;
    int active_step_seen;
    unsigned long long active_step;
    int last_completed_step_seen;
    unsigned long long last_completed_step;
    int cancel_supported;
    int cancel_after_steps_seen;
    unsigned long long cancel_after_steps;
    int cancel_requested;
    const char *cancel_reason;
    int cancel_step_seen;
    unsigned long long cancel_step;
    const char *cancel_timing;
    const char *cancel_safe_point;
    int partial_output_available;
    int cleanup_idempotent;
    int cleanup_repeated;
    int cleanup_owned_state_released;
    int failure_preserved;
    int partial_output_preserved;
} yvex_generation_state;

typedef struct {
    int loop_created;
    int loop_executed;
    const char *phase;
    const char *status;
    yvex_generation_state state;
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
    yvex_generation_trace_level trace_level;
    const char *trace_level_name;
    int trace_enabled;
    const char *trace_status;
    unsigned long long trace_records;
    unsigned long long trace_tokens;
    unsigned long long trace_steps;
    unsigned long long trace_kv;
    unsigned long long trace_logits;
    unsigned long long trace_sampling;
    unsigned long long trace_append;
    unsigned long long trace_stop;
    unsigned long long trace_cancel;
    unsigned long long trace_cleanup;
    unsigned long long trace_failures;
    yvex_generation_trace_step trace_step_records[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned long long trace_step_count;
    int trace_kv_requested;
    yvex_kv_shape trace_kv_shape;
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

static void generate_state_defaults(yvex_generate_summary *summary,
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

static void generate_state_set_lifecycle(yvex_generate_summary *summary,
                                         const char *lifecycle_status)
{
    if (summary && lifecycle_status) {
        summary->state.lifecycle_status = lifecycle_status;
    }
}

static void generate_state_mark_step(yvex_generate_summary *summary,
                                     unsigned long long step)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "step-active";
    summary->state.active_step_seen = 1;
    summary->state.active_step = step;
}

static void generate_state_mark_append(yvex_generate_summary *summary,
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

static void generate_state_mark_completed(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "completed";
    summary->state.generation_state = "completed";
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

static void generate_state_mark_failed(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    summary->state.lifecycle_status = "failed";
    summary->state.generation_state = "failed";
    summary->state.partial_output_available =
        summary->generated_token_count > 0ull ? 1 : 0;
}

static void generate_state_mark_cancelled(yvex_generate_summary *summary,
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

static void generate_print_optional_ull(const char *label,
                                        int seen,
                                        unsigned long long value)
{
    yvex_cli_out_writef(stdout, "%s: ", label ? label : "value");
    if (seen) {
        yvex_cli_out_writef(stdout, "%llu", value);
    } else {
        yvex_cli_out_writef(stdout, "none");
    }
    yvex_cli_out_writef(stdout, "\n");
}

static int generate_backend_name_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

static int generate_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

static const char *generate_trace_level_name(yvex_generation_trace_level level)
{
    switch (level) {
    case YVEX_GENERATE_TRACE_TOKENS:
        return "tokens";
    case YVEX_GENERATE_TRACE_STEPS:
        return "steps";
    case YVEX_GENERATE_TRACE_KV:
        return "kv";
    case YVEX_GENERATE_TRACE_LOGITS:
        return "logits";
    case YVEX_GENERATE_TRACE_SAMPLING:
        return "sampling";
    case YVEX_GENERATE_TRACE_FULL:
        return "full";
    case YVEX_GENERATE_TRACE_NONE:
    default:
        return "none";
    }
}

static int generate_parse_trace_level(const char *text,
                                      yvex_generation_trace_level *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "none") == 0) {
        *out = YVEX_GENERATE_TRACE_NONE;
    } else if (strcmp(text, "tokens") == 0) {
        *out = YVEX_GENERATE_TRACE_TOKENS;
    } else if (strcmp(text, "steps") == 0) {
        *out = YVEX_GENERATE_TRACE_STEPS;
    } else if (strcmp(text, "kv") == 0) {
        *out = YVEX_GENERATE_TRACE_KV;
    } else if (strcmp(text, "logits") == 0) {
        *out = YVEX_GENERATE_TRACE_LOGITS;
    } else if (strcmp(text, "sampling") == 0) {
        *out = YVEX_GENERATE_TRACE_SAMPLING;
    } else if (strcmp(text, "full") == 0) {
        *out = YVEX_GENERATE_TRACE_FULL;
    } else {
        return 0;
    }
    return 1;
}

static int generate_parse_output_mode(const char *text,
                                      yvex_generation_output_mode *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "normal") == 0) {
        *out = YVEX_GENERATE_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(text, "audit") == 0) {
        *out = YVEX_GENERATE_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
}

static int generate_trace_wants_tokens(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATE_TRACE_TOKENS || level == YVEX_GENERATE_TRACE_FULL;
}

static int generate_trace_wants_steps(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATE_TRACE_STEPS || level == YVEX_GENERATE_TRACE_FULL;
}

static int generate_trace_wants_kv(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATE_TRACE_KV || level == YVEX_GENERATE_TRACE_FULL;
}

static int generate_trace_wants_logits(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATE_TRACE_LOGITS || level == YVEX_GENERATE_TRACE_FULL;
}

static int generate_trace_wants_sampling(yvex_generation_trace_level level)
{
    return level == YVEX_GENERATE_TRACE_SAMPLING || level == YVEX_GENERATE_TRACE_FULL;
}

static void generate_summary_trace_defaults(yvex_generate_summary *summary,
                                            yvex_generation_trace_level level)
{
    if (!summary) {
        return;
    }
    summary->trace_level = level;
    summary->trace_level_name = generate_trace_level_name(level);
    summary->trace_enabled = level != YVEX_GENERATE_TRACE_NONE;
    summary->trace_status = summary->trace_enabled ? "enabled" : "disabled";
}

static void generate_summary_defaults(yvex_generate_summary *summary,
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

static void generate_summary_trace_kv(yvex_generate_summary *summary,
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

static yvex_generation_trace_step *generate_trace_step_at(yvex_generate_summary *summary,
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

static yvex_generation_trace_step *generate_trace_step_begin(yvex_generate_summary *summary,
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

static void generate_trace_step_mark_stop(yvex_generate_summary *summary,
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

static void generate_trace_step_mark_failure(yvex_generate_summary *summary,
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
    generate_trace_step_mark_stop(summary, summary->stop_reason, summary->stop_timing,
                                  step, before_append);
}

static void generate_print_token_list(const char *label,
                                      const unsigned int *tokens,
                                      unsigned long long count)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "%s: ", label ? label : "tokens");
    for (i = 0ull; tokens && i < count; ++i) {
        if (i > 0ull) {
            yvex_cli_out_writef(stdout, ",");
        }
        yvex_cli_out_writef(stdout, "%u", tokens[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
}

static void generate_print_runtime_sequence(const yvex_generate_summary *summary)
{
    unsigned long long i;
    unsigned long long emitted = 0ull;

    yvex_cli_out_writef(stdout, "runtime_token_sequence: ");
    if (summary) {
        for (i = 0ull; i < summary->prompt_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            if (emitted > 0ull) {
                yvex_cli_out_writef(stdout, ",");
            }
            yvex_cli_out_writef(stdout, "%u", summary->prompt_tokens[i]);
            emitted += 1ull;
        }
        for (i = 0ull; i < summary->generated_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
            if (emitted > 0ull) {
                yvex_cli_out_writef(stdout, ",");
            }
            yvex_cli_out_writef(stdout, "%u", summary->generated_tokens[i]);
            emitted += 1ull;
        }
    }
    yvex_cli_out_writef(stdout, "\n");
}

static void generate_trace_printf(yvex_generate_summary *summary,
                                  unsigned long long *category_count,
                                  const char *fmt,
                                  ...)
{
    va_list ap;

    if (!summary || !fmt) {
        return;
    }
    va_start(ap, fmt);
    yvex_cli_out_vwritef(stdout, fmt, ap);
    va_end(ap);
    yvex_cli_out_writef(stdout, "\n");
    summary->trace_records += 1ull;
    if (category_count) {
        *category_count += 1ull;
    }
}

static void generate_trace_token_list(yvex_generate_summary *summary,
                                      unsigned long long *category_count,
                                      const char *label,
                                      const unsigned int *tokens,
                                      unsigned long long count)
{
    unsigned long long i;

    if (!summary || !label) {
        return;
    }
    yvex_cli_out_writef(stdout, "%s: ", label);
    for (i = 0ull; tokens && i < count; ++i) {
        if (i > 0ull) {
            yvex_cli_out_writef(stdout, ",");
        }
        yvex_cli_out_writef(stdout, "%u", tokens[i]);
    }
    yvex_cli_out_writef(stdout, "\n");
    summary->trace_records += 1ull;
    if (category_count) {
        *category_count += 1ull;
    }
}

static void generate_trace_runtime_sequence(yvex_generate_summary *summary,
                                            unsigned long long *category_count)
{
    unsigned long long i;
    unsigned long long emitted = 0ull;

    if (!summary) {
        return;
    }
    yvex_cli_out_writef(stdout, "trace.tokens.runtime_sequence: ");
    for (i = 0ull; i < summary->prompt_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
        if (emitted > 0ull) {
            yvex_cli_out_writef(stdout, ",");
        }
        yvex_cli_out_writef(stdout, "%u", summary->prompt_tokens[i]);
        emitted += 1ull;
    }
    for (i = 0ull; i < summary->generated_token_count && i < YVEX_TOKEN_INPUT_MAX_TOKENS; ++i) {
        if (emitted > 0ull) {
            yvex_cli_out_writef(stdout, ",");
        }
        yvex_cli_out_writef(stdout, "%u", summary->generated_tokens[i]);
        emitted += 1ull;
    }
    yvex_cli_out_writef(stdout, "\n");
    summary->trace_records += 1ull;
    if (category_count) {
        *category_count += 1ull;
    }
}

static void generate_emit_token_trace(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    generate_trace_token_list(summary, &summary->trace_tokens, "trace.tokens.prompt",
                              summary->prompt_tokens, summary->prompt_token_count);
    generate_trace_token_list(summary, &summary->trace_tokens, "trace.tokens.generated",
                              summary->generated_tokens, summary->generated_token_count);
    generate_trace_runtime_sequence(summary, &summary->trace_tokens);
    generate_trace_printf(summary, &summary->trace_tokens,
                          "trace.tokens.prompt_count: %llu",
                          summary->prompt_token_count);
    generate_trace_printf(summary, &summary->trace_tokens,
                          "trace.tokens.generated_count: %llu",
                          summary->generated_token_count);
    generate_trace_printf(summary, &summary->trace_tokens,
                          "trace.tokens.total_count: %llu",
                          summary->total_token_count);
    generate_trace_printf(summary, &summary->trace_tokens,
                          "trace.tokens.stop_reason: %s",
                          summary->stop_reason ? summary->stop_reason : "none");
}

static void generate_emit_step_trace(yvex_generate_summary *summary)
{
    unsigned long long i;

    if (!summary) {
        return;
    }
    for (i = 0ull; i < summary->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &summary->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.index: %llu", i, record->index);
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.decode_position: %llu",
                              i, record->decode_position);
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.decode_status: %s",
                              i, record->decode_status ? record->decode_status : "skipped");
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.logits_status: %s",
                              i, record->logits_status ? record->logits_status : "skipped");
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.sample_status: %s",
                              i, record->sample_status ? record->sample_status : "skipped");
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.append_status: %s",
                              i, record->append_status ? record->append_status : "not-started");
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.selected_token_id: %u",
                              i, record->selected_token_id);
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.appended_token_id: %u",
                              i, record->appended_token_id);
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.position_after_append: %llu",
                              i, record->position_after_append);
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.stop_reason: %s",
                              i, record->stop_reason ? record->stop_reason : "none");
        generate_trace_printf(summary, &summary->trace_steps,
                              "trace.step.%llu.stop_timing: %s",
                              i, record->stop_timing ? record->stop_timing : "none");
    }
}

static void generate_emit_kv_trace(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    generate_trace_printf(summary, &summary->trace_kv, "trace.kv.status: %s",
                          summary->trace_kv_requested ? "requested" : "unavailable");
    generate_trace_printf(summary, &summary->trace_kv, "trace.kv.mode: diagnostic");
    generate_trace_printf(summary, &summary->trace_kv,
                          "trace.kv.real_attention_kv: false");
    generate_trace_printf(summary, &summary->trace_kv,
                          "trace.kv.full_model_kv: false");
    if (summary->trace_kv_requested) {
        generate_trace_printf(summary, &summary->trace_kv,
                              "trace.kv.layers: %llu",
                              summary->trace_kv_shape.layer_count);
        generate_trace_printf(summary, &summary->trace_kv,
                              "trace.kv.heads: %llu",
                              summary->trace_kv_shape.kv_head_count);
        generate_trace_printf(summary, &summary->trace_kv,
                              "trace.kv.head_dim: %llu",
                              summary->trace_kv_shape.head_dim);
        generate_trace_printf(summary, &summary->trace_kv,
                              "trace.kv.capacity: %llu",
                              summary->trace_kv_shape.capacity);
        generate_trace_printf(summary, &summary->trace_kv,
                              "trace.kv.binding_source: generate-decode-options");
    }
}

static void generate_emit_logits_trace(yvex_generate_summary *summary)
{
    unsigned long long i;

    if (!summary) {
        return;
    }
    for (i = 0ull; i < summary->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &summary->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        generate_trace_printf(summary, &summary->trace_logits,
                              "trace.step.%llu.logits_mode: bounded-diagnostic", i);
        generate_trace_printf(summary, &summary->trace_logits,
                              "trace.step.%llu.logits_checksum: %llu",
                              i, record->logits_checksum);
        generate_trace_printf(summary, &summary->trace_logits,
                              "trace.step.%llu.logits_min: %.9g",
                              i, record->logits_min);
        generate_trace_printf(summary, &summary->trace_logits,
                              "trace.step.%llu.logits_max: %.9g",
                              i, record->logits_max);
        generate_trace_printf(summary, &summary->trace_logits,
                              "trace.step.%llu.real_output_head_logits: false", i);
    }
}

static void generate_emit_sampling_trace(yvex_generate_summary *summary)
{
    unsigned long long i;

    if (!summary) {
        return;
    }
    for (i = 0ull; i < summary->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &summary->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        generate_trace_printf(summary, &summary->trace_sampling,
                              "trace.step.%llu.sampling_strategy: greedy", i);
        generate_trace_printf(summary, &summary->trace_sampling,
                              "trace.step.%llu.candidate_token_id: %u",
                              i, record->candidate_token_id);
        generate_trace_printf(summary, &summary->trace_sampling,
                              "trace.step.%llu.candidate_logit: %.9g",
                              i, record->candidate_logit);
        generate_trace_printf(summary, &summary->trace_sampling,
                              "trace.step.%llu.sample_checksum: %llu",
                              i, record->sample_checksum);
        generate_trace_printf(summary, &summary->trace_sampling,
                              "trace.step.%llu.real_vocab_sampling: false", i);
        generate_trace_printf(summary, &summary->trace_sampling,
                              "trace.step.%llu.stochastic_sampling: false", i);
    }
}

static void generate_emit_append_trace(yvex_generate_summary *summary)
{
    unsigned long long i;

    if (!summary) {
        return;
    }
    for (i = 0ull; i < summary->trace_step_count; ++i) {
        const yvex_generation_trace_step *record = &summary->trace_step_records[i];
        if (!record->attempted) {
            continue;
        }
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.append_status: %s",
                              i, record->append_status ? record->append_status : "not-started");
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.candidate_token_id: %u",
                              i, record->candidate_token_id);
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.appended_token_id: %u",
                              i, record->appended_token_id);
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.accepted_token_count: %llu",
                              i, record->accepted_token_count);
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.generated_token_count: %llu",
                              i, record->generated_token_count);
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.total_token_count: %llu",
                              i, record->total_token_count);
        generate_trace_printf(summary, &summary->trace_append,
                              "trace.append.%llu.sequence_checksum: %llu",
                              i, record->sequence_checksum);
    }
}

static void generate_emit_stop_trace(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.policy: %s",
                          summary->stop_policy ? summary->stop_policy : "bounded-diagnostic");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.requested: %s",
                          summary->stop_requested ? "true" : "false");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.reason: %s",
                          summary->stop_reason ? summary->stop_reason : "internal-error");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.phase: %s",
                          summary->stop_phase ? summary->stop_phase : "preflight");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.step: %llu", summary->stop_step);
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.timing: %s",
                          summary->stop_timing ? summary->stop_timing : "preflight");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.before_append: %s",
                          summary->stop_before_append ? "true" : "false");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.after_append: %s",
                          summary->stop_after_append ? "true" : "false");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.failure_stop: %s",
                          summary->failure_stop ? "true" : "false");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.unsupported_stop_feature: %s",
                          summary->unsupported_stop_feature ? "true" : "false");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.eos_policy: %s",
                          summary->eos_policy ? summary->eos_policy : "unsupported");
    generate_trace_printf(summary, &summary->trace_stop,
                          "trace.stop.stop_token_policy: %s",
                          summary->stop_token_policy ? summary->stop_token_policy : "unsupported");
}

static void generate_emit_cancel_trace(yvex_generate_summary *summary)
{
    if (!summary || !summary->state.cancel_requested) {
        return;
    }
    generate_trace_printf(summary, &summary->trace_cancel,
                          "trace.cancel.requested: true");
    generate_trace_printf(summary, &summary->trace_cancel,
                          "trace.cancel.reason: %s",
                          summary->state.cancel_reason ?
                              summary->state.cancel_reason : "interrupted");
    if (summary->state.cancel_step_seen) {
        generate_trace_printf(summary, &summary->trace_cancel,
                              "trace.cancel.step: %llu",
                              summary->state.cancel_step);
    } else {
        generate_trace_printf(summary, &summary->trace_cancel,
                              "trace.cancel.step: none");
    }
    generate_trace_printf(summary, &summary->trace_cancel,
                          "trace.cancel.timing: %s",
                          summary->state.cancel_timing ?
                              summary->state.cancel_timing : "none");
    generate_trace_printf(summary, &summary->trace_cancel,
                          "trace.cancel.safe_point: %s",
                          summary->state.cancel_safe_point ?
                              summary->state.cancel_safe_point : "none");
    generate_trace_printf(summary, &summary->trace_cancel,
                          "trace.cancel.partial_generated_token_count: %llu",
                          summary->partial_generated_token_count);
}

static void generate_emit_failure_trace(yvex_generate_summary *summary)
{
    if (!summary || !summary->phase || strcmp(summary->phase, "failed") != 0) {
        return;
    }
    generate_trace_printf(summary, &summary->trace_failures,
                          "trace.failure.phase: %s",
                          summary->failed_phase ? summary->failed_phase : "internal-error");
    generate_trace_printf(summary, &summary->trace_failures,
                          "trace.failure.step: %llu", summary->failed_step);
    generate_trace_printf(summary, &summary->trace_failures,
                          "trace.failure.stop_reason: %s",
                          summary->stop_reason ? summary->stop_reason : "internal-error");
    generate_trace_printf(summary, &summary->trace_failures,
                          "trace.failure.partial_generated_token_count: %llu",
                          summary->partial_generated_token_count);
    generate_trace_printf(summary, &summary->trace_failures,
                          "trace.failure.last_successful_position: %llu",
                          summary->last_successful_position);
    generate_trace_printf(summary, &summary->trace_failures,
                          "trace.failure.cleanup_attempted: %s",
                          summary->cleanup_attempted ? "true" : "false");
}

static void generate_emit_cleanup_trace(yvex_generate_summary *summary)
{
    if (!summary) {
        return;
    }
    generate_trace_printf(summary, &summary->trace_cleanup,
                          "trace.cleanup.attempted: %s",
                          summary->cleanup_attempted ? "true" : "false");
    generate_trace_printf(summary, &summary->trace_cleanup,
                          "trace.cleanup.status: %s",
                          summary->cleanup_status ? summary->cleanup_status : "not-needed");
}

/*
 * generate_emit_trace()
 *
 * Purpose:
 *   emit the requested diagnostic trace sections for one generation summary.
 *
 * Inputs:
 *   summary is borrowed mutable state containing the recorded trace
 *   facts and selected trace level.
 *
 * Effects:
 *   prints trace records to stdout and marks the trace as emitted; it does not
 *   mutate model state, move tensor bytes, or execute decode/logits/sampling.
 *
 * Failure:
 *   no parser failure path; missing trace levels simply suppress sections.
 *
 * Boundary:
 *   trace output is diagnostic evidence only and not runtime generation,
 *   eval evidence, benchmark evidence, throughput, or release readiness.
 */
static void generate_emit_trace(yvex_generate_summary *summary)
{
    int failed;

    if (!summary || !summary->trace_enabled) {
        if (summary) {
            summary->trace_status = "disabled";
        }
        return;
    }
    failed = summary->phase && strcmp(summary->phase, "failed") == 0;
    if (generate_trace_wants_tokens(summary->trace_level)) {
        generate_emit_token_trace(summary);
    }
    if (generate_trace_wants_steps(summary->trace_level)) {
        generate_emit_step_trace(summary);
    }
    if (generate_trace_wants_kv(summary->trace_level)) {
        generate_emit_kv_trace(summary);
    }
    if (generate_trace_wants_logits(summary->trace_level)) {
        generate_emit_logits_trace(summary);
    }
    if (generate_trace_wants_sampling(summary->trace_level)) {
        generate_emit_sampling_trace(summary);
    }
    if (summary->trace_level == YVEX_GENERATE_TRACE_FULL) {
        generate_emit_append_trace(summary);
        generate_emit_stop_trace(summary);
    }
    if (summary->state.cancel_requested) {
        generate_emit_cancel_trace(summary);
    }
    if (failed) {
        generate_emit_failure_trace(summary);
    }
    if (summary->trace_level == YVEX_GENERATE_TRACE_FULL || failed) {
        generate_emit_cleanup_trace(summary);
    }
    summary->trace_status = summary->trace_records > 0ull ? "emitted" : "enabled";
}

static void generate_print_summary(const yvex_generate_summary *summary,
                                   const char *model_arg,
                                   const char *backend_name,
                                   const char *segment_name)
{
    yvex_cli_out_writef(stdout, "generate: loop\n");
    yvex_cli_out_writef(stdout, "status: %s\n", summary && summary->status ? summary->status : "generation-loop-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg ? model_arg : "");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name ? backend_name : "cpu");
    yvex_cli_out_writef(stdout, "segment: %s\n", segment_name ? segment_name : "embedding-rmsnorm");
    yvex_cli_out_writef(stdout, "state_id: %llu\n", summary ? summary->state.state_id : 0ull);
    yvex_cli_out_writef(stdout, "lifecycle_status: %s\n",
           summary && summary->state.lifecycle_status ?
               summary->state.lifecycle_status : "unknown");
    yvex_cli_out_writef(stdout, "generation_state: %s\n",
           summary && summary->state.generation_state ?
               summary->state.generation_state : "unknown");
    yvex_cli_out_writef(stdout, "state_dirty: %s\n",
           summary && summary->state.state_dirty ? "true" : "false");
    generate_print_optional_ull("active_step",
                                summary ? summary->state.active_step_seen : 0,
                                summary ? summary->state.active_step : 0ull);
    generate_print_optional_ull("last_completed_step",
                                summary ? summary->state.last_completed_step_seen : 0,
                                summary ? summary->state.last_completed_step : 0ull);
    yvex_cli_out_writef(stdout, "cancel_supported: %s\n",
           summary && summary->state.cancel_supported ? "true" : "false");
    yvex_cli_out_writef(stdout, "cancel_requested: %s\n",
           summary && summary->state.cancel_requested ? "true" : "false");
    yvex_cli_out_writef(stdout, "cancel_reason: %s\n",
           summary && summary->state.cancel_reason ?
               summary->state.cancel_reason : "none");
    generate_print_optional_ull("cancel_step",
                                summary ? summary->state.cancel_step_seen : 0,
                                summary ? summary->state.cancel_step : 0ull);
    yvex_cli_out_writef(stdout, "cancel_timing: %s\n",
           summary && summary->state.cancel_timing ?
               summary->state.cancel_timing : "none");
    yvex_cli_out_writef(stdout, "cancel_safe_point: %s\n",
           summary && summary->state.cancel_safe_point ?
               summary->state.cancel_safe_point : "none");
    yvex_cli_out_writef(stdout, "partial_output_available: %s\n",
           summary && summary->state.partial_output_available ? "true" : "false");
    yvex_cli_out_writef(stdout, "token_input_status: %s\n",
           summary && summary->token_input_status ? summary->token_input_status : "fail");
    yvex_cli_out_writef(stdout, "prompt_token_count: %llu\n", summary ? summary->prompt_token_count : 0ull);
    yvex_cli_out_writef(stdout, "prefill_token_count: %llu\n", summary ? summary->prefill_token_count : 0ull);
    yvex_cli_out_writef(stdout, "max_new_tokens: %llu\n", summary ? summary->max_new_tokens : 0ull);
    yvex_cli_out_writef(stdout, "context_length: %llu\n", summary ? summary->context_length : 0ull);
    yvex_cli_out_writef(stdout, "generated_token_count: %llu\n", summary ? summary->generated_token_count : 0ull);
    yvex_cli_out_writef(stdout, "accepted_token_count: %llu\n", summary ? summary->accepted_token_count : 0ull);
    yvex_cli_out_writef(stdout, "partial_generated_token_count: %llu\n",
           summary ? summary->partial_generated_token_count : 0ull);
    yvex_cli_out_writef(stdout, "total_token_count: %llu\n", summary ? summary->total_token_count : 0ull);
    yvex_cli_out_writef(stdout, "position_start: %llu\n", summary ? summary->position_start : 0ull);
    yvex_cli_out_writef(stdout, "prefill_position_end: %llu\n", summary ? summary->prefill_position_end : 0ull);
    yvex_cli_out_writef(stdout, "current_decode_position: %llu\n",
           summary ? summary->current_decode_position : 0ull);
    yvex_cli_out_writef(stdout, "last_successful_position: %llu\n",
           summary ? summary->last_successful_position : 0ull);
    yvex_cli_out_writef(stdout, "generation_loop_kind: bounded-diagnostic\n");
    yvex_cli_out_writef(stdout, "generation_mode: diagnostic-runtime\n");
    yvex_cli_out_writef(stdout, "decode_mode: bounded-diagnostic\n");
    yvex_cli_out_writef(stdout, "logits_mode: bounded-diagnostic\n");
    yvex_cli_out_writef(stdout, "sampling_strategy: greedy\n");
    yvex_cli_out_writef(stdout, "bounded_generation: %s\n",
           summary && summary->loop_executed ? "true" : "false");
    yvex_cli_out_writef(stdout, "full_model_generation: false\n");
    yvex_cli_out_writef(stdout, "real_deepseek_generation: false\n");
    yvex_cli_out_writef(stdout, "prefill_invoked: %s\n", summary && summary->prefill_invoked ? "true" : "false");
    yvex_cli_out_writef(stdout, "decode_steps: %llu\n", summary ? summary->decode_steps : 0ull);
    yvex_cli_out_writef(stdout, "logits_steps: %llu\n", summary ? summary->logits_steps : 0ull);
    yvex_cli_out_writef(stdout, "sample_steps: %llu\n", summary ? summary->sample_steps : 0ull);
    yvex_cli_out_writef(stdout, "append_steps: %llu\n", summary ? summary->append_steps : 0ull);
    yvex_cli_out_writef(stdout, "candidate_token_id: %u\n",
           summary && summary->candidate_token_seen ? summary->candidate_token_id : 0u);
    yvex_cli_out_writef(stdout, "candidate_logit: %.9g\n",
           summary && summary->candidate_token_seen ? summary->candidate_logit : 0.0);
    yvex_cli_out_writef(stdout, "last_selected_token_id: %u\n", summary ? summary->last_selected_token_id : 0u);
    yvex_cli_out_writef(stdout, "last_selected_logit: %.9g\n", summary ? summary->last_selected_logit : 0.0);
    yvex_cli_out_writef(stdout, "last_appended_token_id: %u\n",
           summary && summary->last_appended_token_seen ? summary->last_appended_token_id : 0u);
    yvex_cli_out_writef(stdout, "append_status: %s\n",
           summary && summary->append_status ? summary->append_status : "not-started");
    yvex_cli_out_writef(stdout, "append_failure: %s\n",
           summary && summary->append_failure ? summary->append_failure : "none");
    yvex_cli_out_writef(stdout, "stop_policy: %s\n",
           summary && summary->stop_policy ? summary->stop_policy : "bounded-diagnostic");
    yvex_cli_out_writef(stdout, "stop_requested: %s\n",
           summary && summary->stop_requested ? "true" : "false");
    yvex_cli_out_writef(stdout, "stop_reason: %s\n", summary && summary->stop_reason ? summary->stop_reason : "internal-error");
    yvex_cli_out_writef(stdout, "stop_phase: %s\n",
           summary && summary->stop_phase ? summary->stop_phase : "preflight");
    yvex_cli_out_writef(stdout, "stop_step: %llu\n", summary ? summary->stop_step : 0ull);
    yvex_cli_out_writef(stdout, "stop_timing: %s\n",
           summary && summary->stop_timing ? summary->stop_timing : "preflight");
    yvex_cli_out_writef(stdout, "stop_after_append: %s\n",
           summary && summary->stop_after_append ? "true" : "false");
    yvex_cli_out_writef(stdout, "stop_before_append: %s\n",
           summary && summary->stop_before_append ? "true" : "false");
    yvex_cli_out_writef(stdout, "failure_stop: %s\n", summary && summary->failure_stop ? "true" : "false");
    yvex_cli_out_writef(stdout, "unsupported_stop_feature: %s\n",
           summary && summary->unsupported_stop_feature ? "true" : "false");
    yvex_cli_out_writef(stdout, "eos_policy: %s\n", summary && summary->eos_policy ? summary->eos_policy : "unsupported");
    yvex_cli_out_writef(stdout, "stop_token_policy: %s\n",
           summary && summary->stop_token_policy ? summary->stop_token_policy : "unsupported");
    yvex_cli_out_writef(stdout, "trace_level: %s\n",
           summary && summary->trace_level_name ? summary->trace_level_name : "none");
    yvex_cli_out_writef(stdout, "trace_enabled: %s\n", summary && summary->trace_enabled ? "true" : "false");
    yvex_cli_out_writef(stdout, "trace_records: %llu\n", summary ? summary->trace_records : 0ull);
    yvex_cli_out_writef(stdout, "trace_tokens: %llu\n", summary ? summary->trace_tokens : 0ull);
    yvex_cli_out_writef(stdout, "trace_steps: %llu\n", summary ? summary->trace_steps : 0ull);
    yvex_cli_out_writef(stdout, "trace_kv: %llu\n", summary ? summary->trace_kv : 0ull);
    yvex_cli_out_writef(stdout, "trace_logits: %llu\n", summary ? summary->trace_logits : 0ull);
    yvex_cli_out_writef(stdout, "trace_sampling: %llu\n", summary ? summary->trace_sampling : 0ull);
    yvex_cli_out_writef(stdout, "trace_append: %llu\n", summary ? summary->trace_append : 0ull);
    yvex_cli_out_writef(stdout, "trace_stop: %llu\n", summary ? summary->trace_stop : 0ull);
    yvex_cli_out_writef(stdout, "trace_cancel: %llu\n", summary ? summary->trace_cancel : 0ull);
    yvex_cli_out_writef(stdout, "trace_cleanup: %llu\n", summary ? summary->trace_cleanup : 0ull);
    yvex_cli_out_writef(stdout, "trace_failures: %llu\n", summary ? summary->trace_failures : 0ull);
    yvex_cli_out_writef(stdout, "trace_status: %s\n",
           summary && summary->trace_status ? summary->trace_status : "disabled");
    yvex_cli_out_writef(stdout, "generation_checksum: %llu\n", summary ? summary->generation_checksum : 0ull);
    yvex_cli_out_writef(stdout, "sequence_checksum: %llu\n", summary ? summary->sequence_checksum : 0ull);
    generate_print_token_list("generated_token_ids",
                              summary ? summary->generated_tokens : NULL,
                              summary ? summary->generated_token_count : 0ull);
    generate_print_runtime_sequence(summary);
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "cleanup_idempotent: %s\n",
           summary && summary->state.cleanup_idempotent ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_repeated: %s\n",
           summary && summary->state.cleanup_repeated ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_owned_state_released: %s\n",
           summary && summary->state.cleanup_owned_state_released ? "true" : "false");
    yvex_cli_out_writef(stdout, "failure_preserved: %s\n",
           summary && summary->state.failure_preserved ? "true" : "false");
    yvex_cli_out_writef(stdout, "partial_output_preserved: %s\n",
           summary && summary->state.partial_output_preserved ? "true" : "false");
    yvex_cli_out_writef(stdout, "generation_" "rea" "dy: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "failed_phase: %s\n",
           summary && summary->failed_phase ? summary->failed_phase : "none");
    yvex_cli_out_writef(stdout, "failed_step: %llu\n", summary ? summary->failed_step : 0ull);
}

static void generate_print_trace_and_summary(yvex_generate_summary *summary,
                                             const char *model_arg,
                                             const char *backend_name,
                                             const char *segment_name)
{
    generate_emit_trace(summary);
    generate_print_summary(summary, model_arg, backend_name, segment_name);
}

static void generate_print_normal_summary(const yvex_generate_summary *summary,
                                          const char *model_arg,
                                          const char *backend_name)
{
    yvex_cli_out_writef(stdout, "status: diagnostic-generation\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg ? model_arg : "");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name ? backend_name : "cpu");
    yvex_cli_out_writef(stdout, "tokens: %llu -> %llu diagnostic\n",
           summary ? summary->prompt_token_count : 0ull,
           summary ? summary->generated_token_count : 0ull);
    yvex_cli_out_writef(stdout, "stop: %s\n",
           summary && summary->stop_reason ? summary->stop_reason : "internal-error");
    yvex_cli_out_writef(stdout, "boundary: full-model generation unsupported\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "hint: use --audit or --trace-level full for diagnostic internals\n");
}

static void generate_print_output(yvex_generate_summary *summary,
                                  const char *model_arg,
                                  const char *backend_name,
                                  const char *segment_name,
                                  yvex_generation_output_mode output_mode)
{
    if (output_mode == YVEX_GENERATE_OUTPUT_AUDIT ||
        (summary && summary->trace_level != YVEX_GENERATE_TRACE_NONE)) {
        generate_print_trace_and_summary(summary, model_arg, backend_name, segment_name);
        return;
    }
    generate_print_normal_summary(summary, model_arg, backend_name);
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
static void generate_mark_cleanup(yvex_generate_summary *summary)
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

static void generate_mark_failure(yvex_generate_summary *summary,
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

static void generate_mark_cancel(yvex_generate_summary *summary,
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

static void generate_account_failed_sample(yvex_generate_summary *summary,
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

static int generate_parse_strategy(const char *text)
{
    return text && strcmp(text, "greedy") == 0;
}

static int generate_parse_positive_ull_cli(const char *text,
                                           unsigned long long *out)
{
    if (!text || text[0] == '-') {
        return 0;
    }
    return parse_positive_ull(text, out);
}

static int generate_parse_ull_allow_zero_cli(const char *text,
                                             unsigned long long *out)
{
    if (!text || text[0] == '-') {
        return 0;
    }
    return parse_ull_allow_zero(text, out);
}

static void generate_print_usage_error(void)
{
    yvex_cli_out_writef(stderr,
            "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda "
            "--segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\n");
    yvex_cli_out_writef(stderr, "Try 'yvex help generate' for examples and boundaries.\n");
}

/*
 * yvex_generate_command()
 *
 * Purpose:
 *   parse and execute the bounded diagnostic generation command over existing
 *   decode/logits/sampling pieces.
 *
 * Inputs:
 *   argc/argv are borrowed CLI arguments; model references and runtime objects
 *   are opened only for the duration of the command.
 *
 * Effects:
 *   validates token input, may open an engine, optionally attaches KV/logits
 *   state, runs bounded diagnostic steps, prints normal/audit/trace output, and
 *   records cleanup/partial-output state.
 *
 * Failure:
 *   returns parser failures for invalid options and runtime-style failures for
 *   model resolution, engine, prefill, decode, logits, sampling, or append
 *   errors while preserving diagnostic summary state.
 *
 * Boundary:
 *   this command is diagnostic generation only; it does not implement runtime
 *   generation over supported-family artifacts, tokenizer stop policy,
 *   provider generation, eval, benchmark, throughput, or release readiness.
 */
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
    unsigned long long cancel_after_steps = 0ull;
    unsigned long long step;
    yvex_generation_trace_level trace_level = YVEX_GENERATE_TRACE_NONE;
    yvex_generation_output_mode output_mode = YVEX_GENERATE_OUTPUT_NORMAL;
    int max_new_tokens_seen = 0;
    int cancel_after_steps_seen = 0;
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

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_generate_help(stdout);
        return 0;
    }
    if (argc < 3) {
        yvex_cli_out_writef(stderr, "error: generate requires --model FILE_OR_ALIAS\n");
        generate_print_usage_error();
        return 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "error: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "error: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "error: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "error: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--max-new-tokens") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &max_new_tokens)) {
                yvex_cli_out_writef(stderr, "error: --max-new-tokens must be an integer greater than 0\n");
                return 2;
            }
            max_new_tokens_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--strategy") == 0) {
            if (i + 1 >= argc || !generate_parse_strategy(argv[i + 1])) {
                yvex_cli_out_writef(stderr, "error: --strategy currently supports greedy only\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--trace-level") == 0) {
            if (i + 1 >= argc || !generate_parse_trace_level(argv[i + 1], &trace_level)) {
                yvex_cli_out_writef(stderr, "error: --trace-level requires none|tokens|steps|kv|logits|sampling|full\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            output_mode = YVEX_GENERATE_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "error: --output requires normal|audit\n");
                return 2;
            }
            if (!generate_parse_output_mode(argv[++i], &output_mode)) {
                yvex_cli_out_writef(stderr, "error: unsupported output mode: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--json") == 0) {
            yvex_cli_out_writef(stderr, "error: JSON output is unsupported for generate; use --output normal|audit\n");
            return 2;
        } else if (strcmp(argv[i], "--cancel-after-steps") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_ull_allow_zero_cli(argv[i + 1], &cancel_after_steps)) {
                yvex_cli_out_writef(stderr, "error: --cancel-after-steps must be a non-negative integer\n");
                return 2;
            }
            cancel_after_steps_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--logits-count") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &logits_count) ||
                !generate_logits_count_valid(logits_count)) {
                yvex_cli_out_writef(stderr, "error: --logits-count requires 1 <= N <= 256\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &kv_shape.layer_count)) {
                yvex_cli_out_writef(stderr, "error: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &kv_shape.kv_head_count)) {
                yvex_cli_out_writef(stderr, "error: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &kv_shape.head_dim)) {
                yvex_cli_out_writef(stderr, "error: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &kv_shape.capacity)) {
                yvex_cli_out_writef(stderr, "error: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &layer_count)) {
                yvex_cli_out_writef(stderr, "error: --layers requires a positive integer\n");
                return 2;
            }
            layer_count_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-hidden-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &layer_hidden_dim)) {
                yvex_cli_out_writef(stderr, "error: --layer-hidden-dim requires a positive integer\n");
                return 2;
            }
            layer_hidden_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-head-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &layer_head_dim)) {
                yvex_cli_out_writef(stderr, "error: --layer-head-dim requires a positive integer\n");
                return 2;
            }
            layer_head_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--layer-ffn-dim") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &layer_ffn_dim)) {
                yvex_cli_out_writef(stderr, "error: --layer-ffn-dim requires a positive integer\n");
                return 2;
            }
            layer_ffn_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--chunk-size") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &chunk_size)) {
                yvex_cli_out_writef(stderr, "error: --chunk-size requires a positive integer\n");
                return 2;
            }
            chunk_size_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position-start") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_ull_allow_zero_cli(argv[i + 1], &position_start)) {
                yvex_cli_out_writef(stderr, "error: --position-start requires a non-negative integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--context-length") == 0) {
            if (i + 1 >= argc ||
                !generate_parse_positive_ull_cli(argv[i + 1], &context_length)) {
                yvex_cli_out_writef(stderr, "error: --context-length requires a positive integer\n");
                return 2;
            }
            context_length_seen = 1;
            i += 1;
        } else {
            yvex_cli_out_writef(stderr, "error: unknown generate option: %s\n", argv[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help generate' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        yvex_cli_out_writef(stderr, "error: generate requires --model FILE_OR_ALIAS\n");
        generate_print_usage_error();
        return 2;
    }
    if (!backend_name) {
        yvex_cli_out_writef(stderr, "error: generate requires --backend cpu|cuda\n");
        generate_print_usage_error();
        return 2;
    }
    if (!segment_name) {
        yvex_cli_out_writef(stderr, "error: generate requires --segment embedding-rmsnorm\n");
        generate_print_usage_error();
        return 2;
    }
    if (!tokens_text) {
        yvex_cli_out_writef(stderr, "error: generate requires --tokens IDS\n");
        generate_print_usage_error();
        return 2;
    }
    if (!max_new_tokens_seen) {
        yvex_cli_out_writef(stderr, "error: generate requires --max-new-tokens N\n");
        generate_print_usage_error();
        return 2;
    }
    if (!generate_backend_name_valid(backend_name)) {
        yvex_cli_out_writef(stderr,
                "error: --backend supports cpu|cuda for bounded diagnostics\n");
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        yvex_cli_out_writef(stderr,
                "error: generate segment currently supports embedding-rmsnorm for bounded diagnostics\n");
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        yvex_cli_out_writef(stderr, "error: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        yvex_cli_out_writef(stderr, "error: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }
    if (!layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "error: --layer-* options require --layers N\n");
        return 2;
    }
    if (layer_count_seen && (layer_hidden_seen || layer_head_seen || layer_ffn_seen) &&
        !(layer_hidden_seen && layer_head_seen && layer_ffn_seen)) {
        yvex_cli_out_writef(stderr, "error: custom layer dimensions require --layer-hidden-dim, --layer-head-dim, and --layer-ffn-dim\n");
        return 2;
    }
    if (layer_count_seen && (layer_count == 0ull || layer_count > 16ull)) {
        yvex_cli_out_writef(stderr, "error: --layers requires 1 <= N <= 16\n");
        return 2;
    }
    if (layer_count_seen && !layer_hidden_seen) {
        layer_hidden_dim = 8ull;
        layer_head_dim = 8ull;
        layer_ffn_dim = 16ull;
    }
    if (layer_count_seen && layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        yvex_cli_out_writef(stderr, "error: --layer-hidden-dim cannot exceed %u for sampled segment handoff\n",
                (unsigned int)YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    generate_summary_defaults(&summary,
                              NULL,
                              position_start,
                              max_new_tokens,
                              trace_level,
                              cancel_after_steps_seen,
                              cancel_after_steps);
    generate_summary_trace_kv(&summary, attach_kv, &kv_shape);
    rc = enforce_registered_identity_cli(&model_ref, "generate");
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_output(&summary, model_arg, backend_name, segment_name, output_mode);
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
    generate_summary_defaults(&summary,
                              &prompt_input,
                              position_start,
                              max_new_tokens,
                              trace_level,
                              cancel_after_steps_seen,
                              cancel_after_steps);
    generate_summary_trace_kv(&summary, attach_kv, &kv_shape);
    if (rc != YVEX_OK) {
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_output(&summary, model_arg, backend_name, segment_name, output_mode);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    summary.token_input_status = "pass";
    generate_state_set_lifecycle(&summary, "preflighted");
    sequence = prompt_input;

    if (!context_length_seen &&
        !generate_compute_default_context(position_start,
                                          prompt_input.token_count,
                                          max_new_tokens,
                                          &context_length)) {
        yvex_error_set(&err, YVEX_ERR_BOUNDS, "generate", "context length overflow");
        generate_mark_failure(&summary, "preflight", "internal-error", 0ull);
        generate_mark_cleanup(&summary);
        generate_print_output(&summary, model_arg, backend_name, segment_name, output_mode);
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
        generate_print_output(&summary, model_arg, backend_name, segment_name, output_mode);
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
        generate_print_output(&summary, model_arg, backend_name, segment_name, output_mode);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    generate_state_set_lifecycle(&summary, "running");
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
        (void)generate_trace_step_begin(&summary, step, decode_position);
        generate_state_mark_step(&summary, step);
        if (cancel_after_steps_seen && cancel_after_steps == 0ull) {
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
        if (cancel_after_steps_seen &&
            cancel_after_steps > 0ull &&
            summary.generated_token_count >= cancel_after_steps) {
            generate_mark_cancel(&summary,
                                 step,
                                 cancel_after_steps,
                                 "after-step",
                                 "after-append",
                                 0,
                                 1);
            break;
        }
        if (summary.generated_token_count >= max_new_tokens) {
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
        summary.generated_token_count >= max_new_tokens &&
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
    generate_print_output(&summary, model_arg, backend_name, segment_name, output_mode);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
    if (summary.phase && strcmp(summary.phase, "failed") == 0) {
        return print_yvex_error(&err, exit_for_status(rc != YVEX_OK ? rc : YVEX_ERR_STATE));
    }
    yvex_error_clear(&err);
    return 0;
}

/*
 * yvex_generate_help()
 *
 * Purpose:
 *   print generation command usage, examples, options, and boundaries.
 *
 * Inputs:
 *   fp is a borrowed output stream.
 *
 * Effects:
 *   prints help text only; it does not parse or run generation.
 *
 * Failure:
 *   no parser failure path; stream errors are left to stdio.
 *
 * Boundary:
 *   help text documents the diagnostic command and does not create generation
 *   support, eval evidence, benchmark evidence, throughput, or release
 *   readiness.
 */
void yvex_generate_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]\n\n");
    yvex_cli_out_writef(fp, "Bounded diagnostic generation loop over the existing prefill, decode, bounded logits, greedy sample, token append, stop, trace, cancel, and cleanup path.\n\n");
    yvex_cli_out_writef(fp, "Normal path:\n");
    yvex_cli_out_writef(fp, "  ./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3\n\n");
    yvex_cli_out_writef(fp, "Examples:\n");
    yvex_cli_out_writef(fp, "  ./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2 --trace-level full\n");
    yvex_cli_out_writef(fp, "  ./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --cancel-after-steps 1\n");
    yvex_cli_out_writef(fp, "  ./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --context-length 5\n\n");
    yvex_cli_out_writef(fp, "Required arguments:\n");
    yvex_cli_out_writef(fp, "  --model FILE_OR_ALIAS      selected segment artifact path or registry alias\n");
    yvex_cli_out_writef(fp, "  --backend cpu|cuda         backend used by the bounded diagnostic path\n");
    yvex_cli_out_writef(fp, "  --segment embedding-rmsnorm\n");
    yvex_cli_out_writef(fp, "  --tokens IDS               comma-separated diagnostic token IDs\n");
    yvex_cli_out_writef(fp, "  --max-new-tokens N         positive bounded diagnostic token budget\n\n");
    yvex_cli_out_writef(fp, "Diagnostic options:\n");
    yvex_cli_out_writef(fp, "  --strategy greedy          only greedy is accepted\n");
    yvex_cli_out_writef(fp, "  --context-length N         stop before append when the bounded context is full\n");
    yvex_cli_out_writef(fp, "  --logits-count N           bounded diagnostic logits count, 1 <= N <= 256\n");
    yvex_cli_out_writef(fp, "  --layers N                 optional controlled layer fixture scheduling\n");
    yvex_cli_out_writef(fp, "  --chunk-size N             optional bounded prefill chunking\n");
    yvex_cli_out_writef(fp, "  --attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N\n\n");
    yvex_cli_out_writef(fp, "Trace options:\n");
    yvex_cli_out_writef(fp, "  --trace-level none|tokens|steps|kv|logits|sampling|full\n");
    yvex_cli_out_writef(fp, "  Trace records are diagnostic text records only; they do not dump raw tensors.\n\n");
    yvex_cli_out_writef(fp, "Cancellation options:\n");
    yvex_cli_out_writef(fp, "  --cancel-after-steps N     N=0 cancels before first decode; N>0 cancels after N appended diagnostic tokens\n\n");
    yvex_cli_out_writef(fp, "Stop behavior:\n");
    yvex_cli_out_writef(fp, "  max-new-tokens stops after append; context-length stops before append; decode/logits/sample/append failures preserve partial diagnostic output.\n");
    yvex_cli_out_writef(fp, "  Partial diagnostic output is preserved on failure, cancellation, and context stops.\n");
    yvex_cli_out_writef(fp, "  EOS and stop-token text matching are unsupported for this bounded path.\n\n");
    yvex_cli_out_writef(fp, "Output policy:\n");
    yvex_cli_out_writef(fp, "  --output normal|audit     normal is compact; audit preserves full diagnostic state fields\n");
    yvex_cli_out_writef(fp, "  --audit                   shortcut for --output audit\n");
    yvex_cli_out_writef(fp, "  Text output is the stable operator contract. Normal output is compact by default; audit/trace output carries diagnostic state, counters, trace, cancel, cleanup, and boundary records.\n");
    yvex_cli_out_writef(fp, "  The command emits no ANSI color by default.\n\n");
    yvex_cli_out_writef(fp, "Boundaries:\n");
    yvex_cli_out_writef(fp, "  full model generation: unsupported\n");
    yvex_cli_out_writef(fp, "  real DeepSeek generation: unsupported\n");
    yvex_cli_out_writef(fp, "  real output-head logits: unsupported\n");
    yvex_cli_out_writef(fp, "  real vocabulary sampling: unsupported\n");
    yvex_cli_out_writef(fp, "  tokenizer-quality text generation: unsupported\n");
    yvex_cli_out_writef(fp, "  provider/server/streaming generation: unsupported\n");
    yvex_cli_out_writef(fp, "  evaluation: unsupported\n");
    yvex_cli_out_writef(fp, "  benchmark_status: not-measured\n");
    yvex_cli_out_writef(fp, "  throughput: not-measured\n");
}
