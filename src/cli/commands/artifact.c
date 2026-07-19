/* Owner: CLI artifact command.
 * Owns: artifact argv validation, dispatch, help, and compatibility rendering.
 * Does not own: artifact file lifecycle, identity, integrity truth, or materialization.
 * Invariants: bytes are written only through the CLI IO owner.
 * Boundary: consumes typed artifact APIs and returns process exit status.
 * Purpose: provide artifact argv validation, dispatch, help, and compatibility rendering.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/io/private.h"
#include "src/cli/render/private.h"
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/model.h>
#include <yvex/core.h>
#include <yvex/gguf.h>
#include <yvex/registry.h>
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

static const char *const literal_pair_0[] = {
    "reason: current artifact metadata could not be parsed", "status: models-metadata-drift"};

static const char *const literal_pair_1[] = { "format: gguf", "status: unsupported"};

static const char *const literal_pair_2[] = { "format: unknown", "status: unsupported"};

static const char *const literal_lines_0[] = {
    "metadata_status: fail", "readiness_status: fail", "metadata_issue_0_code: current-metadata-unavailable",
    "metadata_issue_0_registered: available"};

#define MATERIALIZE_FIELD(key_, kind_, member_, fallback_) \
    {key_, kind_, offsetof(yvex_materialize_gate_summary, member_), fallback_}

static const yvex_render_field_spec materialize_gate_heading_fields[] = {
    MATERIALIZE_FIELD("label", YVEX_RENDER_FIELD_TEXT, label, ""),
    MATERIALIZE_FIELD("family", YVEX_RENDER_FIELD_TEXT, family, ""),
};
static const yvex_render_field_spec materialize_gate_state_fields[] = {
    MATERIALIZE_FIELD("model", YVEX_RENDER_FIELD_TEXT, model_path, ""),
    MATERIALIZE_FIELD("expected_sha256", YVEX_RENDER_FIELD_TEXT, expected_sha256, ""),
    MATERIALIZE_FIELD("actual_sha256", YVEX_RENDER_FIELD_TEXT_ARRAY, actual_sha256, ""),
    MATERIALIZE_FIELD("digest_status", YVEX_RENDER_FIELD_TEXT, digest_status, "unrequested"),
    MATERIALIZE_FIELD("identity_status", YVEX_RENDER_FIELD_TEXT, identity_status, "unrequested"),
    MATERIALIZE_FIELD("metadata_status", YVEX_RENDER_FIELD_TEXT, metadata_status, "unregistered"),
    MATERIALIZE_FIELD("materialization_gate", YVEX_RENDER_FIELD_TEXT, materialization_gate, "fail"),
    MATERIALIZE_FIELD("materialization_phase", YVEX_RENDER_FIELD_TEXT,
                      materialization_phase, "preflight"),
    MATERIALIZE_FIELD("integrity_status", YVEX_RENDER_FIELD_TEXT, integrity_status, "unchecked"),
    MATERIALIZE_FIELD("shape_status", YVEX_RENDER_FIELD_TEXT, shape_status, "unchecked"),
    MATERIALIZE_FIELD("range_status", YVEX_RENDER_FIELD_TEXT, range_status, "unchecked"),
    MATERIALIZE_FIELD("backend_status", YVEX_RENDER_FIELD_TEXT, backend_status, "not-opened"),
    MATERIALIZE_FIELD("allocation_attempted", YVEX_RENDER_FIELD_BOOL, allocation_attempted, NULL),
    MATERIALIZE_FIELD("transfer_attempted", YVEX_RENDER_FIELD_BOOL, transfer_attempted, NULL),
    MATERIALIZE_FIELD("cleanup_attempted", YVEX_RENDER_FIELD_BOOL, cleanup_attempted, NULL),
    MATERIALIZE_FIELD("cleanup_status", YVEX_RENDER_FIELD_TEXT, cleanup_status, "not-needed"),
    MATERIALIZE_FIELD("bytes_planned", YVEX_RENDER_FIELD_U64, bytes_planned, NULL),
    MATERIALIZE_FIELD("bytes_allocated", YVEX_RENDER_FIELD_U64, bytes_allocated, NULL),
    MATERIALIZE_FIELD("bytes_transferred", YVEX_RENDER_FIELD_U64, bytes_transferred, NULL),
    MATERIALIZE_FIELD("file_bytes", YVEX_RENDER_FIELD_U64, file_bytes, NULL),
    MATERIALIZE_FIELD("tensor_count", YVEX_RENDER_FIELD_U64, tensor_count, NULL),
    MATERIALIZE_FIELD("expected_tensor_matches", YVEX_RENDER_FIELD_U64,
                      expected_tensor_matches, NULL),
    MATERIALIZE_FIELD("expected_tensor_mismatches", YVEX_RENDER_FIELD_U64,
                      expected_tensor_mismatches, NULL),
    MATERIALIZE_FIELD("bytes_materialized_cpu", YVEX_RENDER_FIELD_U64,
                      bytes_materialized_cpu, NULL),
    MATERIALIZE_FIELD("bytes_materialized_cuda", YVEX_RENDER_FIELD_U64,
                      bytes_materialized_cuda, NULL),
};
#undef MATERIALIZE_FIELD
#define MODEL_GATE_FIELD(key_, kind_, member_, fallback_) \
    {key_, kind_, offsetof(yvex_model_gate_summary, member_), fallback_}

static const yvex_render_field_spec model_gate_state_fields[] = {
    MODEL_GATE_FIELD("label", YVEX_RENDER_FIELD_TEXT, model_label, ""),
    MODEL_GATE_FIELD("family", YVEX_RENDER_FIELD_TEXT, family, ""),
    MODEL_GATE_FIELD("model", YVEX_RENDER_FIELD_TEXT, model_path, ""),
    MODEL_GATE_FIELD("expected_sha256", YVEX_RENDER_FIELD_TEXT, expected_sha256, ""),
    MODEL_GATE_FIELD("actual_sha256", YVEX_RENDER_FIELD_TEXT_ARRAY, actual_sha256, ""),
    MODEL_GATE_FIELD("digest_status", YVEX_RENDER_FIELD_TEXT, digest_status, "unrequested"),
    MODEL_GATE_FIELD("identity_status", YVEX_RENDER_FIELD_TEXT, identity_status, "unrequested"),
    MODEL_GATE_FIELD("file_bytes", YVEX_RENDER_FIELD_U64, file_bytes, NULL),
    MODEL_GATE_FIELD("tensor_count", YVEX_RENDER_FIELD_U64, tensor_count, NULL),
    MODEL_GATE_FIELD("expected_tensor_matches", YVEX_RENDER_FIELD_U64,
                     expected_tensor_matches, NULL),
    MODEL_GATE_FIELD("expected_tensor_mismatches", YVEX_RENDER_FIELD_U64,
                     expected_tensor_mismatches, NULL),
};
#undef MODEL_GATE_FIELD

static int command_integrity_report(int arg_count, char **args);

/* Purpose: Render print integrity report from typed facts (`print_integrity_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
    yvex_cli_out_writef(stdout, "registered_sha256: %s\n",
        report->registered_sha256[0] ? report->registered_sha256 : "absent");
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
/* Purpose: Render print metadata value from typed facts (`print_metadata_value`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
/* Purpose: parse and execute artifact integrity check/report CLI requests.
 * Inputs: Borrowed typed facts.
 * Effects: CLI-local effects only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
        yvex_cli_out_writef(stderr,
            "usage: yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-"
                "embedding] [--partial-token N] | yvex integrity report --model FILE_OR_ALIAS [--backend cpu|"
                "cuda] [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N]\n");
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
            yvex_cli_out_writef(stderr, "Try 'yvex help integrity' for usage.\n");
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
/* Purpose: parse and execute descriptor-only artifact inspection.
 * Inputs: Borrowed typed facts.
 * Effects: CLI-local effects only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
        yvex_cli_out_writef(stderr, "usage: yvex inspect FILE_OR_ALIAS\n");
        return 2;
    }

    rc = open_artifact_for_gguf(args[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_probe_file(artifact, &probe, &err);
    if (rc == YVEX_OK && !probe.is_gguf) {
        yvex_cli_out_lines(stdout, literal_pair_2, sizeof(literal_pair_2) / sizeof(literal_pair_2[0]));
        yvex_artifact_close(artifact);
        return 5;
    }

    if (rc != YVEX_OK) {
        if (rc == YVEX_ERR_UNSUPPORTED) {
            yvex_cli_out_lines(stdout, literal_pair_1, sizeof(literal_pair_1) / sizeof(literal_pair_1[0]));
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
/* Purpose: Orchestrate the typed command metadata request (`command_metadata`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
        yvex_cli_out_writef(stderr, "usage: yvex metadata FILE_OR_ALIAS\n");
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
/* Purpose: Validate enforce registered identity before downstream use (`enforce_registered_identity_cli`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
            yvex_cli_out_writef(stdout, "registered_sha256: %s\n",
                ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
            yvex_cli_out_writef(stdout, "current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
            yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", ref->registered_file_size);
            yvex_cli_out_writef(stdout, "current_file_size: %llu\n", identity.file_size);
            yvex_cli_out_writef(stdout, "digest_status: %s\n", digest_status);
            yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status);
            yvex_cli_out_lines(stdout, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
            yvex_cli_out_writef(stdout, "metadata_issue_0_current: %s\n", yvex_error_message(&err));
            yvex_cli_out_lines(stdout, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
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
            yvex_cli_out_writef(stdout, "registered_sha256: %s\n",
                ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
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
/* Purpose: Render print materialization gate fields from typed facts (`print_materialization_gate_fields`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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

/* Render one admitted materialization summary without changing its state. */
/* Purpose: Render print materialization summary from typed facts (`print_materialization_summary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void print_materialization_summary(const yvex_model_context *ctx,
                                          const yvex_model_ref *model_ref,
                                          const char *backend_name,
                                          const yvex_materialize_summary *summary)
{
    yvex_cli_out_writef(stdout, "materialization status: %s\n",
                        yvex_weight_status_name(summary->status));
    yvex_cli_out_writef(stdout, "model: %s\n",
                        yvex_model_name(ctx->model)[0] ? yvex_model_name(ctx->model) : "unknown");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend_name);
    print_materialization_gate_fields(summary->materialization_gate,
                                      summary->materialization_phase, "pass",
                                      model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                      model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                      summary->shape_status, summary->range_status,
                                      summary->backend_status, summary->allocation_attempted,
                                      summary->transfer_attempted, summary->cleanup_attempted,
                                      summary->cleanup_status, summary->bytes_planned,
                                      summary->bytes_allocated, summary->bytes_transferred);
    yvex_cli_out_writef(stdout, "tensors_total: %llu\n", summary->tensors_total);
    yvex_cli_out_writef(stdout, "tensors_materialized: %llu\n", summary->tensors_materialized);
    yvex_cli_out_writef(stdout, "tensors_failed: %llu\n", summary->tensors_failed);
    yvex_cli_out_writef(stdout, "bytes_total: %llu\n", summary->bytes_total);
    yvex_cli_out_writef(stdout, "bytes_materialized: %llu\n", summary->bytes_materialized);
    yvex_cli_out_writef(stdout, "backend_allocated_bytes: %llu\n", summary->backend_allocated_bytes);
    yvex_cli_out_writef(stdout, "execution_ready: false\n");
    yvex_cli_out_writef(stdout,
                        "reason: graph partial; materialized weights do not imply executable inference\n");
    yvex_cli_out_writef(stdout, "status: %s\n",
                        summary->status == YVEX_WEIGHT_STATUS_MATERIALIZED
                            ? "weights-materialized" : "weights-partial");
}
/* Purpose: parse and execute selected artifact materialization diagnostics.
 * Inputs: Borrowed typed facts.
 * Effects: CLI-local effects only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
            yvex_cli_out_writef(stderr, "Try 'yvex help materialize' for usage.\n");
            return 2;
        }
    }

    if (!model_path || !backend_name) {
        yvex_cli_out_writef(stderr, "yvex: materialize requires --model FILE_OR_ALIAS and --backend cpu|cuda\n");
        yvex_cli_out_writef(stderr,
            "usage: yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-"
                "unsupported-dtype]\n");
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

    print_materialization_summary(&ctx, &model_ref, backend_name, &summary);

    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    yvex_model_context_close(&ctx);
    yvex_model_ref_clear(&model_ref);
    return 0;
}
/* Purpose: Parse parse materialize scope name into typed CLI state (`parse_materialize_scope_name`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static yvex_materialize_scope parse_materialize_scope_name(const char *name)
{
    if (!name) return YVEX_MATERIALIZE_SCOPE_UNKNOWN;
    if (strcmp(name, "selected-tensor") == 0) return YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR;
    if (strcmp(name, "partial-model") == 0) return YVEX_MATERIALIZE_SCOPE_PARTIAL_MODEL;
    if (strcmp(name, "full-model") == 0) return YVEX_MATERIALIZE_SCOPE_FULL_MODEL;
    return YVEX_MATERIALIZE_SCOPE_UNKNOWN;
}

typedef enum gate_cli_kind {
    GATE_CLI_MODEL,
    GATE_CLI_MATERIALIZE
} gate_cli_kind;

typedef struct gate_cli_request {
    const char *model;
    const char *label;
    const char *family;
    const char *sha256;
    const char *tensor_name;
    const char *tensor_dtype;
    const char *report_out;
    unsigned long long tensor_dims[4];
    unsigned long long tensor_bytes;
    unsigned int tensor_rank;
    unsigned int tensor_seen;
    unsigned int repeat_count;
    yvex_materialize_scope scope;
    int require_cpu;
    int require_cuda;
    int check_cpu;
    int check_cuda;
    int check_cleanup;
    int json;
} gate_cli_request;

enum {
    GATE_TENSOR_NAME = 1u << 0u,
    GATE_TENSOR_RANK = 1u << 1u,
    GATE_TENSOR_DIMS = 1u << 2u,
    GATE_TENSOR_DTYPE = 1u << 3u,
    GATE_TENSOR_BYTES = 1u << 4u,
    GATE_TENSOR_COMPLETE = (1u << 5u) - 1u
};

/* Parse the common model/materialization gate grammar into one CLI-only record. */
/* Purpose: Parse gate parse into typed CLI state (`gate_cli_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int gate_cli_parse(int arg_count,
                          char **args,
                          gate_cli_kind kind,
                          gate_cli_request *request)
{
    const char *surface = kind == GATE_CLI_MATERIALIZE ? "materialize-gate" : "model-gate";
    int i;

    memset(request, 0, sizeof(*request));
    request->scope = YVEX_MATERIALIZE_SCOPE_UNKNOWN;
    request->repeat_count = 1u;
    for (i = 3; i < arg_count; ++i) {
        const char *option = args[i];
        const char *value;
        unsigned long long parsed;

        if (strcmp(option, "--require-cpu") == 0) {
            request->require_cpu = request->check_cpu = 1;
            continue;
        }
        if (strcmp(option, "--require-cuda") == 0) {
            request->require_cuda = request->check_cuda = 1;
            continue;
        }
        if (kind == GATE_CLI_MATERIALIZE && strcmp(option, "--check-cleanup") == 0) {
            request->check_cleanup = 1;
            continue;
        }
        if (kind == GATE_CLI_MATERIALIZE && strcmp(option, "--json") == 0) {
            request->json = 1;
            continue;
        }
        if (i + 1 >= arg_count) {
            yvex_cli_out_writef(stderr, "yvex: %s option requires a value: %s\n",
                                surface, option);
            return 2;
        }
        value = args[++i];
        if (strcmp(option, "--model") == 0) request->model = value;
        else if (strcmp(option, "--label") == 0) request->label = value;
        else if (strcmp(option, "--family") == 0) request->family = value;
        else if (strcmp(option, "--sha256") == 0) request->sha256 = value;
        else if (strcmp(option, "--expect-tensor") == 0) {
            request->tensor_name = value;
            request->tensor_seen |= GATE_TENSOR_NAME;
        } else if (strcmp(option, "--expect-rank") == 0) {
            if (!parse_positive_ull(value, &parsed) || parsed > 4ull) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-rank\n");
                return 2;
            }
            request->tensor_rank = (unsigned int)parsed;
            request->tensor_seen |= GATE_TENSOR_RANK;
        } else if (strcmp(option, "--expect-dims") == 0) {
            if (!(request->tensor_seen & GATE_TENSOR_RANK) ||
                !parse_dims_csv(value, request->tensor_rank, request->tensor_dims)) {
                yvex_cli_out_writef(
                    stderr,
                    "yvex: invalid --expect-dims; pass --expect-rank before --expect-dims\n");
                return 2;
            }
            request->tensor_seen |= GATE_TENSOR_DIMS;
        } else if (strcmp(option, "--expect-dtype") == 0) {
            request->tensor_dtype = value;
            request->tensor_seen |= GATE_TENSOR_DTYPE;
        } else if (strcmp(option, "--expect-bytes") == 0) {
            if (!parse_positive_ull(value, &request->tensor_bytes)) {
                yvex_cli_out_writef(stderr, "yvex: invalid --expect-bytes\n");
                return 2;
            }
            request->tensor_seen |= GATE_TENSOR_BYTES;
        } else if (strcmp(option, "--backend") == 0) {
            if (strcmp(value, "cpu") == 0) request->check_cpu = 1;
            else if (strcmp(value, "cuda") == 0) request->check_cuda = 1;
            else {
                yvex_cli_out_writef(stderr, "yvex: %s backend must be cpu or cuda\n", surface);
                return 2;
            }
        } else if (strcmp(option, "--report-out") == 0) {
            request->report_out = value;
        } else if (kind == GATE_CLI_MATERIALIZE && strcmp(option, "--scope") == 0) {
            request->scope = parse_materialize_scope_name(value);
            if (request->scope == YVEX_MATERIALIZE_SCOPE_UNKNOWN) {
                yvex_cli_out_writef(stderr, "yvex: invalid materialize-gate scope\n");
                return 2;
            }
        } else if (kind == GATE_CLI_MATERIALIZE && strcmp(option, "--repeat") == 0) {
            if (!parse_positive_ull(value, &parsed) || parsed > 1000ull) {
                yvex_cli_out_writef(stderr, "yvex: invalid --repeat\n");
                return 2;
            }
            request->repeat_count = (unsigned int)parsed;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown %s option: %s\n", surface, option);
            yvex_cli_out_writef(stderr, "Try 'yvex help %s' for usage.\n", surface);
            return 2;
        }
    }
    return 0;
}
/* Purpose: Render print materialize gate report from typed facts (`print_materialize_gate_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void print_materialize_gate_report(FILE *fp,
                                          const yvex_materialize_gate_options *options,
                                          const yvex_materialize_gate_summary *summary,
                                          const char *reason)
{
    yvex_cli_out_writef(fp, "materialize gate: check\n");
    render_object_fields(fp, summary, materialize_gate_heading_fields,
                         sizeof(materialize_gate_heading_fields) /
                             sizeof(materialize_gate_heading_fields[0]));
    yvex_cli_out_writef(fp, "scope: %s\n", yvex_materialize_scope_name(summary->scope));
    render_object_fields(fp, summary, materialize_gate_state_fields,
                         sizeof(materialize_gate_state_fields) /
                             sizeof(materialize_gate_state_fields[0]));
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
/* Purpose: Orchestrate the typed command materialize gate request (`command_materialize_gate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_materialize_gate(int arg_count, char **args)
{
    yvex_materialize_gate_options options;
    yvex_materialize_expected_tensor expected;
    yvex_materialize_gate_summary summary;
    gate_cli_request request;
    yvex_model_ref model_ref;
    yvex_error err;
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

    rc = gate_cli_parse(arg_count, args, GATE_CLI_MATERIALIZE, &request);
    if (rc != 0) return rc;
    options.model_path = request.model;
    options.label = request.label;
    options.family = request.family;
    options.sha256 = request.sha256;
    options.scope = request.scope;
    options.require_cpu = request.require_cpu;
    options.require_cuda = request.require_cuda;
    options.check_cpu = request.check_cpu;
    options.check_cuda = request.check_cuda;
    options.check_cleanup = request.check_cleanup;
    options.json = request.json;
    options.repeat_count = request.repeat_count;
    if (!request.model || !request.label || !request.family ||
        request.scope == YVEX_MATERIALIZE_SCOPE_UNKNOWN) {
        yvex_cli_out_writef(stderr, "yvex: materialize-gate check requires --model --label --family --scope\n");
        return 2;
    }
    if (request.tensor_seen != 0u) {
        if (request.tensor_seen != GATE_TENSOR_COMPLETE) {
            yvex_cli_out_writef(stderr, "yvex: materialize-gate expected tensor spec must be complete\n");
            return 2;
        }
        expected.name = request.tensor_name;
        expected.rank = request.tensor_rank;
        memcpy(expected.dims, request.tensor_dims, sizeof(expected.dims));
        expected.dtype = request.tensor_dtype;
        expected.bytes = request.tensor_bytes;
        options.expected_tensors = &expected;
        options.expected_tensor_count = 1;
    }

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
    if (request.report_out) {
        FILE *fp = fopen(request.report_out, "wb");
        if (!fp) {
            yvex_cli_out_writef(stderr, "yvex: cannot write report: %s\n", request.report_out);
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
/* Purpose: Render print model gate report from typed facts (`print_model_gate_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void print_model_gate_report(FILE *fp,
                                    const yvex_model_gate_options *options,
                                    const yvex_model_gate_summary *summary,
                                    const char *reason)
{
    yvex_cli_out_writef(fp, "model gate: check\n");
    render_object_fields(fp, summary, model_gate_state_fields,
                         sizeof(model_gate_state_fields) /
                             sizeof(model_gate_state_fields[0]));
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
/* Purpose: Orchestrate the typed command model gate request (`command_model_gate`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_model_gate(int arg_count, char **args)
{
    yvex_model_gate_options options;
    yvex_model_gate_expected_tensor expected;
    yvex_model_gate_summary summary;
    gate_cli_request request;
    yvex_model_ref model_ref;
    yvex_error err;
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

    rc = gate_cli_parse(arg_count, args, GATE_CLI_MODEL, &request);
    if (rc != 0) return rc;
    if (!request.model || !request.label || !request.family ||
        request.tensor_seen != GATE_TENSOR_COMPLETE) {
        yvex_cli_out_writef(stderr,
            "yvex: model-gate check requires --model --label --family and one complete --expect-* tensor spec\n");
        return 2;
    }
    options.model_path = request.model;
    options.model_label = request.label;
    options.family = request.family;
    options.artifact_sha256 = request.sha256;
    options.require_cpu = request.require_cpu;
    options.require_cuda = request.require_cuda;
    options.check_cpu = request.check_cpu;
    options.check_cuda = request.check_cuda;
    expected.name = request.tensor_name;
    expected.rank = request.tensor_rank;
    memcpy(expected.dims, request.tensor_dims, sizeof(expected.dims));
    expected.dtype = request.tensor_dtype;
    expected.bytes = request.tensor_bytes;
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
    if (request.report_out) {
        FILE *fp = fopen(request.report_out, "wb");
        if (!fp) {
            yvex_cli_out_writef(stderr, "yvex: cannot write report: %s\n", request.report_out);
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

typedef struct {
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_model_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_cli_graph_guard_report graph_report;
    yvex_backend_options backend_options;
    yvex_model_ref ref;
    const char *model_arg;
    const char *backend_name;
    const char *identity_status;
    const char *metadata_status;
    const char *readiness_status;
    const char *support_level;
    const char *materialization_preflight;
    const char *materialization_gate;
    const char *materialization_backend;
    const char *backend_status;
    const char *graph_fixture_guard;
    const char *graph_partial_guard;
    const char *graph_partial_backend;
    const char *graph_partial_dispatch_ready;
    const char *graph_partial_reference_ready;
    const char *report_status;
    const char *status;
    const char *model_input_kind;
    unsigned long long selected_hidden_size;
    unsigned long long selected_vocab_size;
    unsigned long long selected_output_count;
    unsigned long long selected_output_bytes;
    unsigned long long selected_slice_bytes;
    int audit_output;
    int metadata_checked;
    int selected_ready;
    int hard_fail;
} integrity_cli_state;

/* Initialize explicit report states before any file or backend is admitted. */
/* Purpose: Construct the owned integrity state init state (`integrity_cli_state_init`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void integrity_cli_state_init(integrity_cli_state *state)
{
    memset(state, 0, sizeof(*state));
    state->identity_status = "unregistered";
    state->metadata_status = "unregistered";
    state->readiness_status = "not-checked";
    state->support_level = "not-checked";
    state->materialization_preflight = "not-checked";
    state->materialization_gate = "not-checked";
    state->materialization_backend = "not-checked";
    state->backend_status = "not-checked";
    state->graph_fixture_guard = "not-applicable";
    state->graph_partial_guard = "not-checked";
    state->graph_partial_backend = "not-checked";
    state->graph_partial_dispatch_ready = "false";
    state->graph_partial_reference_ready = "false";
    state->report_status = "pass";
    state->status = "integrity-report-pass";
}

/* Parse integrity-report options without reading the artifact. */
/* Purpose: Parse integrity parse into typed CLI state (`integrity_cli_parse`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int integrity_cli_parse(int arg_count, char **args, integrity_cli_state *state)
{
    int i;

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--model") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            state->model_arg = args[++i];
        } else if (strcmp(args[i], "--backend") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            state->backend_name = args[++i];
        } else if (strcmp(args[i], "--expect-sha256") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: --expect-sha256 requires HASH\n");
                return 2;
            }
            state->integrity_options.expect_sha256 = args[++i];
        } else if (strcmp(args[i], "--require-token-embedding") == 0) {
            state->integrity_options.require_token_embedding = 1;
        } else if (strcmp(args[i], "--partial-token") == 0) {
            if (i + 1 >= arg_count || !parse_uint_allow_zero(args[i + 1],
                                                        &state->integrity_options.token_id)) {
                yvex_cli_out_writef(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            state->integrity_options.require_token_embedding = 1;
            i += 1;
        } else if (strcmp(args[i], "--audit") == 0) {
            state->audit_output = 1;
        } else if (strcmp(args[i], "--output") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: integrity report --output requires normal, table, or audit\n");
                return 2;
            }
            i += 1;
            if (strcmp(args[i], "normal") == 0 || strcmp(args[i], "table") == 0) {
                state->audit_output = 0;
            } else if (strcmp(args[i], "audit") == 0) {
                state->audit_output = 1;
            } else {
                yvex_cli_out_writef(stderr, "yvex: integrity report unsupported output mode: %s\n", args[i]);
                return 2;
            }
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown integrity report option: %s\n", args[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help integrity' for usage.\n");
            return 2;
        }
    }

    if (!state->model_arg) {
        yvex_cli_out_writef(stderr, "yvex: integrity report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (state->backend_name && strcmp(state->backend_name, "cpu") != 0 &&
        strcmp(state->backend_name, "cuda") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", state->backend_name);
        return 2;
    }
    return 0;
}

/* Bind registered identity and metadata drift to the immutable file snapshot. */
/* Purpose: Compute integrity metadata for its CLI invariant (`integrity_cli_metadata`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void integrity_cli_metadata(integrity_cli_state *state)
{
    yvex_error metadata_err;

    state->model_input_kind = state->ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path";
    if (state->ref.kind == YVEX_MODEL_REF_ALIAS) {
        if (!state->ref.sha256 || !state->ref.sha256[0] ||
            !yvex_sha256_hex_is_valid(state->ref.sha256)) {
            state->identity_status = "missing";
            state->hard_fail = 1;
        } else if (strcmp(state->integrity_report.digest_status, "pass") == 0 &&
                   (state->ref.registered_file_size == 0ull ||
                    state->ref.registered_file_size == state->integrity_report.file_size)) {
            state->identity_status = "pass";
        } else {
            state->identity_status = "fail";
            state->hard_fail = 1;
        }
    }
    if (!state->integrity_report.passed) return;
    yvex_error_clear(&metadata_err);
    if (yvex_model_metadata_snapshot_read(&state->current_metadata, state->ref.path,
                                          &metadata_err) != YVEX_OK) {
        if (state->ref.kind == YVEX_MODEL_REF_ALIAS &&
            strcmp(state->identity_status, "pass") == 0) {
            state->metadata_status = "fail";
            state->readiness_status = "fail";
            state->hard_fail = 1;
        }
        yvex_error_clear(&metadata_err);
        return;
    }
    state->metadata_checked = 1;
    state->support_level = state->current_metadata.entry.support_level &&
        state->current_metadata.entry.support_level[0]
            ? state->current_metadata.entry.support_level : "not-checked";
    state->selected_ready = state->current_metadata.entry.selected_embedding_ready;
    state->selected_hidden_size = state->current_metadata.entry.selected_embedding_hidden_size;
    state->selected_vocab_size = state->current_metadata.entry.selected_embedding_vocab_size;
    state->selected_output_count = state->current_metadata.entry.selected_embedding_output_count;
    state->selected_slice_bytes = state->current_metadata.entry.selected_embedding_slice_bytes;
    state->selected_output_bytes = state->selected_output_count * sizeof(float);
    if (state->ref.kind == YVEX_MODEL_REF_ALIAS &&
        strcmp(state->identity_status, "pass") == 0) {
        yvex_model_ref_registry_entry_view(&state->ref, &state->registered_metadata);
        if (yvex_model_registry_compare_metadata(&state->registered_metadata,
                &state->current_metadata.entry, &state->metadata_report,
                &metadata_err) == YVEX_OK) {
            state->metadata_status = state->metadata_report.metadata_status[0]
                ? state->metadata_report.metadata_status : "unknown";
            state->readiness_status = state->metadata_report.readiness_status[0]
                ? state->metadata_report.readiness_status : "unknown";
            state->support_level = state->ref.support_level && state->ref.support_level[0]
                ? state->ref.support_level : state->support_level;
            if (strcmp(state->metadata_status, "pass") != 0 ||
                strcmp(state->readiness_status, "pass") != 0) state->hard_fail = 1;
        } else {
            state->metadata_status = "fail";
            state->readiness_status = "fail";
            state->hard_fail = 1;
        }
    } else if (state->ref.kind == YVEX_MODEL_REF_ALIAS) {
        state->metadata_status = "not-checked";
        state->readiness_status = "not-checked";
    } else {
        state->metadata_status = "unregistered";
        state->readiness_status = state->selected_ready ? "pass" : "not-checked";
    }
}

/* Prefer exact selected-embedding facts produced by the integrity owner. */
/* Purpose: Compute integrity selected shape for its CLI invariant (`integrity_cli_selected_shape`). */
static void integrity_cli_selected_shape(integrity_cli_state *state)
{
    if (state->integrity_report.selected_embedding_shape[0]) {
        state->selected_ready =
            strcmp(state->integrity_report.selected_embedding_shape, "valid") == 0;
        state->selected_hidden_size = state->integrity_report.selected_embedding_hidden_size;
        state->selected_vocab_size = state->integrity_report.selected_embedding_vocab_size;
        state->selected_output_count = state->integrity_report.selected_embedding_output_count;
        state->selected_output_bytes = state->integrity_report.selected_embedding_output_bytes;
        state->selected_slice_bytes = state->integrity_report.selected_embedding_slice_bytes;
        state->readiness_status = state->selected_ready ? "pass" : "fail";
    } else if (state->integrity_options.require_token_embedding) {
        state->selected_ready = 0;
        state->readiness_status = "fail";
        state->hard_fail = 1;
    }
}

/* Probe optional backend/materialization and graph boundaries without execution claims. */
/* Purpose: Compute integrity backend for its CLI invariant (`integrity_cli_backend`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void integrity_cli_backend(integrity_cli_state *state, yvex_error *err)
{
    yvex_backend *backend = NULL;
    int rc;

    if (!state->backend_name) return;
    state->materialization_backend = state->backend_name;
    state->graph_partial_backend = state->backend_name;
    if (!state->hard_fail) {
        state->backend_options.kind = strcmp(state->backend_name, "cuda") == 0
            ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU;
        rc = yvex_backend_open(&backend, &state->backend_options, err);
        if (rc == YVEX_OK) {
            state->backend_status = yvex_backend_status_name(yvex_backend_status_of(backend));
            if (yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) &&
                yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE)) {
                state->materialization_preflight = "pass";
                state->materialization_gate = "pass";
            } else {
                state->materialization_preflight = "fail";
                state->materialization_gate = "fail";
                state->hard_fail = 1;
            }
            yvex_backend_close(backend);
        } else {
            state->backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
            state->materialization_preflight = "fail";
            state->materialization_gate = "fail";
            state->hard_fail = 1;
            yvex_error_clear(err);
        }
    } else {
        state->backend_status = "not-opened";
        state->materialization_preflight = "fail";
        state->materialization_gate = "fail";
    }
    if (!state->hard_fail && state->selected_ready) {
        yvex_error graph_err;
        yvex_error_clear(&graph_err);
        rc = yvex_graph_preflight(&state->ref, state->backend_name, 0, 0,
            state->integrity_options.token_id, &state->graph_report, &graph_err);
        if (rc == YVEX_OK && strcmp(state->graph_report.guard_status, "pass") == 0) {
            state->graph_partial_guard = "pass";
            state->graph_partial_dispatch_ready = "true";
            state->graph_partial_reference_ready = "true";
        } else {
            state->graph_partial_guard = "fail";
            state->hard_fail = 1;
            yvex_error_clear(&graph_err);
        }
    } else if (!state->selected_ready && state->integrity_options.require_token_embedding) {
        state->graph_partial_guard = "fail";
    } else if (!state->selected_ready) {
        state->graph_partial_guard = "not-applicable";
    }
}

/* Render the compact integrity surface and one highest-priority blocker. */
/* Purpose: Render integrity render normal from typed facts (`integrity_cli_render_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int integrity_cli_render_normal(integrity_cli_state *state)
{
    const char *top_blocker = "none";

    if (state->hard_fail) {
        if (state->integrity_report.error_count > 0u &&
            state->integrity_report.issues[0].code[0]) {
            top_blocker = state->integrity_report.issues[0].code;
        } else if (strcmp(state->identity_status, "pass") != 0) {
            top_blocker = "identity";
        } else if (strcmp(state->metadata_status, "pass") != 0 &&
                   strcmp(state->metadata_status, "unregistered") != 0) {
            top_blocker = "metadata";
        } else if (strcmp(state->materialization_preflight, "pass") != 0 &&
                   strcmp(state->materialization_preflight, "not-checked") != 0) {
            top_blocker = "materialization-preflight";
        } else {
            top_blocker = "integrity";
        }
    }
    yvex_cli_out_writef(stdout, "integrity: %s model=%s backend=%s\n",
        state->hard_fail ? "fail" : "pass", state->model_arg,
        state->backend_name ? state->backend_name : "not-requested");
    yvex_cli_out_writef(stdout, "identity: %s digest: %s tensors=%llu invalid_ranges=%llu\n",
        state->identity_status,
        state->integrity_report.digest_status[0] ? state->integrity_report.digest_status : "unknown",
        state->integrity_report.tensor_count, state->integrity_report.tensor_ranges_invalid);
    yvex_cli_out_writef(stdout, "materialization_preflight: %s\n",
                        state->materialization_preflight);
    yvex_cli_out_writef(stdout, "top_blocker: %s\n", top_blocker);
    yvex_cli_out_writef(stdout, "boundary: integrity gate only, generation unsupported\n");
    yvex_cli_out_writef(stdout, "status: %s\n", state->status);
    return state->hard_fail ? exit_for_status(YVEX_ERR_STATE) : 0;
}

/* Render the complete audit projection from typed integrity state. */
/* Purpose: Render integrity render audit from typed facts (`integrity_cli_render_audit`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int integrity_cli_render_audit(integrity_cli_state *state)
{
    const yvex_artifact_integrity_report *report = &state->integrity_report;
    int i;

    yvex_cli_out_writef(stdout, "artifact_integrity_report: summary\n");
    yvex_cli_out_writef(stdout, "model: %s\nresolved_path: %s\nmodel_input_kind: %s\n",
        state->model_arg, state->ref.path ? state->ref.path : "", state->model_input_kind);
    yvex_cli_out_writef(stdout, "format: %s\n", report->format[0] ? report->format : "unknown");
    if (report->version) yvex_cli_out_writef(stdout, "version: %u\n", report->version);
    yvex_cli_out_writef(stdout, "architecture: %s\nidentity_status: %s\ndigest_status: %s\n",
        report->architecture[0] ? report->architecture : "unknown", state->identity_status,
        report->digest_status[0] ? report->digest_status : "unknown");
    yvex_cli_out_writef(stdout, "sha256: %s\nregistered_sha256: %s\n",
        report->sha256[0] ? report->sha256 : "unavailable",
        report->registered_sha256[0] ? report->registered_sha256 : "absent");
    if (report->expected_sha256[0]) {
        yvex_cli_out_writef(stdout, "expected_sha256: %s\nactual_sha256: %s\n",
            report->expected_sha256, report->sha256[0] ? report->sha256 : "unavailable");
    }
    yvex_cli_out_writef(stdout,
        "metadata_status: %s\nreadiness_status: %s\nsupport_level: %s\nintegrity_status: %s\n",
        state->metadata_status, state->readiness_status, state->support_level,
        report->passed ? "pass" : "fail");
    yvex_cli_out_writef(stdout, "integrity_errors: %u\nintegrity_warnings: %u\n",
                        report->error_count, report->warning_count);
    yvex_cli_out_writef(stdout,
        "tensor_count: %llu\nknown_tensor_bytes: %llu\ntensor_ranges_checked: %llu\ntensor_ranges_invalid: "
            "%llu\ntensor_shapes_checked: %llu\ntensor_shapes_invalid: %llu\ntensor_dtypes_checked: %llu\n"
            "tensor_dtypes_invalid: %llu\ntensor_byte_counts_checked: %llu\ntensor_byte_counts_invalid: %llu\n",
        report->tensor_count, report->known_tensor_bytes, report->tensor_ranges_checked,
        report->tensor_ranges_invalid, report->tensor_shapes_checked, report->tensor_shapes_invalid,
        report->tensor_dtypes_checked, report->tensor_dtypes_invalid,
        report->tensor_byte_counts_checked, report->tensor_byte_counts_invalid);
    yvex_cli_out_writef(stdout, "primary_tensor: %s\nprimary_tensor_role: %s\nprimary_tensor_dtype: %s\n",
        state->metadata_checked && state->current_metadata.entry.primary_tensor_name
            ? state->current_metadata.entry.primary_tensor_name : "",
        state->metadata_checked && state->current_metadata.entry.primary_tensor_role
            ? state->current_metadata.entry.primary_tensor_role : "",
        state->metadata_checked && state->current_metadata.entry.primary_tensor_dtype
            ? state->current_metadata.entry.primary_tensor_dtype : "");
    yvex_cli_out_writef(stdout, "primary_tensor_rank: %u\nprimary_tensor_dims: %s\nprimary_tensor_bytes: %llu\n",
        state->metadata_checked ? state->current_metadata.entry.primary_tensor_rank : 0u,
        state->metadata_checked && state->current_metadata.entry.primary_tensor_dims
            ? state->current_metadata.entry.primary_tensor_dims : "",
        state->metadata_checked ? state->current_metadata.entry.primary_tensor_bytes : 0ull);
    yvex_cli_out_writef(stdout,
        "selected_embedding_ready: %s\nselected_embedding_hidden_size: %llu\nselected_embedding_vocab_size:"
            " %llu\nselected_embedding_output_count: %llu\nselected_embedding_output_bytes: %llu\n"
            "selected_embedding_slice_bytes: %llu\n",
        state->selected_ready ? "true" : "false", state->selected_hidden_size,
        state->selected_vocab_size, state->selected_output_count,
        state->selected_output_bytes, state->selected_slice_bytes);
    yvex_cli_out_writef(stdout,
        "backend_status: %s\nmaterialization_preflight: %s\nmaterialization_backend: %s\n"
            "materialization_gate: %s\nallocation_required_bytes: %llu\n",
        state->backend_status, state->materialization_preflight,
        state->materialization_backend, state->materialization_gate,
        report->known_tensor_bytes);
    yvex_cli_out_writef(stdout,
        "graph_fixture_guard: %s\ngraph_partial_guard: %s\ngraph_partial_backend: %s\ngraph_partial_token: "
            "%u\ngraph_partial_dispatch_ready: %s\ngraph_partial_reference_ready: %s\n",
        state->graph_fixture_guard, state->graph_partial_guard, state->graph_partial_backend,
        state->integrity_options.token_id, state->graph_partial_dispatch_ready,
        state->graph_partial_reference_ready);
    if (state->metadata_checked && state->ref.kind == YVEX_MODEL_REF_ALIAS) {
        for (i = 0; i < (int)state->metadata_report.issue_count; ++i) {
            yvex_cli_out_writef(stdout, "metadata_issue_%u_code: %s\n",
                                (unsigned int)i, state->metadata_report.issues[i].code);
            yvex_cli_out_writef(stdout, "metadata_issue_%u_registered: %s\n",
                (unsigned int)i, state->metadata_report.issues[i].registered_value);
            yvex_cli_out_writef(stdout, "metadata_issue_%u_current: %s\n",
                (unsigned int)i, state->metadata_report.issues[i].current_value);
        }
    }
    yvex_cli_out_writef(stdout,
        "execution_ready: false\nprefill_ready: false\nlogits_ready: false\ngeneration: unsupported\n"
            "report_status: %s\nstatus: %s\n",
        state->report_status, state->status);
    return state->hard_fail ? exit_for_status(YVEX_ERR_STATE) : 0;
}
/* Purpose: Orchestrate the typed command integrity report request (`command_integrity_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int command_integrity_report(int arg_count, char **args)
{
    integrity_cli_state state;
    yvex_error err;
    int rc;

    integrity_cli_state_init(&state);
    yvex_error_clear(&err);
    rc = integrity_cli_parse(arg_count, args, &state);
    if (rc != 0) return rc;

    rc = yvex_model_ref_resolve(&state.ref, state.model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (state.ref.kind == YVEX_MODEL_REF_ALIAS && state.ref.sha256 && state.ref.sha256[0]) {
        state.integrity_options.registered_sha256 = state.ref.sha256;
    }
    rc = yvex_artifact_integrity_check_path(state.ref.path, &state.integrity_options,
                                            &state.integrity_report, &err);
    (void)rc;
    if (!state.integrity_report.passed) state.hard_fail = 1;
    integrity_cli_metadata(&state);
    integrity_cli_selected_shape(&state);
    integrity_cli_backend(&state, &err);
    if (state.hard_fail) {
        state.report_status = "fail";
        state.status = "integrity-report-fail";
    }
    rc = state.audit_output ? integrity_cli_render_audit(&state)
                            : integrity_cli_render_normal(&state);
    yvex_model_ref_clear(&state.ref);
    return rc;
}
/* Purpose: Orchestrate the typed command tensors request (`command_tensors`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
        yvex_cli_out_writef(stderr, "usage: yvex tensors FILE_OR_ALIAS\n");
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
/* Purpose: Orchestrate the typed inspect command request (`yvex_inspect_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_inspect_command(int arg_count, char **args)
{
    return command_inspect(arg_count, args);
}
/* Purpose: Orchestrate the typed integrity command request (`yvex_integrity_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_integrity_command(int arg_count, char **args)
{
    return command_integrity(arg_count, args);
}
/* Purpose: Orchestrate the typed materialize command request (`yvex_materialize_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_materialize_command(int arg_count, char **args)
{
    return command_materialize(arg_count, args);
}
/* Purpose: Orchestrate the typed materialize gate command request (`yvex_materialize_gate_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_materialize_gate_command(int arg_count, char **args)
{
    return command_materialize_gate(arg_count, args);
}
/* Purpose: Orchestrate the typed metadata command request (`yvex_metadata_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_metadata_command(int arg_count, char **args)
{
    return command_metadata(arg_count, args);
}
/* Purpose: Orchestrate the typed model gate command request (`yvex_model_gate_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_model_gate_command(int arg_count, char **args)
{
    return command_model_gate(arg_count, args);
}
/* Purpose: Orchestrate the typed tensors command request (`yvex_tensors_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_tensors_command(int arg_count, char **args)
{
    return command_tensors(arg_count, args);
}
/* Purpose: Render inspect help from typed facts (`yvex_inspect_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_inspect_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex inspect FILE_OR_ALIAS\n\nInspect parses a GGUF descriptor and prints a descriptor-"
            "only summary. It does not materialize weights or execute a graph.\n");
}
/* Purpose: Render integrity help from typed facts (`yvex_integrity_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_integrity_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-"
            "embedding] [--partial-token N]\n");
    yvex_cli_out_writef(fp,
        "       yvex integrity report --model FILE_OR_ALIAS [--backend cpu|cuda] [--expect-sha256 HASH] [--"
            "require-token-embedding] [--partial-token N] [--audit | --output normal|table|audit]\n");
    yvex_cli_out_writef(fp,
        "\nIntegrity validates local GGUF structure, tensor accounting, digest identity when supplied, "
            "metadata drift, and selected embedding readiness. It is not a supply-chain security audit.\n");
    yvex_cli_out_writef(fp, "Default report output is compact. Use --audit for full diagnostic fields.\n");
}
/* Purpose: Render materialize help from typed facts (`yvex_materialize_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_materialize_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-"
            "unsupported-dtype]\n\nMaterialize copies selected GGUF tensor bytes into backend-owned storage "
            "after integrity preflight. It does not execute prefill, decode, sampling, generation, or "
            "inference.\n");
}
/* Purpose: Render materialize gate help from typed facts (`yvex_materialize_gate_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_materialize_gate_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex materialize-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --scope "
            "selected-tensor --expect-tensor NAME --expect-rank N --expect-dims D1[,D2,D3] --expect-dtype "
            "DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--"
            "require-cuda] [--repeat N] [--check-cleanup] [--report-out FILE]\n\nThe materialization gate "
            "validates identity, tensor facts, repeated backend materialization, cleanup, and failure classes.\n"
            "");
}
/* Purpose: Render metadata help from typed facts (`yvex_metadata_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_metadata_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex metadata FILE_OR_ALIAS\n\nMetadata prints parsed GGUF metadata key/value summaries. "
            "Arrays are summarized.\n");
}
/* Purpose: Render model gate help from typed facts (`yvex_model_gate_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_model_gate_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex model-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --expect-tensor "
            "NAME --expect-rank N --expect-dims D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 "
            "HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--report-out FILE]\n\n"
            "Model gate checks selected tensor identity, expected tensor specs, and requested CPU/CUDA "
            "materialization without claiming full-model support.\n");
}
/* Purpose: Render tensors help from typed facts (`yvex_tensors_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_tensors_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex tensors FILE_OR_ALIAS\n\nTensors prints YVEX tensor table rows with role, dtype, "
            "known storage bytes, and checked offsets.\n");
}
