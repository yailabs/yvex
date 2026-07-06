/*
 * yvex_accounts.c - Local provider account boundary.
 *
 * This file owns local provider CLI discovery, non-secret account observation,
 * interactive login handoff, and provider command execution for source
 * acquisition. It does not store provider tokens and does not implement OAuth.
 */

#define _POSIX_C_SOURCE 200809L

#include <yvex/accounts.h>
#include "yvex_console_private.h"
#include "yvex/version.h"
#include "yvex_cli_out.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef enum {
    YVEX_ACCOUNTS_OUTPUT_NORMAL = 0,
    YVEX_ACCOUNTS_OUTPUT_TABLE,
    YVEX_ACCOUNTS_OUTPUT_AUDIT
} yvex_accounts_output_mode;

typedef struct {
    yvex_accounts_output_mode output_mode;
    const char *token_env_name;
    const char *cli_override;
    const char *hostname;
    const char *git_protocol;
    const char *scopes;
    int force;
    int web;
    int device;
    int add_to_git_credential;
    int token_stdin;
    int skip_ssh_key;
} yvex_accounts_cli_options;

static int accounts_path_format(char *dst,
                                size_t cap,
                                yvex_error *err,
                                const char *where,
                                const char *a,
                                const char *b)
{
    int n;

    if (!dst || cap == 0u || !a) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path output and base are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (b) {
        n = snprintf(dst, cap, "%s/%s", a, b);
    } else {
        n = snprintf(dst, cap, "%s", a);
    }
    if (n < 0 || (size_t)n >= cap) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "path exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int accounts_mkdir_one(const char *path, yvex_error *err)
{
    struct stat st;

    if (mkdir(path, 0777) == 0) return YVEX_OK;
    if (errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts", "mkdir failed for %s: %s", path, strerror(errno));
        return YVEX_ERR_IO;
    }
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts", "path exists and is not a directory: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int accounts_mkdir_p(const char *path, yvex_error *err)
{
    char tmp[YVEX_PATH_CAP];
    char *p;
    size_t len;
    int rc;

    if (!path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts", "directory path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = accounts_path_format(tmp, sizeof(tmp), err, "accounts", path, NULL);
    if (rc != YVEX_OK) return rc;
    len = strlen(tmp);
    while (len > 1u && tmp[len - 1u] == '/') {
        tmp[--len] = '\0';
    }
    for (p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            rc = accounts_mkdir_one(tmp, err);
            if (rc != YVEX_OK) return rc;
            *p = '/';
        }
    }
    return accounts_mkdir_one(tmp, err);
}

static int accounts_mkdir_parent(const char *path, yvex_error *err)
{
    char tmp[YVEX_PATH_CAP];
    char *slash;
    int rc;

    if (!path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts", "file path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = accounts_path_format(tmp, sizeof(tmp), err, "accounts", path, NULL);
    if (rc != YVEX_OK) return rc;
    slash = strrchr(tmp, '/');
    if (!slash) return YVEX_OK;
    if (slash == tmp) slash[1] = '\0';
    else *slash = '\0';
    return accounts_mkdir_p(tmp, err);
}

static void accounts_timestamp(char *out, size_t cap)
{
    time_t now;
    struct tm tm_utc;

    if (!out || cap == 0u) return;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1 || !gmtime_r(&now, &tm_utc)) {
        snprintf(out, cap, "unknown");
        return;
    }
    if (strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        snprintf(out, cap, "unknown");
    }
}

static void accounts_json_string(FILE *fp, const char *value)
{
    if (!value) value = "";
    fputc('"', fp);
    while (*value) {
        unsigned char ch = (unsigned char)*value++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            yvex_cli_out_fputs("\\n", fp);
        } else if (ch == '\r') {
            yvex_cli_out_fputs("\\r", fp);
        } else if (ch == '\t') {
            yvex_cli_out_fputs("\\t", fp);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

static void accounts_json_field(FILE *fp,
                                const char *indent,
                                const char *key,
                                const char *value,
                                int comma)
{
    yvex_cli_out_fputs(indent, fp);
    yvex_cli_out_writef(fp, "\"%s\": ", key);
    accounts_json_string(fp, value);
    yvex_cli_out_writef(fp, "%s\n", comma ? "," : "");
}

static void accounts_json_bool(FILE *fp,
                               const char *indent,
                               const char *key,
                               int value,
                               int comma)
{
    yvex_cli_out_fputs(indent, fp);
    yvex_cli_out_writef(fp, "\"%s\": %s%s\n", key, value ? "true" : "false", comma ? "," : "");
}

static int accounts_state_path(char *out, size_t cap, yvex_error *err)
{
    yvex_paths paths;
    int rc;

    memset(&paths, 0, sizeof(paths));
    rc = yvex_paths_default(&paths, err);
    if (rc != YVEX_OK) return rc;
    return accounts_path_format(out, cap, err, "accounts", paths.config_dir, "accounts.local.json");
}

int yvex_account_provider_from_name(const char *name, yvex_account_provider *out)
{
    if (!name || !out) return 0;
    if (strcmp(name, "huggingface") == 0 || strcmp(name, "hf") == 0) {
        *out = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
        return 1;
    }
    if (strcmp(name, "github") == 0 || strcmp(name, "gh") == 0) {
        *out = YVEX_ACCOUNT_PROVIDER_GITHUB;
        return 1;
    }
    return 0;
}

const char *yvex_account_provider_name(yvex_account_provider provider)
{
    switch (provider) {
    case YVEX_ACCOUNT_PROVIDER_HUGGINGFACE: return "huggingface";
    case YVEX_ACCOUNT_PROVIDER_GITHUB: return "github";
    default: return "unknown";
    }
}

const char *yvex_account_default_token_env(yvex_account_provider provider)
{
    switch (provider) {
    case YVEX_ACCOUNT_PROVIDER_HUGGINGFACE: return "HF_TOKEN";
    case YVEX_ACCOUNT_PROVIDER_GITHUB: return "GH_TOKEN";
    default: return "";
    }
}

static const char *accounts_provider_env_override(yvex_account_provider provider)
{
    switch (provider) {
    case YVEX_ACCOUNT_PROVIDER_HUGGINGFACE: return "YVEX_HF_CLI";
    case YVEX_ACCOUNT_PROVIDER_GITHUB: return "YVEX_GH_CLI";
    default: return "";
    }
}

static const char *accounts_provider_binary(yvex_account_provider provider)
{
    switch (provider) {
    case YVEX_ACCOUNT_PROVIDER_HUGGINGFACE: return "hf";
    case YVEX_ACCOUNT_PROVIDER_GITHUB: return "gh";
    default: return "";
    }
}

static const char *accounts_provider_legacy_binary(yvex_account_provider provider)
{
    return provider == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE ? "huggingface-cli" : "";
}

static int accounts_find_in_path(const char *binary, char *out, size_t cap)
{
    const char *path_env;
    const char *start;

    if (!binary || !binary[0] || !out || cap == 0u) return 0;
    if (strchr(binary, '/')) {
        if (access(binary, X_OK) == 0) {
            snprintf(out, cap, "%s", binary);
            return 1;
        }
        return 0;
    }
    path_env = getenv("PATH");
    start = path_env ? path_env : "";
    while (start && *start) {
        char candidate[YVEX_PATH_CAP];
        const char *end = strchr(start, ':');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        const char *dir = len == 0u ? "." : start;
        int n;

        n = snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)len, dir, binary);
        if (n >= 0 && (size_t)n < sizeof(candidate) && access(candidate, X_OK) == 0) {
            snprintf(out, cap, "%s", candidate);
            return 1;
        }
        start = end ? end + 1 : NULL;
    }
    return 0;
}

static int accounts_find_cli(yvex_account_provider provider,
                             const char *explicit_cli,
                             char *out,
                             size_t cap,
                             char *source,
                             size_t source_cap)
{
    const char *env_name = accounts_provider_env_override(provider);
    const char *env_value = env_name && env_name[0] ? getenv(env_name) : NULL;
    const char *binary = accounts_provider_binary(provider);
    const char *legacy = accounts_provider_legacy_binary(provider);

    if (!out || cap == 0u || !source || source_cap == 0u) return 0;
    out[0] = '\0';
    source[0] = '\0';
    if (explicit_cli && explicit_cli[0]) {
        if (accounts_find_in_path(explicit_cli, out, cap)) {
            snprintf(source, source_cap, "--cli");
            return 1;
        }
        return 0;
    }
    if (env_value && env_value[0]) {
        if (accounts_find_in_path(env_value, out, cap)) {
            snprintf(source, source_cap, "%s", env_name);
            return 1;
        }
        return 0;
    }
    if (accounts_find_in_path(binary, out, cap)) {
        snprintf(source, source_cap, "PATH");
        return 1;
    }
    if (legacy && legacy[0] && accounts_find_in_path(legacy, out, cap)) {
        snprintf(source, source_cap, "PATH-legacy");
        return 1;
    }
    return 0;
}

static void accounts_first_line(char *value)
{
    char *p;

    if (!value) return;
    for (p = value; *p; ++p) {
        if (*p == '\n' || *p == '\r') {
            *p = '\0';
            break;
        }
    }
    while (*value == ' ' || *value == '\t') {
        memmove(value, value + 1, strlen(value));
    }
    for (p = value + strlen(value); p > value && (p[-1] == ' ' || p[-1] == '\t'); --p) {
        p[-1] = '\0';
    }
}

static int accounts_run_capture(const char *const *args,
                                char *out,
                                size_t out_cap,
                                int *exit_code,
                                yvex_error *err)
{
    int pipefd[2];
    int devnull = -1;
    pid_t pid;
    int status;
    size_t used = 0u;

    if (!args || !args[0] || !out || out_cap == 0u || !exit_code) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts_exec", "arguments and outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    out[0] = '\0';
    *exit_code = 1;
    if (pipe(pipefd) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "pipe failed: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    devnull = open("/dev/null", O_WRONLY);
    pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        if (devnull >= 0) close(devnull);
        yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "fork failed: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        if (devnull >= 0) dup2(devnull, STDERR_FILENO);
        close(pipefd[1]);
        if (devnull >= 0) close(devnull);
        execv(args[0], (char *const *)args);
        _exit(127);
    }
    close(pipefd[1]);
    if (devnull >= 0) close(devnull);
    for (;;) {
        char buf[512];
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            close(pipefd[0]);
            yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "read failed: %s", strerror(errno));
            return YVEX_ERR_IO;
        }
        if (n == 0) break;
        if (used + 1u < out_cap) {
            size_t take = (size_t)n;
            if (take > out_cap - used - 1u) take = out_cap - used - 1u;
            memcpy(out + used, buf, take);
            used += take;
            out[used] = '\0';
        }
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "waitpid failed: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    if (WIFEXITED(status)) *exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) *exit_code = 128 + WTERMSIG(status);
    else *exit_code = 1;
    return YVEX_OK;
}

int yvex_accounts_run_provider_command(const yvex_account_command_options *options,
                                       yvex_error *err)
{
    int stdout_fd = -1;
    int stderr_fd = -1;
    int status;
    pid_t pid;

    if (!options || !options->argv[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts_exec", "provider command arguments are required");
        return -1;
    }
    if (options->stdout_path && options->stdout_path[0]) {
        stdout_fd = open(options->stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (stdout_fd < 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "cannot open stdout log: %s", options->stdout_path);
            return -1;
        }
    }
    if (options->stderr_path && options->stderr_path[0]) {
        stderr_fd = open(options->stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (stderr_fd < 0) {
            if (stdout_fd >= 0) close(stdout_fd);
            yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "cannot open stderr log: %s", options->stderr_path);
            return -1;
        }
    }
    pid = fork();
    if (pid < 0) {
        if (stdout_fd >= 0) close(stdout_fd);
        if (stderr_fd >= 0) close(stderr_fd);
        yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "fork failed: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        if (stdout_fd >= 0) dup2(stdout_fd, STDOUT_FILENO);
        if (stderr_fd >= 0) dup2(stderr_fd, STDERR_FILENO);
        if (stdout_fd >= 0) close(stdout_fd);
        if (stderr_fd >= 0) close(stderr_fd);
        execv(options->argv[0], (char *const *)options->argv);
        _exit(127);
    }
    if (stdout_fd >= 0) close(stdout_fd);
    if (stderr_fd >= 0) close(stderr_fd);
    if (waitpid(pid, &status, 0) < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts_exec", "waitpid failed: %s", strerror(errno));
        return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

static int accounts_run_foreground(const char *const *args, yvex_error *err)
{
    yvex_account_command_options command;
    unsigned int i;

    memset(&command, 0, sizeof(command));
    for (i = 0u; i + 1u < YVEX_ACCOUNT_ARG_CAP && args && args[i]; ++i) {
        command.argv[i] = args[i];
    }
    command.argv[i] = NULL;
    return yvex_accounts_run_provider_command(&command, err);
}

int yvex_account_observe(const yvex_account_observe_options *options,
                         yvex_account_observation *out,
                         yvex_error *err)
{
    const char *token_env;
    const char *token_value;
    const char *args[5];
    char capture[512];
    int exit_code = 1;
    int rc;

    if (!options || !out || options->provider == YVEX_ACCOUNT_PROVIDER_UNKNOWN) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts", "provider and observation output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->provider = options->provider;
    snprintf(out->provider_name, sizeof(out->provider_name), "%s",
             yvex_account_provider_name(options->provider));
    token_env = options->token_env_name && options->token_env_name[0]
        ? options->token_env_name
        : yvex_account_default_token_env(options->provider);
    snprintf(out->token_env_name, sizeof(out->token_env_name), "%s", token_env);
    token_value = token_env && token_env[0] ? getenv(token_env) : NULL;
    out->token_env_present = token_value && token_value[0];
    out->token_value_redacted = out->token_env_present ? 1 : 0;
    out->raw_token_stored_by_yvex = 0;
    accounts_timestamp(out->last_checked_at, sizeof(out->last_checked_at));
    if (accounts_state_path(out->state_path, sizeof(out->state_path), err) != YVEX_OK) {
        return yvex_error_code(err);
    }

    if (!accounts_find_cli(options->provider, options->cli_override,
                           out->cli_path, sizeof(out->cli_path),
                           out->cli_source, sizeof(out->cli_source))) {
        snprintf(out->cli_status, sizeof(out->cli_status), "missing");
        snprintf(out->auth_state, sizeof(out->auth_state), "blocked");
        snprintf(out->credential_source, sizeof(out->credential_source),
                 out->token_env_present ? "environment" : "none");
        snprintf(out->account_hint, sizeof(out->account_hint),
                 out->token_env_present ? "env-token" : "unknown");
        snprintf(out->status, sizeof(out->status), "account-provider-blocked");
        snprintf(out->top_blocker, sizeof(out->top_blocker), "missing-%s-cli", out->provider_name);
        if (options->provider == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE) {
            snprintf(out->top_blocker, sizeof(out->top_blocker), "missing-huggingface-cli");
            snprintf(out->next, sizeof(out->next), "install Hugging Face CLI and retry");
        } else {
            snprintf(out->top_blocker, sizeof(out->top_blocker), "missing-github-cli");
            snprintf(out->next, sizeof(out->next), "install GitHub CLI and retry");
        }
        return YVEX_OK;
    }

    out->cli_present = 1;
    snprintf(out->cli_status, sizeof(out->cli_status), "present");
    if (out->token_env_present) {
        snprintf(out->auth_state, sizeof(out->auth_state), "env-token-present");
        snprintf(out->credential_source, sizeof(out->credential_source), "environment");
        snprintf(out->account_hint, sizeof(out->account_hint), "env:%s", out->token_env_name);
        snprintf(out->status, sizeof(out->status), "account-provider-pass");
        return YVEX_OK;
    }

    args[0] = out->cli_path;
    if (options->provider == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE) {
        args[1] = "auth";
        args[2] = "whoami";
        args[3] = NULL;
    } else {
        args[1] = "auth";
        args[2] = "status";
        args[3] = NULL;
    }
    rc = accounts_run_capture(args, capture, sizeof(capture), &exit_code, err);
    if (rc != YVEX_OK) return rc;
    out->command_exit_code = exit_code;
    if (exit_code == 0) {
        accounts_first_line(capture);
        snprintf(out->auth_state, sizeof(out->auth_state), "logged-in");
        snprintf(out->credential_source, sizeof(out->credential_source), "provider-store");
        snprintf(out->account_hint, sizeof(out->account_hint), "%.*s",
                 (int)sizeof(out->account_hint) - 1,
                 capture[0] ? capture : "provider-store");
        snprintf(out->status, sizeof(out->status), "account-provider-pass");
    } else {
        snprintf(out->auth_state, sizeof(out->auth_state), "not-logged-in");
        snprintf(out->credential_source, sizeof(out->credential_source), "none");
        snprintf(out->account_hint, sizeof(out->account_hint), "unknown");
        snprintf(out->status, sizeof(out->status), "account-provider-blocked");
        snprintf(out->top_blocker, sizeof(out->top_blocker), "provider-login-required");
        snprintf(out->next, sizeof(out->next), "yvex accounts login %s", out->provider_name);
    }
    return YVEX_OK;
}

int yvex_account_write_state(const yvex_account_observation *observations,
                             unsigned long count,
                             yvex_error *err)
{
    char path[YVEX_PATH_CAP];
    FILE *fp;
    unsigned long i;
    int rc;

    if (!observations || count == 0u) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts", "observations are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = accounts_state_path(path, sizeof(path), err);
    if (rc != YVEX_OK) return rc;
    rc = accounts_mkdir_parent(path, err);
    if (rc != YVEX_OK) return rc;
    fp = fopen(path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts", "cannot open account state: %s", path);
        return YVEX_ERR_IO;
    }
    yvex_cli_out_writef(fp, "{\n");
    accounts_json_field(fp, "  ", "schema", "yvex.accounts.v1", 1);
    yvex_cli_out_writef(fp, "  \"providers\": [\n");
    for (i = 0; i < count; ++i) {
        const yvex_account_observation *obs = &observations[i];
        yvex_cli_out_writef(fp, "    {\n");
        accounts_json_field(fp, "      ", "provider", obs->provider_name, 1);
        accounts_json_field(fp, "      ", "cli", obs->cli_path[0] ? obs->cli_path : "missing", 1);
        accounts_json_field(fp, "      ", "cli_status", obs->cli_status, 1);
        accounts_json_field(fp, "      ", "auth_state", obs->auth_state, 1);
        accounts_json_field(fp, "      ", "account_hint", obs->account_hint, 1);
        accounts_json_field(fp, "      ", "credential_source", obs->credential_source, 1);
        accounts_json_field(fp, "      ", "token_env_name", obs->token_env_name, 1);
        accounts_json_bool(fp, "      ", "token_value_redacted", obs->token_value_redacted, 1);
        accounts_json_field(fp, "      ", "last_checked_at", obs->last_checked_at, 1);
        accounts_json_bool(fp, "      ", "raw_token_stored_by_yvex", 0, 0);
        yvex_cli_out_writef(fp, "    }%s\n", i + 1u == count ? "" : ",");
    }
    yvex_cli_out_writef(fp, "  ]\n");
    yvex_cli_out_writef(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "accounts", "cannot close account state: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_account_ensure(const yvex_account_ensure_options *options,
                        yvex_account_observation *out,
                        yvex_error *err)
{
    yvex_account_observe_options observe;
    int rc;

    if (!options || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "accounts", "ensure options and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&observe, 0, sizeof(observe));
    observe.provider = options->provider;
    observe.cli_override = options->cli_override;
    observe.token_env_name = options->token_env_name;
    rc = yvex_account_observe(&observe, out, err);
    if (rc != YVEX_OK) return rc;
    if (!out->cli_present) return YVEX_OK;
    if (strcmp(out->auth_state, "logged-in") == 0 ||
        strcmp(out->auth_state, "env-token-present") == 0) {
        return yvex_account_write_state(out, 1u, err);
    }
    if (options->required || options->interactive == YVEX_ACCOUNT_INTERACTIVE_NEVER ||
        (options->interactive == YVEX_ACCOUNT_INTERACTIVE_AUTO &&
         (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)))) {
        snprintf(out->status, sizeof(out->status), "account-ensure-blocked");
        snprintf(out->top_blocker, sizeof(out->top_blocker), "provider-login-required");
        snprintf(out->next, sizeof(out->next), "yvex accounts login %s", out->provider_name);
        return YVEX_OK;
    }
    snprintf(out->status, sizeof(out->status), "account-ensure-login-required");
    snprintf(out->top_blocker, sizeof(out->top_blocker), "provider-login-required");
    snprintf(out->next, sizeof(out->next), "yvex accounts login %s", out->provider_name);
    return YVEX_OK;
}

static int accounts_parse_output_mode(const char *value, yvex_accounts_output_mode *mode)
{
    if (!value || !mode) return 0;
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_ACCOUNTS_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "table") == 0) {
        *mode = YVEX_ACCOUNTS_OUTPUT_TABLE;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_ACCOUNTS_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
}

static int accounts_parse_value(const char *command,
                                const char *flag,
                                int argc,
                                char **argv,
                                int *index,
                                const char **value)
{
    if (*index + 1 >= argc) {
        yvex_cli_out_writef(stderr, "accounts %s: %s requires a value\n", command, flag);
        return 2;
    }
    *value = argv[++(*index)];
    if (!*value || !(*value)[0] || strchr(*value, '\n') || strchr(*value, '\r')) {
        yvex_cli_out_writef(stderr, "accounts %s: %s value is empty or invalid\n", command, flag);
        return 2;
    }
    return 0;
}

static int accounts_parse_common(const char *command,
                                 int argc,
                                 char **argv,
                                 int start,
                                 yvex_accounts_cli_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->output_mode = YVEX_ACCOUNTS_OUTPUT_NORMAL;
    for (i = start; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_ACCOUNTS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            int rc = accounts_parse_value(command, "--output", argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (!accounts_parse_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "accounts %s: unsupported output mode: %s\n", command, value);
                return 2;
            }
        } else if (strcmp(argv[i], "--token-env") == 0) {
            int rc = accounts_parse_value(command, "--token-env", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->token_env_name = value;
        } else if (strcmp(argv[i], "--cli") == 0) {
            int rc = accounts_parse_value(command, "--cli", argc, argv, &i, &value);
            if (rc != 0) return rc;
            options->cli_override = value;
        } else if (strcmp(argv[i], "--json") == 0) {
            yvex_cli_out_writef(stderr, "accounts %s: JSON output is unsupported; use --output normal|table|audit\n", command);
            return 2;
        } else {
            yvex_cli_out_writef(stderr, "accounts %s: unknown option: %s\n", command, argv[i]);
            return 2;
        }
    }
    return 0;
}

static void accounts_print_observation_audit(const char *prefix,
                                             const yvex_account_observation *obs)
{
    yvex_cli_out_writef(stdout, "%sprovider: %s\n", prefix, obs->provider_name);
    yvex_cli_out_writef(stdout, "%scli: %s\n", prefix, obs->cli_path[0] ? obs->cli_path : "missing");
    yvex_cli_out_writef(stdout, "%scli_status: %s\n", prefix, obs->cli_status);
    yvex_cli_out_writef(stdout, "%sauth_state: %s\n", prefix, obs->auth_state);
    yvex_cli_out_writef(stdout, "%saccount_hint: %s\n", prefix, obs->account_hint);
    yvex_cli_out_writef(stdout, "%scredential_source: %s\n", prefix, obs->credential_source);
    yvex_cli_out_writef(stdout, "%stoken_env_name: %s\n", prefix, obs->token_env_name);
    yvex_cli_out_writef(stdout, "%stoken_value_redacted: %s\n", prefix, obs->token_value_redacted ? "true" : "false");
    yvex_cli_out_writef(stdout, "%sraw_token_stored_by_yvex: false\n", prefix);
    yvex_cli_out_writef(stdout, "%slast_checked_at: %s\n", prefix, obs->last_checked_at);
    if (obs->top_blocker[0]) yvex_cli_out_writef(stdout, "%stop_blocker: %s\n", prefix, obs->top_blocker);
    if (obs->next[0]) yvex_cli_out_writef(stdout, "%snext: %s\n", prefix, obs->next);
}

static void accounts_print_single(const char *surface,
                                  const yvex_account_observation *obs,
                                  yvex_accounts_output_mode mode)
{
    if (mode == YVEX_ACCOUNTS_OUTPUT_TABLE) {
        yvex_cli_out_writef(stdout, "PROVIDER      CLI      AUTH               ACCOUNT\n");
        yvex_cli_out_writef(stdout, "%-13s %-8s %-18s %s\n",
               obs->provider_name, obs->cli_status, obs->auth_state, obs->account_hint);
        yvex_cli_out_writef(stdout, "status: %s\n", obs->status);
        return;
    }
    if (mode == YVEX_ACCOUNTS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "accounts: %s\n", surface);
        accounts_print_observation_audit("", obs);
        yvex_cli_out_writef(stdout, "state_path: %s\n", obs->state_path);
        yvex_cli_out_writef(stdout, "boundary: local provider account state only; no tokens stored by YVEX\n");
        yvex_cli_out_writef(stdout, "status: %s\n", obs->status);
        return;
    }
    yvex_cli_out_writef(stdout, "provider: %s\n", obs->provider_name);
    yvex_cli_out_writef(stdout, "cli: %s\n", obs->cli_status);
    yvex_cli_out_writef(stdout, "auth_state: %s\n", obs->auth_state);
    yvex_cli_out_writef(stdout, "account: %s\n", obs->account_hint);
    if (obs->top_blocker[0]) yvex_cli_out_writef(stdout, "top_blocker: %s\n", obs->top_blocker);
    if (obs->next[0]) yvex_cli_out_writef(stdout, "next: %s\n", obs->next);
    yvex_cli_out_writef(stdout, "boundary: local provider account state only, no tokens stored by YVEX\n");
    yvex_cli_out_writef(stdout, "status: %s\n", obs->status);
}

static int command_accounts_providers(int argc, char **argv)
{
    yvex_accounts_cli_options options;
    yvex_account_observation observations[2];
    yvex_account_observe_options observe;
    yvex_error err;
    int rc;

    rc = accounts_parse_common("providers", argc, argv, 3, &options);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    memset(&observe, 0, sizeof(observe));
    observe.provider = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
    rc = yvex_account_observe(&observe, &observations[0], &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    observe.provider = YVEX_ACCOUNT_PROVIDER_GITHUB;
    rc = yvex_account_observe(&observe, &observations[1], &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    if (options.output_mode == YVEX_ACCOUNTS_OUTPUT_TABLE) {
        yvex_cli_out_writef(stdout, "PROVIDER      ALIAS  CLI      AUTH\n");
        yvex_cli_out_writef(stdout, "%-13s %-6s %-8s %s\n", "huggingface", "hf", observations[0].cli_status, observations[0].auth_state);
        yvex_cli_out_writef(stdout, "%-13s %-6s %-8s %s\n", "github", "gh", observations[1].cli_status, observations[1].auth_state);
    } else if (options.output_mode == YVEX_ACCOUNTS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "accounts: providers\n");
        accounts_print_observation_audit("provider_0_", &observations[0]);
        accounts_print_observation_audit("provider_1_", &observations[1]);
        yvex_cli_out_writef(stdout, "boundary: provider discovery only; no login, no token storage, no download\n");
    } else {
        yvex_cli_out_writef(stdout, "accounts: providers\n");
        yvex_cli_out_writef(stdout, "provider: huggingface alias=hf cli=%s\n", observations[0].cli_status);
        yvex_cli_out_writef(stdout, "provider: github alias=gh cli=%s\n", observations[1].cli_status);
        yvex_cli_out_writef(stdout, "boundary: provider discovery only; no login, no token storage, no download\n");
    }
    yvex_cli_out_writef(stdout, "status: account-providers\n");
    return 0;
}

static int command_accounts_status(int argc, char **argv)
{
    yvex_accounts_cli_options options;
    yvex_account_observation observations[2];
    yvex_account_observe_options observe;
    yvex_error err;
    int rc;

    rc = accounts_parse_common("status", argc, argv, 3, &options);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    memset(&observe, 0, sizeof(observe));
    observe.provider = YVEX_ACCOUNT_PROVIDER_HUGGINGFACE;
    rc = yvex_account_observe(&observe, &observations[0], &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    observe.provider = YVEX_ACCOUNT_PROVIDER_GITHUB;
    rc = yvex_account_observe(&observe, &observations[1], &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    (void)yvex_account_write_state(observations, 2u, &err);

    if (options.output_mode == YVEX_ACCOUNTS_OUTPUT_TABLE) {
        yvex_cli_out_writef(stdout, "PROVIDER      CLI      AUTH               ACCOUNT\n");
        yvex_cli_out_writef(stdout, "%-13s %-8s %-18s %s\n", observations[0].provider_name,
               observations[0].cli_status, observations[0].auth_state, observations[0].account_hint);
        yvex_cli_out_writef(stdout, "%-13s %-8s %-18s %s\n", observations[1].provider_name,
               observations[1].cli_status, observations[1].auth_state, observations[1].account_hint);
    } else if (options.output_mode == YVEX_ACCOUNTS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "accounts: status\n");
        accounts_print_observation_audit("provider_0_", &observations[0]);
        accounts_print_observation_audit("provider_1_", &observations[1]);
        yvex_cli_out_writef(stdout, "state_path: %s\n", observations[0].state_path);
        yvex_cli_out_writef(stdout, "raw_token_stored_by_yvex: false\n");
        yvex_cli_out_writef(stdout, "boundary: local provider account state only, no tokens stored by YVEX\n");
    } else {
        yvex_cli_out_writef(stdout, "accounts: provider status\n");
        yvex_cli_out_writef(stdout, "huggingface: %s cli=%s account=%s\n",
               observations[0].auth_state, observations[0].cli_status, observations[0].account_hint);
        yvex_cli_out_writef(stdout, "github: %s cli=%s account=%s\n",
               observations[1].auth_state, observations[1].cli_status, observations[1].account_hint);
        yvex_cli_out_writef(stdout, "boundary: local provider account state only, no tokens stored by YVEX\n");
    }
    yvex_cli_out_writef(stdout, "status: accounts-status\n");
    return 0;
}

static int command_accounts_whoami(int argc, char **argv)
{
    yvex_accounts_cli_options options;
    yvex_account_provider provider;
    yvex_account_observe_options observe;
    yvex_account_observation obs;
    yvex_error err;
    int rc;

    if (argc < 4) {
        yvex_cli_out_writef(stderr, "accounts whoami: PROVIDER is required\n");
        return 2;
    }
    if (!yvex_account_provider_from_name(argv[3], &provider)) {
        yvex_cli_out_writef(stderr, "accounts whoami: unsupported provider: %s\n", argv[3]);
        return 2;
    }
    rc = accounts_parse_common("whoami", argc, argv, 4, &options);
    if (rc != 0) return rc;
    memset(&observe, 0, sizeof(observe));
    observe.provider = provider;
    observe.cli_override = options.cli_override;
    observe.token_env_name = options.token_env_name;
    yvex_error_clear(&err);
    rc = yvex_account_observe(&observe, &obs, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (strcmp(obs.auth_state, "logged-in") == 0 ||
        strcmp(obs.auth_state, "env-token-present") == 0) {
        snprintf(obs.status, sizeof(obs.status), "account-whoami-pass");
        (void)yvex_account_write_state(&obs, 1u, &err);
        accounts_print_single("whoami", &obs, options.output_mode);
        return 0;
    }
    snprintf(obs.status, sizeof(obs.status), "account-whoami-blocked");
    accounts_print_single("whoami", &obs, options.output_mode);
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

static int accounts_parse_login_options(const char *command,
                                        int argc,
                                        char **argv,
                                        int start,
                                        yvex_accounts_cli_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->output_mode = YVEX_ACCOUNTS_OUTPUT_NORMAL;
    for (i = start; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--audit") == 0) {
            options->output_mode = YVEX_ACCOUNTS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0 ||
                   strcmp(argv[i], "--token-env") == 0 ||
                   strcmp(argv[i], "--cli") == 0 ||
                   strcmp(argv[i], "--hostname") == 0 ||
                   strcmp(argv[i], "--git-protocol") == 0 ||
                   strcmp(argv[i], "--scopes") == 0) {
            const char *flag = argv[i];
            int rc = accounts_parse_value(command, flag, argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--output") == 0) {
                if (!accounts_parse_output_mode(value, &options->output_mode)) {
                    yvex_cli_out_writef(stderr, "accounts %s: unsupported output mode: %s\n", command, value);
                    return 2;
                }
            } else if (strcmp(flag, "--token-env") == 0) {
                options->token_env_name = value;
            } else if (strcmp(flag, "--cli") == 0) {
                options->cli_override = value;
            } else if (strcmp(flag, "--hostname") == 0) {
                options->hostname = value;
            } else if (strcmp(flag, "--git-protocol") == 0) {
                if (strcmp(value, "https") != 0 && strcmp(value, "ssh") != 0) {
                    yvex_cli_out_writef(stderr, "accounts %s: --git-protocol requires https|ssh\n", command);
                    return 2;
                }
                options->git_protocol = value;
            } else {
                options->scopes = value;
            }
        } else if (strcmp(argv[i], "--force") == 0) {
            options->force = 1;
        } else if (strcmp(argv[i], "--web") == 0) {
            options->web = 1;
        } else if (strcmp(argv[i], "--device") == 0) {
            options->device = 1;
        } else if (strcmp(argv[i], "--add-to-git-credential") == 0) {
            options->add_to_git_credential = 1;
        } else if (strcmp(argv[i], "--token-stdin") == 0) {
            options->token_stdin = 1;
        } else if (strcmp(argv[i], "--skip-ssh-key") == 0) {
            options->skip_ssh_key = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            yvex_cli_out_writef(stderr, "accounts %s: JSON output is unsupported; use --output normal|table|audit\n", command);
            return 2;
        } else {
            yvex_cli_out_writef(stderr, "accounts %s: unknown option: %s\n", command, argv[i]);
            return 2;
        }
    }
    return 0;
}

static int command_accounts_login(int argc, char **argv)
{
    yvex_accounts_cli_options options;
    yvex_account_provider provider;
    yvex_account_observe_options observe;
    yvex_account_observation obs;
    yvex_error err;
    const char *token_env;
    const char *token_value = NULL;
    const char *args[24];
    unsigned int n = 0u;
    int exit_code = 0;
    int rc;

    if (argc < 4) {
        yvex_cli_out_writef(stderr, "accounts login: PROVIDER is required\n");
        return 2;
    }
    if (!yvex_account_provider_from_name(argv[3], &provider)) {
        yvex_cli_out_writef(stderr, "accounts login: unsupported provider: %s\n", argv[3]);
        return 2;
    }
    rc = accounts_parse_login_options("login", argc, argv, 4, &options);
    if (rc != 0) return rc;
    memset(&observe, 0, sizeof(observe));
    observe.provider = provider;
    observe.cli_override = options.cli_override;
    observe.token_env_name = options.token_env_name;
    yvex_error_clear(&err);
    rc = yvex_account_observe(&observe, &obs, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (!obs.cli_present) {
        snprintf(obs.status, sizeof(obs.status), "account-login-blocked");
        accounts_print_single("login", &obs, options.output_mode);
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    token_env = options.token_env_name && options.token_env_name[0]
        ? options.token_env_name
        : yvex_account_default_token_env(provider);
    token_value = token_env && token_env[0] ? getenv(token_env) : NULL;

    if (provider == YVEX_ACCOUNT_PROVIDER_GITHUB && token_value && token_value[0]) {
        snprintf(obs.auth_state, sizeof(obs.auth_state), "env-token-present");
        snprintf(obs.credential_source, sizeof(obs.credential_source), "environment");
        snprintf(obs.account_hint, sizeof(obs.account_hint), "env:%s", token_env);
        snprintf(obs.login_method, sizeof(obs.login_method), "token-env");
        snprintf(obs.status, sizeof(obs.status), "account-login-pass");
        obs.token_value_redacted = 1;
        (void)yvex_account_write_state(&obs, 1u, &err);
        accounts_print_single("login", &obs, options.output_mode);
        return 0;
    }

    args[n++] = obs.cli_path;
    args[n++] = "auth";
    args[n++] = "login";
    if (provider == YVEX_ACCOUNT_PROVIDER_HUGGINGFACE) {
        if (options.force) args[n++] = "--force";
        if (options.add_to_git_credential) args[n++] = "--add-to-git-credential";
        if (options.device) args[n++] = "--device";
        if (token_value && token_value[0]) {
            args[n++] = "--token";
            args[n++] = token_value;
            obs.token_value_redacted = 1;
        }
    } else {
        if (options.web) args[n++] = "--web";
        if (options.hostname) {
            args[n++] = "--hostname";
            args[n++] = options.hostname;
        }
        if (options.git_protocol) {
            args[n++] = "--git-protocol";
            args[n++] = options.git_protocol;
        }
        if (options.scopes) {
            args[n++] = "--scopes";
            args[n++] = options.scopes;
        }
        if (options.skip_ssh_key) args[n++] = "--skip-ssh-key";
    }
    args[n] = NULL;
    snprintf(obs.login_method, sizeof(obs.login_method), token_value && token_value[0] ? "token-env" : "provider-cli");
    exit_code = accounts_run_foreground(args, &err);
    obs.command_exit_code = exit_code;
    if (exit_code != 0) {
        snprintf(obs.status, sizeof(obs.status), "account-login-fail");
        snprintf(obs.top_blocker, sizeof(obs.top_blocker), "provider-login-failed");
        snprintf(obs.next, sizeof(obs.next), "retry provider login");
        accounts_print_single("login", &obs, options.output_mode);
        return 1;
    }
    rc = yvex_account_observe(&observe, &obs, &err);
    if (rc == YVEX_OK && (strcmp(obs.auth_state, "logged-in") == 0 ||
                          strcmp(obs.auth_state, "env-token-present") == 0)) {
        snprintf(obs.status, sizeof(obs.status), "account-login-pass");
        (void)yvex_account_write_state(&obs, 1u, &err);
        accounts_print_single("login", &obs, options.output_mode);
        return 0;
    }
    snprintf(obs.status, sizeof(obs.status), "account-login-pass");
    snprintf(obs.auth_state, sizeof(obs.auth_state), "unknown");
    (void)yvex_account_write_state(&obs, 1u, &err);
    accounts_print_single("login", &obs, options.output_mode);
    return 0;
}

static int command_accounts_ensure(int argc, char **argv)
{
    yvex_accounts_cli_options options;
    yvex_account_provider provider;
    yvex_account_ensure_options ensure;
    yvex_account_observation obs;
    yvex_error err;
    int rc;
    int i;

    if (argc < 4) {
        yvex_cli_out_writef(stderr, "accounts ensure: PROVIDER is required\n");
        return 2;
    }
    if (!yvex_account_provider_from_name(argv[3], &provider)) {
        yvex_cli_out_writef(stderr, "accounts ensure: unsupported provider: %s\n", argv[3]);
        return 2;
    }
    memset(&options, 0, sizeof(options));
    options.output_mode = YVEX_ACCOUNTS_OUTPUT_NORMAL;
    memset(&ensure, 0, sizeof(ensure));
    ensure.provider = provider;
    ensure.interactive = YVEX_ACCOUNT_INTERACTIVE_AUTO;
    for (i = 4; i < argc; ++i) {
        const char *value = NULL;
        if (strcmp(argv[i], "--audit") == 0) {
            options.output_mode = YVEX_ACCOUNTS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--required") == 0) {
            ensure.required = 1;
        } else if (strcmp(argv[i], "--output") == 0 ||
                   strcmp(argv[i], "--token-env") == 0 ||
                   strcmp(argv[i], "--interactive") == 0 ||
                   strcmp(argv[i], "--cli") == 0) {
            const char *flag = argv[i];
            rc = accounts_parse_value("ensure", flag, argc, argv, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--output") == 0) {
                if (!accounts_parse_output_mode(value, &options.output_mode)) {
                    yvex_cli_out_writef(stderr, "accounts ensure: unsupported output mode: %s\n", value);
                    return 2;
                }
            } else if (strcmp(flag, "--token-env") == 0) {
                ensure.token_env_name = value;
            } else if (strcmp(flag, "--cli") == 0) {
                ensure.cli_override = value;
            } else if (strcmp(value, "auto") == 0) {
                ensure.interactive = YVEX_ACCOUNT_INTERACTIVE_AUTO;
            } else if (strcmp(value, "always") == 0) {
                ensure.interactive = YVEX_ACCOUNT_INTERACTIVE_ALWAYS;
            } else if (strcmp(value, "never") == 0) {
                ensure.interactive = YVEX_ACCOUNT_INTERACTIVE_NEVER;
            } else {
                yvex_cli_out_writef(stderr, "accounts ensure: --interactive requires auto|always|never\n");
                return 2;
            }
        } else {
            yvex_cli_out_writef(stderr, "accounts ensure: unknown option: %s\n", argv[i]);
            return 2;
        }
    }
    yvex_error_clear(&err);
    rc = yvex_account_ensure(&ensure, &obs, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (strcmp(obs.auth_state, "logged-in") == 0 ||
        strcmp(obs.auth_state, "env-token-present") == 0) {
        snprintf(obs.status, sizeof(obs.status), "account-ensure-pass");
        accounts_print_single("ensure", &obs, options.output_mode);
        return 0;
    }
    if (!obs.status[0] || strcmp(obs.status, "account-provider-blocked") == 0) {
        snprintf(obs.status, sizeof(obs.status), "account-ensure-blocked");
    }
    accounts_print_single("ensure", &obs, options.output_mode);
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

static int command_accounts_logout(int argc, char **argv)
{
    yvex_accounts_cli_options options;
    yvex_account_provider provider;
    yvex_account_observe_options observe;
    yvex_account_observation obs;
    yvex_error err;
    const char *args[5];
    int rc;
    int exit_code;

    if (argc < 4) {
        yvex_cli_out_writef(stderr, "accounts logout: PROVIDER is required\n");
        return 2;
    }
    if (!yvex_account_provider_from_name(argv[3], &provider)) {
        yvex_cli_out_writef(stderr, "accounts logout: unsupported provider: %s\n", argv[3]);
        return 2;
    }
    rc = accounts_parse_common("logout", argc, argv, 4, &options);
    if (rc != 0) return rc;
    memset(&observe, 0, sizeof(observe));
    observe.provider = provider;
    observe.cli_override = options.cli_override;
    observe.token_env_name = options.token_env_name;
    yvex_error_clear(&err);
    rc = yvex_account_observe(&observe, &obs, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (!obs.cli_present) {
        snprintf(obs.status, sizeof(obs.status), "account-logout-blocked");
        accounts_print_single("logout", &obs, options.output_mode);
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    args[0] = obs.cli_path;
    args[1] = "auth";
    args[2] = "logout";
    args[3] = NULL;
    exit_code = accounts_run_foreground(args, &err);
    obs.command_exit_code = exit_code;
    snprintf(obs.status, sizeof(obs.status), exit_code == 0 ? "account-logout-pass" : "account-logout-fail");
    accounts_print_single("logout", &obs, options.output_mode);
    return exit_code == 0 ? 0 : 1;
}

static void accounts_help_command(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: yvex accounts providers [--output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex accounts status [--audit | --output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex accounts login PROVIDER [options]\n");
    yvex_cli_out_writef(fp, "       yvex accounts logout PROVIDER [options]\n");
    yvex_cli_out_writef(fp, "       yvex accounts whoami PROVIDER [--audit]\n");
    yvex_cli_out_writef(fp, "       yvex accounts ensure PROVIDER [--interactive auto|always|never] [--required]\n");
    yvex_cli_out_writef(fp, "\nProviders: huggingface|hf, github|gh.\n");
    yvex_cli_out_writef(fp, "Hugging Face CLI discovery uses YVEX_HF_CLI, then hf, then legacy huggingface-cli.\n");
    yvex_cli_out_writef(fp, "GitHub CLI discovery uses YVEX_GH_CLI, then gh.\n");
    yvex_cli_out_writef(fp, "Boundary: local provider account state only; YVEX stores no raw tokens, does not install tools, does not implement OAuth, does not use MCP/YAI, and does not download, materialize, run, generate, evaluate, or benchmark models from this command.\n");
}

int yvex_accounts_command(int argc, char **argv)
{
    const char *sub;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        accounts_help_command(stdout);
        return 0;
    }
    if (argc < 3) {
        yvex_cli_out_writef(stderr, "yvex: accounts requires providers, status, login, logout, whoami, or ensure\n");
        return 2;
    }
    sub = argv[2];
    if (strcmp(sub, "providers") == 0) return command_accounts_providers(argc, argv);
    if (strcmp(sub, "status") == 0) return command_accounts_status(argc, argv);
    if (strcmp(sub, "login") == 0) return command_accounts_login(argc, argv);
    if (strcmp(sub, "logout") == 0) return command_accounts_logout(argc, argv);
    if (strcmp(sub, "whoami") == 0) return command_accounts_whoami(argc, argv);
    if (strcmp(sub, "ensure") == 0) return command_accounts_ensure(argc, argv);
    yvex_cli_out_writef(stderr, "yvex: unknown accounts subcommand: %s\n", sub);
    return 2;
}

void yvex_accounts_help(FILE *fp)
{
    accounts_help_command(fp);
}
