/*
 * yvex_model_artifact_report.c - model artifact report coordination and shared facts.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   typed model-artifact report request/result helpers, shared registry metadata
 *   facts, model-ref-to-registry views, and report build dispatch.
 *
 * Does not own:
 *   command argument parsing, command dispatch, renderer formatting, explicit registry
 *   file writing, artifact emission, runtime generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   report facts are typed values; this module does not own command grammar or
 *   pre-rendered command output buffers.
 *
 * Boundary:
 *   model artifact reports are not artifact emission, runtime descriptors,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_model_artifact_report.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <yvex/artifact_identity.h>
#include <yvex/model_registry.h>
#include <yvex/model_ref.h>
#include <yvex/yvex.h>

#include "yvex_model_artifact_gate.h"

typedef struct {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *table;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
} yvex_cli_tokenizer_context;

typedef struct {
    yvex_model_registry_entry entry;
    char format[16];
    char architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    char support_level[64];
} yvex_cli_metadata_snapshot;

int open_model_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err);
void close_model_context(yvex_cli_tokenizer_context *ctx);

static void model_artifact_core_writef(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof(buf)) n = (int)sizeof(buf) - 1;
    (void)write(STDOUT_FILENO, buf, (size_t)n);
}

int models_registry_open(yvex_model_registry **registry,
                         const char *registry_path,
                         int create_if_missing,
                         yvex_error *err)
{
    yvex_model_registry_options options;

    memset(&options, 0, sizeof(options));
    options.registry_path = registry_path;
    options.create_if_missing = create_if_missing;
    return yvex_model_registry_open(registry, &options, err);
}

static void dims_to_text(const unsigned long long *dims,
                         unsigned int rank,
                         char *out,
                         size_t out_cap)
{
    unsigned int i;
    size_t used = 0u;

    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    if (used + 1u < out_cap) {
        out[used++] = '[';
        out[used] = '\0';
    }
    for (i = 0; i < rank && used < out_cap; ++i) {
        int n = snprintf(out + used,
                         used < out_cap ? out_cap - used : 0u,
                         "%s%llu",
                         i == 0 ? "" : ",",
                         dims[i]);
        if (n < 0 || (size_t)n >= (used < out_cap ? out_cap - used : 0u)) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (used + 1u < out_cap) {
        out[used++] = ']';
        out[used] = '\0';
    }
}

static const char *current_support_from_metadata(const yvex_model_registry_entry *entry)
{
    if (entry && entry->primary_tensor_name && entry->primary_tensor_name[0]) {
        return "selected-tensor-materialized";
    }
    if (entry && entry->format && entry->format[0]) {
        return "descriptor-only";
    }
    return "";
}

void model_ref_registry_entry_view(const yvex_model_ref *ref,
                                   yvex_model_registry_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    if (!ref) return;
    entry->alias = ref->alias;
    entry->path = ref->path;
    entry->sha256 = ref->sha256;
    entry->file_size = ref->registered_file_size;
    entry->format = ref->format;
    entry->architecture = ref->architecture;
    entry->tensor_count = ref->tensor_count;
    entry->known_tensor_bytes = ref->known_tensor_bytes;
    entry->primary_tensor_name = ref->primary_tensor_name;
    entry->primary_tensor_role = ref->primary_tensor_role;
    entry->primary_tensor_dtype = ref->primary_tensor_dtype;
    entry->primary_tensor_rank = ref->primary_tensor_rank;
    entry->primary_tensor_dims = ref->primary_tensor_dims;
    entry->primary_tensor_bytes = ref->primary_tensor_bytes;
    entry->support_level = ref->support_level;
    entry->selected_embedding_ready = ref->selected_embedding_ready;
    entry->selected_embedding_hidden_size = ref->selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = ref->selected_embedding_vocab_size;
    entry->selected_embedding_output_count = ref->selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = ref->selected_embedding_slice_bytes;
    entry->execution_ready = ref->execution_ready;
}

void print_metadata_drift_cli(const yvex_model_metadata_drift_report *report)
{
    unsigned int i;

    if (!report) return;
    model_artifact_core_writef("metadata_status: %s\n", report->metadata_status[0] ? report->metadata_status : "unknown");
    model_artifact_core_writef("readiness_status: %s\n", report->readiness_status[0] ? report->readiness_status : "unknown");
    for (i = 0; i < report->issue_count; ++i) {
        model_artifact_core_writef("metadata_issue_%u_code: %s\n", i, report->issues[i].code);
        model_artifact_core_writef("metadata_issue_%u_registered: %s\n", i, report->issues[i].registered_value);
        model_artifact_core_writef("metadata_issue_%u_current: %s\n", i, report->issues[i].current_value);
    }
}

int populate_registry_metadata(yvex_cli_metadata_snapshot *snapshot,
                               const char *path,
                               yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *primary = NULL;
    const yvex_tensor_info *embedding = NULL;
    yvex_selected_embedding_shape selected_shape;
    unsigned long long known_bytes = 0ull;
    unsigned long long i;
    int rc;

    if (!snapshot || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_metadata",
                       "metadata snapshot and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) return rc;

    snprintf(snapshot->format, sizeof(snapshot->format), "gguf");
    snprintf(snapshot->architecture, sizeof(snapshot->architecture), "%s",
             yvex_arch_name(yvex_model_arch(ctx.model)));

    for (i = 0; i < yvex_tensor_table_count(ctx.table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        known_bytes += tensor->storage_bytes;
        if (!primary && strcmp(tensor->name, "token_embd.weight") == 0) {
            primary = tensor;
            embedding = tensor;
        }
    }
    if (!primary && yvex_tensor_table_count(ctx.table) > 0ull) {
        primary = yvex_tensor_table_at(ctx.table, 0);
    }

    if (primary) {
        snprintf(snapshot->primary_tensor_name, sizeof(snapshot->primary_tensor_name),
                 "%s", primary->name ? primary->name : "");
        snprintf(snapshot->primary_tensor_role, sizeof(snapshot->primary_tensor_role),
                 "%s", yvex_tensor_role_name(primary->role));
        snprintf(snapshot->primary_tensor_dtype, sizeof(snapshot->primary_tensor_dtype),
                 "%s", yvex_dtype_name(primary->dtype));
        dims_to_text(primary->dims, primary->rank, snapshot->primary_tensor_dims,
                     sizeof(snapshot->primary_tensor_dims));
        snapshot->entry.primary_tensor_rank = primary->rank;
        snapshot->entry.primary_tensor_bytes = primary->storage_bytes;
    }

    if (embedding) {
        yvex_error shape_err;
        yvex_error_clear(&shape_err);
        memset(&selected_shape, 0, sizeof(selected_shape));
        if (yvex_selected_embedding_shape_validate(embedding, 0u, &selected_shape,
                                                   &shape_err) == YVEX_OK) {
            snapshot->entry.selected_embedding_ready = 1;
            snapshot->entry.selected_embedding_hidden_size = selected_shape.hidden_size;
            snapshot->entry.selected_embedding_vocab_size = selected_shape.vocab_size;
            snapshot->entry.selected_embedding_output_count = selected_shape.output_count;
            snapshot->entry.selected_embedding_slice_bytes = selected_shape.slice_bytes;
        } else {
            yvex_error_clear(&shape_err);
        }
    }

    snapshot->entry.path = path;
    snapshot->entry.format = snapshot->format;
    snapshot->entry.architecture = snapshot->architecture;
    snapshot->entry.tensor_count = yvex_tensor_table_count(ctx.table);
    snapshot->entry.known_tensor_bytes = known_bytes;
    snapshot->entry.primary_tensor_name = snapshot->primary_tensor_name;
    snapshot->entry.primary_tensor_role = snapshot->primary_tensor_role;
    snapshot->entry.primary_tensor_dtype = snapshot->primary_tensor_dtype;
    snapshot->entry.primary_tensor_dims = snapshot->primary_tensor_dims;
    snprintf(snapshot->support_level, sizeof(snapshot->support_level), "%s",
             current_support_from_metadata(&snapshot->entry));
    snapshot->entry.support_level = snapshot->support_level;

    close_model_context(&ctx);
    return YVEX_OK;
}

static void artifact_report_clear(yvex_model_artifact_report *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
}

/*
 * Builds a typed machine-readable inventory view from canonical admission.
 * The result borrows admission strings and performs no parsing, IO, or support
 * inference from paths, names, or independent booleans.
 */
int yvex_model_artifact_report_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_model_artifact_report *report,
    yvex_error *err)
{
    yvex_model_complete_artifact_gate_fact gate;
    int rc;

    if (!report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifact.report",
                       "report output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    artifact_report_clear(report);
    rc = yvex_model_artifact_gate_from_admission(admission, &gate, err);
    if (rc != YVEX_OK) {
        report->kind = YVEX_MODEL_ARTIFACT_REPORT_STATUS;
        report->status = "blocked";
        report->exit_code = 2;
        report->support_level = "none";
        report->execution_ready = 0;
        report->reason = "canonical complete-artifact admission is absent";
        report->boundary = "tensor proofs and structural GGUF files are not complete artifacts";
        report->next_row = "V010.ARTIFACT.MATERIALIZE.0";
        return rc;
    }
    report->kind = YVEX_MODEL_ARTIFACT_REPORT_STATUS;
    report->status = "complete-artifact-admitted";
    report->exit_code = 0;
    report->artifact_class = yvex_artifact_class_name(
        admission->artifact_class);
    report->qprofile = gate.profile_name;
    report->path = gate.artifact_path;
    report->sha256 = gate.artifact_identity;
    report->file_size = gate.file_bytes;
    report->format = "gguf";
    report->tensor_count = gate.tensor_count;
    report->support_level = "descriptor-only";
    report->execution_ready = 0;
    report->integrity_status = "pass";
    report->materialization_status = "not-started";
    report->backend_status = "not-tested";
    report->reason = "canonical complete-artifact admission passed";
    report->boundary = "complete artifact ready for materialization; runtime unsupported";
    report->next_row = "V010.ARTIFACT.MATERIALIZE.0";
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_model_artifact_report_build(const yvex_model_artifact_report_request *request,
                                     yvex_model_artifact_report *report,
                                     yvex_error *err)
{
    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifact_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    artifact_report_clear(report);
    report->kind = request->kind;
    report->status = "report-only";
    report->exit_code = 0;
    report->alias = request->artifact_alias;
    report->path = request->artifact_path;
    report->family = request->expected_family;
    report->qprofile = request->expected_qprofile;
    report->execution_ready = 0;
    report->reason = "typed model artifact report dispatch is available";
    report->boundary = "model artifact report is not artifact emission or runtime generation";
    report->next_row = "topology cleanup";
    yvex_error_clear(err);
    return YVEX_OK;
}
