/*
 * yvex_models_download_control_surface.c - models download control surface.
 * Owner: src/cli/model_artifacts
 * Owns: download status/stop/resume/cleanup command routing.
 * Does not own: domain registry storage, artifact emission, runtime generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing control command behavior.
 * Boundary: download control state is not runtime support.
 */
#include "yvex_models_download_surface.h"
#include "yvex_models_surface.h"

static int model_download_read_receipt_status(const char *path,
                                              char *status,
                                              size_t status_cap)
{
    char buf[8192];

    if (status && status_cap > 0u) status[0] = '\0';
    if (!model_download_read_small_file(path, buf, sizeof(buf))) return 0;
    if (!model_download_json_string_field(buf, "status", status, status_cap)) {
        if (status && status_cap > 0u) snprintf(status, status_cap, "unknown");
    }
    return 1;
}

static int model_download_pid_alive(pid_t pid)
{
    if (pid <= 0) return 0;
    if (kill(pid, 0) == 0) return 1;
    return errno == EPERM;
}

static int model_download_pgid_alive(pid_t pgid)
{
    if (pgid <= 0) return 0;
    if (kill(-pgid, 0) == 0) return 1;
    return errno == EPERM;
}

static int model_download_read_active_process(const char *active_path,
                                              pid_t *pid_out,
                                              pid_t *pgid_out)
{
    char buf[8192];
    long long pid;
    long long pgid;

    if (pid_out) *pid_out = -1;
    if (pgid_out) *pgid_out = -1;
    if (!model_download_read_small_file(active_path, buf, sizeof(buf))) return 0;
    pid = model_download_json_i64_field(buf, "provider_pid");
    pgid = model_download_json_i64_field(buf, "provider_pgid");
    if (pid_out) *pid_out = (pid_t)pid;
    if (pgid_out) *pgid_out = (pid_t)pgid;
    return 1;
}

static void model_download_find_provider_processes(const char *local_source_dir,
                                                   yvex_model_download_process_match *match)
{
    DIR *proc;
    struct dirent *ent;

    if (!match) return;
    memset(match, 0, sizeof(*match));
    match->first_pid = -1;
    match->first_pgid = -1;
    if (!local_source_dir || !local_source_dir[0]) return;
    proc = opendir("/proc");
    if (!proc) return;
    while ((ent = readdir(proc)) != NULL) {
        char *endptr = NULL;
        long pid_long;
        char cmdline_path[128];
        char cmdline[8192];
        int fd;
        ssize_t got;
        ssize_t i;
        pid_t pid;
        pid_t pgid;

        if (!isdigit((unsigned char)ent->d_name[0])) continue;
        pid_long = strtol(ent->d_name, &endptr, 10);
        if (!endptr || *endptr || pid_long <= 0) continue;
        pid = (pid_t)pid_long;
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%ld/cmdline", pid_long);
        fd = open(cmdline_path, O_RDONLY);
        if (fd < 0) continue;
        got = read(fd, cmdline, sizeof(cmdline) - 1u);
        close(fd);
        if (got <= 0) continue;
        for (i = 0; i < got; ++i) {
            if (cmdline[i] == '\0') cmdline[i] = ' ';
        }
        cmdline[got] = '\0';
        if (!strstr(cmdline, local_source_dir)) continue;
        if (!strstr(cmdline, "hf") && !strstr(cmdline, "huggingface") &&
            !strstr(cmdline, "gh") && !strstr(cmdline, "fake-hf") &&
            !strstr(cmdline, "fake-gh")) {
            continue;
        }
        pgid = getpgid(pid);
        match->count++;
        if (match->first_pid <= 0) {
            match->first_pid = pid;
            match->first_pgid = pgid > 0 ? pgid : pid;
        }
    }
    closedir(proc);
}

static int model_download_resolve_for_control(int arg_count,
                                              char **args,
                                              int start_index,
                                              yvex_cli_models_download_options *options,
                                              yvex_model_download_report *report,
                                              yvex_operator_paths *operator_paths,
                                              yvex_account_provider *provider_kind,
                                              yvex_error *err)
{
    const yvex_model_download_catalog_row *row = NULL;
    yvex_paths paths;
    char hf_family_dir[YVEX_PATH_CAP];
    char provider_root_dir[YVEX_PATH_CAP];
    char github_repo_dir[YVEX_PATH_CAP];
    char reports_family_dir[YVEX_PATH_CAP];
    char registry_family_dir[YVEX_PATH_CAP];
    char logs_dir[YVEX_PATH_CAP];
    char file_name[256];
    const char *target_id;
    const char *family;
    const char *repo_id;
    const char *local_name;
    const char *revision;
    const char *provider_name;
    yvex_model_download_resolved_target resolved;
    int resolved_dynamic = 0;
    int rc;

    rc = parse_models_download_options_from(arg_count, args, start_index, options);
    if (rc != 0) return rc;
    model_download_report_init(report);
    memset(&paths, 0, sizeof(paths));
    memset(operator_paths, 0, sizeof(*operator_paths));
    model_download_timestamp(report->created_at, sizeof(report->created_at));
    if (!yvex_account_provider_from_name(options->provider, provider_kind)) {
        yvex_cli_out_writef(stderr, "yvex: models download --provider requires hf|huggingface|gh|github\n");
        return 2;
    }
    provider_name = yvex_account_provider_name(*provider_kind);
    rc = yvex_operator_paths_resolve(&paths, options->models_root, operator_paths, err);
    if (rc != YVEX_OK) return rc;

    if (options->repo) {
        target_id = options->name;
        family = *provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB ? "github" : options->family;
        repo_id = options->repo;
        local_name = options->name;
        revision = *provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB
            ? (options->release ? options->release : "latest")
            : (options->revision ? options->revision : "main");
    } else {
        memset(&resolved, 0, sizeof(resolved));
        resolved_dynamic = model_download_resolve_downloaded_target(options->target,
                                                                    operator_paths,
                                                                    &resolved,
                                                                    err);
        if (resolved_dynamic) {
            target_id = resolved.target_id;
            family = resolved.family;
            repo_id = resolved.repo_id;
            local_name = resolved.local_name;
            revision = options->revision ? options->revision : resolved.revision;
            if (!yvex_account_provider_from_name(resolved.provider, provider_kind)) {
                *provider_kind = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
            }
            provider_name = yvex_account_provider_name(*provider_kind);
        } else if ((row = model_download_find_catalog(options->target)) != NULL) {
            target_id = row->target_id;
            family = row->family;
            repo_id = row->repo_id;
            local_name = row->local_name;
            revision = options->revision ? options->revision : row->revision_default;
            if (!yvex_account_provider_from_name(row->provider, provider_kind)) {
                *provider_kind = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
            }
            provider_name = yvex_account_provider_name(*provider_kind);
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown models download target: %s\n",
                    options->target ? options->target : "");
            return 2;
        }
    }
    snprintf(report->target_id, sizeof(report->target_id), "%s", target_id);
    snprintf(report->family, sizeof(report->family), "%s", family);
    snprintf(report->provider, sizeof(report->provider), "%s", provider_name);
    snprintf(report->repo_id, sizeof(report->repo_id), "%s", repo_id);
    snprintf(report->revision, sizeof(report->revision), "%s", revision);
    snprintf(report->local_name, sizeof(report->local_name), "%s", local_name);
    snprintf(report->token_env_name, sizeof(report->token_env_name), "%s",
             options->token_env ? options->token_env : yvex_account_default_token_env(*provider_kind));
    snprintf(report->models_root, sizeof(report->models_root), "%s", operator_paths->models_root);
    snprintf(report->models_root_source, sizeof(report->models_root_source), "%s",
             operator_paths->models_root_source);
    if (*provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB) {
        rc = path_join2(provider_root_dir, sizeof(provider_root_dir), operator_paths->models_root,
                        "github", err, "models_download_control");
        if (rc == YVEX_OK) rc = path_join2(github_repo_dir, sizeof(github_repo_dir),
                                           provider_root_dir, repo_id, err,
                                           "models_download_control");
        if (rc == YVEX_OK) rc = path_join2(report->local_source_dir,
                                           sizeof(report->local_source_dir),
                                           github_repo_dir, revision, err,
                                           "models_download_control");
    } else {
        rc = path_join2(hf_family_dir, sizeof(hf_family_dir), operator_paths->hf_root,
                        family, err, "models_download_control");
        if (rc == YVEX_OK) rc = path_join2(report->local_source_dir,
                                           sizeof(report->local_source_dir),
                                           hf_family_dir, local_name, err,
                                           "models_download_control");
    }
    if (rc == YVEX_OK && resolved_dynamic && resolved.local_source_dir[0]) {
        snprintf(report->local_source_dir, sizeof(report->local_source_dir), "%s",
                 resolved.local_source_dir);
    }
    if (rc == YVEX_OK) rc = path_join2(reports_family_dir, sizeof(reports_family_dir),
                                       operator_paths->reports_root, family, err,
                                       "models_download_control");
    if (rc == YVEX_OK) rc = path_join2(registry_family_dir, sizeof(registry_family_dir),
                                       operator_paths->registry_root, family, err,
                                       "models_download_control");
    if (rc == YVEX_OK) rc = path_join2(logs_dir, sizeof(logs_dir), operator_paths->models_root,
                                       "logs", err, "models_download_control");
    if (rc != YVEX_OK) return rc;
    snprintf(report->reports_dir, sizeof(report->reports_dir), "%s", reports_family_dir);
    snprintf(report->registry_dir, sizeof(report->registry_dir), "%s", registry_family_dir);
    snprintf(file_name, sizeof(file_name), "%s.download.receipt", target_id);
    rc = path_join2(report->receipt_path, sizeof(report->receipt_path),
                    reports_family_dir, file_name, err, "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.download.active.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->active_receipt_path,
                                       sizeof(report->active_receipt_path),
                                       reports_family_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.download.last.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->last_receipt_path,
                                       sizeof(report->last_receipt_path),
                                       reports_family_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.download-report.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->download_report_path,
                                       sizeof(report->download_report_path),
                                       reports_family_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.source-manifest.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->manifest_path,
                                       sizeof(report->manifest_path),
                                       reports_family_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.native-inventory.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->native_inventory_path,
                                       sizeof(report->native_inventory_path),
                                       reports_family_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.download.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->registry_path,
                                       sizeof(report->registry_path),
                                       registry_family_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.download.stdout.log", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->stdout_log_path,
                                       sizeof(report->stdout_log_path),
                                       logs_dir, file_name, err,
                                       "models_download_control");
    snprintf(file_name, sizeof(file_name), "%s.download.stderr.log", target_id);
    if (rc == YVEX_OK) rc = path_join2(report->stderr_log_path,
                                       sizeof(report->stderr_log_path),
                                       logs_dir, file_name, err,
                                       "models_download_control");
    if (rc == YVEX_OK && resolved_dynamic) {
        if (resolved.registry_path[0]) {
            snprintf(report->registry_path, sizeof(report->registry_path), "%s",
                     resolved.registry_path);
        }
        if (resolved.download_report_path[0]) {
            snprintf(report->download_report_path, sizeof(report->download_report_path), "%s",
                     resolved.download_report_path);
        }
        if (resolved.manifest_path[0]) {
            snprintf(report->manifest_path, sizeof(report->manifest_path), "%s",
                     resolved.manifest_path);
        }
        if (resolved.native_inventory_path[0]) {
            snprintf(report->native_inventory_path, sizeof(report->native_inventory_path), "%s",
                     resolved.native_inventory_path);
        }
    }
    return rc;
}

static void model_download_print_status_report(
    const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report,
    const yvex_model_download_process_match *match,
    const yvex_model_download_safetensors_check *safe_check,
    int active_receipt_present,
    int active_receipt_stale,
    int last_receipt_present,
    const char *last_receipt_status,
    pid_t provider_pid,
    pid_t provider_pgid)
{
    int provider_alive = provider_pid > 0 && model_download_pid_alive(provider_pid);
    int process_alive = provider_alive || (match && match->count > 0u);
    int resume_available = !process_alive &&
                           report->source_scan.file_count > 0ull &&
                           strcmp(safe_check->status, "ok") != 0;
    int stop_available = process_alive;
    unsigned long long i;
    char bytes_text[32];
    char largest_text[32];
    char largest_name[64];

    if (options && options->output_mode == YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "models: download status\n");
        yvex_cli_out_writef(stdout, "target_id: %s\n", report->target_id);
        yvex_cli_out_writef(stdout, "family: %s\n", report->family);
        yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
        yvex_cli_out_writef(stdout, "repo_id: %s\n", report->repo_id);
        yvex_cli_out_writef(stdout, "revision: %s\n", report->revision);
        yvex_cli_out_writef(stdout, "local_source_dir: %s\n", report->local_source_dir);
        yvex_cli_out_writef(stdout, "receipt_path: %s\n", report->receipt_path);
        yvex_cli_out_writef(stdout, "active_receipt_path: %s\n", report->active_receipt_path);
        yvex_cli_out_writef(stdout, "receipt_status: %s\n",
               active_receipt_present ? (active_receipt_stale ? "stale-active-receipt" : "active") : "none");
        yvex_cli_out_writef(stdout, "last_receipt_path: %s\n", report->last_receipt_path);
        yvex_cli_out_writef(stdout, "last_receipt_status: %s\n",
               last_receipt_present ? (last_receipt_status && last_receipt_status[0]
                                       ? last_receipt_status : "unknown") : "none");
        yvex_cli_out_writef(stdout, "provider_pid: %lld\n", (long long)provider_pid);
        yvex_cli_out_writef(stdout, "provider_pgid: %lld\n", (long long)provider_pgid);
        yvex_cli_out_writef(stdout, "provider_process_alive: %s\n", process_alive ? "true" : "false");
        yvex_cli_out_writef(stdout, "matching_provider_process_count: %u\n", match ? match->count : 0u);
        yvex_cli_out_writef(stdout, "lock_count: %llu\n", report->source_scan.lock_count);
        for (i = 0; i < report->source_scan.lock_count && i < YVEX_MODEL_DOWNLOAD_PATTERN_CAP; ++i) {
            char abs_lock[YVEX_PATH_CAP];
            yvex_error err;
            yvex_error_clear(&err);
            if (path_join2(abs_lock, sizeof(abs_lock), report->local_source_dir,
                           report->source_scan.lock_paths[i], &err,
                           "models_download_status") == YVEX_OK) {
                yvex_cli_out_writef(stdout, "lock.%llu.path: %s\n", i, abs_lock);
            }
            yvex_cli_out_writef(stdout, "lock.%llu.age_seconds: %llu\n", i, report->source_scan.lock_age_seconds[i]);
        }
        yvex_cli_out_writef(stdout, "source_file_count: %llu\n", report->source_scan.file_count);
        yvex_cli_out_writef(stdout, "safetensors_count: %llu\n", report->source_scan.safetensors_count);
        yvex_cli_out_writef(stdout, "total_regular_file_bytes: %llu\n", report->source_scan.total_regular_file_bytes);
        yvex_cli_out_writef(stdout, "largest_file_name: %s\n",
               report->source_scan.largest_file_name[0] ? report->source_scan.largest_file_name : "none");
        yvex_cli_out_writef(stdout, "largest_file_bytes: %llu\n", report->source_scan.largest_file_bytes);
        yvex_cli_out_writef(stdout, "partial_file_count: %llu\n", report->source_scan.partial_file_count);
        yvex_cli_out_writef(stdout, "cache_file_count: %llu\n", report->source_scan.cache_file_count);
        yvex_cli_out_writef(stdout, "stdout_log: %s\n", report->stdout_log_path);
        yvex_cli_out_writef(stdout, "stderr_log: %s\n", report->stderr_log_path);
        yvex_cli_out_writef(stdout, "source_manifest_path: %s\n", report->manifest_path);
        yvex_cli_out_writef(stdout, "native_inventory_path: %s\n", report->native_inventory_path);
        yvex_cli_out_writef(stdout, "safetensors_header_checked: %s\n", safe_check->checked ? "true" : "false");
        yvex_cli_out_writef(stdout, "safetensors_size_status: %s\n", safe_check->status);
        yvex_cli_out_writef(stdout, "resume_available: %s\n", resume_available ? "true" : "false");
        yvex_cli_out_writef(stdout, "stop_available: %s\n", stop_available ? "true" : "false");
        yvex_cli_out_writef(stdout, "runtime_ready: false\n");
        yvex_cli_out_writef(stdout, "generation: unsupported\n");
        yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
        yvex_cli_out_writef(stdout, "status: model-download-status\n");
        return;
    }

    model_download_format_bytes(bytes_text, sizeof(bytes_text),
                                report->source_scan.total_regular_file_bytes);
    model_download_format_bytes(largest_text, sizeof(largest_text),
                                report->source_scan.largest_file_bytes);
    model_download_short_file_name(largest_name, sizeof(largest_name),
                                   report->source_scan.largest_file_name);

    yvex_cli_out_writef(stdout, "model-download-status: target=%s\n", report->target_id);
    yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
    yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
    yvex_cli_out_writef(stdout, "source: %s\n", report->local_source_dir);
    yvex_cli_out_writef(stdout, "process: %s\n", process_alive ? "active" : "none");
    yvex_cli_out_writef(stdout, "last_status: %s\n",
           last_receipt_present ? (last_receipt_status && last_receipt_status[0]
                                   ? last_receipt_status : "unknown") : "none");
    yvex_cli_out_writef(stdout, "locks: %llu\n", report->source_scan.lock_count);
    yvex_cli_out_writef(stdout, "files: %llu partial=%llu safetensors=%llu bytes=%s\n",
           report->source_scan.file_count,
           report->source_scan.partial_file_count,
           report->source_scan.safetensors_count,
           bytes_text);
    yvex_cli_out_writef(stdout, "largest: %s (%s)\n", largest_name, largest_text);
    yvex_cli_out_writef(stdout, "safetensors_size_status: %s\n", safe_check->status);
    yvex_cli_out_writef(stdout, "resume: %s\n", resume_available ? "available" : "not-needed");
    yvex_cli_out_writef(stdout, "stop: %s\n", stop_available ? "available" : "not-needed");
    yvex_cli_out_writef(stdout, "boundary: source state only, runtime unsupported\n");
    yvex_cli_out_writef(stdout, "status: model-download-status\n");
}

static int command_models_download_status(int arg_count, char **args)
{
    yvex_cli_models_download_options options;
    yvex_model_download_report report;
    yvex_operator_paths operator_paths;
    yvex_account_provider provider_kind;
    yvex_model_download_process_match match;
    yvex_model_download_safetensors_check safe_check;
    yvex_error err;
    pid_t receipt_pid = -1;
    pid_t receipt_pgid = -1;
    int active_present;
    int active_stale;
    int last_present;
    char last_status[64];
    int rc;

    yvex_error_clear(&err);
    rc = model_download_resolve_for_control(arg_count, args, 4, &options, &report,
                                            &operator_paths, &provider_kind, &err);
    if (rc == 1) {
        yvex_model_artifacts_surface_models_help(stdout);
        return 0;
    }
    if (rc != YVEX_OK) return rc == 2 ? 2 : print_yvex_error(&err, exit_for_status(rc));
    (void)operator_paths;
    (void)provider_kind;
    active_present = model_download_read_active_process(report.active_receipt_path,
                                                        &receipt_pid,
                                                        &receipt_pgid);
    active_stale = active_present &&
                   !(model_download_pid_alive(receipt_pid) ||
                     model_download_pgid_alive(receipt_pgid));
    last_present = model_download_read_receipt_status(report.last_receipt_path,
                                                      last_status,
                                                      sizeof(last_status));
    yvex_error_clear(&err);
    (void)model_download_scan_source(report.local_source_dir, &report.source_scan, &err);
    yvex_error_clear(&err);
    (void)model_download_check_safetensors_source(report.local_source_dir, &safe_check, &err);
    model_download_find_provider_processes(report.local_source_dir, &match);
    model_download_print_status_report(&options, &report, &match, &safe_check,
                                       active_present, active_stale,
                                       last_present, last_status,
                                       receipt_pid, receipt_pgid);
    return 0;
}

static int model_download_wait_group_gone(pid_t pgid, unsigned long long timeout_seconds)
{
    time_t start = time(NULL);

    while (model_download_pgid_alive(pgid)) {
        time_t now = time(NULL);
        if (start != (time_t)-1 && now != (time_t)-1 &&
            now >= start + (time_t)timeout_seconds) {
            return 0;
        }
        (void)poll(NULL, 0, 100);
    }
    return 1;
}

static void model_download_wait_path_gone(const char *path,
                                          unsigned long long timeout_seconds)
{
    time_t start = time(NULL);

    if (!path || !path[0]) return;
    while (access(path, F_OK) == 0) {
        time_t now = time(NULL);
        if (start != (time_t)-1 && now != (time_t)-1 &&
            now >= start + (time_t)timeout_seconds) {
            return;
        }
        (void)poll(NULL, 0, 100);
    }
}

static int command_models_download_stop(int arg_count, char **args)
{
    yvex_cli_models_download_options options;
    yvex_model_download_report report;
    yvex_operator_paths operator_paths;
    yvex_account_provider provider_kind;
    yvex_model_download_process_match match;
    yvex_error err;
    pid_t pid = -1;
    pid_t pgid = -1;
    int active_present;
    int signaled = 0;
    int killed = 0;
    int stopped = 0;
    int rc;

    yvex_error_clear(&err);
    rc = model_download_resolve_for_control(arg_count, args, 4, &options, &report,
                                            &operator_paths, &provider_kind, &err);
    if (rc != YVEX_OK) return rc == 2 ? 2 : print_yvex_error(&err, exit_for_status(rc));
    (void)operator_paths;
    (void)provider_kind;
    active_present = model_download_read_active_process(report.active_receipt_path, &pid, &pgid);
    if (!active_present || !model_download_pgid_alive(pgid)) {
        if (options.match_provider_process) {
            model_download_find_provider_processes(report.local_source_dir, &match);
            if (match.count > 0u) {
                pid = match.first_pid;
                pgid = match.first_pgid;
            }
        }
    }
    if (pgid <= 0 || !model_download_pgid_alive(pgid)) {
        yvex_cli_out_writef(stdout, "model-download-stop: target=%s\n", report.target_id);
        yvex_cli_out_writef(stdout, "process: none\n");
        yvex_cli_out_writef(stdout, "action: none\n");
        yvex_cli_out_writef(stdout, "next: yvex models download status %s --models-root %s\n",
               report.target_id, report.models_root);
        yvex_cli_out_writef(stdout, "status: model-download-stop-none\n");
        return 0;
    }
    if (!options.dry_run) {
        if (kill(-pgid, SIGTERM) == 0 || errno == ESRCH) {
            signaled = 1;
        }
        stopped = model_download_wait_group_gone(pgid, options.timeout_seconds);
        if (!stopped && options.force) {
            if (kill(-pgid, SIGKILL) == 0 || errno == ESRCH) killed = 1;
            stopped = model_download_wait_group_gone(pgid, options.timeout_seconds);
        }
        snprintf(report.status, sizeof(report.status), "model-download-stopped");
        report.provider_pid = pid;
        report.provider_process_group = pgid;
        report.signal_forwarded = signaled;
        report.child_killed_after_timeout = killed;
        report.partial_source_preserved = 1;
        report.lock_files_deleted = 0;
        model_download_wait_path_gone(report.active_receipt_path, options.timeout_seconds);
        (void)model_download_finalize_control_receipt(&options, &report, "stopped");
    }
    yvex_cli_out_writef(stdout, "model-download-stop: target=%s\n", report.target_id);
    yvex_cli_out_writef(stdout, "provider: %s\n", report.provider);
    yvex_cli_out_writef(stdout, "signal: SIGTERM\n");
    yvex_cli_out_writef(stdout, "process_group: %lld\n", (long long)pgid);
    yvex_cli_out_writef(stdout, "child_signal_forwarded: %s\n", signaled ? "true" : "false");
    yvex_cli_out_writef(stdout, "child_killed_after_timeout: %s\n", killed ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup: preserved-partial-source\n");
    yvex_cli_out_writef(stdout, "lock_cleanup: not-attempted\n");
    yvex_cli_out_writef(stdout, "partial_source_preserved: true\n");
    yvex_cli_out_writef(stdout, "lock_files_deleted: false\n");
    yvex_cli_out_writef(stdout, "boundary: partial source files may exist; runtime unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", options.dry_run ? "model-download-stop-dry-run" : "model-download-stopped");
    return signaled || stopped || options.dry_run ? 0 : 1;
}

static int model_download_delete_lock_paths(const yvex_model_download_report *report,
                                            int dry_run,
                                            int yes,
                                            unsigned long long *candidate_index,
                                            unsigned long long *deleted_out)
{
    unsigned long long i;
    unsigned long long deleted = 0ull;

    if (deleted_out) *deleted_out = 0ull;
    if (!report) return 0;
    for (i = 0; i < report->source_scan.lock_count && i < YVEX_MODEL_DOWNLOAD_PATTERN_CAP; ++i) {
        char abs_lock[YVEX_PATH_CAP];
        yvex_error err;
        if (!model_download_name_contains(report->source_scan.lock_paths[i],
                                          ".cache/huggingface/download/")) {
            continue;
        }
        yvex_error_clear(&err);
        if (path_join2(abs_lock, sizeof(abs_lock), report->local_source_dir,
                       report->source_scan.lock_paths[i], &err,
                       "models_download_cleanup") != YVEX_OK) {
            continue;
        }
        yvex_cli_out_writef(stdout, "delete_candidate.%llu: %s\n",
               candidate_index ? *candidate_index : i,
               abs_lock);
        if (candidate_index) (*candidate_index)++;
        if (!dry_run && yes) {
            if (unlink(abs_lock) == 0) deleted++;
        }
    }
    if (deleted_out) *deleted_out = deleted;
    return 1;
}

static int model_download_remove_tree_path(const char *path,
                                           unsigned long long *removed_out)
{
    struct stat st;
    unsigned long long removed = 0ull;
    int rc = 0;

    if (removed_out) *removed_out = 0ull;
    if (!path || !path[0]) return -1;
    if (lstat(path, &st) != 0) {
        return errno == ENOENT ? 0 : -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        if (unlink(path) != 0) return -1;
        if (removed_out) *removed_out = 1ull;
        return 0;
    }
    {
        DIR *dir = opendir(path);
        struct dirent *ent;
        if (!dir) return -1;
        while ((ent = readdir(dir)) != NULL) {
            char child[YVEX_PATH_CAP];
            unsigned long long child_removed = 0ull;
            int n;

            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }
            n = snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
            if (n < 0 || (size_t)n >= sizeof(child)) {
                rc = -1;
                continue;
            }
            if (model_download_remove_tree_path(child, &child_removed) != 0) {
                rc = -1;
            } else {
                removed += child_removed;
            }
        }
        closedir(dir);
    }
    if (rmdir(path) != 0) return -1;
    removed++;
    if (removed_out) *removed_out = removed;
    return rc;
}

static int model_download_path_exists(const char *path)
{
    struct stat st;
    return path && path[0] && lstat(path, &st) == 0;
}

static int model_download_delete_path_candidate(const char *path,
                                                int recursive,
                                                int dry_run,
                                                int yes,
                                                unsigned long long *candidate_index,
                                                unsigned long long *deleted_out,
                                                unsigned long long *missing_out)
{
    unsigned long long removed = 0ull;

    if (deleted_out) *deleted_out = 0ull;
    if (missing_out) *missing_out = 0ull;
    if (!path || !path[0]) return 0;
    yvex_cli_out_writef(stdout, "delete_candidate.%llu: %s\n",
           candidate_index ? *candidate_index : 0ull,
           path);
    if (candidate_index) (*candidate_index)++;
    if (!model_download_path_exists(path)) {
        if (missing_out) *missing_out = 1ull;
        return 1;
    }
    if (dry_run || !yes) return 1;
    if (recursive) {
        if (model_download_remove_tree_path(path, &removed) != 0) return 0;
    } else {
        if (unlink(path) != 0) return 0;
        removed = 1ull;
    }
    if (deleted_out) *deleted_out = removed;
    return 1;
}

static void model_download_cleanup_sidecars(const yvex_model_download_report *report,
                                            int dry_run,
                                            int yes,
                                            unsigned long long *candidate_index,
                                            unsigned long long *deleted_inout,
                                            unsigned long long *missing_inout,
                                            unsigned long long *failed_inout)
{
    const char *paths[7];
    unsigned long long i;

    if (!report) return;
    paths[0] = report->receipt_path;
    paths[1] = report->active_receipt_path;
    paths[2] = report->last_receipt_path;
    paths[3] = report->download_report_path;
    paths[4] = report->manifest_path;
    paths[5] = report->native_inventory_path;
    paths[6] = report->registry_path;
    for (i = 0ull; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        unsigned long long deleted = 0ull;
        unsigned long long missing = 0ull;

        if (model_download_delete_path_candidate(paths[i], 0, dry_run, yes,
                                                 candidate_index, &deleted,
                                                 &missing)) {
            if (deleted_inout) *deleted_inout += deleted;
            if (missing_inout) *missing_inout += missing;
        } else if (failed_inout) {
            (*failed_inout)++;
        }
    }
}

static void model_download_cleanup_logs(const yvex_model_download_report *report,
                                        int dry_run,
                                        int yes,
                                        unsigned long long *candidate_index,
                                        unsigned long long *deleted_inout,
                                        unsigned long long *missing_inout,
                                        unsigned long long *failed_inout)
{
    const char *paths[2];
    unsigned long long i;

    if (!report) return;
    paths[0] = report->stdout_log_path;
    paths[1] = report->stderr_log_path;
    for (i = 0ull; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        unsigned long long deleted = 0ull;
        unsigned long long missing = 0ull;

        if (model_download_delete_path_candidate(paths[i], 0, dry_run, yes,
                                                 candidate_index, &deleted,
                                                 &missing)) {
            if (deleted_inout) *deleted_inout += deleted;
            if (missing_inout) *missing_inout += missing;
        } else if (failed_inout) {
            (*failed_inout)++;
        }
    }
}

static int command_models_download_cleanup(int arg_count, char **args)
{
    yvex_cli_models_download_options options;
    yvex_model_download_report report;
    yvex_operator_paths operator_paths;
    yvex_account_provider provider_kind;
    yvex_model_download_process_match match;
    yvex_error err;
    unsigned long long deleted = 0ull;
    unsigned long long candidates = 0ull;
    unsigned long long missing = 0ull;
    unsigned long long source_deleted = 0ull;
    unsigned long long sidecar_deleted = 0ull;
    unsigned long long log_deleted = 0ull;
    unsigned long long cache_deleted = 0ull;
    unsigned long long lock_deleted = 0ull;
    unsigned long long failed_deletes = 0ull;
    const char *cleanup_status;
    int sidecars_requested;
    int logs_requested;
    int rc;

    yvex_error_clear(&err);
    rc = model_download_resolve_for_control(arg_count, args, 4, &options, &report,
                                            &operator_paths, &provider_kind, &err);
    if (rc != YVEX_OK) return rc == 2 ? 2 : print_yvex_error(&err, exit_for_status(rc));
    (void)provider_kind;
    if (!model_download_source_path_allowed(&operator_paths, report.local_source_dir, &report)) {
        yvex_cli_out_writef(stdout, "model-download-cleanup: target=%s\n", report.target_id);
        yvex_cli_out_writef(stdout, "top_blocker: %s\n",
               report.top_blocker[0] ? report.top_blocker : "unsafe-source-path");
        yvex_cli_out_writef(stdout, "reason: %s\n",
               report.error[0] ? report.error : "resolved local source path is unsafe");
        yvex_cli_out_writef(stdout, "deleted: 0\n");
        yvex_cli_out_writef(stdout, "status: model-download-cleanup-blocked\n");
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    yvex_error_clear(&err);
    (void)model_download_scan_source(report.local_source_dir, &report.source_scan, &err);
    model_download_find_provider_processes(report.local_source_dir, &match);
    yvex_cli_out_writef(stdout, "model-download-cleanup: target=%s\n", report.target_id);
    if (match.count > 0u) {
        yvex_cli_out_writef(stdout, "provider_process_alive: true\n");
        yvex_cli_out_writef(stdout, "deleted: 0\n");
        yvex_cli_out_writef(stdout, "status: model-download-cleanup-blocked\n");
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    sidecars_requested = options.cleanup_receipts || options.cleanup_failed_partials;
    logs_requested = options.cleanup_logs || options.cleanup_failed_partials;
    if (options.cleanup_stale_locks) {
        (void)model_download_delete_lock_paths(&report, options.dry_run, options.yes,
                                               &candidates, &lock_deleted);
        deleted += lock_deleted;
    }
    if (options.cleanup_all_provider_cache) {
        char cache_path[YVEX_PATH_CAP];
        unsigned long long cache_missing = 0ull;
        yvex_error path_err;

        yvex_error_clear(&path_err);
        if (path_join2(cache_path, sizeof(cache_path), report.local_source_dir,
                       ".cache", &path_err, "models_download_cleanup") == YVEX_OK) {
            if (model_download_delete_path_candidate(cache_path, 1,
                                                     options.dry_run,
                                                     options.yes,
                                                     &candidates,
                                                     &cache_deleted,
                                                     &cache_missing)) {
                deleted += cache_deleted;
                missing += cache_missing;
            } else {
                failed_deletes++;
            }
        }
    }
    if (options.cleanup_failed_partials) {
        unsigned long long source_missing = 0ull;

        if (model_download_delete_path_candidate(report.local_source_dir, 1,
                                                 options.dry_run,
                                                 options.yes,
                                                 &candidates,
                                                 &source_deleted,
                                                 &source_missing)) {
            deleted += source_deleted;
            missing += source_missing;
        } else {
            failed_deletes++;
        }
    }
    if (sidecars_requested) {
        model_download_cleanup_sidecars(&report, options.dry_run, options.yes,
                                        &candidates, &sidecar_deleted,
                                        &missing, &failed_deletes);
        deleted += sidecar_deleted;
    }
    if (logs_requested) {
        model_download_cleanup_logs(&report, options.dry_run, options.yes,
                                    &candidates, &log_deleted,
                                    &missing, &failed_deletes);
        deleted += log_deleted;
    }
    cleanup_status = options.dry_run ? "model-download-cleanup-dry-run" :
                     failed_deletes > 0ull ? "model-download-cleanup-partial" :
                     "model-download-cleanup";
    yvex_cli_out_writef(stdout, "stale_locks: %llu\n", report.source_scan.lock_count);
    yvex_cli_out_writef(stdout, "cleanup_stale_locks: %s\n", options.cleanup_stale_locks ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_failed_partials: %s\n", options.cleanup_failed_partials ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_sidecars: %s\n", sidecars_requested ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_logs: %s\n", logs_requested ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_provider_cache: %s\n", options.cleanup_all_provider_cache ? "true" : "false");
    yvex_cli_out_writef(stdout, "delete_candidates: %llu\n", candidates);
    yvex_cli_out_writef(stdout, "missing: %llu\n", missing);
    yvex_cli_out_writef(stdout, "failed_deletes: %llu\n", failed_deletes);
    yvex_cli_out_writef(stdout, "deleted_locks: %llu\n", lock_deleted);
    yvex_cli_out_writef(stdout, "deleted_source_entries: %llu\n", source_deleted);
    yvex_cli_out_writef(stdout, "deleted_sidecars: %llu\n", sidecar_deleted);
    yvex_cli_out_writef(stdout, "deleted_logs: %llu\n", log_deleted);
    yvex_cli_out_writef(stdout, "deleted_provider_cache_entries: %llu\n", cache_deleted);
    yvex_cli_out_writef(stdout, "deleted: %llu\n", deleted);
    yvex_cli_out_writef(stdout, "dry_run: %s\n", options.dry_run ? "true" : "false");
    yvex_cli_out_writef(stdout, "yes: %s\n", options.yes ? "true" : "false");
    yvex_cli_out_writef(stdout, "boundary: cleanup removes only target download state under models-root; runtime unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", cleanup_status);
    return failed_deletes > 0ull ? 1 : 0;
}

static int command_models_download_execute(int arg_count, char **args, int start_index, int resume_mode)
{
    yvex_cli_models_download_options options;
    const yvex_model_download_catalog_row *row = NULL;
    yvex_model_download_report report;
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_error err;
    yvex_source_manifest_options manifest_options;
    yvex_source_manifest_summary manifest_summary;
    yvex_native_weight_options native_options;
    yvex_native_weight_table *native_table = NULL;
    yvex_account_provider provider_kind = YVEX_ACCOUNT_PROVIDER_UNKNOWN;
    yvex_account_observation account_obs;
    char hf_family_dir[YVEX_PATH_CAP];
    char provider_root_dir[YVEX_PATH_CAP];
    char github_repo_dir[YVEX_PATH_CAP];
    char reports_family_dir[YVEX_PATH_CAP];
    char registry_family_dir[YVEX_PATH_CAP];
    char logs_dir[YVEX_PATH_CAP];
    char file_name[256];
    const char *target_id;
    const char *family;
    const char *repo_id;
    const char *local_name;
    const char *revision;
    const char *provider_name;
    const char *token_value;
    yvex_model_download_resolved_target resolved;
    int resolved_dynamic = 0;
    int token_present;
    int rc;

    rc = parse_models_download_options_from(arg_count, args, start_index, &options);
    if (rc == 1) {
        yvex_model_artifacts_surface_models_help(stdout);
        return 0;
    }
    if (rc != 0) return rc;
    options.resume = resume_mode;

    model_download_report_init(&report);
    yvex_error_clear(&err);
    memset(&paths, 0, sizeof(paths));
    memset(&operator_paths, 0, sizeof(operator_paths));
    memset(&account_obs, 0, sizeof(account_obs));
    model_download_timestamp(report.created_at, sizeof(report.created_at));

    if (!yvex_account_provider_from_name(options.provider, &provider_kind)) {
        yvex_cli_out_writef(stderr, "yvex: models download --provider requires hf|huggingface|gh|github\n");
        return 2;
    }
    provider_name = yvex_account_provider_name(provider_kind);
    rc = yvex_operator_paths_resolve(&paths, options.models_root, &operator_paths, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (options.repo) {
        target_id = options.name;
        family = provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB ? "github" : options.family;
        repo_id = options.repo;
        local_name = options.name;
        revision = provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB
            ? (options.release ? options.release : "latest")
            : (options.revision ? options.revision : "main");
        snprintf(report.stage_resolve_target, sizeof(report.stage_resolve_target), "pass");
    } else {
        memset(&resolved, 0, sizeof(resolved));
        resolved_dynamic = model_download_resolve_downloaded_target(options.target,
                                                                    &operator_paths,
                                                                    &resolved,
                                                                    &err);
        if (resolved_dynamic) {
            target_id = resolved.target_id;
            family = resolved.family;
            repo_id = resolved.repo_id;
            local_name = resolved.local_name;
            revision = options.revision ? options.revision : resolved.revision;
            if (!yvex_account_provider_from_name(resolved.provider, &provider_kind)) {
                provider_kind = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
            }
            provider_name = yvex_account_provider_name(provider_kind);
            snprintf(report.stage_resolve_target, sizeof(report.stage_resolve_target), "pass");
        } else if ((row = model_download_find_catalog(options.target)) != NULL) {
            target_id = row->target_id;
            family = row->family;
            repo_id = row->repo_id;
            local_name = row->local_name;
            revision = options.revision ? options.revision : row->revision_default;
            if (!yvex_account_provider_from_name(row->provider, &provider_kind)) {
                provider_kind = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
            }
            provider_name = yvex_account_provider_name(provider_kind);
            snprintf(report.stage_resolve_target, sizeof(report.stage_resolve_target), "pass");
        } else {
            yvex_cli_out_writef(stdout, "models: download\n");
            yvex_cli_out_writef(stdout, "target_id: %s\n", options.target ? options.target : "");
            model_stage_print("resolve-target", "fail");
            yvex_cli_out_writef(stdout, "reason: unknown models download target\n");
            yvex_cli_out_writef(stdout, "status: model-download-unknown-target\n");
            return 2;
        }
    }

    snprintf(report.target_id, sizeof(report.target_id), "%s", target_id);
    snprintf(report.family, sizeof(report.family), "%s", family);
    snprintf(report.provider, sizeof(report.provider), "%s", provider_name);
    snprintf(report.repo_id, sizeof(report.repo_id), "%s", repo_id);
    snprintf(report.revision, sizeof(report.revision), "%s", revision);
    snprintf(report.local_name, sizeof(report.local_name), "%s", local_name);
    snprintf(report.token_env_name, sizeof(report.token_env_name), "%s",
             options.token_env ? options.token_env : yvex_account_default_token_env(provider_kind));
    token_value = getenv(report.token_env_name);
    token_present = token_value && token_value[0];
    snprintf(report.auth_state, sizeof(report.auth_state), "%s",
             token_present ? "env-token-present" : "not-provided");

    snprintf(report.models_root, sizeof(report.models_root), "%s", operator_paths.models_root);
    snprintf(report.models_root_source, sizeof(report.models_root_source), "%s",
             operator_paths.models_root_source);

    if (provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB) {
        rc = path_join2(provider_root_dir, sizeof(provider_root_dir), operator_paths.models_root,
                        "github", &err, "models_download");
        if (rc == YVEX_OK) {
            rc = path_join2(github_repo_dir, sizeof(github_repo_dir), provider_root_dir,
                            repo_id, &err, "models_download");
        }
        if (rc == YVEX_OK) {
            rc = path_join2(report.local_source_dir, sizeof(report.local_source_dir),
                            github_repo_dir, revision, &err, "models_download");
        }
    } else {
        rc = path_join2(hf_family_dir, sizeof(hf_family_dir), operator_paths.hf_root,
                        family, &err, "models_download");
        if (rc == YVEX_OK) {
            rc = path_join2(report.local_source_dir, sizeof(report.local_source_dir),
                            hf_family_dir, local_name, &err, "models_download");
        }
    }
    if (rc == YVEX_OK && resolved_dynamic && resolved.local_source_dir[0]) {
        snprintf(report.local_source_dir, sizeof(report.local_source_dir), "%s",
                 resolved.local_source_dir);
    }
    if (rc == YVEX_OK) {
        rc = path_join2(reports_family_dir, sizeof(reports_family_dir),
                        operator_paths.reports_root, family, &err, "models_download");
    }
    if (rc == YVEX_OK) {
        rc = path_join2(registry_family_dir, sizeof(registry_family_dir),
                        operator_paths.registry_root, family, &err, "models_download");
    }
    if (rc == YVEX_OK) {
        rc = path_join2(logs_dir, sizeof(logs_dir), operator_paths.models_root,
                        "logs", &err, "models_download");
    }
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    snprintf(report.reports_dir, sizeof(report.reports_dir), "%s", reports_family_dir);
    snprintf(report.registry_dir, sizeof(report.registry_dir), "%s", registry_family_dir);
    snprintf(file_name, sizeof(file_name), "%s.download.receipt", target_id);
    rc = path_join2(report.receipt_path, sizeof(report.receipt_path),
                    reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.active.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.active_receipt_path,
                                       sizeof(report.active_receipt_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.last.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.last_receipt_path,
                                       sizeof(report.last_receipt_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download-report.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.download_report_path,
                                       sizeof(report.download_report_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.source-manifest.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.manifest_path,
                                       sizeof(report.manifest_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.native-inventory.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.native_inventory_path,
                                       sizeof(report.native_inventory_path),
                                       reports_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.json", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.registry_path,
                                       sizeof(report.registry_path),
                                       registry_family_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.stdout.log", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.stdout_log_path,
                                       sizeof(report.stdout_log_path),
                                       logs_dir, file_name, &err, "models_download");
    snprintf(file_name, sizeof(file_name), "%s.download.stderr.log", target_id);
    if (rc == YVEX_OK) rc = path_join2(report.stderr_log_path,
                                       sizeof(report.stderr_log_path),
                                       logs_dir, file_name, &err, "models_download");
    if (rc == YVEX_OK && resolved_dynamic) {
        if (resolved.registry_path[0]) {
            snprintf(report.registry_path, sizeof(report.registry_path), "%s",
                     resolved.registry_path);
        }
        if (resolved.download_report_path[0]) {
            snprintf(report.download_report_path, sizeof(report.download_report_path), "%s",
                     resolved.download_report_path);
        }
        if (resolved.manifest_path[0]) {
            snprintf(report.manifest_path, sizeof(report.manifest_path), "%s",
                     resolved.manifest_path);
        }
        if (resolved.native_inventory_path[0]) {
            snprintf(report.native_inventory_path, sizeof(report.native_inventory_path), "%s",
                     resolved.native_inventory_path);
        }
    }
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    if (!model_download_source_path_allowed(&operator_paths, report.local_source_dir, &report)) {
        snprintf(report.status, sizeof(report.status), "model-download-blocked");
        snprintf(report.stage_resolve_paths, sizeof(report.stage_resolve_paths), "fail");
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_resolve_paths, sizeof(report.stage_resolve_paths), "pass");

    rc = yvex_model_registry_mkdir_parent(report.local_source_dir, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.receipt_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.active_receipt_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.last_receipt_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.download_report_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.registry_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.stdout_log_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(report.stderr_log_path, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_prepare_dirs, sizeof(report.stage_prepare_dirs), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_prepare_dirs, sizeof(report.stage_prepare_dirs), "pass");

    if (options.resume) {
        yvex_model_download_process_match match;
        pid_t active_pid = -1;
        pid_t active_pgid = -1;
        int active_present;

        active_present = model_download_read_active_process(report.active_receipt_path,
                                                            &active_pid,
                                                            &active_pgid);
        model_download_find_provider_processes(report.local_source_dir, &match);
        if ((active_present &&
             (model_download_pid_alive(active_pid) ||
              model_download_pgid_alive(active_pgid))) ||
            match.count > 0u) {
            snprintf(report.status, sizeof(report.status), "model-download-resume-blocked");
            snprintf(report.stage_download, sizeof(report.stage_download), "blocked");
            snprintf(report.top_blocker, sizeof(report.top_blocker), "active-provider-process");
            snprintf(report.error, sizeof(report.error), "another provider process is active for this source directory");
            return model_download_finish(&options, &report);
        }
        yvex_error_clear(&err);
        (void)model_download_scan_source(report.local_source_dir, &report.source_scan, &err);
        if (report.source_scan.lock_count > 0ull && !options.clear_stale_locks) {
            snprintf(report.status, sizeof(report.status), "model-download-resume-blocked");
            snprintf(report.stage_download, sizeof(report.stage_download), "blocked");
            snprintf(report.top_blocker, sizeof(report.top_blocker), "stale-lock-candidates");
            snprintf(report.error, sizeof(report.error), "stale lock candidates require --clear-stale-locks");
            return model_download_finish(&options, &report);
        }
        if (report.source_scan.lock_count > 0ull && options.clear_stale_locks) {
            unsigned long long deleted = 0ull;
            (void)model_download_delete_lock_paths(&report, 0, 1, NULL, &deleted);
            report.lock_files_deleted = deleted > 0ull;
            memset(&report.source_scan, 0, sizeof(report.source_scan));
        }
    }

    if (options.auth_mode == YVEX_MODEL_DOWNLOAD_AUTH_NEVER) {
        yvex_account_observe_options observe;
        memset(&observe, 0, sizeof(observe));
        observe.provider = provider_kind;
        observe.cli_override = options.cli;
        observe.token_env_name = report.token_env_name;
        rc = yvex_account_observe(&observe, &account_obs, &err);
    } else {
        yvex_account_ensure_options ensure;
        memset(&ensure, 0, sizeof(ensure));
        ensure.provider = provider_kind;
        ensure.cli_override = options.cli;
        ensure.token_env_name = report.token_env_name;
        ensure.interactive = options.auth_mode == YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED
            ? YVEX_ACCOUNT_INTERACTIVE_NEVER
            : YVEX_ACCOUNT_INTERACTIVE_AUTO;
        ensure.required = options.auth_mode == YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED;
        rc = yvex_account_ensure(&ensure, &account_obs, &err);
    }
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_account_provider, sizeof(report.stage_account_provider), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }

    snprintf(report.provider_cli_path, sizeof(report.provider_cli_path), "%s", account_obs.cli_path);
    snprintf(report.provider_cli_source, sizeof(report.provider_cli_source), "%s", account_obs.cli_source);
    snprintf(report.provider_cli_status, sizeof(report.provider_cli_status), "%.*s",
             (int)sizeof(report.provider_cli_status) - 1, account_obs.cli_status);
    snprintf(report.auth_state, sizeof(report.auth_state), "%s", account_obs.auth_state);
    snprintf(report.credential_source, sizeof(report.credential_source), "%s", account_obs.credential_source);
    snprintf(report.account_hint, sizeof(report.account_hint), "%s", account_obs.account_hint);
    snprintf(report.accounts_state_path, sizeof(report.accounts_state_path), "%s", account_obs.state_path);
    if (provider_kind == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE) {
        snprintf(report.hf_cli_path, sizeof(report.hf_cli_path), "%s", account_obs.cli_path);
        snprintf(report.hf_cli_source, sizeof(report.hf_cli_source), "%.*s",
                 (int)sizeof(report.hf_cli_source) - 1, account_obs.cli_source);
    }

    if (!account_obs.cli_present) {
        snprintf(report.status, sizeof(report.status), "model-download-blocked");
        snprintf(report.stage_account_provider, sizeof(report.stage_account_provider), "blocked");
        snprintf(report.stage_provider_cli, sizeof(report.stage_provider_cli), "blocked");
        if (provider_kind == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE) {
            snprintf(report.stage_hf_cli, sizeof(report.stage_hf_cli), "blocked");
        }
        snprintf(report.top_blocker, sizeof(report.top_blocker), "%s", account_obs.top_blocker);
        snprintf(report.error, sizeof(report.error), "%s", account_obs.next);
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_provider_cli, sizeof(report.stage_provider_cli), "pass");
    if (provider_kind == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE) {
        snprintf(report.stage_hf_cli, sizeof(report.stage_hf_cli), "pass");
    }

    if (options.auth_mode == YVEX_MODEL_DOWNLOAD_AUTH_NEVER) {
        snprintf(report.stage_account_provider, sizeof(report.stage_account_provider), "skipped");
    } else if (strcmp(account_obs.auth_state, "logged-in") == 0 ||
               strcmp(account_obs.auth_state, "env-token-present") == 0) {
        snprintf(report.stage_account_provider, sizeof(report.stage_account_provider), "pass");
    } else {
        snprintf(report.status, sizeof(report.status), "model-download-blocked");
        snprintf(report.stage_account_provider, sizeof(report.stage_account_provider), "blocked");
        snprintf(report.top_blocker, sizeof(report.top_blocker), "provider-login-required");
        snprintf(report.error, sizeof(report.error), "%s",
                 account_obs.next[0] ? account_obs.next : "yvex accounts login provider");
        return model_download_finish(&options, &report);
    }

    rc = model_download_write_receipt(report.receipt_path, &options, &report,
                                      token_present, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }

    if (provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB) {
        report.provider_exit_code = model_download_run_github(&options, &report, &err);
        report.hf_exit_code = report.provider_exit_code;
    } else {
        report.provider_exit_code = model_download_run_hf(&options, &report,
                                                          token_present ? token_value : NULL,
                                                          &err);
        report.hf_exit_code = report.provider_exit_code;
    }
    if (provider_kind == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE &&
        model_download_effective_progress_mode(options.progress_mode) !=
            YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) {
        snprintf(report.stage_progress_stream, sizeof(report.stage_progress_stream),
                 report.provider_exit_code < 0 ? "fail" : "pass");
        snprintf(report.stage_progress_ticks, sizeof(report.stage_progress_ticks),
                 report.tick_count > 0ull ? "pass" : "skipped");
    } else {
        snprintf(report.stage_progress_stream, sizeof(report.stage_progress_stream), "skipped");
        snprintf(report.stage_progress_ticks, sizeof(report.stage_progress_ticks), "skipped");
    }
    if (report.interrupted || report.provider_exit_code == 130 || report.provider_exit_code == 143) {
        int write_rc;
        if (!report.interrupted) {
            report.interrupted = 1;
            report.interrupt_signal = report.provider_exit_code == 143 ? SIGTERM : SIGINT;
            report.partial_source_preserved = 1;
            report.lock_files_deleted = 0;
            snprintf(report.child_exit_status, sizeof(report.child_exit_status),
                     "interrupted");
        }
        (void)model_download_scan_source(report.local_source_dir, &report.source_scan, &err);
        snprintf(report.status, sizeof(report.status), "model-download-interrupted");
        snprintf(report.stage_download, sizeof(report.stage_download), "interrupted");
        snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "partial");
        snprintf(report.top_blocker, sizeof(report.top_blocker), "provider-download-interrupted");
        snprintf(report.error, sizeof(report.error), "provider download interrupted");
        write_rc = model_download_write_json_sidecar(report.download_report_path,
                                                    "yvex.model_download.report.v1",
                                                    &options,
                                                    &report,
                                                    &err);
        if (write_rc == YVEX_OK) {
            report.report_written = 1;
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "pass");
        } else {
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "fail");
        }
        (void)model_download_finalize_control_receipt(&options, &report, "interrupted");
        return model_download_finish(&options, &report);
    }
    if (options.dry_run && report.provider_exit_code == 0) {
        snprintf(report.status, sizeof(report.status), "model-download-dry-run");
        snprintf(report.stage_download, sizeof(report.stage_download), "dry-run");
        snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "skipped");
        snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "skipped");
        snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "skipped");
        snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "skipped");
        return model_download_finish(&options, &report);
    }
    if (report.provider_exit_code != 0) {
        int write_rc;
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_download, sizeof(report.stage_download), "fail");
        snprintf(report.top_blocker, sizeof(report.top_blocker), "provider-download-failed");
        if (report.provider_exit_code < 0 && yvex_error_message(&err)[0]) {
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        } else {
            snprintf(report.error, sizeof(report.error), "%s download exited nonzero",
                     report.provider);
        }
        write_rc = model_download_write_json_sidecar(report.download_report_path,
                                                    "yvex.model_download.report.v1",
                                                    &options,
                                                    &report,
                                                    &err);
        if (write_rc == YVEX_OK) {
            report.report_written = 1;
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "pass");
        } else {
            snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "fail");
        }
        (void)model_download_finalize_control_receipt(&options, &report, "failed");
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_download, sizeof(report.stage_download), "pass");

    rc = model_download_scan_source(report.local_source_dir, &report.source_scan, &err);
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }
    snprintf(report.stage_source_scan, sizeof(report.stage_source_scan), "pass");

    if (options.no_manifest) {
        snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "skipped");
    } else {
        memset(&manifest_options, 0, sizeof(manifest_options));
        memset(&manifest_summary, 0, sizeof(manifest_summary));
        manifest_options.repo = report.repo_id;
        manifest_options.revision = report.revision;
        manifest_options.local_path = report.local_source_dir;
        manifest_options.node_name = report.local_name;
        manifest_options.download_log = report.stdout_log_path;
        manifest_options.dry_run_log = "";
        manifest_options.pid_file = "";
        manifest_options.download_command = provider_kind == YVEX_ACCOUNT_PROVIDER_GITHUB
            ? "gh release download (see receipt; token redacted)"
            : "hf download (see receipt; token redacted)";
        manifest_options.status = YVEX_SOURCE_STATUS_COMPLETE;
        manifest_options.include_files = 1;
        rc = yvex_source_manifest_write_json(report.manifest_path,
                                             &manifest_options,
                                             &manifest_summary,
                                             &err);
        if (rc != YVEX_OK) {
            snprintf(report.status, sizeof(report.status), "model-download-fail");
            snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "fail");
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            return model_download_finish(&options, &report);
        }
        report.source_manifest_written = 1;
        snprintf(report.stage_source_manifest, sizeof(report.stage_source_manifest), "pass");
    }

    if (options.no_native_inventory) {
        snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "skipped");
    } else {
        memset(&native_options, 0, sizeof(native_options));
        memset(&report.native_summary, 0, sizeof(report.native_summary));
        native_options.source_dir = report.local_source_dir;
        native_options.recursive = 1;
        rc = yvex_native_weight_table_open(&native_table, &native_options, &err);
        if (rc == YVEX_OK) rc = yvex_native_weight_table_summary(native_table,
                                                                 &report.native_summary,
                                                                 &err);
        if (rc == YVEX_OK) rc = model_download_write_native_inventory_json(report.native_inventory_path,
                                                                           report.local_source_dir,
                                                                           native_table,
                                                                           &err);
        yvex_native_weight_table_close(native_table);
        native_table = NULL;
        if (rc != YVEX_OK) {
            snprintf(report.status, sizeof(report.status), "model-download-fail");
            snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "fail");
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            return model_download_finish(&options, &report);
        }
        report.native_inventory_written = 1;
        snprintf(report.stage_native_inventory, sizeof(report.stage_native_inventory), "pass");
    }

    snprintf(report.status, sizeof(report.status), "%s",
             options.resume ? "model-download-resume-pass" : "model-download-pass");
    rc = model_download_write_json_sidecar(report.download_report_path,
                                          "yvex.model_download.report.v1",
                                          &options,
                                          &report,
                                          &err);
    if (rc == YVEX_OK) {
        report.report_written = 1;
        rc = model_download_write_json_sidecar(report.registry_path,
                                              "yvex.model_download.registry.v1",
                                              &options,
                                              &report,
                                              &err);
    }
    if (rc != YVEX_OK) {
        snprintf(report.status, sizeof(report.status), "model-download-fail");
        snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "fail");
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_download_finish(&options, &report);
    }
    report.registry_written = 1;
    snprintf(report.stage_sidecar, sizeof(report.stage_sidecar), "pass");
    (void)model_download_finalize_control_receipt(&options, &report, "pass");
    return model_download_finish(&options, &report);
}

int yvex_models_download_surface_command(int arg_count, char **args)
{
    if (arg_count >= 4) {
        if (strcmp(args[3], "status") == 0) {
            return command_models_download_status(arg_count, args);
        }
        if (strcmp(args[3], "stop") == 0) {
            return command_models_download_stop(arg_count, args);
        }
        if (strcmp(args[3], "resume") == 0) {
            return command_models_download_execute(arg_count, args, 4, 1);
        }
        if (strcmp(args[3], "cleanup") == 0) {
            return command_models_download_cleanup(arg_count, args);
        }
    }
    return command_models_download_execute(arg_count, args, 3, 0);
}
