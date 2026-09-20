/* Project the Source-owned acquisition operation into detached CLI supervision. */
#define _POSIX_C_SOURCE 200809L

#include "src/cli/model_artifacts/private.h"
#include "src/cli/io/terminal/private.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <yvex/internal/core.h>

#define ACQUISITION_WORKER_ENV "YVEX_SOURCE_ACQUISITION_WORKER"
#define ACQUISITION_PATH_ENV "YVEX_SOURCE_ACQUISITION_OPERATION"
#define ACQUISITION_ID_ENV "YVEX_SOURCE_ACQUISITION_ID"

static unsigned long long acquisition_now(void)
{
    time_t now = time(NULL);
    return now == (time_t)-1 || now < 0 ? 0ull : (unsigned long long)now;
}

int model_acquisition_worker_active(void)
{
    const char *worker = getenv(ACQUISITION_WORKER_ENV);
    return worker && !strcmp(worker, "1");
}

int model_acquisition_operation_matches_report(
    const yvex_source_acquisition_operation *operation,
    const yvex_model_download_report *report, const char *selection_identity)
{
    if (!operation || !report) return 0;
    return !strcmp(operation->provider, report->provider) &&
           !strcmp(operation->repository, report->repo_id) &&
           !strcmp(operation->revision, report->revision) &&
           !strcmp(operation->source_path, report->local_source_dir) &&
           (!selection_identity || !selection_identity[0] ||
            !strcmp(operation->selection_identity, selection_identity));
}

static int acquisition_operation_load(const yvex_model_download_report *report,
                                      yvex_source_acquisition_operation *operation,
                                      yvex_error *err)
{
    const char *worker_path = getenv(ACQUISITION_PATH_ENV);
    const char *path = worker_path && worker_path[0] ? worker_path : report->operation_path;
    const char *worker_id = getenv(ACQUISITION_ID_ENV);
    int rc = yvex_source_acquisition_operation_read(path, operation, err);
    if (rc != YVEX_OK) return rc;
    if (!model_acquisition_operation_matches_report(operation, report, NULL) ||
        (worker_id && worker_id[0] && strcmp(worker_id, operation->operation_id))) {
        yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.worker",
                       "operation identity does not match the acquisition target");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int acquisition_operation_store(const yvex_model_download_report *report,
                                       yvex_source_acquisition_operation *operation,
                                       yvex_error *err)
{
    const char *worker_path = getenv(ACQUISITION_PATH_ENV);
    const char *path = worker_path && worker_path[0] ? worker_path : report->operation_path;
    operation->updated_unix = acquisition_now();
    return yvex_source_acquisition_operation_publish(path, operation, err);
}

static int acquisition_process_io(pid_t pid, unsigned long long *write_bytes)
{
    char path[64], key[64];
    unsigned long long value;
    FILE *stream;
    if (pid <= 0 || !write_bytes ||
        snprintf(path, sizeof(path), "/proc/%lld/io", (long long)pid) >=
            (int)sizeof(path)) return 0;
    stream = fopen(path, "rb");
    if (!stream) return 0;
    while (fscanf(stream, "%63s %llu", key, &value) == 2) {
        if (!strcmp(key, "write_bytes:")) {
            fclose(stream);
            *write_bytes = value;
            return 1;
        }
    }
    fclose(stream);
    return 0;
}

static void acquisition_provider_event_observe(
    const yvex_model_download_report *report,
    yvex_source_acquisition_operation *operation, unsigned long long now)
{
    char *record, schema[64], kind[32];
    char object[YVEX_SOURCE_ACQUISITION_OBJECT_CAP];
    unsigned long long retry_count = 0ull, sequence = 0ull;
    size_t length = 0u;
    yvex_error err;
    if (!report->provider_event_path[0] ||
        access(report->provider_event_path, R_OK) != 0) return;
    yvex_error_clear(&err);
    record = yvex_read_bounded_file(report->provider_event_path, 4096u,
                                    &length, &err);
    if (!record || !length) {
        free(record);
        return;
    }
    object[0] = '\0';
    if (!yvex_json_probe_string_field(record, "schema", schema, sizeof(schema)) ||
        strcmp(schema, "yvex.provider.acquisition.event.v1") ||
        !yvex_json_probe_string_field(record, "kind", kind, sizeof(kind))) {
        free(record);
        return;
    }
    {
        const char *value = yvex_json_probe_field_value(record, "sequence");
        yvex_json json;
        if (!value) {
            free(record);
            return;
        }
        yvex_json_init(&json, value, strlen(value));
        if (!yvex_json_u64(&json, &sequence) || !sequence ||
            (operation->progress.provider_event_sequence.known &&
             sequence <= operation->progress.provider_event_sequence.value)) {
            free(record);
            return;
        }
    }
    (void)yvex_json_probe_string_field(record, "object", object, sizeof(object));
    if (!strcmp(kind, "retry")) {
        const char *value = yvex_json_probe_field_value(record, "retry_count");
        yvex_json json;
        if (!value) {
            free(record);
            return;
        }
        yvex_json_init(&json, value, strlen(value));
        if (!yvex_json_u64(&json, &retry_count)) {
            free(record);
            return;
        }
        operation->lifecycle = YVEX_SOURCE_ACQUISITION_RETRYING;
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED;
        operation->progress.retry_count =
            (yvex_source_acquisition_u64){retry_count, 1};
        snprintf(operation->reason, sizeof(operation->reason), "provider-retrying");
    } else if (!strcmp(kind, "progress")) {
        if (operation->lifecycle == YVEX_SOURCE_ACQUISITION_RETRYING)
            operation->lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
        operation->progress.last_progress_unix =
            (yvex_source_acquisition_u64){now, 1};
        snprintf(operation->reason, sizeof(operation->reason), "provider-progress");
    } else if (strcmp(kind, "heartbeat")) {
        free(record);
        return;
    }
    operation->progress.last_provider_event_unix =
        (yvex_source_acquisition_u64){now, 1};
    operation->progress.provider_event_sequence =
        (yvex_source_acquisition_u64){sequence, 1};
    if (object[0])
        snprintf(operation->progress.current_object,
                 sizeof(operation->progress.current_object), "%s", object);
    free(record);
}

static void acquisition_progress_from_scan(
    yvex_source_acquisition_operation *operation,
    const yvex_model_download_source_scan *scan, unsigned long long provider_bytes,
    int provider_bytes_known, unsigned long long now)
{
    unsigned long long previous = operation->progress.provider_activity_bytes.value;
    unsigned long long previous_time = operation->updated_unix;
    int advanced = 0;
    if (!operation->progress.committed_bytes.known ||
        scan->total_regular_file_bytes != operation->progress.committed_bytes.value ||
        !operation->progress.completed_files.known ||
        scan->file_count != operation->progress.completed_files.value)
        advanced = 1;
    operation->progress.committed_bytes =
        (yvex_source_acquisition_u64){scan->total_regular_file_bytes, 1};
    operation->progress.completed_files =
        (yvex_source_acquisition_u64){scan->file_count, 1};
    operation->progress.completed_shards =
        (yvex_source_acquisition_u64){scan->safetensors_count, 1};
    operation->progress.provider_partial_objects =
        (yvex_source_acquisition_u64){scan->partial_file_count, 1};
    operation->progress.provider_lock_objects =
        (yvex_source_acquisition_u64){scan->lock_count, 1};
    if (operation->progress.selected_files.known &&
        scan->file_count <= operation->progress.selected_files.value)
        operation->progress.incomplete_files = (yvex_source_acquisition_u64){
            operation->progress.selected_files.value - scan->file_count, 1};
    if (provider_bytes_known) {
        if (!operation->progress.provider_activity_bytes.known || provider_bytes != previous)
            advanced = 1;
        operation->progress.provider_activity_bytes =
            (yvex_source_acquisition_u64){provider_bytes, 1};
        if (previous_time && now > previous_time && provider_bytes >= previous) {
            unsigned long long rate = (provider_bytes - previous) / (now - previous_time);
            operation->progress.provider_activity_current_bytes_per_second =
                (yvex_source_acquisition_u64){rate, 1};
            if (operation->progress.provider_activity_rolling_bytes_per_second.known)
                rate = (operation->progress.provider_activity_rolling_bytes_per_second.value * 3ull + rate) /
                       4ull;
            operation->progress.provider_activity_rolling_bytes_per_second =
                (yvex_source_acquisition_u64){rate, 1};
        }
    }
    if (advanced) {
        operation->progress.last_progress_unix =
            (yvex_source_acquisition_u64){now, 1};
    }
}

int model_acquisition_worker_begin(const yvex_model_download_report *report,
                                   yvex_error *err)
{
    yvex_source_acquisition_operation operation;
    int rc;
    if (!model_acquisition_worker_active()) return YVEX_OK;
    rc = acquisition_operation_load(report, &operation, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_source_acquisition_process_capture(getpid(), getpgrp(),
                                                  &operation.supervisor, err);
    if (rc != YVEX_OK) return rc;
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN;
    snprintf(operation.reason, sizeof(operation.reason), "provider-starting");
    operation.result[0] = '\0';
    return acquisition_operation_store(report, &operation, err);
}

void model_acquisition_provider_started(const yvex_model_download_report *report,
                                        pid_t pid, pid_t process_group)
{
    yvex_source_acquisition_operation operation;
    yvex_error err;
    if (!model_acquisition_worker_active()) return;
    yvex_error_clear(&err);
    if (acquisition_operation_load(report, &operation, &err) != YVEX_OK ||
        yvex_source_acquisition_process_capture(pid, process_group,
                                                &operation.provider_process,
                                                &err) != YVEX_OK) return;
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    snprintf(operation.reason, sizeof(operation.reason), "provider-running");
    operation.progress.last_provider_event_unix =
        (yvex_source_acquisition_u64){acquisition_now(), 1};
    (void)acquisition_operation_store(report, &operation, &err);
}

void model_acquisition_provider_observe(const yvex_model_download_report *report)
{
    yvex_source_acquisition_operation operation;
    yvex_model_download_source_scan scan;
    yvex_error err;
    unsigned long long provider_bytes = 0ull, now = acquisition_now();
    int provider_known;
    if (!model_acquisition_worker_active()) return;
    yvex_error_clear(&err);
    if (acquisition_operation_load(report, &operation, &err) != YVEX_OK) return;
    memset(&scan, 0, sizeof(scan));
    if (model_download_scan_source(report->local_source_dir, &scan, &err) != YVEX_OK)
        return;
    provider_known = acquisition_process_io(operation.provider_process.pid,
                                            &provider_bytes);
    acquisition_progress_from_scan(&operation, &scan, provider_bytes,
                                   provider_known, now);
    acquisition_provider_event_observe(report, &operation, now);
    (void)yvex_source_acquisition_reconcile(&operation, now,
                                            operation.stall_window_seconds, &err);
    (void)acquisition_operation_store(report, &operation, &err);
}

void model_acquisition_worker_finalizing(const yvex_model_download_report *report)
{
    yvex_source_acquisition_operation operation;
    yvex_error err;
    if (!model_acquisition_worker_active()) return;
    yvex_error_clear(&err);
    if (acquisition_operation_load(report, &operation, &err) != YVEX_OK) return;
    operation.lifecycle = YVEX_SOURCE_ACQUISITION_FINALIZING;
    operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_UNKNOWN;
    snprintf(operation.reason, sizeof(operation.reason), "source-finalization");
    (void)acquisition_operation_store(report, &operation, &err);
}

void model_acquisition_worker_finish(const yvex_model_download_report *report,
                                     int command_status)
{
    yvex_source_acquisition_operation operation;
    yvex_error err;
    if (!model_acquisition_worker_active()) return;
    model_acquisition_provider_observe(report);
    yvex_error_clear(&err);
    if (acquisition_operation_load(report, &operation, &err) != YVEX_OK) return;
    if (command_status == 0) {
        operation.lifecycle = YVEX_SOURCE_ACQUISITION_COMPLETE;
        operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE;
        snprintf(operation.reason, sizeof(operation.reason), "selected-source-complete");
        snprintf(operation.result, sizeof(operation.result), "transfer-complete");
    } else if (report->interrupted || model_download_provider_was_interrupted() ||
               command_status == 130 || command_status == 143 ||
               !strcmp(operation.reason, "stop-requested")) {
        operation.lifecycle = YVEX_SOURCE_ACQUISITION_STOPPED;
        operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE;
        snprintf(operation.reason, sizeof(operation.reason), "operator-stop");
        snprintf(operation.result, sizeof(operation.result), "interrupted-resumable");
    } else {
        operation.lifecycle = YVEX_SOURCE_ACQUISITION_FAILED;
        operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED;
        snprintf(operation.reason, sizeof(operation.reason), "acquisition-command-failed");
        snprintf(operation.result, sizeof(operation.result), "exit-status-%d", command_status);
    }
    memset(&operation.provider_process, 0, sizeof(operation.provider_process));
    (void)acquisition_operation_store(report, &operation, &err);
}

static int acquisition_exec_supervisor(int arg_count, char **args,
                                       const yvex_model_download_report *report,
                                       const char *operation_id,
                                       pid_t *supervisor_out,
                                       yvex_error *err)
{
    int descriptors[2], status;
    pid_t launcher, supervisor = -1;
    ssize_t got;
    char **exec_args;
    int index, input_index = 3, output_index = 0;
    int resume = arg_count > 3 && !strcmp(args[3], "resume");
    if (resume) input_index = 4;
    exec_args = calloc((size_t)arg_count + 2u, sizeof(*exec_args));
    if (!exec_args) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source.acquisition.supervisor",
                       "supervisor argument allocation failed");
        return YVEX_ERR_NOMEM;
    }
    /* The offline adapter receives an internal argv (models download ...).
     * A self-exec must re-enter through the canonical public command path. */
    exec_args[output_index++] = args[0];
    exec_args[output_index++] = "source";
    exec_args[output_index++] = resume ? "resume" : "acquire";
    for (index = input_index; index < arg_count; ++index)
        exec_args[output_index++] = args[index];
    if (pipe(descriptors) != 0) goto io_failure;
    launcher = fork();
    if (launcher < 0) goto io_failure;
    if (launcher == 0) {
        pid_t child;
        close(descriptors[0]);
        if (setsid() < 0) _exit(126);
        child = fork();
        if (child < 0) _exit(126);
        if (child > 0) {
            ssize_t written = write(descriptors[1], &child, sizeof(child));
            if (written != (ssize_t)sizeof(child)) _exit(126);
            _exit(0);
        }
        close(descriptors[1]);
        {
            int input_fd = open("/dev/null", O_RDONLY);
            int output_fd = open(report->supervisor_log_path,
                              O_WRONLY | O_CREAT | O_TRUNC, 0664);
            if (input_fd < 0 || output_fd < 0 || dup2(input_fd, STDIN_FILENO) < 0 ||
                dup2(output_fd, STDOUT_FILENO) < 0 ||
                dup2(output_fd, STDERR_FILENO) < 0)
                _exit(126);
            if (input_fd > STDERR_FILENO) close(input_fd);
            if (output_fd > STDERR_FILENO) close(output_fd);
        }
        if (setenv(ACQUISITION_WORKER_ENV, "1", 1) != 0 ||
            setenv(ACQUISITION_PATH_ENV, report->operation_path, 1) != 0 ||
            setenv(ACQUISITION_ID_ENV, operation_id, 1) != 0) _exit(126);
        execv("/proc/self/exe", exec_args);
        _exit(127);
    }
    close(descriptors[1]);
    got = read(descriptors[0], &supervisor, sizeof(supervisor));
    close(descriptors[0]);
    while (waitpid(launcher, &status, 0) < 0 && errno == EINTR) {}
    free(exec_args);
    if (got != (ssize_t)sizeof(supervisor) || supervisor <= 0 ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.supervisor",
                       "detached supervisor failed to start");
        return YVEX_ERR_STATE;
    }
    if (supervisor_out) *supervisor_out = supervisor;
    return YVEX_OK;
io_failure:
    free(exec_args);
    yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.supervisor",
                   "detached supervisor process creation failed");
    return YVEX_ERR_IO;
}

int model_acquisition_supervisor_start(
    int arg_count, char **args, const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report, const char *selection_identity,
    yvex_error *err)
{
    yvex_source_acquisition_operation operation, previous;
    yvex_source_acquisition_create_options create;
    unsigned long long generation = 1ull;
    pid_t supervisor = -1;
    int rc;
    if (access(report->operation_path, F_OK) == 0) {
        rc = yvex_source_acquisition_operation_read(report->operation_path,
                                                     &previous, err);
        if (rc != YVEX_OK) return rc;
        if (!model_acquisition_operation_matches_report(&previous, report,
                                                        selection_identity)) {
            yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.supervisor",
                           "existing operation belongs to another immutable target");
            return YVEX_ERR_STATE;
        }
        if (!yvex_source_acquisition_terminal(previous.lifecycle) &&
            yvex_source_acquisition_process_matches(&previous.supervisor))
            return YVEX_OK;
        if (yvex_source_acquisition_process_matches(&previous.provider_process)) {
            yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.supervisor",
                           "authenticated orphan provider is still active; stop it before resume");
            return YVEX_ERR_STATE;
        }
        generation = previous.generation + 1ull;
    }
    memset(&create, 0, sizeof(create));
    create.provider = report->provider;
    create.repository = report->repo_id;
    create.revision = report->revision;
    create.selection_identity = selection_identity;
    create.source_path = report->local_source_dir;
    create.generation = generation;
    create.now_unix = acquisition_now();
    create.stall_window_seconds = options->stall_seconds;
    create.expected_bytes = (yvex_source_acquisition_u64){
        options->expected_bytes, options->expected_bytes_known};
    create.selected_shards = (yvex_source_acquisition_u64){
        options->selected_shards, options->selected_shards_known};
    rc = yvex_source_acquisition_operation_create(&create, &operation, err);
    if (rc == YVEX_OK)
        rc = yvex_core_mkdir_parent(report->operation_path,
                                    "source.acquisition.supervisor", err);
    if (rc == YVEX_OK)
        rc = yvex_core_mkdir_parent(report->supervisor_log_path,
                                    "source.acquisition.supervisor", err);
    if (rc == YVEX_OK && unlink(report->provider_event_path) != 0 &&
        errno != ENOENT) {
        rc = YVEX_ERR_IO;
        yvex_error_set(err, rc, "source.acquisition.supervisor",
                       "stale provider event cannot be retired safely");
    }
    if (rc == YVEX_OK)
        rc = yvex_source_acquisition_operation_publish(report->operation_path,
                                                        &operation, err);
    if (rc == YVEX_OK)
        rc = acquisition_exec_supervisor(arg_count, args, report,
                                         operation.operation_id, &supervisor, err);
    if (rc == YVEX_OK) {
        unsigned int attempt;
        for (attempt = 0u; attempt < 100u; ++attempt) {
            yvex_source_acquisition_operation ready;
            yvex_error observed;
            yvex_error_clear(&observed);
            if (yvex_source_acquisition_operation_read(report->operation_path,
                                                        &ready, &observed) == YVEX_OK &&
                ready.supervisor.present &&
                !strcmp(ready.operation_id, operation.operation_id) &&
                (yvex_source_acquisition_process_matches(&ready.supervisor) ||
                 yvex_source_acquisition_terminal(ready.lifecycle)))
                return YVEX_OK;
            if (supervisor > 0 && kill(supervisor, 0) != 0 && errno == ESRCH) break;
            (void)poll(NULL, 0, 50);
        }
        rc = YVEX_ERR_STATE;
        yvex_error_set(err, rc, "source.acquisition.supervisor",
                       "detached supervisor did not publish authenticated readiness");
    }
    if (rc != YVEX_OK) {
        operation.lifecycle = YVEX_SOURCE_ACQUISITION_FAILED;
        operation.health = YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED;
        snprintf(operation.reason, sizeof(operation.reason), "supervisor-start-failed");
        snprintf(operation.result, sizeof(operation.result), "not-started");
        (void)yvex_source_acquisition_operation_publish(report->operation_path,
                                                        &operation, err);
    }
    return rc;
}

int model_acquisition_status_read(
    const yvex_model_download_report *report,
    yvex_source_acquisition_operation *operation, int reconcile,
    yvex_error *err)
{
    yvex_source_acquisition_health before;
    yvex_source_acquisition_lifecycle lifecycle;
    int rc = yvex_source_acquisition_operation_read(report->operation_path,
                                                     operation, err);
    if (rc != YVEX_OK || !reconcile) return rc;
    if (!model_acquisition_operation_matches_report(operation, report, NULL)) {
        yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.status",
                       "operation target identity mismatch");
        return YVEX_ERR_STATE;
    }
    before = operation->health;
    lifecycle = operation->lifecycle;
    if (yvex_source_acquisition_process_matches(&operation->supervisor))
        return YVEX_OK;
    rc = yvex_source_acquisition_reconcile(operation, acquisition_now(),
                                            operation->stall_window_seconds, err);
    if (rc == YVEX_OK && (before != operation->health ||
                          lifecycle != operation->lifecycle))
        rc = yvex_source_acquisition_operation_publish(report->operation_path,
                                                        operation, err);
    return rc;
}

static void acquisition_json_fact(const char *name,
                                  yvex_source_acquisition_u64 fact)
{
    yvex_cli_out_writef(stdout, ",\"%s\":", name);
    if (fact.known) yvex_cli_out_writef(stdout, "%llu", fact.value);
    else yvex_cli_out_fputs("null", stdout);
}

void model_acquisition_status_render(
    const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report,
    const yvex_source_acquisition_operation *operation)
{
    if (options && options->output_mode == YVEX_MODELS_OUTPUT_JSON) {
        yvex_cli_out_fputs("{\"schema\":\"yvex.model.acquisition.status.v2\",\"operation_id\":", stdout);
        yvex_cli_out_json_string(stdout, operation->operation_id);
        yvex_cli_out_writef(stdout,
                            ",\"generation\":%llu,\"stall_window_seconds\":%llu,"
                            "\"provider\":",
                            operation->generation, operation->stall_window_seconds);
        yvex_cli_out_json_string(stdout, operation->provider);
        yvex_cli_out_fputs(",\"repository\":", stdout);
        yvex_cli_out_json_string(stdout, operation->repository);
        yvex_cli_out_fputs(",\"target_id\":", stdout);
        yvex_cli_out_json_string(stdout, report ? report->target_id : "");
        yvex_cli_out_fputs(",\"family\":", stdout);
        yvex_cli_out_json_string(stdout, report ? report->family : "");
        yvex_cli_out_fputs(",\"revision\":", stdout);
        yvex_cli_out_json_string(stdout, operation->revision);
        yvex_cli_out_fputs(",\"selection_identity\":", stdout);
        yvex_cli_out_json_string(stdout, operation->selection_identity);
        yvex_cli_out_fputs(",\"lifecycle\":", stdout);
        yvex_cli_out_json_string(stdout,
            yvex_source_acquisition_lifecycle_name(operation->lifecycle));
        yvex_cli_out_fputs(",\"health\":", stdout);
        yvex_cli_out_json_string(stdout,
            yvex_source_acquisition_health_name(operation->health));
        acquisition_json_fact("expected_bytes", operation->progress.expected_bytes);
        acquisition_json_fact("committed_bytes", operation->progress.committed_bytes);
        acquisition_json_fact("provider_activity_bytes",
                              operation->progress.provider_activity_bytes);
        acquisition_json_fact("inflight_selected_bytes",
                              operation->progress.inflight_selected_bytes);
        acquisition_json_fact("selected_files", operation->progress.selected_files);
        acquisition_json_fact("completed_files", operation->progress.completed_files);
        acquisition_json_fact("incomplete_files", operation->progress.incomplete_files);
        acquisition_json_fact("selected_shards", operation->progress.selected_shards);
        acquisition_json_fact("completed_shards", operation->progress.completed_shards);
        acquisition_json_fact("selected_sidecars", operation->progress.selected_sidecars);
        acquisition_json_fact("completed_sidecars", operation->progress.completed_sidecars);
        acquisition_json_fact("provider_partial_objects",
                              operation->progress.provider_partial_objects);
        acquisition_json_fact("provider_lock_objects",
                              operation->progress.provider_lock_objects);
        acquisition_json_fact("provider_activity_current_bytes_per_second",
                              operation->progress.provider_activity_current_bytes_per_second);
        acquisition_json_fact("provider_activity_rolling_bytes_per_second",
                              operation->progress.provider_activity_rolling_bytes_per_second);
        acquisition_json_fact("current_rate_bytes_per_second",
                              operation->progress.current_rate_bytes_per_second);
        acquisition_json_fact("rolling_rate_bytes_per_second",
                              operation->progress.rolling_rate_bytes_per_second);
        acquisition_json_fact("last_progress_unix",
                              operation->progress.last_progress_unix);
        acquisition_json_fact("last_provider_event_unix",
                              operation->progress.last_provider_event_unix);
        acquisition_json_fact("provider_event_sequence",
                              operation->progress.provider_event_sequence);
        acquisition_json_fact("retry_count", operation->progress.retry_count);
        yvex_cli_out_fputs(",\"current_object\":", stdout);
        if (operation->progress.current_object[0])
            yvex_cli_out_json_string(stdout, operation->progress.current_object);
        else
            yvex_cli_out_fputs("null", stdout);
        yvex_cli_out_writef(stdout,
            ",\"supervisor_pid\":%lld,\"provider_pid\":%lld,"
            "\"active\":%s,\"stop_available\":%s,\"resume_available\":%s,"
            "\"bytes\":%llu,\"partial_files\":%llu,\"reason\":",
            (long long)operation->supervisor.pid,
            (long long)operation->provider_process.pid,
            yvex_source_acquisition_process_matches(&operation->supervisor) ||
                    yvex_source_acquisition_process_matches(&operation->provider_process)
                ? "true" : "false",
            yvex_source_acquisition_process_matches(&operation->supervisor) ||
                    yvex_source_acquisition_process_matches(&operation->provider_process)
                ? "true" : "false",
            operation->lifecycle == YVEX_SOURCE_ACQUISITION_STOPPED ||
                    operation->lifecycle == YVEX_SOURCE_ACQUISITION_FAILED
                ? "true" : "false",
            operation->progress.committed_bytes.known
                ? operation->progress.committed_bytes.value : 0ull,
            operation->progress.provider_partial_objects.known
                ? operation->progress.provider_partial_objects.value : 0ull);
        yvex_cli_out_json_string(stdout, operation->reason);
        yvex_cli_out_fputs(",\"result\":", stdout);
        if (operation->result[0]) yvex_cli_out_json_string(stdout, operation->result);
        else yvex_cli_out_fputs("null", stdout);
        yvex_cli_out_fputs(",\"legacy\":{\"active_semantics\":\"process-liveness\","
                           "\"bytes_semantics\":\"committed-canonical-bytes\","
                           "\"partial_files_semantics\":\"provider-internal-objects\"}}\n",
                           stdout);
        return;
    }
    yvex_cli_out_writef(stdout, "acquisition %s\n", operation->operation_id);
    yvex_cli_out_writef(stdout, "generation  %llu\n", operation->generation);
    yvex_cli_out_writef(stdout, "provider    %s\n", operation->provider);
    if (report) {
        yvex_cli_out_writef(stdout, "target_id   %s\n", report->target_id);
        yvex_cli_out_writef(stdout, "family      %s\n", report->family);
    }
    yvex_cli_out_writef(stdout, "repository  %s\n", operation->repository);
    yvex_cli_out_writef(stdout, "revision    %s\n", operation->revision);
    yvex_cli_out_writef(stdout, "state       %s\n",
                        yvex_source_acquisition_lifecycle_name(operation->lifecycle));
    yvex_cli_out_writef(stdout, "health      %s\n",
                        yvex_source_acquisition_health_name(operation->health));
    if (operation->progress.completed_files.known) {
        yvex_cli_out_writef(stdout, "files       %llu", operation->progress.completed_files.value);
        if (operation->progress.selected_files.known)
            yvex_cli_out_writef(stdout, " / %llu", operation->progress.selected_files.value);
        yvex_cli_out_fputs("\n", stdout);
    } else yvex_cli_out_fputs("files       unknown\n", stdout);
    if (operation->progress.completed_shards.known) {
        yvex_cli_out_writef(stdout, "shards      %llu", operation->progress.completed_shards.value);
        if (operation->progress.selected_shards.known)
            yvex_cli_out_writef(stdout, " / %llu", operation->progress.selected_shards.value);
        yvex_cli_out_fputs("\n", stdout);
    } else yvex_cli_out_fputs("shards      unknown\n", stdout);
    if (operation->progress.committed_bytes.known)
        yvex_cli_out_writef(stdout, "committed   %llu bytes\n",
                            operation->progress.committed_bytes.value);
    else yvex_cli_out_fputs("committed   unknown\n", stdout);
    if (operation->progress.expected_bytes.known)
        yvex_cli_out_writef(stdout, "planned     %llu payload bytes\n",
                            operation->progress.expected_bytes.value);
    else yvex_cli_out_fputs("planned     unknown\n", stdout);
    yvex_cli_out_writef(stdout, "in-flight   %s\n",
                        operation->progress.inflight_selected_bytes.known
                            ? "known; see --output json" : "unknown");
    if (operation->progress.provider_partial_objects.known)
        yvex_cli_out_writef(stdout, "provider partial objects %llu\n",
                            operation->progress.provider_partial_objects.value);
    else yvex_cli_out_fputs("provider partial objects unknown\n", stdout);
    if (operation->progress.provider_activity_current_bytes_per_second.known)
        yvex_cli_out_writef(stdout, "provider write activity %llu B/s\n",
                            operation->progress.provider_activity_current_bytes_per_second.value);
    else yvex_cli_out_fputs("provider write activity unknown\n", stdout);
    if (operation->progress.last_progress_unix.known)
        yvex_cli_out_writef(stdout, "last progress unix %llu\n",
                            operation->progress.last_progress_unix.value);
    else yvex_cli_out_fputs("last progress unknown\n", stdout);
    if (operation->progress.retry_count.known)
        yvex_cli_out_writef(stdout, "retries     %llu\n",
                            operation->progress.retry_count.value);
    else yvex_cli_out_fputs("retries     unknown\n", stdout);
    yvex_cli_out_writef(stdout, "reason      %s\n", operation->reason);
}

static int acquisition_attach_render(
    const yvex_cli_models_download_options *options,
    const yvex_source_acquisition_operation *operation, int live)
{
    if (options->progress_mode == YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) return 0;
    if (live) {
        unsigned int width = yvex_cli_terminal_width(stdout);
        yvex_cli_out_writef(stdout, "\r\033[2K%s · %s",
                            yvex_source_acquisition_lifecycle_name(operation->lifecycle),
                            yvex_source_acquisition_health_name(operation->health));
        if (width == 0u || width >= 60u) {
            if (operation->progress.completed_files.known)
                yvex_cli_out_writef(stdout, " · files %llu",
                                    operation->progress.completed_files.value);
            else yvex_cli_out_fputs(" · files unknown", stdout);
        }
        if (width == 0u || width >= 92u) {
            if (operation->progress.committed_bytes.known)
                yvex_cli_out_writef(stdout, " · committed %llu",
                                    operation->progress.committed_bytes.value);
            else yvex_cli_out_fputs(" · committed unknown", stdout);
        }
        fflush(stdout);
    } else {
        yvex_cli_out_writef(stdout, "acquisition: state=%s health=%s",
            yvex_source_acquisition_lifecycle_name(operation->lifecycle),
            yvex_source_acquisition_health_name(operation->health));
        if (operation->progress.completed_files.known)
            yvex_cli_out_writef(stdout, " files=%llu",
                                operation->progress.completed_files.value);
        else yvex_cli_out_fputs(" files=unknown", stdout);
        if (operation->progress.committed_bytes.known)
            yvex_cli_out_writef(stdout, " committed=%llu",
                                operation->progress.committed_bytes.value);
        else yvex_cli_out_fputs(" committed=unknown", stdout);
        if (operation->progress.provider_activity_current_bytes_per_second.known)
            yvex_cli_out_writef(stdout, " provider_write_activity=%lluB/s\n",
                operation->progress.provider_activity_current_bytes_per_second.value);
        else yvex_cli_out_fputs(" provider_write_activity=unknown\n", stdout);
        fflush(stdout);
    }
    return 0;
}

static int acquisition_attach_audit(const char *path, yvex_error *err)
{
    unsigned char buffer[8192];
    FILE *stream;
    size_t count;
    if (!path || !path[0]) return YVEX_OK;
    stream = fopen(path, "rb");
    if (!stream) {
        yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.audit",
                       "cannot open the supervisor audit record");
        return YVEX_ERR_IO;
    }
    while ((count = fread(buffer, 1u, sizeof(buffer), stream)) > 0u) {
        if (fwrite(buffer, 1u, count, stdout) != count) {
            fclose(stream);
            yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.audit",
                           "cannot project the supervisor audit record");
            return YVEX_ERR_IO;
        }
    }
    if (ferror(stream) || fclose(stream) != 0) {
        yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.audit",
                       "cannot read the supervisor audit record");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int model_acquisition_attach(
    const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report, yvex_error *err)
{
    yvex_source_acquisition_operation operation, previous;
    int first = 1, live = options->progress_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
        isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    memset(&previous, 0, sizeof(previous));
    for (;;) {
        int rc = model_acquisition_status_read(report, &operation, 1, err);
        if (rc != YVEX_OK) return rc;
        if (first || operation.lifecycle != previous.lifecycle ||
            operation.health != previous.health ||
            operation.progress.committed_bytes.value !=
                previous.progress.committed_bytes.value ||
            operation.progress.provider_activity_bytes.value !=
                previous.progress.provider_activity_bytes.value) {
            (void)acquisition_attach_render(options, &operation, live);
            previous = operation;
            first = 0;
        }
        if (yvex_source_acquisition_terminal(operation.lifecycle)) break;
        (void)poll(NULL, 0, 250);
    }
    if (live) yvex_cli_out_fputs("\n", stdout);
    if ((options->output_mode == YVEX_MODELS_OUTPUT_AUDIT ||
         options->output_mode == YVEX_MODELS_OUTPUT_JSON) &&
        acquisition_attach_audit(report->supervisor_log_path, err) != YVEX_OK)
        return yvex_error_code(err);
    if (operation.lifecycle == YVEX_SOURCE_ACQUISITION_COMPLETE) return YVEX_OK;
    yvex_error_setf(err, YVEX_ERR_STATE, "source.acquisition.attach",
                    "acquisition ended in %s: %s",
                    yvex_source_acquisition_lifecycle_name(operation.lifecycle),
                    operation.reason);
    return YVEX_ERR_STATE;
}

int model_acquisition_stop_supervisor(
    const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report,
    yvex_source_acquisition_operation *operation, yvex_error *err)
{
    unsigned long long deadline;
    if (!model_acquisition_operation_matches_report(operation, report, NULL)) {
        yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.stop",
                       "operation target identity mismatch");
        return YVEX_ERR_STATE;
    }
    if (yvex_source_acquisition_process_matches(&operation->provider_process) &&
        !yvex_source_acquisition_process_matches(&operation->supervisor)) {
        yvex_source_acquisition_process provider = operation->provider_process;
        if (getpgid(operation->provider_process.pid) !=
                operation->provider_process.process_group ||
            kill(-operation->provider_process.process_group, SIGTERM) != 0) {
            yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.stop",
                           "authenticated orphan provider signal failed");
            return YVEX_ERR_IO;
        }
        deadline = acquisition_now() + (options && options->timeout_seconds
            ? options->timeout_seconds : 10ull);
        while (yvex_source_acquisition_process_matches(&provider) &&
               acquisition_now() <= deadline)
            (void)poll(NULL, 0, 100);
        if (yvex_source_acquisition_process_matches(&provider) &&
            options && options->force) {
            if (getpgid(provider.pid) != provider.process_group ||
                kill(-provider.process_group, SIGKILL) != 0) {
                yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.stop",
                               "authenticated orphan provider force-stop failed");
                return YVEX_ERR_IO;
            }
            deadline = acquisition_now() + options->timeout_seconds;
            while (yvex_source_acquisition_process_matches(&provider) &&
                   acquisition_now() <= deadline)
                (void)poll(NULL, 0, 100);
        }
        if (yvex_source_acquisition_process_matches(&provider)) {
            yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.stop",
                           "orphan provider did not terminate within the bounded wait");
            return YVEX_ERR_STATE;
        }
        operation->lifecycle = YVEX_SOURCE_ACQUISITION_STOPPED;
        operation->health = YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE;
        snprintf(operation->reason, sizeof(operation->reason), "orphan-provider-stop");
        snprintf(operation->result, sizeof(operation->result), "interrupted-resumable");
        memset(&operation->provider_process, 0, sizeof(operation->provider_process));
        return yvex_source_acquisition_operation_publish(report->operation_path,
                                                          operation, err);
    }
    if (yvex_source_acquisition_terminal(operation->lifecycle)) return YVEX_OK;
    if (!yvex_source_acquisition_process_matches(&operation->supervisor)) {
        (void)yvex_source_acquisition_reconcile(operation, acquisition_now(),
                                                operation->stall_window_seconds, err);
        return yvex_source_acquisition_operation_publish(report->operation_path,
                                                         operation, err);
    }
    if (kill(operation->supervisor.pid, SIGTERM) != 0 && errno != ESRCH) {
        yvex_error_set(err, YVEX_ERR_IO, "source.acquisition.stop",
                       "authenticated supervisor signal failed");
        return YVEX_ERR_IO;
    }
    deadline = acquisition_now() + (options && options->timeout_seconds
        ? options->timeout_seconds : 10ull);
    while (acquisition_now() <= deadline) {
        yvex_source_acquisition_operation observed;
        yvex_error status_error;
        yvex_error_clear(&status_error);
        if (model_acquisition_status_read(report, &observed, 1, &status_error) == YVEX_OK) {
            *operation = observed;
            if (yvex_source_acquisition_terminal(operation->lifecycle)) return YVEX_OK;
        }
        (void)poll(NULL, 0, 100);
    }
    yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.stop",
                   "supervisor accepted stop but did not publish terminal state in time");
    return YVEX_ERR_STATE;
}
