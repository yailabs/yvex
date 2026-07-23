/* Owner: runtime.benchmark.
 * Owns: immutable runtime benchmark baselines, compatibility comparison, and deterministic SVG evidence.
 * Does not own: measurement, benchmark admission, runtime execution, CLI parsing, or capability promotion.
 * Invariants: canonical records are bounded, content-addressed, independently reopenable, and never overwritten.
 * Boundary: this file-serialization lifecycle consumes typed execution facts after production execution completes.
 * Purpose: persist trustworthy external benchmark evidence and compare equivalent measurements
 *   without policy thresholds.
 * Inputs: canonical identity keys, nanosecond statistics, and explicit external destination paths.
 * Effects: publication creates and syncs only unique owner-created files in a no-symlink directory walk.
 * Failure: typed refusal preserves existing files and removes only the exact temporary owned by the failed call. */

#define _GNU_SOURCE
#include <yvex/internal/benchmark.h>
#include <yvex/internal/runtime.h>

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <build_commit.h>

#define BENCHMARK_FILE_MAX (32u * 1024u)
#define BENCHMARK_CHART_MAX (64u * 1024u)
#define BENCHMARK_EMPTY_SOURCE_DELTA \
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

typedef yvex_core_bytes benchmark_bytes;

typedef enum {
    BENCHMARK_FIELD_COMMIT = 0,
    BENCHMARK_FIELD_SOURCE_STATE,
    BENCHMARK_FIELD_SHA256,
    BENCHMARK_FIELD_TEXT,
    BENCHMARK_FIELD_BOOL,
    BENCHMARK_FIELD_U64
} benchmark_field_kind;

typedef struct {
    const char *name;
    size_t offset, width;
    benchmark_field_kind kind;
} benchmark_field;

static const benchmark_field benchmark_key_fields[] = {
    {"commit", offsetof(yvex_runtime_benchmark_baseline, key.commit),
     sizeof(((yvex_runtime_benchmark_key *)0)->commit), BENCHMARK_FIELD_COMMIT},
    {"build_source_state", offsetof(yvex_runtime_benchmark_baseline, key.build_source_state),
     sizeof(((yvex_runtime_benchmark_key *)0)->build_source_state),
     BENCHMARK_FIELD_SOURCE_STATE},
    {"source_delta_identity",
     offsetof(yvex_runtime_benchmark_baseline, key.source_delta_identity),
     sizeof(((yvex_runtime_benchmark_key *)0)->source_delta_identity), BENCHMARK_FIELD_SHA256},
    {"build_identity", offsetof(yvex_runtime_benchmark_baseline, key.build_identity),
     sizeof(((yvex_runtime_benchmark_key *)0)->build_identity), BENCHMARK_FIELD_SHA256},
    {"artifact_identity", offsetof(yvex_runtime_benchmark_baseline, key.artifact_identity),
     sizeof(((yvex_runtime_benchmark_key *)0)->artifact_identity), BENCHMARK_FIELD_SHA256},
    {"runtime_binding_identity", offsetof(yvex_runtime_benchmark_baseline, key.runtime_binding_identity),
     sizeof(((yvex_runtime_benchmark_key *)0)->runtime_binding_identity),
     BENCHMARK_FIELD_SHA256},
    {"runtime_descriptor_identity", offsetof(yvex_runtime_benchmark_baseline, key.runtime_descriptor_identity),
     sizeof(((yvex_runtime_benchmark_key *)0)->runtime_descriptor_identity),
     BENCHMARK_FIELD_SHA256},
    {"execution_descriptor_identity", offsetof(yvex_runtime_benchmark_baseline,
                                                 key.execution_descriptor_identity),
     sizeof(((yvex_runtime_benchmark_key *)0)->execution_descriptor_identity),
     BENCHMARK_FIELD_SHA256},
    {"device", offsetof(yvex_runtime_benchmark_baseline, key.device),
     sizeof(((yvex_runtime_benchmark_key *)0)->device), BENCHMARK_FIELD_TEXT},
    {"driver", offsetof(yvex_runtime_benchmark_baseline, key.driver),
     sizeof(((yvex_runtime_benchmark_key *)0)->driver), BENCHMARK_FIELD_TEXT},
    {"cuda_build", offsetof(yvex_runtime_benchmark_baseline, key.cuda_build),
     sizeof(((yvex_runtime_benchmark_key *)0)->cuda_build), BENCHMARK_FIELD_TEXT},
    {"mode", offsetof(yvex_runtime_benchmark_baseline, key.mode),
     sizeof(((yvex_runtime_benchmark_key *)0)->mode), BENCHMARK_FIELD_TEXT},
    {"phase", offsetof(yvex_runtime_benchmark_baseline, key.phase),
     sizeof(((yvex_runtime_benchmark_key *)0)->phase), BENCHMARK_FIELD_TEXT},
    {"scope", offsetof(yvex_runtime_benchmark_baseline, key.scope),
     sizeof(((yvex_runtime_benchmark_key *)0)->scope), BENCHMARK_FIELD_TEXT},
    {"capture_bucket", offsetof(yvex_runtime_benchmark_baseline, key.capture_bucket),
     sizeof(((yvex_runtime_benchmark_key *)0)->capture_bucket), BENCHMARK_FIELD_TEXT},
    {"warmup_count", offsetof(yvex_runtime_benchmark_baseline, key.warmup_count),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"iteration_count", offsetof(yvex_runtime_benchmark_baseline, key.iteration_count),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
};

static const benchmark_field benchmark_metric_fields[] = {
    {"cold_total_ns", offsetof(yvex_runtime_benchmark_baseline, metrics.cold_total_ns),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cold_artifact_ns", offsetof(yvex_runtime_benchmark_baseline, metrics.cold_artifact_ns),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cold_residency_ns", offsetof(yvex_runtime_benchmark_baseline, metrics.cold_residency_ns),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cold_graph_ns", offsetof(yvex_runtime_benchmark_baseline, metrics.cold_graph_ns),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"minimum_ns", offsetof(yvex_runtime_benchmark_baseline,
                              metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_MINIMUM]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"p50_ns", offsetof(yvex_runtime_benchmark_baseline,
                          metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_P50]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"p90_ns", offsetof(yvex_runtime_benchmark_baseline,
                          metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_P90]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"p99_ns", offsetof(yvex_runtime_benchmark_baseline,
                          metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_P99]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"maximum_ns", offsetof(yvex_runtime_benchmark_baseline,
                              metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_MAXIMUM]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"mean_ns", offsetof(yvex_runtime_benchmark_baseline,
                           metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_MEAN]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"standard_deviation_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.host_timing.values[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_timing_available", offsetof(yvex_runtime_benchmark_baseline,
                                           metrics.device_timing.available),
     sizeof(int), BENCHMARK_FIELD_BOOL},
    {"device_minimum_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_MINIMUM]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_p50_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_P50]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_p90_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_P90]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_p99_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_P99]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_maximum_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_MAXIMUM]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_mean_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_MEAN]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"device_standard_deviation_ns", offsetof(yvex_runtime_benchmark_baseline,
        metrics.device_timing.values[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION]),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"artifact_bytes_hashed", offsetof(yvex_runtime_benchmark_baseline,
                                        metrics.artifact_bytes_hashed),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"resident_encoded_bytes", offsetof(yvex_runtime_benchmark_baseline,
                                         metrics.resident_encoded_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"resident_h2d_bytes", offsetof(yvex_runtime_benchmark_baseline, metrics.resident_h2d_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"warm_weight_reads", offsetof(yvex_runtime_benchmark_baseline, metrics.warm_weight_reads),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"warm_upload_bytes", offsetof(yvex_runtime_benchmark_baseline, metrics.warm_upload_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"warm_host_allocations", offsetof(yvex_runtime_benchmark_baseline,
                                        metrics.warm_host_allocations),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"warm_device_allocations", offsetof(yvex_runtime_benchmark_baseline,
                                          metrics.warm_device_allocations),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"warm_device_frees", offsetof(yvex_runtime_benchmark_baseline, metrics.warm_device_frees),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"last_dispatch_kernel_launches", offsetof(yvex_runtime_benchmark_baseline,
                                                metrics.last_dispatch_kernel_launches),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cuda_graph_launches", offsetof(yvex_runtime_benchmark_baseline,
                                     metrics.cuda_graph_launches),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cuda_graph_captures", offsetof(yvex_runtime_benchmark_baseline,
                                     metrics.cuda_graph_captures),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cuda_graph_replays", offsetof(yvex_runtime_benchmark_baseline, metrics.cuda_graph_replays),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"cuda_graph_nodes", offsetof(yvex_runtime_benchmark_baseline, metrics.cuda_graph_nodes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"peak_host_bytes", offsetof(yvex_runtime_benchmark_baseline, metrics.peak_host_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"last_dispatch_peak_device_bytes", offsetof(yvex_runtime_benchmark_baseline,
                                                  metrics.last_dispatch_peak_device_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"resident_bytes", offsetof(yvex_runtime_benchmark_baseline, metrics.resident_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"host_workspace_bytes", offsetof(yvex_runtime_benchmark_baseline, metrics.host_workspace_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
    {"state_bytes", offsetof(yvex_runtime_benchmark_baseline, metrics.state_bytes),
     sizeof(unsigned long long), BENCHMARK_FIELD_U64},
};

static const yvex_runtime_benchmark_failure_code benchmark_file_codes[] = {
    YVEX_RUNTIME_BENCHMARK_FAILURE_PUBLISH,
    YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
    YVEX_RUNTIME_BENCHMARK_FAILURE_PATH,
    YVEX_RUNTIME_BENCHMARK_FAILURE_CREATE,
    YVEX_RUNTIME_BENCHMARK_FAILURE_WRITE,
    YVEX_RUNTIME_BENCHMARK_FAILURE_SYNC,
    YVEX_RUNTIME_BENCHMARK_FAILURE_SYNC,
    YVEX_RUNTIME_BENCHMARK_FAILURE_CONFLICT,
    YVEX_RUNTIME_BENCHMARK_FAILURE_PUBLISH,
    YVEX_RUNTIME_BENCHMARK_FAILURE_SYNC,
    YVEX_RUNTIME_BENCHMARK_FAILURE_OPEN,
    YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS,
    YVEX_RUNTIME_BENCHMARK_FAILURE_READ,
    YVEX_RUNTIME_BENCHMARK_FAILURE_READ,
    YVEX_RUNTIME_BENCHMARK_FAILURE_READ,
    YVEX_RUNTIME_BENCHMARK_FAILURE_CLEANUP,
};

/* Purpose: publish one stable typed benchmark refusal and clear no caller-owned evidence. */
static int benchmark_reject(yvex_runtime_benchmark_failure *failure,
                            yvex_runtime_benchmark_failure_code code,
                            const char *field, unsigned long long expected,
                            unsigned long long actual, yvex_status status,
                            const char *reason, yvex_error *err)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->expected = expected;
        failure->actual = actual;
        failure->reason = reason;
        if (field) yvex_core_text_copy(failure->field, sizeof(failure->field), field);
    }
    yvex_error_set(err, status, "runtime_benchmark", reason);
    return status;
}

/* Purpose: reject one malformed benchmark field through the canonical typed failure owner. */
static int benchmark_field_reject(yvex_runtime_benchmark_failure *failure,
                                  const char *field, unsigned long long expected,
                                  unsigned long long actual, yvex_status status,
                                  const char *reason, yvex_error *err)
{
    return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                            field, expected, actual, status, reason, err);
}

/* Purpose: append one complete immutable ASCII string without hand-maintained byte counts. */
static int bytes_text(benchmark_bytes *bytes, const char *text)
{
    return text && yvex_core_bytes_append(bytes, text, strlen(text));
}

/* Purpose: append formatted ASCII while rejecting truncation and allocation failure.
 * Inputs: initialized buffer, trusted format, and matching scalar arguments.
 * Effects: appends exactly one complete formatted span.
 * Failure: returns false without advancing count on formatting or allocation failure.
 * Boundary: callers retain field validation and canonical ordering. */
static int bytes_format(benchmark_bytes *bytes, const char *format, ...)
{
    char stack[512];
    va_list args, copy;
    int needed;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(stack, sizeof(stack), format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return 0;
    }
    if ((size_t)needed < sizeof(stack)) {
        va_end(args);
        return yvex_core_bytes_append(bytes, stack, (size_t)needed);
    }
    if (!yvex_core_bytes_reserve(bytes, (size_t)needed)) {
        va_end(args);
        return 0;
    }
    needed = vsnprintf((char *)bytes->data + bytes->count,
                       bytes->capacity - bytes->count, format, args);
    va_end(args);
    if (needed < 0 || (size_t)needed >= bytes->capacity - bytes->count) return 0;
    bytes->count += (size_t)needed;
    return 1;
}

/* Purpose: validate one bounded printable field that enters identity and text serialization. */
static int text_valid(const char *text, size_t capacity)
{
    size_t index, length;

    if (!text || !memchr(text, '\0', capacity)) return 0;
    length = strlen(text);
    if (!length) return 0;
    for (index = 0u; index < length; ++index) {
        unsigned char value = (unsigned char)text[index];
        if (value < 0x20u || value > 0x7eu || value == '\t') return 0;
    }
    return 1;
}

/* Purpose: copy one benchmark text field without accepting truncation. */
static int benchmark_text_copy(char *output, size_t capacity, const char *text)
{
    return output && text && capacity &&
           snprintf(output, capacity, "%s", text) < (int)capacity;
}

/* Purpose: convert one finite nonnegative duration to canonical nanoseconds. */
static int benchmark_seconds_ns(double seconds, unsigned long long *output)
{
    double integral_seconds, fractional_seconds;
    unsigned long long integral_ns, fractional_ns;

    if (!output || !isfinite(seconds) || seconds < 0.0) return 0;
    fractional_seconds = modf(seconds, &integral_seconds);
    if (integral_seconds > (double)(ULLONG_MAX / 1000000000ull)) return 0;
    fractional_ns = (unsigned long long)(fractional_seconds * 1000000000.0 + 0.5);
    return yvex_core_u64_mul((unsigned long long)integral_seconds, 1000000000ull,
                             &integral_ns) &&
           yvex_core_u64_add(integral_ns, fractional_ns, output);
}

typedef struct {
    double values[YVEX_RUNTIME_BENCHMARK_STATISTIC_COUNT];
} benchmark_seconds_distribution;

/* Purpose: impose one total numeric ordering over finite benchmark samples. */
static int benchmark_sample_compare(const void *left, const void *right)
{
    const double a = *(const double *)left, b = *(const double *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

/* Purpose: validate, sort, and summarize one positive finite timing sample set.
 * Inputs: mutable sample storage, positive representable count, and summary output.
 * Effects: sorts caller-owned samples and fills the summary only after scalar validation.
 * Failure: returns false for null, empty, oversized, non-finite, or non-positive samples.
 * Boundary: the helper computes distribution facts but neither publishes nor interprets them. */
static int benchmark_samples_summarize(double *samples, unsigned long long count,
                                       benchmark_seconds_distribution *summary)
{
    double sum = 0.0, squared = 0.0;
    unsigned long long index;

    if (!samples || !summary || !count || count > (unsigned long long)(SIZE_MAX / sizeof(*samples)))
        return 0;
    for (index = 0ull; index < count; ++index)
        if (!isfinite(samples[index]) || samples[index] <= 0.0) return 0;
    qsort(samples, (size_t)count, sizeof(*samples), benchmark_sample_compare);
    for (index = 0ull; index < count; ++index) sum += samples[index];
    summary->values[YVEX_RUNTIME_BENCHMARK_MINIMUM] = samples[0];
    summary->values[YVEX_RUNTIME_BENCHMARK_P50] =
        samples[(count * 50ull + 99ull) / 100ull - 1ull];
    summary->values[YVEX_RUNTIME_BENCHMARK_P90] =
        samples[(count * 90ull + 99ull) / 100ull - 1ull];
    summary->values[YVEX_RUNTIME_BENCHMARK_P99] =
        samples[(count * 99ull + 99ull) / 100ull - 1ull];
    summary->values[YVEX_RUNTIME_BENCHMARK_MAXIMUM] = samples[count - 1ull];
    summary->values[YVEX_RUNTIME_BENCHMARK_MEAN] = sum / (double)count;
    for (index = 0ull; index < count; ++index) {
        const double delta = samples[index] - summary->values[YVEX_RUNTIME_BENCHMARK_MEAN];
        squared += delta * delta;
    }
    summary->values[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION] =
        sqrt(squared / (double)count);
    return isfinite(sum) && isfinite(squared) &&
           isfinite(summary->values[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION]);
}

/* Purpose: publish host and optional device benchmark distributions through one owner.
 * Inputs: mutable sample arrays, positive count, device requirement, and result.
 * Effects: sorts samples and replaces only benchmark distribution fields.
 * Failure: invalid host or requested-device samples publish no partial distribution.
 * Boundary: sample statistics record evidence and promote no execution capability. */
int yvex_runtime_benchmark_samples_finish(
    double *host_seconds, double *device_seconds, unsigned long long count,
    int device_requested, yvex_graph_attention_operator_result *result, yvex_error *err)
{
    benchmark_seconds_distribution host, device = {0};

    if (!result || (device_requested != 0 && device_requested != 1) ||
        !benchmark_samples_summarize(host_seconds, count, &host))
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                                "host-samples", count, 0ull, YVEX_ERR_BOUNDS,
                                "benchmark samples must be positive, finite, and complete", err);
    if (device_requested && !benchmark_samples_summarize(device_seconds, count, &device))
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                                "device-samples", count, 0ull, YVEX_ERR_BOUNDS,
                                "device benchmark samples must be positive and finite", err);
    result->benchmark_sample_count = count;
    result->benchmark_minimum_seconds = host.values[YVEX_RUNTIME_BENCHMARK_MINIMUM];
    result->benchmark_p50_seconds = host.values[YVEX_RUNTIME_BENCHMARK_P50];
    result->benchmark_p90_seconds = host.values[YVEX_RUNTIME_BENCHMARK_P90];
    result->benchmark_p99_seconds = host.values[YVEX_RUNTIME_BENCHMARK_P99];
    result->benchmark_maximum_seconds = host.values[YVEX_RUNTIME_BENCHMARK_MAXIMUM];
    result->benchmark_mean_seconds = host.values[YVEX_RUNTIME_BENCHMARK_MEAN];
    result->benchmark_standard_deviation_seconds =
        host.values[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION];
    result->benchmark_device_timing_available = device_requested;
    result->benchmark_device_minimum_seconds = device.values[YVEX_RUNTIME_BENCHMARK_MINIMUM];
    result->benchmark_device_p50_seconds = device.values[YVEX_RUNTIME_BENCHMARK_P50];
    result->benchmark_device_p90_seconds = device.values[YVEX_RUNTIME_BENCHMARK_P90];
    result->benchmark_device_p99_seconds = device.values[YVEX_RUNTIME_BENCHMARK_P99];
    result->benchmark_device_maximum_seconds = device.values[YVEX_RUNTIME_BENCHMARK_MAXIMUM];
    result->benchmark_device_mean_seconds = device.values[YVEX_RUNTIME_BENCHMARK_MEAN];
    result->benchmark_device_standard_deviation_seconds =
        device.values[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION];
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: identify the CPU execution host for an identity-bound baseline.
 * Inputs: caller-owned bounded labels and typed error output.
 * Effects: reads immutable kernel host facts and fills both labels.
 * Failure: unavailable or oversized host identity returns a typed refusal.
 * Boundary: host identification does not probe runtime/backend capability. */
static int benchmark_cpu_identity(char device[YVEX_RUNTIME_BENCHMARK_TEXT_CAP],
                                  char driver[YVEX_RUNTIME_BENCHMARK_TEXT_CAP],
                                  yvex_error *err)
{
    struct utsname host;
    long processors;
    int device_length, driver_length;

    if (uname(&host) != 0 || (processors = sysconf(_SC_NPROCESSORS_ONLN)) <= 0)
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                                "device", 1ull, 0ull, YVEX_ERR_IO,
                                "CPU benchmark host identity is unavailable", err);
    device_length = snprintf(device, YVEX_RUNTIME_BENCHMARK_TEXT_CAP,
                             "cpu:%s:%s:%ld", host.nodename, host.machine, processors);
    driver_length = snprintf(driver, YVEX_RUNTIME_BENCHMARK_TEXT_CAP,
                             "kernel:%s:%s", host.sysname, host.release);
    if (device_length < 0 || device_length >= (int)YVEX_RUNTIME_BENCHMARK_TEXT_CAP ||
        driver_length < 0 || driver_length >= (int)YVEX_RUNTIME_BENCHMARK_TEXT_CAP)
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                                "device", YVEX_RUNTIME_BENCHMARK_TEXT_CAP, 0ull,
                                YVEX_ERR_BOUNDS, "CPU benchmark identity is too long", err);
    return YVEX_OK;
}

/* Purpose: project immutable execution and build facts into one benchmark key.
 * Inputs: completed production result and caller-owned key.
 * Effects: copies bounded identity, build, backend, phase, mode, and scope facts.
 * Failure: unavailable CPU identity or field overflow refuses the projection.
 * Boundary: key projection neither seals evidence nor decides performance policy. */
static int benchmark_attention_key(const yvex_graph_attention_operator_result *result,
                                   yvex_runtime_benchmark_key *key, yvex_error *err)
{
    char cpu_device[YVEX_RUNTIME_BENCHMARK_TEXT_CAP] = {0};
    char cpu_driver[YVEX_RUNTIME_BENCHMARK_TEXT_CAP] = {0};
    const int cuda = strcmp(result->backend, "cuda") == 0;
    const char *device = result->probe.cuda_device, *driver = result->cuda_driver;
    const char *text[] = {
        YVEX_BUILD_COMMIT,
        YVEX_BUILD_SOURCE_STATE,
        YVEX_BUILD_SOURCE_DELTA_IDENTITY,
        YVEX_BUILD_IDENTITY,
        result->artifact_identity,
        result->runtime_binding_identity,
        result->runtime_descriptor_identity,
        result->execution_descriptor_identity,
        device,
        driver,
        cuda ? result->cuda_build_identity : "not-applicable",
        result->selected_mode,
        result->phase,
        result->operation_scope,
        result->capture_bucket,
    };
    size_t index;

    if (!cuda && benchmark_cpu_identity(cpu_device, cpu_driver, err) != YVEX_OK)
        return yvex_error_code(err);
    if (!cuda) {
        text[8] = cpu_device;
        text[9] = cpu_driver;
    }
    for (index = 0u; index < sizeof(text) / sizeof(text[0]); ++index) {
        const benchmark_field *field = &benchmark_key_fields[index];
        char *destination = (char *)key + field->offset - offsetof(
            yvex_runtime_benchmark_baseline, key);
        if (field->kind == BENCHMARK_FIELD_U64 ||
            !benchmark_text_copy(destination, field->width, text[index]))
            return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                                    "key", 1ull, 0ull, YVEX_ERR_BOUNDS,
                                    "attention benchmark key exceeds its fixed capacity", err);
    }
    key->warmup_count = result->warmup_count;
    key->iteration_count = result->benchmark_sample_count;
    return YVEX_OK;
}

/* Purpose: convert one complete seconds distribution into canonical nanoseconds. */
static int benchmark_timing_project(
    const benchmark_seconds_distribution *source, int available,
    yvex_runtime_benchmark_timing_distribution *target)
{
    size_t index;

    memset(target, 0, sizeof(*target));
    target->available = available;
    for (index = 0u; index < YVEX_RUNTIME_BENCHMARK_STATISTIC_COUNT; ++index) {
        if ((!available && source->values[index] != 0.0) ||
            (available && !benchmark_seconds_ns(source->values[index], &target->values[index])))
            return 0;
    }
    return !available || target->values[YVEX_RUNTIME_BENCHMARK_MINIMUM] != 0ull;
}

/* Purpose: project measured lifecycle and latency values into canonical nanoseconds.
 * Inputs: completed production timings and caller-owned metrics.
 * Effects: converts each finite duration using one checked rounding contract.
 * Failure: negative, non-finite, or unrepresentable timing refuses the projection.
 * Boundary: conversion does not measure, publish, or interpret performance. */
static int benchmark_attention_times(const yvex_graph_attention_operator_result *result,
                                     yvex_runtime_benchmark_metrics *metrics,
                                     yvex_error *err)
{
    const benchmark_seconds_distribution host = {{
        result->benchmark_minimum_seconds, result->benchmark_p50_seconds,
        result->benchmark_p90_seconds, result->benchmark_p99_seconds,
        result->benchmark_maximum_seconds, result->benchmark_mean_seconds,
        result->benchmark_standard_deviation_seconds,
    }};
    const benchmark_seconds_distribution device = {{
        result->benchmark_device_minimum_seconds, result->benchmark_device_p50_seconds,
        result->benchmark_device_p90_seconds, result->benchmark_device_p99_seconds,
        result->benchmark_device_maximum_seconds, result->benchmark_device_mean_seconds,
        result->benchmark_device_standard_deviation_seconds,
    }};
    double artifact, graph, total;

    artifact = result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_ARTIFACT_OPEN] +
               result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_ARTIFACT_HASH] +
               result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_ARTIFACT_ADMISSION];
    graph = result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_BINDING_OPEN] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_MATERIALIZATION_OPEN] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_MODEL_SEAL] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_BACKEND_OPEN] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_WORKSPACE_PREPARE] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_GRAPH_WARMUP] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_GRAPH_CAPTURE] +
            result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_GRAPH_INSTANTIATE];
    total = artifact + graph + result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_RESIDENCY];
    if (!benchmark_seconds_ns(total, &metrics->cold_total_ns) ||
        !benchmark_seconds_ns(artifact, &metrics->cold_artifact_ns) ||
        !benchmark_seconds_ns(result->lifecycle_seconds[YVEX_RUNTIME_LIFECYCLE_RESIDENCY],
                              &metrics->cold_residency_ns) ||
        !benchmark_seconds_ns(graph, &metrics->cold_graph_ns) ||
        !benchmark_timing_project(&host, 1, &metrics->host_timing))
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS, "timing",
                                1ull, 0ull, YVEX_ERR_BOUNDS,
                                "attention benchmark timing is not representable", err);
    if (result->benchmark_device_timing_available != 0 &&
        result->benchmark_device_timing_available != 1)
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
                                "device-timing-availability", 1ull,
                                (unsigned long long)result->benchmark_device_timing_available,
                                YVEX_ERR_FORMAT, "device timing availability is not canonical", err);
    if (!benchmark_timing_project(&device, result->benchmark_device_timing_available,
                                  &metrics->device_timing))
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS,
                                "device-timing", result->benchmark_device_timing_available, 0ull,
                                YVEX_ERR_BOUNDS,
                                "device timing values disagree with availability", err);
    return YVEX_OK;
}

/* Purpose: project resource and dispatch counters without reconstructing runtime truth.
 * Inputs: completed production counters and caller-owned metrics.
 * Effects: copies counters and derives checked aggregate memory extents.
 * Failure: memory-accounting overflow refuses the projection.
 * Boundary: projection cannot infer capability from the copied evidence. */
static int benchmark_attention_metrics(const yvex_graph_attention_operator_result *result,
                                       yvex_runtime_benchmark_metrics *metrics,
                                       yvex_error *err)
{
    unsigned long long peak;

    metrics->artifact_bytes_hashed = result->artifact_bytes_hashed;
    metrics->resident_encoded_bytes = result->resident_encoded_bytes;
    metrics->resident_h2d_bytes = result->upload_bytes;
    metrics->warm_weight_reads = result->warm_weight_artifact_reads;
    metrics->warm_upload_bytes = result->warm_weight_upload_bytes;
    metrics->warm_host_allocations = result->warm_host_allocations;
    metrics->warm_device_allocations = result->warm_device_allocations;
    metrics->warm_device_frees = result->warm_device_frees;
    metrics->last_dispatch_kernel_launches = result->probe.kernel_launches;
    metrics->cuda_graph_launches = result->cuda_graph_launch_count;
    metrics->cuda_graph_captures = result->cuda_graph_capture_count;
    metrics->cuda_graph_replays = result->cuda_graph_replay_count;
    metrics->cuda_graph_nodes = result->cuda_graph_node_count;
    metrics->last_dispatch_peak_device_bytes = result->probe.peak_device_bytes;
    metrics->host_workspace_bytes = result->workspace_bytes;
    metrics->state_bytes = result->state_allocated_bytes;
    if (!yvex_core_u64_add(result->host_resident_bytes, result->device_resident_bytes,
                           &metrics->resident_bytes) ||
        !yvex_core_u64_add(result->host_resident_bytes, result->workspace_bytes, &peak) ||
        !yvex_core_u64_add(peak, result->pinned_host_peak_bytes, &peak) ||
        !yvex_core_u64_add(peak, result->state_allocated_bytes, &metrics->peak_host_bytes))
        return benchmark_reject(NULL, YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS,
                                "memory", 1ull, 0ull, YVEX_ERR_BOUNDS,
                                "attention benchmark memory accounting overflowed", err);
    return YVEX_OK;
}

/* Purpose: validate a full lowercase hexadecimal commit identity.
 * Inputs: fixed-capacity NUL-terminated commit field.
 * Effects: none.
 * Failure: returns false for short, long, uppercase, or nonhex content.
 * Boundary: validation does not discover repository state. */
static int commit_valid(const char commit[YVEX_RUNTIME_BENCHMARK_COMMIT_CAP])
{
    return commit && strnlen(commit, YVEX_RUNTIME_BENCHMARK_COMMIT_CAP) == 40u &&
           strspn(commit, "0123456789abcdef") == 40u;
}

/* Purpose: admit only the two source-tree states captured by the generated build provenance. */
static int source_state_valid(const char state[YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP])
{
    return state && (strcmp(state, "clean") == 0 || strcmp(state, "dirty") == 0);
}

/* Purpose: name the first execution vocabulary field outside the canonical benchmark domain. */
static const char *key_vocabulary_mismatch(const yvex_runtime_benchmark_baseline *record)
{
    if (strcmp(record->key.mode, "eager") != 0 &&
        strcmp(record->key.mode, "piecewise") != 0 && strcmp(record->key.mode, "full") != 0)
        return "mode";
    if (strcmp(record->key.phase, "prefill") != 0 && strcmp(record->key.phase, "decode") != 0)
        return "phase";
    if (strcmp(record->key.scope, "core") != 0 && strcmp(record->key.scope, "envelope") != 0 &&
        strcmp(record->key.scope, "release-attention-set") != 0)
        return "scope";
    return NULL;
}

/* Purpose: validate the single canonical key-field schema used by hash, I/O, and equality.
 * Inputs: complete record plus typed refusal outputs.
 * Effects: writes refusal state only.
 * Failure: rejects the first malformed commit, identity, or printable text field.
 * Boundary: scalar geometry and measured values remain separately validated. */
static int key_validate(const yvex_runtime_benchmark_baseline *record,
                        yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    const unsigned char *base = (const unsigned char *)record;
    const char *vocabulary;
    size_t index;

    if (!record)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
                                "key", 1ull, 0ull, YVEX_ERR_INVALID_ARG, "benchmark key is required", err);
    for (index = 0u; index < sizeof(benchmark_key_fields) /
                                  sizeof(benchmark_key_fields[0]); ++index) {
        const benchmark_field *field = &benchmark_key_fields[index];
        const char *text = (const char *)(base + field->offset);
        int valid = field->kind == BENCHMARK_FIELD_U64 ||
                    (field->kind == BENCHMARK_FIELD_COMMIT && commit_valid(text)) ||
                    (field->kind == BENCHMARK_FIELD_SOURCE_STATE && source_state_valid(text)) ||
                    (field->kind == BENCHMARK_FIELD_SHA256 && yvex_sha256_hex_valid(text)) ||
                    (field->kind == BENCHMARK_FIELD_TEXT && text_valid(text, field->width));
        if (!valid) {
            unsigned long long expected = field->kind == BENCHMARK_FIELD_COMMIT ? 40ull :
                                          field->kind == BENCHMARK_FIELD_SHA256 ? 64ull : 1ull;
            return benchmark_field_reject(
                failure, field->name, expected, strnlen(text, field->width),
                YVEX_ERR_FORMAT,
                field->kind == BENCHMARK_FIELD_COMMIT
                    ? "benchmark commit must be a full lowercase SHA"
                : field->kind == BENCHMARK_FIELD_SOURCE_STATE
                    ? "benchmark source state must be clean or dirty"
                : field->kind == BENCHMARK_FIELD_SHA256
                    ? "benchmark identity field is invalid"
                    : "benchmark text field is invalid",
                err);
        }
    }
    if ((strcmp(record->key.build_source_state, "clean") == 0) !=
        (strcmp(record->key.source_delta_identity, BENCHMARK_EMPTY_SOURCE_DELTA) == 0))
        return benchmark_field_reject(
            failure, "source_delta_identity", 1ull, 0ull, YVEX_ERR_FORMAT,
            "benchmark source state and exact delta identity disagree", err);
    vocabulary = key_vocabulary_mismatch(record);
    if (vocabulary)
        return benchmark_field_reject(failure, vocabulary, 1ull, 0ull, YVEX_ERR_FORMAT,
                                      "benchmark execution vocabulary is not canonical", err);
    if (!record->key.iteration_count)
        return benchmark_field_reject(failure, "iteration_count", 1ull, 0ull,
                                      YVEX_ERR_BOUNDS,
                                      "benchmark iteration count must be positive", err);
    return YVEX_OK;
}

/* Purpose: append canonical schema fields to a hash without hashing native structure bytes.
 * Inputs: active hash, immutable record, and its ordered field table.
 * Effects: appends every declared scalar exactly once in canonical order.
 * Failure: returns false on any canonical hash update failure.
 * Boundary: pointers, padding, native layout, and destination paths remain excluded. */
static int fields_hash(yvex_sha256 *hash, const yvex_runtime_benchmark_baseline *record,
                       const benchmark_field *fields, size_t count)
{
    const unsigned char *base = (const unsigned char *)record;
    size_t index;

    for (index = 0u; index < count; ++index) {
        unsigned long long value;
        if (fields[index].kind == BENCHMARK_FIELD_BOOL) {
            int boolean;
            memcpy(&boolean, base + fields[index].offset, sizeof(boolean));
            if (!yvex_sha256_update_u64(hash, (unsigned long long)boolean)) return 0;
            continue;
        }
        if (fields[index].kind != BENCHMARK_FIELD_U64) {
            if (!yvex_sha256_update_text(hash, (const char *)(base + fields[index].offset)))
                return 0;
            continue;
        }
        memcpy(&value, base + fields[index].offset, sizeof(value));
        if (!yvex_sha256_update_u64(hash, value)) return 0;
    }
    return 1;
}

/* Purpose: serialize canonical schema fields in their single declared order.
 * Inputs: bounded byte sink, sealed record, and one immutable schema field table.
 * Effects: appends one canonical text line for each declared field.
 * Failure: returns false on formatting, allocation, or configured byte-bound failure.
 * Boundary: field validation and final identity-line ownership remain with the baseline lifecycle. */
static int fields_serialize(benchmark_bytes *bytes,
                            const yvex_runtime_benchmark_baseline *record,
                            const benchmark_field *fields, size_t count)
{
    const unsigned char *base = (const unsigned char *)record;
    size_t index;

    for (index = 0u; index < count; ++index) {
        unsigned long long value;
        if (fields[index].kind == BENCHMARK_FIELD_BOOL) {
            int boolean;
            memcpy(&boolean, base + fields[index].offset, sizeof(boolean));
            if (!bytes_format(bytes, "%s\t%d\n", fields[index].name, boolean)) return 0;
            continue;
        }
        if (fields[index].kind != BENCHMARK_FIELD_U64) {
            if (!bytes_format(bytes, "%s\t%s\n", fields[index].name,
                              (const char *)(base + fields[index].offset)))
                return 0;
            continue;
        }
        memcpy(&value, base + fields[index].offset, sizeof(value));
        if (!bytes_format(bytes, "%s\t%016llx\n", fields[index].name, value)) return 0;
    }
    return 1;
}

/* Purpose: validate one host or device distribution under an exact availability contract.
 * Inputs: immutable canonical timing values and the backend-required availability bit.
 * Effects: none.
 * Failure: returns false for unavailable residue, unordered values, zero minima, or signed overflow.
 * Boundary: validates measured facts without assigning performance policy. */
static int timing_valid(const yvex_runtime_benchmark_timing_distribution *timing,
                        int expected_available)
{
    const unsigned long long *value = timing->values;
    size_t index;

    if (timing->available != expected_available) return 0;
    if (!timing->available) {
        for (index = 0u; index < YVEX_RUNTIME_BENCHMARK_STATISTIC_COUNT; ++index)
            if (value[index]) return 0;
        return 1;
    }
    return value[YVEX_RUNTIME_BENCHMARK_MINIMUM] != 0ull &&
           value[YVEX_RUNTIME_BENCHMARK_MINIMUM] <= value[YVEX_RUNTIME_BENCHMARK_P50] &&
           value[YVEX_RUNTIME_BENCHMARK_P50] <= value[YVEX_RUNTIME_BENCHMARK_P90] &&
           value[YVEX_RUNTIME_BENCHMARK_P90] <= value[YVEX_RUNTIME_BENCHMARK_P99] &&
           value[YVEX_RUNTIME_BENCHMARK_P99] <= value[YVEX_RUNTIME_BENCHMARK_MAXIMUM] &&
           value[YVEX_RUNTIME_BENCHMARK_MEAN] >= value[YVEX_RUNTIME_BENCHMARK_MINIMUM] &&
           value[YVEX_RUNTIME_BENCHMARK_MEAN] <= value[YVEX_RUNTIME_BENCHMARK_MAXIMUM] &&
           value[YVEX_RUNTIME_BENCHMARK_MAXIMUM] <= (unsigned long long)LLONG_MAX &&
           value[YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION] <=
               (unsigned long long)LLONG_MAX;
}

/* Purpose: validate ordered timing, resource, and execution-mode benchmark evidence.
 * Inputs: one complete benchmark record plus optional typed refusal outputs.
 * Effects: writes only refusal state on failure.
 * Failure: rejects zero evidence, unordered percentiles, or contradictory graph counters.
 * Boundary: validation establishes execution consistency, not performance policy. */
static int metrics_validate(const yvex_runtime_benchmark_baseline *record,
                            yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    const yvex_runtime_benchmark_metrics *metrics;
    const yvex_runtime_benchmark_timing_distribution *device;
    unsigned long long cold = 0ull;
    unsigned int graph_fields;
    int eager, cuda;

    if (!record)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
                                "metrics", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "benchmark metrics are required", err);
    metrics = &record->metrics;
    device = &metrics->device_timing;
    if (!metrics->cold_total_ns || !timing_valid(&metrics->host_timing, 1) ||
        metrics->cold_total_ns > (unsigned long long)LLONG_MAX)
        return benchmark_field_reject(
            failure, "metrics", 1ull, 0ull, YVEX_ERR_BOUNDS,
            "benchmark nanosecond statistics are invalid or unordered", err);
    cuda = strcmp(record->key.cuda_build, "not-applicable") != 0;
    if (!timing_valid(device, cuda))
        return benchmark_field_reject(
            failure, "device-timing", cuda ? 1ull : 0ull,
            (unsigned long long)(device->available == 1), YVEX_ERR_BOUNDS,
            "device timing availability or distribution is invalid", err);
    if (!metrics->artifact_bytes_hashed || !metrics->resident_encoded_bytes ||
        !metrics->resident_bytes || !metrics->state_bytes)
        return benchmark_field_reject(failure, "structural-evidence", 1ull, 0ull,
                                      YVEX_ERR_BOUNDS,
                                      "benchmark structural byte evidence is incomplete", err);
    if (!yvex_core_u64_add(metrics->cold_artifact_ns, metrics->cold_residency_ns, &cold) ||
        !yvex_core_u64_add(cold, metrics->cold_graph_ns, &cold) ||
        cold != metrics->cold_total_ns ||
        metrics->resident_encoded_bytes > metrics->resident_bytes ||
        metrics->peak_host_bytes < metrics->host_workspace_bytes ||
        metrics->peak_host_bytes < metrics->state_bytes)
        return benchmark_field_reject(
            failure, "resource-accounting", metrics->cold_total_ns, cold,
            YVEX_ERR_BOUNDS,
            "benchmark phase or owned-memory accounting is inconsistent", err);
    graph_fields = (metrics->cuda_graph_launches != 0ull) +
                   (metrics->cuda_graph_captures != 0ull) +
                   (metrics->cuda_graph_replays != 0ull) +
                   (metrics->cuda_graph_nodes != 0ull);
    eager = strcmp(record->key.mode, "eager") == 0;
    if ((eager && graph_fields) || (!eager && graph_fields != 4u))
        return benchmark_field_reject(
            failure, "execution-mode-evidence", eager ? 0ull : 4ull, graph_fields,
            YVEX_ERR_STATE,
            eager ? "eager benchmark cannot claim CUDA Graph evidence"
                  : "graph benchmark mode requires four complete CUDA Graph counters",
            err);
    if (metrics->warm_weight_reads || metrics->warm_upload_bytes ||
        metrics->warm_host_allocations || metrics->warm_device_allocations ||
        metrics->warm_device_frees)
        return benchmark_field_reject(
            failure, "steady-state", 0ull, 1ull, YVEX_ERR_STATE,
            "benchmark steady-state allocation or weight-transfer invariant failed", err);
    return YVEX_OK;
}

/* Purpose: hash one record using canonical scalar values rather than C object representation.
 * Inputs: validated record and fixed-capacity identity output.
 * Effects: writes one lowercase SHA-256 identity.
 * Failure: returns false when canonical hash updates or finalization fail.
 * Boundary: excludes pointers, padding, native layout, and destination path. */
static int baseline_identity(const yvex_runtime_benchmark_baseline *record,
                             char output[YVEX_SHA256_HEX_BYTES])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.runtime.benchmark.baseline.v4") ||
        !yvex_sha256_update_u64(&hash, record->schema_version) ||
        !fields_hash(&hash, record, benchmark_key_fields,
                     sizeof(benchmark_key_fields) / sizeof(benchmark_key_fields[0])) ||
        !fields_hash(&hash, record, benchmark_metric_fields,
                     sizeof(benchmark_metric_fields) / sizeof(benchmark_metric_fields[0])) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

/* Purpose: prove a borrowed record is sealed under its exact canonical identity.
 * Inputs: immutable record plus optional typed refusal outputs.
 * Effects: writes only refusal state on failure.
 * Failure: rejects schema, field, metric, or identity mismatch.
 * Boundary: validation never changes or reseals borrowed evidence. */
static int baseline_validate(const yvex_runtime_benchmark_baseline *record,
                             yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    char identity[YVEX_SHA256_HEX_BYTES];
    int rc;

    if (!record)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
                                "record", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "benchmark record is required", err);
    if (record->schema_version != YVEX_RUNTIME_BENCHMARK_SCHEMA_V4)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_SCHEMA, "schema",
                                YVEX_RUNTIME_BENCHMARK_SCHEMA_V4, record->schema_version,
                                YVEX_ERR_UNSUPPORTED, "benchmark schema is unsupported", err);
    if ((rc = key_validate(record, failure, err)) != YVEX_OK ||
        (rc = metrics_validate(record, failure, err)) != YVEX_OK)
        return rc;
    if (!baseline_identity(record, identity) || strcmp(identity, record->identity) != 0)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_IDENTITY,
                                "baseline_identity", 1ull, 0ull, YVEX_ERR_STATE,
                                "benchmark baseline identity does not match canonical content", err);
    return YVEX_OK;
}

/* Purpose: serialize one validated record into the exact version-four line format.
 * Inputs: sealed record and empty owner-controlled byte buffer.
 * Effects: appends every field in one fixed order and encoding.
 * Failure: returns false on bound or allocation failure.
 * Boundary: serialization does not publish or reopen files. */
static int baseline_serialize(const yvex_runtime_benchmark_baseline *record,
                              benchmark_bytes *bytes)
{
    bytes->maximum = BENCHMARK_FILE_MAX;
    bytes->initial_capacity = 1024u;
    return bytes_format(bytes, "YVEX_RUNTIME_BENCHMARK_BASELINE\t4\n") &&
           bytes_format(bytes, "identity\t%s\n", record->identity) &&
           fields_serialize(bytes, record, benchmark_key_fields,
                            sizeof(benchmark_key_fields) / sizeof(benchmark_key_fields[0])) &&
           fields_serialize(bytes, record, benchmark_metric_fields,
                            sizeof(benchmark_metric_fields) /
                                sizeof(benchmark_metric_fields[0]));
}

/* Purpose: translate core file mechanics into the benchmark evidence failure vocabulary.
 * Inputs: one typed core lifecycle stage.
 * Effects: returns a domain code without changing either failure object.
 * Failure: unknown stages map to a fail-closed publication failure.
 * Boundary: mapping does not alter the original filesystem status or evidence bytes. */
static yvex_runtime_benchmark_failure_code benchmark_file_code(yvex_core_file_stage stage)
{
    return (unsigned int)stage < sizeof(benchmark_file_codes) /
                                    sizeof(benchmark_file_codes[0])
               ? benchmark_file_codes[stage] : YVEX_RUNTIME_BENCHMARK_FAILURE_PUBLISH;
}

/* Purpose: name the exact owned cleanup operation retained by the core file lifecycle. */
static const char *benchmark_cleanup_field(yvex_core_file_cleanup_stage stage)
{
    switch (stage) {
    case YVEX_CORE_FILE_CLEANUP_FILE_CLOSE: return "cleanup-file-close";
    case YVEX_CORE_FILE_CLEANUP_TEMPORARY_UNLINK: return "cleanup-temporary-unlink";
    case YVEX_CORE_FILE_CLEANUP_DESTINATION_UNLINK: return "cleanup-destination-unlink";
    case YVEX_CORE_FILE_CLEANUP_DIRECTORY_SYNC: return "cleanup-directory-sync";
    case YVEX_CORE_FILE_CLEANUP_NONE: break;
    }
    return "file-cleanup";
}

/* Purpose: transactionally publish exact bytes through one no-symlink, no-replace file lifecycle.
 * Inputs: safe path, exact immutable bytes, optional lifecycle fault controls, and typed
 * publication/refusal outputs.
 * Effects: creates, writes, syncs, atomically links, and directory-syncs one file.
 * Failure: removes only its exact temporary or newly published candidate.
 * Boundary: bytes are canonical; fault controls remain explicit lifecycle-test inputs. */
static int publish_bytes(const char *path, const void *data, size_t count,
                         const yvex_core_file_faults *faults,
                         yvex_runtime_benchmark_publication *result,
                         yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    yvex_core_file_result file_result;
    int rc;

    if (result) memset(result, 0, sizeof(*result));
    if (!result || !data || !count || !path || path[0] != '/')
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_PATH,
                                "path", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "benchmark publication requires an absolute external path", err);
    memset(&file_result, 0, sizeof(file_result));
    rc = yvex_core_file_publish_noreplace(
        path, data, count, faults, NULL, NULL, &file_result, err);
    if (file_result.cleanup_stage != YVEX_CORE_FILE_CLEANUP_NONE)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_CLEANUP,
                                benchmark_cleanup_field(file_result.cleanup_stage), 0ull,
                                (unsigned long long)file_result.cleanup_system_error,
                                YVEX_ERR_IO, "benchmark file cleanup failed", err);
    if (rc != YVEX_OK)
        return benchmark_reject(failure, benchmark_file_code(file_result.stage),
                                "file-lifecycle", file_result.expected, file_result.actual, (yvex_status)rc,
                                "benchmark file lifecycle failed", err);
    result->published = 1;
    result->file_bytes = count;
    yvex_core_text_copy(result->path, sizeof(result->path), path);
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Purpose: extract one exact key/value line in canonical order without accepting duplicates.
 * Inputs: mutable cursor/end bounds, expected key, and borrowed value output.
 * Effects: terminates one line in place and advances the cursor exactly once.
 * Failure: returns false for missing newline/tab, wrong key, or empty value.
 * Boundary: field-specific capacity and type parsing remain caller-owned. */
static int parse_line(char **cursor, char *end, const char *key, char **value)
{
    char *line_end, *separator;
    size_t key_length = strlen(key);

    if (!cursor || !*cursor || *cursor >= end) return 0;
    line_end = memchr(*cursor, '\n', (size_t)(end - *cursor));
    if (!line_end) return 0;
    separator = memchr(*cursor, '\t', (size_t)(line_end - *cursor));
    if (!separator || (size_t)(separator - *cursor) != key_length ||
        memcmp(*cursor, key, key_length) != 0 || separator + 1 == line_end)
        return 0;
    *separator = '\0';
    *line_end = '\0';
    *value = separator + 1;
    *cursor = line_end + 1;
    return 1;
}

/* Purpose: decode every field in one schema table from canonical ordered text lines.
 * Inputs: bounded mutable parse cursor, record, and the same field table used for writing.
 * Effects: advances exactly one line per field and initializes only declared destinations.
 * Failure: rejects reordering, text overflow, or noncanonical sixteen-digit integers.
 * Boundary: semantic field validation follows complete parsing. */
static int fields_parse(char **cursor, char *end,
                        yvex_runtime_benchmark_baseline *record,
                        const benchmark_field *fields, size_t count)
{
    unsigned char *base = (unsigned char *)record;
    size_t index;

    for (index = 0u; index < count; ++index) {
        const benchmark_field *field = &fields[index];
        char *value, *tail;
        unsigned long long parsed;
        if (!parse_line(cursor, end, field->name, &value)) return 0;
        if (field->kind == BENCHMARK_FIELD_BOOL) {
            int boolean;
            if ((value[0] != '0' && value[0] != '1') || value[1]) return 0;
            boolean = value[0] == '1';
            memcpy(base + field->offset, &boolean, sizeof(boolean));
            continue;
        }
        if (field->kind != BENCHMARK_FIELD_U64) {
            if (strlen(value) >= field->width) return 0;
            yvex_core_text_copy((char *)(base + field->offset), field->width, value);
            continue;
        }
        if (strlen(value) != 16u) return 0;
        errno = 0;
        parsed = strtoull(value, &tail, 16);
        if (errno || tail != value + 16u || *tail) return 0;
        memcpy(base + field->offset, &parsed, sizeof(parsed));
    }
    return 1;
}

/* Purpose: parse every canonical baseline field from one mutable bounded file buffer.
 * Inputs: mutable exact file bytes and caller-owned record output.
 * Effects: tokenizes bytes in place and fills every version-four field once.
 * Failure: returns false on missing, reordered, duplicate, or trailing data.
 * Boundary: identity and canonical byte equality are validated by the reopen owner. */
static int baseline_parse(char *data, size_t count, yvex_runtime_benchmark_baseline *record)
{
    char *cursor = data, *end = data + count, *header, *identity;

    memset(record, 0, sizeof(*record));
    if (!parse_line(&cursor, end, "YVEX_RUNTIME_BENCHMARK_BASELINE", &header))
        return 0;
    if (header[0] < '1' || header[0] > '4' || header[1]) return 0;
    record->schema_version = (unsigned int)(header[0] - '0');
    if (record->schema_version != YVEX_RUNTIME_BENCHMARK_SCHEMA_V4) return 0;
    if (!parse_line(&cursor, end, "identity", &identity) ||
        strlen(identity) >= sizeof(record->identity) ||
        !fields_parse(&cursor, end, record, benchmark_key_fields,
                      sizeof(benchmark_key_fields) / sizeof(benchmark_key_fields[0])) ||
        !fields_parse(&cursor, end, record, benchmark_metric_fields,
                      sizeof(benchmark_metric_fields) / sizeof(benchmark_metric_fields[0])) ||
        cursor != end)
        return 0;
    record->metrics.host_timing.available = 1;
    yvex_core_text_copy(record->identity, sizeof(record->identity), identity);
    return 1;
}

/* Purpose: read one stable regular file through a no-symlink path and exact bounded snapshot.
 * Inputs: safe path, owned buffer outputs, and typed refusal outputs.
 * Effects: allocates and fills one exact caller-owned byte buffer.
 * Failure: rejects nonregular, oversized, short, changed, or unreadable files.
 * Boundary: parsing and identity validation remain caller-owned. */
static int read_file(const char *path, char **data, size_t *count,
                     yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    yvex_core_file_result file_result;
    unsigned char *bytes = NULL;
    int rc;

    if (!path || path[0] != '/')
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_PATH,
                                "path", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "benchmark evidence requires an absolute external path", err);
    memset(&file_result, 0, sizeof(file_result));
    rc = yvex_core_file_read_snapshot(path, BENCHMARK_FILE_MAX, &bytes, count,
                                      &file_result, err);
    if (rc != YVEX_OK)
        return benchmark_reject(failure, benchmark_file_code(file_result.stage),
                                "file-lifecycle", file_result.expected, file_result.actual, (yvex_status)rc,
                                "benchmark file snapshot failed", err);
    *data = (char *)bytes;
    return YVEX_OK;
}

/* Purpose: compare workload compatibility while retaining commit and source state provenance.
 * Inputs: two independently validated benchmark records.
 * Effects: reads canonical key fields without mutating either record.
 * Failure: returns false at the first workload, device, or toolchain mismatch.
 * Boundary: commits may differ across revisions; exact source delta and build identity must match. */
static int keys_equal(const yvex_runtime_benchmark_baseline *left,
                      const yvex_runtime_benchmark_baseline *right)
{
    const unsigned char *left_base = (const unsigned char *)left;
    const unsigned char *right_base = (const unsigned char *)right;
    size_t index;

    for (index = 0u; index < sizeof(benchmark_key_fields) /
                                  sizeof(benchmark_key_fields[0]); ++index) {
        const benchmark_field *field = &benchmark_key_fields[index];
        const void *left_value = left_base + field->offset;
        const void *right_value = right_base + field->offset;
        if (field->kind == BENCHMARK_FIELD_COMMIT) continue;
        if (field->kind == BENCHMARK_FIELD_U64) {
            if (memcmp(left_value, right_value, sizeof(unsigned long long)) != 0) return 0;
        } else if (strcmp((const char *)left_value, (const char *)right_value) != 0) {
            return 0;
        }
    }
    return 1;
}

/* Purpose: produce one signed measured delta after metrics validation bounded both operands. */
static long long metric_delta(unsigned long long current, unsigned long long baseline)
{
    return (long long)current - (long long)baseline;
}

/* Purpose: escape one bounded identity/device label for deterministic SVG text. */
static int svg_text(benchmark_bytes *bytes, const char *text)
{
    const char *cursor, *start;
    for (cursor = start = text; *cursor; ++cursor) {
        const char *replacement = NULL;
        if (*cursor == '&') replacement = "&amp;";
        else if (*cursor == '<') replacement = "&lt;";
        else if (*cursor == '>') replacement = "&gt;";
        else if (*cursor == '\"') replacement = "&quot;";
        else if (*cursor == '\'') replacement = "&apos;";
        if (replacement) {
            if (!yvex_core_bytes_append(bytes, start, (size_t)(cursor - start)) ||
                !bytes_text(bytes, replacement))
                return 0;
            start = cursor + 1;
        }
    }
    return yvex_core_bytes_append(bytes, start, (size_t)(cursor - start));
}

/* Purpose: scale one bar with integer half-up rounding that is identical across ABIs. */
static unsigned int chart_height(unsigned long long value, unsigned long long maximum,
                                 unsigned int extent)
{
    unsigned long long scale, quotient, remainder;
    unsigned int height;

    if (!value || !maximum || !extent) return 0u;
    if (value >= maximum) return extent;
    scale = (unsigned long long)extent * 2ull;
    quotient = maximum / scale;
    remainder = maximum % scale;
    for (height = 0u; height < extent; ++height) {
        unsigned long long factor = (unsigned long long)height * 2ull + 1ull;
        unsigned long long tail = remainder * factor;
        unsigned long long threshold = quotient * factor + tail / scale + (tail % scale != 0ull);
        if (value < threshold) break;
    }
    return height ? height : 1u;
}

/* Purpose: hash canonical chart inputs independently from formatting or destination path. */
static int chart_bytes_identity(const benchmark_bytes *svg,
                                char output[YVEX_SHA256_HEX_BYTES])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!svg || !yvex_sha256_update(&hash, svg->data, svg->count) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

/* Purpose: render five warm-latency percentiles for current and optional baseline evidence.
 * Inputs: bounded SVG buffer plus sealed current and optional baseline records.
 * Effects: appends deterministic percentile bars and legend.
 * Failure: returns false on serialization bound or allocation failure.
 * Boundary: the panel visualizes raw values without a regression threshold. */
static int chart_warm(benchmark_bytes *svg, const yvex_runtime_benchmark_baseline *current,
                      const yvex_runtime_benchmark_baseline *baseline)
{
    typedef struct {
        const yvex_runtime_benchmark_timing_distribution *timing;
        const char *style;
        unsigned int offset, width;
    } chart_series;
    const chart_series series[] = {
        {&current->metrics.host_timing, "current", 0u, 30u},
        {&current->metrics.device_timing, "device", 10u, 10u},
        {baseline ? &baseline->metrics.host_timing : NULL, "baseline", 32u, 30u},
        {baseline ? &baseline->metrics.device_timing : NULL, "device_baseline", 42u, 10u},
    };
    const char *const labels[] = {"min", "p50", "p90", "p99", "max"};
    unsigned long long maximum = 0ull;
    size_t index, series_index;

    for (series_index = 0u; series_index < sizeof(series) / sizeof(series[0]); ++series_index)
        if (series[series_index].timing && series[series_index].timing->available &&
            series[series_index].timing->values[YVEX_RUNTIME_BENCHMARK_MAXIMUM] > maximum)
            maximum = series[series_index].timing->values[YVEX_RUNTIME_BENCHMARK_MAXIMUM];
    if (!bytes_text(svg,
                    "<text x=\"410\" y=\"142\" class=\"section\">WARM HOST / DEVICE</text>"
                    "<line x1=\"410\" y1=\"360\" x2=\"900\" y2=\"360\" class=\"axis\"/>"))
        return 0;
    for (index = 0u; index < 5u; ++index) {
        unsigned int x = 436u + (unsigned int)index * 94u;
        if (!bytes_format(svg, "<text x=\"%u\" y=\"382\" class=\"label\">%s</text>",
                          x + 27u, labels[index]))
            return 0;
        for (series_index = 0u; series_index < sizeof(series) / sizeof(series[0]); ++series_index) {
            const chart_series *item = &series[series_index];
            unsigned int height;
            if (!item->timing || !item->timing->available) continue;
            height = chart_height(item->timing->values[index], maximum, 190u);
            if (!bytes_format(svg,
                              "<rect x=\"%u\" y=\"%u\" width=\"%u\" height=\"%u\" class=\"%s\"/>",
                              x + item->offset, 360u - height, item->width, height, item->style))
                return 0;
        }
    }
    if (!bytes_text(svg,
                    "<circle cx=\"650\" cy=\"411\" r=\"6\" class=\"current\"/>"
                    "<text x=\"663\" y=\"416\" class=\"meta\">current</text>"))
        return 0;
    if (baseline &&
        !bytes_text(svg,
                    "<circle cx=\"750\" cy=\"411\" r=\"6\" class=\"baseline\"/>"
                    "<text x=\"763\" y=\"416\" class=\"meta\">baseline</text>"))
        return 0;
    if (!current->metrics.device_timing.available)
        return bytes_text(svg,
                          "<text x=\"650\" y=\"438\" class=\"meta\">DEVICE TIMING UNAVAILABLE</text>");
    return bytes_text(svg,
                      "<circle cx=\"850\" cy=\"411\" r=\"6\" class=\"device\"/>"
                      "<text x=\"863\" y=\"416\" class=\"meta\">device event</text>");
}

/* Purpose: render one exact resource counter as a current/baseline horizontal comparison. */
static int chart_resource_row(benchmark_bytes *svg, const char *label, unsigned int y,
                              unsigned long long current, unsigned long long baseline,
                              int has_baseline)
{
    unsigned long long maximum = current > baseline ? current : baseline;
    unsigned int current_width = chart_height(current, maximum, 220u);
    unsigned int baseline_width = chart_height(baseline, maximum, 220u);

    if (!bytes_format(svg,
                      "<text x=\"64\" y=\"%u\" class=\"meta\">%s</text>"
                      "<rect x=\"180\" y=\"%u\" width=\"%u\" height=\"7\" class=\"current\"/>"
                      "<text x=\"414\" y=\"%u\" class=\"metric\">%llu B</text>",
                      y + 7u, label, y, current_width, y + 7u, current))
        return 0;
    return !has_baseline ||
           bytes_format(svg,
                        "<rect x=\"180\" y=\"%u\" width=\"%u\" height=\"7\" class=\"baseline\"/>"
                        "<text x=\"414\" y=\"%u\" class=\"baseline_metric\">%llu B</text>",
                        y + 9u, baseline_width, y + 16u, baseline);
}

/* Purpose: visualize resource and launch evidence retained by benchmark schema four.
 * Inputs: sealed current record and optional compatible baseline.
 * Effects: appends exact byte counters, launch topology, and warm-allocation invariants.
 * Failure: returns false on bounded SVG serialization failure.
 * Boundary: counters are measured runtime facts; the chart assigns no performance grade. */
static int chart_resources(benchmark_bytes *svg,
                           const yvex_runtime_benchmark_baseline *current,
                           const yvex_runtime_benchmark_baseline *baseline)
{
    const yvex_runtime_benchmark_metrics *metrics = &current->metrics;
    const yvex_runtime_benchmark_metrics *prior = baseline ? &baseline->metrics : NULL;

    if (!bytes_text(svg,
                    "<line x1=\"64\" y1=\"448\" x2=\"900\" y2=\"448\" class=\"axis\"/>"
                    "<text x=\"64\" y=\"477\" class=\"section\">STRUCTURAL EVIDENCE</text>"))
        return 0;
    if (!chart_resource_row(svg, "resident", 492u, metrics->resident_bytes,
                            prior ? prior->resident_bytes : 0ull, prior != NULL) ||
        !chart_resource_row(svg, "host workspace", 518u, metrics->host_workspace_bytes,
                            prior ? prior->host_workspace_bytes : 0ull, prior != NULL) ||
        !chart_resource_row(svg, "encoded weight pack", 544u,
                            metrics->resident_encoded_bytes,
                            prior ? prior->resident_encoded_bytes : 0ull,
                            prior != NULL) ||
        !chart_resource_row(svg, "resident H2D", 570u, metrics->resident_h2d_bytes,
                            prior ? prior->resident_h2d_bytes : 0ull, prior != NULL) ||
        !chart_resource_row(svg, "attention state", 596u, metrics->state_bytes,
                            prior ? prior->state_bytes : 0ull, prior != NULL))
        return 0;
    return bytes_format(
        svg,
        "<text x=\"600\" y=\"505\" class=\"meta\">last dispatch kernels %llu - graph launches %llu</text>"
        "<text x=\"600\" y=\"528\" class=\"meta\">captures %llu - replays %llu - nodes %llu</text>"
        "<text x=\"600\" y=\"551\" class=\"meta\">warm alloc host/device %llu/%llu</text>"
        "<text x=\"600\" y=\"574\" class=\"meta\">warm reads %llu - uploads %llu B</text>"
        "<text x=\"600\" y=\"600\" class=\"meta\">state %llu B</text>",
        metrics->last_dispatch_kernel_launches, metrics->cuda_graph_launches,
        metrics->cuda_graph_captures, metrics->cuda_graph_replays,
        metrics->cuda_graph_nodes, metrics->warm_host_allocations,
        metrics->warm_device_allocations, metrics->warm_weight_reads,
        metrics->warm_upload_bytes, metrics->state_bytes);
}

/* Purpose: assemble deterministic SVG bytes bound to benchmark and optional baseline identities.
 * Inputs: sealed current record, optional compatible baseline, and empty SVG buffer.
 * Effects: appends one complete dependency-free SVG document.
 * Failure: returns false on escaping, formatting, bound, or allocation failure.
 * Boundary: exact-byte identity is computed only after the document is complete. */
static int chart_serialize(const yvex_runtime_benchmark_baseline *current,
                           const yvex_runtime_benchmark_baseline *baseline,
                           benchmark_bytes *svg)
{
    svg->maximum = BENCHMARK_CHART_MAX;
    svg->initial_capacity = 1024u;
    if (!bytes_format(svg,
                      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"960\" height=\"760\" viewBox=\"0 0 960 760\""
                      " data-chart-schema=\"4\">"
                      "<style>text{font-family:ui-monospace,SFMono-Regular,monospace;fill:#12212b}"
                      ".title{font-size:24px;font-weight:700}"
                      ".section{font-size:14px;font-weight:700;letter-spacing:1px}"
                      ".meta{font-size:11px;fill:#52636d}.label{font-size:11px;text-anchor:middle}"
                      ".value{font-size:10px;text-anchor:middle}.metric{font-size:10px;fill:#007f73}"
                      ".baseline_metric{font-size:10px;fill:#a86b12}.axis{stroke:#b8c4ca;stroke-width:1}"
                      ".current{fill:#007f73}.baseline{fill:#e9a23b}"
                      ".device{fill:#2e5aac}.device_baseline{fill:#7b65aa}</style>"
                      "<rect width=\"960\" height=\"760\" fill=\"#f4f1e8\"/>"
                      "<text x=\"64\" y=\"62\" class=\"title\">YVEX ATTENTION RUNTIME</text>"
                      "<text x=\"64\" y=\"89\" class=\"meta\">") ||
        !svg_text(svg, current->key.device) ||
        !bytes_text(svg, " - ") || !svg_text(svg, current->key.phase) ||
        !bytes_text(svg, " - ") || !svg_text(svg, current->key.mode) ||
        !bytes_text(svg, " - ") || !svg_text(svg, current->key.scope) ||
        !bytes_format(svg,
                      "</text><text x=\"64\" y=\"142\" class=\"section\">COLD PREPARATION</text>"
                      "<text x=\"64\" y=\"180\" class=\"meta\">total %llu ns</text>"
                      "<text x=\"64\" y=\"205\" class=\"meta\">artifact %llu ns</text>"
                      "<text x=\"64\" y=\"230\" class=\"meta\">residency %llu ns</text>"
                      "<text x=\"64\" y=\"255\" class=\"meta\">graph %llu ns</text>",
                      current->metrics.cold_total_ns, current->metrics.cold_artifact_ns,
                      current->metrics.cold_residency_ns, current->metrics.cold_graph_ns) ||
        !chart_warm(svg, current, baseline) || !chart_resources(svg, current, baseline) ||
        !bytes_format(svg,
                      "<line x1=\"64\" y1=\"626\" x2=\"900\" y2=\"626\" class=\"axis\"/>"
                      "<text x=\"64\" y=\"650\" class=\"meta\">current %s</text>"
                      "<text x=\"64\" y=\"670\" class=\"meta\">build %s - source %s</text>"
                      "<text x=\"64\" y=\"700\" class=\"meta\">baseline %s</text>"
                      "<text x=\"64\" y=\"720\" class=\"meta\">build %s - source %s</text>"
                      "</svg>\n",
                      current->identity, current->key.commit, current->key.build_source_state,
                      baseline ? baseline->identity : "none",
                      baseline ? baseline->key.commit : "none",
                      baseline ? baseline->key.build_source_state : "none"))
        return 0;
    return 1;
}

/* Purpose: seal one mutable benchmark record under its canonical content identity.
 * Inputs: complete key, measured nanosecond statistics, and schema version.
 * Effects: writes only the record identity after every field validates.
 * Failure: leaves the prior identity empty and reports the exact invalid field.
 * Boundary: sealing records evidence; it does not admit benchmark capability. */
int yvex_runtime_benchmark_baseline_seal(yvex_runtime_benchmark_baseline *record,
                                         yvex_runtime_benchmark_failure *failure,
                                         yvex_error *err)
{
    int rc;
    if (!record)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
                                "record", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "benchmark record is required", err);
    record->identity[0] = '\0';
    if (record->schema_version != YVEX_RUNTIME_BENCHMARK_SCHEMA_V4)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_SCHEMA, "schema",
                                YVEX_RUNTIME_BENCHMARK_SCHEMA_V4, record->schema_version,
                                YVEX_ERR_UNSUPPORTED, "benchmark schema is unsupported", err);
    if ((rc = key_validate(record, failure, err)) != YVEX_OK ||
        (rc = metrics_validate(record, failure, err)) != YVEX_OK)
        return rc;
    if (!baseline_identity(record, record->identity))
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_IDENTITY,
                                "identity", 1ull, 0ull, YVEX_ERR_STATE,
                                "benchmark identity construction failed", err);
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: construct one sealed benchmark record from completed runtime attention evidence.
 * Inputs: immutable production result with measured samples and exact identities.
 * Effects: fills and seals only caller-owned benchmark evidence.
 * Failure: missing, oversized, non-finite, or inconsistent facts refuse publication.
 * Boundary: runtime owns evidence projection; CLI owns only operator paths and rendering. */
int yvex_runtime_benchmark_baseline_from_attention(
    const yvex_graph_attention_operator_result *result,
    yvex_runtime_benchmark_baseline *record,
    yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    int rc;

    if (!record || !result || !result->completed || !result->benchmark_sample_count ||
        !yvex_sha256_hex_valid(result->execution_descriptor_identity))
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
                                "attention-result", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "completed identity-bound benchmark samples are required", err);
    memset(record, 0, sizeof(*record));
    record->schema_version = YVEX_RUNTIME_BENCHMARK_SCHEMA_V4;
    rc = benchmark_attention_key(result, &record->key, err);
    if (rc == YVEX_OK) rc = benchmark_attention_times(result, &record->metrics, err);
    if (rc == YVEX_OK) rc = benchmark_attention_metrics(result, &record->metrics, err);
    return rc == YVEX_OK ? yvex_runtime_benchmark_baseline_seal(record, failure, err) : rc;
}

/* Purpose: transactionally publish one sealed benchmark baseline without replacement.
 * Inputs: safe external path and immutable sealed record.
 * Effects: creates, fsyncs, and atomically publishes one exact canonical file.
 * Failure: preserves a pre-existing destination and removes only the owned temporary.
 * Boundary: publication stores evidence but does not decide regression or readiness. */
int yvex_runtime_benchmark_baseline_write(
    const char *path, const yvex_runtime_benchmark_baseline *record,
    yvex_runtime_benchmark_publication *result,
    yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    benchmark_bytes bytes = {0};
    int rc = baseline_validate(record, failure, err);
    if (rc != YVEX_OK) return rc;
    if (!baseline_serialize(record, &bytes)) {
        free(bytes.data);
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS,
                                "serialization", BENCHMARK_FILE_MAX, bytes.count, YVEX_ERR_BOUNDS,
                                "benchmark serialization exceeded its bound", err);
    }
    rc = publish_bytes(path, bytes.data, bytes.count, NULL, result, failure, err);
    if (rc == YVEX_OK)
        yvex_core_text_copy(result->identity, sizeof(result->identity), record->identity);
    free(bytes.data);
    return rc;
}

/* Purpose: independently reopen and authenticate one canonical benchmark baseline.
 * Inputs: safe exact external file path and caller-owned result storage.
 * Effects: fills the result only after stable read, strict parse, identity, and byte-canonical checks.
 * Failure: returns typed format, drift, or identity refusal without changing external state.
 * Boundary: reopening validates evidence; it does not execute or compare runtime work. */
int yvex_runtime_benchmark_baseline_open(const char *path,
                                         yvex_runtime_benchmark_baseline *record,
                                         yvex_runtime_benchmark_failure *failure,
                                         yvex_error *err)
{
    yvex_runtime_benchmark_baseline parsed;
    benchmark_bytes canonical = {0};
    char *data = NULL, *parse_copy = NULL;
    size_t count = 0u;
    int rc;

    if (record) memset(record, 0, sizeof(*record));
    if (!record)
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
                                "record", 1ull, 0ull, YVEX_ERR_INVALID_ARG,
                                "benchmark reopen result is required", err);
    rc = read_file(path, &data, &count, failure, err);
    if (rc != YVEX_OK) return rc;
    parse_copy = (char *)malloc(count + 1u);
    if (!parse_copy) {
        rc = benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_READ, "allocation",
                              count, 0ull, YVEX_ERR_NOMEM,
                              "benchmark parse buffer allocation failed", err);
        goto done;
    }
    memcpy(parse_copy, data, count + 1u);
    if (!baseline_parse(parse_copy, count, &parsed)) {
        if (parsed.schema_version == YVEX_RUNTIME_BENCHMARK_SCHEMA_V1 ||
            parsed.schema_version == YVEX_RUNTIME_BENCHMARK_SCHEMA_V2 ||
            parsed.schema_version == YVEX_RUNTIME_BENCHMARK_SCHEMA_V3)
            rc = benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_SCHEMA, "schema",
                                  YVEX_RUNTIME_BENCHMARK_SCHEMA_V4,
                                  parsed.schema_version, YVEX_ERR_UNSUPPORTED,
                                  "legacy benchmark schema requires provenance-bound regeneration", err);
        else
            rc = benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_FORMAT,
                                  "format", 1ull, 0ull, YVEX_ERR_FORMAT,
                                  "benchmark file is malformed", err);
        goto done;
    }
    if ((rc = baseline_validate(&parsed, failure, err)) != YVEX_OK) goto done;
    if (!baseline_serialize(&parsed, &canonical) || canonical.count != count ||
        memcmp(canonical.data, data, count) != 0) {
        rc = benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_FORMAT,
                              "canonical-bytes", count, canonical.count, YVEX_ERR_FORMAT,
                              "benchmark file bytes are not canonical", err);
        goto done;
    }
    *record = parsed;
    rc = YVEX_OK;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);

done:
    free(canonical.data);
    free(parse_copy);
    free(data);
    return rc;
}

/* Purpose: compare two identity-compatible measurements without inventing a regression threshold.
 * Inputs: independently sealed current and baseline records.
 * Effects: fills signed measured nanosecond deltas only after exact key compatibility.
 * Failure: refuses an identity mismatch rather than comparing unrelated runs.
 * Boundary: comparison reports deltas; benchmark policy remains unowned and no pass claim is produced. */
int yvex_runtime_benchmark_compare(
    const yvex_runtime_benchmark_baseline *current,
    const yvex_runtime_benchmark_baseline *baseline,
    yvex_runtime_benchmark_comparison *result,
    yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!result || (rc = baseline_validate(current, failure, err)) != YVEX_OK ||
        (rc = baseline_validate(baseline, failure, err)) != YVEX_OK)
        return result ? rc : benchmark_reject(
            failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT, "result", 1ull, 0ull,
            YVEX_ERR_INVALID_ARG, "benchmark comparison result is required", err);
    if (!keys_equal(current, baseline))
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INCOMPATIBLE,
                                "compatibility-key", 1ull, 0ull, YVEX_ERR_STATE,
                                "benchmark baseline identity key does not match this execution", err);
    result->compatible = 1;
    yvex_core_text_copy(result->current_commit, sizeof(result->current_commit), current->key.commit);
    yvex_core_text_copy(result->baseline_commit, sizeof(result->baseline_commit), baseline->key.commit);
    yvex_core_text_copy(result->current_source_state,
                        sizeof(result->current_source_state),
                        current->key.build_source_state);
    yvex_core_text_copy(result->baseline_source_state,
                        sizeof(result->baseline_source_state),
                        baseline->key.build_source_state);
    yvex_core_text_copy(result->current_identity, sizeof(result->current_identity), current->identity);
    yvex_core_text_copy(result->baseline_identity, sizeof(result->baseline_identity), baseline->identity);
#define TIMING_DELTA(DIST, STAT)                                                                \
    metric_delta(current->metrics.DIST.values[STAT], baseline->metrics.DIST.values[STAT])
    result->cold_total_delta_ns =
        metric_delta(current->metrics.cold_total_ns, baseline->metrics.cold_total_ns);
    result->minimum_delta_ns = TIMING_DELTA(host_timing, YVEX_RUNTIME_BENCHMARK_MINIMUM);
    result->p50_delta_ns = TIMING_DELTA(host_timing, YVEX_RUNTIME_BENCHMARK_P50);
    result->p90_delta_ns = TIMING_DELTA(host_timing, YVEX_RUNTIME_BENCHMARK_P90);
    result->p99_delta_ns = TIMING_DELTA(host_timing, YVEX_RUNTIME_BENCHMARK_P99);
    result->maximum_delta_ns = TIMING_DELTA(host_timing, YVEX_RUNTIME_BENCHMARK_MAXIMUM);
    result->mean_delta_ns = TIMING_DELTA(host_timing, YVEX_RUNTIME_BENCHMARK_MEAN);
    result->device_timing_available = current->metrics.device_timing.available;
    result->device_minimum_delta_ns =
        TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_MINIMUM);
    result->device_p50_delta_ns = TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_P50);
    result->device_p90_delta_ns = TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_P90);
    result->device_p99_delta_ns = TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_P99);
    result->device_maximum_delta_ns =
        TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_MAXIMUM);
    result->device_mean_delta_ns = TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_MEAN);
    result->device_standard_deviation_delta_ns =
        TIMING_DELTA(device_timing, YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION);
#undef TIMING_DELTA
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: publish one deterministic dependency-free SVG benchmark chart.
 * Inputs: sealed current evidence, optional compatible baseline, and safe external SVG path.
 * Effects: creates one identity-bound SVG through the canonical no-replace file lifecycle.
 * Failure: preserves every previously published baseline and chart destination.
 * Boundary: baseline publication is a separate authoritative operation owned by its writer. */
int yvex_runtime_benchmark_chart_write(
    const yvex_runtime_benchmark_chart_request *request,
    yvex_runtime_benchmark_chart_result *result,
    yvex_runtime_benchmark_failure *failure, yvex_error *err)
{
    yvex_runtime_benchmark_publication publication;
    yvex_runtime_benchmark_comparison comparison;
    benchmark_bytes svg = {0};
    char identity[YVEX_SHA256_HEX_BYTES];
    int rc = YVEX_OK;

    if (result) memset(result, 0, sizeof(*result));
    if (!request || !result || !request->path ||
        (rc = baseline_validate(request->current, failure, err)) != YVEX_OK)
        return request && result ? rc : benchmark_reject(
            failure, YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT, "chart", 1ull, 0ull,
            YVEX_ERR_INVALID_ARG, "benchmark chart request is incomplete", err);
    if (request->baseline &&
        (rc = yvex_runtime_benchmark_compare(request->current, request->baseline,
                                             &comparison, failure, err)) != YVEX_OK)
        return rc;
    if (!chart_serialize(request->current, request->baseline, &svg) ||
        !chart_bytes_identity(&svg, identity)) {
        free(svg.data);
        return benchmark_reject(failure, YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS,
                                "chart-bytes", BENCHMARK_CHART_MAX, svg.count, YVEX_ERR_BOUNDS,
                                "benchmark SVG exceeded its bound", err);
    }
    rc = publish_bytes(request->path, svg.data, svg.count, request->file_faults,
                       &publication, failure, err);
    if (rc == YVEX_OK) {
        result->generated = 1;
        result->file_bytes = publication.file_bytes;
        yvex_core_text_copy(result->path, sizeof(result->path), publication.path);
        yvex_core_text_copy(result->identity, sizeof(result->identity), identity);
    }
    free(svg.data);
    return rc;
}
