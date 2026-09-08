/* Typed execution and host-event projections shared by the CLI and foreground server. */
#define _POSIX_C_SOURCE 200809L
#include "src/cli/io/private.h"
#include <yvex/internal/core.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int server_event_watch_visible(const yvex_server_event *event)
{
    if (event && !strncmp(event->phase, "http:", 5u) &&
        (event->kind == YVEX_SERVER_EVENT_REQUEST_RECEIVED ||
         event->kind == YVEX_SERVER_EVENT_CLIENT_DISCONNECTED)) return 1;
    return event && event->kind != YVEX_SERVER_EVENT_CLIENT_DISCONNECTED &&
           event->kind != YVEX_SERVER_EVENT_REQUEST_RECEIVED &&
           !(event->kind == YVEX_SERVER_EVENT_REQUEST_QUEUED && event->value_a <= 1u) &&
           !(event->kind >= YVEX_SERVER_EVENT_DRAFT_STARTED &&
             event->kind <= YVEX_SERVER_EVENT_CANDIDATE_REJECTED) &&
           event->kind != YVEX_SERVER_EVENT_GENERATION_FRAGMENT &&
           event->kind != YVEX_SERVER_EVENT_GENERATION_PROFILE;
}

static const char *server_event_category(yvex_server_event_kind kind)
{
    if (kind == YVEX_SERVER_EVENT_RUNTIME_READY) return "READY";
    if (kind >= YVEX_SERVER_EVENT_ENGINE_LOAD_REQUESTED) return "ENGINE";
    if (kind <= YVEX_SERVER_EVENT_LISTENER_READY) return "STARTUP";
    if (kind <= YVEX_SERVER_EVENT_SESSION_CLOSED) return "SESSION";
    if (kind <= YVEX_SERVER_EVENT_REQUEST_STARTED) return "REQUEST";
    if (kind <= YVEX_SERVER_EVENT_PREFILL_COMPLETED) return "PREFILL";
    if (kind <= YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED) return "SPECULATION";
    if (kind <= YVEX_SERVER_EVENT_GENERATION_FAILED) return "GENERATE";
    if (kind == YVEX_SERVER_EVENT_TELEMETRY_DROPPED) return "WARNING";
    return "RUNTIME";
}

static const char *server_event_color(const yvex_server_event *event,
                                      const yvex_cli_terminal_style *style)
{
    if (event->severity >= YVEX_SERVER_SEVERITY_ERROR) return style->error;
    if (event->severity == YVEX_SERVER_SEVERITY_WARNING) return style->warning;
    switch (event->kind) {
    case YVEX_SERVER_EVENT_RUNTIME_READY:
    case YVEX_SERVER_EVENT_LISTENER_READY:
    case YVEX_SERVER_EVENT_PREFILL_COMPLETED:
    case YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED:
    case YVEX_SERVER_EVENT_GENERATION_COMPLETED:
    case YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_COMPLETE:
    case YVEX_SERVER_EVENT_ENGINE_READY:
    case YVEX_SERVER_EVENT_ENGINE_UNLOADED:
        return style->success;
    case YVEX_SERVER_EVENT_GENERATION_CANCELLED:
    case YVEX_SERVER_EVENT_TELEMETRY_DROPPED:
        return style->warning;
    case YVEX_SERVER_EVENT_ENGINE_LOAD_FAILED:
    case YVEX_SERVER_EVENT_ENGINE_UNLOAD_FAILED:
        return style->error;
    case YVEX_SERVER_EVENT_PREFILL_STARTED:
    case YVEX_SERVER_EVENT_PREFILL_PROGRESS:
    case YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN:
        return style->accent;
    default:
        return style->strong;
    }
}

const char *yvex_cli_out_stop_reason(unsigned long long reason)
{
    static const char *const names[] = {
        "none", "EOS", "tokenizer stop", "maximum tokens", "context capacity",
        "cancelled", "model failure", "tokenizer failure", "output failure"};
    return reason < sizeof(names) / sizeof(names[0]) ? names[reason] : "unknown";
}

static void execution_rates(FILE *output,
                            const yvex_execution_measurement *measurement,
                            double fallback_rate, int compact)
{
    int typed = measurement &&
                measurement->schema_version ==
                    YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1;
    const char *scope = typed &&
                                measurement->scope ==
                                    YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE
                            ? "subsequent decode cumulative"
                        : typed && measurement->scope == YVEX_EXECUTION_SCOPE_PREFILL
                            ? "prefill"
                        : typed &&
                                  measurement->scope ==
                                      YVEX_EXECUTION_SCOPE_TOTAL_OPERATION
                            ? "total cumulative"
                            : "cumulative";
    if (compact) {
        double cumulative = 0.0;
        if (typed && (measurement->available &
                      YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE))
            cumulative = measurement->cumulative_rate;
        else if (!typed && fallback_rate > 0.0)
            cumulative = fallback_rate;
        const char *label = !typed ? "rate" :
            measurement->scope == YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE
                ? "decode-avg" :
            measurement->scope == YVEX_EXECUTION_SCOPE_TOTAL_OPERATION
                ? "total-avg" : "avg";
        if (cumulative > 0.0)
            fprintf(output, " | %s=%.2f tok/s", label, cumulative);
        if (typed && (measurement->available &
                      YVEX_EXECUTION_MEASUREMENT_ROLLING_RATE_AVAILABLE)) {
            fprintf(output, "%s rolling[", cumulative > 0.0 ? "" : " |");
            if (measurement->rolling_units < measurement->rolling_window_units)
                fprintf(output, "%llu/", measurement->rolling_units);
            fprintf(output, "%llu]=%.2f tok/s", measurement->rolling_window_units,
                    measurement->rolling_rate);
        }
        return;
    }
    if (typed &&
        (measurement->available &
         YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE))
        fprintf(output, " · %s %.2f tok/s", scope,
                measurement->cumulative_rate);
    else if (fallback_rate > 0.0)
        fprintf(output, " · cumulative %.2f tok/s", fallback_rate);
    if (typed &&
        (measurement->available &
         YVEX_EXECUTION_MEASUREMENT_ROLLING_RATE_AVAILABLE))
        fprintf(output, " · rolling%llu %.2f tok/s",
                measurement->rolling_window_units,
                measurement->rolling_rate);
}

void yvex_cli_out_turn_metrics(FILE *output, const yvex_client_message *message,
                               unsigned long long context_capacity,
                               const yvex_cli_terminal_style *style)
{
    if (message->engine_kind == YVEX_SERVER_ENGINE_MEDIA) {
        fprintf(output, "%smedia%s · %.2f s", style->success, style->reset,
                message->decode_seconds);
        return;
    }
    fprintf(output,
            "\n%s  generation %llu tokens · decode wall %.2f s",
            style->dim, message->generated_tokens, message->decode_seconds);
    execution_rates(output, &message->measurement, message->decode_rate, 0);
    fprintf(output,
            " · TTFT %.2f s%s\n"
            "%s  prefill %llu new/%llu prompt/%llu reused · %.2f s · %.2f tok/s",
            message->first_token_seconds, style->reset, style->dim,
            message->prefill_tokens, message->prompt_tokens,
            message->reused_tokens, message->prefill_seconds,
            message->prefill_rate);
    if (message->execution_strategy == YVEX_SERVER_EXECUTION_SPECULATIVE)
        fprintf(output, " · speculative %llu proposed/%llu accepted/%llu rejected/%llu verified",
                message->proposed_tokens,
                message->accepted_draft_tokens, message->rejected_draft_tokens,
                message->target_verification_count);
    if (context_capacity)
        fprintf(output, " · context %llu->%llu/%llu", message->initial_position,
                message->context_used, context_capacity);
    else
        fprintf(output, " · context %llu", message->context_used);
    if (message->output_limit_explicit)
        fprintf(output, " · output explicit %llu · envelope %llu",
                message->requested_maximum_new_tokens,
                message->resolved_maximum_new_tokens);
    else
        fprintf(output, " · output adaptive · envelope %llu",
                message->resolved_maximum_new_tokens);
    fputs(style->reset, output);
}

void yvex_cli_out_turn_complete(FILE *output,
                                const yvex_client_message *message,
                                unsigned long long context_capacity,
                                const yvex_cli_terminal_style *style)
{
    if (message->media_result.available) {
        fprintf(output,
                "%smedia complete%s · %s\n"
                "%llux%llu · %llu frames · %.3f s · %llu/%llu fps · "
                "%llu audio samples · %llu bytes · seed %llu · %llu evals\n"
                "%spreset %s · trajectory %s · execution %s%s\n",
                style->success, style->reset, message->media_result.output_path,
                message->media_result.width, message->media_result.height,
                message->media_result.frames,
                (double)message->media_result.duration_milliseconds / 1000.0,
                message->media_result.fps_numerator,
                message->media_result.fps_denominator,
                message->media_result.audio_samples,
                message->media_result.file_bytes, message->media_result.seed,
                message->media_result.model_evaluations, style->dim,
                message->media_result.preset_identity,
                message->media_result.trajectory_identity,
                message->media_result.execution_identity, style->reset);
    } else {
        yvex_cli_out_turn_metrics(output, message, context_capacity, style);
        fprintf(output, "%s · stop %s · session %s%s\n", style->dim,
                yvex_cli_out_stop_reason(message->stop_reason),
                message->session_name, style->reset);
    }
    if (message->reasoning_tokens || message->first_reasoning_seconds > 0.0)
        fprintf(output,
                "%s  reasoning %llu tokens · %.2f s · %.2f tok/s · "
                "TTFR %.2f s · final %llu tokens · %.2f s · %.2f tok/s · "
                "TTFF %.2f s · total %.2f tok/s%s\n",
                style->dim, message->reasoning_tokens,
                message->reasoning_seconds, message->reasoning_rate,
                message->first_reasoning_seconds, message->final_tokens,
                message->final_seconds, message->final_rate,
                message->first_final_seconds,
                message->total_completion_rate, style->reset);
}

static const char *server_backend_name(unsigned long long backend)
{
    return backend == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU";
}

static void server_event_name(const yvex_server_event *event)
{
    const char *name = yvex_server_event_kind_name(event->kind);
    while (*name) {
        int byte = *name++;
        putchar(byte == '.' || byte == '_' ? ' ' : byte);
    }
}

static void server_event_bytes(const char *name, unsigned long long bytes, int compact)
{
    double scale = bytes >= 1073741824u ? 1073741824.0 : 1048576.0;
    const char *unit = bytes >= 1073741824u ? "GiB" : "MiB";
    if (compact)
        printf(" %s=%.1f%s", name, (double)bytes / scale, unit);
    else
        printf(" · %s %.2f %s", name, (double)bytes / scale, unit);
}

static const char *execution_work_name(yvex_execution_work_unit unit)
{
    switch (unit) {
    case YVEX_EXECUTION_WORK_TOKENS: return "tokens";
    case YVEX_EXECUTION_WORK_BYTES: return "bytes";
    case YVEX_EXECUTION_WORK_TENSORS: return "tensors";
    case YVEX_EXECUTION_WORK_PLANS: return "plans";
    case YVEX_EXECUTION_WORK_COMPONENTS: return "components";
    case YVEX_EXECUTION_WORK_EVALUATIONS: return "evaluations";
    case YVEX_EXECUTION_WORK_FRAMES: return "frames";
    case YVEX_EXECUTION_WORK_SAMPLES: return "samples";
    case YVEX_EXECUTION_WORK_OPERATIONS: return "operations";
    case YVEX_EXECUTION_WORK_NONE: break;
    }
    return "work";
}

static void server_profile_values(const yvex_server_event *event)
{
    if (!strcmp(event->phase, "movement"))
        printf(" · H2D %llu · D2H %llu · D2D %llu bytes", event->value_a,
               event->value_b, event->value_c);
    else if (!strcmp(event->phase, "transfers"))
        printf(" · uploads %llu · downloads %llu · expert subviews %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "launches"))
        printf(" · kernel launches %llu · stream syncs %llu · device syncs %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "target"))
        printf(" · target forwards %llu · rows %llu · replayed %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "speculation"))
        printf(" · draft forwards %llu · verified rows %llu · promoted rows %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "candidate"))
        printf(" · accepted %llu · discarded %llu · extensions %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "moe"))
        printf(" · row/expert pairs %llu · subviews %llu · bytes %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "output"))
        printf(" · rows %llu · D2H %llu bytes · committed %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "prefill"))
        printf(" · prompt %llu · reused %llu · new %llu", event->value_a,
               event->value_b, event->value_c);
    else if (!strcmp(event->phase, "decode"))
        printf(" · first decode %llu · later decode %llu · tokens %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batches"))
        printf(" · physical %llu · multi-source %llu · max width %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-rows"))
        printf(" · submitted %llu · executed %llu · admitted width %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-multi-source"))
        printf(" · physical %llu · rows %llu · max real width %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-sources"))
        printf(" · max sources %llu · active producers %llu",
               event->value_a, event->value_b);
    else if (!strcmp(event->phase, "execution-batch-experts"))
        printf(" · worklists %llu · pairs %llu · max population %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-expert-rows"))
        printf(" · TC eligible %llu · TC executed %llu · narrow %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "batch-expert-population-1-3"))
        printf(" · population 1 %llu · 2 %llu · 3 %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "batch-expert-population-4-6"))
        printf(" · population 4 %llu · 5 %llu · 6 %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-coalescing"))
        printf(" · waits %llu · timeouts %llu · producers %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-policy"))
        printf(" · coalescing limit %llu ns · width %llu · producers %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-step-rendezvous"))
        printf(" · submissions %llu · multi-source %llu · max width %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-step-policy"))
        printf(" · limit %llu ns · steps %llu · producers %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-mismatch"))
        printf(" · phase %llu · layer %llu · operation %llu",
               event->value_a, event->value_b, event->value_c);
    else if (!strcmp(event->phase, "execution-batch-mismatch-other"))
        printf(" · geometry %llu · profile %llu · identity %llu",
               event->value_a, event->value_b, event->value_c);
}

static void server_event_values(const yvex_server_event *event, int detailed)
{
    if (!strncmp(event->phase, "http:", 5u) &&
        (event->kind == YVEX_SERVER_EVENT_REQUEST_RECEIVED ||
         event->kind == YVEX_SERVER_EVENT_CLIENT_DISCONNECTED)) {
        printf(" · openai %s · peer 127.0.0.1:%llu", event->phase + 5u, event->value_a);
        if (event->kind == YVEX_SERVER_EVENT_CLIENT_DISCONNECTED) {
            if (event->value_b) printf(" · HTTP %llu", event->value_b);
            else fputs(" · HTTP status unavailable", stdout);
            if (event->value_c) printf(" · error -%llu", event->value_c);
        }
        return;
    }
    if (event->kind >= YVEX_SERVER_EVENT_DRAFT_STARTED &&
        event->kind <= YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED) {
        printf(" · cycle %llu", event->speculative_cycle);
        if (event->proposed_tokens)
            printf(" · accepted %llu/%llu", event->accepted_tokens,
                   event->proposed_tokens);
        if (event->selected_verification_tokens)
            printf(" · verified %llu", event->selected_verification_tokens);
        if (event->rejected_tokens) printf(" · rejected %llu", event->rejected_tokens);
        if (event->discarded_tokens) printf(" · discarded %llu", event->discarded_tokens);
        if (detailed && event->confidence_logit_count)
            printf(" · confidence %.4g..%.4g mean %.4g",
                   event->confidence_logit_minimum, event->confidence_logit_maximum,
                   event->confidence_logit_mean);
        return;
    }
    switch (event->kind) {
    case YVEX_SERVER_EVENT_MATERIALIZATION_COMPLETE:
        server_event_bytes("host", event->value_a, 0);
        server_event_bytes("device", event->value_b, 0);
        printf(" · %llu tensor binding%s", event->value_c,
               event->value_c == 1u ? "" : "s");
        break;
    case YVEX_SERVER_EVENT_RESIDENCY_READY:
        server_event_bytes("host", event->value_a, 0);
        server_event_bytes("device", event->value_b, 0);
        printf(" · %llu upload%s", event->value_c, event->value_c == 1u ? "" : "s");
        break;
    case YVEX_SERVER_EVENT_ARTIFACT_OPEN_COMPLETE:
        if (event->engine_kind == YVEX_SERVER_ENGINE_MEDIA &&
            strcmp(event->phase, "media-model")) {
            static const char *const modes[] = {
                "full-hash", "verified-reopen", "fallback-full-hash",
            };
            static const char *const states[] = {
                "disabled", "miss", "hit", "invalid",
            };
            unsigned long long mode = event->value_c & 0xffull;
            unsigned long long state = (event->value_c >> 8u) & 0xffull;
            server_event_bytes("hashed", event->value_a, 0);
            server_event_bytes("file", event->value_b, 0);
            printf(" · %s · lease %s",
                   mode < sizeof(modes) / sizeof(modes[0]) ? modes[mode] : "unknown",
                   state < sizeof(states) / sizeof(states[0]) ? states[state] : "unknown");
            if ((event->value_c >> 16u) & 1ull) printf(" · published");
            if ((event->value_c >> 17u) & 1ull) printf(" · repaired");
            if ((event->value_c >> 18u) & 1ull) printf(" · cache warning");
        } else if (event->engine_kind == YVEX_SERVER_ENGINE_MEDIA) {
            server_event_bytes("hashed", event->value_a, 0);
            server_event_bytes("files", event->value_b, 0);
            printf(" · %llu components", event->value_c);
        } else {
            server_event_bytes("hashed", event->value_a, 0);
            server_event_bytes("host", event->value_b, 0);
            server_event_bytes("device", event->value_c, 0);
        }
        break;
    case YVEX_SERVER_EVENT_LISTENER_READY:
        printf(" · socket %04llo · queue %llu · sessions %llu", event->value_a,
               event->value_b, event->value_c);
        break;
    case YVEX_SERVER_EVENT_RUNTIME_READY:
        printf(" · %s · context %llu", server_backend_name(event->value_c), event->value_b);
        break;
    case YVEX_SERVER_EVENT_SESSION_CREATED:
        printf(" · %llu active", event->value_c);
        break;
    case YVEX_SERVER_EVENT_SESSION_CLOSED:
        printf(" · %llu active", event->value_b);
        break;
    case YVEX_SERVER_EVENT_SESSION_ATTACHED:
    case YVEX_SERVER_EVENT_SESSION_DETACHED:
        printf(" · %llu client%s", event->value_a, event->value_a == 1u ? "" : "s");
        break;
    case YVEX_SERVER_EVENT_REQUEST_RECEIVED:
        if (detailed) printf(" · prompt %llu bytes", event->value_a);
        break;
    case YVEX_SERVER_EVENT_REQUEST_QUEUED:
        printf(" · depth %llu/%llu", event->value_a, event->value_b);
        break;
    case YVEX_SERVER_EVENT_REQUEST_STARTED:
        printf(" · input %llu · prefix %llu · limit %llu", event->value_a,
               event->value_b, event->value_c);
        break;
    case YVEX_SERVER_EVENT_TOKENIZER_COMPLETED:
        printf(" · prompt %llu · reused %llu", event->value_a, event->value_b);
        break;
    case YVEX_SERVER_EVENT_PREFILL_STARTED:
        printf(" · %llu new · chunk %llu", event->value_a, event->value_b);
        break;
    case YVEX_SERVER_EVENT_PREFILL_PROGRESS:
        printf(" · %llu/%llu tokens", event->value_a, event->value_b);
        break;
    case YVEX_SERVER_EVENT_PREFILL_COMPLETED:
        printf(" · %llu tokens · %llu chunk%s", event->value_a, event->value_b,
               event->value_b == 1u ? "" : "s");
        break;
    case YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN:
        if (detailed) printf(" · ordinal %llu · token %llu", event->value_a, event->value_b);
        break;
    case YVEX_SERVER_EVENT_GENERATION_COMPLETED:
    case YVEX_SERVER_EVENT_GENERATION_CANCELLED:
    case YVEX_SERVER_EVENT_GENERATION_FAILED:
        printf(" · %llu token%s · position %llu · stop %s", event->value_a,
               event->value_a == 1u ? "" : "s", event->value_b,
               yvex_cli_out_stop_reason(event->value_c));
        break;
    case YVEX_SERVER_EVENT_GENERATION_PROFILE:
        server_profile_values(event);
        break;
    case YVEX_SERVER_EVENT_ENGINE_LOAD_PROGRESS:
        if (event->measurement.available &
            YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE)
            printf(" · %llu/%llu %s", event->measurement.completed_units,
                   event->measurement.total_units,
                   execution_work_name(event->measurement.work_unit));
        else
            printf(" · %llu %s completed · total unavailable",
                   event->measurement.completed_units,
                   execution_work_name(event->measurement.work_unit));
        break;
    case YVEX_SERVER_EVENT_TELEMETRY_DROPPED:
        printf(" · %llu dropped · capacity %llu", event->value_a, event->value_b);
        break;
    default:
        break;
    }
}

int yvex_cli_out_server_event(const yvex_server_event *event, int detailed)
{
    static const char *const severity[] = {"debug", "info", "warning", "error", "fatal"};
    yvex_cli_terminal_style style;
    const char *color;
    unsigned int level;
    if (!event || (!detailed && !server_event_watch_visible(event))) return 0;
    yvex_cli_terminal_style_get(stdout, &style);
    color = server_event_color(event, &style);
    level = event->severity <= YVEX_SERVER_SEVERITY_FATAL
                ? (unsigned int)event->severity : YVEX_SERVER_SEVERITY_FATAL;
    if (detailed) {
        const char *severity_color = event->severity >= YVEX_SERVER_SEVERITY_ERROR
                                         ? style.error
                                         : event->severity == YVEX_SERVER_SEVERITY_WARNING
                                               ? style.warning
                                               : style.dim;
        printf("%s#%llu%s %s%-7s%s ", style.dim, event->sequence, style.reset,
               severity_color, severity[level], style.reset);
    } else {
        time_t seconds = (time_t)(event->wall_time_ns / 1000000000u);
        struct tm clock;
        char stamp[16] = "--:--:--";
        if (event->wall_time_ns && localtime_r(&seconds, &clock))
            (void)strftime(stamp, sizeof(stamp), "%H:%M:%S", &clock);
        printf("%s%s%s  %s%-8s%s ", style.dim, stamp, style.reset, color,
               server_event_category(event->kind), style.reset);
    }
    fputs(color, stdout);
    server_event_name(event);
    fputs(style.reset, stdout);
    if (!detailed && event->session_id[0] && event->request_id[0])
        printf(" · %s/%s", event->session_id, event->request_id);
    else {
        if (event->session_id[0]) printf(" · session %s", event->session_id);
        if (event->request_id[0]) printf(" · request %s", event->request_id);
    }
    if (detailed && event->turn_id[0]) printf(" · turn %s", event->turn_id);
    if (detailed && event->phase[0]) printf(" · phase %s", event->phase);
    server_event_values(event, detailed);
    if (event->seconds > 0.0) printf(" · %.3f s", event->seconds);
    if (event->kind == YVEX_SERVER_EVENT_GENERATION_PROGRESS ||
        (event->kind >= YVEX_SERVER_EVENT_GENERATION_COMPLETED &&
         event->kind <= YVEX_SERVER_EVENT_GENERATION_FAILED))
        execution_rates(stdout, &event->measurement, event->rate, 0);
    else if (event->rate > 0.0)
        printf(" · %.2f %s/s", event->rate,
               execution_work_name(event->measurement.work_unit));
    putchar('\n');
    fflush(stdout);
    return 1;
}

static void watch_stamp(const yvex_server_event *event, char stamp[16])
{
    time_t seconds = (time_t)(event->wall_time_ns / 1000000000u);
    struct tm clock;
    memcpy(stamp, "--:--:--", 9u);
    if (event->wall_time_ns && localtime_r(&seconds, &clock))
        (void)strftime(stamp, 16u, "%H:%M:%S", &clock);
}

static void watch_line_begin(const yvex_cli_watch_renderer *renderer,
                             const yvex_server_event *event,
                             const char *color, const char *label)
{
    char stamp[16];
    watch_stamp(event, stamp);
    printf("%s%s%s  %s%-9s%s ", renderer->style.dim, stamp,
           renderer->style.reset, color, label, renderer->style.reset);
}

static void watch_text(const char *value, size_t maximum)
{
    size_t length = strlen(value);
    if (length <= maximum) fputs(value, stdout);
    else printf("%.*s..%s", (int)(maximum - 6u), value, value + length - 4u);
}
static void watch_request_id(const yvex_server_event *event)
{
    if (event->session_id[0] && event->request_id[0]) {
        watch_text(event->session_id, 12u);
        printf("/%s", event->request_id);
    }
    else if (event->session_id[0])
        watch_text(event->session_id, 12u);
    else if (event->request_id[0])
        fputs(event->request_id, stdout);
    else
        fputs("runtime", stdout);
}

static void watch_request_begin(yvex_cli_watch_renderer *renderer,
                                const yvex_server_event *event)
{
    renderer->request_open = 1;
    watch_line_begin(renderer, event, renderer->style.accent, "REQUEST");
    watch_request_id(event);
    printf(" %s%s=%llu prefix_tokens=%llu max_tokens=%llu%s",
           renderer->style.dim,
           event->provider_request_identity[0] ? "messages" : "input_bytes",
           event->value_a, event->value_b,
           event->value_c, renderer->style.reset);
    putchar('\n');
}
static void watch_session(const yvex_cli_watch_renderer *renderer,
                          const yvex_server_event *event)
{
    const char *verb = event->kind == YVEX_SERVER_EVENT_SESSION_CREATED ? "created" :
        event->kind == YVEX_SERVER_EVENT_SESSION_ATTACHED ? "attached" :
        event->kind == YVEX_SERVER_EVENT_SESSION_DETACHED ? "detached" :
        event->kind == YVEX_SERVER_EVENT_SESSION_RESET ? "reset" : "closed";
    watch_line_begin(renderer, event, server_event_color(event, &renderer->style),
                     "SESSION");
    watch_text(event->session_id, 16u);
    printf(" %s", verb);
    if (event->kind == YVEX_SERVER_EVENT_SESSION_ATTACHED ||
        event->kind == YVEX_SERVER_EVENT_SESSION_DETACHED)
        printf(" clients=%llu", event->value_a);
    else if (event->kind == YVEX_SERVER_EVENT_SESSION_CREATED ||
             event->kind == YVEX_SERVER_EVENT_SESSION_CLOSED)
        printf(" active_sessions=%llu", event->kind == YVEX_SERVER_EVENT_SESSION_CREATED
                                            ? event->value_c : event->value_b);
    putchar('\n');
}
static void watch_cycle(yvex_cli_watch_renderer *renderer,
                        const yvex_server_event *event)
{
    if (!renderer->detailed) return;
    watch_line_begin(renderer, event, renderer->style.success, "SPEC");
    watch_request_id(event);
    printf(" cycle=%llu accepted=%llu/%llu", event->speculative_cycle,
           event->accepted_tokens, event->proposed_tokens);
    if (event->selected_verification_tokens)
        printf(" verified=%llu", event->selected_verification_tokens);
    if (event->rejected_tokens) printf(" rejected=%llu", event->rejected_tokens);
    if (event->discarded_tokens) printf(" discarded=%llu", event->discarded_tokens);
    if (event->seconds > 0.0) printf(" elapsed=%.3fs", event->seconds);
    putchar('\n');
}

static void watch_request_end(yvex_cli_watch_renderer *renderer,
                              const yvex_server_event *event)
{
    const char *label = event->kind == YVEX_SERVER_EVENT_GENERATION_COMPLETED
                            ? "DONE"
                        : event->kind == YVEX_SERVER_EVENT_GENERATION_CANCELLED
                            ? "CANCELLED"
                            : "FAIL";
    const char *color = server_event_color(event, &renderer->style);
    watch_line_begin(renderer, event, color, label);
    watch_request_id(event);
    printf(" generated=%llu position=%llu stop=\"%s\"", event->value_a,
           event->value_b, yvex_cli_out_stop_reason(event->value_c));
    if (event->seconds > 0.0) printf(" elapsed=%.1fs", event->seconds);
    execution_rates(stdout, &event->measurement, event->rate, 1);
    if (event->proposed_tokens)
        printf(" spec-accepted=%llu/%llu", event->accepted_tokens,
               event->proposed_tokens);
    putchar('\n');
    renderer->request_open = 0;
}

static int watch_progress_due(yvex_cli_watch_renderer *renderer,
                              const yvex_server_event *event)
{
    int reasoning = !strcmp(event->phase, "reasoning");
    int same = !strcmp(renderer->session_id, event->session_id) &&
               !strcmp(renderer->request_id, event->request_id);
    if (same && reasoning == renderer->progress_reasoning &&
        event->value_a < renderer->progress_tokens + 16ull &&
        event->seconds < renderer->progress_seconds + 1.0)
        return 0;
    yvex_core_text_copy(renderer->session_id, sizeof(renderer->session_id),
                        event->session_id);
    yvex_core_text_copy(renderer->request_id, sizeof(renderer->request_id),
                        event->request_id);
    renderer->progress_tokens = event->value_a;
    renderer->progress_seconds = event->seconds;
    renderer->progress_reasoning = reasoning;
    return 1;
}

static void watch_live_resources(yvex_cli_watch_renderer *renderer,
                                 const yvex_server_event *event,
                                 const yvex_server_summary *live, int force)
{
    const yvex_execution_resource_summary *resource;
    if (!live) return;
    if (!force && renderer->resource_stamp_ns &&
        event->wall_time_ns >= renderer->resource_stamp_ns &&
        event->wall_time_ns - renderer->resource_stamp_ns < 10000000000ull) return;
    renderer->resource_stamp_ns = event->wall_time_ns;
    resource = &live->metrics.resources;
    watch_line_begin(renderer, event, renderer->style.dim, "RESOURCES");
    fputs("host snapshot", stdout);
    if (resource->available & YVEX_EXECUTION_RESOURCE_PROCESS_AVAILABLE)
        server_event_bytes("process-rss", resource->process_rss_current_bytes, 1);
    if (resource->model_explicit_device_bytes)
        server_event_bytes("model-device-alloc", resource->model_explicit_device_bytes, 1);
    if (resource->available & YVEX_EXECUTION_RESOURCE_WORKSPACE_AVAILABLE)
        server_event_bytes("workspace", resource->workspace_current_bytes, 1);
    if (resource->available & YVEX_EXECUTION_RESOURCE_SESSION_AVAILABLE)
        server_event_bytes("session-state", resource->session_physical_state_bytes, 1);
    printf(" active_requests=%llu queued=%llu\n", live->metrics.active_requests,
           live->metrics.queue_depth);
}

static int watch_generation_progress(yvex_cli_watch_renderer *renderer,
                                     const yvex_server_event *event,
                                     const yvex_server_summary *live)
{
    if (!watch_progress_due(renderer, event)) return 0;
    watch_line_begin(renderer, event, renderer->style.accent, "DECODE");
    watch_request_id(event);
    printf(" generated=%llu position=%llu phase=%s", event->value_a, event->value_b,
           event->phase[0] ? event->phase : "decode");
    if (event->value_c && strcmp(event->phase, "reasoning"))
        printf(" reasoning=%llu", event->value_c);
    if (event->seconds > 0.0) printf(" elapsed=%.1fs", event->seconds);
    execution_rates(stdout, &event->measurement, event->rate, 1);
    if (event->proposed_tokens)
        printf(" spec-accepted=%llu/%llu", event->accepted_tokens,
               event->proposed_tokens);
    putchar('\n');
    watch_live_resources(renderer, event, live, 0);
    return 1;
}

void yvex_cli_watch_renderer_open(yvex_cli_watch_renderer *renderer, int detailed)
{
    if (!renderer) return;
    memset(renderer, 0, sizeof(*renderer));
    renderer->detailed = detailed != 0;
    yvex_cli_terminal_style_get(stdout, &renderer->style);
}

static int watch_prefill_due(yvex_cli_watch_renderer *renderer,
                             const yvex_server_event *event)
{
    const yvex_execution_measurement *measurement = &event->measurement;
    double seconds = (double)measurement->duration_ns / 1e9;
    int same = renderer->progress_reasoning == -1 &&
               !strcmp(renderer->session_id, event->session_id) &&
               !strcmp(renderer->request_id, event->request_id);
    int terminal = (measurement->available & YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE) &&
                   measurement->total_units && measurement->completed_units >= measurement->total_units;
    if (renderer->detailed ||
        !(measurement->available & YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE)) return 1;
    if (same && !terminal && event->kind != YVEX_SERVER_EVENT_PREFILL_STARTED &&
        seconds >= renderer->progress_seconds &&
        seconds - renderer->progress_seconds < 1.0) return 0;
    yvex_core_text_copy(renderer->session_id, sizeof(renderer->session_id),
                        event->session_id);
    yvex_core_text_copy(renderer->request_id, sizeof(renderer->request_id),
                        event->request_id);
    renderer->progress_seconds = seconds;
    renderer->progress_reasoning = -1; /* Prefill and decode share only presentation cadence. */
    return 1;
}

static int watch_operation_progress(yvex_cli_watch_renderer *renderer,
                                     const yvex_server_event *event, const char *label)
{
    const yvex_execution_measurement *measurement = &event->measurement;
    const char *phase = event->phase[0] ? event->phase : "work";
    const char *unit = execution_work_name(measurement->work_unit);
    if ((event->kind == YVEX_SERVER_EVENT_PREFILL_STARTED ||
         event->kind == YVEX_SERVER_EVENT_PREFILL_PROGRESS) &&
        !watch_prefill_due(renderer, event)) return 0;
    watch_line_begin(renderer, event, renderer->style.accent, label);
    if (event->request_id[0]) {
        watch_request_id(event);
        putchar(' ');
    }
    printf("phase=%s", phase);
    if ((measurement->available & YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE) &&
        measurement->total_units) {
        if (measurement->work_unit == YVEX_EXECUTION_WORK_BYTES) {
            double scale = measurement->total_units >= 1073741824ull
                               ? 1073741824.0
                           : measurement->total_units >= 1048576ull
                               ? 1048576.0
                           : measurement->total_units >= 1024ull ? 1024.0 : 1.0;
            const char *suffix = scale == 1073741824.0 ? "GiB" :
                                 scale == 1048576.0 ? "MiB" :
                                 scale == 1024.0 ? "KiB" : "B";
            printf(" completed=%.1f/%.1f%s", (double)measurement->completed_units / scale,
                   (double)measurement->total_units / scale, suffix);
        } else {
            printf(" completed=%llu/%llu %s", measurement->completed_units,
                   measurement->total_units, unit);
        }
    } else {
        printf(" completed=%llu %s total=unknown", measurement->completed_units, unit);
    }
    if (measurement->available & YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE)
        printf(" elapsed=%.2fs", (double)measurement->duration_ns / 1000000000.0);
    if (measurement->schema_version == YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1 &&
        (measurement->available & YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE)) {
        if (measurement->work_unit == YVEX_EXECUTION_WORK_TOKENS) {
            execution_rates(stdout, measurement, 0.0, 1);
        } else if (measurement->work_unit == YVEX_EXECUTION_WORK_BYTES) {
            double rate = measurement->cumulative_rate;
            double scale = rate >= 1073741824.0 ? 1073741824.0 :
                           rate >= 1048576.0 ? 1048576.0 :
                           rate >= 1024.0 ? 1024.0 : 1.0;
            const char *suffix = scale == 1073741824.0 ? "GiB/s" :
                                 scale == 1048576.0 ? "MiB/s" :
                                 scale == 1024.0 ? "KiB/s" : "B/s";
            printf(" | avg=%.2f %s", rate / scale, suffix);
        } else if (measurement->work_unit == YVEX_EXECUTION_WORK_TENSORS ||
                   measurement->work_unit == YVEX_EXECUTION_WORK_OPERATIONS) {
            printf(" | avg=%.2f %s/s", measurement->cumulative_rate, unit);
        }
    }
    putchar('\n');
    return 1;
}

static int watch_http_access(const yvex_cli_watch_renderer *renderer,
                             const yvex_server_event *event)
{
    int closed = event->kind == YVEX_SERVER_EVENT_CLIENT_DISCONNECTED;
    watch_line_begin(renderer, event, server_event_color(event, &renderer->style), "HTTP");
    fputs(event->request_id, stdout);
    printf(" openai %s peer=127.0.0.1:%llu %s", event->phase + 5u,
           event->value_a, closed ? "closed" : "received");
    if (closed) {
        const char *outcome = event->value_c == (unsigned long long)-YVEX_ERR_CANCELLED
                                ? "cancelled"
                            : !event->value_b ? "incomplete"
                            : event->value_b >= 500 ? "failed"
                            : event->value_b >= 400 ? "rejected"
                            : event->value_c ? "failed" : "complete";
        if (event->value_b) printf(" status=%llu", event->value_b);
        else fputs(" status=unavailable", stdout);
        printf(" outcome=%s", outcome);
        if (event->value_c)
            printf(" error=%s", event->value_c <= (unsigned long long)-YVEX_ERR_TIMEOUT
                                 ? yvex_status_name(-(yvex_status)event->value_c) : "unknown");
        if (event->seconds > 0.0) printf(" elapsed=%.3fs", event->seconds);
        if (event->session_id[0]) printf(" session=%s", event->session_id);
    }
    putchar('\n');
    fflush(stdout);
    return 1;
}

int yvex_cli_watch_renderer_event(yvex_cli_watch_renderer *renderer,
                                  const yvex_server_event *event,
                                  const yvex_server_summary *live)
{
    if (!renderer || !event) return 0;
    if (!strncmp(event->phase, "http:", 5u) &&
        (event->kind == YVEX_SERVER_EVENT_REQUEST_RECEIVED ||
         event->kind == YVEX_SERVER_EVENT_CLIENT_DISCONNECTED))
        return watch_http_access(renderer, event);
    if (event->engine_kind == YVEX_SERVER_ENGINE_MEDIA &&
        event->kind == YVEX_SERVER_EVENT_ARTIFACT_OPEN_COMPLETE &&
        strcmp(event->phase, "media-model")) {
        static const char *const modes[] = {"hash", "reopen", "rehash"};
        unsigned long long mode = event->value_c & 0xffull;
        watch_line_begin(renderer, event, renderer->style.accent, "COMPONENT");
        printf("%-12s %s", event->phase,
               mode < sizeof(modes) / sizeof(modes[0]) ? modes[mode] : "unknown");
        server_event_bytes("hashed", event->value_a, 1);
        server_event_bytes("file", event->value_b, 1);
        if (event->seconds > 0.0) printf(" elapsed=%.2fs", event->seconds);
        putchar('\n');
        return 1;
    }
    if (event->engine_kind == YVEX_SERVER_ENGINE_MEDIA &&
        event->kind == YVEX_SERVER_EVENT_REQUEST_STARTED) {
        watch_line_begin(renderer, event, renderer->style.accent, "MEDIA");
        watch_request_id(event);
        puts(" started");
        renderer->request_open = 1;
        return 1;
    }
    if (event->engine_kind == YVEX_SERVER_ENGINE_MEDIA &&
        (event->kind == YVEX_SERVER_EVENT_PREFILL_STARTED ||
         event->kind == YVEX_SERVER_EVENT_PREFILL_COMPLETED ||
         event->kind == YVEX_SERVER_EVENT_GENERATION_PROGRESS ||
         event->kind == YVEX_SERVER_EVENT_GENERATION_PROFILE)) {
        watch_line_begin(renderer, event, renderer->style.accent, "MEDIA");
        watch_request_id(event);
        printf(" phase=%s", event->phase[0] ? event->phase : "executing");
        if (event->value_b)
            printf(" completed=%llu/%llu %s", event->value_a, event->value_b,
                   execution_work_name(event->measurement.work_unit));
        if (event->value_c) printf(" value=%llu", event->value_c);
        putchar('\n');
        watch_live_resources(renderer, event, live, 0);
        return 1;
    }
    if (event->engine_kind == YVEX_SERVER_ENGINE_MEDIA &&
        event->kind >= YVEX_SERVER_EVENT_GENERATION_COMPLETED &&
        event->kind <= YVEX_SERVER_EVENT_GENERATION_FAILED) {
        const char *label = event->kind == YVEX_SERVER_EVENT_GENERATION_COMPLETED
                                ? "COMPLETE"
                            : event->kind == YVEX_SERVER_EVENT_GENERATION_CANCELLED
                                ? "CANCELLED" : "FAILED";
        watch_line_begin(renderer, event, server_event_color(event, &renderer->style),
                         !strcmp(label, "COMPLETE") ? "DONE" :
                         !strcmp(label, "CANCELLED") ? "CANCELLED" : "FAIL");
        watch_request_id(event);
        if (event->kind == YVEX_SERVER_EVENT_GENERATION_COMPLETED)
            printf(" frames=%llu bytes=%llu audio_samples=%llu",
                   event->value_a, event->value_b, event->value_c);
        else
            printf(" media phase=%s", event->phase[0] ? event->phase : "end");
        if (event->seconds > 0.0) printf(" elapsed=%.2fs", event->seconds);
        putchar('\n');
        watch_live_resources(renderer, event, live, 1);
        renderer->request_open = 0;
        return 1;
    }
    if (event->kind == YVEX_SERVER_EVENT_PROCESS_START ||
        event->kind == YVEX_SERVER_EVENT_TELEMETRY_READY ||
        event->kind == YVEX_SERVER_EVENT_LISTENER_READY ||
        (event->kind == YVEX_SERVER_EVENT_RUNTIME_READY &&
         event->engine_kind == YVEX_SERVER_ENGINE_NONE) ||
        event->kind == YVEX_SERVER_EVENT_CLIENT_DISCONNECTED ||
        event->kind == YVEX_SERVER_EVENT_REQUEST_RECEIVED ||
        event->kind == YVEX_SERVER_EVENT_GENERATION_FRAGMENT ||
        event->kind == YVEX_SERVER_EVENT_GENERATION_PROFILE ||
        (event->kind >= YVEX_SERVER_EVENT_DRAFT_STARTED &&
         event->kind <= YVEX_SERVER_EVENT_CANDIDATE_REJECTED))
        return 0;
    if (event->kind == YVEX_SERVER_EVENT_ENGINE_LOAD_PROGRESS)
        return watch_operation_progress(renderer, event, "LOAD");
    if (event->kind == YVEX_SERVER_EVENT_PREFILL_STARTED ||
        event->kind == YVEX_SERVER_EVENT_PREFILL_PROGRESS)
        return watch_operation_progress(renderer, event, "PREFILL");
    if (event->kind == YVEX_SERVER_EVENT_RUNTIME_READY ||
        event->kind >= YVEX_SERVER_EVENT_ENGINE_LOAD_REQUESTED) {
        const char *label = event->kind == YVEX_SERVER_EVENT_RUNTIME_READY ||
                            event->kind == YVEX_SERVER_EVENT_ENGINE_READY
                                ? "MODEL"
                            : event->kind == YVEX_SERVER_EVENT_ENGINE_LOAD_REQUESTED
                                ? "LOAD"
                            : event->kind == YVEX_SERVER_EVENT_ENGINE_LOAD_FAILED
                                ? "FAIL"
                            : event->kind == YVEX_SERVER_EVENT_ENGINE_UNLOAD_STARTED
                                ? "UNLOAD"
                            : event->kind == YVEX_SERVER_EVENT_ENGINE_UNLOADED
                                ? "UNLOADED" : "FAIL";
        watch_line_begin(renderer, event,
                         server_event_color(event, &renderer->style), label);
        watch_text(event->phase, sizeof(event->phase));
        printf(" generation=%llu backend=%s", event->value_a, server_backend_name(event->value_c));
        if (event->execution_strategy == YVEX_SERVER_EXECUTION_SPECULATIVE)
            printf(" strategy=speculative");
        else if (event->execution_strategy == YVEX_SERVER_EXECUTION_TARGET_ONLY)
            printf(" strategy=target-only");
        putchar('\n');
        return 1;
    }
    if (event->kind >= YVEX_SERVER_EVENT_ARTIFACT_OPEN_START &&
        event->kind <= YVEX_SERVER_EVENT_RESIDENCY_READY) return 0;
    if (event->kind >= YVEX_SERVER_EVENT_SESSION_CREATED &&
        event->kind <= YVEX_SERVER_EVENT_SESSION_CLOSED) {
        watch_session(renderer, event);
        return 1;
    }
    if (event->kind == YVEX_SERVER_EVENT_REQUEST_QUEUED) {
        if (event->value_a <= 1ull) return 0;
        watch_line_begin(renderer, event, renderer->style.warning, "QUEUE");
        watch_request_id(event);
        printf(" depth=%llu/%llu\n", event->value_a, event->value_b);
        return 1;
    }
    if (event->kind == YVEX_SERVER_EVENT_REQUEST_STARTED) {
        watch_request_begin(renderer, event);
        return 1;
    }
    if (event->kind == YVEX_SERVER_EVENT_TOKENIZER_COMPLETED) {
        watch_line_begin(renderer, event, renderer->style.strong, "PROMPT");
        watch_request_id(event);
        printf(" tokens=%llu reused=%llu\n", event->value_a, event->value_b);
    } else if (event->kind == YVEX_SERVER_EVENT_PREFILL_COMPLETED) {
        watch_line_begin(renderer, event, renderer->style.success, "PREFILL");
        watch_request_id(event);
        printf(" tokens=%llu chunks=%llu", event->value_a, event->value_b);
        if (event->seconds > 0.0) printf(" elapsed=%.2fs", event->seconds);
        execution_rates(stdout, &event->measurement, event->rate, 1);
        putchar('\n');
    } else if (event->kind == YVEX_SERVER_EVENT_GENERATION_FIRST_TOKEN) {
        watch_line_begin(renderer, event, renderer->style.accent, "FIRST");
        watch_request_id(event);
        if (event->seconds > 0.0) printf(" first-token=%.3fs", event->seconds);
        putchar('\n');
    } else if (event->kind == YVEX_SERVER_EVENT_GENERATION_PROGRESS) {
        if (!watch_generation_progress(renderer, event, live)) return 0;
    } else if (event->kind == YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED) {
        watch_cycle(renderer, event);
    } else if (event->kind >= YVEX_SERVER_EVENT_GENERATION_COMPLETED &&
               event->kind <= YVEX_SERVER_EVENT_GENERATION_FAILED) {
        watch_request_end(renderer, event);
        watch_live_resources(renderer, event, live, 1);
    } else if (event->kind == YVEX_SERVER_EVENT_TELEMETRY_DROPPED) {
        watch_line_begin(renderer, event, renderer->style.warning, "WARN");
        printf("telemetry coalesced=%llu dropped=%llu capacity=%llu\n",
               event->value_c, event->value_a, event->value_b);
    } else if (event->kind == YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_START ||
               event->kind == YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_COMPLETE) {
        watch_line_begin(renderer, event, server_event_color(event, &renderer->style),
                         "HOST");
        fputs(event->kind == YVEX_SERVER_EVENT_RUNTIME_SHUTDOWN_START
                  ? "stopping" : "stopped", stdout);
        putchar('\n');
    } else {
        return 0;
    }
    fflush(stdout);
    return 1;
}

void yvex_cli_watch_renderer_finish(yvex_cli_watch_renderer *renderer)
{
    if (renderer) renderer->request_open = 0;
}
