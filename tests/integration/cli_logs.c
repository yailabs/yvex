/* Standalone CLI integration fixture: exercise the real renderer without a model. */
#include "src/cli/io/private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char output[16384];
static int failures;

static void expect(int condition, const char *reason)
{
    if (condition) return;
    fprintf(stderr, "log renderer: %s\n%s\n", reason, output);
    failures++;
}

static int capture(yvex_cli_watch_renderer *renderer, yvex_server_event *event,
                   const yvex_server_summary *live)
{
    yvex_server_event before = *event;
    FILE *file = tmpfile();
    int saved, shown;
    size_t size;
    if (!file || (saved = dup(STDOUT_FILENO)) < 0) exit(2);
    fflush(stdout);
    if (dup2(fileno(file), STDOUT_FILENO) < 0) exit(2);
    shown = yvex_cli_watch_renderer_event(renderer, event, live);
    fflush(stdout);
    if (dup2(saved, STDOUT_FILENO) < 0) exit(2);
    close(saved);
    rewind(file);
    size = fread(output, 1, sizeof(output) - 1, file);
    output[size] = '\0';
    expect(feof(file), "bounded output fits the capture");
    fclose(file);
    expect(!memcmp(&before, event, sizeof(before)), "typed event is unchanged");
    return shown;
}

static yvex_server_event decode_event(void)
{
    yvex_server_event event = {0};
    event.kind = YVEX_SERVER_EVENT_GENERATION_PROGRESS;
    event.engine_kind = YVEX_SERVER_ENGINE_TEXT;
    event.wall_time_ns = 1788708011000000000ull;
    strcpy(event.session_id, "main");
    strcpy(event.request_id, "r5");
    strcpy(event.phase, "answer");
    event.value_a = 280;
    event.value_b = 307;
    event.value_c = 97;
    event.seconds = 40;
    event.rate = 999; /* The typed measurement, not this legacy fallback, wins. */
    event.measurement.schema_version = YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1;
    event.measurement.scope = YVEX_EXECUTION_SCOPE_SUBSEQUENT_DECODE;
    event.measurement.work_unit = YVEX_EXECUTION_WORK_TOKENS;
    event.measurement.available = YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE |
                                  YVEX_EXECUTION_MEASUREMENT_ROLLING_RATE_AVAILABLE;
    event.measurement.cumulative_rate = 6.98;
    event.measurement.rolling_rate = 5.38;
    event.measurement.rolling_units = 32;
    event.measurement.rolling_window_units = 32;
    return event;
}

static void test_decode(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = decode_event();
    yvex_cli_watch_renderer_open(&renderer, 0);
    expect(capture(&renderer, &event, NULL), "first progress is visible");
    expect(strstr(output, "generated=280 position=307 phase=answer reasoning=97") != NULL,
           "counts and phase are named");
    expect(strstr(output, "decode-avg=6.98 tok/s rolling[32]=5.38 tok/s") != NULL,
           "slow tail and cumulative scope remain distinct");
    expect(!strstr(output, "999") && !strstr(output, "RESOURCES"),
           "no fallback rate or fabricated resources");
    expect(!capture(&renderer, &event, NULL), "duplicate progress remains coalesced");
    strcpy(event.phase, "reasoning");
    expect(capture(&renderer, &event, NULL), "phase transition is never hidden");
    expect(!strstr(output, "reasoning=97"), "reasoning count does not duplicate phase");
    strcpy(event.session_id, "auxiliary");
    event.measurement.rolling_units = 10;
    expect(capture(&renderer, &event, NULL), "another session's progress survives");
    expect(strstr(output, "rolling[10/32]=5.38 tok/s") != NULL,
           "warming rolling window exposes actual and maximum token counts");
    event.seconds++;
    event.measurement.available = 0;
    capture(&renderer, &event, NULL);
    expect(!strstr(output, "tok/s"), "typed unavailable rate rejects the legacy fallback");
    event.seconds++;
    event.rate = 0;
    capture(&renderer, &event, NULL);
    expect(!strstr(output, "tok/s"), "unavailable rate is not a false zero");
    event.seconds++;
    event.measurement.schema_version = 0;
    event.rate = 4;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "rate=4.00 tok/s") != NULL && !strstr(output, "decode-avg"),
           "legacy rate does not borrow a typed scope");
    event.kind = YVEX_SERVER_EVENT_GENERATION_CANCELLED;
    event.value_c = 5;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "CANCELLED") && strstr(output, "stop=\"cancelled\""),
           "cancellation is explicit even after coalesced progress");
    event.kind = YVEX_SERVER_EVENT_GENERATION_FAILED;
    event.value_c = 6;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "FAIL") && strstr(output, "stop=\"model failure\""),
           "model failure is not a cryptic stop code");
    event.kind = YVEX_SERVER_EVENT_GENERATION_COMPLETED;
    event.measurement.schema_version = YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1;
    event.measurement.scope = YVEX_EXECUTION_SCOPE_TOTAL_OPERATION;
    event.measurement.available = YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE;
    event.proposed_tokens = 100;
    event.accepted_tokens = 39;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "total-avg=6.98 tok/s") && strstr(output, "spec-accepted=39/100") &&
           !strstr(output, "decode-avg") && !strchr(output, '%'),
           "total wall and exact speculative counts keep their scopes");
    event.kind = YVEX_SERVER_EVENT_SPECULATIVE_CYCLE_COMMITTED;
    event.speculative_cycle = 8;
    event.proposed_tokens = 10;
    event.accepted_tokens = 4;
    event.rejected_tokens = 6;
    event.discarded_tokens = 2;
    yvex_cli_watch_renderer_open(&renderer, 1);
    capture(&renderer, &event, NULL);
    expect(strstr(output, "auxiliary/r5 cycle=8 accepted=4/10 rejected=6 discarded=2") != NULL,
           "detailed speculation retains request identity and exact counts");
}

static void test_lifecycle(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = decode_event();
    yvex_cli_watch_renderer_open(&renderer, 0);
    event.kind = YVEX_SERVER_EVENT_REQUEST_STARTED;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "input_bytes=280 prefix_tokens=307 max_tokens=97") != NULL,
           "native request input is bytes, not tokens");
    strcpy(event.provider_request_identity, "provider-request");
    capture(&renderer, &event, NULL);
    expect(strstr(output, "messages=280") && !strstr(output, "input_bytes"),
           "provider request input is messages, not bytes");
    event.kind = YVEX_SERVER_EVENT_TOKENIZER_COMPLETED;
    event.value_a = 126;
    event.value_b = 100;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "PROMPT") && strstr(output, "tokens=126 reused=100"),
           "prompt reuse is visible without detailed logging");
    event.kind = YVEX_SERVER_EVENT_SESSION_RESET;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "main reset") && !strstr(output, "closed"),
           "reset is not falsely rendered as session close");
    event.kind = YVEX_SERVER_EVENT_ENGINE_LOAD_PROGRESS;
    strcpy(event.phase, "artifact-verification");
    event.measurement.available = YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE;
    event.measurement.work_unit = YVEX_EXECUTION_WORK_BYTES;
    event.measurement.completed_units = 1024;
    event.measurement.total_units = 2048;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "phase=artifact-verification completed=1.0/2.0KiB") &&
           !strchr(output, '%'), "load has exact progress and binary units, not a percentage");
    event.measurement.available = 0;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "completed=1024 bytes total=unknown") && !strchr(output, '%'),
           "unknown denominator stays unknown");
    event.measurement.available = YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE;
    event.measurement.total_units = 0;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "total=unknown") != NULL, "zero denominator remains unavailable");
    event.kind = YVEX_SERVER_EVENT_PREFILL_PROGRESS;
    strcpy(event.phase, "prefill");
    event.measurement.work_unit = YVEX_EXECUTION_WORK_TOKENS;
    event.measurement.total_units = 2048;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "PREFILL") && strstr(output, "main/r5 phase=prefill") &&
           strstr(output, "completed=1024/2048 tokens") && !strchr(output, '%'),
           "long prefill progress keeps request identity and a real denominator");
    event.kind = YVEX_SERVER_EVENT_TELEMETRY_DROPPED;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "telemetry coalesced=97 dropped=126 capacity=100") != NULL,
           "telemetry pressure remains visible");
}

static void test_progress_rates(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = decode_event();
    yvex_execution_measurement *measurement = &event.measurement;
    yvex_cli_watch_renderer_open(&renderer, 0);
    event.kind = YVEX_SERVER_EVENT_PREFILL_PROGRESS;
    strcpy(event.phase, "prefill");
    measurement->scope = YVEX_EXECUTION_SCOPE_PREFILL;
    measurement->completed_units = 330;
    measurement->total_units = 330;
    measurement->duration_ns = 28070000000ull;
    measurement->cumulative_rate = 11.76;
    measurement->available = YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE |
                             YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE |
                             YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "completed=330/330 tokens elapsed=28.07s | avg=11.76 tok/s") &&
           !strchr(output, '%') && !strstr(output, "999"),
           "prefill exposes measured token throughput, not percentage or fallback");
    measurement->available &= ~YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE;
    capture(&renderer, &event, NULL);
    expect(!strstr(output, "tok/s"), "missing typed rate is not invented from counters");
    event.kind = YVEX_SERVER_EVENT_PREFILL_STARTED;
    measurement->completed_units = 0;
    measurement->duration_ns = 0;
    capture(&renderer, &event, NULL);
    expect(!strstr(output, "tok/s"), "prefill start has no fabricated zero rate");
    event.kind = YVEX_SERVER_EVENT_ENGINE_LOAD_PROGRESS;
    strcpy(event.phase, "artifact-verification");
    measurement->scope = YVEX_EXECUTION_SCOPE_MODEL_LIFECYCLE;
    measurement->work_unit = YVEX_EXECUTION_WORK_BYTES;
    measurement->completed_units = 1073741824ull;
    measurement->total_units = 2147483648ull;
    measurement->duration_ns = 500000000ull;
    measurement->cumulative_rate = 2147483648.0;
    measurement->available |= YVEX_EXECUTION_MEASUREMENT_CUMULATIVE_RATE_AVAILABLE;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "completed=1.0/2.0GiB elapsed=0.50s | avg=2.00 GiB/s") &&
           !strstr(output, "tok/s") && !strchr(output, '%'),
           "byte throughput has binary transfer units, never token units");
    measurement->schema_version = 0;
    capture(&renderer, &event, NULL);
    expect(!strstr(output, "GiB/s"), "unrecognized measurement cannot supply a typed rate");
    measurement->schema_version = YVEX_EXECUTION_MEASUREMENT_SCHEMA_V1;
    measurement->work_unit = YVEX_EXECUTION_WORK_TENSORS;
    measurement->completed_units = 1409;
    measurement->total_units = 1409;
    measurement->duration_ns = 36750000000ull;
    measurement->cumulative_rate = 38.34;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "avg=38.34 tensors/s") && !strstr(output, "tok/s"),
           "tensor progress cannot masquerade as generation speed");
}

static void test_http_access(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = {0};
    yvex_cli_watch_renderer_open(&renderer, 0);
    event.kind = YVEX_SERVER_EVENT_REQUEST_RECEIVED;
    strcpy(event.phase, "http:GET /v1/models");
    strcpy(event.request_id, "http-7");
    event.value_a = 44150;
    expect(capture(&renderer, &event, NULL), "external discovery is visible without inference");
    expect(strstr(output, "HTTP") && strstr(output, "http-7 openai GET /v1/models") &&
           strstr(output, "peer=127.0.0.1:44150 received") && !strstr(output, "status="),
           "received request carries actual peer and route but no invented result");
    event.kind = YVEX_SERVER_EVENT_CLIENT_DISCONNECTED;
    event.value_b = 200;
    event.seconds = 0.125;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "closed status=200 outcome=complete elapsed=0.125s") != NULL,
           "HTTP closure has observed status and duration");
    event.value_c = (unsigned long long)-YVEX_ERR_CANCELLED;
    strcpy(event.session_id, "oa-000000000007");
    capture(&renderer, &event, NULL);
    expect(strstr(output, "status=200 outcome=cancelled") && strstr(output, "session=oa-000000000007"),
           "SSE cancellation preserves HTTP 200 and model-session correlation");
    event.value_b = 404;
    event.value_c = 0;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "outcome=rejected") != NULL, "written rejection is not success");
    event.value_b = 504;
    event.value_c = (unsigned long long)-YVEX_ERR_TIMEOUT;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "status=504 outcome=failed error=YVEX_ERR_TIMEOUT") != NULL,
           "server failure is distinct from invalid client input and has a named error");
    event.value_b = 0;
    event.value_c = (unsigned long long)-YVEX_ERR_IO;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "status=unavailable outcome=incomplete") && !strstr(output, "YAI"),
           "failed header write has no HTTP status or fabricated client identity");
    strcpy(event.phase, "request");
    expect(!capture(&renderer, &event, NULL), "native internal connection churn remains suppressed");
    event.kind = YVEX_SERVER_EVENT_SESSION_CREATED;
    event.value_b = 0;
    event.value_c = 1;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "created active_sessions=1") != NULL,
           "created session count is the lifecycle owner's count, not its zero argument");
    event.kind = YVEX_SERVER_EVENT_SESSION_CLOSED;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "closed active_sessions=0") != NULL, "closure uses its own count field");
}

static void test_prefill_cadence(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = decode_event();
    yvex_execution_measurement *measurement = &event.measurement;
    int shown = 0;
    yvex_cli_watch_renderer_open(&renderer, 0);
    event.kind = YVEX_SERVER_EVENT_PREFILL_STARTED;
    strcpy(event.phase, "prefill");
    measurement->available = YVEX_EXECUTION_MEASUREMENT_DURATION_AVAILABLE |
                             YVEX_EXECUTION_MEASUREMENT_DENOMINATOR_AVAILABLE;
    measurement->total_units = 100;
    capture(&renderer, &event, NULL);
    event.kind = YVEX_SERVER_EVENT_PREFILL_PROGRESS;
    for (unsigned int index = 1; index <= 100; index++) {
        measurement->completed_units = index;
        measurement->duration_ns = (unsigned long long)index * 100000000ull;
        shown += capture(&renderer, &event, NULL);
    }
    expect(shown == 10, "100 prefill chunk events become 10 timed progress rows, including final");
    yvex_cli_watch_renderer_open(&renderer, 1);
    expect(capture(&renderer, &event, NULL) && capture(&renderer, &event, NULL),
           "verbose prefill retains all supplied events");
}

static void test_resources(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = decode_event();
    yvex_server_summary live = {0}, before;
    unsigned long long start = event.wall_time_ns;
    int index, snapshots = 0;
    live.metrics.resources.available = YVEX_EXECUTION_RESOURCE_PROCESS_AVAILABLE |
        YVEX_EXECUTION_RESOURCE_WORKSPACE_AVAILABLE | YVEX_EXECUTION_RESOURCE_SESSION_AVAILABLE;
    live.metrics.resources.process_rss_current_bytes = 98784247808ull;
    live.metrics.resources.workspace_current_bytes = 9985798963ull;
    live.metrics.resources.session_physical_state_bytes = 78957773ull;
    live.metrics.resources.model_device_addressable_bytes = 53687091200ull;
    live.metrics.active_requests = 1;
    before = live;
    yvex_cli_watch_renderer_open(&renderer, 0);
    for (index = 0; index < 100; ++index) {
        event.wall_time_ns = start + (unsigned long long)index * 1000000000ull;
        event.seconds = 40 + index;
        capture(&renderer, &event, &live);
        if (strstr(output, "RESOURCES")) snapshots++;
        if (index == 0) {
            expect(strstr(output, "host snapshot process-rss=92.0GiB workspace=9.3GiB") != NULL,
                   "resource snapshot is host-wide and has units");
            expect(!strstr(output, "device-alloc=0") && !strstr(output, "resident=0"),
                   "UMA mapping is not represented as zero GPU usage");
        }
    }
    expect(snapshots == 10, "100 progress events produce only 10 resource snapshots");
    event.kind = YVEX_SERVER_EVENT_GENERATION_COMPLETED;
    event.value_c = 3;
    capture(&renderer, &event, &live);
    expect(strstr(output, "DONE") && strstr(output, "RESOURCES"),
           "completion forces the final available snapshot");
    expect(!memcmp(&live, &before, sizeof(live)), "resource authority is unchanged");
    live.metrics.resources.available = 0;
    capture(&renderer, &event, &live);
    expect(!strstr(output, "process-rss=") && !strstr(output, "workspace="),
           "unavailable resources are not printed as measured");
    event.engine_kind = YVEX_SERVER_ENGINE_MEDIA;
    capture(&renderer, &event, NULL);
    expect(strstr(output, "frames=280 bytes=307 audio_samples=3") && !strstr(output, "tok/s"),
           "media completion does not borrow text-token units");
}

int main(void)
{
    yvex_cli_watch_renderer renderer;
    yvex_server_event event = decode_event();
    test_decode();
    test_lifecycle();
    test_progress_rates();
    test_http_access();
    test_prefill_cadence();
    test_resources();
    if (failures) return 1;
    yvex_cli_watch_renderer_open(&renderer, 0);
    yvex_cli_watch_renderer_event(&renderer, &event, NULL);
    puts("log renderer: PASS");
    return 0;
}
