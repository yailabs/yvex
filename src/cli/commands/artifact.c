/*
 * artifact.c - artifact command surface.
 *
 * Owner: CLI artifact command.
 * Owns: artifact argv validation, dispatch, help, and compatibility rendering.
 * Does not own: artifact file lifecycle, identity, integrity truth, or materialization.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes typed artifact APIs and returns process exit status.
 */
#include "src/core/operator.h"
#include "src/cli/io/out.h"
#include <yvex/api.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


static int command_integrity_report(int arg_count, char **args);

static void print_integrity_report(const yvex_artifact_integrity_report *report,
                                   const char *model_label)
{
    unsigned int i;

    yvex_cli_out_writef(stdout, "artifact_integrity: check\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_label ? model_label : report->path);
    yvex_cli_out_writef(stdout, "format: %s\n", report->format[0] ? report->format : "unknown");
    if (report->version) {
        yvex_cli_out_writef(stdout, "version: %u\n", report->version);
    }
    yvex_cli_out_writef(stdout, "file_size: %llu\n", report->file_size);
    if (report->architecture[0]) {
        yvex_cli_out_writef(stdout, "architecture: %s\n", report->architecture);
    }
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", report->tensor_count);
    yvex_cli_out_writef(stdout, "known_tensor_bytes: %llu\n", report->known_tensor_bytes);
    yvex_cli_out_writef(stdout, "tensor_ranges_checked: %llu\n", report->tensor_ranges_checked);
    yvex_cli_out_writef(stdout, "tensor_ranges_valid: %llu\n", report->tensor_ranges_valid);
    yvex_cli_out_writef(stdout, "tensor_ranges_invalid: %llu\n", report->tensor_ranges_invalid);
    yvex_cli_out_writef(stdout, "tensor_shapes_checked: %llu\n", report->tensor_shapes_checked);
    yvex_cli_out_writef(stdout, "tensor_shapes_valid: %llu\n", report->tensor_shapes_valid);
    yvex_cli_out_writef(stdout, "tensor_shapes_invalid: %llu\n", report->tensor_shapes_invalid);
    yvex_cli_out_writef(stdout, "tensor_dtypes_checked: %llu\n", report->tensor_dtypes_checked);
    yvex_cli_out_writef(stdout, "tensor_dtypes_valid: %llu\n", report->tensor_dtypes_valid);
    yvex_cli_out_writef(stdout, "tensor_dtypes_invalid: %llu\n", report->tensor_dtypes_invalid);
    yvex_cli_out_writef(stdout, "tensor_byte_counts_checked: %llu\n", report->tensor_byte_counts_checked);
    yvex_cli_out_writef(stdout, "tensor_byte_counts_invalid: %llu\n", report->tensor_byte_counts_invalid);
    if (report->selected_embedding_shape[0]) {
        yvex_cli_out_writef(stdout, "selected_embedding_shape: %s\n", report->selected_embedding_shape);
        yvex_cli_out_writef(stdout, "selected_embedding_hidden_size: %llu\n", report->selected_embedding_hidden_size);
        yvex_cli_out_writef(stdout, "selected_embedding_vocab_size: %llu\n", report->selected_embedding_vocab_size);
        yvex_cli_out_writef(stdout, "selected_embedding_output_count: %llu\n", report->selected_embedding_output_count);
        yvex_cli_out_writef(stdout, "selected_embedding_output_bytes: %llu\n", report->selected_embedding_output_bytes);
        yvex_cli_out_writef(stdout, "selected_embedding_slice_bytes: %llu\n", report->selected_embedding_slice_bytes);
    }
    yvex_cli_out_writef(stdout, "identity_checked: %s\n", report->identity_checked ? "true" : "false");
    yvex_cli_out_writef(stdout, "sha256: %s\n", report->sha256[0] ? report->sha256 : "unavailable");
    yvex_cli_out_writef(stdout, "registered_sha256: %s\n", report->registered_sha256[0] ? report->registered_sha256 : "absent");
    if (report->expected_sha256[0]) {
        yvex_cli_out_writef(stdout, "expected_sha256: %s\n", report->expected_sha256);
        yvex_cli_out_writef(stdout, "actual_sha256: %s\n", report->sha256[0] ? report->sha256 : "unavailable");
    }
    yvex_cli_out_writef(stdout, "digest_status: %s\n", report->digest_status[0] ? report->digest_status : "unknown");
    yvex_cli_out_writef(stdout, "integrity_status: %s\n", report->passed ? "pass" : "fail");
    yvex_cli_out_writef(stdout, "integrity_errors: %u\n", report->error_count);
    yvex_cli_out_writef(stdout, "integrity_warnings: %u\n", report->warning_count);

    for (i = 0; i < report->issue_count; ++i) {
        const yvex_integrity_issue *issue = yvex_artifact_integrity_issue_at(report, i);
        const char *prefix;

        if (!issue) {
            continue;
        }
        prefix = issue->severity == YVEX_INTEGRITY_SEVERITY_WARNING ? "warning" : "error";
        yvex_cli_out_writef(stdout, "%s_%u_code: %s\n", prefix, i, issue->code);
        if (issue->tensor[0]) {
            yvex_cli_out_writef(stdout, "%s_%u_tensor: %s\n", prefix, i, issue->tensor);
        }
        if (issue->has_range) {
            yvex_cli_out_writef(stdout, "%s_%u_relative_offset: %llu\n", prefix, i, issue->relative_offset);
            yvex_cli_out_writef(stdout, "%s_%u_absolute_offset: %llu\n", prefix, i, issue->absolute_offset);
            yvex_cli_out_writef(stdout, "%s_%u_tensor_bytes: %llu\n", prefix, i, issue->tensor_bytes);
            yvex_cli_out_writef(stdout, "%s_%u_file_size: %llu\n", prefix, i, issue->file_size);
        }
        yvex_cli_out_writef(stdout, "%s_%u_reason: %s\n", prefix, i, issue->reason);
    }

    yvex_cli_out_writef(stdout, "status: %s\n", report->passed ? "artifact-integrity-pass"
                                          : "artifact-integrity-fail");
}

static void print_metadata_value(const yvex_gguf_value *value)
{
    unsigned long long u64;
    long long i64;
    double f64;
    int bool_value;
    const char *string_data;
    unsigned long long string_len;
    yvex_gguf_array_info array;

    switch (yvex_gguf_value_type_of(value)) {
    case YVEX_GGUF_VALUE_UINT8:
    case YVEX_GGUF_VALUE_UINT16:
    case YVEX_GGUF_VALUE_UINT32:
    case YVEX_GGUF_VALUE_UINT64:
        if (yvex_gguf_value_as_u64(value, &u64) == YVEX_OK) {
            yvex_cli_out_writef(stdout, "%llu", u64);
        }
        break;
    case YVEX_GGUF_VALUE_INT8:
    case YVEX_GGUF_VALUE_INT16:
    case YVEX_GGUF_VALUE_INT32:
    case YVEX_GGUF_VALUE_INT64:
        if (yvex_gguf_value_as_i64(value, &i64) == YVEX_OK) {
            yvex_cli_out_writef(stdout, "%lld", i64);
        }
        break;
    case YVEX_GGUF_VALUE_FLOAT32:
    case YVEX_GGUF_VALUE_FLOAT64:
        if (yvex_gguf_value_as_f64(value, &f64) == YVEX_OK) {
            yvex_cli_out_writef(stdout, "%g", f64);
        }
        break;
    case YVEX_GGUF_VALUE_BOOL:
        if (yvex_gguf_value_as_bool(value, &bool_value) == YVEX_OK) {
            yvex_cli_out_writef(stdout, "%s", bool_value ? "true" : "false");
        }
        break;
    case YVEX_GGUF_VALUE_STRING:
        if (yvex_gguf_value_as_string(value, &string_data, &string_len) == YVEX_OK) {
            print_quoted_bytes(string_data, string_len);
        }
        break;
    case YVEX_GGUF_VALUE_ARRAY:
        if (yvex_gguf_value_array_info(value, &array) == YVEX_OK) {
            yvex_cli_out_writef(stdout, "array<%s>[%llu]", yvex_gguf_value_type_name(array.element_type), array.count);
        }
        break;
    case YVEX_GGUF_VALUE_INVALID:
        yvex_cli_out_writef(stdout, "%s", "<invalid>");
        break;
    }
}

/*
 * command_integrity()
 *
 * Purpose:
 *   parse and execute artifact integrity check/report CLI requests.
 *
 * Inputs:
 *   arg_count/args are borrowed CLI arguments.
 *
 * Effects:
 *   resolves model references, reads artifact metadata, prints integrity
 *   reports, and clears model-ref state.
 *
 * Failure:
 *   returns parser failures for invalid options and propagated integrity/model
 *   reference failures for invalid artifacts.
 *
 * Boundary:
 *   integrity reports are artifact evidence only and not runtime support,
 *   generation support, eval evidence, benchmark evidence, or release
 *   readiness.
 */
static int command_integrity(int arg_count, char **args)
{
    yvex_artifact_integrity_options options;
    yvex_artifact_integrity_report report;
    yvex_model_ref ref;
    yvex_error err;
    const char *model_arg = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&report, 0, sizeof(report));
    memset(&ref, 0, sizeof(ref));

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_integrity_help(stdout);
        return 0;
    }
    if (arg_count >= 3 && strcmp(args[2], "report") == 0) {
        return command_integrity_report(arg_count, args);
    }
    if (arg_count < 3 || strcmp(args[2], "check") != 0) {
        yvex_cli_out_writef(stderr, "yvex: integrity requires check or report\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N] | yvex integrity report --model FILE_OR_ALIAS [--backend cpu|cuda] [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N]\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = args[++i];
        } else if (strcmp(args[i], "--expect-sha256") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --expect-sha256 requires HASH\n");
                return 2;
            }
            options.expect_sha256 = args[++i];
        } else if (strcmp(args[i], "--require-token-embedding") == 0) {
            options.require_token_embedding = 1;
        } else if (strcmp(args[i], "--partial-token") == 0) {
            if (i + 1 >= arg_count || !parse_uint_allow_zero(args[i + 1], &options.token_id)) {
                yvex_cli_out_writef(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            options.require_token_embedding = 1;
            i += 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown integrity option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help integrity' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        yvex_cli_out_writef(stderr, "yvex: integrity check requires --model FILE_OR_ALIAS\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (ref.kind == YVEX_MODEL_REF_ALIAS && ref.sha256 && ref.sha256[0]) {
        options.registered_sha256 = ref.sha256;
    }

    rc = yvex_artifact_integrity_check_path(ref.path, &options, &report, &err);
    print_integrity_report(&report, ref.path);
    yvex_model_ref_clear(&ref);
    if (rc != YVEX_OK) {
        return exit_for_status(rc);
    }
    return 0;
}

/*
 * command_inspect()
 *
 * Purpose:
 *   parse and execute descriptor-only artifact inspection.
 *
 * Inputs:
 *   arg_count/args are borrowed CLI arguments naming a file or model alias.
 *
 * Effects:
 *   opens artifact/GGUF/model/tensor metadata, prints descriptor facts, and
 *   closes all owned state before return.
 *
 * Failure:
 *   returns parser failures for invalid usage and propagated artifact/GGUF/model
 *   metadata errors for malformed inputs.
 *
 * Boundary:
 *   inspection is metadata reporting only and does not materialize full weights,
 *   execute graph work, generate, evaluate, benchmark, or mark release ready.
 */
static int command_inspect(int arg_count, char **args)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_model_descriptor *model = NULL;
    yvex_gguf_probe probe;
    const yvex_gguf_header *header;
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));

    if (arg_count != 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        if (arg_count == 3) {
            yvex_inspect_help(stdout);
            return 0;
        }
        yvex_cli_out_writef(stderr, "yvex: inspect requires exactly one FILE_OR_ALIAS\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex inspect FILE_OR_ALIAS\n");
        return 2;
    }

    rc = open_artifact_for_gguf(args[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_probe_file(artifact, &probe, &err);
    if (rc == YVEX_OK && !probe.is_gguf) {
        yvex_cli_out_writef(stdout, "format: unknown\n");
        yvex_cli_out_writef(stdout, "status: unsupported\n");
        yvex_artifact_close(artifact);
        return 5;
    }

    if (rc != YVEX_OK) {
        if (rc == YVEX_ERR_UNSUPPORTED) {
            yvex_cli_out_writef(stdout, "format: gguf\n");
            yvex_cli_out_writef(stdout, "status: unsupported\n");
        }
        yvex_artifact_close(artifact);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&model, gguf, tensors, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(artifact,
                                              gguf,
                                              tensors,
                                              &integrity_options,
                                              &integrity_report,
                                              &err);
    }
    if (rc == YVEX_OK) {
        header = yvex_gguf_header_view(gguf);
        yvex_cli_out_writef(stdout, "format: gguf\n");
        yvex_cli_out_writef(stdout, "version: %u\n", header->version);
        yvex_cli_out_writef(stdout, "metadata_count: %llu\n", header->metadata_count);
        yvex_cli_out_writef(stdout, "tensor_count: %llu\n", header->tensor_count);
        yvex_cli_out_writef(stdout, "tensor_data_offset: %llu\n", yvex_gguf_tensor_data_offset(gguf));
        yvex_cli_out_writef(stdout, "alignment: %u\n", yvex_gguf_alignment(gguf));
        yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(yvex_model_arch(model)));
        yvex_cli_out_writef(stdout, "model_name: %s\n", yvex_model_name(model));
        yvex_cli_out_writef(stdout, "known_tensor_bytes: %llu\n", yvex_model_total_storage_bytes(model));
        yvex_cli_out_writef(stdout, "unsupported_tensor_accounting: %llu\n",
               yvex_model_unsupported_tensor_accounting_count(model));
        yvex_cli_out_writef(stdout, "status: descriptor-only\n");
        yvex_model_descriptor_close(model);
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return 0;
    }

    yvex_model_descriptor_close(model);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return print_yvex_error(&err, exit_for_status(rc));
}

static int command_metadata(int arg_count, char **args)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    const yvex_gguf_header *header;
    yvex_error err;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);

    if (arg_count != 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        if (arg_count == 3) {
            yvex_metadata_help(stdout);
            return 0;
        }
        yvex_cli_out_writef(stderr, "yvex: metadata requires exactly one FILE_OR_ALIAS\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex metadata FILE_OR_ALIAS\n");
        return 2;
    }

    rc = open_artifact_for_gguf(args[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc != YVEX_OK) {
        yvex_artifact_close(artifact);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    header = yvex_gguf_header_view(gguf);
    yvex_cli_out_writef(stdout, "format: gguf\n");
    yvex_cli_out_writef(stdout, "version: %u\n", header->version);
    yvex_cli_out_writef(stdout, "metadata_count: %llu\n", yvex_gguf_metadata_count(gguf));
    yvex_cli_out_writef(stdout, "\n");

    for (i = 0; i < yvex_gguf_metadata_count(gguf); ++i) {
        const char *key = yvex_gguf_metadata_key(gguf, i);
        const yvex_gguf_value *value = yvex_gguf_metadata_value(gguf, i);
        yvex_cli_out_writef(stdout, "%s = ", key ? key : "");
        print_metadata_value(value);
        yvex_cli_out_writef(stdout, "\n");
    }

    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

int enforce_registered_identity_cli(const yvex_model_ref *ref, const char *surface)
{
    yvex_artifact_file_identity identity;
    yvex_model_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_error err;
    const char *identity_status = "pass";
    const char *digest_status = "pass";
    const char *reason = "current file identity matches registered alias";
    int pass = 1;
    int rc;

    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) {
        return YVEX_OK;
    }

    yvex_error_clear(&err);
    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(ref->path, &identity, &err);
    if (rc != YVEX_OK) {
        identity_status = "fail";
        digest_status = "fail";
        reason = yvex_error_message(&err);
        pass = 0;
    } else if (!ref->sha256 || !ref->sha256[0] || !yvex_sha256_hex_is_valid(ref->sha256)) {
        identity_status = "missing";
        digest_status = "missing";
        reason = "registered alias lacks digest identity; re-add model";
        pass = 0;
        rc = YVEX_ERR_STATE;
    } else if (strcmp(ref->sha256, identity.sha256) != 0 ||
               (ref->registered_file_size != 0ull &&
                ref->registered_file_size != identity.file_size)) {
        identity_status = "fail";
        digest_status = "fail";
        reason = "digest mismatch for registered alias";
        pass = 0;
        rc = YVEX_ERR_STATE;
    } else {
        rc = YVEX_OK;
    }

    if (pass) {
        memset(&current_metadata, 0, sizeof(current_metadata));
        memset(&registered_metadata, 0, sizeof(registered_metadata));
        memset(&metadata_report, 0, sizeof(metadata_report));
        rc = yvex_model_metadata_snapshot_read(&current_metadata, ref->path, &err);
        if (rc != YVEX_OK) {
            yvex_cli_out_writef(stdout, "artifact_identity: check\n");
            yvex_cli_out_writef(stdout, "surface: %s\n", surface ? surface : "unknown");
            yvex_cli_out_writef(stdout, "alias: %s\n", ref->alias ? ref->alias : "");
            yvex_cli_out_writef(stdout, "path: %s\n", ref->path ? ref->path : "");
            yvex_cli_out_writef(stdout, "registered_sha256: %s\n", ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
            yvex_cli_out_writef(stdout, "current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
            yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", ref->registered_file_size);
            yvex_cli_out_writef(stdout, "current_file_size: %llu\n", identity.file_size);
            yvex_cli_out_writef(stdout, "digest_status: %s\n", digest_status);
            yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status);
            yvex_cli_out_writef(stdout, "metadata_status: fail\n");
            yvex_cli_out_writef(stdout, "readiness_status: fail\n");
            yvex_cli_out_writef(stdout, "metadata_issue_0_code: current-metadata-unavailable\n");
            yvex_cli_out_writef(stdout, "metadata_issue_0_registered: available\n");
            yvex_cli_out_writef(stdout, "metadata_issue_0_current: %s\n", yvex_error_message(&err));
            yvex_cli_out_writef(stdout, "reason: current artifact metadata could not be parsed\n");
            yvex_cli_out_writef(stdout, "status: models-metadata-drift\n");
            return exit_for_status(YVEX_ERR_STATE);
        }
        yvex_model_ref_registry_entry_view(ref, &registered_metadata);
        rc = yvex_model_registry_compare_metadata(&registered_metadata,
                                                  &current_metadata.entry,
                                                  &metadata_report,
                                                  &err);
        if (rc != YVEX_OK ||
            strcmp(metadata_report.metadata_status, "pass") != 0 ||
            strcmp(metadata_report.readiness_status, "pass") != 0) {
            const char *status = strcmp(metadata_report.metadata_status, "missing") == 0
                                     ? "models-metadata-missing"
                                     : "models-metadata-drift";
            yvex_cli_out_writef(stdout, "artifact_identity: check\n");
            yvex_cli_out_writef(stdout, "surface: %s\n", surface ? surface : "unknown");
            yvex_cli_out_writef(stdout, "alias: %s\n", ref->alias ? ref->alias : "");
            yvex_cli_out_writef(stdout, "path: %s\n", ref->path ? ref->path : "");
            yvex_cli_out_writef(stdout, "registered_sha256: %s\n", ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
            yvex_cli_out_writef(stdout, "current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
            yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", ref->registered_file_size);
            yvex_cli_out_writef(stdout, "current_file_size: %llu\n", identity.file_size);
            yvex_cli_out_writef(stdout, "digest_status: %s\n", digest_status);
            yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status);
            print_metadata_drift_cli(&metadata_report);
            yvex_cli_out_writef(stdout, "reason: registered alias metadata does not match current artifact facts\n");
            yvex_cli_out_writef(stdout, "status: %s\n", status);
            return exit_for_status(YVEX_ERR_STATE);
        }
    }

    if (!pass) {
        yvex_cli_out_writef(stdout, "artifact_identity: check\n");
        yvex_cli_out_writef(stdout, "surface: %s\n", surface ? surface : "unknown");
        yvex_cli_out_writef(stdout, "alias: %s\n", ref->alias ? ref->alias : "");
        yvex_cli_out_writef(stdout, "path: %s\n", ref->path ? ref->path : "");
        yvex_cli_out_writef(stdout, "registered_sha256: %s\n", ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
        yvex_cli_out_writef(stdout, "current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
        yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", ref->registered_file_size);
        yvex_cli_out_writef(stdout, "current_file_size: %llu\n", identity.file_size);
        yvex_cli_out_writef(stdout, "digest_status: %s\n", digest_status);
        yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason);
        yvex_cli_out_writef(stdout, "status: %s\n", strcmp(identity_status, "missing") == 0
               ? "models-identity-missing"
               : "models-identity-fail");
    }
    return rc;
}

static void print_materialization_gate_fields(const char *gate,
                                              const char *phase,
                                              const char *integrity_status,
                                              const char *identity_status,
                                              const char *metadata_status,
                                              const char *shape_status,
                                              const char *range_status,
                                              const char *backend_status,
                                              int allocation_attempted,
                                              int transfer_attempted,
                                              int cleanup_attempted,
                                              const char *cleanup_status,
                                              unsigned long long bytes_planned,
                                              unsigned long long bytes_allocated,
                                              unsigned long long bytes_transferred)
{
    yvex_cli_out_writef(stdout, "materialization_gate: %s\n", gate ? gate : "fail");
    yvex_cli_out_writef(stdout, "materialization_phase: %s\n", phase ? phase : "preflight");
    yvex_cli_out_writef(stdout, "integrity_status: %s\n", integrity_status ? integrity_status : "unchecked");
    yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status ? identity_status : "unregistered");
    yvex_cli_out_writef(stdout, "metadata_status: %s\n", metadata_status ? metadata_status : "unregistered");
    yvex_cli_out_writef(stdout, "shape_status: %s\n", shape_status ? shape_status : "unchecked");
    yvex_cli_out_writef(stdout, "range_status: %s\n", range_status ? range_status : "unchecked");
    yvex_cli_out_writef(stdout, "backend_status: %s\n", backend_status ? backend_status : "not-opened");
    yvex_cli_out_writef(stdout, "allocation_attempted: %s\n", allocation_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "transfer_attempted: %s\n", transfer_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n", cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n", cleanup_status ? cleanup_status : "not-needed");
    yvex_cli_out_writef(stdout, "bytes_planned: %llu\n", bytes_planned);
    yvex_cli_out_writef(stdout, "bytes_allocated: %llu\n", bytes_allocated);
    yvex_cli_out_writef(stdout, "bytes_transferred: %llu\n", bytes_transferred);
}

/*
 * command_materialize()
 *
 * Purpose:
 *   parse and execute selected artifact materialization diagnostics.
 *
 * Inputs:
 *   arg_count/args are borrowed CLI arguments selecting a model and backend.
 *
 * Effects:
 *   resolves model references, opens artifact/metadata/backend state, transfers
 *   selected tensor bytes through materialization paths, prints summaries, and
 *   releases owned state.
 *
 * Failure:
 *   returns parser, model reference, artifact, metadata, backend, or
 *   materialization failures with cleanup through owned close paths.
 *
 * Boundary:
 *   materialization diagnostics are not graph execution, runtime generation,
 *   eval evidence, benchmark evidence, throughput, or release readiness.
 */
static int command_materialize(int arg_count, char **args)
{
    yvex_model_context ctx;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_materialize_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ctx, 0, sizeof(ctx));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));
    memset(&model_ref, 0, sizeof(model_ref));

    if (arg_count == 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_materialize_help(stdout);
        return 0;
    }

    for (i = 2; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_path = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu or cuda\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--require-all") == 0) {
            materialize_options.require_all_tensors = 1;
        } else if (strcmp(args[i], "--allow-unsupported-dtype") == 0) {
            materialize_options.allow_unsupported_dtype = 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown materialize option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help materialize' for usage.\n");
            return 2;
        }
    }

    if (!model_path || !backend_name) {
        yvex_cli_out_writef(stderr, "yvex: materialize requires --model FILE_OR_ALIAS and --backend cpu|cuda\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]\n");
        return 2;
    }

    if (strcmp(backend_name, "cpu") == 0) {
        backend_options.kind = YVEX_BACKEND_KIND_CPU;
    } else if (strcmp(backend_name, "cuda") == 0) {
        backend_options.kind = YVEX_BACKEND_KIND_CUDA;
    } else {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "materialize");
    if (rc != YVEX_OK) {
        print_materialization_gate_fields("fail", "preflight",
                                          "not-checked", "fail", "fail",
                                          "not-checked", "not-checked", "not-opened",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        yvex_cli_out_writef(stdout, "status: materialization-integrity-fail\n");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_model_context_open(model_ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        print_materialization_gate_fields("fail", "preflight",
                                          "fail",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          "unchecked", "unchecked", "not-opened",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        yvex_cli_out_writef(stdout, "status: materialization-integrity-fail\n");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        yvex_cli_out_writef(stdout, "materialization status: unsupported\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        print_materialization_gate_fields("fail", "preflight", "pass",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          "pass", "pass", "unavailable",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "status: weights-unsupported\n");
        yvex_model_context_close(&ctx);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_model_context_close(&ctx);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    materialize_options.backend_name = backend_name;
    rc = yvex_weight_table_materialize(&weights,
                                       ctx.artifact,
                                       ctx.gguf,
                                       ctx.table,
                                       backend,
                                       &materialize_options,
                                       &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        yvex_cli_out_writef(stdout, "materialization status: unsupported\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
        print_materialization_gate_fields("fail", "preflight", "pass",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          "fail", "fail", "ready",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "status: weights-unsupported\n");
        yvex_backend_close(backend);
        yvex_model_context_close(&ctx);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        if (getenv("YVEX_TEST_FAIL_MATERIALIZE_AFTER_TRANSFER")) {
            print_materialization_gate_fields("fail", "transfer", "pass",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              "pass", "pass", "ready",
                                              1, 1, 1, "pass", 0, 0, 0);
            yvex_cli_out_writef(stdout, "status: materialization-failed-cleaned\n");
        } else if (getenv("YVEX_TEST_FAIL_MATERIALIZE_AFTER_ALLOC")) {
            print_materialization_gate_fields("fail", "allocation", "pass",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              "pass", "pass", "ready",
                                              1, 0, 1, "pass", 0, 0, 0);
            yvex_cli_out_writef(stdout, "status: materialization-failed-cleaned\n");
        } else {
            print_materialization_gate_fields("fail", "preflight", "fail",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              "fail", "fail", "ready",
                                              0, 0, 0, "not-needed", 0, 0, 0);
            yvex_cli_out_writef(stdout, "status: materialization-integrity-fail\n");
        }
        yvex_backend_close(backend);
        yvex_model_context_close(&ctx);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_weight_table_get_summary(weights, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        yvex_model_context_close(&ctx);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_cli_out_writef(stdout, "materialization status: %s\n", yvex_weight_status_name(summary.status));
    yvex_cli_out_writef(stdout, "model: %s\n", yvex_model_name(ctx.model)[0] ? yvex_model_name(ctx.model) : "unknown");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    print_materialization_gate_fields(summary.materialization_gate,
                                      summary.materialization_phase,
                                      "pass",
                                      model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                      model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                      summary.shape_status,
                                      summary.range_status,
                                      summary.backend_status,
                                      summary.allocation_attempted,
                                      summary.transfer_attempted,
                                      summary.cleanup_attempted,
                                      summary.cleanup_status,
                                      summary.bytes_planned,
                                      summary.bytes_allocated,
                                      summary.bytes_transferred);
    yvex_cli_out_writef(stdout, "tensors_total: %llu\n", summary.tensors_total);
    yvex_cli_out_writef(stdout, "tensors_materialized: %llu\n", summary.tensors_materialized);
    yvex_cli_out_writef(stdout, "tensors_failed: %llu\n", summary.tensors_failed);
    yvex_cli_out_writef(stdout, "bytes_total: %llu\n", summary.bytes_total);
    yvex_cli_out_writef(stdout, "bytes_materialized: %llu\n", summary.bytes_materialized);
    yvex_cli_out_writef(stdout, "backend_allocated_bytes: %llu\n", summary.backend_allocated_bytes);
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "reason: graph partial; materialized weights do not imply executable inference\n");
    yvex_cli_out_writef(stdout, "status: %s\n", summary.status == YVEX_WEIGHT_STATUS_MATERIALIZED
           ? "weights-materialized"
           : "weights-partial");

    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    yvex_model_context_close(&ctx);
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static yvex_materialize_scope parse_materialize_scope_name(const char *name)
{
    if (!name) return YVEX_MATERIALIZE_SCOPE_UNKNOWN;
    if (strcmp(name, "selected-tensor") == 0) return YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR;
    if (strcmp(name, "partial-model") == 0) return YVEX_MATERIALIZE_SCOPE_PARTIAL_MODEL;
    if (strcmp(name, "full-model") == 0) return YVEX_MATERIALIZE_SCOPE_FULL_MODEL;
    return YVEX_MATERIALIZE_SCOPE_UNKNOWN;
}

static void print_materialize_gate_report(FILE *fp,
                                          const yvex_materialize_gate_options *options,
                                          const yvex_materialize_gate_summary *summary,
                                          const char *reason)
{
    yvex_cli_out_writef(fp, "materialize gate: check\n");
    yvex_cli_out_writef(fp, "label: %s\n", summary->label ? summary->label : "");
    yvex_cli_out_writef(fp, "family: %s\n", summary->family ? summary->family : "");
    yvex_cli_out_writef(fp, "scope: %s\n", yvex_materialize_scope_name(summary->scope));
    yvex_cli_out_writef(fp, "model: %s\n", summary->model_path ? summary->model_path : "");
    yvex_cli_out_writef(fp, "expected_sha256: %s\n", summary->expected_sha256 ? summary->expected_sha256 : "");
    yvex_cli_out_writef(fp, "actual_sha256: %s\n", summary->actual_sha256);
    yvex_cli_out_writef(fp, "digest_status: %s\n", summary->digest_status ? summary->digest_status : "unrequested");
    yvex_cli_out_writef(fp, "identity_status: %s\n", summary->identity_status ? summary->identity_status : "unrequested");
    yvex_cli_out_writef(fp, "metadata_status: %s\n", summary->metadata_status ? summary->metadata_status : "unregistered");
    yvex_cli_out_writef(fp, "materialization_gate: %s\n", summary->materialization_gate ? summary->materialization_gate : "fail");
    yvex_cli_out_writef(fp, "materialization_phase: %s\n", summary->materialization_phase ? summary->materialization_phase : "preflight");
    yvex_cli_out_writef(fp, "integrity_status: %s\n", summary->integrity_status ? summary->integrity_status : "unchecked");
    yvex_cli_out_writef(fp, "shape_status: %s\n", summary->shape_status ? summary->shape_status : "unchecked");
    yvex_cli_out_writef(fp, "range_status: %s\n", summary->range_status ? summary->range_status : "unchecked");
    yvex_cli_out_writef(fp, "backend_status: %s\n", summary->backend_status ? summary->backend_status : "not-opened");
    yvex_cli_out_writef(fp, "allocation_attempted: %s\n", summary->allocation_attempted ? "true" : "false");
    yvex_cli_out_writef(fp, "transfer_attempted: %s\n", summary->transfer_attempted ? "true" : "false");
    yvex_cli_out_writef(fp, "cleanup_attempted: %s\n", summary->cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(fp, "cleanup_status: %s\n", summary->cleanup_status ? summary->cleanup_status : "not-needed");
    yvex_cli_out_writef(fp, "bytes_planned: %llu\n", summary->bytes_planned);
    yvex_cli_out_writef(fp, "bytes_allocated: %llu\n", summary->bytes_allocated);
    yvex_cli_out_writef(fp, "bytes_transferred: %llu\n", summary->bytes_transferred);
    yvex_cli_out_writef(fp, "file_bytes: %llu\n", summary->file_bytes);
    yvex_cli_out_writef(fp, "tensor_count: %llu\n", summary->tensor_count);
    yvex_cli_out_writef(fp, "expected_tensor_matches: %llu\n", summary->expected_tensor_matches);
    yvex_cli_out_writef(fp, "expected_tensor_mismatches: %llu\n", summary->expected_tensor_mismatches);
    yvex_cli_out_writef(fp, "bytes_materialized_cpu: %llu\n", summary->bytes_materialized_cpu);
    yvex_cli_out_writef(fp, "bytes_materialized_cuda: %llu\n", summary->bytes_materialized_cuda);
    yvex_cli_out_writef(fp, "cpu: %s\n", yvex_materialize_backend_status_name(summary->cpu_status));
    yvex_cli_out_writef(fp, "cuda: %s\n", yvex_materialize_backend_status_name(summary->cuda_status));
    yvex_cli_out_writef(fp, "repeat_count: %u\n", summary->repeat_count);
    yvex_cli_out_writef(fp, "cleanup_verified: %s\n", summary->cleanup_verified ? "yes" : "no");
    yvex_cli_out_writef(fp, "execution_ready: false\n");
    if (options && options->expected_tensor_count == 1 && options->expected_tensors) {
        const yvex_materialize_expected_tensor *t = &options->expected_tensors[0];
        yvex_cli_out_writef(fp, "expected_tensor: %s\n", t->name ? t->name : "");
        yvex_cli_out_writef(fp, "expected_rank: %u\n", t->rank);
        yvex_cli_out_writef(fp, "expected_dims:");
        if (t->rank > 0) yvex_cli_out_writef(fp, " %llu", t->dims[0]);
        if (t->rank > 1) yvex_cli_out_writef(fp, ",%llu", t->dims[1]);
        if (t->rank > 2) yvex_cli_out_writef(fp, ",%llu", t->dims[2]);
        if (t->rank > 3) yvex_cli_out_writef(fp, ",%llu", t->dims[3]);
        yvex_cli_out_writef(fp, "\n");
        yvex_cli_out_writef(fp, "expected_dtype: %s\n", t->dtype ? t->dtype : "");
        yvex_cli_out_writef(fp, "expected_bytes: %llu\n", t->bytes);
    }
    yvex_cli_out_writef(fp, "failure_class: %s\n",
            yvex_materialize_failure_class_name(summary->failure_class));
    yvex_cli_out_writef(fp, "reason: %s\n", reason && reason[0] ? reason : "materialization hardening gate");
    yvex_cli_out_writef(fp, "status: %s\n", yvex_materialize_gate_status_name(summary->status));
}

static int command_materialize_gate(int arg_count, char **args)
{
    yvex_materialize_gate_options options;
    yvex_materialize_expected_tensor expected;
    yvex_materialize_gate_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    const char *report_out = NULL;
    unsigned long long value;
    int have_tensor = 0;
    int have_rank = 0;
    int have_dims = 0;
    int have_dtype = 0;
    int have_bytes = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&expected, 0, sizeof(expected));
    memset(&summary, 0, sizeof(summary));
    memset(&model_ref, 0, sizeof(model_ref));

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_materialize_gate_help(stdout);
        return 0;
    }
    if (arg_count < 3 || strcmp(args[2], "check") != 0) {
        yvex_cli_out_writef(stderr, "yvex: materialize-gate requires subcommand check\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--require-cpu") == 0) {
            options.require_cpu = 1;
            options.check_cpu = 1;
            continue;
        }
        if (strcmp(args[i], "--require-cuda") == 0) {
            options.require_cuda = 1;
            options.check_cuda = 1;
            continue;
        }
        if (strcmp(args[i], "--check-cleanup") == 0) {
            options.check_cleanup = 1;
            continue;
        }
        if (strcmp(args[i], "--" "json") == 0) {
            options.json = 1;
            continue;
        }
        if (i + 1 >= arg_count) {
            yvex_cli_out_writef(stderr, "yvex: materialize-gate option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--model") == 0) {
            options.model_path = args[++i];
        } else if (strcmp(args[i], "--label") == 0) {
            options.label = args[++i];
        } else if (strcmp(args[i], "--family") == 0) {
            options.family = args[++i];
        } else if (strcmp(args[i], "--sha256") == 0) {
            options.sha256 = args[++i];
        } else if (strcmp(args[i], "--scope") == 0) {
            options.scope = parse_materialize_scope_name(args[++i]);
            if (options.scope == YVEX_MATERIALIZE_SCOPE_UNKNOWN) {
                yvex_cli_out_writef(stderr, "yvex: invalid materialize-gate scope\n");
                return 2;
            }
        } else if (strcmp(args[i], "--expect-tensor") == 0) {
            expected.name = args[++i];
            have_tensor = 1;
        } else if (strcmp(args[i], "--expect-rank") == 0) {
            if (!parse_positive_ull(args[++i], &value) || value > 4ull) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-rank\n");
                return 2;
            }
            expected.rank = (unsigned int)value;
            have_rank = 1;
        } else if (strcmp(args[i], "--expect-dims") == 0) {
            const char *dims_text = args[++i];
            if (!have_rank || !parse_dims_csv(dims_text, expected.rank, expected.dims)) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-dims; pass --expect-rank before --expect-dims\n");
                return 2;
            }
            have_dims = 1;
        } else if (strcmp(args[i], "--expect-dtype") == 0) {
            expected.dtype = args[++i];
            have_dtype = 1;
        } else if (strcmp(args[i], "--expect-bytes") == 0) {
            if (!parse_positive_ull(args[++i], &expected.bytes)) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-bytes\n");
                return 2;
            }
            have_bytes = 1;
        } else if (strcmp(args[i], "--backend") == 0) {
            const char *backend = args[++i];
            if (strcmp(backend, "cpu") == 0) {
                options.check_cpu = 1;
            } else if (strcmp(backend, "cuda") == 0) {
                options.check_cuda = 1;
            } else {
                yvex_cli_out_writef(stderr, "yvex: materialize-gate backend must be cpu or cuda\n");
                return 2;
            }
        } else if (strcmp(args[i], "--repeat") == 0) {
            if (!parse_positive_ull(args[++i], &value) || value > 1000ull) {
                yvex_cli_out_writef(stderr, "yvex: invalid --repeat\n");
                return 2;
            }
            options.repeat_count = (unsigned int)value;
        } else if (strcmp(args[i], "--report-out") == 0) {
            report_out = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown materialize-gate option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help materialize-gate' for usage.\n");
            return 2;
        }
    }

    if (!options.model_path || !options.label || !options.family ||
        options.scope == YVEX_MATERIALIZE_SCOPE_UNKNOWN) {
        yvex_cli_out_writef(stderr, "yvex: materialize-gate check requires --model --label --family --scope\n");
        return 2;
    }
    if (have_tensor || have_rank || have_dims || have_dtype || have_bytes) {
        if (!have_tensor || !have_rank || !have_dims || !have_dtype || !have_bytes) {
            yvex_cli_out_writef(stderr, "yvex: materialize-gate expected tensor spec must be complete\n");
            return 2;
        }
        options.expected_tensors = &expected;
        options.expected_tensor_count = 1;
    }
    if (options.repeat_count == 0) options.repeat_count = 1;

    rc = yvex_model_ref_resolve(&model_ref, options.model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "materialize-gate");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }
    options.model_path = model_ref.path;
    if ((!options.sha256 || !options.sha256[0]) &&
        model_ref.kind == YVEX_MODEL_REF_ALIAS &&
        model_ref.sha256 && model_ref.sha256[0]) {
        options.sha256 = model_ref.sha256;
    }
    options.metadata_status = model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";

    rc = yvex_materialize_gate_check(&options, &summary, &err);
    print_materialize_gate_report(stdout, &options, &summary,
                                  rc == YVEX_OK ? NULL : yvex_error_message(&err));
    if (report_out) {
        FILE *fp = fopen(report_out, "wb");
        if (!fp) {
            yvex_cli_out_writef(stderr, "yvex: cannot write report: %s\n", report_out);
            yvex_model_ref_clear(&model_ref);
            return 1;
        }
        print_materialize_gate_report(fp, &options, &summary,
                                      rc == YVEX_OK ? NULL : yvex_error_message(&err));
        fclose(fp);
    }
    yvex_model_ref_clear(&model_ref);
    return rc == YVEX_OK ? 0 : exit_for_status(rc);
}

static void print_model_gate_report(FILE *fp,
                                    const yvex_model_gate_options *options,
                                    const yvex_model_gate_summary *summary,
                                    const char *reason)
{
    yvex_cli_out_writef(fp, "model gate: check\n");
    yvex_cli_out_writef(fp, "label: %s\n", summary->model_label ? summary->model_label : "");
    yvex_cli_out_writef(fp, "family: %s\n", summary->family ? summary->family : "");
    yvex_cli_out_writef(fp, "model: %s\n", summary->model_path ? summary->model_path : "");
    yvex_cli_out_writef(fp, "expected_sha256: %s\n", summary->expected_sha256 ? summary->expected_sha256 : "");
    yvex_cli_out_writef(fp, "actual_sha256: %s\n", summary->actual_sha256);
    yvex_cli_out_writef(fp, "digest_status: %s\n", summary->digest_status ? summary->digest_status : "unrequested");
    yvex_cli_out_writef(fp, "identity_status: %s\n", summary->identity_status ? summary->identity_status : "unrequested");
    yvex_cli_out_writef(fp, "file_bytes: %llu\n", summary->file_bytes);
    yvex_cli_out_writef(fp, "tensor_count: %llu\n", summary->tensor_count);
    yvex_cli_out_writef(fp, "expected_tensor_matches: %llu\n", summary->expected_tensor_matches);
    yvex_cli_out_writef(fp, "expected_tensor_mismatches: %llu\n", summary->expected_tensor_mismatches);
    yvex_cli_out_writef(fp, "cpu: %s\n", yvex_model_gate_backend_status_name(summary->cpu_status));
    yvex_cli_out_writef(fp, "cuda: %s\n", yvex_model_gate_backend_status_name(summary->cuda_status));
    yvex_cli_out_writef(fp, "support_level: %s\n", yvex_model_support_level_name(summary->support_level));
    yvex_cli_out_writef(fp, "execution_ready: false\n");
    if (options && options->expected_tensor_count == 1 && options->expected_tensors) {
        const yvex_model_gate_expected_tensor *t = &options->expected_tensors[0];
        yvex_cli_out_writef(fp, "expected_tensor: %s\n", t->name ? t->name : "");
        yvex_cli_out_writef(fp, "expected_rank: %u\n", t->rank);
        yvex_cli_out_writef(fp, "expected_dims:");
        if (t->rank > 0) yvex_cli_out_writef(fp, " %llu", t->dims[0]);
        if (t->rank > 1) yvex_cli_out_writef(fp, ",%llu", t->dims[1]);
        if (t->rank > 2) yvex_cli_out_writef(fp, ",%llu", t->dims[2]);
        if (t->rank > 3) yvex_cli_out_writef(fp, ",%llu", t->dims[3]);
        yvex_cli_out_writef(fp, "\n");
        yvex_cli_out_writef(fp, "expected_dtype: %s\n", t->dtype ? t->dtype : "");
        yvex_cli_out_writef(fp, "expected_bytes: %llu\n", t->bytes);
    }
    yvex_cli_out_writef(fp, "reason: %s\n", reason && reason[0] ? reason : "selected tensor materialization gate");
    yvex_cli_out_writef(fp, "status: %s\n", yvex_model_gate_status_name(summary->status));
}

static int command_model_gate(int arg_count, char **args)
{
    yvex_model_gate_options options;
    yvex_model_gate_expected_tensor expected;
    yvex_model_gate_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    const char *report_out = NULL;
    unsigned long long value;
    int have_tensor = 0;
    int have_rank = 0;
    int have_dims = 0;
    int have_dtype = 0;
    int have_bytes = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&expected, 0, sizeof(expected));
    memset(&summary, 0, sizeof(summary));
    memset(&model_ref, 0, sizeof(model_ref));

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_gate_help(stdout);
        return 0;
    }
    if (arg_count < 3 || strcmp(args[2], "check") != 0) {
        yvex_cli_out_writef(stderr, "yvex: model-gate requires subcommand check\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--require-cpu") == 0) {
            options.require_cpu = 1;
            options.check_cpu = 1;
            continue;
        }
        if (strcmp(args[i], "--require-cuda") == 0) {
            options.require_cuda = 1;
            options.check_cuda = 1;
            continue;
        }
        if (i + 1 >= arg_count) {
            yvex_cli_out_writef(stderr, "yvex: model-gate option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--model") == 0) {
            options.model_path = args[++i];
        } else if (strcmp(args[i], "--label") == 0) {
            options.model_label = args[++i];
        } else if (strcmp(args[i], "--family") == 0) {
            options.family = args[++i];
        } else if (strcmp(args[i], "--sha256") == 0) {
            options.artifact_sha256 = args[++i];
        } else if (strcmp(args[i], "--expect-tensor") == 0) {
            expected.name = args[++i];
            have_tensor = 1;
        } else if (strcmp(args[i], "--expect-rank") == 0) {
            if (!parse_positive_ull(args[++i], &value) || value > 4ull) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-rank\n");
                return 2;
            }
            expected.rank = (unsigned int)value;
            have_rank = 1;
        } else if (strcmp(args[i], "--expect-dims") == 0) {
            const char *dims_text = args[++i];
            if (!have_rank || !parse_dims_csv(dims_text, expected.rank, expected.dims)) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-dims; pass --expect-rank before --expect-dims\n");
                return 2;
            }
            have_dims = 1;
        } else if (strcmp(args[i], "--expect-dtype") == 0) {
            expected.dtype = args[++i];
            have_dtype = 1;
        } else if (strcmp(args[i], "--expect-bytes") == 0) {
            if (!parse_positive_ull(args[++i], &expected.bytes)) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-bytes\n");
                return 2;
            }
            have_bytes = 1;
        } else if (strcmp(args[i], "--backend") == 0) {
            const char *backend = args[++i];
            if (strcmp(backend, "cpu") == 0) {
                options.check_cpu = 1;
            } else if (strcmp(backend, "cuda") == 0) {
                options.check_cuda = 1;
            } else {
                yvex_cli_out_writef(stderr, "yvex: model-gate backend must be cpu or cuda\n");
                return 2;
            }
        } else if (strcmp(args[i], "--report-out") == 0) {
            report_out = args[++i];
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown model-gate option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help model-gate' for usage.\n");
            return 2;
        }
    }

    if (!options.model_path || !options.model_label || !options.family ||
        !have_tensor || !have_rank || !have_dims || !have_dtype || !have_bytes) {
        yvex_cli_out_writef(stderr, "yvex: model-gate check requires --model --label --family and one complete --expect-* tensor spec\n");
        return 2;
    }

    options.expected_tensors = &expected;
    options.expected_tensor_count = 1;
    rc = yvex_model_ref_resolve(&model_ref, options.model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "model-gate");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }
    options.model_path = model_ref.path;
    if ((!options.artifact_sha256 || !options.artifact_sha256[0]) &&
        model_ref.kind == YVEX_MODEL_REF_ALIAS &&
        model_ref.sha256 && model_ref.sha256[0]) {
        options.artifact_sha256 = model_ref.sha256;
    }

    rc = yvex_model_gate_check(&options, &summary, &err);
    print_model_gate_report(stdout, &options, &summary,
                            rc == YVEX_OK ? NULL : yvex_error_message(&err));
    if (report_out) {
        FILE *fp = fopen(report_out, "wb");
        if (!fp) {
            yvex_cli_out_writef(stderr, "yvex: cannot write report: %s\n", report_out);
            yvex_model_ref_clear(&model_ref);
            return 1;
        }
        print_model_gate_report(fp, &options, &summary,
                                rc == YVEX_OK ? NULL : yvex_error_message(&err));
        fclose(fp);
    }
    yvex_model_ref_clear(&model_ref);
    return rc == YVEX_OK ? 0 : exit_for_status(rc);
}

static int command_integrity_report(int arg_count, char **args)
{
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_model_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_cli_graph_guard_report graph_report;
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    yvex_model_ref ref;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    const char *identity_status = "unregistered";
    const char *metadata_status = "unregistered";
    const char *readiness_status = "not-checked";
    const char *support_level = "not-checked";
    const char *materialization_preflight = "not-checked";
    const char *materialization_gate = "not-checked";
    const char *materialization_backend = "not-checked";
    const char *backend_status = "not-checked";
    const char *graph_fixture_guard = "not-applicable";
    const char *graph_partial_guard = "not-checked";
    const char *graph_partial_backend = "not-checked";
    const char *graph_partial_dispatch_ready = "false";
    const char *graph_partial_reference_ready = "false";
    const char *report_status = "pass";
    const char *status = "integrity-report-pass";
    const char *model_input_kind;
    int audit_output = 0;
    unsigned long long selected_hidden_size = 0ull;
    unsigned long long selected_vocab_size = 0ull;
    unsigned long long selected_output_count = 0ull;
    unsigned long long selected_output_bytes = 0ull;
    unsigned long long selected_slice_bytes = 0ull;
    int metadata_checked = 0;
    int selected_ready = 0;
    int hard_fail = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&current_metadata, 0, sizeof(current_metadata));
    memset(&registered_metadata, 0, sizeof(registered_metadata));
    memset(&metadata_report, 0, sizeof(metadata_report));
    memset(&graph_report, 0, sizeof(graph_report));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&ref, 0, sizeof(ref));

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = args[++i];
        } else if (strcmp(args[i], "--expect-sha256") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --expect-sha256 requires HASH\n");
                return 2;
            }
            integrity_options.expect_sha256 = args[++i];
        } else if (strcmp(args[i], "--require-token-embedding") == 0) {
            integrity_options.require_token_embedding = 1;
        } else if (strcmp(args[i], "--partial-token") == 0) {
            if (i + 1 >= arg_count || !parse_uint_allow_zero(args[i + 1],
                                                        &integrity_options.token_id)) {
                yvex_cli_out_writef(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            integrity_options.require_token_embedding = 1;
            i += 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            audit_output = 1;
        } else if (strcmp(args[i], "--" "output") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: integrity report --" "output requires normal, table, or audit\n");
                return 2;
            }
            i += 1;
            if (strcmp(args[i], "normal") == 0 || strcmp(args[i], "table") == 0) {
                audit_output = 0;
            } else if (strcmp(args[i], "audit") == 0) {
                audit_output = 1;
            } else {
                yvex_cli_out_writef(stderr, "yvex: integrity report unsupported output mode: %s\n", args[i]);
                return 2;
            }
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown integrity report option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try '" "yvex help integrity' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        yvex_cli_out_writef(stderr, "yvex: integrity report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (backend_name &&
        strcmp(backend_name, "cpu") != 0 &&
        strcmp(backend_name, "cuda") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (ref.kind == YVEX_MODEL_REF_ALIAS && ref.sha256 && ref.sha256[0]) {
        integrity_options.registered_sha256 = ref.sha256;
    }

    rc = yvex_artifact_integrity_check_path(ref.path, &integrity_options,
                                            &integrity_report, &err);
    if (!integrity_report.passed) {
        hard_fail = 1;
    }

    model_input_kind = ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path";
    if (ref.kind == YVEX_MODEL_REF_ALIAS) {
        if (!ref.sha256 || !ref.sha256[0] || !yvex_sha256_hex_is_valid(ref.sha256)) {
            identity_status = "missing";
            hard_fail = 1;
        } else if (strcmp(integrity_report.digest_status, "pass") == 0 &&
                   (ref.registered_file_size == 0ull ||
                    ref.registered_file_size == integrity_report.file_size)) {
            identity_status = "pass";
        } else {
            identity_status = "fail";
            hard_fail = 1;
        }
    }

    if (integrity_report.passed) {
        yvex_error metadata_err;
        yvex_error_clear(&metadata_err);
        if (yvex_model_metadata_snapshot_read(&current_metadata, ref.path, &metadata_err) == YVEX_OK) {
            metadata_checked = 1;
            support_level = current_metadata.entry.support_level &&
                            current_metadata.entry.support_level[0]
                                ? current_metadata.entry.support_level
                                : "not-checked";
            selected_ready = current_metadata.entry.selected_embedding_ready;
            selected_hidden_size = current_metadata.entry.selected_embedding_hidden_size;
            selected_vocab_size = current_metadata.entry.selected_embedding_vocab_size;
            selected_output_count = current_metadata.entry.selected_embedding_output_count;
            selected_slice_bytes = current_metadata.entry.selected_embedding_slice_bytes;
            selected_output_bytes = selected_output_count * (unsigned long long)sizeof(float);

            if (ref.kind == YVEX_MODEL_REF_ALIAS && strcmp(identity_status, "pass") == 0) {
                yvex_model_ref_registry_entry_view(&ref, &registered_metadata);
                if (yvex_model_registry_compare_metadata(&registered_metadata,
                                                         &current_metadata.entry,
                                                         &metadata_report,
                                                         &metadata_err) == YVEX_OK) {
                    metadata_status = metadata_report.metadata_status[0]
                                          ? metadata_report.metadata_status
                                          : "unknown";
                    readiness_status = metadata_report.readiness_status[0]
                                           ? metadata_report.readiness_status
                                           : "unknown";
                    support_level = ref.support_level && ref.support_level[0]
                                        ? ref.support_level
                                        : support_level;
                    if (strcmp(metadata_status, "pass") != 0 ||
                        strcmp(readiness_status, "pass") != 0) {
                        hard_fail = 1;
                    }
                } else {
                    metadata_status = "fail";
                    readiness_status = "fail";
                    hard_fail = 1;
                }
            } else if (ref.kind == YVEX_MODEL_REF_ALIAS) {
                metadata_status = "not-checked";
                readiness_status = "not-checked";
            } else {
                metadata_status = "unregistered";
                readiness_status = selected_ready ? "pass" : "not-checked";
            }
        } else {
            metadata_checked = 0;
            if (ref.kind == YVEX_MODEL_REF_ALIAS && strcmp(identity_status, "pass") == 0) {
                metadata_status = "fail";
                readiness_status = "fail";
                hard_fail = 1;
            }
            yvex_error_clear(&metadata_err);
        }
    }

    if (integrity_report.selected_embedding_shape[0]) {
        selected_ready = strcmp(integrity_report.selected_embedding_shape, "valid") == 0;
        selected_hidden_size = integrity_report.selected_embedding_hidden_size;
        selected_vocab_size = integrity_report.selected_embedding_vocab_size;
        selected_output_count = integrity_report.selected_embedding_output_count;
        selected_output_bytes = integrity_report.selected_embedding_output_bytes;
        selected_slice_bytes = integrity_report.selected_embedding_slice_bytes;
        readiness_status = selected_ready ? "pass" : "fail";
    } else if (integrity_options.require_token_embedding) {
        selected_ready = 0;
        readiness_status = "fail";
        hard_fail = 1;
    }

    if (backend_name) {
        materialization_backend = backend_name;
        graph_partial_backend = backend_name;
        if (!hard_fail) {
            backend_options.kind = strcmp(backend_name, "cuda") == 0
                                       ? YVEX_BACKEND_KIND_CUDA
                                       : YVEX_BACKEND_KIND_CPU;
            rc = yvex_backend_open(&backend, &backend_options, &err);
            if (rc == YVEX_OK) {
                backend_status = yvex_backend_status_name(
                    yvex_backend_status_of(backend));
                if (yvex_backend_supports(backend,
                                          YVEX_BACKEND_CAP_TENSOR_ALLOC) &&
                    yvex_backend_supports(backend,
                                          YVEX_BACKEND_CAP_TENSOR_READ_WRITE)) {
                    materialization_preflight = "pass";
                    materialization_gate = "pass";
                } else {
                    materialization_preflight = "fail";
                    materialization_gate = "fail";
                    hard_fail = 1;
                }
                yvex_backend_close(backend);
                backend = NULL;
            } else {
                backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
                materialization_preflight = "fail";
                materialization_gate = "fail";
                hard_fail = 1;
                yvex_error_clear(&err);
            }
        } else {
            backend_status = "not-opened";
            materialization_preflight = "fail";
            materialization_gate = "fail";
        }

        if (!hard_fail && selected_ready) {
            yvex_error graph_err;
            yvex_error_clear(&graph_err);
            rc = yvex_graph_preflight(&ref,
                                       backend_name,
                                       0,
                                       0,
                                       integrity_options.token_id,
                                       &graph_report,
                                       &graph_err);
            if (rc == YVEX_OK && strcmp(graph_report.guard_status, "pass") == 0) {
                graph_partial_guard = "pass";
                graph_partial_dispatch_ready = "true";
                graph_partial_reference_ready = "true";
            } else {
                graph_partial_guard = "fail";
                graph_partial_dispatch_ready = "false";
                graph_partial_reference_ready = "false";
                hard_fail = 1;
                yvex_error_clear(&graph_err);
            }
        } else if (backend_name && !selected_ready && integrity_options.require_token_embedding) {
            graph_partial_guard = "fail";
        } else if (backend_name && !selected_ready) {
            graph_partial_guard = "not-applicable";
        }
    }

    if (hard_fail) {
        report_status = "fail";
        status = "integrity-report-fail";
    }

    if (!audit_output) {
        const char *top_blocker = "none";
        if (hard_fail) {
            if (integrity_report.error_count > 0u && integrity_report.issues[0].code[0]) {
                top_blocker = integrity_report.issues[0].code;
            } else if (strcmp(identity_status, "pass") != 0) {
                top_blocker = "identity";
            } else if (strcmp(metadata_status, "pass") != 0 &&
                       strcmp(metadata_status, "unregistered") != 0) {
                top_blocker = "metadata";
            } else if (strcmp(materialization_preflight, "pass") != 0 &&
                       strcmp(materialization_preflight, "not-checked") != 0) {
                top_blocker = "materialization-preflight";
            } else {
                top_blocker = "integrity";
            }
        }
        yvex_cli_out_writef(stdout, "integrity: %s model=%s backend=%s\n",
               hard_fail ? "fail" : "pass",
               model_arg,
               backend_name ? backend_name : "not-requested");
        yvex_cli_out_writef(stdout, "identity: %s digest: %s tensors=%llu invalid_ranges=%llu\n",
               identity_status,
               integrity_report.digest_status[0] ? integrity_report.digest_status : "unknown",
               integrity_report.tensor_count,
               integrity_report.tensor_ranges_invalid);
        yvex_cli_out_writef(stdout, "materialization_preflight: %s\n", materialization_preflight);
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", top_blocker);
        yvex_cli_out_writef(stdout, "boundary: integrity gate only, generation unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", status);
        yvex_model_ref_clear(&ref);
        return hard_fail ? exit_for_status(YVEX_ERR_STATE) : 0;
    }

    yvex_cli_out_writef(stdout, "artifact_integrity_report: summary\n");
    yvex_cli_out_writef(stdout, "model: %s\n", model_arg);
    yvex_cli_out_writef(stdout, "resolved_path: %s\n", ref.path ? ref.path : "");
    yvex_cli_out_writef(stdout, "model_input_kind: %s\n", model_input_kind);
    yvex_cli_out_writef(stdout, "format: %s\n", integrity_report.format[0] ? integrity_report.format : "unknown");
    if (integrity_report.version) {
        yvex_cli_out_writef(stdout, "version: %u\n", integrity_report.version);
    }
    yvex_cli_out_writef(stdout, "architecture: %s\n",
           integrity_report.architecture[0] ? integrity_report.architecture : "unknown");
    yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status);
    yvex_cli_out_writef(stdout, "digest_status: %s\n",
           integrity_report.digest_status[0] ? integrity_report.digest_status : "unknown");
    yvex_cli_out_writef(stdout, "sha256: %s\n", integrity_report.sha256[0] ? integrity_report.sha256 : "unavailable");
    yvex_cli_out_writef(stdout, "registered_sha256: %s\n",
           integrity_report.registered_sha256[0] ? integrity_report.registered_sha256 : "absent");
    if (integrity_report.expected_sha256[0]) {
        yvex_cli_out_writef(stdout, "expected_sha256: %s\n", integrity_report.expected_sha256);
        yvex_cli_out_writef(stdout, "actual_sha256: %s\n",
               integrity_report.sha256[0] ? integrity_report.sha256 : "unavailable");
    }
    yvex_cli_out_writef(stdout, "metadata_status: %s\n", metadata_status);
    yvex_cli_out_writef(stdout, "readiness_status: %s\n", readiness_status);
    yvex_cli_out_writef(stdout, "support_level: %s\n", support_level);
    yvex_cli_out_writef(stdout, "integrity_status: %s\n", integrity_report.passed ? "pass" : "fail");
    yvex_cli_out_writef(stdout, "integrity_errors: %u\n", integrity_report.error_count);
    yvex_cli_out_writef(stdout, "integrity_warnings: %u\n", integrity_report.warning_count);
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", integrity_report.tensor_count);
    yvex_cli_out_writef(stdout, "known_tensor_bytes: %llu\n", integrity_report.known_tensor_bytes);
    yvex_cli_out_writef(stdout, "tensor_ranges_checked: %llu\n", integrity_report.tensor_ranges_checked);
    yvex_cli_out_writef(stdout, "tensor_ranges_invalid: %llu\n", integrity_report.tensor_ranges_invalid);
    yvex_cli_out_writef(stdout, "tensor_shapes_checked: %llu\n", integrity_report.tensor_shapes_checked);
    yvex_cli_out_writef(stdout, "tensor_shapes_invalid: %llu\n", integrity_report.tensor_shapes_invalid);
    yvex_cli_out_writef(stdout, "tensor_dtypes_checked: %llu\n", integrity_report.tensor_dtypes_checked);
    yvex_cli_out_writef(stdout, "tensor_dtypes_invalid: %llu\n", integrity_report.tensor_dtypes_invalid);
    yvex_cli_out_writef(stdout, "tensor_byte_counts_checked: %llu\n", integrity_report.tensor_byte_counts_checked);
    yvex_cli_out_writef(stdout, "tensor_byte_counts_invalid: %llu\n", integrity_report.tensor_byte_counts_invalid);

    yvex_cli_out_writef(stdout, "primary_tensor: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_name
               ? current_metadata.entry.primary_tensor_name
               : "");
    yvex_cli_out_writef(stdout, "primary_tensor_role: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_role
               ? current_metadata.entry.primary_tensor_role
               : "");
    yvex_cli_out_writef(stdout, "primary_tensor_dtype: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_dtype
               ? current_metadata.entry.primary_tensor_dtype
               : "");
    yvex_cli_out_writef(stdout, "primary_tensor_rank: %u\n",
           metadata_checked ? current_metadata.entry.primary_tensor_rank : 0u);
    yvex_cli_out_writef(stdout, "primary_tensor_dims: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_dims
               ? current_metadata.entry.primary_tensor_dims
               : "");
    yvex_cli_out_writef(stdout, "primary_tensor_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.primary_tensor_bytes : 0ull);
    yvex_cli_out_writef(stdout, "selected_embedding_ready: %s\n", selected_ready ? "true" : "false");
    yvex_cli_out_writef(stdout, "selected_embedding_hidden_size: %llu\n", selected_hidden_size);
    yvex_cli_out_writef(stdout, "selected_embedding_vocab_size: %llu\n", selected_vocab_size);
    yvex_cli_out_writef(stdout, "selected_embedding_output_count: %llu\n", selected_output_count);
    yvex_cli_out_writef(stdout, "selected_embedding_output_bytes: %llu\n", selected_output_bytes);
    yvex_cli_out_writef(stdout, "selected_embedding_slice_bytes: %llu\n", selected_slice_bytes);
    yvex_cli_out_writef(stdout, "backend_status: %s\n", backend_status);
    yvex_cli_out_writef(stdout, "materialization_preflight: %s\n", materialization_preflight);
    yvex_cli_out_writef(stdout, "materialization_backend: %s\n", materialization_backend);
    yvex_cli_out_writef(stdout, "materialization_gate: %s\n", materialization_gate);
    yvex_cli_out_writef(stdout, "allocation_required_bytes: %llu\n", integrity_report.known_tensor_bytes);
    yvex_cli_out_writef(stdout, "graph_fixture_guard: %s\n", graph_fixture_guard);
    yvex_cli_out_writef(stdout, "graph_partial_guard: %s\n", graph_partial_guard);
    yvex_cli_out_writef(stdout, "graph_partial_backend: %s\n", graph_partial_backend);
    yvex_cli_out_writef(stdout, "graph_partial_token: %u\n", integrity_options.token_id);
    yvex_cli_out_writef(stdout, "graph_partial_dispatch_ready: %s\n", graph_partial_dispatch_ready);
    yvex_cli_out_writef(stdout, "graph_partial_reference_ready: %s\n", graph_partial_reference_ready);
    if (metadata_checked && ref.kind == YVEX_MODEL_REF_ALIAS) {
        for (i = 0; i < (int)metadata_report.issue_count; ++i) {
            yvex_cli_out_writef(stdout, "metadata_issue_%u_code: %s\n", (unsigned int)i,
                   metadata_report.issues[i].code);
            yvex_cli_out_writef(stdout, "metadata_issue_%u_registered: %s\n", (unsigned int)i,
                   metadata_report.issues[i].registered_value);
            yvex_cli_out_writef(stdout, "metadata_issue_%u_current: %s\n", (unsigned int)i,
                   metadata_report.issues[i].current_value);
        }
    }
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "report_status: %s\n", report_status);
    yvex_cli_out_writef(stdout, "status: %s\n", status);

    yvex_model_ref_clear(&ref);
    return hard_fail ? exit_for_status(YVEX_ERR_STATE) : 0;
}

static int command_tensors(int arg_count, char **args)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *table = NULL;
    const yvex_gguf_header *header;
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_error err;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));

    if (arg_count != 3 || strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0) {
        if (arg_count == 3) {
            yvex_tensors_help(stdout);
            return 0;
        }
        yvex_cli_out_writef(stderr, "yvex: tensors requires exactly one FILE_OR_ALIAS\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex tensors FILE_OR_ALIAS\n");
        return 2;
    }

    rc = open_artifact_for_gguf(args[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&table, gguf, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(artifact,
                                              gguf,
                                              table,
                                              &integrity_options,
                                              &integrity_report,
                                              &err);
    }
    if (rc != YVEX_OK) {
        yvex_tensor_table_close(table);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    header = yvex_gguf_header_view(gguf);
    yvex_cli_out_writef(stdout, "format: gguf\n");
    yvex_cli_out_writef(stdout, "version: %u\n", header->version);
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", yvex_gguf_tensor_count(gguf));
    yvex_cli_out_writef(stdout, "tensor_data_offset: %llu\n", yvex_gguf_tensor_data_offset(gguf));
    yvex_cli_out_writef(stdout, "alignment: %u\n", yvex_gguf_alignment(gguf));
    yvex_cli_out_writef(stdout, "\n");

    for (i = 0; i < yvex_tensor_table_count(table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(table, i);
        yvex_tensor_range range;
        int range_rc;

        memset(&range, 0, sizeof(range));
        range_rc = yvex_tensor_range_validate(artifact, gguf, tensor, &range, &err);
        yvex_cli_out_writef(stdout, "%llu %s role=%s rank=%u dims=",
               i,
               tensor->name,
               yvex_tensor_role_name(tensor->role),
               tensor->rank);
        print_tensor_dims(tensor->dims, tensor->rank);
        yvex_cli_out_writef(stdout, " dtype=%s bytes=%llu offset=%llu absolute=%llu",
               yvex_dtype_name(tensor->dtype),
               tensor->storage_bytes,
               tensor->relative_offset,
               tensor->absolute_offset);
        if (range_rc == YVEX_OK) {
            yvex_cli_out_writef(stdout, " range=%llu..%llu range_status=valid alignment_status=%s\n",
                   range.tensor_absolute_offset,
                   range.tensor_end_offset,
                   range.aligned ? "valid" : "invalid");
        } else {
            yvex_cli_out_writef(stdout, " range_status=invalid alignment_status=unknown\n");
            yvex_error_clear(&err);
        }
    }

    yvex_tensor_table_close(table);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

int yvex_inspect_command(int arg_count, char **args)
{
    return command_inspect(arg_count, args);
}

int yvex_integrity_command(int arg_count, char **args)
{
    return command_integrity(arg_count, args);
}

int yvex_materialize_command(int arg_count, char **args)
{
    return command_materialize(arg_count, args);
}

int yvex_materialize_gate_command(int arg_count, char **args)
{
    return command_materialize_gate(arg_count, args);
}

int yvex_metadata_command(int arg_count, char **args)
{
    return command_metadata(arg_count, args);
}

int yvex_model_gate_command(int arg_count, char **args)
{
    return command_model_gate(arg_count, args);
}

int yvex_tensors_command(int arg_count, char **args)
{
    return command_tensors(arg_count, args);
}

void yvex_inspect_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex inspect FILE_OR_ALIAS\n\nInspect parses a GGUF descriptor and prints a descriptor-only summary. It does not materialize weights or execute a graph.\n");
}

void yvex_integrity_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N]\n");
    yvex_cli_out_writef(fp, "       yvex integrity report --model FILE_OR_ALIAS [--backend cpu|cuda] [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "\nIntegrity validates local GGUF structure, tensor accounting, digest identity when supplied, metadata drift, and selected embedding readiness. It is not a supply-chain security audit.\n");
    yvex_cli_out_writef(fp, "Default report output is compact. Use --" "audit for full diagnostic fields.\n");
}

void yvex_materialize_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]\n\nMaterialize copies selected GGUF tensor bytes into backend-owned storage after integrity preflight. It does not execute prefill, decode, sampling, generation, or inference.\n");
}

void yvex_materialize_gate_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex materialize-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --scope selected-tensor --expect-tensor NAME --expect-rank N --expect-dims D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--repeat N] [--check-cleanup] [--report-out FILE]\n\nThe materialization gate validates identity, tensor facts, repeated backend materialization, cleanup, and failure classes.\n");
}

void yvex_metadata_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex metadata FILE_OR_ALIAS\n\nMetadata prints parsed GGUF metadata key/value summaries. Arrays are summarized.\n");
}

void yvex_model_gate_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex model-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --expect-tensor NAME --expect-rank N --expect-dims D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--report-out FILE]\n\nModel gate checks selected tensor identity, expected tensor specs, and requested CPU/CUDA materialization without claiming full-model support.\n");
}

void yvex_tensors_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex tensors FILE_OR_ALIAS\n\nTensors prints YVEX tensor table rows with role, dtype, known storage bytes, and checked offsets.\n");
}
