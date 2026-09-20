/* Durable identity, lifecycle, health, and progress for supervised source acquisition. */
#ifndef INCLUDE_YVEX_INTERNAL_SOURCE_ACQUISITION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_SOURCE_ACQUISITION_H_INCLUDED

#include <sys/types.h>
#include <yvex/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_SOURCE_ACQUISITION_OPERATION_SCHEMA 1u
#define YVEX_SOURCE_ACQUISITION_ID_CAP 65u
#define YVEX_SOURCE_ACQUISITION_PROVIDER_CAP 32u
#define YVEX_SOURCE_ACQUISITION_REPOSITORY_CAP 256u
#define YVEX_SOURCE_ACQUISITION_REVISION_CAP 128u
#define YVEX_SOURCE_ACQUISITION_REASON_CAP 160u
#define YVEX_SOURCE_ACQUISITION_OBJECT_CAP 256u
#define YVEX_SOURCE_ACQUISITION_BOOT_ID_CAP 64u

typedef enum {
    YVEX_SOURCE_ACQUISITION_STARTING = 0,
    YVEX_SOURCE_ACQUISITION_DOWNLOADING,
    YVEX_SOURCE_ACQUISITION_RETRYING,
    YVEX_SOURCE_ACQUISITION_FINALIZING,
    YVEX_SOURCE_ACQUISITION_STOPPED,
    YVEX_SOURCE_ACQUISITION_FAILED,
    YVEX_SOURCE_ACQUISITION_COMPLETE
} yvex_source_acquisition_lifecycle;

typedef enum {
    YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN = 0,
    YVEX_SOURCE_ACQUISITION_HEALTH_HEALTHY,
    YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED,
    YVEX_SOURCE_ACQUISITION_HEALTH_STALLED,
    YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE
} yvex_source_acquisition_health;

typedef struct {
    unsigned long long value;
    int known;
} yvex_source_acquisition_u64;

typedef struct {
    pid_t pid, process_group;
    unsigned long long start_ticks;
    char boot_id[YVEX_SOURCE_ACQUISITION_BOOT_ID_CAP];
    int present;
} yvex_source_acquisition_process;

typedef struct {
    yvex_source_acquisition_u64 expected_bytes;
    yvex_source_acquisition_u64 committed_bytes;
    /* Provider writes/activity are diagnostic work, not selected in-flight bytes. */
    yvex_source_acquisition_u64 provider_activity_bytes;
    yvex_source_acquisition_u64 inflight_selected_bytes;
    yvex_source_acquisition_u64 selected_files;
    yvex_source_acquisition_u64 completed_files;
    yvex_source_acquisition_u64 incomplete_files;
    yvex_source_acquisition_u64 selected_shards;
    yvex_source_acquisition_u64 completed_shards;
    yvex_source_acquisition_u64 selected_sidecars;
    yvex_source_acquisition_u64 completed_sidecars;
    yvex_source_acquisition_u64 provider_partial_objects;
    yvex_source_acquisition_u64 provider_lock_objects;
    /* These diagnose provider writes; they are not selected-payload transfer rates. */
    yvex_source_acquisition_u64 provider_activity_current_bytes_per_second;
    yvex_source_acquisition_u64 provider_activity_rolling_bytes_per_second;
    /* Selected-payload rates remain unknown unless the adapter can prove their domain. */
    yvex_source_acquisition_u64 current_rate_bytes_per_second;
    yvex_source_acquisition_u64 rolling_rate_bytes_per_second;
    yvex_source_acquisition_u64 last_progress_unix;
    yvex_source_acquisition_u64 last_provider_event_unix;
    yvex_source_acquisition_u64 provider_event_sequence;
    yvex_source_acquisition_u64 retry_count;
    char current_object[YVEX_SOURCE_ACQUISITION_OBJECT_CAP];
} yvex_source_acquisition_progress;

typedef struct {
    unsigned int schema_version;
    char operation_id[YVEX_SOURCE_ACQUISITION_ID_CAP];
    unsigned long long generation;
    char provider[YVEX_SOURCE_ACQUISITION_PROVIDER_CAP];
    char repository[YVEX_SOURCE_ACQUISITION_REPOSITORY_CAP];
    char revision[YVEX_SOURCE_ACQUISITION_REVISION_CAP];
    char selection_identity[YVEX_SOURCE_ACQUISITION_ID_CAP];
    char source_path[YVEX_PATH_CAP];
    unsigned long long stall_window_seconds;
    yvex_source_acquisition_process supervisor;
    yvex_source_acquisition_process provider_process;
    yvex_source_acquisition_lifecycle lifecycle;
    yvex_source_acquisition_health health;
    yvex_source_acquisition_progress progress;
    unsigned long long created_unix, updated_unix;
    char reason[YVEX_SOURCE_ACQUISITION_REASON_CAP];
    char result[YVEX_SOURCE_ACQUISITION_REASON_CAP];
} yvex_source_acquisition_operation;

typedef struct {
    const char *provider, *repository, *revision, *selection_identity, *source_path;
    unsigned long long generation, now_unix, stall_window_seconds;
    yvex_source_acquisition_u64 expected_bytes;
    yvex_source_acquisition_u64 selected_files;
    yvex_source_acquisition_u64 selected_shards;
    yvex_source_acquisition_u64 selected_sidecars;
} yvex_source_acquisition_create_options;

const char *yvex_source_acquisition_lifecycle_name(
    yvex_source_acquisition_lifecycle lifecycle);
const char *yvex_source_acquisition_health_name(
    yvex_source_acquisition_health health);
int yvex_source_acquisition_terminal(yvex_source_acquisition_lifecycle lifecycle);
int yvex_source_acquisition_operation_create(
    const yvex_source_acquisition_create_options *options,
    yvex_source_acquisition_operation *out, yvex_error *err);
int yvex_source_acquisition_operation_read(
    const char *path, yvex_source_acquisition_operation *out, yvex_error *err);
int yvex_source_acquisition_operation_publish(
    const char *path, const yvex_source_acquisition_operation *operation,
    yvex_error *err);
int yvex_source_acquisition_process_capture(
    pid_t pid, pid_t process_group, yvex_source_acquisition_process *out,
    yvex_error *err);
int yvex_source_acquisition_process_matches(
    const yvex_source_acquisition_process *identity);
int yvex_source_acquisition_reconcile(
    yvex_source_acquisition_operation *operation, unsigned long long now_unix,
    unsigned long long stall_window_seconds, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_SOURCE_ACQUISITION_H_INCLUDED */
