/*
 * gguf/commands.c - GGUF and intake command adapters.
 *
 * This file owns argv/output adapters for GGUF templates, controlled emission,
 * conversion, quantization manifests, imatrix manifests, source manifests, and
 * native tensor inventory. Domain behavior remains owned by gguf/ and source
 * modules.
 */

#include "../yvex_command_private.h"

static int parse_source_status(const char *text, yvex_source_status *out)
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

static int cli_parse_gguf_template_options(int argc, char **argv, int start,
                                           const char **template_path,
                                           const char **native_source,
                                           int *require_all)
{
    int i = start;

    *template_path = NULL;
    *native_source = NULL;
    *require_all = 0;
    while (i < argc) {
        if (strcmp(argv[i], "--require-all-template-tensors-in-native") == 0) {
            *require_all = 1;
            i++;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: gguf-template option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--template") == 0) {
            *template_path = argv[i + 1];
        } else if (strcmp(argv[i], "--native-source") == 0) {
            *native_source = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown gguf-template option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static void cli_print_template_issues(const yvex_gguf_template *tmpl)
{
    unsigned long long i;
    unsigned long long count = yvex_gguf_template_issue_count(tmpl);

    for (i = 0; i < count; ++i) {
        const yvex_gguf_template_issue *issue = yvex_gguf_template_issue_at(tmpl, i);
        if (!issue) {
            continue;
        }
        if (issue->tensor_name && issue->tensor_name[0] != '\0') {
            printf("issue %llu %s tensor=\"%s\" message=\"%s\"\n",
                   i,
                   yvex_gguf_template_issue_kind_name(issue->kind),
                   issue->tensor_name,
                   issue->message ? issue->message : "");
        } else {
            printf("issue %llu %s message=\"%s\"\n",
                   i,
                   yvex_gguf_template_issue_kind_name(issue->kind),
                   issue->message ? issue->message : "");
        }
    }
}

static int command_gguf_template(int argc, char **argv)
{
    yvex_gguf_template_options options;
    yvex_gguf_template *tmpl = NULL;
    yvex_gguf_template_summary summary;
    yvex_error err;
    const char *template_path;
    const char *native_source;
    int require_all;
    int rc;

    yvex_error_clear(&err);
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("gguf-template"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: gguf-template requires inspect, validate, or compare\n");
        fprintf(stderr, "usage: yvex gguf-template inspect|validate --template FILE\n");
        return 2;
    }
    if (strcmp(argv[2], "inspect") != 0 && strcmp(argv[2], "validate") != 0 &&
        strcmp(argv[2], "compare") != 0) {
        fprintf(stderr, "yvex: unknown gguf-template subcommand: %s\n", argv[2]);
        return 2;
    }
    rc = cli_parse_gguf_template_options(argc, argv, 3, &template_path, &native_source, &require_all);
    if (rc != 0) {
        return rc;
    }
    if (!template_path) {
        fprintf(stderr, "yvex: gguf-template requires --template FILE\n");
        return 2;
    }
    if (strcmp(argv[2], "compare") == 0 && !native_source) {
        fprintf(stderr, "yvex: gguf-template compare requires --native-source DIR\n");
        return 2;
    }

    memset(&options, 0, sizeof(options));
    options.template_path = template_path;
    options.native_source_dir = native_source;
    options.compare_native = strcmp(argv[2], "compare") == 0;
    options.require_all_template_tensors_in_native = require_all;

    rc = yvex_gguf_template_open(&tmpl, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_gguf_template_get_summary(tmpl, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (strcmp(argv[2], "compare") == 0) {
        printf("gguf template: compare\n");
        printf("template: %s\n", template_path);
        printf("native_source: %s\n", native_source);
        printf("template_tensors: %llu\n", summary.tensor_count);
        printf("native_tensors: %llu\n", summary.native_tensor_count);
        printf("matched_exact: %llu\n", summary.matched_exact);
        printf("missing_in_native: %llu\n", summary.missing_in_native);
        printf("shape_mismatch: %llu\n", summary.shape_mismatch);
        printf("status: template-compare-%s\n",
               summary.status == YVEX_GGUF_TEMPLATE_STATUS_VALID ? "valid" :
               summary.status == YVEX_GGUF_TEMPLATE_STATUS_INVALID ? "invalid" : "partial");
        if (summary.missing_in_native > 0 || summary.shape_mismatch > 0) {
            printf("reason: architecture adapter mapping requires open-weight intake\n");
        }
        cli_print_template_issues(tmpl);
    } else if (strcmp(argv[2], "validate") == 0) {
        printf("gguf template: validate\n");
        printf("template: %s\n", template_path);
        printf("status: %s\n", yvex_gguf_template_status_name(summary.status));
        printf("issues: %llu\n", summary.issue_count);
        cli_print_template_issues(tmpl);
    } else {
        printf("gguf template: inspect\n");
        printf("template: %s\n", template_path);
        printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
        printf("model_name: %s\n", summary.model_name ? summary.model_name : "");
        printf("metadata_count: %llu\n", summary.metadata_count);
        printf("tensor_count: %llu\n", summary.tensor_count);
        printf("has_tokenizer: %s\n", summary.has_tokenizer ? "yes" : "no");
        printf("known_roles: %llu\n", summary.known_role_count);
        printf("unknown_roles: %llu\n", summary.unknown_role_count);
        printf("status: %s\n", yvex_gguf_template_status_name(summary.status));
    }

    yvex_gguf_template_close(tmpl);
    return 0;
}

static int command_gguf_emit(int argc, char **argv)
{
    yvex_gguf_emit_options options;
    yvex_gguf_emit_summary summary;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("gguf-emit"));
        return 0;
    }
    if (argc < 3 || strcmp(argv[2], "controlled") != 0) {
        fprintf(stderr, "yvex: gguf-emit requires subcommand controlled\n");
        fprintf(stderr, "usage: yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n");
        return 2;
    }

    options.tensor_name = "embed.weight";
    options.target_name = "token_embd.weight";
    options.model_name = "yvex-owned-gguf-test";
    options.architecture = "llama";
    options.transpose_2d = 1;

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--overwrite") == 0) {
            options.overwrite = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: gguf-emit option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--out") == 0) {
            options.out_path = argv[++i];
        } else if (strcmp(argv[i], "--template") == 0) {
            options.template_path = argv[++i];
        } else if (strcmp(argv[i], "--native-source") == 0) {
            options.native_source_dir = argv[++i];
        } else if (strcmp(argv[i], "--tensor-name") == 0) {
            options.tensor_name = argv[++i];
        } else if (strcmp(argv[i], "--target-name") == 0) {
            options.target_name = argv[++i];
        } else if (strcmp(argv[i], "--target-qtype") == 0) {
            options.target_qtype = argv[++i];
        } else if (strcmp(argv[i], "--model-name") == 0) {
            options.model_name = argv[++i];
        } else if (strcmp(argv[i], "--arch") == 0) {
            options.architecture = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown gguf-emit option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help gguf-emit' for usage.\n");
            return 2;
        }
    }

    if (!options.out_path) {
        fprintf(stderr, "yvex: gguf-emit controlled requires --out FILE\n");
        fprintf(stderr, "usage: yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n");
        return 2;
    }

    rc = yvex_gguf_emit_controlled(&options, &summary, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("gguf emit: controlled\n");
    printf("out: %s\n", summary.out_path ? summary.out_path : "");
    printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
    printf("model_name: %s\n", summary.model_name ? summary.model_name : "");
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("tensor_payload_bytes: %llu\n", summary.tensor_payload_bytes);
    printf("alignment: %llu\n", summary.alignment);
    printf("roundtrip_validated: %s\n", summary.roundtrip_validated ? "yes" : "no");
    printf("status: %s\n", yvex_gguf_emit_status_name(summary.status));
    return 0;
}

static int command_qtype_support(int argc, char **argv)
{
    unsigned long long i;

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("qtype-support"));
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "yvex: qtype-support takes no arguments\n");
        return 2;
    }
    printf("qtype support:\n");
    for (i = 0; i < yvex_qtype_support_count(); ++i) {
        const yvex_qtype_support_info *row = yvex_qtype_support_at(i);
        printf("  %s policy=%s storage=%s emit=%s quantize=%s compute=%s notes=%s\n",
               row->qtype,
               row->policy_supported ? "yes" : "no",
               row->storage_supported ? "yes" : "no",
               row->emit_supported ? "yes" : "no",
               strcmp(row->qtype, "F32") == 0 ? "n/a" : (row->quantize_supported ? "yes" : "no"),
               row->compute_supported ? "partial" : "no",
               row->notes ? row->notes : "");
    }
    printf("status: qtype-support\n");
    return 0;
}

static int command_convert(int argc, char **argv)
{
    yvex_conversion_options options;
    yvex_conversion_summary summary;
    yvex_error err;
    const char *out_plan = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("convert"));
        return 0;
    }
    if (argc < 3 || (strcmp(argv[2], "plan") != 0 && strcmp(argv[2], "emit") != 0)) {
        fprintf(stderr, "yvex: convert requires plan or emit\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--overwrite") == 0) {
            options.overwrite = 1;
            continue;
        }
        if (strcmp(argv[i], "--allow-unsupported-qtype") == 0) {
            options.allow_unsupported_qtype = 1;
            continue;
        }
        if (strcmp(argv[i], "--require-all") == 0) {
            options.require_all = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: convert option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--arch") == 0) options.architecture = argv[++i];
        else if (strcmp(argv[i], "--source-manifest") == 0) options.source_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--native-source") == 0) options.native_source_dir = argv[++i];
        else if (strcmp(argv[i], "--template") == 0) options.template_path = argv[++i];
        else if (strcmp(argv[i], "--quant-policy") == 0) options.quant_policy_path = argv[++i];
        else if (strcmp(argv[i], "--imatrix-manifest") == 0) options.imatrix_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0) options.out_path = argv[++i];
        else if (strcmp(argv[i], "--out-plan") == 0) out_plan = argv[++i];
        else if (strcmp(argv[i], "--tensor") == 0) options.tensor_name = argv[++i];
        else if (strcmp(argv[i], "--target-qtype") == 0) options.target_qtype = argv[++i];
        else if (strcmp(argv[i], "--limit") == 0) {
            char *end = NULL;
            options.limit_tensors = strtoull(argv[++i], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid convert limit\n");
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: unknown convert option: %s\n", argv[i]);
            return 2;
        }
    }

    if (strcmp(argv[2], "plan") == 0) {
        if (!options.architecture || !options.native_source_dir || !out_plan) {
            fprintf(stderr, "yvex: convert plan requires --arch --native-source --out-plan\n");
            return 2;
        }
        rc = yvex_conversion_plan_write_json(&options, out_plan, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        printf("conversion plan: written\n");
        printf("architecture: %s\n", options.architecture);
        printf("native_tensors: %llu\n", summary.native_tensor_count);
        printf("planned_tensors: %llu\n", summary.planned_tensor_count);
        printf("unmapped_tensors: %llu\n", summary.unmapped_tensor_count);
        printf("unsupported_qtypes: %llu\n", summary.unsupported_qtype_count);
        printf("out: %s\n", out_plan);
        printf("status: conversion-plan-written\n");
        return 0;
    }

    if (!options.architecture || !options.native_source_dir || !options.tensor_name ||
        !options.target_qtype || !options.out_path) {
        fprintf(stderr, "yvex: convert emit requires --arch --native-source --tensor --target-qtype --out\n");
        return 2;
    }
    rc = yvex_conversion_emit_gguf(&options, &summary, &err);
    if (rc != YVEX_OK) {
        printf("conversion emit: gguf\n");
        printf("architecture: %s\n", options.architecture);
        printf("source_tensor: %s\n", options.tensor_name);
        printf("target_qtype: %s\n", options.target_qtype);
        printf("status: conversion-failed\n");
        fprintf(stderr, "reason: %s\n", yvex_error_message(&err));
        return exit_for_status(rc);
    }
    printf("conversion emit: gguf\n");
    printf("architecture: %s\n", options.architecture);
    printf("source_tensor: %s\n", options.tensor_name);
    printf("target_qtype: %s\n", options.target_qtype);
    printf("out: %s\n", options.out_path);
    printf("bytes_read: %llu\n", summary.bytes_read);
    printf("bytes_written: %llu\n", summary.bytes_written);
    printf("roundtrip_validated: %s\n", summary.roundtrip_validated ? "yes" : "no");
    printf("execution_ready: false\n");
    printf("status: conversion-gguf-written\n");
    return 0;
}

static int parse_imatrix_create_options(int argc, char **argv,
                                        yvex_imatrix_manifest_options *options,
                                        const char **out_path)
{
    int i = 3;

    while (i < argc) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: imatrix option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--name") == 0) options->name = argv[i + 1];
        else if (strcmp(argv[i], "--arch") == 0) options->architecture = argv[i + 1];
        else if (strcmp(argv[i], "--source-manifest") == 0) options->source_manifest_path = argv[i + 1];
        else if (strcmp(argv[i], "--quant-policy") == 0) options->quant_policy_path = argv[i + 1];
        else if (strcmp(argv[i], "--imatrix") == 0) options->imatrix_path = argv[i + 1];
        else if (strcmp(argv[i], "--format") == 0) options->format = yvex_imatrix_format_from_name(argv[i + 1]);
        else if (strcmp(argv[i], "--status") == 0) options->status = yvex_imatrix_status_from_name(argv[i + 1]);
        else if (strcmp(argv[i], "--dataset") == 0) options->calibration_dataset = argv[i + 1];
        else if (strcmp(argv[i], "--command") == 0) options->calibration_command = argv[i + 1];
        else if (strcmp(argv[i], "--producer") == 0) options->producer = argv[i + 1];
        else if (strcmp(argv[i], "--out") == 0) *out_path = argv[i + 1];
        else {
            fprintf(stderr, "yvex: unknown imatrix option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static int parse_imatrix_manifest_option(int argc, char **argv, const char **manifest_path)
{
    int i = 3;

    while (i < argc) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: imatrix option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--manifest") == 0) {
            *manifest_path = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown imatrix option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static void print_imatrix_summary(const char *mode,
                                  const char *manifest_path,
                                  const yvex_imatrix_summary *summary)
{
    printf("imatrix: %s\n", mode);
    if (manifest_path) printf("manifest: %s\n", manifest_path);
    printf("name: %s\n", summary->name ? summary->name : "");
    printf("architecture: %s\n", summary->architecture ? summary->architecture : "");
    printf("format: %s\n", yvex_imatrix_format_name(summary->format));
    printf("status: %s\n", yvex_imatrix_status_name(summary->status));
    printf("file_exists: %s\n", summary->file_exists ? "yes" : "no");
    printf("source_manifest: %s\n", summary->source_manifest_path ? summary->source_manifest_path : "");
    printf("quant_policy: %s\n", summary->quant_policy_path ? summary->quant_policy_path : "");
    printf("imatrix: %s\n", summary->imatrix_path ? summary->imatrix_path : "");
}

static int command_imatrix(int argc, char **argv)
{
    yvex_error err;
    yvex_imatrix_manifest *manifest = NULL;
    yvex_imatrix_summary summary;
    int rc;

    yvex_error_clear(&err);
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("imatrix"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: imatrix requires create, inspect, or validate\n");
        return 2;
    }

    if (strcmp(argv[2], "create") == 0) {
        yvex_imatrix_manifest_options options;
        const char *out_path = NULL;

        memset(&options, 0, sizeof(options));
        rc = parse_imatrix_create_options(argc, argv, &options, &out_path);
        if (rc != 0) return rc;
        if (!options.name || !options.architecture || !options.imatrix_path || !out_path ||
            options.format == YVEX_IMATRIX_FORMAT_UNKNOWN ||
            options.status == YVEX_IMATRIX_STATUS_UNKNOWN) {
            fprintf(stderr, "yvex: imatrix create requires --name --arch --imatrix --format --status --out\n");
            return 2;
        }
        rc = yvex_imatrix_manifest_create(&manifest, &options, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = yvex_imatrix_manifest_validate(manifest, &err);
        if (rc == YVEX_OK) rc = yvex_imatrix_manifest_write_json(out_path, manifest, &err);
        if (rc == YVEX_OK) rc = yvex_imatrix_manifest_get_summary(manifest, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_imatrix_manifest_close(manifest);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("imatrix manifest: written\n");
        printf("name: %s\n", summary.name);
        printf("architecture: %s\n", summary.architecture);
        printf("format: %s\n", yvex_imatrix_format_name(summary.format));
        printf("status: %s\n", yvex_imatrix_status_name(summary.status));
        printf("file_exists: %s\n", summary.file_exists ? "yes" : "no");
        printf("out: %s\n", out_path);
        printf("status: imatrix-manifest-written\n");
        yvex_imatrix_manifest_close(manifest);
        return 0;
    }

    if (strcmp(argv[2], "inspect") == 0 || strcmp(argv[2], "validate") == 0) {
        const char *manifest_path = NULL;

        rc = parse_imatrix_manifest_option(argc, argv, &manifest_path);
        if (rc != 0) return rc;
        if (!manifest_path) {
            fprintf(stderr, "yvex: imatrix %s requires --manifest FILE\n", argv[2]);
            return 2;
        }
        rc = yvex_imatrix_manifest_open(&manifest, manifest_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (strcmp(argv[2], "validate") == 0) {
            rc = yvex_imatrix_manifest_validate(manifest, &err);
            if (rc != YVEX_OK) {
                yvex_imatrix_manifest_close(manifest);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
        rc = yvex_imatrix_manifest_get_summary(manifest, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_imatrix_manifest_close(manifest);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        print_imatrix_summary(argv[2], manifest_path, &summary);
        if (strcmp(argv[2], "validate") == 0) {
            printf("issues: %llu\n", summary.issue_count);
            printf("requires_imatrix_rules: %llu\n", summary.requires_imatrix_rule_count);
            printf("covered_rules: %llu\n", summary.covered_rule_count);
            printf("uncovered_rules: %llu\n", summary.uncovered_rule_count);
            printf("status: imatrix-%s\n",
                   summary.issue_count == 0 ? "valid" :
                   (summary.file_exists ? "partial" : "invalid"));
        } else {
            printf("status: imatrix-manifest\n");
        }
        yvex_imatrix_manifest_close(manifest);
        return 0;
    }

    fprintf(stderr, "yvex: unknown imatrix subcommand: %s\n", argv[2]);
    return 2;
}

static int command_native_weights(int argc, char **argv)
{
    yvex_native_weight_options options;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_summary summary;
    yvex_error err;
    const char *tensor_name = NULL;
    unsigned long long limit = 20;
    int json = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.recursive = 1;

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("native-weights"));
        return 0;
    }

    i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            i++;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: native-weights option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--source") == 0) {
            options.source_dir = argv[i + 1];
        } else if (strcmp(argv[i], "--limit") == 0) {
            char *end = NULL;
            limit = strtoull(argv[i + 1], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid native-weights limit: %s\n", argv[i + 1]);
                return 2;
            }
        } else if (strcmp(argv[i], "--tensor") == 0) {
            tensor_name = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown native-weights option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }

    if (!options.source_dir) {
        fprintf(stderr, "yvex: native-weights requires --source DIR\n");
        return 2;
    }

    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_native_weight_table_summary(table, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_native_weight_table_close(table);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (json) {
        printf("{\n");
        printf("  \"schema\": \"yvex.native_weights.v1\",\n");
        printf("  \"source\": \"%s\",\n", options.source_dir);
        printf("  \"summary\": {\n");
        printf("    \"shard_count\": %llu,\n", summary.shard_count);
        printf("    \"tensor_count\": %llu,\n", summary.tensor_count);
        printf("    \"total_tensor_bytes\": %llu,\n", summary.total_tensor_bytes);
        printf("    \"unknown_dtype_count\": %llu,\n", summary.unknown_dtype_count);
        printf("    \"malformed_shard_count\": %llu\n", summary.malformed_shard_count);
        printf("  }\n");
        printf("}\n");
        yvex_native_weight_table_close(table);
        return 0;
    }

    printf("native weights: safetensors\n");
    printf("source: %s\n", options.source_dir);
    printf("shards: %llu\n", summary.shard_count);
    printf("tensors: %llu\n", summary.tensor_count);
    printf("total_tensor_bytes: %llu\n", summary.total_tensor_bytes);
    printf("unknown_dtype_count: %llu\n", summary.unknown_dtype_count);
    printf("malformed_shard_count: %llu\n", summary.malformed_shard_count);
    printf("\n");

    if (tensor_name) {
        const yvex_native_weight_info *row = yvex_native_weight_table_find(table, tensor_name);
        if (!row) {
            yvex_native_weight_table_close(table);
            fprintf(stderr, "yvex: native tensor not found: %s\n", tensor_name);
            return 4;
        }
        printf("0 %s shard=%s dtype=%s rank=%u shape=",
               row->name, row->shard_path, yvex_native_dtype_name(row->dtype), row->rank);
        print_native_dims(row->dims, row->rank);
        printf(" bytes=%llu offsets=[%llu,%llu]\n", row->data_bytes, row->data_start, row->data_end);
    } else {
        unsigned long long count = yvex_native_weight_table_count(table);
        unsigned long long n = limit < count ? limit : count;
        unsigned long long idx;
        for (idx = 0; idx < n; ++idx) {
            const yvex_native_weight_info *row = yvex_native_weight_table_at(table, idx);
            printf("%llu %s shard=%s dtype=%s rank=%u shape=",
                   idx, row->name, row->shard_path, yvex_native_dtype_name(row->dtype), row->rank);
            print_native_dims(row->dims, row->rank);
            printf(" bytes=%llu offsets=[%llu,%llu]\n", row->data_bytes, row->data_start, row->data_end);
        }
    }
    printf("status: %s\n", summary.shard_count == 0 ? "native-weights-empty" : "native-weights");
    yvex_native_weight_table_close(table);
    return 0;
}

static int command_tensor_map(int argc, char **argv)
{
    yvex_weight_mapping_options options;
    yvex_weight_mapping_table *table = NULL;
    yvex_error err;
    const char *tensor_name = NULL;
    unsigned long long limit = 20;
    unsigned long long mapped = 0;
    unsigned long long unmapped = 0;
    unsigned long long shape_mismatch = 0;
    int json = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("tensor-map"));
        return 0;
    }

    i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            i++;
            continue;
        }
        if (strcmp(argv[i], "--require-all-native-mapped") == 0) {
            options.require_all_native_mapped = 1;
            i++;
            continue;
        }
        if (strcmp(argv[i], "--require-all-template-matched") == 0) {
            options.require_all_template_matched = 1;
            i++;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: tensor-map option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--arch") == 0) {
            options.architecture = argv[i + 1];
        } else if (strcmp(argv[i], "--native-source") == 0) {
            options.native_source_dir = argv[i + 1];
        } else if (strcmp(argv[i], "--template") == 0) {
            options.template_path = argv[i + 1];
            options.compare_template = 1;
        } else if (strcmp(argv[i], "--tensor") == 0) {
            tensor_name = argv[i + 1];
        } else if (strcmp(argv[i], "--limit") == 0) {
            char *end = NULL;
            limit = strtoull(argv[i + 1], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid tensor-map limit: %s\n", argv[i + 1]);
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: unknown tensor-map option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }

    if (!options.architecture || !options.native_source_dir) {
        fprintf(stderr, "yvex: tensor-map requires --arch NAME and --native-source DIR\n");
        return 2;
    }

    rc = yvex_weight_mapping_table_build(&table, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    for (i = 0; (unsigned long long)i < yvex_weight_mapping_table_count(table); ++i) {
        const yvex_weight_mapping_info *row = yvex_weight_mapping_table_at(table, (unsigned long long)i);
        if (!row) continue;
        if (row->status == YVEX_WEIGHT_MAPPING_STATUS_MAPPED) mapped++;
        else if (row->status == YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH) shape_mismatch++;
        else unmapped++;
    }

    if (json) {
        printf("{\n");
        printf("  \"schema\": \"yvex.tensor_map.v1\",\n");
        printf("  \"architecture\": \"%s\",\n", options.architecture);
        printf("  \"native_source\": \"%s\",\n", options.native_source_dir);
        printf("  \"native_tensors\": %llu,\n", yvex_weight_mapping_table_count(table));
        printf("  \"mapped\": %llu,\n", mapped);
        printf("  \"unmapped\": %llu,\n", unmapped);
        printf("  \"shape_mismatch\": %llu\n", shape_mismatch);
        printf("}\n");
        yvex_weight_mapping_table_close(table);
        return 0;
    }

    printf("tensor map: %s\n", options.architecture);
    printf("native_source: %s\n", options.native_source_dir);
    if (options.template_path) {
        printf("template: %s\n", options.template_path);
    }
    printf("native_tensors: %llu\n", yvex_weight_mapping_table_count(table));
    printf("mapped: %llu\n", mapped);
    printf("unmapped: %llu\n", unmapped);
    printf("shape_mismatch: %llu\n", shape_mismatch);
    printf("\n");

    if (tensor_name) {
        const yvex_weight_mapping_info *row = yvex_weight_mapping_table_find_native(table, tensor_name);
        if (!row) {
            yvex_weight_mapping_table_close(table);
            fprintf(stderr, "yvex: native tensor not found: %s\n", tensor_name);
            return 4;
        }
        printf("0 native=%s role=%s target=%s status=%s native_shape=",
               row->native_name, yvex_tensor_role_name(row->role), row->target_name,
               yvex_weight_mapping_status_name(row->status));
        print_native_dims(row->native_dims, row->native_rank);
        printf(" target_shape=");
        if (row->target_rank > 0) print_native_dims(row->target_dims, row->target_rank);
        else printf("unknown");
        printf(" transform=%s", row->requires_transpose ? "transpose" : "none");
        if (row->issue != YVEX_WEIGHT_MAPPING_ISSUE_NONE) {
            printf(" issue=%s", yvex_weight_mapping_issue_kind_name(row->issue));
        }
        printf("\n");
    } else {
        unsigned long long count = yvex_weight_mapping_table_count(table);
        unsigned long long n = limit < count ? limit : count;
        unsigned long long idx;

        for (idx = 0; idx < n; ++idx) {
            const yvex_weight_mapping_info *row = yvex_weight_mapping_table_at(table, idx);
            printf("%llu native=%s role=%s target=%s status=%s native_shape=",
                   idx, row->native_name, yvex_tensor_role_name(row->role), row->target_name,
                   yvex_weight_mapping_status_name(row->status));
            print_native_dims(row->native_dims, row->native_rank);
            printf(" target_shape=");
            if (row->target_rank > 0) print_native_dims(row->target_dims, row->target_rank);
            else printf("unknown");
            printf(" transform=%s", row->requires_transpose ? "transpose" : "none");
            if (row->issue != YVEX_WEIGHT_MAPPING_ISSUE_NONE) {
                printf(" issue=%s", yvex_weight_mapping_issue_kind_name(row->issue));
            }
            printf("\n");
        }
    }
    printf("status: tensor-map\n");
    yvex_weight_mapping_table_close(table);
    return 0;
}

static void print_quant_policy_rules(const yvex_quant_policy *policy)
{
    unsigned long long i;

    for (i = 0; i < yvex_quant_policy_rule_count(policy); ++i) {
        const yvex_quant_policy_rule *rule = yvex_quant_policy_rule_at(policy, i);
        if (!rule) continue;
        printf("%llu selector=%s:%s qtype=%s storage_supported=%s compute_supported=%s requires_imatrix=%s\n",
               i,
               yvex_quant_selector_kind_name(rule->selector_kind),
               rule->selector,
               yvex_quant_qtype_name(rule->qtype),
               rule->storage_supported ? "yes" : "no",
               rule->compute_supported ? "yes" : "no",
               rule->requires_imatrix ? "yes" : "no");
    }
}

static int parse_quant_policy_common(int argc, char **argv, int start,
                                     const char **policy_path,
                                     const char **template_path,
                                     const char **arch,
                                     const char **out_path)
{
    int i = start;

    while (i < argc) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: quant-policy option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--policy") == 0) {
            *policy_path = argv[i + 1];
        } else if (strcmp(argv[i], "--template") == 0) {
            *template_path = argv[i + 1];
        } else if (strcmp(argv[i], "--arch") == 0) {
            *arch = argv[i + 1];
        } else if (strcmp(argv[i], "--out") == 0) {
            *out_path = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown quant-policy option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static int command_quant_policy(int argc, char **argv)
{
    const char *policy_path = NULL;
    const char *template_path = NULL;
    const char *arch = NULL;
    const char *out_path = NULL;
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("quant-policy"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: quant-policy requires inspect, validate, or derive\n");
        return 2;
    }
    rc = parse_quant_policy_common(argc, argv, 3, &policy_path, &template_path, &arch, &out_path);
    if (rc != 0) return rc;

    if (strcmp(argv[2], "derive") == 0) {
        if (!template_path || !arch || !out_path) {
            fprintf(stderr, "yvex: quant-policy derive requires --template FILE --arch NAME --out FILE\n");
            return 2;
        }
        rc = yvex_quant_policy_create_from_template(&policy, template_path, arch, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = yvex_quant_policy_write_json(out_path, policy, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = yvex_quant_policy_get_summary(policy, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("quant policy: derived\n");
        printf("architecture: %s\n", summary.architecture);
        printf("template: %s\n", template_path);
        printf("rules: %llu\n", summary.rule_count);
        printf("requires_imatrix: %llu\n", summary.requires_imatrix_count);
        printf("out: %s\n", out_path);
        printf("status: quant-policy-written\n");
        yvex_quant_policy_close(policy);
        return 0;
    }

    if (strcmp(argv[2], "inspect") == 0 || strcmp(argv[2], "validate") == 0) {
        if (!policy_path) {
            fprintf(stderr, "yvex: quant-policy %s requires --policy FILE\n", argv[2]);
            return 2;
        }
        rc = yvex_quant_policy_open(&policy, policy_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (strcmp(argv[2], "validate") == 0) {
            rc = yvex_quant_policy_validate(policy, template_path, &err);
            if (rc != YVEX_OK) {
                yvex_quant_policy_close(policy);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
        rc = yvex_quant_policy_get_summary(policy, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("quant policy: %s\n", argv[2]);
        printf("policy: %s\n", policy_path);
        if (template_path) printf("template: %s\n", template_path);
        printf("name: %s\n", summary.name);
        printf("architecture: %s\n", summary.architecture);
        printf("rules: %llu\n", summary.rule_count);
        printf("issues: %llu\n", summary.issue_count);
        printf("requires_imatrix: %llu\n", summary.requires_imatrix_count);
        printf("storage_supported: %llu\n", summary.storage_supported_count);
        printf("compute_supported: %llu\n", summary.compute_supported_count);
        printf("\n");
        if (strcmp(argv[2], "inspect") == 0) {
            print_quant_policy_rules(policy);
        }
        printf("status: %s\n", yvex_quant_policy_status_name(summary.status));
        yvex_quant_policy_close(policy);
        return 0;
    }

    fprintf(stderr, "yvex: unknown quant-policy subcommand: %s\n", argv[2]);
    return 2;
}

static int parse_quant_job_create_options(int argc, char **argv,
                                          yvex_quant_job_options *options,
                                          const char **out_path)
{
    int i;

    memset(options, 0, sizeof(*options));
    *out_path = NULL;
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: quant-job option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--name") == 0) options->name = argv[++i];
        else if (strcmp(argv[i], "--arch") == 0) options->architecture = argv[++i];
        else if (strcmp(argv[i], "--tool") == 0) options->tool = yvex_quant_job_tool_from_name(argv[++i]);
        else if (strcmp(argv[i], "--tool-path") == 0) options->tool_path = argv[++i];
        else if (strcmp(argv[i], "--source-manifest") == 0) options->source_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--native-source") == 0) options->native_source_dir = argv[++i];
        else if (strcmp(argv[i], "--template") == 0) options->template_path = argv[++i];
        else if (strcmp(argv[i], "--quant-policy") == 0) options->quant_policy_path = argv[++i];
        else if (strcmp(argv[i], "--imatrix-manifest") == 0) options->imatrix_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--imatrix") == 0) options->imatrix_path = argv[++i];
        else if (strcmp(argv[i], "--out-gguf") == 0) options->out_gguf_path = argv[++i];
        else if (strcmp(argv[i], "--log") == 0) options->log_path = argv[++i];
        else if (strcmp(argv[i], "--status") == 0) options->status = yvex_quant_job_status_from_name(argv[++i]);
        else if (strcmp(argv[i], "--command") == 0) options->command = argv[++i];
        else if (strcmp(argv[i], "--out") == 0) *out_path = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown quant-job option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_quant_job_manifest_option(int argc, char **argv, const char **manifest_path)
{
    int i;

    *manifest_path = NULL;
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: quant-job option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--manifest") == 0) *manifest_path = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown quant-job option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static void print_quant_job_summary(const char *mode,
                                    const char *path,
                                    const yvex_quant_job_summary *summary)
{
    printf("quant job: %s\n", mode);
    if (path) printf("manifest: %s\n", path);
    printf("name: %s\n", summary->name ? summary->name : "");
    printf("architecture: %s\n", summary->architecture ? summary->architecture : "");
    printf("tool: %s\n", yvex_quant_job_tool_name(summary->tool));
    printf("tool_path: %s\n", summary->tool_path ? summary->tool_path : "");
    printf("native_source: %s\n", summary->native_source_dir ? summary->native_source_dir : "");
    printf("template: %s\n", summary->template_path ? summary->template_path : "");
    printf("out_gguf: %s\n", summary->out_gguf_path ? summary->out_gguf_path : "");
    printf("log: %s\n", summary->log_path ? summary->log_path : "");
    printf("tool_exists: %s\n", summary->tool_exists ? "yes" : "no");
    printf("source_exists: %s\n", summary->source_exists ? "yes" : "no");
    printf("template_exists: %s\n", summary->template_exists ? "yes" : "no");
    printf("imatrix_exists: %s\n", summary->imatrix_exists ? "yes" : "no");
    printf("output_exists: %s\n", summary->output_exists ? "yes" : "no");
    printf("status: %s\n", yvex_quant_job_status_name(summary->status));
}

static int command_quant_job(int argc, char **argv)
{
    yvex_quant_job_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&summary, 0, sizeof(summary));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("quant-job"));
        return 0;
    }
    if (argc < 3 || (strcmp(argv[2], "create") != 0 &&
                     strcmp(argv[2], "inspect") != 0 &&
                     strcmp(argv[2], "validate") != 0)) {
        fprintf(stderr, "yvex: quant-job requires create, inspect, or validate\n");
        return 2;
    }

    if (strcmp(argv[2], "create") == 0) {
        yvex_quant_job_options options;
        const char *out_path = NULL;

        rc = parse_quant_job_create_options(argc, argv, &options, &out_path);
        if (rc != 0) return rc;
        if (!options.name || !options.architecture || !options.tool_path ||
            !options.native_source_dir || !options.template_path ||
            !options.out_gguf_path || !options.log_path || !options.command ||
            !out_path || options.status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
            fprintf(stderr, "yvex: quant-job create requires --name --arch --tool --tool-path --native-source --template --out-gguf --log --status --command --out\n");
            return 2;
        }
        rc = yvex_quant_job_write_json(out_path, &options, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        printf("quant job: written\n");
        printf("name: %s\n", summary.name);
        printf("architecture: %s\n", summary.architecture);
        printf("tool: %s\n", yvex_quant_job_tool_name(summary.tool));
        printf("tool_exists: %s\n", summary.tool_exists ? "yes" : "no");
        printf("source_exists: %s\n", summary.source_exists ? "yes" : "no");
        printf("template_exists: %s\n", summary.template_exists ? "yes" : "no");
        printf("imatrix_exists: %s\n", summary.imatrix_exists ? "yes" : "no");
        printf("output_exists: %s\n", summary.output_exists ? "yes" : "no");
        printf("status: %s\n", yvex_quant_job_status_name(summary.status));
        printf("out: %s\n", out_path);
        printf("status: quant-job-written\n");
        return 0;
    }

    {
        const char *manifest_path = NULL;

        rc = parse_quant_job_manifest_option(argc, argv, &manifest_path);
        if (rc != 0) return rc;
        if (!manifest_path) {
            fprintf(stderr, "yvex: quant-job %s requires --manifest FILE\n", argv[2]);
            return 2;
        }
        rc = yvex_quant_job_validate(manifest_path, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        print_quant_job_summary(argv[2], manifest_path, &summary);
        printf("status: quant-job-%s\n", strcmp(argv[2], "validate") == 0 ? "valid" : "manifest");
        return 0;
    }
}

static int command_source_manifest(int argc, char **argv)
{
    yvex_source_manifest_options options;
    yvex_source_manifest_summary summary;
    yvex_error err;
    const char *out_path = NULL;
    int i;
    int rc;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("source-manifest"));
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "yvex: source-manifest requires a subcommand\n");
        fprintf(stderr, "usage: yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE\n");
        return 2;
    }

    if (strcmp(argv[2], "inspect") == 0) {
        fprintf(stderr, "yvex: source-manifest inspect is not implemented in open-weight intake\n");
        return 5;
    }
    if (strcmp(argv[2], "create") != 0) {
        fprintf(stderr, "yvex: unknown source-manifest subcommand: %s\n", argv[2]);
        return 2;
    }

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
            fprintf(stderr, "yvex: option requires a value: %s\n", name);
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
            if (!parse_source_status(value, &options.status)) {
                fprintf(stderr, "yvex: unknown source status: %s\n", value);
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
            fprintf(stderr, "yvex: unknown source-manifest option: %s\n", name);
            return 2;
        }
        i += 2;
    }

    if (!options.repo || !options.revision || !options.local_path || !out_path) {
        fprintf(stderr, "yvex: --hf-repo, --revision, --local-path, and --out are required\n");
        return 2;
    }

    rc = yvex_source_manifest_write_json(out_path, &options, &summary, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("source manifest: written\n");
    printf("repo: %s\n", options.repo);
    printf("revision: %s\n", options.revision);
    printf("local_path: %s\n", options.local_path);
    printf("status: %s\n", yvex_source_status_name(options.status));
    printf("files: %llu\n", summary.file_count);
    printf("safetensors: %llu\n", summary.safetensors_count);
    printf("total_size_bytes: %llu\n", summary.total_size_bytes);
    printf("out: %s\n", out_path);
    printf("status: source-manifest-written\n");
    return 0;
}

int yvex_cli_command_convert(int argc, char **argv)
{
    return command_convert(argc, argv);
}

int yvex_cli_command_gguf_template(int argc, char **argv)
{
    return command_gguf_template(argc, argv);
}

int yvex_cli_command_gguf_emit(int argc, char **argv)
{
    return command_gguf_emit(argc, argv);
}

int yvex_cli_command_imatrix(int argc, char **argv)
{
    return command_imatrix(argc, argv);
}

int yvex_cli_command_native_weights(int argc, char **argv)
{
    return command_native_weights(argc, argv);
}

int yvex_cli_command_tensor_map(int argc, char **argv)
{
    return command_tensor_map(argc, argv);
}

int yvex_cli_command_quant_job(int argc, char **argv)
{
    return command_quant_job(argc, argv);
}

int yvex_cli_command_quant_policy(int argc, char **argv)
{
    return command_quant_policy(argc, argv);
}

int yvex_cli_command_qtype_support(int argc, char **argv)
{
    return command_qtype_support(argc, argv);
}

int yvex_cli_command_source_manifest(int argc, char **argv)
{
    return command_source_manifest(argc, argv);
}
