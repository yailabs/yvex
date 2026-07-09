/*
 * yvex_models_download_write_surface.c - models download CLI file/progress writers.
 * Owner: src/cli/model_artifacts
 * Owns: CLI-only download receipts, sidecar JSON text, log mirroring, and progress helpers.
 * Does not own: domain artifact emission, runtime generation, registry storage, eval, benchmark, or release claims.
 * Invariants: writes explicit files/progress for existing CLI behavior only; CLI-only and excluded from libyvex.a.
 * Boundary: file/progress output is source intake evidence, not generation support.
 */
#include "yvex_models_download_surface.h"

static void model_download_write_pattern_array(FILE *fp,
                                               const char *name,
                                               const yvex_cli_models_download_options *options,
                                               int includes,
                                               int comma)
{
    unsigned int count = includes
        ? model_download_effective_include_count(options)
        : model_download_effective_exclude_count(options);
    unsigned int i;

    yvex_cli_out_writef(fp, "  \"%s\": [", name);
    for (i = 0; i < count; ++i) {
        if (i > 0) yvex_cli_out_fputs(", ", fp);
        write_escaped(fp, includes
            ? model_download_effective_include_at(options, i)
            : model_download_effective_exclude_at(options, i));
    }
    yvex_cli_out_writef(fp, "]%s\n", comma ? "," : "");
}

int model_download_write_native_inventory_json(const char *path,
                                                      const char *source_dir,
                                                      const yvex_native_weight_table *table,
                                                      yvex_error *err)
{
    yvex_native_weight_summary summary;
    FILE *fp;
    unsigned long long count;
    unsigned long long i;

    if (!path || !source_dir || !table) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_native_inventory",
                       "path, source directory, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_native_weight_table_summary(table, &summary, err) != YVEX_OK) {
        return yvex_error_code(err);
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_native_inventory",
                        "cannot open native inventory: %s", path);
        return YVEX_ERR_IO;
    }
    count = yvex_native_weight_table_count(table);
    yvex_cli_out_writef(fp, "{\n");
    write_field(fp, "  ", "schema", "yvex.native_inventory.v1", 1);
    write_field(fp, "  ", "source_dir", source_dir, 1);
    write_bool_field(fp, "  ", "payload_loaded", 0, 1);
    yvex_cli_out_writef(fp, "  \"summary\": {\n");
    write_u64_field(fp, "    ", "shard_count", summary.shard_count, 1);
    write_u64_field(fp, "    ", "tensor_count", summary.tensor_count, 1);
    write_u64_field(fp, "    ", "total_tensor_bytes", summary.total_tensor_bytes, 1);
    write_u64_field(fp, "    ", "unknown_dtype_count", summary.unknown_dtype_count, 1);
    write_u64_field(fp, "    ", "malformed_shard_count", summary.malformed_shard_count, 0);
    yvex_cli_out_writef(fp, "  },\n");
    yvex_cli_out_writef(fp, "  \"tensors\": [\n");
    for (i = 0; i < count; ++i) {
        const yvex_native_weight_info *info = yvex_native_weight_table_at(table, i);
        unsigned int d;

        yvex_cli_out_writef(fp, "    {\n");
        write_field(fp, "      ", "name", info && info->name ? info->name : "", 1);
        write_field(fp, "      ", "shard_path", info && info->shard_path ? info->shard_path : "", 1);
        write_field(fp, "      ", "dtype", info && info->dtype_name ? info->dtype_name : "UNKNOWN", 1);
        yvex_cli_out_writef(fp, "      \"rank\": %u,\n", info ? info->rank : 0u);
        yvex_cli_out_writef(fp, "      \"shape\": [");
        if (info) {
            for (d = 0; d < info->rank; ++d) {
                yvex_cli_out_writef(fp, "%s%llu", d == 0 ? "" : ", ", info->dims[d]);
            }
        }
        yvex_cli_out_writef(fp, "],\n");
        write_u64_field(fp, "      ", "data_start", info ? info->data_start : 0ull, 1);
        write_u64_field(fp, "      ", "data_end", info ? info->data_end : 0ull, 1);
        write_u64_field(fp, "      ", "data_bytes", info ? info->data_bytes : 0ull, 0);
        yvex_cli_out_writef(fp, "    }%s\n", i + 1ull == count ? "" : ",");
    }
    yvex_cli_out_writef(fp, "  ]\n");
    yvex_cli_out_writef(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_native_inventory",
                        "cannot close native inventory: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int model_download_write_json_sidecar(const char *path,
                                             const char *schema,
                                             const yvex_cli_models_download_options *options,
                                             const yvex_model_download_report *report,
                                             yvex_error *err)
{
    FILE *fp;

    if (!path || !schema || !options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_sidecar",
                       "path, schema, options, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_sidecar",
                        "cannot open sidecar: %s", path);
        return YVEX_ERR_IO;
    }
    yvex_cli_out_writef(fp, "{\n");
    write_field(fp, "  ", "schema", schema, 1);
    write_field(fp, "  ", "status", report->status, 1);
    write_field(fp, "  ", "target_id", report->target_id, 1);
    write_field(fp, "  ", "family", report->family, 1);
    write_field(fp, "  ", "provider", report->provider, 1);
    write_field(fp, "  ", "repo_id", report->repo_id, 1);
    write_field(fp, "  ", "revision", report->revision, 1);
    write_field(fp, "  ", "local_source_dir", report->local_source_dir, 1);
    write_field(fp, "  ", "models_root", report->models_root, 1);
    write_field(fp, "  ", "models_root_source", report->models_root_source, 1);
    model_download_write_pattern_array(fp, "include_patterns", options, 1, 1);
    model_download_write_pattern_array(fp, "exclude_patterns", options, 0, 1);
    write_field(fp, "  ", "auth_mode", model_download_auth_mode_name(options->auth_mode), 1);
    write_field(fp, "  ", "provider_cli_path", report->provider_cli_path, 1);
    write_field(fp, "  ", "provider_cli_status", report->provider_cli_status, 1);
    write_field(fp, "  ", "account_provider_stage", report->stage_account_provider, 1);
    write_field(fp, "  ", "credential_source", report->credential_source, 1);
    write_field(fp, "  ", "account_hint", report->account_hint, 1);
    write_field(fp, "  ", "accounts_state_path", report->accounts_state_path, 1);
    write_field(fp, "  ", "hf_cli_path", report->hf_cli_path, 1);
    yvex_cli_out_writef(fp, "  \"hf_exit_code\": %d,\n", report->hf_exit_code);
    yvex_cli_out_writef(fp, "  \"provider_exit_code\": %d,\n", report->provider_exit_code);
    write_field(fp, "  ", "auth_state", report->auth_state, 1);
    write_field(fp, "  ", "progress_mode",
                model_download_progress_mode_name(options->progress_mode), 1);
    write_u64_field(fp, "  ", "tick_seconds", options->tick_seconds, 1);
    write_u64_field(fp, "  ", "tick_count", report->tick_count, 1);
    write_bool_field(fp, "  ", "stdout_streamed", report->stdout_streamed, 1);
    write_bool_field(fp, "  ", "stderr_streamed", report->stderr_streamed, 1);
    write_u64_field(fp, "  ", "stdout_bytes", report->stdout_bytes, 1);
    write_u64_field(fp, "  ", "stderr_bytes", report->stderr_bytes, 1);
    yvex_cli_out_writef(fp, "  \"provider_pid\": %lld,\n", (long long)report->provider_pid);
    yvex_cli_out_writef(fp, "  \"provider_process_group\": %lld,\n",
            (long long)report->provider_process_group);
    write_bool_field(fp, "  ", "interrupted", report->interrupted, 1);
    write_field(fp, "  ", "interrupt_signal",
                model_download_signal_name(report->interrupt_signal), 1);
    write_bool_field(fp, "  ", "signal_forwarded", report->signal_forwarded, 1);
    write_bool_field(fp, "  ", "child_terminated", report->child_terminated, 1);
    write_bool_field(fp, "  ", "child_killed_after_timeout",
                     report->child_killed_after_timeout, 1);
    write_field(fp, "  ", "child_exit_status", report->child_exit_status, 1);
    write_bool_field(fp, "  ", "orphan_check_performed",
                     report->orphan_check_performed, 1);
    write_field(fp, "  ", "orphan_check_status", report->orphan_check_status, 1);
    write_bool_field(fp, "  ", "partial_source_preserved",
                     report->partial_source_preserved, 1);
    write_bool_field(fp, "  ", "lock_files_deleted", report->lock_files_deleted, 1);
    write_u64_field(fp, "  ", "file_count", report->source_scan.file_count, 1);
    write_u64_field(fp, "  ", "safetensors_count", report->source_scan.safetensors_count, 1);
    write_bool_field(fp, "  ", "config_present", report->source_scan.config_present, 1);
    write_bool_field(fp, "  ", "tokenizer_present", report->source_scan.tokenizer_present, 1);
    write_u64_field(fp, "  ", "total_regular_file_bytes", report->source_scan.total_regular_file_bytes, 1);
    write_field(fp, "  ", "largest_file_name",
                report->source_scan.largest_file_name[0] ? report->source_scan.largest_file_name : "none", 1);
    write_u64_field(fp, "  ", "largest_file_bytes", report->source_scan.largest_file_bytes, 1);
    write_field(fp, "  ", "manifest_path", report->manifest_path, 1);
    write_field(fp, "  ", "native_inventory_path", report->native_inventory_path, 1);
    write_field(fp, "  ", "stdout_log", report->stdout_log_path, 1);
    write_field(fp, "  ", "stderr_log", report->stderr_log_path, 1);
    write_field(fp, "  ", "created_at", report->created_at, 1);
    write_field(fp, "  ", "yvex_version", yvex_version_string(), 1);
    write_bool_field(fp, "  ", "upstream_identity_verified", 0, 1);
    write_bool_field(fp, "  ", "remote_lookup_performed", 0, 1);
    write_bool_field(fp, "  ", "payload_hash_verified", 0, 1);
    write_bool_field(fp, "  ", "force_sidecars", options->force_sidecars, 1);
    write_bool_field(fp, "  ", "yes", options->yes, 1);
    yvex_cli_out_writef(fp, "  \"boundary\": {\n");
    write_field(fp, "    ", "source_download",
                options->dry_run ? "dry-run" :
                ((strcmp(report->status, "model-download-pass") == 0 ||
                  strcmp(report->status, "model-download-resume-pass") == 0) ? "performed" :
                 (strcmp(report->status, "model-download-blocked") == 0 ? "blocked" : "failed")), 1);
    write_bool_field(fp, "    ", "source_manifest_written", report->source_manifest_written, 1);
    write_bool_field(fp, "    ", "native_inventory_written", report->native_inventory_written, 1);
    write_bool_field(fp, "    ", "payload_loaded", 0, 1);
    write_bool_field(fp, "    ", "gguf_created", 0, 1);
    write_bool_field(fp, "    ", "materialized", 0, 1);
    write_bool_field(fp, "    ", "runtime_ready", 0, 1);
    write_field(fp, "    ", "generation", "unsupported", 1);
    write_field(fp, "    ", "eval", "unsupported", 1);
    write_field(fp, "    ", "benchmark", "not-measured", 0);
    yvex_cli_out_writef(fp, "  }\n");
    yvex_cli_out_writef(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_sidecar",
                        "cannot close sidecar: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int model_download_write_receipt(const char *path,
                                        const yvex_cli_models_download_options *options,
                                        const yvex_model_download_report *report,
                                        int token_present,
                                        yvex_error *err)
{
    FILE *fp;
    unsigned int i;

    if (!path || !options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_receipt",
                       "receipt path, options, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_receipt",
                        "cannot open receipt: %s", path);
        return YVEX_ERR_IO;
    }
    yvex_cli_out_writef(fp, "schema: yvex.model_download.receipt.v1\n");
    yvex_cli_out_writef(fp, "target_id: %s\n", report->target_id);
    yvex_cli_out_writef(fp, "family: %s\n", report->family);
    yvex_cli_out_writef(fp, "repo_id: %s\n", report->repo_id);
    yvex_cli_out_writef(fp, "revision: %s\n", report->revision);
    yvex_cli_out_writef(fp, "local_source_dir: %s\n", report->local_source_dir);
    yvex_cli_out_writef(fp, "hf_cli_path: %s\n", report->hf_cli_path);
    yvex_cli_out_writef(fp, "provider_cli_path: %s\n", report->provider_cli_path);
    yvex_cli_out_writef(fp, "provider_cli_status: %s\n", report->provider_cli_status);
    yvex_cli_out_writef(fp, "auth_mode: %s\n", model_download_auth_mode_name(options->auth_mode));
    yvex_cli_out_writef(fp, "auth_state: %s\n", report->auth_state);
    yvex_cli_out_writef(fp, "token_env_name: %s\n", report->token_env_name);
    yvex_cli_out_writef(fp, "token_value_redacted: %s\n", token_present ? "true" : "false");
    if (strcmp(report->provider, "github") == 0) {
        yvex_cli_out_writef(fp, "command: %s release download", report->provider_cli_path);
        if (options->release && options->release[0]) yvex_cli_out_writef(fp, " %s", options->release);
        yvex_cli_out_writef(fp, " --repo %s --pattern %s --dir %s --skip-existing",
                report->repo_id, options->asset ? options->asset : "", report->local_source_dir);
    } else {
        yvex_cli_out_writef(fp, "command: %s download %s --revision %s --local-dir %s",
                report->provider_cli_path, report->repo_id, report->revision,
                report->local_source_dir);
        for (i = 0; i < model_download_effective_include_count(options); ++i) {
            yvex_cli_out_writef(fp, " --include %s", model_download_effective_include_at(options, i));
        }
        for (i = 0; i < model_download_effective_exclude_count(options); ++i) {
            yvex_cli_out_writef(fp, " --exclude %s", model_download_effective_exclude_at(options, i));
        }
        yvex_cli_out_writef(fp, " --max-workers %llu", options->max_workers);
        if (options->dry_run) yvex_cli_out_writef(fp, " --dry-run");
        if (token_present) yvex_cli_out_writef(fp, " --token <redacted>");
    }
    yvex_cli_out_writef(fp, "\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_receipt",
                        "cannot close receipt: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int model_download_write_control_receipt(const char *path,
                                                const yvex_cli_models_download_options *options,
                                                const yvex_model_download_report *report,
                                                const char *status,
                                                yvex_error *err)
{
    FILE *fp;

    if (!path || !options || !report || !status) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_control_receipt",
                       "path, options, report, and status are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_control_receipt",
                        "cannot open download receipt: %s", path);
        return YVEX_ERR_IO;
    }
    yvex_cli_out_writef(fp, "{\n");
    write_field(fp, "  ", "schema", "yvex.model_download.active.v1", 1);
    write_field(fp, "  ", "target_id", report->target_id, 1);
    write_field(fp, "  ", "family", report->family, 1);
    write_field(fp, "  ", "provider", report->provider, 1);
    write_field(fp, "  ", "repo_id", report->repo_id, 1);
    write_field(fp, "  ", "revision", report->revision, 1);
    write_field(fp, "  ", "local_source_dir", report->local_source_dir, 1);
    model_download_write_pattern_array(fp, "include_patterns", options, 1, 1);
    model_download_write_pattern_array(fp, "exclude_patterns", options, 0, 1);
    write_field(fp, "  ", "auth_mode", model_download_auth_mode_name(options->auth_mode), 1);
    write_field(fp, "  ", "progress_mode",
                model_download_progress_mode_name(options->progress_mode), 1);
    write_field(fp, "  ", "started_at", report->created_at, 1);
    yvex_cli_out_writef(fp, "  \"yvex_pid\": %lld,\n", (long long)getpid());
    yvex_cli_out_writef(fp, "  \"provider_pid\": %lld,\n", (long long)report->provider_pid);
    yvex_cli_out_writef(fp, "  \"provider_pgid\": %lld,\n", (long long)report->provider_process_group);
    write_field(fp, "  ", "stdout_log", report->stdout_log_path, 1);
    write_field(fp, "  ", "stderr_log", report->stderr_log_path, 1);
    write_field(fp, "  ", "status", status, 0);
    yvex_cli_out_writef(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download_control_receipt",
                        "cannot close download receipt: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int model_download_finalize_control_receipt(
    const yvex_cli_models_download_options *options,
    const yvex_model_download_report *report,
    const char *status)
{
    yvex_error err;
    int rc = YVEX_OK;

    yvex_error_clear(&err);
    if (report && report->last_receipt_path[0]) {
        rc = model_download_write_control_receipt(report->last_receipt_path,
                                                  options,
                                                  report,
                                                  status,
                                                  &err);
    }
    if (report && report->active_receipt_path[0]) {
        (void)unlink(report->active_receipt_path);
    }
    return rc;
}

int model_download_write_all_fd(int fd, const void *buf, size_t len)
{
    const unsigned char *p = (const unsigned char *)buf;

    while (len > 0u) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        if (n == 0) return 0;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return 1;
}

void model_download_mirror_provider_bytes(int fd,
                                                 const char *buf,
                                                 size_t len,
                                                 int normalize_cr)
{
    size_t i;

    if (fd < 0 || !buf || len == 0u) return;
    if (!normalize_cr) {
        (void)model_download_write_all_fd(fd, buf, len);
        return;
    }
    for (i = 0; i < len; ++i) {
        char c = buf[i] == '\r' ? '\n' : buf[i];
        (void)model_download_write_all_fd(fd, &c, 1u);
    }
}

void model_download_print_start_progress(
    const yvex_model_download_report *report,
    yvex_model_download_progress_mode effective_mode)
{
    if (!report || effective_mode == YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) {
        return;
    }
    yvex_cli_out_writef(stdout, "model-download: start target=%s\n", report->target_id);
    yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
    yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
    yvex_cli_out_writef(stdout, "source: %s\n", report->local_source_dir);
    yvex_cli_out_writef(stdout, "stage: account-provider %s\n", report->stage_account_provider);
    yvex_cli_out_writef(stdout, "stage: download running\n");
    fflush(stdout);
}

void model_download_format_bytes(char *out,
                                        size_t cap,
                                        unsigned long long bytes)
{
    static const char *const units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    double value = (double)bytes;
    unsigned int unit = 0u;

    if (!out || cap == 0u) return;
    while (value >= 1024.0 && unit + 1u < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit++;
    }
    if (unit == 0u) {
        snprintf(out, cap, "%llu B", bytes);
    } else if (value >= 100.0) {
        snprintf(out, cap, "%.0f %s", value, units[unit]);
    } else {
        snprintf(out, cap, "%.1f %s", value, units[unit]);
    }
}

void model_download_format_elapsed(char *out,
                                          size_t cap,
                                          unsigned long long seconds)
{
    unsigned long long hours = seconds / 3600ull;
    unsigned long long minutes = (seconds / 60ull) % 60ull;
    unsigned long long secs = seconds % 60ull;

    if (!out || cap == 0u) return;
    if (hours > 0ull) {
        snprintf(out, cap, "%lluh%02llum%02llus", hours, minutes, secs);
    } else if (minutes > 0ull) {
        snprintf(out, cap, "%llum%02llus", minutes, secs);
    } else {
        snprintf(out, cap, "%llus", secs);
    }
}

void model_download_short_file_name(char *out,
                                           size_t cap,
                                           const char *path)
{
    const char *name;
    size_t len;
    size_t head;
    size_t tail;

    if (!out || cap == 0u) return;
    if (!path || !path[0]) {
        snprintf(out, cap, "none");
        return;
    }
    name = strrchr(path, '/');
    name = name ? name + 1 : path;
    len = strlen(name);
    if (len + 1u <= cap && len <= 48u) {
        snprintf(out, cap, "%s", name);
        return;
    }
    if (cap < 16u) {
        snprintf(out, cap, "%.*s", (int)(cap - 1u), name);
        return;
    }
    head = 18u;
    tail = 22u;
    if (head + tail + 4u > cap) {
        head = (cap - 4u) / 2u;
        tail = cap - 4u - head;
    }
    if (tail > len) tail = len;
    snprintf(out, cap, "%.*s...%s", (int)head, name, name + len - tail);
}

static int model_download_tick_scan_changed(const yvex_model_download_report *report,
                                            const yvex_model_download_source_scan *scan)
{
    if (!report || !scan || report->tick_count == 0ull) return 1;
    return report->tick_last_file_count != scan->file_count ||
           report->tick_last_safetensors_count != scan->safetensors_count ||
           report->tick_last_partial_file_count != scan->partial_file_count ||
           report->tick_last_cache_file_count != scan->cache_file_count ||
           report->tick_last_total_regular_file_bytes != scan->total_regular_file_bytes ||
           report->tick_last_largest_file_bytes != scan->largest_file_bytes ||
           strcmp(report->tick_last_largest_file_name, scan->largest_file_name) != 0;
}

void model_download_print_tick_progress(const char *source_dir,
                                               time_t started_at,
                                               yvex_model_download_report *report,
                                               yvex_model_download_progress_mode effective_mode)
{
    yvex_model_download_source_scan scan;
    yvex_error err;
    time_t now;
    unsigned long long elapsed = 0ull;
    unsigned long long delta_bytes = 0ull;
    char elapsed_text[32];
    char total_text[32];
    char delta_text[32];
    char largest_text[32];
    char largest_name[64];
    const char *delta_sign = "+";

    if (!report || effective_mode == YVEX_MODEL_DOWNLOAD_PROGRESS_OFF) {
        return;
    }
    memset(&scan, 0, sizeof(scan));
    yvex_error_clear(&err);
    (void)model_download_scan_source(source_dir, &scan, &err);
    now = time(NULL);
    if (now != (time_t)-1 && started_at != (time_t)-1 && now >= started_at) {
        elapsed = (unsigned long long)(now - started_at);
    }
    if (!model_download_tick_scan_changed(report, &scan) &&
        elapsed < report->tick_last_elapsed_seconds + 60ull) {
        return;
    }
    if (report->tick_count > 0ull) {
        if (scan.total_regular_file_bytes >= report->tick_last_total_regular_file_bytes) {
            delta_bytes = scan.total_regular_file_bytes - report->tick_last_total_regular_file_bytes;
        } else {
            delta_sign = "-";
            delta_bytes = report->tick_last_total_regular_file_bytes - scan.total_regular_file_bytes;
        }
    }
    model_download_format_elapsed(elapsed_text, sizeof(elapsed_text), elapsed);
    model_download_format_bytes(total_text, sizeof(total_text), scan.total_regular_file_bytes);
    model_download_format_bytes(delta_text, sizeof(delta_text), delta_bytes);
    model_download_format_bytes(largest_text, sizeof(largest_text), scan.largest_file_bytes);
    model_download_short_file_name(largest_name, sizeof(largest_name),
                                   scan.largest_file_name[0] ? scan.largest_file_name : "none");
    yvex_cli_out_writef(stdout, "tick: elapsed=%s files=%llu partial=%llu safetensors=%llu bytes=%s delta=%s%s largest=%s (%s)\n",
           elapsed_text,
           scan.file_count,
           scan.partial_file_count,
           scan.safetensors_count,
           total_text,
           delta_sign,
           delta_text,
           largest_name,
           largest_text);
    fflush(stdout);
    report->tick_last_elapsed_seconds = elapsed;
    report->tick_last_file_count = scan.file_count;
    report->tick_last_safetensors_count = scan.safetensors_count;
    report->tick_last_partial_file_count = scan.partial_file_count;
    report->tick_last_cache_file_count = scan.cache_file_count;
    report->tick_last_total_regular_file_bytes = scan.total_regular_file_bytes;
    report->tick_last_largest_file_bytes = scan.largest_file_bytes;
    snprintf(report->tick_last_largest_file_name,
             sizeof(report->tick_last_largest_file_name),
             "%s", scan.largest_file_name);
    report->tick_count++;
}

volatile sig_atomic_t yvex_model_download_provider_signal_seen = 0;

static void model_download_provider_signal_handler(int signo)
{
    if ((signo == SIGINT || signo == SIGTERM) &&
        yvex_model_download_provider_signal_seen == 0) {
        yvex_model_download_provider_signal_seen = signo;
    }
}

int model_download_install_provider_signal_handlers(struct sigaction *old_int,
                                                           struct sigaction *old_term,
                                                           yvex_error *err)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = model_download_provider_signal_handler;
    sigemptyset(&action.sa_mask);

    yvex_model_download_provider_signal_seen = 0;
    if (sigaction(SIGINT, &action, old_int) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot install SIGINT handler: %s", strerror(errno));
        return 0;
    }
    if (sigaction(SIGTERM, &action, old_term) != 0) {
        (void)sigaction(SIGINT, old_int, NULL);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot install SIGTERM handler: %s", strerror(errno));
        return 0;
    }
    return 1;
}

void model_download_restore_provider_signal_handlers(const struct sigaction *old_int,
                                                            const struct sigaction *old_term)
{
    if (old_int) (void)sigaction(SIGINT, old_int, NULL);
    if (old_term) (void)sigaction(SIGTERM, old_term, NULL);
    yvex_model_download_provider_signal_seen = 0;
}

void model_download_reset_child_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);
}

int model_download_set_nonblocking(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void model_download_record_child_exit_status(yvex_model_download_report *report,
                                                    int child_status)
{
    int code;
    int sig;

    if (!report) return;
    report->child_terminated = 1;
    if (WIFEXITED(child_status)) {
        code = WEXITSTATUS(child_status);
        if (report->interrupted &&
            (code == 0 ||
             (report->interrupt_signal > 0 && code == 128 + report->interrupt_signal))) {
            snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                     "interrupted");
        } else if (code == 0) {
            snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                     "exited");
        } else {
            snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                     "terminated");
        }
        return;
    }
    if (WIFSIGNALED(child_status)) {
        sig = WTERMSIG(child_status);
        snprintf(report->child_exit_status, sizeof(report->child_exit_status), "%s",
                 report->interrupted &&
                 (sig == report->interrupt_signal || sig == SIGINT || sig == SIGTERM)
                 ? "interrupted"
                 : "terminated");
        return;
    }
    snprintf(report->child_exit_status, sizeof(report->child_exit_status), "unknown");
}

void model_download_mark_provider_interrupted(yvex_model_download_report *report,
                                                     int signo,
                                                     pid_t pgid)
{
    int kill_rc;

    if (!report || report->interrupted) return;
    report->interrupted = 1;
    report->interrupt_signal = signo;
    report->partial_source_preserved = 1;
    report->lock_files_deleted = 0;
    if (pgid > 0) {
        kill_rc = kill(-pgid, signo);
        if (kill_rc == 0 || errno == ESRCH) {
            report->signal_forwarded = 1;
        }
    }
}

void model_download_orphan_check(yvex_model_download_report *report)
{
    pid_t pgid;

    if (!report) return;
    report->orphan_check_performed = 1;
    pgid = report->provider_process_group;
    if (pgid <= 0) {
        snprintf(report->orphan_check_status, sizeof(report->orphan_check_status),
                 "unknown");
        return;
    }
    if (kill(-pgid, 0) == 0) {
        snprintf(report->orphan_check_status, sizeof(report->orphan_check_status),
                 "fail");
        return;
    }
    snprintf(report->orphan_check_status, sizeof(report->orphan_check_status), "%s",
             errno == ESRCH ? "pass" : "unknown");
}
