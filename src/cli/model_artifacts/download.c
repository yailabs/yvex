/*
 * download.c - complete models-download CLI workflow owner.
 * Owner: src/cli/render
 * Owns: typed option parsing, provider process lifecycle, bounded source
 * intake checks, progress/file output, control routing, and report rendering.
 * Does not own: domain model registry storage, runtime generation, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; existing command syntax is preserved.
 * Boundary: source download facts do not make artifacts generation-capable.
 */
#include "src/cli/model_artifacts/download.h"
#include "models.h"

static const yvex_model_download_catalog_row model_download_catalog[] = {
    { "gemma-4-e2b", "gemma", "hf", "google/gemma-4-E2B", "gemma-4-e2b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e2b", "target-routing-default" },
    { "gemma-4-e2b-it", "gemma", "hf", "google/gemma-4-E2B-it", "gemma-4-e2b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e2b-it", "target-routing-default" },
    { "gemma-4-e4b", "gemma", "hf", "google/gemma-4-E4B", "gemma-4-e4b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e4b", "target-routing-default" },
    { "gemma-4-e4b-it", "gemma", "hf", "google/gemma-4-E4B-it", "gemma-4-e4b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-e4b-it", "target-routing-default" },
    { "gemma-4-12b", "gemma", "hf", "google/gemma-4-12B", "gemma-4-12b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-12b", "target-routing-default" },
    { "gemma-4-12b-it", "gemma", "hf", "google/gemma-4-12B-it", "gemma-4-12b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-12b-it", "target-routing-default" },
    { "gemma-4-26b-a4b", "gemma", "hf", "google/gemma-4-26B-A4B", "gemma-4-26b-a4b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-26b-a4b", "target-routing-default" },
    { "gemma-4-26b-a4b-it", "gemma", "hf", "google/gemma-4-26B-A4B-it", "gemma-4-26b-a4b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-26b-a4b-it", "target-routing-default" },
    { "gemma-4-31b", "gemma", "hf", "google/gemma-4-31B", "gemma-4-31b", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-31b", "target-routing-default" },
    { "gemma-4-31b-it", "gemma", "hf", "google/gemma-4-31B-it", "gemma-4-31b-it", "main",
      "official-safetensors", "huggingface-repository", "gemma-4-31b-it", "target-routing-default" },
    { "qwen3-8b", "qwen", "hf", "Qwen/Qwen3-8B", "qwen3-8b", "main",
      "official-safetensors", "huggingface-repository", "qwen3-8b", "target-routing-default" },
    { "qwen3-32b", "qwen", "hf", "Qwen/Qwen3-32B", "qwen3-32b", "main",
      "official-safetensors", "huggingface-repository", "qwen3-32b", "source-download-routing-only" }
};

static const char *const model_download_default_includes[] = {
    "*.safetensors",
    "*.json",
    "*.txt",
    "*.model",
    "*.jinja",
    "*.md"
};

static const char *const model_download_default_excludes[] = {
    "*.bin",
    "*.pt",
    "*.onnx",
    "*.msgpack",
    "*.tflite",
    "*.h5",
    "*.ckpt",
    "*.tar",
    "*.zip"
};

const yvex_model_download_catalog_row *model_download_find_catalog(const char *target)
{
    unsigned long i;

    if (!target) return NULL;
    for (i = 0; i < sizeof(model_download_catalog) / sizeof(model_download_catalog[0]); ++i) {
        if (strcmp(model_download_catalog[i].target_id, target) == 0) {
            return &model_download_catalog[i];
        }
    }
    return NULL;
}

static int model_download_family_valid(const char *family)
{
    return family &&
           (strcmp(family, "deepseek") == 0 ||
            strcmp(family, "glm") == 0 ||
            strcmp(family, "qwen") == 0 ||
            strcmp(family, "gemma") == 0);
}

static int model_download_local_name_valid(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if (!name || !name[0]) return 0;
    while (*p) {
        if (!(isalnum(*p) || *p == '-' || *p == '_' || *p == '.')) {
            return 0;
        }
        ++p;
    }
    return 1;
}

static int model_download_repo_valid(const char *repo)
{
    const unsigned char *p = (const unsigned char *)repo;
    int slash_count = 0;

    if (!repo || !repo[0] || repo[0] == '/' || strstr(repo, "..")) return 0;
    while (*p) {
        if (*p == '/') {
            slash_count++;
            if (p[1] == '\0') return 0;
        } else if (!(isalnum(*p) || *p == '-' || *p == '_' || *p == '.')) {
            return 0;
        }
        ++p;
    }
    return slash_count == 1;
}

static const char *model_download_repo_basename(const char *repo)
{
    const char *slash = repo ? strrchr(repo, '/') : NULL;
    return slash && slash[1] ? slash + 1 : NULL;
}

const char *model_download_progress_mode_name(yvex_model_download_progress_mode mode)
{
    switch (mode) {
    case YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO: return "auto";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE: return "live";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN: return "plain";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_LOG: return "log";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_OFF: return "off";
    }
    return "auto";
}

const char *model_download_signal_name(int signo)
{
    switch (signo) {
    case SIGINT: return "SIGINT";
    case SIGTERM: return "SIGTERM";
    case SIGKILL: return "SIGKILL";
    case 0: return "none";
    }
    return "unknown";
}

static int model_download_parse_progress_mode(const char *value,
                                              yvex_model_download_progress_mode *out)
{
    if (!value || !out) return 0;
    if (strcmp(value, "auto") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO;
        return 1;
    }
    if (strcmp(value, "live") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE;
        return 1;
    }
    if (strcmp(value, "plain") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN;
        return 1;
    }
    if (strcmp(value, "log") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_LOG;
        return 1;
    }
    if (strcmp(value, "off") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_PROGRESS_OFF;
        return 1;
    }
    return 0;
}

yvex_model_download_progress_mode model_download_effective_progress_mode(
    yvex_model_download_progress_mode mode)
{
    if (mode != YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO) {
        return mode;
    }
    return isatty(STDOUT_FILENO) && isatty(STDERR_FILENO)
        ? YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE
        : YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN;
}

static int model_download_auth_mode_parse(const char *value, yvex_model_download_auth_mode *out)
{
    if (!value || !out) return 0;
    if (strcmp(value, "auto") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_AUTH_AUTO;
        return 1;
    }
    if (strcmp(value, "required") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED;
        return 1;
    }
    if (strcmp(value, "never") == 0) {
        *out = YVEX_MODEL_DOWNLOAD_AUTH_NEVER;
        return 1;
    }
    return 0;
}

const char *model_download_auth_mode_name(yvex_model_download_auth_mode mode)
{
    switch (mode) {
    case YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED: return "required";
    case YVEX_MODEL_DOWNLOAD_AUTH_NEVER: return "never";
    case YVEX_MODEL_DOWNLOAD_AUTH_AUTO:
    default: return "auto";
    }
}

int parse_models_download_options_from(int arg_count,
                                              char **args,
                                              int start_index,
                                              yvex_cli_models_download_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->source = "hf";
    options->revision = "main";
    options->max_workers = 8ull;
    options->auth_mode = YVEX_MODEL_DOWNLOAD_AUTH_AUTO;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    options->progress_mode = YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO;
    options->tick_seconds = 2ull;
    options->timeout_seconds = 5ull;

    if (arg_count > start_index &&
        (strcmp(args[start_index], "--help") == 0 || strcmp(args[start_index], "-h") == 0)) {
        return 1;
    }

    for (i = start_index; i < arg_count; ++i) {
        const char *value = NULL;
        if (strcmp(args[i], "--models-root") == 0 ||
            strcmp(args[i], "--repo") == 0 ||
            strcmp(args[i], "--family") == 0 ||
            strcmp(args[i], "--name") == 0 ||
            strcmp(args[i], "--revision") == 0 ||
            strcmp(args[i], "--source") == 0 ||
            strcmp(args[i], "--provider") == 0 ||
            strcmp(args[i], "--asset") == 0 ||
            strcmp(args[i], "--asset-name") == 0 ||
            strcmp(args[i], "--release") == 0 ||
            strcmp(args[i], "--github-source") == 0 ||
            strcmp(args[i], "--auth") == 0 ||
            strcmp(args[i], "--include") == 0 ||
            strcmp(args[i], "--exclude") == 0 ||
            strcmp(args[i], "--max-workers") == 0 ||
            strcmp(args[i], "--token-env") == 0 ||
            strcmp(args[i], "--cli") == 0 ||
            strcmp(args[i], "--progress") == 0 ||
            strcmp(args[i], "--tick-seconds") == 0 ||
            strcmp(args[i], "--timeout-seconds") == 0 ||
            strcmp(args[i], "--" "output") == 0) {
            const char *flag = args[i];
            int rc = parse_models_value_option("models download", flag,
                                               arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--models-root") == 0) {
                options->models_root = value;
            } else if (strcmp(flag, "--repo") == 0) {
                options->repo = value;
            } else if (strcmp(flag, "--family") == 0) {
                options->family = value;
            } else if (strcmp(flag, "--name") == 0) {
                options->name = value;
            } else if (strcmp(flag, "--revision") == 0) {
                options->revision = value;
            } else if (strcmp(flag, "--source") == 0) {
                if (strcmp(value, "hf") != 0) {
                    yvex_cli_out_writef(stderr, "yvex: models download --source supports hf only\n");
                    return 2;
                }
                options->source = value;
                if (!options->provider) options->provider = "huggingface";
            } else if (strcmp(flag, "--provider") == 0) {
                yvex_account_provider provider;
                if (!yvex_account_provider_from_name(value, &provider)) {
                    yvex_cli_out_writef(stderr, "yvex: models download --provider requires hf|huggingface|gh|github\n");
                    return 2;
                }
                options->provider = yvex_account_provider_name(provider);
            } else if (strcmp(flag, "--asset") == 0) {
                options->asset = value;
            } else if (strcmp(flag, "--asset-name") == 0) {
                options->asset_name = value;
            } else if (strcmp(flag, "--release") == 0) {
                options->release = value;
                options->revision = value;
            } else if (strcmp(flag, "--github-source") == 0) {
                if (strcmp(value, "release-asset") != 0) {
                    yvex_cli_out_writef(stderr, "yvex: models download --github-source supports release-asset only\n");
                    return 2;
                }
                options->github_source = value;
            } else if (strcmp(flag, "--auth") == 0) {
                if (!model_download_auth_mode_parse(value, &options->auth_mode)) {
                    yvex_cli_out_writef(stderr, "yvex: models download --auth requires auto|required|never\n");
                    return 2;
                }
            } else if (strcmp(flag, "--include") == 0) {
                if (options->include_count >= YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    yvex_cli_out_writef(stderr, "yvex: models download too many --include patterns\n");
                    return 2;
                }
                options->include_patterns[options->include_count++] = value;
            } else if (strcmp(flag, "--exclude") == 0) {
                if (options->exclude_count >= YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    yvex_cli_out_writef(stderr, "yvex: models download too many --exclude patterns\n");
                    return 2;
                }
                options->exclude_patterns[options->exclude_count++] = value;
            } else if (strcmp(flag, "--max-workers") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    yvex_cli_out_writef(stderr, "yvex: models download --max-workers requires a positive integer\n");
                    return 2;
                }
                options->max_workers = parsed;
            } else if (strcmp(flag, "--token-env") == 0) {
                options->token_env = value;
            } else if (strcmp(flag, "--cli") == 0) {
                options->cli = value;
            } else if (strcmp(flag, "--progress") == 0) {
                if (!model_download_parse_progress_mode(value, &options->progress_mode)) {
                    yvex_cli_out_writef(stderr, "yvex: models download --progress requires auto|live|plain|log|off\n");
                    return 2;
                }
            } else if (strcmp(flag, "--tick-seconds") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    yvex_cli_out_writef(stderr, "yvex: models download --tick-seconds requires a positive integer\n");
                    return 2;
                }
                options->tick_seconds = parsed;
            } else if (strcmp(flag, "--timeout-seconds") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    yvex_cli_out_writef(stderr, "yvex: models download --timeout-seconds requires a positive integer\n");
                    return 2;
                }
                options->timeout_seconds = parsed;
            } else if (strcmp(flag, "--" "output") == 0) {
                if (!parse_models_output_mode(value, &options->output_mode)) {
                    yvex_cli_out_writef(stderr, "yvex: models download unsupported output mode: %s\n", value);
                    return 2;
                }
            }
        } else if (strcmp(args[i], "--dry-run") == 0) {
            options->dry_run = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--no-manifest") == 0) {
            options->no_manifest = 1;
        } else if (strcmp(args[i], "--no-native-inventory") == 0) {
            options->no_native_inventory = 1;
        } else if (strcmp(args[i], "--force-sidecars") == 0) {
            options->force_sidecars = 1;
        } else if (strcmp(args[i], "--yes") == 0) {
            options->yes = 1;
        } else if (strcmp(args[i], "--clear-stale-locks") == 0) {
            options->clear_stale_locks = 1;
        } else if (strcmp(args[i], "--force") == 0) {
            options->force = 1;
        } else if (strcmp(args[i], "--match-provider-process") == 0) {
            options->match_provider_process = 1;
        } else if (strcmp(args[i], "--stale-locks") == 0) {
            options->cleanup_stale_locks = 1;
        } else if (strcmp(args[i], "--logs") == 0) {
            options->cleanup_logs = 1;
        } else if (strcmp(args[i], "--receipts") == 0) {
            options->cleanup_receipts = 1;
        } else if (strcmp(args[i], "--failed-partials") == 0) {
            options->cleanup_failed_partials = 1;
        } else if (strcmp(args[i], "--all-provider-cache") == 0) {
            options->cleanup_all_provider_cache = 1;
        } else if (strcmp(args[i], "--no-progress") == 0) {
            options->progress_mode = YVEX_MODEL_DOWNLOAD_PROGRESS_OFF;
        } else if (strcmp(args[i], "--" "json") == 0) {
            yvex_cli_out_writef(stderr, "yvex: models download JSON output is unsupported; use --" "output normal|table|audit\n");
            return 2;
        } else if (args[i][0] == '-') {
            yvex_cli_out_writef(stderr, "yvex: unknown models download option: %s\n", args[i]);
            return 2;
        } else if (!options->target) {
            options->target = args[i];
            if (!cli_arg_value_valid(options->target)) {
                yvex_cli_out_writef(stderr, "yvex: models download target is empty or invalid\n");
                return 2;
            }
        } else {
            yvex_cli_out_writef(stderr, "yvex: models download received extra positional argument: %s\n", args[i]);
            return 2;
        }
    }

    if (!options->target && !options->repo) {
        yvex_cli_out_writef(stderr, "yvex: models download requires TARGET or --repo OWNER/NAME\n");
        return 2;
    }
    if (options->target && options->repo) {
        yvex_cli_out_writef(stderr, "yvex: models download accepts either TARGET or --repo, not both\n");
        return 2;
    }
    if (!options->provider) {
        options->provider = "huggingface";
    }
    if (options->repo && !model_download_repo_valid(options->repo)) {
        yvex_cli_out_writef(stderr, "yvex: models download --repo requires OWNER/NAME\n");
        return 2;
    }
    if (strcmp(options->provider, "github") == 0 && !options->repo) {
        yvex_cli_out_writef(stderr, "yvex: models download --provider github requires --repo OWNER/NAME\n");
        return 2;
    }
    if (strcmp(options->provider, "github") == 0 && !options->asset) {
        yvex_cli_out_writef(stderr, "yvex: models download --provider github requires --asset GLOB\n");
        return 2;
    }
    if (strcmp(options->provider, "github") == 0 && options->target) {
        yvex_cli_out_writef(stderr, "yvex: models download catalog targets use Hugging Face provider in this wave\n");
        return 2;
    }
    if (strcmp(options->provider, "github") != 0 &&
        options->repo && (!options->family || !model_download_family_valid(options->family))) {
        yvex_cli_out_writef(stderr, "yvex: models download --repo requires --family deepseek|glm|qwen|gemma\n");
        return 2;
    }
    if (options->repo && !options->name) {
        options->name = options->asset_name ? options->asset_name : model_download_repo_basename(options->repo);
    }
    if (options->repo && !model_download_local_name_valid(options->name)) {
        yvex_cli_out_writef(stderr, "yvex: models download --name is required and must be a local model name\n");
        return 2;
    }
    return 0;
}

unsigned int model_download_effective_include_count(const yvex_cli_models_download_options *options)
{
    return options && options->include_count
        ? options->include_count
        : (unsigned int)(sizeof(model_download_default_includes) /
                         sizeof(model_download_default_includes[0]));
}

unsigned int model_download_effective_exclude_count(const yvex_cli_models_download_options *options)
{
    return options && options->exclude_count
        ? options->exclude_count
        : (unsigned int)(sizeof(model_download_default_excludes) /
                         sizeof(model_download_default_excludes[0]));
}

const char *model_download_effective_include_at(const yvex_cli_models_download_options *options,
                                                       unsigned int index)
{
    if (options && options->include_count) return options->include_patterns[index];
    return model_download_default_includes[index];
}

const char *model_download_effective_exclude_at(const yvex_cli_models_download_options *options,
                                                       unsigned int index)
{
    if (options && options->exclude_count) return options->exclude_patterns[index];
    return model_download_default_excludes[index];
}

void model_download_report_init(yvex_model_download_report *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    snprintf(report->status, sizeof(report->status), "model-download-fail");
    snprintf(report->provider, sizeof(report->provider), "huggingface");
    snprintf(report->auth_state, sizeof(report->auth_state), "not-provided");
    snprintf(report->credential_source, sizeof(report->credential_source), "unknown");
    snprintf(report->account_hint, sizeof(report->account_hint), "unknown");
    snprintf(report->provider_cli_status, sizeof(report->provider_cli_status), "unknown");
    snprintf(report->stage_resolve_target, sizeof(report->stage_resolve_target), "fail");
    snprintf(report->stage_resolve_paths, sizeof(report->stage_resolve_paths), "fail");
    snprintf(report->stage_prepare_dirs, sizeof(report->stage_prepare_dirs), "skipped");
    snprintf(report->stage_account_provider, sizeof(report->stage_account_provider), "skipped");
    snprintf(report->stage_provider_cli, sizeof(report->stage_provider_cli), "skipped");
    snprintf(report->stage_hf_cli, sizeof(report->stage_hf_cli), "skipped");
    snprintf(report->stage_download, sizeof(report->stage_download), "skipped");
    snprintf(report->stage_progress_stream, sizeof(report->stage_progress_stream), "skipped");
    snprintf(report->stage_progress_ticks, sizeof(report->stage_progress_ticks), "skipped");
    snprintf(report->stage_source_scan, sizeof(report->stage_source_scan), "skipped");
    snprintf(report->stage_source_manifest, sizeof(report->stage_source_manifest), "skipped");
    snprintf(report->stage_native_inventory, sizeof(report->stage_native_inventory), "skipped");
    snprintf(report->stage_sidecar, sizeof(report->stage_sidecar), "skipped");
    report->hf_exit_code = -1;
    report->provider_exit_code = -1;
    report->provider_pid = -1;
    report->provider_process_group = -1;
    snprintf(report->child_exit_status, sizeof(report->child_exit_status), "unknown");
    snprintf(report->orphan_check_status, sizeof(report->orphan_check_status), "unknown");
}

void model_download_timestamp(char *out, size_t cap)
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

static int model_download_path_under(const char *path, const char *dir)
{
    size_t dir_len;

    if (!path || !dir || !path[0] || !dir[0]) return 0;
    dir_len = strlen(dir);
    while (dir_len > 1u && dir[dir_len - 1u] == '/') {
        --dir_len;
    }
    return strncmp(path, dir, dir_len) == 0 &&
           (path[dir_len] == '\0' || path[dir_len] == '/');
}

int model_download_source_path_allowed(const yvex_operator_paths *operator_paths,
                                              const char *local_source_dir,
                                              yvex_model_download_report *report)
{
    char repo_root[YVEX_PATH_CAP];

    if (!operator_paths || !local_source_dir) return 0;
    if (strcmp(operator_paths->models_root_source, "explicit") == 0) {
        return 1;
    }
    if (!getcwd(repo_root, sizeof(repo_root))) {
        snprintf(report->top_blocker, sizeof(report->top_blocker),
                 "repository-path-check-unavailable");
        snprintf(report->error, sizeof(report->error), "getcwd failed while checking local source path");
        return 0;
    }
    if (model_download_path_under(local_source_dir, repo_root)) {
        snprintf(report->top_blocker, sizeof(report->top_blocker),
                 "unsafe-repository-source-path");
        snprintf(report->error, sizeof(report->error),
                 "resolved local source path is inside the repository tree");
        return 0;
    }
    return 1;
}

int model_download_file_name_ends_with(const char *path, const char *suffix)
{
    size_t path_len;
    size_t suffix_len;

    if (!path || !suffix) return 0;
    path_len = strlen(path);
    suffix_len = strlen(suffix);
    return suffix_len <= path_len && strcmp(path + path_len - suffix_len, suffix) == 0;
}

int model_download_name_starts_with(const char *name, const char *prefix)
{
    size_t n;

    if (!name || !prefix) return 0;
    n = strlen(prefix);
    return strncmp(name, prefix, n) == 0;
}

int model_download_name_contains(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

static unsigned long long model_download_lock_age_seconds(const struct stat *st)
{
    time_t now;

    if (!st) return 0ull;
    now = time(NULL);
    if (now == (time_t)-1 || st->st_mtime > now) return 0ull;
    return (unsigned long long)(now - st->st_mtime);
}

static int model_download_scan_dir(const char *root,
                                   const char *rel_dir,
                                   yvex_model_download_source_scan *scan,
                                   yvex_error *err)
{
    char abs_dir[YVEX_PATH_CAP];
    DIR *dir;
    struct dirent *ent;
    int rc = YVEX_OK;

    if (rel_dir && rel_dir[0]) {
        rc = path_join2(abs_dir, sizeof(abs_dir), root, rel_dir, err, "models_download");
        if (rc != YVEX_OK) return rc;
    } else {
        int n = snprintf(abs_dir, sizeof(abs_dir), "%s", root);
        if (n < 0 || (size_t)n >= sizeof(abs_dir)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "models_download", "source path is too long");
            return YVEX_ERR_BOUNDS;
        }
    }

    dir = opendir(abs_dir);
    if (!dir) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_download", "cannot open source directory: %s", abs_dir);
        return YVEX_ERR_IO;
    }

    while ((ent = readdir(dir)) != NULL) {
        char rel_path[YVEX_PATH_CAP];
        char abs_path[YVEX_PATH_CAP];
        struct stat st;
        const char *base = ent->d_name;

        if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
            continue;
        }
        if (rel_dir && rel_dir[0]) {
            rc = path_join2(rel_path, sizeof(rel_path), rel_dir, base, err, "models_download");
        } else {
            int n = snprintf(rel_path, sizeof(rel_path), "%s", base);
            rc = (n < 0 || (size_t)n >= sizeof(rel_path)) ? YVEX_ERR_BOUNDS : YVEX_OK;
            if (rc != YVEX_OK) {
                yvex_error_set(err, rc, "models_download", "relative source path is too long");
            }
        }
        if (rc != YVEX_OK) break;
        rc = path_join2(abs_path, sizeof(abs_path), root, rel_path, err, "models_download");
        if (rc != YVEX_OK) break;
        if (lstat(abs_path, &st) != 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "models_download", "cannot stat source path: %s", abs_path);
            rc = YVEX_ERR_IO;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            rc = model_download_scan_dir(root, rel_path, scan, err);
            if (rc != YVEX_OK) break;
        } else if (S_ISREG(st.st_mode)) {
            unsigned long long bytes = st.st_size > 0 ? (unsigned long long)st.st_size : 0ull;
            scan->file_count++;
            scan->total_regular_file_bytes += bytes;
            if (model_download_file_name_ends_with(rel_path, ".safetensors")) {
                scan->safetensors_count++;
            }
            if (model_download_file_name_ends_with(rel_path, ".lock")) {
                unsigned long long idx = scan->lock_count;
                if (idx < YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    snprintf(scan->lock_paths[idx], sizeof(scan->lock_paths[idx]), "%s", rel_path);
                    scan->lock_age_seconds[idx] = model_download_lock_age_seconds(&st);
                }
                scan->lock_count++;
            }
            if (model_download_file_name_ends_with(rel_path, ".partial") ||
                model_download_file_name_ends_with(rel_path, ".incomplete") ||
                model_download_file_name_ends_with(rel_path, ".tmp") ||
                model_download_name_contains(rel_path, ".part")) {
                scan->partial_file_count++;
            }
            if (model_download_name_starts_with(rel_path, ".cache/") ||
                model_download_name_contains(rel_path, "/.cache/")) {
                scan->cache_file_count++;
            }
            if (strcmp(base, "config.json") == 0) {
                scan->config_present = 1;
            }
            if (strcmp(base, "tokenizer.json") == 0 ||
                strcmp(base, "tokenizer.model") == 0 ||
                strcmp(base, "tokenizer_config.json") == 0 ||
                model_download_name_starts_with(base, "tokenizer.")) {
                scan->tokenizer_present = 1;
            }
            if (bytes > scan->largest_file_bytes) {
                scan->largest_file_bytes = bytes;
                snprintf(scan->largest_file_name, sizeof(scan->largest_file_name), "%s", rel_path);
            }
        }
    }

    closedir(dir);
    return rc;
}

int model_download_scan_source(const char *root,
                                      yvex_model_download_source_scan *scan,
                                      yvex_error *err)
{
    if (!root || !scan) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_download", "source root and scan output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(scan, 0, sizeof(*scan));
    return model_download_scan_dir(root, "", scan, err);
}

static int model_download_read_u64_le(FILE *fp, unsigned long long *out)
{
    unsigned char raw[8];
    size_t got;
    unsigned int i;
    unsigned long long v = 0ull;

    if (!fp || !out) return 0;
    got = fread(raw, 1u, sizeof(raw), fp);
    if (got != sizeof(raw)) return 0;
    for (i = 0; i < 8u; ++i) {
        v |= ((unsigned long long)raw[i]) << (8u * i);
    }
    *out = v;
    return 1;
}

static int model_download_parse_data_offsets(const char *header,
                                             unsigned long long *max_end,
                                             unsigned long long *count)
{
    const char *p = header;
    const char *needle = "data_offsets";

    if (!header || !max_end || !count) return 0;
    *max_end = 0ull;
    *count = 0ull;
    while ((p = strstr(p, needle)) != NULL) {
        const char *bracket = strchr(p, '[');
        char *endptr = NULL;
        unsigned long long start;
        unsigned long long end;

        if (!bracket) return 0;
        errno = 0;
        start = strtoull(bracket + 1, &endptr, 10);
        (void)start;
        if (errno != 0 || !endptr || *endptr != ',') return 0;
        errno = 0;
        end = strtoull(endptr + 1, &endptr, 10);
        if (errno != 0 || !endptr || *endptr != ']') return 0;
        if (end > *max_end) *max_end = end;
        (*count)++;
        p = endptr + 1;
    }
    return *count > 0ull;
}

static const char *model_download_safetensors_file_status(const char *path)
{
    FILE *fp;
    struct stat st;
    unsigned long long header_len = 0ull;
    unsigned long long max_end = 0ull;
    unsigned long long tensor_count = 0ull;
    unsigned long long required_size;
    char *header = NULL;
    size_t header_size;
    const char *status = "unknown";

    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return "unknown";
    fp = fopen(path, "rb");
    if (!fp) return "unknown";
    if (!model_download_read_u64_le(fp, &header_len)) {
        fclose(fp);
        return "invalid-header";
    }
    if (header_len == 0ull || header_len > 128ull * 1024ull * 1024ull ||
        header_len > (unsigned long long)SIZE_MAX - 1ull) {
        fclose(fp);
        return "invalid-header";
    }
    header_size = (size_t)header_len;
    header = (char *)malloc(header_size + 1u);
    if (!header) {
        fclose(fp);
        return "unknown";
    }
    if (fread(header, 1u, header_size, fp) != header_size) {
        free(header);
        fclose(fp);
        return "invalid-header";
    }
    header[header_size] = '\0';
    if (!model_download_parse_data_offsets(header, &max_end, &tensor_count)) {
        status = "invalid-header";
    } else {
        required_size = 8ull + header_len + max_end;
        status = (unsigned long long)st.st_size >= required_size ? "ok" : "truncated";
    }
    free(header);
    fclose(fp);
    return status;
}

static int model_download_check_safetensors_dir(const char *root,
                                                const char *rel_dir,
                                                yvex_model_download_safetensors_check *check,
                                                yvex_error *err)
{
    char abs_dir[YVEX_PATH_CAP];
    DIR *dir;
    struct dirent *ent;
    int rc = YVEX_OK;

    if (rel_dir && rel_dir[0]) {
        rc = path_join2(abs_dir, sizeof(abs_dir), root, rel_dir, err, "models_download_status");
        if (rc != YVEX_OK) return rc;
    } else {
        int n = snprintf(abs_dir, sizeof(abs_dir), "%s", root);
        if (n < 0 || (size_t)n >= sizeof(abs_dir)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "models_download_status", "source path is too long");
            return YVEX_ERR_BOUNDS;
        }
    }
    dir = opendir(abs_dir);
    if (!dir) return YVEX_OK;
    while ((ent = readdir(dir)) != NULL) {
        char rel_path[YVEX_PATH_CAP];
        char abs_path[YVEX_PATH_CAP];
        struct stat st;
        const char *base = ent->d_name;

        if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) continue;
        if (rel_dir && rel_dir[0]) {
            rc = path_join2(rel_path, sizeof(rel_path), rel_dir, base, err, "models_download_status");
        } else {
            int n = snprintf(rel_path, sizeof(rel_path), "%s", base);
            rc = (n < 0 || (size_t)n >= sizeof(rel_path)) ? YVEX_ERR_BOUNDS : YVEX_OK;
            if (rc != YVEX_OK) {
                yvex_error_set(err, rc, "models_download_status", "relative source path is too long");
            }
        }
        if (rc != YVEX_OK) break;
        rc = path_join2(abs_path, sizeof(abs_path), root, rel_path, err, "models_download_status");
        if (rc != YVEX_OK) break;
        if (lstat(abs_path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            rc = model_download_check_safetensors_dir(root, rel_path, check, err);
            if (rc != YVEX_OK) break;
        } else if (S_ISREG(st.st_mode) &&
                   model_download_file_name_ends_with(rel_path, ".safetensors")) {
            const char *status = model_download_safetensors_file_status(abs_path);
            check->checked = 1;
            if (strcmp(status, "ok") == 0) check->ok_count++;
            else if (strcmp(status, "truncated") == 0) check->truncated_count++;
            else check->invalid_count++;
        }
    }
    closedir(dir);
    return rc;
}

int model_download_check_safetensors_source(const char *root,
                                                   yvex_model_download_safetensors_check *check,
                                                   yvex_error *err)
{
    int rc;

    if (!check) return YVEX_ERR_INVALID_ARG;
    memset(check, 0, sizeof(*check));
    snprintf(check->status, sizeof(check->status), "not-checked");
    if (!root || access(root, F_OK) != 0) return YVEX_OK;
    rc = model_download_check_safetensors_dir(root, "", check, err);
    if (rc != YVEX_OK) return rc;
    if (!check->checked) {
        snprintf(check->status, sizeof(check->status), "not-checked");
    } else if (check->truncated_count > 0) {
        snprintf(check->status, sizeof(check->status), "truncated");
    } else if (check->invalid_count > 0) {
        snprintf(check->status, sizeof(check->status), "invalid-header");
    } else {
        snprintf(check->status, sizeof(check->status), "ok");
    }
    return YVEX_OK;
}
static int provider_process_run_streaming(const char *const *args,
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
    return provider_process_run_streaming(args,
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
        manifest_options.status = YVEX_SOURCE_STATUS_IN_PROGRESS;
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
