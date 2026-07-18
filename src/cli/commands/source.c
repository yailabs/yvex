/*
 * source.c - source command adapter.
 *
 * Owner: src/cli/commands.
 * Owns: source command dispatch from parsed input to source report builder and renderer.
 * Does not own: source report facts, local scanning, rendering internals, runtime, generation, eval, or benchmark.
 * Invariants: adapter stays thin and does not hide domain behavior.
 * Boundary: command dispatch is not source verification or runtime readiness.
 *
 * Purpose: bind source-manifest report CLI input to the typed source report API.
 * Inputs: argv from yvex source-manifest report.
 * Effects: renders source report output or parser errors.
 * Failure: returns parser, report-builder, or renderer exit codes.
 */
#include "src/cli/input/source.h"
#include "src/cli/render/source.h"
#include "src/cli/io/out.h"

#include <string.h>
#include <stdio.h>
#include <yvex/source_manifest.h>

int yvex_source_manifest_report_command(int argc, char **argv);
void yvex_source_manifest_help(FILE *fp);

static int source_cli_exit_for_status(int status)
{
    if (status == YVEX_OK) return 0;
    if (status == YVEX_ERR_INVALID_ARG) return 2;
    if (status == YVEX_ERR_FORMAT || status == YVEX_ERR_BOUNDS) return 4;
    if (status == YVEX_ERR_UNSUPPORTED) return 5;
    if (status == YVEX_ERR_IO || status == YVEX_ERR_NOMEM) return 3;
    return 3;
}

static int source_cli_print_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(stderr, "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return source_cli_exit_for_status(status);
}

static int source_cli_parse_status(const char *text, yvex_source_status *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "unknown") == 0) {
        *out = YVEX_SOURCE_STATUS_UNKNOWN;
        return 1;
    }
    if (strcmp(text, "in-progress") == 0) {
        *out = YVEX_SOURCE_STATUS_IN_PROGRESS;
        return 1;
    }
    if (strcmp(text, "incomplete") == 0) {
        *out = YVEX_SOURCE_STATUS_INCOMPLETE;
        return 1;
    }
    if (strcmp(text, "complete") == 0) {
        *out = YVEX_SOURCE_STATUS_COMPLETE;
        return 1;
    }
    if (strcmp(text, "failed") == 0) {
        *out = YVEX_SOURCE_STATUS_FAILED;
        return 1;
    }
    return 0;
}

static int source_cli_create_manifest(int argc, char **argv)
{
    yvex_source_manifest_options options;
    yvex_source_manifest_summary summary;
    yvex_error err;
    const char *out_path = NULL;
    int i;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    yvex_error_clear(&err);
    options.status = YVEX_SOURCE_STATUS_UNKNOWN;
    options.include_files = 1;

    i = 3;
    while (i < argc) {
        const char *name = argv[i];
        const char *value;

        if (i + 1 >= argc) {
            yvex_cli_out_writef(stderr, "yvex: option requires a value: %s\n", name);
            return 2;
        }
        value = argv[i + 1];

        if (strcmp(name, "--hf-repo") == 0) {
            options.repo = value;
        } else if (strcmp(name, "--revision") == 0) {
            options.revision = value;
        } else if (strcmp(name, "--license") == 0) {
            options.license = value;
        } else if (strcmp(name, "--model-card") == 0) {
            options.model_card = value;
        } else if (strcmp(name, "--local-path") == 0) {
            options.local_path = value;
        } else if (strcmp(name, "--node") == 0) {
            options.node_name = value;
        } else if (strcmp(name, "--status") == 0) {
            if (!source_cli_parse_status(value, &options.status)) {
                yvex_cli_out_writef(stderr, "yvex: unknown source status: %s\n", value);
                return 2;
            }
            if (options.status == YVEX_SOURCE_STATUS_COMPLETE) {
                yvex_cli_out_writef(
                    stderr,
                    "yvex: source status complete is verifier-owned; run strict exact-source verification\n");
                return 2;
            }
        } else if (strcmp(name, "--dry-run-log") == 0) {
            options.dry_run_log = value;
        } else if (strcmp(name, "--download-log") == 0) {
            options.download_log = value;
        } else if (strcmp(name, "--pid-file") == 0) {
            options.pid_file = value;
        } else if (strcmp(name, "--download-command") == 0) {
            options.download_command = value;
        } else if (strcmp(name, "--out") == 0) {
            out_path = value;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown source-manifest option: %s\n", name);
            return 2;
        }
        i += 2;
    }

    if (!options.repo || !options.revision || !options.local_path || !out_path) {
        yvex_cli_out_writef(stderr,
                            "yvex: --hf-repo, --revision, --local-path, and --out are required\n");
        return 2;
    }

    rc = yvex_source_manifest_write_json(out_path, &options, &summary, &err);
    if (rc != YVEX_OK) {
        return source_cli_print_error(&err, rc);
    }

    yvex_cli_out_writef(stdout, "source manifest: written\n");
    yvex_cli_out_writef(stdout, "repo: %s\n", options.repo);
    yvex_cli_out_writef(stdout, "revision: %s\n", options.revision);
    yvex_cli_out_writef(stdout, "local_path: %s\n", options.local_path);
    yvex_cli_out_writef(stdout, "status: %s\n", yvex_source_status_name(options.status));
    yvex_cli_out_writef(stdout, "files: %llu\n", summary.file_count);
    yvex_cli_out_writef(stdout, "safetensors: %llu\n", summary.safetensors_count);
    yvex_cli_out_writef(stdout, "total_size_bytes: %llu\n", summary.total_size_bytes);
    yvex_cli_out_writef(stdout, "out: %s\n", out_path);
    yvex_cli_out_writef(stdout, "status: source-manifest-written\n");
    return 0;
}

int yvex_source_manifest_command(int argc, char **argv)
{
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_source_manifest_help(stdout);
        return 0;
    }

    if (argc < 3) {
        yvex_cli_out_writef(stderr, "yvex: source-manifest requires a subcommand\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE\n");
        yvex_cli_out_writef(stderr, "       yvex source-manifest report --family qwen --release v0.1.0 [options]\n");
        return 2;
    }

    if (strcmp(argv[2], "report") == 0) {
        return yvex_source_manifest_report_command(argc, argv);
    }
    if (strcmp(argv[2], "inspect") == 0) {
        yvex_cli_out_writef(stderr,
                            "yvex: source-manifest inspect is not implemented in open-weight intake\n");
        return 5;
    }
    if (strcmp(argv[2], "create") == 0) {
        return source_cli_create_manifest(argc, argv);
    }

    yvex_cli_out_writef(stderr, "yvex: unknown source-manifest subcommand: %s\n", argv[2]);
    return 2;
}

void yvex_source_manifest_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE [--license TEXT] [--model-card URL] [--node NAME] [--dry-run-log FILE] [--download-log FILE] [--pid-file FILE] [--download-command TEXT]\n");
    yvex_cli_out_writef(fp, "       yvex source-manifest report --family deepseek|qwen|gemma --release v0.1.0 [options]\n\n");
    yvex_cli_out_writef(fp, "Source manifest scans a local official-weight source directory and writes provenance JSON. It does not download, parse safetensors payloads, quantize, emit GGUF, materialize, or infer.\n\n");
    yvex_cli_out_writef(fp, "The DeepSeek report verifies exact source identity and metadata without reading tensor payloads. Qwen and Gemma remain bounded engineering reports. No report emits artifacts, executes runtime paths, generates, evaluates, benchmarks, or marks a release ready.\n");
    yvex_cli_out_writef(fp, "Report options: --source DIR --models-root DIR --target TARGET --" "include-files --" "include-config --" "include-blockers --" "include-next --strict --" "audit --json --" "output normal|table|audit|json\n");
}

int yvex_source_manifest_report_command(int argc, char **argv)
{
    yvex_source_args args;
    yvex_source_report_request request;
    yvex_source_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_source_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        yvex_cli_out_writef(stderr, "%s\n", yvex_error_message(&err));
        return source_cli_exit_for_status(rc);
    }
    if (args.help) {
        yvex_source_render_help(stdout);
        return 0;
    }

    yvex_source_report_request_from_parsed(&request, &args);
    rc = yvex_source_report_build(&request, &report, &err);
    if (rc != YVEX_OK) {
        return source_cli_print_error(&err, rc);
    }

    return yvex_source_render(stdout, args.render_mode, &report);
}
