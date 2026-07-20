/* Owner: src/cli/input
 * Owns: argc/argv parsing into typed model artifact report request fields.
 * Does not own: registry lookup, model gate checks, report building, rendering, stdout/stderr, artifact emission,
 *   runtime generation, eval, benchmark, or release decisions.
 * Invariants: parser performs no artifact IO and calls no report builders.
 * Boundary: argument parsing is not artifact emission or runtime support.
 * Purpose: provide argc/argv parsing into typed model artifact report request fields.
 * Inputs: bounded command arguments and caller-owned typed request storage.
 * Effects: publishes request fields only after complete grammar validation.
 * Failure: invalid or ambiguous grammar leaves the request uncommitted. */
#include "src/cli/input/private.h"
#include "src/cli/model_artifacts/private.h"

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *const literal_lines_0[] = {
    "yvex: fullmodel requires report, materialization-plan, materialize, descriptor, or "
    "family-runtime",
    "usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] "
    "[--residency "
    "resident|host-staged|ssd-staged|hybrid] [--limit-tensors N]",
    "usage: yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--dry-run] "
    "[--plan-"
    "only] [--limit-bytes N]",
    "usage: yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] "
    "[--"
    "format text] [--limit-tensors N]",
    "usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] "
    "[--backend cpu|cuda]"};

static const char *const literal_lines_1[] = {
    "usage: yvex fullmodel report --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] "
    "[--limit-tensors N]",
    "usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] "
    "[--residency "
    "resident|host-staged|ssd-staged|hybrid] [--limit-tensors N]",
    "usage: yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--dry-run] "
    "[--plan-"
    "only] [--limit-bytes N]",
    "usage: yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] "
    "[--"
    "format text] [--limit-tensors N]",
    "usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] "
    "[--backend cpu|cuda]"};

static const yvex_models_option_spec download_bound_options[] = {
    {"--models-root", YVEX_MODELS_OPTION_TEXT,
     offsetof(yvex_cli_models_download_options, models_root)},
    {"--repo", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, repo)},
    {"--family", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, family)},
    {"--name", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, name)},
    {"--revision", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, revision)},
    {"--asset", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, asset)},
    {"--asset-name", YVEX_MODELS_OPTION_TEXT,
     offsetof(yvex_cli_models_download_options, asset_name)},
    {"--token-env", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, token_env)},
    {"--cli", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_models_download_options, cli)},
    {"--dry-run", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_models_download_options, dry_run)},
    {"--no-manifest", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, no_manifest)},
    {"--no-native-inventory", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, no_native_inventory)},
    {"--force-sidecars", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, force_sidecars)},
    {"--yes", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_models_download_options, yes)},
    {"--clear-stale-locks", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, clear_stale_locks)},
    {"--force", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_models_download_options, force)},
    {"--match-provider-process", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, match_provider_process)},
    {"--stale-locks", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, cleanup_stale_locks)},
    {"--logs", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_models_download_options, cleanup_logs)},
    {"--receipts", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, cleanup_receipts)},
    {"--failed-partials", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, cleanup_failed_partials)},
    {"--all-provider-cache", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_models_download_options, cleanup_all_provider_cache)},
};

static const yvex_models_option_spec fullmodel_bound_options[] = {
    {"--model", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, model)},
    {"--backend", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, backend)},
    {"--target", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, target)},
    {"--registry", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, registry_path)},
    {"--family", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, family)},
    {"--residency", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, residency)},
    {"--require-role", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, require_role)},
    {"--require-collection", YVEX_MODELS_OPTION_TEXT,
     offsetof(yvex_cli_fullmodel_options, require_collection)},
    {"--fail-after-phase", YVEX_MODELS_OPTION_TEXT,
     offsetof(yvex_cli_fullmodel_options, fail_after_phase)},
    {"--report-dir", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, report_dir)},
    {"--format", YVEX_MODELS_OPTION_TEXT, offsetof(yvex_cli_fullmodel_options, format)},
    {"--dry-run", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_fullmodel_options, dry_run)},
    {"--plan-only", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_fullmodel_options, plan_only)},
    {"--include-blockers", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_fullmodel_options, include_blockers)},
    {"--include-roles", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_fullmodel_options, include_roles)},
    {"--include-placement", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_fullmodel_options, include_placement)},
    {"--include-graph", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_fullmodel_options, include_graph)},
    {"--include-kv", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_fullmodel_options, include_kv)},
    {"--include-logits", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_fullmodel_options, include_logits)},
    {"--include-moe", YVEX_MODELS_OPTION_FLAG, offsetof(yvex_cli_fullmodel_options, include_moe)},
    {"--include-output", YVEX_MODELS_OPTION_FLAG,
     offsetof(yvex_cli_fullmodel_options, include_output)},
    {"--output", YVEX_MODELS_OPTION_OUTPUT, offsetof(yvex_cli_fullmodel_options, output_mode)},
};

/* Purpose: Parse model artifacts args parse into typed CLI state (`yvex_model_artifacts_args_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_model_artifacts_args_parse(int argc, char **argv, yvex_model_artifacts_args *out,
                                    yvex_error *err) {
    int i;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifacts_args",
                       "output args are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->request.kind = YVEX_MODEL_ARTIFACT_REPORT_STATUS;
    out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_NORMAL;

    for (i = 0; i < argc; ++i) {
        const char *arg = argv ? argv[i] : NULL;
        if (!arg)
            continue;
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "help") == 0) {
            out->help_requested = 1;
        } else if (strcmp(arg, "list") == 0) {
            out->request.kind = YVEX_MODEL_ARTIFACT_REPORT_LIST;
        } else if (strcmp(arg, "check") == 0) {
            out->request.kind = YVEX_MODEL_ARTIFACT_REPORT_CHECK;
        } else if (strcmp(arg, "--audit") == 0) {
            out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_AUDIT;
        } else if (strcmp(arg, "--output") == 0 && i + 1 < argc) {
            const char *mode = argv[++i];
            if (strcmp(mode, "table") == 0)
                out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_TABLE;
            else if (strcmp(mode, "audit") == 0)
                out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_AUDIT;
            else
                out->request.mode = YVEX_MODEL_ARTIFACT_RENDER_NORMAL;
        } else if (strcmp(arg, "--registry") == 0 && i + 1 < argc) {
            out->request.registry_path = argv[++i];
        } else if (strcmp(arg, "--model") == 0 && i + 1 < argc) {
            out->request.model_ref = argv[++i];
        } else if (strcmp(arg, "--path") == 0 && i + 1 < argc) {
            out->request.artifact_path = argv[++i];
        }
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Compute fullmodel string is empty for its CLI invariant (`fullmodel_string_is_empty`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_string_is_empty(const char *text) {
    return !text || !text[0];
}

/* Purpose: Parse fullmodel parse value option into typed CLI state (`fullmodel_parse_value_option`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_parse_value_option(const char *flag, int arg_count, char **args, int *index,
                                        const char **value) {
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: fullmodel %s requires a value\n", flag);
        return 2;
    }
    *value = args[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        yvex_cli_out_writef(stderr, "yvex: fullmodel %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

/* Purpose: Transfer bounded model download progress mode name data (`model_download_progress_mode_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *model_download_progress_mode_name(yvex_model_download_progress_mode mode) {
    switch (mode) {
    case YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO:
        return "auto";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE:
        return "live";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN:
        return "plain";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_LOG:
        return "log";
    case YVEX_MODEL_DOWNLOAD_PROGRESS_OFF:
        return "off";
    }
    return "auto";
}

/* Purpose: Transfer bounded model download signal name data (`model_download_signal_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *model_download_signal_name(int signo) {
    switch (signo) {
    case SIGINT:
        return "SIGINT";
    case SIGTERM:
        return "SIGTERM";
    case SIGKILL:
        return "SIGKILL";
    case 0:
        return "none";
    }
    return "unknown";
}

/* Purpose: Parse model download parse progress mode into typed CLI state (`model_download_parse_progress_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int model_download_parse_progress_mode(const char *value,
                                              yvex_model_download_progress_mode *out) {
    if (!value || !out)
        return 0;
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

/* Purpose: Transfer bounded model download effective progress mode data (`model_download_effective_progress_mode`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
yvex_model_download_progress_mode
model_download_effective_progress_mode(yvex_model_download_progress_mode mode) {
    if (mode != YVEX_MODEL_DOWNLOAD_PROGRESS_AUTO) {
        return mode;
    }
    return isatty(STDOUT_FILENO) && isatty(STDERR_FILENO) ? YVEX_MODEL_DOWNLOAD_PROGRESS_LIVE
                                                          : YVEX_MODEL_DOWNLOAD_PROGRESS_PLAIN;
}

/* Purpose: Parse model download auth mode parse into typed CLI state (`model_download_auth_mode_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int model_download_auth_mode_parse(const char *value, yvex_model_download_auth_mode *out) {
    if (!value || !out)
        return 0;
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

/* Purpose: Transfer bounded model download auth mode name data (`model_download_auth_mode_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
const char *model_download_auth_mode_name(yvex_model_download_auth_mode mode) {
    switch (mode) {
    case YVEX_MODEL_DOWNLOAD_AUTH_REQUIRED:
        return "required";
    case YVEX_MODEL_DOWNLOAD_AUTH_NEVER:
        return "never";
    case YVEX_MODEL_DOWNLOAD_AUTH_AUTO:
    default:
        return "auto";
    }
}

/* Purpose: Validate model download options validate before downstream use (`model_download_options_validate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int model_download_options_validate(yvex_cli_models_download_options *options) {
    if (!options->target && !options->repo) {
        yvex_cli_out_writef(stderr, "yvex: models download requires TARGET or --repo OWNER/NAME\n");
        return 2;
    }
    if (options->target && options->repo) {
        yvex_cli_out_writef(stderr,
                            "yvex: models download accepts either TARGET or --repo, not both\n");
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
        yvex_cli_out_writef(stderr,
                            "yvex: models download --provider github requires --repo OWNER/NAME\n");
        return 2;
    }
    if (strcmp(options->provider, "github") == 0 && !options->asset) {
        yvex_cli_out_writef(stderr,
                            "yvex: models download --provider github requires --asset GLOB\n");
        return 2;
    }
    if (strcmp(options->provider, "github") == 0 && options->target) {
        yvex_cli_out_writef(
            stderr,
            "yvex: models download catalog targets use Hugging Face provider in this wave\n");
        return 2;
    }
    if (strcmp(options->provider, "github") != 0 && options->repo &&
        (!options->family || !model_download_family_valid(options->family))) {
        yvex_cli_out_writef(
            stderr, "yvex: models download --repo requires --family deepseek|glm|qwen|gemma\n");
        return 2;
    }
    if (options->repo && !options->name) {
        options->name =
            options->asset_name ? options->asset_name : model_download_repo_basename(options->repo);
    }
    if (options->repo && !model_download_local_name_valid(options->name)) {
        yvex_cli_out_writef(
            stderr, "yvex: models download --name is required and must be a local model name\n");
        return 2;
    }
    return 0;
}

/* Purpose: Parse parse models download options from into typed CLI state (`parse_models_download_options_from`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int parse_models_download_options_from(int arg_count, char **args, int start_index,
                                       yvex_cli_models_download_options *options) {
    int i;

    if (!options)
        return 2;
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
        int handled = 0;
        int rc = parse_models_bound_option(
            "models download", arg_count, args, &i, options, download_bound_options,
            sizeof(download_bound_options) / sizeof(download_bound_options[0]), &handled);
        if (rc != 0)
            return rc;
        if (handled)
            continue;

        if (strcmp(args[i], "--source") == 0 || strcmp(args[i], "--provider") == 0 ||
            strcmp(args[i], "--release") == 0 || strcmp(args[i], "--github-source") == 0 ||
            strcmp(args[i], "--auth") == 0 || strcmp(args[i], "--include") == 0 ||
            strcmp(args[i], "--exclude") == 0 || strcmp(args[i], "--max-workers") == 0 ||
            strcmp(args[i], "--progress") == 0 || strcmp(args[i], "--tick-seconds") == 0 ||
            strcmp(args[i], "--timeout-seconds") == 0 || strcmp(args[i], "--output") == 0) {
            const char *flag = args[i];
            rc = parse_models_value_option("models download", flag, arg_count, args, &i, &value);
            if (rc != 0)
                return rc;
            if (strcmp(flag, "--source") == 0) {
                if (strcmp(value, "hf") != 0) {
                    yvex_cli_out_writef(stderr,
                                        "yvex: models download --source supports hf only\n");
                    return 2;
                }
                options->source = value;
                if (!options->provider)
                    options->provider = "huggingface";
            } else if (strcmp(flag, "--provider") == 0) {
                yvex_account_provider provider;
                if (!yvex_account_provider_from_name(value, &provider)) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: models download --provider requires hf|huggingface|gh|github\n");
                    return 2;
                }
                options->provider = yvex_account_provider_name(provider);
            } else if (strcmp(flag, "--release") == 0) {
                options->release = value;
                options->revision = value;
            } else if (strcmp(flag, "--github-source") == 0) {
                if (strcmp(value, "release-asset") != 0) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: models download --github-source supports release-asset only\n");
                    return 2;
                }
                options->github_source = value;
            } else if (strcmp(flag, "--auth") == 0) {
                if (!model_download_auth_mode_parse(value, &options->auth_mode)) {
                    yvex_cli_out_writef(
                        stderr, "yvex: models download --auth requires auto|required|never\n");
                    return 2;
                }
            } else if (strcmp(flag, "--include") == 0) {
                if (options->include_count >= YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    yvex_cli_out_writef(stderr,
                                        "yvex: models download too many --include patterns\n");
                    return 2;
                }
                options->include_patterns[options->include_count++] = value;
            } else if (strcmp(flag, "--exclude") == 0) {
                if (options->exclude_count >= YVEX_MODEL_DOWNLOAD_PATTERN_CAP) {
                    yvex_cli_out_writef(stderr,
                                        "yvex: models download too many --exclude patterns\n");
                    return 2;
                }
                options->exclude_patterns[options->exclude_count++] = value;
            } else if (strcmp(flag, "--max-workers") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: models download --max-workers requires a positive integer\n");
                    return 2;
                }
                options->max_workers = parsed;
            } else if (strcmp(flag, "--progress") == 0) {
                if (!model_download_parse_progress_mode(value, &options->progress_mode)) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: models download --progress requires auto|live|plain|log|off\n");
                    return 2;
                }
            } else if (strcmp(flag, "--tick-seconds") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: models download --tick-seconds requires a positive integer\n");
                    return 2;
                }
                options->tick_seconds = parsed;
            } else if (strcmp(flag, "--timeout-seconds") == 0) {
                unsigned long long parsed = 0ull;
                if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: models download --timeout-seconds requires a positive integer\n");
                    return 2;
                }
                options->timeout_seconds = parsed;
            } else if (strcmp(flag, "--output") == 0) {
                if (!parse_models_output_mode(value, &options->output_mode)) {
                    yvex_cli_out_writef(
                        stderr, "yvex: models download unsupported output mode: %s\n", value);
                    return 2;
                }
            }
        } else if (strcmp(args[i], "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--no-progress") == 0) {
            options->progress_mode = YVEX_MODEL_DOWNLOAD_PROGRESS_OFF;
        } else if (strcmp(args[i], "--json") == 0) {
            yvex_cli_out_writef(stderr, "yvex: models download JSON output is unsupported; use "
                                        "--output normal|table|audit\n");
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
            yvex_cli_out_writef(
                stderr, "yvex: models download received extra positional argument: %s\n", args[i]);
            return 2;
        }
    }

    return model_download_options_validate(options);
}

/* Purpose: Transfer bounded model download read small file data (`model_download_read_small_file`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int model_download_read_small_file(const char *path, char *buf, size_t cap) {
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u)
        return 0;
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp)
        return 0;
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

/* Purpose: Transfer bounded model download json i64 field data (`model_download_json_i64_field`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
long long model_download_json_i64_field(const char *text, const char *key) {
    char needle[96];
    const char *p;

    if (!text || !key)
        return -1;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p)
        return -1;
    p = strchr(p, ':');
    if (!p)
        return -1;
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    return strtoll(p, NULL, 10);
}

/* Purpose: Transfer bounded model download json string field data (`model_download_json_string_field`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int model_download_json_string_field(const char *text, const char *key, char *out, size_t cap) {
    char needle[96];
    const char *p;
    const char *q;
    size_t len;

    if (out && cap > 0u)
        out[0] = '\0';
    if (!text || !key || !out || cap == 0u)
        return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p)
        return 0;
    p = strchr(p, ':');
    if (!p)
        return 0;
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '"')
        return 0;
    p++;
    q = strchr(p, '"');
    if (!q)
        return 0;
    len = (size_t)(q - p);
    if (len >= cap)
        len = cap - 1u;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

/* Purpose: Transfer bounded model download identity family hint data (`model_download_identity_family_hint`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int model_download_identity_family_hint(const char *target, char *family,
                                               size_t family_cap) {
    if (family && family_cap > 0u)
        family[0] = '\0';
    if (!target || !family || family_cap == 0u)
        return 0;
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

/* Purpose: Transfer bounded model download identity paths data (`model_download_identity_paths`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int model_download_identity_paths(const char *target, const char *family,
                                  const yvex_operator_paths *operator_paths,
                                  yvex_model_download_resolved_target *out, yvex_error *err) {
    char reports_family_dir[YVEX_PATH_CAP];
    char registry_family_dir[YVEX_PATH_CAP];
    char file_name[256];
    int rc;

    if (!target || !family || !operator_paths || !out)
        return 0;
    rc = path_join2(reports_family_dir, sizeof(reports_family_dir), operator_paths->reports_root,
                    family, err, "models_download_identity");
    if (rc == YVEX_OK) {
        rc = path_join2(registry_family_dir, sizeof(registry_family_dir),
                        operator_paths->registry_root, family, err, "models_download_identity");
    }
    if (rc != YVEX_OK)
        return 0;

    snprintf(file_name, sizeof(file_name), "%s.download.json", target);
    rc = path_join2(out->registry_path, sizeof(out->registry_path), registry_family_dir, file_name,
                    err, "models_download_identity");
    snprintf(file_name, sizeof(file_name), "%s.download-report.json", target);
    if (rc == YVEX_OK) {
        rc = path_join2(out->download_report_path, sizeof(out->download_report_path),
                        reports_family_dir, file_name, err, "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.source-manifest.json", target);
    if (rc == YVEX_OK) {
        rc = path_join2(out->manifest_path, sizeof(out->manifest_path), reports_family_dir,
                        file_name, err, "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.native-inventory.json", target);
    if (rc == YVEX_OK) {
        rc = path_join2(out->native_inventory_path, sizeof(out->native_inventory_path),
                        reports_family_dir, file_name, err, "models_download_identity");
    }
    return rc == YVEX_OK;
}

/* Purpose: Transfer bounded model download read identity file data (`model_download_read_identity_file`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int model_download_read_identity_file(const char *path, const char *target, const char *family,
                                      yvex_model_download_resolved_target *out) {
    char buf[16384];
    char parsed_target[128];
    char parsed_family[32];
    char parsed_repo[256];
    char parsed_provider[32];
    char parsed_revision[128];
    char parsed_source[YVEX_PATH_CAP];

    if (!path || !path[0] || !target || !family || !out)
        return 0;
    if (access(path, F_OK) != 0)
        return 0;
    if (!model_download_read_small_file(path, buf, sizeof(buf)))
        return 0;

    memset(parsed_target, 0, sizeof(parsed_target));
    memset(parsed_family, 0, sizeof(parsed_family));
    memset(parsed_repo, 0, sizeof(parsed_repo));
    memset(parsed_provider, 0, sizeof(parsed_provider));
    memset(parsed_revision, 0, sizeof(parsed_revision));
    memset(parsed_source, 0, sizeof(parsed_source));

    model_download_json_string_field(buf, "target_id", parsed_target, sizeof(parsed_target));
    if (parsed_target[0] && strcmp(parsed_target, target) != 0)
        return 0;
    model_download_json_string_field(buf, "family", parsed_family, sizeof(parsed_family));
    if (parsed_family[0] && strcmp(parsed_family, family) != 0)
        return 0;
    model_download_json_string_field(buf, "repo_id", parsed_repo, sizeof(parsed_repo));
    if (!parsed_repo[0]) {
        model_download_json_string_field(buf, "repo", parsed_repo, sizeof(parsed_repo));
    }
    model_download_json_string_field(buf, "provider", parsed_provider, sizeof(parsed_provider));
    model_download_json_string_field(buf, "revision", parsed_revision, sizeof(parsed_revision));
    model_download_json_string_field(buf, "local_source_dir", parsed_source, sizeof(parsed_source));
    if (!parsed_source[0]) {
        model_download_json_string_field(buf, "path", parsed_source, sizeof(parsed_source));
    }

    snprintf(out->target_id, sizeof(out->target_id), "%s",
             parsed_target[0] ? parsed_target : target);
    snprintf(out->family, sizeof(out->family), "%s", parsed_family[0] ? parsed_family : family);
    snprintf(out->repo_id, sizeof(out->repo_id), "%s", parsed_repo[0] ? parsed_repo : "unknown");
    snprintf(out->provider, sizeof(out->provider), "%s",
             parsed_provider[0] ? parsed_provider : "huggingface");
    snprintf(out->revision, sizeof(out->revision), "%s",
             parsed_revision[0] ? parsed_revision : "main");
    snprintf(out->local_name, sizeof(out->local_name), "%s", target);
    if (parsed_source[0]) {
        snprintf(out->local_source_dir, sizeof(out->local_source_dir), "%s", parsed_source);
    }
    out->found = 1;
    return 1;
}

/* Purpose: Construct the owned model download resolve downloaded target state
 * (`model_download_resolve_downloaded_target`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int model_download_resolve_downloaded_target(const char *target,
                                             const yvex_operator_paths *operator_paths,
                                             yvex_model_download_resolved_target *out,
                                             yvex_error *err) {
    static const char *families[] = {"qwen", "gemma", "deepseek", "glm", "github"};
    char hinted_family[32];
    unsigned long pass;

    if (!out)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!target || !target[0] || !operator_paths)
        return 0;
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
            if (!model_download_identity_paths(target, family, operator_paths, &candidate, err)) {
                continue;
            }
            if (model_download_read_identity_file(candidate.registry_path, target, family,
                                                  &candidate) ||
                model_download_read_identity_file(candidate.download_report_path, target, family,
                                                  &candidate) ||
                model_download_read_identity_file(candidate.manifest_path, target, family,
                                                  &candidate)) {
                if (!candidate.local_source_dir[0] && strcmp(candidate.provider, "github") != 0 &&
                    path_join2(hf_family_dir, sizeof(hf_family_dir), operator_paths->hf_root,
                               family, err, "models_download_identity") == YVEX_OK) {
                    (void)path_join2(candidate.local_source_dir, sizeof(candidate.local_source_dir),
                                     hf_family_dir, target, err, "models_download_identity");
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

/* Purpose: Compute fullmodel phase name is valid for its CLI invariant (`fullmodel_phase_name_is_valid`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_phase_name_is_valid(const char *phase) {
    static const char *const phases[] = {"preflight",
                                         "resolve-model",
                                         "artifact-identity",
                                         "tensor-inventory",
                                         "role-coverage",
                                         "placement-plan",
                                         "memory-budget",
                                         "backend-preflight",
                                         "materialize-embedding",
                                         "materialize-normalization",
                                         "materialize-attention",
                                         "materialize-mlp",
                                         "materialize-moe",
                                         "materialize-output",
                                         "materialize-tokenizer",
                                         "cleanup",
                                         "complete",
                                         "failed"};
    unsigned int i;

    if (!phase || !phase[0])
        return 0;
    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        if (strcmp(phase, phases[i]) == 0)
            return 1;
    }
    return 0;
}

/* Purpose: Orchestrate the typed fullmodel command is materialize request (`fullmodel_command_is_materialize`). */
static int fullmodel_command_is_materialize(const yvex_cli_fullmodel_options *options) {
    return options && options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
}

/* Purpose: Orchestrate the typed fullmodel command is descriptor request (`fullmodel_command_is_descriptor`). */
static int fullmodel_command_is_descriptor(const yvex_cli_fullmodel_options *options) {
    return options && options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
}

/* Purpose: Orchestrate the typed fullmodel command is family runtime request
 *   (`fullmodel_command_is_family_runtime`). */
static int fullmodel_command_is_family_runtime(const yvex_cli_fullmodel_options *options) {
    return options && options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;
}

/* Purpose: Orchestrate the typed fullmodel command accepts includes request (`fullmodel_command_accepts_includes`). */
static int fullmodel_command_accepts_includes(const yvex_cli_fullmodel_options *options) {
    return fullmodel_command_is_descriptor(options) || fullmodel_command_is_family_runtime(options);
}

/* Purpose: Orchestrate the typed fullmodel command accepts requirements request
 *   (`fullmodel_command_accepts_requirements`). */
static int fullmodel_command_accepts_requirements(const yvex_cli_fullmodel_options *options) {
    return fullmodel_command_is_materialize(options) || fullmodel_command_is_descriptor(options);
}

/* Purpose: Compute fullmodel options begin for its CLI invariant (`fullmodel_options_begin`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_options_begin(int arg_count, char **args,
                                   yvex_cli_fullmodel_options *options) {
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->residency = "resident";
    options->format = "text";
    options->family = "auto";
    options->limit_tensors = 5ull;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    options->command = YVEX_FULLMODEL_COMMAND_REPORT;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_artifacts_surface_fullmodel_help(stdout);
        return 1;
    }
    if (arg_count < 3) {
        yvex_cli_out_lines(stderr, literal_lines_0,
                           sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
        return 2;
    }
    if (strcmp(args[2], "report") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_REPORT;
    } else if (strcmp(args[2], "materialization-plan") == 0 || strcmp(args[2], "plan") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    } else if (strcmp(args[2], "materialize") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    } else if (strcmp(args[2], "descriptor") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    } else if (strcmp(args[2], "family-runtime") == 0) {
        options->command = YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;
    } else {
        yvex_cli_out_writef(stderr, "yvex: unknown fullmodel subcommand: %s\n", args[2]);
        yvex_cli_out_lines(stderr, literal_lines_1,
                           sizeof(literal_lines_1) / sizeof(literal_lines_1[0]));
        return 2;
    }
    return 0;
}

/* Require the model operand after every command-specific option has parsed. */
/* Purpose: Compute fullmodel options finish for its CLI invariant (`fullmodel_options_finish`). */
static int fullmodel_options_finish(const yvex_cli_fullmodel_options *options) {
    const char *name = "report";

    if (options->model)
        return 0;
    if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN) {
        name = "materialization-plan";
    } else if (options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE) {
        name = "materialize";
    } else if (options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR) {
        name = "descriptor";
    } else if (options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME) {
        name = "family-runtime";
    }
    yvex_cli_out_writef(stderr, "yvex: fullmodel %s requires --model FILE_OR_ALIAS\n", name);
    return 2;
}

/* Purpose: Parse model artifacts fullmodel options parse into typed CLI state
 * (`model_artifacts_fullmodel_options_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int model_artifacts_fullmodel_options_parse(int arg_count, char **args,
                                            yvex_cli_fullmodel_options *options) {
    int i;
    int begin_rc;

    if (!options)
        return 2;
    begin_rc = fullmodel_options_begin(arg_count, args, options);
    if (begin_rc != 0)
        return begin_rc;

    for (i = 3; i < arg_count; ++i) {
        const char *value = NULL;
        const char *flag = args[i];
        int handled = 0;
        int rc = parse_models_bound_option(
            "fullmodel", arg_count, args, &i, options, fullmodel_bound_options,
            sizeof(fullmodel_bound_options) / sizeof(fullmodel_bound_options[0]), &handled);
        if (rc != 0)
            return rc;
        if (handled) {
            value = i > 3 ? args[i] : NULL;
            if (strcmp(flag, "--backend") == 0 && strcmp(options->backend, "cpu") != 0 &&
                strcmp(options->backend, "cuda") != 0) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --backend must be cpu or cuda\n");
                return 2;
            }
            if (strcmp(flag, "--family") == 0 && !fullmodel_command_is_family_runtime(options)) {
                yvex_cli_out_writef(stderr,
                                    "yvex: fullmodel --family is only valid with family-runtime\n");
                return 2;
            }
            if (strcmp(flag, "--residency") == 0 && strcmp(options->residency, "resident") != 0 &&
                strcmp(options->residency, "host-staged") != 0 &&
                strcmp(options->residency, "ssd-staged") != 0 &&
                strcmp(options->residency, "hybrid") != 0 &&
                strcmp(options->residency, "ssd-streamed") != 0 &&
                strcmp(options->residency, "managed-memory") != 0 &&
                strcmp(options->residency, "distributed") != 0) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel --residency must be resident, "
                                            "host-staged, ssd-staged, hybrid, ssd-"
                                            "streamed, managed-memory, or distributed\n");
                return 2;
            }
            if ((strcmp(flag, "--require-role") == 0 ||
                 strcmp(flag, "--require-collection") == 0) &&
                !fullmodel_command_accepts_requirements(options)) {
                yvex_cli_out_writef(
                    stderr, "yvex: fullmodel %s is only valid with materialize or descriptor\n",
                    flag);
                return 2;
            }
            if ((strcmp(flag, "--dry-run") == 0 || strcmp(flag, "--plan-only") == 0 ||
                 strcmp(flag, "--report-dir") == 0 || strcmp(flag, "--fail-after-phase") == 0) &&
                !fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(stderr, "yvex: fullmodel %s is only valid with materialize\n",
                                    flag);
                return 2;
            }
            if (strcmp(flag, "--fail-after-phase") == 0 &&
                !fullmodel_phase_name_is_valid(options->fail_after_phase)) {
                yvex_cli_out_writef(
                    stderr,
                    "yvex: fullmodel --fail-after-phase value is not a known materialize phase\n");
                return 2;
            }
            if (strcmp(flag, "--format") == 0) {
                if (!fullmodel_command_is_descriptor(options)) {
                    yvex_cli_out_writef(stderr,
                                        "yvex: fullmodel --format is only valid with descriptor\n");
                    return 2;
                }
                if (strcmp(options->format, "text") != 0) {
                    yvex_cli_out_writef(
                        stderr,
                        "yvex: fullmodel descriptor currently supports --format text only\n");
                    return 2;
                }
            }
            if ((strcmp(flag, "--include-blockers") == 0 ||
                 strcmp(flag, "--include-placement") == 0 || strcmp(flag, "--include-graph") == 0 ||
                 strcmp(flag, "--include-kv") == 0 || strcmp(flag, "--include-logits") == 0) &&
                !fullmodel_command_accepts_includes(options)) {
                yvex_cli_out_writef(
                    stderr, "yvex: fullmodel %s is only valid with descriptor or family-runtime\n",
                    flag);
                return 2;
            }
            if ((strcmp(flag, "--include-roles") == 0 || strcmp(flag, "--include-moe") == 0 ||
                 strcmp(flag, "--include-output") == 0) &&
                !fullmodel_command_is_family_runtime(options)) {
                yvex_cli_out_writef(stderr,
                                    "yvex: fullmodel %s is only valid with family-runtime\n", flag);
                return 2;
            }
            continue;
        }

        if (strcmp(flag, "--limit-tensors") == 0) {
            unsigned long long parsed = 0ull;
            rc = fullmodel_parse_value_option(flag, arg_count, args, &i, &value);
            if (rc != 0)
                return rc;
            if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                yvex_cli_out_writef(
                    stderr, "yvex: fullmodel --limit-tensors requires a positive integer\n");
                return 2;
            }
            options->limit_tensors = parsed > 16ull ? 16ull : parsed;
        } else if (strcmp(flag, "--limit-bytes") == 0) {
            unsigned long long parsed = 0ull;
            rc = fullmodel_parse_value_option(flag, arg_count, args, &i, &value);
            if (rc != 0)
                return rc;
            if (!fullmodel_command_is_materialize(options)) {
                yvex_cli_out_writef(
                    stderr, "yvex: fullmodel --limit-bytes is only valid with materialize\n");
                return 2;
            }
            if (!parse_positive_ull(value, &parsed) || parsed == 0ull) {
                yvex_cli_out_writef(stderr,
                                    "yvex: fullmodel --limit-bytes requires a positive integer\n");
                return 2;
            }
            options->limit_bytes = parsed;
            options->has_limit_bytes = 1;
        } else if (strcmp(flag, "--audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown fullmodel option: %s\n", flag);
            return 2;
        }
    }
    return fullmodel_options_finish(options);
}
