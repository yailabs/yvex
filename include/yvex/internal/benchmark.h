/* Owner: runtime.benchmark.
 * Owns: immutable attention-benchmark baselines, compatibility comparison, and deterministic SVG evidence.
 * Does not own: benchmark measurement, execution admission, runtime capability, CLI parsing, or rendering policy.
 * Invariants: records are versioned, content-addressed, bounded, and atomically published without replacement.
 * Boundary: external benchmark evidence is serialized only after production execution has produced typed facts.
 * Purpose: expose one independently reopenable baseline and chart lifecycle to runtime and operator consumers.
 * Inputs: canonical execution identities, device/build facts, counters, and measured nanosecond statistics.
 * Effects: explicit publication calls create one external file; validation and comparison are read-only.
 * Failure: typed refusals preserve every pre-existing destination and remove only owner-created temporaries. */
#ifndef INCLUDE_YVEX_INTERNAL_BENCHMARK_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_BENCHMARK_H_INCLUDED

#include <yvex/core.h>
#include <yvex/internal/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_RUNTIME_BENCHMARK_SCHEMA_V1 1u
#define YVEX_RUNTIME_BENCHMARK_SCHEMA_V2 2u
#define YVEX_RUNTIME_BENCHMARK_SCHEMA_V3 3u
#define YVEX_RUNTIME_BENCHMARK_SCHEMA_V4 4u
#define YVEX_RUNTIME_BENCHMARK_COMMIT_CAP 41u
#define YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP 8u
#define YVEX_RUNTIME_BENCHMARK_TEXT_CAP 128u
#define YVEX_RUNTIME_BENCHMARK_MODE_CAP 16u
#define YVEX_RUNTIME_BENCHMARK_SCOPE_CAP 32u
#define YVEX_RUNTIME_BENCHMARK_BUCKET_CAP 64u

typedef enum {
    YVEX_RUNTIME_BENCHMARK_FAILURE_NONE = 0,
    YVEX_RUNTIME_BENCHMARK_FAILURE_INVALID_ARGUMENT,
    YVEX_RUNTIME_BENCHMARK_FAILURE_SCHEMA,
    YVEX_RUNTIME_BENCHMARK_FAILURE_FIELD,
    YVEX_RUNTIME_BENCHMARK_FAILURE_IDENTITY,
    YVEX_RUNTIME_BENCHMARK_FAILURE_PATH,
    YVEX_RUNTIME_BENCHMARK_FAILURE_CREATE,
    YVEX_RUNTIME_BENCHMARK_FAILURE_CONFLICT,
    YVEX_RUNTIME_BENCHMARK_FAILURE_WRITE,
    YVEX_RUNTIME_BENCHMARK_FAILURE_SYNC,
    YVEX_RUNTIME_BENCHMARK_FAILURE_PUBLISH,
    YVEX_RUNTIME_BENCHMARK_FAILURE_OPEN,
    YVEX_RUNTIME_BENCHMARK_FAILURE_READ,
    YVEX_RUNTIME_BENCHMARK_FAILURE_FORMAT,
    YVEX_RUNTIME_BENCHMARK_FAILURE_BOUNDS,
    YVEX_RUNTIME_BENCHMARK_FAILURE_INCOMPATIBLE,
    YVEX_RUNTIME_BENCHMARK_FAILURE_CLEANUP
} yvex_runtime_benchmark_failure_code;

typedef struct {
    yvex_runtime_benchmark_failure_code code;
    char field[64];
    unsigned long long expected, actual;
    const char *reason;
} yvex_runtime_benchmark_failure;

typedef struct {
    char commit[YVEX_RUNTIME_BENCHMARK_COMMIT_CAP];
    char build_source_state[YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP];
    char source_delta_identity[YVEX_SHA256_HEX_BYTES];
    char build_identity[YVEX_SHA256_HEX_BYTES];
    char artifact_identity[YVEX_SHA256_HEX_BYTES];
    char runtime_binding_identity[YVEX_SHA256_HEX_BYTES];
    char runtime_descriptor_identity[YVEX_SHA256_HEX_BYTES];
    char execution_descriptor_identity[YVEX_SHA256_HEX_BYTES];
    char device[YVEX_RUNTIME_BENCHMARK_TEXT_CAP];
    char driver[YVEX_RUNTIME_BENCHMARK_TEXT_CAP];
    char cuda_build[YVEX_RUNTIME_BENCHMARK_TEXT_CAP];
    char mode[YVEX_RUNTIME_BENCHMARK_MODE_CAP];
    char phase[YVEX_RUNTIME_BENCHMARK_MODE_CAP];
    char scope[YVEX_RUNTIME_BENCHMARK_SCOPE_CAP];
    char capture_bucket[YVEX_RUNTIME_BENCHMARK_BUCKET_CAP];
    unsigned long long warmup_count, iteration_count;
} yvex_runtime_benchmark_key;

typedef enum {
    YVEX_RUNTIME_BENCHMARK_MINIMUM = 0,
    YVEX_RUNTIME_BENCHMARK_P50,
    YVEX_RUNTIME_BENCHMARK_P90,
    YVEX_RUNTIME_BENCHMARK_P99,
    YVEX_RUNTIME_BENCHMARK_MAXIMUM,
    YVEX_RUNTIME_BENCHMARK_MEAN,
    YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION,
    YVEX_RUNTIME_BENCHMARK_STATISTIC_COUNT
} yvex_runtime_benchmark_statistic;

typedef struct {
    int available;
    unsigned long long values[YVEX_RUNTIME_BENCHMARK_STATISTIC_COUNT];
} yvex_runtime_benchmark_timing_distribution;

typedef struct {
    unsigned long long cold_total_ns;
    unsigned long long cold_artifact_ns;
    unsigned long long cold_residency_ns;
    unsigned long long cold_graph_ns;
    yvex_runtime_benchmark_timing_distribution host_timing;
    yvex_runtime_benchmark_timing_distribution device_timing;
    unsigned long long artifact_bytes_hashed, resident_encoded_bytes;
    unsigned long long resident_h2d_bytes;
    unsigned long long warm_weight_reads, warm_upload_bytes;
    unsigned long long warm_host_allocations, warm_device_allocations, warm_device_frees;
    unsigned long long last_dispatch_kernel_launches, cuda_graph_launches, cuda_graph_captures;
    unsigned long long cuda_graph_replays, cuda_graph_nodes;
    unsigned long long peak_host_bytes, last_dispatch_peak_device_bytes, resident_bytes;
    unsigned long long host_workspace_bytes, state_bytes;
} yvex_runtime_benchmark_metrics;

typedef struct {
    unsigned int schema_version;
    yvex_runtime_benchmark_key key;
    yvex_runtime_benchmark_metrics metrics;
    char identity[YVEX_SHA256_HEX_BYTES];
} yvex_runtime_benchmark_baseline;

typedef struct {
    int published;
    unsigned long long file_bytes;
    char path[YVEX_PATH_CAP];
    char identity[YVEX_SHA256_HEX_BYTES];
} yvex_runtime_benchmark_publication;

typedef struct {
    int compatible;
    char current_commit[YVEX_RUNTIME_BENCHMARK_COMMIT_CAP];
    char baseline_commit[YVEX_RUNTIME_BENCHMARK_COMMIT_CAP];
    char current_source_state[YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP];
    char baseline_source_state[YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP];
    char current_identity[YVEX_SHA256_HEX_BYTES];
    char baseline_identity[YVEX_SHA256_HEX_BYTES];
    long long cold_total_delta_ns;
    long long minimum_delta_ns, p50_delta_ns, p90_delta_ns, p99_delta_ns;
    long long maximum_delta_ns, mean_delta_ns;
    int device_timing_available;
    long long device_minimum_delta_ns, device_p50_delta_ns;
    long long device_p90_delta_ns, device_p99_delta_ns;
    long long device_maximum_delta_ns, device_mean_delta_ns;
    long long device_standard_deviation_delta_ns;
} yvex_runtime_benchmark_comparison;

typedef struct {
    const char *path;
    const yvex_runtime_benchmark_baseline *current;
    const yvex_runtime_benchmark_baseline *baseline;
    const yvex_core_file_faults *file_faults;
} yvex_runtime_benchmark_chart_request;

typedef struct {
    int generated;
    unsigned long long file_bytes;
    char path[YVEX_PATH_CAP];
    char identity[YVEX_SHA256_HEX_BYTES];
} yvex_runtime_benchmark_chart_result;

typedef struct {
    char identity[YVEX_SHA256_HEX_BYTES], baseline_identity[YVEX_SHA256_HEX_BYTES];
    char current_commit[YVEX_RUNTIME_BENCHMARK_COMMIT_CAP];
    char baseline_commit[YVEX_RUNTIME_BENCHMARK_COMMIT_CAP];
    char current_source_state[YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP];
    char baseline_source_state[YVEX_RUNTIME_BENCHMARK_SOURCE_STATE_CAP];
    char chart_identity[YVEX_SHA256_HEX_BYTES];
    char path[YVEX_PATH_CAP], chart_path[YVEX_PATH_CAP];
    unsigned long long file_bytes, chart_file_bytes;
    double cold_delta_seconds, minimum_delta_seconds, p50_delta_seconds;
    double p90_delta_seconds, p99_delta_seconds, maximum_delta_seconds, mean_delta_seconds;
    double device_minimum_delta_seconds, device_p50_delta_seconds;
    double device_p90_delta_seconds, device_p99_delta_seconds;
    double device_maximum_delta_seconds, device_mean_delta_seconds;
    double device_standard_deviation_delta_seconds;
    int baseline_written, baseline_compatible, chart_generated;
    int device_timing_available;
} yvex_runtime_benchmark_operator_summary;

struct yvex_graph_attention_operator_result;
int yvex_runtime_benchmark_samples_finish(
    double *host_seconds, double *device_seconds, unsigned long long count,
    int device_requested, struct yvex_graph_attention_operator_result *result,
    yvex_error *err);
int yvex_runtime_benchmark_baseline_from_attention(
    const struct yvex_graph_attention_operator_result *result,
    yvex_runtime_benchmark_baseline *record,
    yvex_runtime_benchmark_failure *failure, yvex_error *err);
int yvex_runtime_benchmark_baseline_seal(yvex_runtime_benchmark_baseline *record,
                                         yvex_runtime_benchmark_failure *failure,
                                         yvex_error *err);
int yvex_runtime_benchmark_baseline_write(
    const char *path, const yvex_runtime_benchmark_baseline *record,
    yvex_runtime_benchmark_publication *result,
    yvex_runtime_benchmark_failure *failure, yvex_error *err);
int yvex_runtime_benchmark_baseline_open(const char *path,
                                         yvex_runtime_benchmark_baseline *record,
                                         yvex_runtime_benchmark_failure *failure,
                                         yvex_error *err);
int yvex_runtime_benchmark_compare(
    const yvex_runtime_benchmark_baseline *current,
    const yvex_runtime_benchmark_baseline *baseline,
    yvex_runtime_benchmark_comparison *result,
    yvex_runtime_benchmark_failure *failure, yvex_error *err);
int yvex_runtime_benchmark_chart_write(
    const yvex_runtime_benchmark_chart_request *request,
    yvex_runtime_benchmark_chart_result *result,
    yvex_runtime_benchmark_failure *failure, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_BENCHMARK_H_INCLUDED */
