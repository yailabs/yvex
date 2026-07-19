/* Owner: src/model/target
 * Owns: unified model-target report dispatch, request routing, and shared report cleanup entry points.
 * Does not own: target catalogs, target decisions, candidate facts, class profiles, tensor collection, tensor
 *   naming, output-head maps, tokenizer maps, missing-role facts, mapping gates, qtype facts, sidecar
 *   file writing, CLI parsing, command dispatch, rendering, stdout/stderr byte emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 * Invariants: the coordinator routes typed requests to specialized model-target modules; it does not render, open
 *   operator streams, or contain report-specific static catalogs.
 * Boundary: model-target reports are report-only facts. This coordinator does not implement quantization, artifact
 *   emission, runtime execution, generation, eval, benchmark, throughput, or release readiness.
 * Purpose: coordinate model-target report routing and shared bounded evidence probes.
 * Inputs: typed requests, report outputs, and explicit source locations.
 * Effects: routes report owners and opens only temporary bounded evidence views.
 * Failure: invalid requests or evidence failures preserve deterministic report state. */
#include <yvex/internal/model_target.h>

#include <yvex/internal/families/deepseek_v4.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <yvex/source.h>

#define MODEL_TARGET_HEADER_CAP (1024ull * 1024ull)

static const char *const output_contract_tail[] = {
    "runtime_claim: unsupported",
    "generation: unsupported-full-model",
    "benchmark_status: not-measured",
    "release_ready: false",
    "boundary: output-contract check only; no runtime/generation claim"
};

typedef struct {
    size_t report_offset;
    size_t profile_offset;
    size_t capacity;
} report_profile_field;

#define REPORT_PROFILE_FIELD(member) \
    { offsetof(yvex_model_target_report, member), \
      offsetof(yvex_model_target_report_profile, member), \
      sizeof(((yvex_model_target_report *)0)->member) }

static const report_profile_field report_profile_fields[] = {
    REPORT_PROFILE_FIELD(target_id),
    REPORT_PROFILE_FIELD(family),
    REPORT_PROFILE_FIELD(model),
    REPORT_PROFILE_FIELD(target_class),
    REPORT_PROFILE_FIELD(stage),
    REPORT_PROFILE_FIELD(eligibility),
    REPORT_PROFILE_FIELD(source_status),
    REPORT_PROFILE_FIELD(artifact_status),
    REPORT_PROFILE_FIELD(tensor_map_status),
    REPORT_PROFILE_FIELD(qtype_policy_status),
    REPORT_PROFILE_FIELD(runtime_status),
    REPORT_PROFILE_FIELD(generation_status),
    REPORT_PROFILE_FIELD(benchmark_status),
    REPORT_PROFILE_FIELD(next_row),
    REPORT_PROFILE_FIELD(boundary),
    REPORT_PROFILE_FIELD(reason)
};

#undef REPORT_PROFILE_FIELD

/* Purpose: initialize the shared report envelope from one typed immutable profile.
 * Inputs: report is mutable; request/profile are borrowed for the duration of the call.
 * Effects: copies only explicitly supplied profile fields into bounded report storage.
 * Failure: invalid inputs are ignored and string truncation remains bounded and terminated.
 * Boundary: envelope initialization records supplied facts and derives no capability state. */
void yvex_model_target_report_prepare(
    yvex_model_target_report *report,
    const yvex_model_target_request *request,
    const yvex_model_target_report_profile *profile)
{
    const unsigned char *source = (const unsigned char *)profile;
    unsigned char *destination = (unsigned char *)report;
    size_t index;

    if (!report || !request || !profile) return;
    report->kind = request->kind;
    report->mode = request->mode;
    report->status = profile->status;
    report->exit_code = 0;
    for (index = 0u; index < sizeof(report_profile_fields) /
                                 sizeof(report_profile_fields[0]); ++index) {
        const report_profile_field *field = &report_profile_fields[index];
        const char *value = *(const char *const *)(source + field->profile_offset);

        if (value) {
            (void)snprintf((char *)(destination + field->report_offset),
                           field->capacity, "%s", value);
        }
    }
}

/* Purpose: project model target report store from typed facts without capability drift. */
static int model_target_report_store(yvex_model_target_text_value *rows,
                                     unsigned long cap,
                                     unsigned long *count,
                                     const char *fmt,
                                     va_list ap)
{
    int n;

    if (!rows || !count || !fmt || *count >= cap) {
        return 0;
    }
    n = vsnprintf(rows[*count].value, sizeof(rows[*count].value), fmt, ap);
    if (n < 0) {
        rows[*count].value[0] = '\0';
        return 0;
    }
    rows[*count].value[sizeof(rows[*count].value) - 1u] = '\0';
    (*count)++;
    return 1;
}

/* Purpose: register one report add row while preserving order and bounds.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_report_add_row(yvex_model_target_report *report,
                                     const char *fmt,
                                     ...)
{
    va_list ap;
    int ok;

    if (!report || !fmt) {
        return 0;
    }
    va_start(ap, fmt);
    ok = model_target_report_store(report->rows,
                                   YVEX_MODEL_TARGET_ROW_CAP,
                                   &report->row_count,
                                   fmt,
                                   ap);
    va_end(ap);
    return ok;
}

/* Purpose: append an immutable ordered row template without duplicating report mechanics.
 * Inputs: report is mutable; rows and their strings are borrowed for this call.
 * Effects: appends rows in declaration order until the bounded report refuses one.
 * Failure: invalid inputs or row-cap exhaustion stop delivery without overrunning storage.
 * Boundary: the table owns presentation constants, never capability or model decisions. */
void yvex_model_target_report_add_rows(yvex_model_target_report *report,
                                       const char *const *rows,
                                       size_t row_count)
{
    size_t row;

    if (!report || (!rows && row_count != 0u)) return;
    for (row = 0u; row < row_count; ++row) {
        if (!yvex_model_target_report_add_row(report, "%s", rows[row])) return;
    }
}

/* Purpose: project immutable row schemas from one typed fact record.
 * Inputs: rows describe field widths and facts points at the matching immutable record.
 * Effects: appends rows in schema order through the bounded report owner.
 * Failure: invalid schema/input or report exhaustion stops without partial field mutation.
 * Boundary: projection formats supplied facts and never derives capability decisions. */
void yvex_model_target_report_project_rows(
    yvex_model_target_report *report,
    const yvex_model_target_row_spec *rows,
    size_t row_count,
    const void *facts)
{
    const unsigned char *base = (const unsigned char *)facts;
    size_t row;

    if (!report || (!rows && row_count != 0u) || (!facts && row_count != 0u)) return;
    for (row = 0u; row < row_count; ++row) {
        const void *value = base + rows[row].value_offset;
        int ok = 0;

        switch (rows[row].kind) {
        case YVEX_MODEL_TARGET_ROW_LITERAL:
            ok = yvex_model_target_report_add_row(report, "%s", rows[row].format);
            break;
        case YVEX_MODEL_TARGET_ROW_STRING:
            ok = yvex_model_target_report_add_row(
                report, rows[row].format, *(const char *const *)value);
            break;
        case YVEX_MODEL_TARGET_ROW_ULONG:
            ok = yvex_model_target_report_add_row(
                report, rows[row].format, *(const unsigned long *)value);
            break;
        case YVEX_MODEL_TARGET_ROW_U64:
            ok = yvex_model_target_report_add_row(
                report, rows[row].format, *(const unsigned long long *)value);
            break;
        case YVEX_MODEL_TARGET_ROW_INT:
            ok = yvex_model_target_report_add_row(report, rows[row].format,
                                                  *(const int *)value);
            break;
        default:
            return;
        }
        if (!ok) return;
    }
}

/* Purpose:
 *   enforce the shared required-target and admitted-source-target contract.
 * Inputs:
 *   request/report are borrowed or mutated; operation is a stable diagnostic noun.
 * Effects:
 *   sets exit status and appends exactly the legacy typed refusal surface.
 * Failure:
 *   returns false for a missing or unsupported target without performing I/O.
 * Boundary:
 *   source-target admission does not infer downstream model capability. */
int yvex_model_target_validate_supported(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    const char *operation,
    int contract_refusal_row)
{
    if (!request || !report || !operation) return 0;
    if (!request->target_id[0]) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(
            report, "model-target %s: requires TARGET", operation);
        return 0;
    }
    if (yvex_model_target_supported_source_target(request->target_id)) return 1;
    report->exit_code = 2;
    if (contract_refusal_row && request->output_contract[0]) {
        yvex_model_target_report_add_row(report, "status: unsupported-target");
    } else {
        yvex_model_target_report_add_error(
            report, "model-target %s: unsupported target: %s",
            operation, request->target_id);
    }
    return 0;
}

/* Purpose: decode the fixed little-endian safetensors header length. */
static unsigned long long probe_le64(const unsigned char bytes[8])
{
    unsigned long long value = 0ull;
    unsigned int byte;

    for (byte = 0u; byte < 8u; ++byte) {
        value |= (unsigned long long)bytes[byte] << (byte * 8u);
    }
    return value;
}

/* Purpose: form one bounded source-relative evidence path from a typed request.
 * Inputs: request/family are borrowed; leaf may be NULL for the source directory.
 * Effects: writes only the caller-owned path buffer and performs no filesystem access.
 * Failure: invalid inputs, missing source authority, or truncation leave out empty.
 * Boundary: local path formation is report evidence plumbing, never source verification. */
int yvex_model_target_probe_source_path(
    const yvex_model_target_request *request,
    const char *family,
    const char *leaf,
    char *out,
    size_t cap)
{
    int length;

    if (!request || !family || !out || cap == 0u) return 0;
    out[0] = '\0';
    if (request->source_path[0]) {
        length = leaf && leaf[0]
                     ? snprintf(out, cap, "%s/%s", request->source_path, leaf)
                     : snprintf(out, cap, "%s", request->source_path);
    } else if (request->models_root[0]) {
        length = leaf && leaf[0]
                     ? snprintf(out, cap, "%s/hf/%s/%s/%s", request->models_root,
                                family, request->target_id, leaf)
                     : snprintf(out, cap, "%s/hf/%s/%s", request->models_root,
                                family, request->target_id);
    } else {
        return 0;
    }
    if (length < 0 || (size_t)length >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

/* Purpose: classify an explicit local path as an existing directory.
 * Inputs: path is borrowed and must be non-empty.
 * Effects: performs one stat and does not mutate caller state.
 * Failure: missing, inaccessible, or non-directory paths return false.
 * Boundary: directory presence is report evidence, not source admission. */
int yvex_model_target_probe_directory(const char *path)
{
    struct stat state;

    return path && path[0] && stat(path, &state) == 0 && S_ISDIR(state.st_mode);
}

/* Purpose: determine whether one report evidence file is locally readable.
 * Inputs: path is borrowed and must be non-empty.
 * Effects: opens and closes the file without reading or retaining bytes.
 * Failure: missing or unreadable paths return false.
 * Boundary: readability is not metadata validity, trust, or capability admission. */
int yvex_model_target_probe_file(const char *path)
{
    FILE *file;

    if (!path || !path[0]) return 0;
    file = fopen(path, "rb");
    if (!file) return 0;
    (void)fclose(file);
    return 1;
}

/* Purpose: read a bounded report sidecar prefix into caller-owned storage.
 * Inputs: path is borrowed; out has cap bytes and receives a NUL-terminated prefix.
 * Effects: performs bounded read-only file IO and never allocates or retains bytes.
 * Failure: invalid or unreadable inputs return false with out empty.
 * Boundary: sidecar text is diagnostic evidence and cannot promote capability. */
int yvex_model_target_probe_read(const char *path, char *out, size_t cap)
{
    FILE *file;
    size_t read_count;

    if (!path || !out || cap == 0u) return 0;
    out[0] = '\0';
    file = fopen(path, "rb");
    if (!file) return 0;
    read_count = fread(out, 1u, cap - 1u, file);
    out[read_count] = '\0';
    (void)fclose(file);
    return 1;
}

/* Purpose: read one bounded safetensors JSON header for target evidence probes.
 * Inputs: path is borrowed; out receives one owned NUL-terminated allocation.
 * Effects: reads the fixed prefix and declared header only; payload bytes are untouched.
 * Failure: malformed, oversized, unreadable, or allocation-failed input returns false.
 * Boundary: lexical header evidence is not retained source truth or payload admission. */
int yvex_model_target_probe_header(const char *path, char **out)
{
    unsigned char length_bytes[8];
    unsigned long long header_length;
    FILE *file;
    char *header;

    if (!path || !out) return 0;
    *out = NULL;
    file = fopen(path, "rb");
    if (!file) return 0;
    if (fread(length_bytes, 1u, sizeof(length_bytes), file) != sizeof(length_bytes)) {
        (void)fclose(file);
        return 0;
    }
    header_length = probe_le64(length_bytes);
    if (header_length == 0ull || header_length > MODEL_TARGET_HEADER_CAP) {
        (void)fclose(file);
        return 0;
    }
    header = (char *)malloc((size_t)header_length + 1u);
    if (!header) {
        (void)fclose(file);
        return 0;
    }
    if (fread(header, 1u, (size_t)header_length, file) != (size_t)header_length) {
        free(header);
        (void)fclose(file);
        return 0;
    }
    (void)fclose(file);
    header[header_length] = '\0';
    *out = header;
    return 1;
}

/* Purpose: test one lexical role marker without owning model semantics. */
static int source_scan_has(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

/* Purpose: project the immutable bounded source scan count view.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */

static void source_scan_count(yvex_model_target_source_scan *scan,
                              const char *name)
{
    int attention = source_scan_has(name, "self_attn") ||
                    source_scan_has(name, "attention");
    int mlp = source_scan_has(name, ".mlp.") ||
              source_scan_has(name, "feed_forward");

    if (source_scan_has(name, "embed_tokens")) scan->embed = 1;
    if (attention &&
        (source_scan_has(name, "q_proj") || source_scan_has(name, "k_proj") ||
         source_scan_has(name, "v_proj") || source_scan_has(name, "o_proj")))
        scan->attn++;
    if (mlp &&
        (source_scan_has(name, "gate_proj") || source_scan_has(name, "up_proj") ||
         source_scan_has(name, "down_proj") ||
         source_scan_has(name, "mlp.gate.weight") ||
         source_scan_has(name, "experts.gate_up_proj") ||
         source_scan_has(name, "shared_expert.down_proj")))
        scan->mlp++;
    if (source_scan_has(name, "norm") || source_scan_has(name, "layernorm"))
        scan->norm++;
    if (source_scan_has(name, "lm_head") || source_scan_has(name, "output_head"))
        scan->head++;
    if (source_scan_has(name, "router") || source_scan_has(name, "expert"))
        scan->moe++;
}

/* Purpose:
 *   scan one admitted native-weight inventory into shared lexical report facts.
 * Inputs:
 *   request/family are borrowed; scan is caller-owned.
 * Effects:
 *   opens only the canonical inventory and closes it before returning.
 * Failure:
 *   missing or unreadable sources leave deterministic zero-valued counters.
 * Boundary:
 *   lexical counts are report evidence, not typed tensor-role admission. */
void yvex_model_target_scan_source(
    const yvex_model_target_request *request,
    const char *family,
    yvex_model_target_source_scan *scan)
{
    yvex_native_weight_options options;
    yvex_native_weight_table *table = NULL;
    yvex_error err;
    unsigned long long index;

    if (!scan) return;
    memset(scan, 0, sizeof(*scan));
    (void)yvex_model_target_probe_source_path(
        request, family, NULL, scan->source_path, sizeof(scan->source_path));
    scan->source_present = yvex_model_target_probe_directory(scan->source_path);
    if (!scan->source_present) return;
    memset(&options, 0, sizeof(options));
    options.source_dir = scan->source_path;
    options.recursive = 1;
    yvex_error_clear(&err);
    if (yvex_native_weight_table_open(&table, &options, &err) != YVEX_OK) return;
    scan->tensors = yvex_native_weight_table_count(table);
    for (index = 0; index < scan->tensors; ++index) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, index);
        if (info) source_scan_count(scan, info->name);
    }
    yvex_native_weight_table_close(table);
    scan->layers = scan->attn >= 4 || scan->mlp >= 3 ? 1ull : 0ull;
}

/* Purpose: register one report add error while preserving order and bounds.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_report_add_error(yvex_model_target_report *report,
                                       const char *fmt,
                                       ...)
{
    va_list ap;
    int ok;

    if (!report || !fmt) {
        return 0;
    }
    va_start(ap, fmt);
    ok = model_target_report_store(report->error_rows,
                                   sizeof(report->error_rows) /
                                       sizeof(report->error_rows[0]),
                                   &report->error_row_count,
                                   fmt,
                                   ap);
    va_end(ap);
    return ok;
}

/* Purpose: register one report add table row while preserving order and bounds.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_report_add_table_row(yvex_model_target_report *report,
                                           unsigned int column_count,
                                           const char *c0,
                                           const char *c1,
                                           const char *c2,
                                           const char *c3,
                                           const char *c4,
                                           const char *c5,
                                           const char *c6,
                                           const char *c7)
{
    const char *cols[YVEX_MODEL_TARGET_TABLE_COL_CAP] = {
        c0, c1, c2, c3, c4, c5, c6, c7
    };
    yvex_model_target_table_row *row;
    unsigned int i;

    if (!report || column_count > YVEX_MODEL_TARGET_TABLE_COL_CAP ||
        report->table_row_count >= YVEX_MODEL_TARGET_TABLE_ROW_CAP) {
        return 0;
    }
    row = &report->table_rows[report->table_row_count++];
    memset(row, 0, sizeof(*row));
    row->column_count = column_count;
    for (i = 0; i < column_count; ++i) {
        snprintf(row->columns[i], sizeof(row->columns[i]), "%s",
                 cols[i] ? cols[i] : "");
    }
    return 1;
}

/*
 * yvex_model_target_report_add_output_contract()
 *
 * Purpose:
 *   append the shared report-only output-contract probe facts.
 *
 * Inputs:
 *   report is mutated; report_name/mode are borrowed strings.
 *
 * Effects:
 *   appends bounded rows only; it does not render, write streams, inspect
 *   artifacts, execute runtime code, or claim readiness.
 *
 * Failure:
 *   row-cap exhaustion is handled by the shared bounded row helper.
 *
 * Boundary:
 *   output-contract probes prove only CLI field shape, not runtime,
 *   generation, benchmark, or release capability. */
void yvex_model_target_report_add_output_contract(yvex_model_target_report *report,
                                                  const char *report_name,
                                                  const char *mode)
{
    yvex_model_target_report_add_row(report, "status: pass");
    yvex_model_target_report_add_row(report, "report: %s",
                                     report_name ? report_name : "unknown");
    yvex_model_target_report_add_row(report, "mode: %s",
                                     mode ? mode : "unknown");
    yvex_model_target_report_add_rows(
        report, output_contract_tail,
        sizeof(output_contract_tail) / sizeof(output_contract_tail[0]));
}

/* Purpose: construct bounded report build state from admitted inputs.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    report->kind = request->kind;
    report->mode = request->mode;
    report->help_requested = request->help_requested;
    report->exit_code = 0;

    switch (request->kind) {
    case YVEX_MODEL_TARGET_COMMAND_HELP:
        return yvex_model_target_help_report_build(report, err);
    case YVEX_MODEL_TARGET_COMMAND_CLASSES:
    case YVEX_MODEL_TARGET_COMMAND_LIST:
    case YVEX_MODEL_TARGET_COMMAND_INSPECT:
        return yvex_model_target_catalog_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_DECISION:
        return yvex_model_target_decision_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_CANDIDATE:
    case YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE:
    case YVEX_MODEL_TARGET_COMMAND_QWEN_METAL:
        return yvex_model_target_candidate_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE:
        return yvex_model_class_profile_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION:
        return yvex_tensor_collection_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP:
        if (request->gate[0]) {
            return yvex_mapping_gate_report_build(request, report, err);
        }
        if (strcmp(request->role, "output-head") == 0) {
            return yvex_output_head_map_report_build(request, report, err);
        }
        if (strcmp(request->role, "tokenizer") == 0) {
            return yvex_tokenizer_map_report_build(request, report, err);
        }
        if (strcmp(request->role, "missing-roles") == 0) {
            return yvex_missing_role_report_build(request, report, err);
        }
        return yvex_tensor_naming_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP:
        return yvex_tokenizer_map_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES:
        return yvex_missing_role_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY:
        if (request->gate[0] || request->include_requirements) {
            return yvex_qtype_role_support_report_build(request, report, err);
        }
        return yvex_qtype_policy_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_UNKNOWN:
    default:
        return yvex_model_target_catalog_report_build(request, report, err);
    }
}

/* Purpose: construct bounded help report build state from admitted inputs.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_help_report_build(yvex_model_target_report *report,
                                        yvex_error *err)
{
    return yvex_model_target_catalog_help_report_build(report, err);
}

/* Purpose: release owned report close resources in dependency order.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
void yvex_model_target_report_close(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    yvex_model_register_deepseek_v4()->lowering.close(
        (yvex_deepseek_gguf_map *)report->family_lowering);
    yvex_model_register_deepseek_v4()->coverage.close(
        (yvex_deepseek_tensor_coverage *)report->family_coverage);
    yvex_model_register_deepseek_v4()->ir.close(
        (yvex_deepseek_v4_ir *)report->family_architecture);
    memset(report, 0, sizeof(*report));
}
