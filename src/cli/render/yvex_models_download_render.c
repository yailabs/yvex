/*
 * yvex_models_download_surface.c - models download parse/status facts.
 * Owner: src/cli/render
 * Owns: download catalog, option parsing, source scanning, and safetensors checks for the CLI download family.
 * Does not own: domain model registry storage, runtime generation, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; existing command syntax is preserved.
 * Boundary: source download facts do not make artifacts generation-capable.
 */
#include "yvex_model_artifacts_surface_common.h"

typedef enum {
    YVEX_MODEL_DOWNLOAD_AUTH_AUTO = 0,
    YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED,
    YVEX_MODEL_DOWNLOAD_AUTH_NEVER
} yvex_model_download_auth_mode;

typedef struct {
    const char *target_id;
    const char *family;
    const char *provider;
    const char *repo_id;
    const char *local_name;
    const char *revision_default;
    const char *artifact_class;
    const char *source_container;
    const char *model_class_hint;
    const char *boundary;
} yvex_model_download_catalog_row;

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

typedef enum {
    YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO = 0,
    YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE,
    YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN,
    YVEX_MODEL_DOWNLOAD_PROGRESS_LOG,
    YVEX_MODEL_DOWNLOAD_PROGRESS_OFF
} yvex_model_download_progress_mode;

typedef struct {
    const char *target;
    const char *repo;
    const char *family;
    const char *name;
    const char *revision;
    const char *source;
    const char *provider;
    const char *asset;
    const char *asset_name;
    const char *release;
    const char *github_source;
    const char *models_root;
    const char *token_env;
    const char *cli;
    const char *include_patterns[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    const char *exclude_patterns[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    unsigned int include_count;
    unsigned int exclude_count;
    unsigned long long max_workers;
    yvex_model_download_auth_mode auth_mode;
    int dry_run;
    int no_manifest;
    int no_native_inventory;
    int force_sidecars;
    int yes;
    int resume;
    int clear_stale_locks;
    int force;
    int match_provider_process;
    int cleanup_stale_locks;
    int cleanup_logs;
    int cleanup_receipts;
    int cleanup_failed_partials;
    int cleanup_all_provider_cache;
    yvex_models_output_mode output_mode;
    yvex_model_download_progress_mode progress_mode;
    unsigned long long tick_seconds;
    unsigned long long timeout_seconds;
} yvex_cli_models_download_options;

typedef struct {
    unsigned long long file_count;
    unsigned long long safetensors_count;
    unsigned long long total_regular_file_bytes;
    unsigned long long largest_file_bytes;
    unsigned long long partial_file_count;
    unsigned long long cache_file_count;
    unsigned long long lock_count;
    unsigned long long lock_age_seconds[YVEX_MODEL_DOWNLOAD_PATTERN_CAP];
    int config_present;
    int tokenizer_present;
    char largest_file_name[YVEX_PATH_CAP];
    char lock_paths[YVEX_MODEL_DOWNLOAD_PATTERN_CAP][YVEX_PATH_CAP];
} yvex_model_download_source_scan;

typedef struct {
    char status[64];
    char target_id[128];
    char family[32];
    char provider[32];
    char repo_id[256];
    char revision[128];
    char local_name[128];
    char local_source_dir[YVEX_PATH_CAP];
    char models_root[YVEX_PATH_CAP];
    char models_root_source[32];
    char reports_dir[YVEX_PATH_CAP];
    char registry_dir[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char receipt_path[YVEX_PATH_CAP];
    char active_receipt_path[YVEX_PATH_CAP];
    char last_receipt_path[YVEX_PATH_CAP];
    char stdout_log_path[YVEX_PATH_CAP];
    char stderr_log_path[YVEX_PATH_CAP];
    char hf_cli_path[YVEX_PATH_CAP];
    char hf_cli_source[32];
    char provider_cli_path[YVEX_PATH_CAP];
    char provider_cli_source[64];
    char provider_cli_status[32];
    char auth_state[32];
    char credential_source[64];
    char account_hint[128];
    char accounts_state_path[YVEX_PATH_CAP];
    char token_env_name[64];
    char created_at[32];
    char top_blocker[128];
    char error[256];
    char stage_resolve_target[16];
    char stage_resolve_paths[16];
    char stage_prepare_dirs[16];
    char stage_account_provider[16];
    char stage_provider_cli[16];
    char stage_hf_cli[16];
    char stage_download[16];
    char stage_progress_stream[16];
    char stage_progress_ticks[16];
    char stage_source_scan[16];
    char stage_source_manifest[16];
    char stage_native_inventory[16];
    char stage_sidecar[16];
    yvex_model_download_source_scan source_scan;
    yvex_native_weight_summary native_summary;
    int hf_exit_code;
    int provider_exit_code;
    int stdout_streamed;
    int stderr_streamed;
    int interrupted;
    int interrupt_signal;
    int signal_forwarded;
    int child_terminated;
    int child_killed_after_timeout;
    int orphan_check_performed;
    int partial_source_preserved;
    int lock_files_deleted;
    pid_t provider_pid;
    pid_t provider_process_group;
    char child_exit_status[32];
    char orphan_check_status[16];
    unsigned long long stdout_bytes;
    unsigned long long stderr_bytes;
    unsigned long long tick_count;
    unsigned long long tick_last_elapsed_seconds;
    unsigned long long tick_last_file_count;
    unsigned long long tick_last_safetensors_count;
    unsigned long long tick_last_partial_file_count;
    unsigned long long tick_last_cache_file_count;
    unsigned long long tick_last_total_regular_file_bytes;
    unsigned long long tick_last_largest_file_bytes;
    char tick_last_largest_file_name[YVEX_PATH_CAP];
    int source_manifest_written;
    int native_inventory_written;
    int report_written;
    int registry_written;
} yvex_model_download_report;

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

typedef struct {
    int checked;
    int ok_count;
    int truncated_count;
    int invalid_count;
    char status[32];
} yvex_model_download_safetensors_check;

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
