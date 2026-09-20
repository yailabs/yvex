/* Own durable supervised source-acquisition identity, state, and reconciliation. */
#define _POSIX_C_SOURCE 200809L

#include <yvex/internal/source_acquisition.h>

#include <yvex/internal/core.h>
#include <yvex/internal/io.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ACQUISITION_RECORD_CAP (YVEX_PATH_CAP * 3u + 16384u)

static int acquisition_refuse(yvex_error *err, yvex_status status,
                               const char *where, const char *message)
{
    yvex_error_set(err, status, where, message);
    return status;
}

static int acquisition_copy(char *out, size_t cap, const char *value,
                            yvex_error *err, const char *field)
{
    size_t length = value ? strlen(value) : 0u;
    if (!value || !value[0] || length >= cap) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "source.acquisition",
                        "%s is missing or exceeds its bound", field);
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, value, length + 1u);
    return YVEX_OK;
}

const char *yvex_source_acquisition_lifecycle_name(
    yvex_source_acquisition_lifecycle lifecycle)
{
    switch (lifecycle) {
    case YVEX_SOURCE_ACQUISITION_STARTING: return "starting";
    case YVEX_SOURCE_ACQUISITION_DOWNLOADING: return "downloading";
    case YVEX_SOURCE_ACQUISITION_RETRYING: return "retrying";
    case YVEX_SOURCE_ACQUISITION_FINALIZING: return "finalizing";
    case YVEX_SOURCE_ACQUISITION_STOPPED: return "stopped";
    case YVEX_SOURCE_ACQUISITION_FAILED: return "failed";
    case YVEX_SOURCE_ACQUISITION_COMPLETE: return "complete";
    }
    return "unknown";
}

const char *yvex_source_acquisition_health_name(
    yvex_source_acquisition_health health)
{
    switch (health) {
    case YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN: return "unknown";
    case YVEX_SOURCE_ACQUISITION_HEALTH_HEALTHY: return "healthy";
    case YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED: return "degraded";
    case YVEX_SOURCE_ACQUISITION_HEALTH_STALLED: return "stalled";
    case YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE: return "not-applicable";
    }
    return "unknown";
}

int yvex_source_acquisition_terminal(yvex_source_acquisition_lifecycle lifecycle)
{
    return lifecycle == YVEX_SOURCE_ACQUISITION_STOPPED ||
           lifecycle == YVEX_SOURCE_ACQUISITION_FAILED ||
           lifecycle == YVEX_SOURCE_ACQUISITION_COMPLETE;
}

static int acquisition_random(unsigned char out[16], yvex_error *err)
{
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    size_t offset = 0u;
    if (fd < 0)
        return acquisition_refuse(err, YVEX_ERR_IO, "source.acquisition",
                                  "operation entropy is unavailable");
    while (offset < 16u) {
        ssize_t got = read(fd, out + offset, 16u - offset);
        if (got > 0) offset += (size_t)got;
        else if (got < 0 && errno == EINTR) continue;
        else break;
    }
    (void)close(fd);
    if (offset != 16u)
        return acquisition_refuse(err, YVEX_ERR_IO, "source.acquisition",
                                  "operation entropy read failed");
    return YVEX_OK;
}

int yvex_source_acquisition_operation_create(
    const yvex_source_acquisition_create_options *options,
    yvex_source_acquisition_operation *out, yvex_error *err)
{
    yvex_sha256 hash;
    unsigned char random[16], digest[YVEX_SHA256_DIGEST_BYTES];
    int rc;
    if (!options || !out)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition",
                                  "create options and output are required");
    memset(out, 0, sizeof(*out));
    out->schema_version = YVEX_SOURCE_ACQUISITION_OPERATION_SCHEMA;
    rc = acquisition_copy(out->provider, sizeof(out->provider), options->provider,
                          err, "provider");
    if (rc == YVEX_OK) rc = acquisition_copy(out->repository, sizeof(out->repository),
                                              options->repository, err, "repository");
    if (rc == YVEX_OK) rc = acquisition_copy(out->revision, sizeof(out->revision),
                                              options->revision, err, "revision");
    if (rc == YVEX_OK) rc = acquisition_copy(out->selection_identity,
                                              sizeof(out->selection_identity),
                                              options->selection_identity, err,
                                              "selection identity");
    if (rc == YVEX_OK && !yvex_sha256_hex_valid(out->selection_identity))
        rc = acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition",
                                "selection identity is not canonical SHA-256");
    if (rc == YVEX_OK) rc = acquisition_copy(out->source_path, sizeof(out->source_path),
                                              options->source_path, err, "source path");
    if (rc != YVEX_OK) return rc;
    if (!options->generation)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition",
                                  "operation generation must be nonzero");
    rc = acquisition_random(random, err);
    if (rc != YVEX_OK) return rc;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.source-acquisition.operation.v1") ||
        !yvex_sha256_update_text(&hash, out->provider) ||
        !yvex_sha256_update_text(&hash, out->repository) ||
        !yvex_sha256_update_text(&hash, out->revision) ||
        !yvex_sha256_update_text(&hash, out->selection_identity) ||
        !yvex_sha256_update_text(&hash, out->source_path) ||
        !yvex_sha256_update_u64(&hash, options->generation) ||
        !yvex_sha256_update(&hash, random, sizeof(random)) ||
        !yvex_sha256_final(&hash, digest))
        return acquisition_refuse(err, YVEX_ERR_STATE, "source.acquisition",
                                  "operation identity construction failed");
    yvex_sha256_hex(digest, out->operation_id);
    out->generation = options->generation;
    out->stall_window_seconds = options->stall_window_seconds;
    if (!out->stall_window_seconds)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition",
                                  "stall window must be nonzero");
    out->created_unix = options->now_unix;
    out->updated_unix = options->now_unix;
    out->lifecycle = YVEX_SOURCE_ACQUISITION_STARTING;
    out->health = YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN;
    out->progress.expected_bytes = options->expected_bytes;
    out->progress.selected_files = options->selected_files;
    out->progress.selected_shards = options->selected_shards;
    out->progress.selected_sidecars = options->selected_sidecars;
    snprintf(out->reason, sizeof(out->reason), "supervisor-starting");
    return YVEX_OK;
}

static int acquisition_boot_id(char out[YVEX_SOURCE_ACQUISITION_BOOT_ID_CAP])
{
    FILE *stream = fopen("/proc/sys/kernel/random/boot_id", "rb");
    size_t length;
    if (!stream) return 0;
    if (!fgets(out, YVEX_SOURCE_ACQUISITION_BOOT_ID_CAP, stream)) {
        fclose(stream);
        return 0;
    }
    fclose(stream);
    length = strlen(out);
    while (length && (out[length - 1u] == '\n' || out[length - 1u] == '\r'))
        out[--length] = '\0';
    return length > 0u;
}

static int acquisition_start_ticks(pid_t pid, unsigned long long *out)
{
    char path[64], buffer[4096], *cursor, *end;
    FILE *stream;
    unsigned int field = 3u;
    if (pid <= 0 || !out) return 0;
    if (snprintf(path, sizeof(path), "/proc/%lld/stat", (long long)pid) >=
        (int)sizeof(path)) return 0;
    stream = fopen(path, "rb");
    if (!stream) return 0;
    if (!fgets(buffer, sizeof(buffer), stream)) {
        fclose(stream);
        return 0;
    }
    fclose(stream);
    cursor = strrchr(buffer, ')');
    if (!cursor || cursor[1] != ' ') return 0;
    cursor += 2;
    while (field <= 22u) {
        while (*cursor == ' ') cursor++;
        if (!*cursor) return 0;
        end = cursor;
        while (*end && *end != ' ') end++;
        if (field == 22u) {
            char saved = *end;
            unsigned long long value;
            *end = '\0';
            errno = 0;
            value = strtoull(cursor, NULL, 10);
            *end = saved;
            if (errno) return 0;
            *out = value;
            return 1;
        }
        cursor = end;
        field++;
    }
    return 0;
}

int yvex_source_acquisition_process_capture(
    pid_t pid, pid_t process_group, yvex_source_acquisition_process *out,
    yvex_error *err)
{
    if (!out || pid <= 0 || process_group <= 0)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition.process",
                                  "positive process and group identities are required");
    memset(out, 0, sizeof(*out));
    if (!acquisition_boot_id(out->boot_id) ||
        !acquisition_start_ticks(pid, &out->start_ticks))
        return acquisition_refuse(err, YVEX_ERR_STATE, "source.acquisition.process",
                                  "process identity cannot be authenticated");
    out->pid = pid;
    out->process_group = process_group;
    out->present = 1;
    return YVEX_OK;
}

int yvex_source_acquisition_process_matches(
    const yvex_source_acquisition_process *identity)
{
    char boot_id[YVEX_SOURCE_ACQUISITION_BOOT_ID_CAP];
    unsigned long long ticks;
    if (!identity || !identity->present || identity->pid <= 0 ||
        !identity->start_ticks || !identity->boot_id[0]) return 0;
    return acquisition_boot_id(boot_id) && !strcmp(boot_id, identity->boot_id) &&
           acquisition_start_ticks(identity->pid, &ticks) &&
           ticks == identity->start_ticks;
}

static int acquisition_write_u64(FILE *stream, const char *name,
                                 yvex_source_acquisition_u64 fact)
{
    return fprintf(stream, ",\n  \"%s_known\": %s,\n  \"%s\": %llu",
                   name, fact.known ? "true" : "false", name, fact.value) >= 0;
}

static int acquisition_write_process(FILE *stream, const char *name,
                                     const yvex_source_acquisition_process *process)
{
    if (fprintf(stream,
                ",\n  \"%s_present\": %s,\n  \"%s_pid\": %lld,"
                "\n  \"%s_pgid\": %lld,\n  \"%s_start_ticks\": %llu,"
                "\n  \"%s_boot_id\": ",
                name, process->present ? "true" : "false",
                name, (long long)process->pid, name,
                (long long)process->process_group, name, process->start_ticks,
                name) < 0) return 0;
    yvex_file_json_write_string(stream, process->boot_id);
    return !ferror(stream);
}

static int acquisition_record_write(FILE *stream,
                                    const yvex_source_acquisition_operation *op)
{
    fputs("{\n  \"schema\": \"yvex.source.acquisition.operation.v1\","
          "\n  \"schema_version\": 1,\n  \"operation_id\": ", stream);
    yvex_file_json_write_string(stream, op->operation_id);
    fprintf(stream, ",\n  \"generation\": %llu,"
                    "\n  \"stall_window_seconds\": %llu,\n  \"provider\": ",
            op->generation, op->stall_window_seconds);
    yvex_file_json_write_string(stream, op->provider);
    fputs(",\n  \"repository\": ", stream); yvex_file_json_write_string(stream, op->repository);
    fputs(",\n  \"revision\": ", stream); yvex_file_json_write_string(stream, op->revision);
    fputs(",\n  \"selection_identity\": ", stream);
    yvex_file_json_write_string(stream, op->selection_identity);
    fputs(",\n  \"source_path\": ", stream); yvex_file_json_write_string(stream, op->source_path);
    if (!acquisition_write_process(stream, "supervisor", &op->supervisor) ||
        !acquisition_write_process(stream, "provider_process", &op->provider_process)) return 0;
    fputs(",\n  \"lifecycle\": ", stream);
    yvex_file_json_write_string(stream, yvex_source_acquisition_lifecycle_name(op->lifecycle));
    fputs(",\n  \"health\": ", stream);
    yvex_file_json_write_string(stream, yvex_source_acquisition_health_name(op->health));
#define WRITE_FACT(member) do { if (!acquisition_write_u64(stream, #member, op->progress.member)) return 0; } while (0)
    WRITE_FACT(expected_bytes); WRITE_FACT(committed_bytes);
    WRITE_FACT(provider_activity_bytes); WRITE_FACT(inflight_selected_bytes);
    WRITE_FACT(selected_files); WRITE_FACT(completed_files); WRITE_FACT(incomplete_files);
    WRITE_FACT(selected_shards); WRITE_FACT(completed_shards);
    WRITE_FACT(selected_sidecars); WRITE_FACT(completed_sidecars);
    WRITE_FACT(provider_partial_objects); WRITE_FACT(provider_lock_objects);
    WRITE_FACT(provider_activity_current_bytes_per_second);
    WRITE_FACT(provider_activity_rolling_bytes_per_second);
    WRITE_FACT(current_rate_bytes_per_second); WRITE_FACT(rolling_rate_bytes_per_second);
    WRITE_FACT(last_progress_unix); WRITE_FACT(last_provider_event_unix);
    WRITE_FACT(provider_event_sequence);
    WRITE_FACT(retry_count);
#undef WRITE_FACT
    fprintf(stream,
            ",\n  \"current_object\": ");
    yvex_file_json_write_string(stream, op->progress.current_object);
    fprintf(stream, ",\n  \"created_unix\": %llu,\n  \"updated_unix\": %llu,"
                    "\n  \"reason\": ", op->created_unix, op->updated_unix);
    yvex_file_json_write_string(stream, op->reason);
    fputs(",\n  \"result\": ", stream); yvex_file_json_write_string(stream, op->result);
    fputs("\n}\n", stream);
    return !ferror(stream);
}

int yvex_source_acquisition_operation_publish(
    const char *path, const yvex_source_acquisition_operation *operation,
    yvex_error *err)
{
    char temporary[YVEX_PATH_CAP];
    FILE *stream;
    int descriptor, rc = YVEX_OK;
    if (!path || !path[0] || !operation)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition.publish",
                                  "operation path and value are required");
    if (operation->schema_version != YVEX_SOURCE_ACQUISITION_OPERATION_SCHEMA ||
        !yvex_sha256_hex_valid(operation->operation_id) ||
        !yvex_sha256_hex_valid(operation->selection_identity))
        return acquisition_refuse(err, YVEX_ERR_FORMAT, "source.acquisition.publish",
                                  "operation identity or schema is invalid");
    if (snprintf(temporary, sizeof(temporary), "%s.pending.%lld", path,
                 (long long)getpid()) >= (int)sizeof(temporary))
        return acquisition_refuse(err, YVEX_ERR_BOUNDS, "source.acquisition.publish",
                                  "operation path exceeds capacity");
    stream = fopen(temporary, "wb");
    if (!stream)
        return acquisition_refuse(err, YVEX_ERR_IO, "source.acquisition.publish",
                                  "cannot create operation candidate");
    if (!acquisition_record_write(stream, operation) || fflush(stream) != 0) rc = YVEX_ERR_IO;
    descriptor = fileno(stream);
    if (rc == YVEX_OK && fsync(descriptor) != 0) rc = YVEX_ERR_IO;
    if (fclose(stream) != 0 && rc == YVEX_OK) rc = YVEX_ERR_IO;
    if (rc == YVEX_OK && rename(temporary, path) != 0) rc = YVEX_ERR_IO;
    if (rc == YVEX_OK) {
        char parent[YVEX_PATH_CAP];
        char *separator;
        int parent_fd;
        if (snprintf(parent, sizeof(parent), "%s", path) >= (int)sizeof(parent))
            rc = YVEX_ERR_BOUNDS;
        separator = strrchr(parent, '/');
        if (rc == YVEX_OK && separator) {
            *separator = '\0';
            parent_fd = open(parent[0] ? parent : "/", O_RDONLY | O_DIRECTORY);
            if (parent_fd < 0 || fsync(parent_fd) != 0) rc = YVEX_ERR_IO;
            if (parent_fd >= 0) close(parent_fd);
        }
    }
    if (rc != YVEX_OK) {
        (void)unlink(temporary);
        return acquisition_refuse(err, YVEX_ERR_IO, "source.acquisition.publish",
                                  "operation publication failed");
    }
    return YVEX_OK;
}

static int acquisition_probe_u64(const char *record, const char *name,
                                 unsigned long long *out)
{
    const char *value = yvex_json_probe_field_value(record, name);
    yvex_json json;
    if (!value) return 0;
    yvex_json_init(&json, value, strlen(value));
    return yvex_json_u64(&json, out);
}

static int acquisition_probe_bool(const char *record, const char *name, int *out)
{
    const char *value = yvex_json_probe_field_value(record, name);
    yvex_json json;
    if (!value) return 0;
    yvex_json_init(&json, value, strlen(value));
    return yvex_json_bool(&json, out);
}

static int acquisition_probe_i64(const char *record, const char *name, pid_t *out)
{
    const char *value = yvex_json_probe_field_value(record, name);
    char *end = NULL;
    long long parsed;
    if (!value) return 0;
    errno = 0;
    parsed = strtoll(value, &end, 10);
    if (errno || end == value) return 0;
    *out = (pid_t)parsed;
    return (long long)*out == parsed;
}

static int acquisition_parse_lifecycle(const char *name,
                                       yvex_source_acquisition_lifecycle *out)
{
    unsigned int value;
    for (value = YVEX_SOURCE_ACQUISITION_STARTING;
         value <= YVEX_SOURCE_ACQUISITION_COMPLETE; ++value) {
        if (!strcmp(name, yvex_source_acquisition_lifecycle_name(
                              (yvex_source_acquisition_lifecycle)value))) {
            *out = (yvex_source_acquisition_lifecycle)value;
            return 1;
        }
    }
    return 0;
}

static int acquisition_parse_health(const char *name,
                                    yvex_source_acquisition_health *out)
{
    unsigned int value;
    for (value = YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN;
         value <= YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE; ++value) {
        if (!strcmp(name, yvex_source_acquisition_health_name(
                              (yvex_source_acquisition_health)value))) {
            *out = (yvex_source_acquisition_health)value;
            return 1;
        }
    }
    return 0;
}

static int acquisition_read_fact(const char *record, const char *name,
                                 yvex_source_acquisition_u64 *out)
{
    char known[96];
    if (snprintf(known, sizeof(known), "%s_known", name) >= (int)sizeof(known)) return 0;
    return acquisition_probe_bool(record, known, &out->known) &&
           acquisition_probe_u64(record, name, &out->value);
}

static int acquisition_read_process(const char *record, const char *name,
                                    yvex_source_acquisition_process *out)
{
    char field[96];
#define PROCESS_FIELD(suffix) \
    (snprintf(field, sizeof(field), "%s_" suffix, name) < (int)sizeof(field))
    if (!PROCESS_FIELD("present") || !acquisition_probe_bool(record, field, &out->present) ||
        !PROCESS_FIELD("pid") || !acquisition_probe_i64(record, field, &out->pid) ||
        !PROCESS_FIELD("pgid") || !acquisition_probe_i64(record, field, &out->process_group) ||
        !PROCESS_FIELD("start_ticks") || !acquisition_probe_u64(record, field, &out->start_ticks) ||
        !PROCESS_FIELD("boot_id") || !yvex_json_probe_string_field(
            record, field, out->boot_id, sizeof(out->boot_id))) return 0;
#undef PROCESS_FIELD
    return !out->present || (out->pid > 0 && out->process_group > 0 &&
                             out->start_ticks && out->boot_id[0]);
}

int yvex_source_acquisition_operation_read(
    const char *path, yvex_source_acquisition_operation *out, yvex_error *err)
{
    char *record, schema[64], lifecycle[32], health[32];
    size_t length = 0u;
    unsigned long long schema_version;
    if (!path || !out)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition.read",
                                  "operation path and output are required");
    record = yvex_read_bounded_file(path, ACQUISITION_RECORD_CAP, &length, err);
    if (!record) return yvex_error_code(err);
    memset(out, 0, sizeof(*out));
    if (!yvex_json_probe_string_field(record, "schema", schema, sizeof(schema)) ||
        strcmp(schema, "yvex.source.acquisition.operation.v1") ||
        !acquisition_probe_u64(record, "schema_version", &schema_version) ||
        schema_version != YVEX_SOURCE_ACQUISITION_OPERATION_SCHEMA ||
        !yvex_json_probe_string_field(record, "operation_id", out->operation_id,
                                      sizeof(out->operation_id)) ||
        !acquisition_probe_u64(record, "generation", &out->generation) ||
        !acquisition_probe_u64(record, "stall_window_seconds",
                               &out->stall_window_seconds) ||
        !yvex_json_probe_string_field(record, "provider", out->provider, sizeof(out->provider)) ||
        !yvex_json_probe_string_field(record, "repository", out->repository,
                                      sizeof(out->repository)) ||
        !yvex_json_probe_string_field(record, "revision", out->revision, sizeof(out->revision)) ||
        !yvex_json_probe_string_field(record, "selection_identity", out->selection_identity,
                                      sizeof(out->selection_identity)) ||
        !yvex_json_probe_string_field(record, "source_path", out->source_path,
                                      sizeof(out->source_path)) ||
        !acquisition_read_process(record, "supervisor", &out->supervisor) ||
        !acquisition_read_process(record, "provider_process", &out->provider_process) ||
        !yvex_json_probe_string_field(record, "lifecycle", lifecycle, sizeof(lifecycle)) ||
        !acquisition_parse_lifecycle(lifecycle, &out->lifecycle) ||
        !yvex_json_probe_string_field(record, "health", health, sizeof(health)) ||
        !acquisition_parse_health(health, &out->health)) goto malformed;
#define READ_FACT(member) \
    do { if (!acquisition_read_fact(record, #member, &out->progress.member)) goto malformed; } while (0)
    READ_FACT(expected_bytes); READ_FACT(committed_bytes);
    READ_FACT(provider_activity_bytes); READ_FACT(inflight_selected_bytes);
    READ_FACT(selected_files); READ_FACT(completed_files); READ_FACT(incomplete_files);
    READ_FACT(selected_shards); READ_FACT(completed_shards);
    READ_FACT(selected_sidecars); READ_FACT(completed_sidecars);
    READ_FACT(provider_partial_objects); READ_FACT(provider_lock_objects);
    READ_FACT(provider_activity_current_bytes_per_second);
    READ_FACT(provider_activity_rolling_bytes_per_second);
    READ_FACT(current_rate_bytes_per_second); READ_FACT(rolling_rate_bytes_per_second);
    READ_FACT(last_progress_unix); READ_FACT(last_provider_event_unix);
    READ_FACT(provider_event_sequence);
    READ_FACT(retry_count);
#undef READ_FACT
    if (!yvex_json_probe_string_field(record, "current_object", out->progress.current_object,
                                      sizeof(out->progress.current_object)) ||
        !acquisition_probe_u64(record, "created_unix", &out->created_unix) ||
        !acquisition_probe_u64(record, "updated_unix", &out->updated_unix) ||
        !yvex_json_probe_string_field(record, "reason", out->reason, sizeof(out->reason)) ||
        !yvex_json_probe_string_field(record, "result", out->result, sizeof(out->result)))
        goto malformed;
    free(record);
    out->schema_version = (unsigned int)schema_version;
    if (!yvex_sha256_hex_valid(out->operation_id) ||
        !yvex_sha256_hex_valid(out->selection_identity) || !out->generation ||
        !out->provider[0] || !out->repository[0] || !out->revision[0] ||
        !out->source_path[0] || !out->stall_window_seconds)
        return acquisition_refuse(err, YVEX_ERR_FORMAT, "source.acquisition.read",
                                  "operation identity is invalid");
    return YVEX_OK;
malformed:
    free(record);
    return acquisition_refuse(err, YVEX_ERR_FORMAT, "source.acquisition.read",
                              "operation record is malformed or unsupported");
}

int yvex_source_acquisition_reconcile(
    yvex_source_acquisition_operation *operation, unsigned long long now_unix,
    unsigned long long stall_window_seconds, yvex_error *err)
{
    int supervisor_alive;
    if (!operation || !stall_window_seconds)
        return acquisition_refuse(err, YVEX_ERR_INVALID_ARG, "source.acquisition.reconcile",
                                  "operation and nonzero stall window are required");
    if (operation->schema_version != YVEX_SOURCE_ACQUISITION_OPERATION_SCHEMA)
        return acquisition_refuse(err, YVEX_ERR_FORMAT, "source.acquisition.reconcile",
                                  "operation schema is unsupported");
    if (yvex_source_acquisition_terminal(operation->lifecycle)) return YVEX_OK;
    supervisor_alive = yvex_source_acquisition_process_matches(&operation->supervisor);
    if (operation->supervisor.present && !supervisor_alive) {
        operation->lifecycle = YVEX_SOURCE_ACQUISITION_STOPPED;
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED;
        operation->updated_unix = now_unix;
        snprintf(operation->reason, sizeof(operation->reason), "supervisor-identity-lost");
        snprintf(operation->result, sizeof(operation->result), "interrupted-resumable");
        return YVEX_OK;
    }
    if (operation->lifecycle == YVEX_SOURCE_ACQUISITION_RETRYING) {
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED;
    } else if (!operation->progress.last_progress_unix.known) {
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN;
    } else if (now_unix >= operation->progress.last_progress_unix.value &&
               now_unix - operation->progress.last_progress_unix.value >=
                   stall_window_seconds) {
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_STALLED;
        snprintf(operation->reason, sizeof(operation->reason), "progress-window-expired");
    } else {
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_HEALTHY;
    }
    return YVEX_OK;
}
