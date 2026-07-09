/*
 * yvex_models_download_process_surface.c - models download provider process surface.
 * Owner: src/cli/model_artifacts
 * Owns: provider CLI process orchestration for the models download command family.
 * Does not own: provider account domain, model registry storage, runtime generation, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing provider CLI behavior.
 * Boundary: provider process completion is source download evidence only.
 */
#include "yvex_models_download_surface.h"

static int yvex_provider_process_run_streaming(const char *const *args,
                                               const char *stdout_log_path,
                                               const char *stderr_log_path,
                                               const yvex_cli_models_download_options *options,
                                               yvex_model_download_progress_mode effective_mode,
                                               unsigned long long tick_seconds,
                                               const char *local_source_dir,
                                               yvex_model_download_report *report,
                                               yvex_error *err)
{
    int stdout_pipe[2] = { -1, -1 };
    int stderr_pipe[2] = { -1, -1 };
    int stdout_log_fd = -1;
    int stderr_log_fd = -1;
    int stdout_open = 1;
    int stderr_open = 1;
    int child_exited = 0;
    int shutdown_signal_sent = 0;
    int kill_signal_sent = 0;
    int child_status = 0;
    int mirror_provider;
    int normalize_cr;
    time_t started_at;
    time_t next_tick;
    time_t shutdown_deadline = (time_t)-1;
    pid_t pid;
    pid_t pgid = -1;
    struct sigaction old_int;
    struct sigaction old_term;
    int signal_handlers_installed = 0;
    unsigned long long shutdown_timeout_seconds;

    if (!args || !args[0] || !stdout_log_path || !stderr_log_path || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "provider_process",
                       "provider args, log paths, and report are required");
        return -1;
    }
    shutdown_timeout_seconds = options && options->timeout_seconds
        ? options->timeout_seconds
        : (unsigned long long)YVEX_MODEL_DOWNLOAD_INTERRUPT_TIMEOUT_SECONDS;

    stdout_log_fd = open(stdout_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (stdout_log_fd < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot open stdout log: %s", stdout_log_path);
        return -1;
    }
    stderr_log_fd = open(stderr_log_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (stderr_log_fd < 0) {
        close(stdout_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot open stderr log: %s", stderr_log_path);
        return -1;
    }
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
        if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
        if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
        if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "pipe failed: %s", strerror(errno));
        return -1;
    }
    if (!model_download_set_nonblocking(stdout_pipe[0]) ||
        !model_download_set_nonblocking(stderr_pipe[0])) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "cannot make provider pipes nonblocking: %s",
                        strerror(errno));
        return -1;
    }

    mirror_provider = effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
                      effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_LOG;
    normalize_cr = effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE;
    started_at = time(NULL);
    next_tick = started_at == (time_t)-1
        ? (time_t)-1
        : started_at + (time_t)(tick_seconds ? tick_seconds : 1ull);

    if (!model_download_install_provider_signal_handlers(&old_int, &old_term, err)) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        return -1;
    }
    signal_handlers_installed = 1;

    pid = fork();
    if (pid < 0) {
        model_download_restore_provider_signal_handlers(&old_int, &old_term);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                        "fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        model_download_reset_child_signal_handlers();
        if (setpgid(0, 0) != 0) _exit(127);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) _exit(127);
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) _exit(127);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stdout_log_fd);
        close(stderr_log_fd);
        execv(args[0], (char *const *)args);
        _exit(127);
    }

    report->provider_pid = pid;
    (void)setpgid(pid, pid);
    pgid = getpgid(pid);
    if (pgid <= 0) pgid = pid;
    report->provider_process_group = pgid;
    if (report->active_receipt_path[0]) {
        yvex_error receipt_err;
        yvex_error_clear(&receipt_err);
        (void)model_download_write_control_receipt(report->active_receipt_path,
                                                   options,
                                                   report,
                                                   "running",
                                                   &receipt_err);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    stdout_pipe[1] = -1;
    stderr_pipe[1] = -1;

    while (stdout_open || stderr_open || !child_exited) {
        struct pollfd fds[2];
        int which[2];
        nfds_t nfds = 0;
        time_t now;
        int timeout_ms = 1000;
        int poll_rc;
        pid_t waited;
        sig_atomic_t seen_signal;

        if (!child_exited) {
            waited = waitpid(pid, &child_status, WNOHANG);
            if (waited == pid) {
                child_exited = 1;
                model_download_record_child_exit_status(report, child_status);
            } else if (waited < 0 && errno == ECHILD) {
                child_exited = 1;
                snprintf(report->child_exit_status, sizeof(report->child_exit_status),
                         "unknown");
            } else if (waited < 0 && errno != EINTR) {
                child_exited = 1;
            }
        }

        now = time(NULL);
        seen_signal = yvex_model_download_provider_signal_seen;
        if (seen_signal != 0 && !report->interrupted && !child_exited) {
            model_download_mark_provider_interrupted(report, (int)seen_signal, pgid);
            shutdown_signal_sent = 1;
            shutdown_deadline = now == (time_t)-1
                ? (time_t)-1
                : now + (time_t)shutdown_timeout_seconds;
        }
        if (report->interrupted &&
            shutdown_signal_sent &&
            !kill_signal_sent &&
            shutdown_deadline != (time_t)-1 &&
            now != (time_t)-1 &&
            now >= shutdown_deadline &&
            (!child_exited || stdout_open || stderr_open)) {
            if (pgid > 0 && kill(-pgid, SIGKILL) == 0) {
                report->child_killed_after_timeout = 1;
            }
            kill_signal_sent = 1;
        }
        if (effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
            tick_seconds > 0ull &&
            now != (time_t)-1 &&
            next_tick != (time_t)-1 &&
            now >= next_tick &&
            !child_exited) {
            model_download_print_tick_progress(local_source_dir, started_at, report, effective_mode);
            next_tick = now + (time_t)tick_seconds;
        }
        if (effective_mode != YVEX_MODEL_DOWNLOAD_PROGRESS_OFF &&
            tick_seconds > 0ull &&
            now != (time_t)-1 &&
            next_tick != (time_t)-1 &&
            next_tick > now) {
            time_t diff = next_tick - now;
            timeout_ms = diff > 1 ? 1000 : (int)(diff * 1000);
            if (timeout_ms <= 0) timeout_ms = 100;
        }
        if (report->interrupted && timeout_ms > 100) {
            timeout_ms = 100;
        }

        if (stdout_open) {
            fds[nfds].fd = stdout_pipe[0];
            fds[nfds].events = POLLIN | POLLHUP | POLLERR;
            fds[nfds].revents = 0;
            which[nfds] = 1;
            nfds++;
        }
        if (stderr_open) {
            fds[nfds].fd = stderr_pipe[0];
            fds[nfds].events = POLLIN | POLLHUP | POLLERR;
            fds[nfds].revents = 0;
            which[nfds] = 2;
            nfds++;
        }

        poll_rc = poll(nfds ? fds : NULL, nfds, timeout_ms);
        if (poll_rc < 0) {
            if (errno == EINTR) continue;
            if (!child_exited && pgid > 0) {
                time_t deadline;
                (void)kill(-pgid, SIGTERM);
                deadline = time(NULL) + (time_t)shutdown_timeout_seconds;
                while (1) {
                    waited = waitpid(pid, &child_status, WNOHANG);
                    if (waited == pid) {
                        model_download_record_child_exit_status(report, child_status);
                        child_exited = 1;
                        break;
                    }
                    if (waited < 0 && errno != EINTR) {
                        child_exited = 1;
                        break;
                    }
                    if (time(NULL) >= deadline) {
                        (void)kill(-pgid, SIGKILL);
                        report->child_killed_after_timeout = 1;
                        while (waitpid(pid, &child_status, 0) < 0 && errno == EINTR) {
                        }
                        model_download_record_child_exit_status(report, child_status);
                        child_exited = 1;
                        break;
                    }
                    (void)poll(NULL, 0, 100);
                }
            }
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            close(stdout_log_fd);
            close(stderr_log_fd);
            model_download_orphan_check(report);
            if (signal_handlers_installed) {
                model_download_restore_provider_signal_handlers(&old_int, &old_term);
            }
            yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                            "poll failed: %s", strerror(errno));
            return -1;
        }
        if (poll_rc > 0) {
            nfds_t i;
            for (i = 0; i < nfds; ++i) {
                char buf[4096];
                ssize_t got;
                int stream_kind = which[i];
                int read_fd = stream_kind == 1 ? stdout_pipe[0] : stderr_pipe[0];
                int log_fd = stream_kind == 1 ? stdout_log_fd : stderr_log_fd;
                int mirror_fd = stream_kind == 1 ? STDOUT_FILENO : STDERR_FILENO;

                if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                    continue;
                }
                got = read(read_fd, buf, sizeof(buf));
                if (got > 0) {
                    (void)model_download_write_all_fd(log_fd, buf, (size_t)got);
                    if (stream_kind == 1) {
                        report->stdout_bytes += (unsigned long long)got;
                        report->stdout_streamed = 1;
                    } else {
                        report->stderr_bytes += (unsigned long long)got;
                        report->stderr_streamed = 1;
                    }
                    if (mirror_provider) {
                        model_download_mirror_provider_bytes(mirror_fd, buf, (size_t)got, normalize_cr);
                        if (stream_kind == 1) fflush(stdout);
                        else fflush(stderr);
                    }
                } else if (got == 0) {
                    if (stream_kind == 1 && stdout_open) {
                        close(stdout_pipe[0]);
                        stdout_pipe[0] = -1;
                        stdout_open = 0;
                    } else if (stream_kind == 2 && stderr_open) {
                        close(stderr_pipe[0]);
                        stderr_pipe[0] = -1;
                        stderr_open = 0;
                    }
                } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    if (stream_kind == 1 && stdout_open) {
                        close(stdout_pipe[0]);
                        stdout_pipe[0] = -1;
                        stdout_open = 0;
                    } else if (stream_kind == 2 && stderr_open) {
                        close(stderr_pipe[0]);
                        stderr_pipe[0] = -1;
                        stderr_open = 0;
                    }
                }
            }
        }
    }

    if (!child_exited) {
        while (waitpid(pid, &child_status, 0) < 0) {
            if (errno == EINTR) continue;
            close(stdout_log_fd);
            close(stderr_log_fd);
            if (signal_handlers_installed) {
                model_download_restore_provider_signal_handlers(&old_int, &old_term);
            }
            yvex_error_setf(err, YVEX_ERR_IO, "provider_process",
                            "waitpid failed: %s", strerror(errno));
            return -1;
        }
        model_download_record_child_exit_status(report, child_status);
    }
    close(stdout_log_fd);
    close(stderr_log_fd);
    model_download_orphan_check(report);
    if (signal_handlers_installed) {
        model_download_restore_provider_signal_handlers(&old_int, &old_term);
    }

    if (WIFEXITED(child_status)) return WEXITSTATUS(child_status);
    if (WIFSIGNALED(child_status)) return 128 + WTERMSIG(child_status);
    return 1;
}

int model_download_run_hf(const yvex_cli_models_download_options *options,
                                 yvex_model_download_report *report,
                                 const char *token_value,
                                 yvex_error *err)
{
    const char *args[180];
    char max_workers_buf[32];
    unsigned int i;
    unsigned int n = 0;
    yvex_model_download_progress_mode effective_mode;

    if (!options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_hf",
                       "download options and report are required");
        return -1;
    }
    snprintf(max_workers_buf, sizeof(max_workers_buf), "%llu", options->max_workers);
    args[n++] = report->provider_cli_path;
    args[n++] = "download";
    args[n++] = report->repo_id;
    args[n++] = "--revision";
    args[n++] = report->revision;
    args[n++] = "--local-dir";
    args[n++] = report->local_source_dir;
    for (i = 0; i < model_download_effective_include_count(options); ++i) {
        args[n++] = "--include";
        args[n++] = model_download_effective_include_at(options, i);
    }
    for (i = 0; i < model_download_effective_exclude_count(options); ++i) {
        args[n++] = "--exclude";
        args[n++] = model_download_effective_exclude_at(options, i);
    }
    args[n++] = "--max-workers";
    args[n++] = max_workers_buf;
    if (options->dry_run) {
        args[n++] = "--dry-run";
    }
    if (token_value && token_value[0]) {
        args[n++] = "--token";
        args[n++] = token_value;
    }
    args[n] = NULL;

    effective_mode = model_download_effective_progress_mode(options->progress_mode);
    model_download_print_start_progress(report, effective_mode);
    return yvex_provider_process_run_streaming(args,
                                               report->stdout_log_path,
                                               report->stderr_log_path,
                                               options,
                                               effective_mode,
                                               options->tick_seconds,
                                               report->local_source_dir,
                                               report,
                                               err);
}

int model_download_run_github(const yvex_cli_models_download_options *options,
                                     const yvex_model_download_report *report,
                                     yvex_error *err)
{
    yvex_account_command_options command;
    unsigned int n = 0u;

    if (!options || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download_github",
                       "download options and report are required");
        return -1;
    }
    memset(&command, 0, sizeof(command));
    command.args[n++] = report->provider_cli_path;
    command.args[n++] = "release";
    command.args[n++] = "download";
    if (options->release && options->release[0]) {
        command.args[n++] = options->release;
    }
    command.args[n++] = "--repo";
    command.args[n++] = report->repo_id;
    command.args[n++] = "--pattern";
    command.args[n++] = options->asset;
    command.args[n++] = "--dir";
    command.args[n++] = report->local_source_dir;
    command.args[n++] = "--skip-existing";
    command.args[n] = NULL;
    command.stdout_path = report->stdout_log_path;
    command.stderr_path = report->stderr_log_path;
    return yvex_accounts_run_provider_command(&command, err);
}

static void model_download_print_audit_patterns(const yvex_cli_models_download_options *options)
{
    unsigned int i;

    for (i = 0; i < model_download_effective_include_count(options); ++i) {
        yvex_cli_out_writef(stdout, "include.%u: %s\n", i, model_download_effective_include_at(options, i));
    }
    for (i = 0; i < model_download_effective_exclude_count(options); ++i) {
        yvex_cli_out_writef(stdout, "exclude.%u: %s\n", i, model_download_effective_exclude_at(options, i));
    }
}

static void model_download_print_normal(const yvex_cli_models_download_options *options,
                                        const yvex_model_download_report *report)
{
    char bytes_text[32];
    char largest_text[32];
    char largest_name[64];

    if (strcmp(report->status, "model-download-dry-run") == 0) {
        yvex_cli_out_writef(stdout, "model-download: dry-run target=%s\n", report->target_id);
        yvex_cli_out_writef(stdout, "family: %s\n", report->family);
        yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
        yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
        yvex_cli_out_writef(stdout, "source: %s\n", report->local_source_dir);
        yvex_cli_out_writef(stdout, "account_provider: %s\n", report->stage_account_provider);
        yvex_cli_out_writef(stdout, "action: planned\n");
        yvex_cli_out_writef(stdout, "manifest: skipped\n");
        yvex_cli_out_writef(stdout, "native_inventory: skipped\n");
        yvex_cli_out_writef(stdout, "boundary: no payload downloaded, runtime unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", report->status);
        return;
    }
    if (strcmp(report->status, "model-download-blocked") == 0 ||
        strcmp(report->status, "model-download-resume-blocked") == 0) {
        yvex_cli_out_writef(stdout, "%s target=%s\n",
               strcmp(report->status, "model-download-resume-blocked") == 0
                   ? "model-download-resume: blocked"
                   : "model-download: blocked",
               report->target_id);
        yvex_cli_out_writef(stdout, "family: %s\n", report->family);
        yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
        yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
        yvex_cli_out_writef(stdout, "stage: account-provider %s\n", report->stage_account_provider);
        yvex_cli_out_writef(stdout, "auth_state: %s\n", report->auth_state);
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", report->top_blocker[0] ? report->top_blocker : "unknown");
        yvex_cli_out_writef(stdout, "next: %s\n", report->error[0] ? report->error : "resolve provider account state");
        yvex_cli_out_writef(stdout, "boundary: no payload downloaded, runtime unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", report->status);
        return;
    }
    if (strcmp(report->status, "model-download-pass") == 0 ||
        strcmp(report->status, "model-download-resume-pass") == 0) {
        yvex_cli_out_writef(stdout, "%s target=%s\n",
               strcmp(report->status, "model-download-resume-pass") == 0
                   ? "model-download-resume: pass"
                   : "model-download: pass",
               report->target_id);
        yvex_cli_out_writef(stdout, "family: %s\n", report->family);
        yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
        yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
        yvex_cli_out_writef(stdout, "source: %s\n", report->local_source_dir);
        yvex_cli_out_writef(stdout, "account_provider: %s\n", report->stage_account_provider);
        model_download_format_bytes(bytes_text, sizeof(bytes_text),
                                    report->source_scan.total_regular_file_bytes);
        yvex_cli_out_writef(stdout, "files: %llu partial=%llu safetensors=%llu bytes=%s\n",
               report->source_scan.file_count,
               report->source_scan.partial_file_count,
               report->source_scan.safetensors_count,
               bytes_text);
        yvex_cli_out_writef(stdout, "manifest: %s\n",
               report->source_manifest_written ? report->manifest_path : "skipped");
        yvex_cli_out_writef(stdout, "native_inventory: %s\n",
               report->native_inventory_written ? report->native_inventory_path : "skipped");
        yvex_cli_out_writef(stdout, "boundary: source tensors only, runtime unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", report->status);
        return;
    }
    if (strcmp(report->status, "model-download-interrupted") == 0) {
        yvex_cli_out_writef(stdout, "model-download: interrupted target=%s\n", report->target_id);
        yvex_cli_out_writef(stdout, "family: %s\n", report->family);
        yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
        yvex_cli_out_writef(stdout, "source: %s\n", report->local_source_dir);
        yvex_cli_out_writef(stdout, "stage: download interrupted\n");
        yvex_cli_out_writef(stdout, "signal: %s\n", model_download_signal_name(report->interrupt_signal));
        yvex_cli_out_writef(stdout, "child_signal_forwarded: %s\n", report->signal_forwarded ? "true" : "false");
        yvex_cli_out_writef(stdout, "child_exit_status: %s\n", report->child_exit_status);
        model_download_format_bytes(bytes_text, sizeof(bytes_text),
                                    report->source_scan.total_regular_file_bytes);
        model_download_format_bytes(largest_text, sizeof(largest_text),
                                    report->source_scan.largest_file_bytes);
        model_download_short_file_name(largest_name, sizeof(largest_name),
                                       report->source_scan.largest_file_name);
        yvex_cli_out_writef(stdout, "files: %llu partial=%llu safetensors=%llu bytes=%s\n",
               report->source_scan.file_count,
               report->source_scan.partial_file_count,
               report->source_scan.safetensors_count,
               bytes_text);
        yvex_cli_out_writef(stdout, "largest: %s (%s)\n", largest_name, largest_text);
        yvex_cli_out_writef(stdout, "cleanup: preserved-partial-source\n");
        yvex_cli_out_writef(stdout, "lock_cleanup: not-attempted\n");
        yvex_cli_out_writef(stdout, "partial_source_preserved: %s\n",
               report->partial_source_preserved ? "true" : "false");
        yvex_cli_out_writef(stdout, "lock_files_deleted: %s\n", report->lock_files_deleted ? "true" : "false");
        yvex_cli_out_writef(stdout, "stdout_log: %s\n", report->stdout_log_path);
        yvex_cli_out_writef(stdout, "stderr_log: %s\n", report->stderr_log_path);
        yvex_cli_out_writef(stdout, "boundary: partial source files may exist; runtime unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", report->status);
        return;
    }

    yvex_cli_out_writef(stdout, "model-download: fail target=%s\n", report->target_id);
    yvex_cli_out_writef(stdout, "family: %s\n", report->family);
    yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
    yvex_cli_out_writef(stdout, "repo: %s\n", report->repo_id);
    yvex_cli_out_writef(stdout, "source: %s\n", report->local_source_dir);
    yvex_cli_out_writef(stdout, "hf_exit_code: %d\n", report->hf_exit_code);
    yvex_cli_out_writef(stdout, "stderr_log: %s\n", report->stderr_log_path);
    if (report->error[0]) yvex_cli_out_writef(stdout, "reason: %s\n", report->error);
    yvex_cli_out_writef(stdout, "boundary: no runtime, no generation, no benchmark\n");
    yvex_cli_out_writef(stdout, "status: %s\n", report->status);
    (void)options;
}

static void model_download_print_table(const yvex_model_download_report *report)
{
    char bytes_text[32];

    model_download_format_bytes(bytes_text, sizeof(bytes_text),
                                report->source_scan.total_regular_file_bytes);
    yvex_cli_out_writef(stdout, "TARGET       PROVIDER     FAMILY  ACCOUNT  STATUS                       FILES  PARTIAL  SAFETENSORS  BYTES\n");
    yvex_cli_out_writef(stdout, "%-12s %-11s  %-6s  %-7s  %-27s  %5llu  %7llu  %11llu  %s\n",
           report->target_id,
           report->provider,
           report->family,
           report->stage_account_provider,
           report->status,
           report->source_scan.file_count,
           report->source_scan.partial_file_count,
           report->source_scan.safetensors_count,
           bytes_text);
    yvex_cli_out_writef(stdout, "status: %s\n", report->status);
}

static void model_download_print_audit(const yvex_cli_models_download_options *options,
                                       const yvex_model_download_report *report)
{
    yvex_cli_out_writef(stdout, "models: download\n");
    yvex_cli_out_writef(stdout, "status: %s\n", report->status);
    yvex_cli_out_writef(stdout, "target_id: %s\n", report->target_id);
    yvex_cli_out_writef(stdout, "family: %s\n", report->family);
    yvex_cli_out_writef(stdout, "provider: %s\n", report->provider);
    yvex_cli_out_writef(stdout, "repo_id: %s\n", report->repo_id);
    yvex_cli_out_writef(stdout, "revision: %s\n", report->revision);
    yvex_cli_out_writef(stdout, "local_source_dir: %s\n", report->local_source_dir);
    yvex_cli_out_writef(stdout, "models_root: %s\n", report->models_root);
    yvex_cli_out_writef(stdout, "models_root_source: %s\n", report->models_root_source);
    yvex_cli_out_writef(stdout, "provider_cli: %s\n", report->provider_cli_path[0] ? report->provider_cli_path : "missing");
    yvex_cli_out_writef(stdout, "provider_cli_source: %s\n", report->provider_cli_source[0] ? report->provider_cli_source : "none");
    yvex_cli_out_writef(stdout, "provider_cli_status: %s\n", report->provider_cli_status);
    yvex_cli_out_writef(stdout, "account_hint: %s\n", report->account_hint);
    yvex_cli_out_writef(stdout, "credential_source: %s\n", report->credential_source);
    yvex_cli_out_writef(stdout, "accounts_state_path: %s\n", report->accounts_state_path);
    yvex_cli_out_writef(stdout, "auth_mode: %s\n", model_download_auth_mode_name(options->auth_mode));
    yvex_cli_out_writef(stdout, "hf_cli: %s\n", report->hf_cli_path[0] ? report->hf_cli_path : "missing");
    yvex_cli_out_writef(stdout, "hf_cli_source: %s\n", report->hf_cli_source[0] ? report->hf_cli_source : "none");
    yvex_cli_out_writef(stdout, "hf_exit_code: %d\n", report->hf_exit_code);
    yvex_cli_out_writef(stdout, "provider_exit_code: %d\n", report->provider_exit_code);
    yvex_cli_out_writef(stdout, "auth_state: %s\n", report->auth_state);
    yvex_cli_out_writef(stdout, "token_env_name: %s\n", report->token_env_name);
    yvex_cli_out_writef(stdout, "token_value_redacted: %s\n",
           strcmp(report->auth_state, "env-token-present") == 0 ? "true" : "false");
    model_download_print_audit_patterns(options);
    yvex_cli_out_writef(stdout, "progress_mode: %s\n", model_download_progress_mode_name(options->progress_mode));
    yvex_cli_out_writef(stdout, "tick_seconds: %llu\n", options->tick_seconds);
    yvex_cli_out_writef(stdout, "tick_count: %llu\n", report->tick_count);
    yvex_cli_out_writef(stdout, "stdout_streamed: %s\n", report->stdout_streamed ? "true" : "false");
    yvex_cli_out_writef(stdout, "stderr_streamed: %s\n", report->stderr_streamed ? "true" : "false");
    yvex_cli_out_writef(stdout, "stdout_bytes: %llu\n", report->stdout_bytes);
    yvex_cli_out_writef(stdout, "stderr_bytes: %llu\n", report->stderr_bytes);
    model_stage_print("resolve-target", report->stage_resolve_target);
    model_stage_print("resolve-paths", report->stage_resolve_paths);
    model_stage_print("prepare-dirs", report->stage_prepare_dirs);
    model_stage_print("account-provider", report->stage_account_provider);
    model_stage_print("provider-cli", report->stage_provider_cli);
    model_stage_print("hf-cli", report->stage_hf_cli);
    model_stage_print("download", report->stage_download);
    model_stage_print("progress-stream", report->stage_progress_stream);
    model_stage_print("progress-ticks", report->stage_progress_ticks);
    model_stage_print("source-scan", report->stage_source_scan);
    model_stage_print("source-manifest", report->stage_source_manifest);
    model_stage_print("native-inventory", report->stage_native_inventory);
    model_stage_print("sidecar", report->stage_sidecar);
    yvex_cli_out_writef(stdout, "provider_pid: %lld\n", (long long)report->provider_pid);
    yvex_cli_out_writef(stdout, "provider_process_group: %lld\n", (long long)report->provider_process_group);
    yvex_cli_out_writef(stdout, "interrupted: %s\n", report->interrupted ? "true" : "false");
    yvex_cli_out_writef(stdout, "interrupt_signal: %s\n", model_download_signal_name(report->interrupt_signal));
    yvex_cli_out_writef(stdout, "signal: %s\n", model_download_signal_name(report->interrupt_signal));
    yvex_cli_out_writef(stdout, "signal_forwarded: %s\n", report->signal_forwarded ? "true" : "false");
    yvex_cli_out_writef(stdout, "child_signal_forwarded: %s\n", report->signal_forwarded ? "true" : "false");
    yvex_cli_out_writef(stdout, "child_terminated: %s\n", report->child_terminated ? "true" : "false");
    yvex_cli_out_writef(stdout, "child_killed_after_timeout: %s\n",
           report->child_killed_after_timeout ? "true" : "false");
    yvex_cli_out_writef(stdout, "child_exit_status: %s\n", report->child_exit_status);
    yvex_cli_out_writef(stdout, "orphan_check_performed: %s\n",
           report->orphan_check_performed ? "true" : "false");
    yvex_cli_out_writef(stdout, "orphan_check_status: %s\n", report->orphan_check_status);
    yvex_cli_out_writef(stdout, "partial_source_preserved: %s\n",
           report->partial_source_preserved ? "true" : "false");
    yvex_cli_out_writef(stdout, "lock_cleanup: not-attempted\n");
    yvex_cli_out_writef(stdout, "lock_files_deleted: %s\n", report->lock_files_deleted ? "true" : "false");
    yvex_cli_out_writef(stdout, "source_file_count: %llu\n", report->source_scan.file_count);
    yvex_cli_out_writef(stdout, "file_count: %llu\n", report->source_scan.file_count);
    yvex_cli_out_writef(stdout, "safetensors_count: %llu\n", report->source_scan.safetensors_count);
    yvex_cli_out_writef(stdout, "config_present: %s\n", report->source_scan.config_present ? "true" : "false");
    yvex_cli_out_writef(stdout, "tokenizer_present: %s\n", report->source_scan.tokenizer_present ? "true" : "false");
    yvex_cli_out_writef(stdout, "total_regular_file_bytes: %llu\n", report->source_scan.total_regular_file_bytes);
    yvex_cli_out_writef(stdout, "largest_file_name: %s\n",
           report->source_scan.largest_file_name[0] ? report->source_scan.largest_file_name : "none");
    yvex_cli_out_writef(stdout, "largest_file_bytes: %llu\n", report->source_scan.largest_file_bytes);
    yvex_cli_out_writef(stdout, "manifest_path: %s\n", report->manifest_path);
    yvex_cli_out_writef(stdout, "native_inventory_path: %s\n", report->native_inventory_path);
    yvex_cli_out_writef(stdout, "download_report_path: %s\n", report->download_report_path);
    yvex_cli_out_writef(stdout, "registry_path: %s\n", report->registry_path);
    yvex_cli_out_writef(stdout, "receipt_path: %s\n", report->receipt_path);
    yvex_cli_out_writef(stdout, "stdout_log: %s\n", report->stdout_log_path);
    yvex_cli_out_writef(stdout, "stderr_log: %s\n", report->stderr_log_path);
    yvex_cli_out_writef(stdout, "created_at: %s\n", report->created_at);
    yvex_cli_out_writef(stdout, "yvex_version: %s\n", yvex_version_string());
    yvex_cli_out_writef(stdout, "upstream_identity_verified: false\n");
    yvex_cli_out_writef(stdout, "remote_lookup_performed: false\n");
    yvex_cli_out_writef(stdout, "payload_hash_verified: false\n");
    yvex_cli_out_writef(stdout, "payload_loaded: false\n");
    yvex_cli_out_writef(stdout, "gguf_created: false\n");
    yvex_cli_out_writef(stdout, "materialized: false\n");
    yvex_cli_out_writef(stdout, "runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "eval: unsupported\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    if (report->top_blocker[0]) yvex_cli_out_writef(stdout, "top_blocker: %s\n", report->top_blocker);
    if (report->error[0]) yvex_cli_out_writef(stdout, "reason: %s\n", report->error);
}

static void model_download_print(const yvex_cli_models_download_options *options,
                                 const yvex_model_download_report *report)
{
    if (options && options->output_mode == YVEX_MODELS_OUTPUT_AUDIT) {
        model_download_print_audit(options, report);
    } else if (options && options->output_mode == YVEX_MODELS_OUTPUT_TABLE) {
        model_download_print_table(report);
    } else {
        model_download_print_normal(options, report);
    }
}

int model_download_finish(const yvex_cli_models_download_options *options,
                                 yvex_model_download_report *report)
{
    model_download_print(options, report);
    if (strcmp(report->status, "model-download-pass") == 0 ||
        strcmp(report->status, "model-download-resume-pass") == 0 ||
        strcmp(report->status, "model-download-dry-run") == 0) {
        return 0;
    }
    if (strcmp(report->status, "model-download-blocked") == 0 ||
        strcmp(report->status, "model-download-resume-blocked") == 0) {
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    return 1;
}

int model_download_read_small_file(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u) return 0;
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) return 0;
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

long long model_download_json_i64_field(const char *text, const char *key)
{
    char needle[96];
    const char *p;

    if (!text || !key) return -1;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) return -1;
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return strtoll(p, NULL, 10);
}

int model_download_json_string_field(const char *text,
                                            const char *key,
                                            char *out,
                                            size_t cap)
{
    char needle[96];
    const char *p;
    const char *q;
    size_t len;

    if (out && cap > 0u) out[0] = '\0';
    if (!text || !key || !out || cap == 0u) return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return 0;
    p++;
    q = strchr(p, '"');
    if (!q) return 0;
    len = (size_t)(q - p);
    if (len >= cap) len = cap - 1u;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

static int model_download_identity_family_hint(const char *target,
                                               char *family,
                                               size_t family_cap)
{
    if (family && family_cap > 0u) family[0] = '\0';
    if (!target || !family || family_cap == 0u) return 0;
    if (model_download_name_starts_with(target, "qwen")) {
        snprintf(family, family_cap, "qwen");
        return 1;
    }
    if (model_download_name_starts_with(target, "gemma")) {
        snprintf(family, family_cap, "gemma");
        return 1;
    }
    if (model_download_name_starts_with(target, "deepseek")) {
        snprintf(family, family_cap, "deepseek");
        return 1;
    }
    if (model_download_name_starts_with(target, "glm")) {
        snprintf(family, family_cap, "glm");
        return 1;
    }
    return 0;
}

int model_download_identity_paths(const char *target,
                                         const char *family,
                                         const yvex_operator_paths *operator_paths,
                                         yvex_model_download_resolved_target *out,
                                         yvex_error *err)
{
    char reports_family_dir[YVEX_PATH_CAP];
    char registry_family_dir[YVEX_PATH_CAP];
    char file_name[256];
    int rc;

    if (!target || !family || !operator_paths || !out) return 0;
    rc = path_join2(reports_family_dir, sizeof(reports_family_dir),
                    operator_paths->reports_root, family, err,
                    "models_download_identity");
    if (rc == YVEX_OK) {
        rc = path_join2(registry_family_dir, sizeof(registry_family_dir),
                        operator_paths->registry_root, family, err,
                        "models_download_identity");
    }
    if (rc != YVEX_OK) return 0;

    snprintf(file_name, sizeof(file_name), "%s.download.json", target);
    rc = path_join2(out->registry_path, sizeof(out->registry_path),
                    registry_family_dir, file_name, err,
                    "models_download_identity");
    snprintf(file_name, sizeof(file_name), "%s.download-report.json", target);
    if (rc == YVEX_OK) {
        rc = path_join2(out->download_report_path,
                        sizeof(out->download_report_path),
                        reports_family_dir, file_name, err,
                        "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.source-manifest.json", target);
    if (rc == YVEX_OK) {
        rc = path_join2(out->manifest_path, sizeof(out->manifest_path),
                        reports_family_dir, file_name, err,
                        "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.native-inventory.json", target);
    if (rc == YVEX_OK) {
        rc = path_join2(out->native_inventory_path,
                        sizeof(out->native_inventory_path),
                        reports_family_dir, file_name, err,
                        "models_download_identity");
    }
    return rc == YVEX_OK;
}

int model_download_read_identity_file(const char *path,
                                             const char *target,
                                             const char *family,
                                             yvex_model_download_resolved_target *out)
{
    char buf[16384];
    char parsed_target[128];
    char parsed_family[32];
    char parsed_repo[256];
    char parsed_provider[32];
    char parsed_revision[128];
    char parsed_source[YVEX_PATH_CAP];

    if (!path || !path[0] || !target || !family || !out) return 0;
    if (access(path, F_OK) != 0) return 0;
    if (!model_download_read_small_file(path, buf, sizeof(buf))) return 0;

    memset(parsed_target, 0, sizeof(parsed_target));
    memset(parsed_family, 0, sizeof(parsed_family));
    memset(parsed_repo, 0, sizeof(parsed_repo));
    memset(parsed_provider, 0, sizeof(parsed_provider));
    memset(parsed_revision, 0, sizeof(parsed_revision));
    memset(parsed_source, 0, sizeof(parsed_source));

    model_download_json_string_field(buf, "target_id", parsed_target,
                                     sizeof(parsed_target));
    if (parsed_target[0] && strcmp(parsed_target, target) != 0) return 0;
    model_download_json_string_field(buf, "family", parsed_family,
                                     sizeof(parsed_family));
    if (parsed_family[0] && strcmp(parsed_family, family) != 0) return 0;
    model_download_json_string_field(buf, "repo_id", parsed_repo,
                                     sizeof(parsed_repo));
    if (!parsed_repo[0]) {
        model_download_json_string_field(buf, "repo", parsed_repo,
                                         sizeof(parsed_repo));
    }
    model_download_json_string_field(buf, "provider", parsed_provider,
                                     sizeof(parsed_provider));
    model_download_json_string_field(buf, "revision", parsed_revision,
                                     sizeof(parsed_revision));
    model_download_json_string_field(buf, "local_source_dir", parsed_source,
                                     sizeof(parsed_source));
    if (!parsed_source[0]) {
        model_download_json_string_field(buf, "path", parsed_source,
                                         sizeof(parsed_source));
    }

    snprintf(out->target_id, sizeof(out->target_id), "%s",
             parsed_target[0] ? parsed_target : target);
    snprintf(out->family, sizeof(out->family), "%s",
             parsed_family[0] ? parsed_family : family);
    snprintf(out->repo_id, sizeof(out->repo_id), "%s",
             parsed_repo[0] ? parsed_repo : "unknown");
    snprintf(out->provider, sizeof(out->provider), "%s",
             parsed_provider[0] ? parsed_provider : "huggingface");
    snprintf(out->revision, sizeof(out->revision), "%s",
             parsed_revision[0] ? parsed_revision : "main");
    snprintf(out->local_name, sizeof(out->local_name), "%s", target);
    if (parsed_source[0]) {
        snprintf(out->local_source_dir, sizeof(out->local_source_dir), "%s",
                 parsed_source);
    }
    out->found = 1;
    return 1;
}

int model_download_resolve_downloaded_target(
    const char *target,
    const yvex_operator_paths *operator_paths,
    yvex_model_download_resolved_target *out,
    yvex_error *err)
{
    static const char *families[] = { "qwen", "gemma", "deepseek", "glm", "github" };
    char hinted_family[32];
    unsigned long pass;

    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!target || !target[0] || !operator_paths) return 0;
    model_download_identity_family_hint(target, hinted_family, sizeof(hinted_family));

    for (pass = 0; pass < 2u; ++pass) {
        unsigned long i;
        for (i = 0; i < sizeof(families) / sizeof(families[0]); ++i) {
            const char *family = families[i];
            char hf_family_dir[YVEX_PATH_CAP];
            yvex_model_download_resolved_target candidate;

            if (pass == 0) {
                if (!hinted_family[0] || strcmp(family, hinted_family) != 0) {
                    continue;
                }
            } else if (hinted_family[0] && strcmp(family, hinted_family) == 0) {
                continue;
            }

            memset(&candidate, 0, sizeof(candidate));
            if (!model_download_identity_paths(target, family, operator_paths,
                                               &candidate, err)) {
                continue;
            }
            if (model_download_read_identity_file(candidate.registry_path,
                                                  target, family, &candidate) ||
                model_download_read_identity_file(candidate.download_report_path,
                                                  target, family, &candidate) ||
                model_download_read_identity_file(candidate.manifest_path,
                                                  target, family, &candidate)) {
                if (!candidate.local_source_dir[0] &&
                    strcmp(candidate.provider, "github") != 0 &&
                    path_join2(hf_family_dir, sizeof(hf_family_dir),
                               operator_paths->hf_root, family, err,
                               "models_download_identity") == YVEX_OK) {
                    (void)path_join2(candidate.local_source_dir,
                                     sizeof(candidate.local_source_dir),
                                     hf_family_dir, target, err,
                                     "models_download_identity");
                }
                if (!candidate.target_id[0]) {
                    snprintf(candidate.target_id, sizeof(candidate.target_id), "%s", target);
                }
                if (!candidate.family[0]) {
                    snprintf(candidate.family, sizeof(candidate.family), "%s", family);
                }
                if (!candidate.provider[0]) {
                    snprintf(candidate.provider, sizeof(candidate.provider), "huggingface");
                }
                if (!candidate.revision[0]) {
                    snprintf(candidate.revision, sizeof(candidate.revision), "main");
                }
                if (!candidate.local_name[0]) {
                    snprintf(candidate.local_name, sizeof(candidate.local_name), "%s", target);
                }
                *out = candidate;
                out->found = 1;
                return 1;
            }
        }
    }
    return 0;
}

/* GGUF artifact discovery and prepare preflight UX. */

#define YVEX_MODELS_ARTIFACT_ROWS_CAP 256u
