/* Owner: TRACK.ARTIFACT / TRACK.INTEGRITY.
 * Owns: bounded native GGUF roundtrip, whole-file identity, and one fail-closed admission result binding writer,
 *   physical emission, pinned official parsing, tokenizer facts, and the published immutable file snapshot.
 * Does not own: writer execution, official-reader production, registry persistence, materialization, runtime
 *   descriptors, execution, or release.
 * Invariants: roundtrip hashes every byte once; complete admission never upgrades a proof, external GGUF,
 *   incomplete file, or changed file snapshot.
 * Boundary: native verification feeds complete-artifact admission; acceptance means ready for materialization only.
 * Purpose: verify exact serialized artifacts and bind their independent proofs.
 * Inputs: sealed writer plans, digest evidence, immutable files, and parser facts.
 * Effects: bounded reads and population of caller-owned summaries or failures.
 * Failure: typed refusal with deterministic cleanup and no artifact mutation. */
#include <yvex/internal/artifact.h>
#include <yvex/internal/core.h>
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/gguf.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDTRIP_DEFAULT_CHUNK (8u * 1024u * 1024u)

/* Purpose: publish one structured complete-artifact admission refusal.
 * Inputs: failure context, expected/actual counters, status, and diagnostic.
 * Effects: resets the caller failure record and writes the shared error object.
 * Failure: returns the supplied non-success status without touching artifact state.
 * Boundary: centralizes admission diagnostics, not admission policy. */
static int admission_fail(yvex_artifact_admission_failure *failure,
                          yvex_artifact_admission_code code,
                          const char *field,
                          unsigned long long expected,
                          unsigned long long actual,
                          yvex_error *err,
                          yvex_status status,
                          const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->expected = expected;
        failure->actual = actual;
        if (field)
            (void)snprintf(failure->field, sizeof(failure->field), "%s",
                           field);
    }
    yvex_error_set(err, status, "artifact.complete.admission", message);
    return status;
}

/* Purpose: compare stable file identity captured by artifact and emission owners.
 * Inputs: immutable artifact snapshot and finalized file-sink summary.
 * Effects: none.
 * Failure: returns false for absent or mismatched identity fields.
 * Boundary: compares explicit fields and never hashes structure storage. */
static int admission_snapshot_matches(
    const yvex_artifact_snapshot *snapshot,
    const yvex_gguf_file_sink_summary *emission)
{
    return snapshot && emission &&
           snapshot->device == emission->file_device &&
           snapshot->inode == emission->file_inode &&
           snapshot->size == emission->file_size &&
           snapshot->mtime_seconds == emission->published_mtime_seconds &&
           snapshot->mtime_nanoseconds ==
               emission->published_mtime_nanoseconds &&
           snapshot->ctime_seconds == emission->published_ctime_seconds &&
           snapshot->ctime_nanoseconds ==
               emission->published_ctime_nanoseconds;
}

/* Purpose: bind official-reader evidence to the validated emission inode.
 * Inputs: pinned-reader fact and finalized file-sink summary.
 * Effects: none.
 * Failure: returns false for absent evidence or any snapshot mismatch.
 * Boundary: validates file identity only; parser correctness stays external. */
static int admission_official_snapshot_matches(
    const yvex_artifact_official_reader_fact *official,
    const yvex_gguf_file_sink_summary *emission)
{
    return official && emission &&
           official->file_device == emission->file_device &&
           official->file_inode == emission->file_inode &&
           official->file_bytes == emission->file_size &&
           official->file_mtime_seconds ==
               emission->validated_mtime_seconds &&
           official->file_mtime_nanoseconds ==
               emission->validated_mtime_nanoseconds &&
           official->file_ctime_seconds ==
               emission->validated_ctime_seconds &&
           official->file_ctime_nanoseconds ==
               emission->validated_ctime_nanoseconds;
}

/* Purpose: admit a published artifact when every independent proof agrees.
 * Inputs: writer plan, emission, native roundtrip, official parse, and path.
 * Effects: reopens the file metadata-only and fills one caller-owned admission.
 * Failure: returns a typed refusal and leaves the output cleared.
 * Boundary: consumes payload proof but does not reread tensor bytes. */
int yvex_complete_artifact_admit(
    const yvex_artifact_admission_request *request,
    yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure,
    yvex_error *err)
{
    const yvex_gguf_writer_plan_summary *plan;
    const yvex_gguf_file_sink_summary *emission;
    const yvex_gguf_roundtrip_summary *roundtrip;
    const yvex_artifact_official_reader_fact *official;
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_artifact_snapshot snapshot;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!request || !out || !request->artifact_path ||
        !request->artifact_path[0] || !request->writer_plan ||
        !request->emission || !request->native_roundtrip ||
        !request->official_reader)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_INVALID_ARGUMENT, "request",
            1u, 0u, err, YVEX_ERR_INVALID_ARG,
            "complete artifact admission request is incomplete");
    plan = yvex_gguf_writer_plan_summary_get(request->writer_plan);
    emission = request->emission;
    roundtrip = request->native_roundtrip;
    official = request->official_reader;
    if (!plan || !plan->complete)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_WRITER_INCOMPLETE, "writer",
            1u, 0u, err, YVEX_ERR_STATE,
            "sealed complete writer plan is required");
    if (!emission->finalized || !emission->published ||
        emission->committed_terminals != plan->tensor_count ||
        emission->aborted_terminals != 0u ||
        emission->encoded_bytes_written != plan->tensor_payload_bytes ||
        emission->file_size != plan->final_file_bytes)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_EMISSION_INCOMPLETE,
            "emission", plan->tensor_count,
            emission->committed_terminals, err, YVEX_ERR_STATE,
            "published emission is incomplete or disagrees with its plan");
    if (!roundtrip->complete || !roundtrip->reader_accepted ||
        !roundtrip->layout_accepted || !roundtrip->payload_accepted ||
        !roundtrip->snapshot_stable ||
        roundtrip->bytes_hashed != plan->final_file_bytes ||
        roundtrip->terminals_verified != plan->tensor_count ||
        roundtrip->file_device != emission->file_device ||
        roundtrip->file_inode != emission->file_inode ||
        roundtrip->file_mtime_seconds !=
            emission->validated_mtime_seconds ||
        roundtrip->file_mtime_nanoseconds !=
            emission->validated_mtime_nanoseconds ||
        roundtrip->file_ctime_seconds !=
            emission->validated_ctime_seconds ||
        roundtrip->file_ctime_nanoseconds !=
            emission->validated_ctime_nanoseconds)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_NATIVE_ROUNDTRIP,
            "native-roundtrip", plan->final_file_bytes,
            roundtrip->bytes_hashed, err, YVEX_ERR_FORMAT,
            "native full-file roundtrip is incomplete");
    if (!official->accepted ||
        strcmp(official->revision,
               YVEX_GGUF_OFFICIAL_READER_REVISION) != 0 ||
        official->metadata_count != plan->metadata_count ||
        official->tensor_count != plan->tensor_count ||
        official->file_bytes != plan->final_file_bytes ||
        !admission_official_snapshot_matches(official, emission))
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_OFFICIAL_READER,
            "official-reader", plan->tensor_count,
            official->tensor_count, err, YVEX_ERR_FORMAT,
            "pinned official-reader evidence is absent or mismatched");
    if (!roundtrip->tokenizer_complete || !plan->tokenizer_token_count ||
        !plan->tokenizer_merge_count)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_TOKENIZER_INCOMPLETE,
            "tokenizer", 1u, 0u, err, YVEX_ERR_FORMAT,
            "complete tokenizer material was not admitted");
    if (roundtrip->tensor_count != plan->tensor_count ||
        roundtrip->payload_bytes_verified != plan->tensor_payload_bytes)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_TENSOR_COVERAGE,
            "tensor-coverage", plan->tensor_count,
            roundtrip->tensor_count, err, YVEX_ERR_FORMAT,
            "complete tensor coverage is not bound to the artifact");
    if (strlen(roundtrip->artifact_identity) != 64u ||
        !emission->execution_identity[0] ||
        (plan->required_execution_identity[0] &&
         strcmp(emission->execution_identity,
                plan->required_execution_identity) != 0))
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH,
            "identity-chain", 1u, 0u, err, YVEX_ERR_FORMAT,
            "artifact or required quant execution identity is mismatched");

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = request->artifact_path;
    artifact_options.readonly = 1;
    artifact_options.map = 0;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc != YVEX_OK)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_FILE_OPEN, "artifact-path",
            1u, 0u, err, (yvex_status)rc,
            "published artifact cannot be reopened");
    rc = yvex_artifact_snapshot_get(artifact, &snapshot, err);
    if (rc == YVEX_OK)
        rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK || !admission_snapshot_matches(&snapshot, emission)) {
        yvex_artifact_close(artifact);
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_FILE_DRIFT, "file-snapshot",
            emission->file_size, rc == YVEX_OK ? snapshot.size : 0u,
            err, YVEX_ERR_STATE,
            "published artifact snapshot differs from validated emission");
    }
    yvex_artifact_close(artifact);

    out->artifact_class = YVEX_ARTIFACT_CLASS_COMPLETE_YVEX;
    out->metadata_count = plan->metadata_count;
    out->tensor_count = plan->tensor_count;
    out->payload_bytes = plan->tensor_payload_bytes;
    out->file_bytes = plan->final_file_bytes;
    out->source_snapshot_identity = plan->source_snapshot_identity;
    out->mapping_identity = plan->mapping_identity;
    out->file_snapshot = snapshot;
    (void)snprintf(out->artifact_path, sizeof(out->artifact_path), "%s",
                   request->artifact_path);
    (void)snprintf(out->payload_identity, sizeof(out->payload_identity), "%s",
                   plan->payload_identity);
    (void)snprintf(out->transform_identity, sizeof(out->transform_identity), "%s",
                   plan->transform_identity);
    (void)snprintf(out->profile_identity, sizeof(out->profile_identity), "%s",
                   plan->profile_identity);
    (void)snprintf(out->profile_name, sizeof(out->profile_name), "%s",
                   plan->profile_name);
    (void)snprintf(out->quant_execution_identity,
                   sizeof(out->quant_execution_identity), "%s",
                   emission->execution_identity);
    (void)snprintf(out->writer_plan_identity,
                   sizeof(out->writer_plan_identity), "%s",
                   plan->writer_plan_identity);
    (void)snprintf(out->artifact_identity, sizeof(out->artifact_identity), "%s",
                   roundtrip->artifact_identity);
    (void)snprintf(out->official_reader_revision,
                   sizeof(out->official_reader_revision), "%s",
                   official->revision);
    out->tokenizer_complete = 1;
    out->native_reader_accepted = 1;
    out->official_reader_accepted = 1;
    out->payload_integrity_accepted = 1;
    out->materialization_input_ready = 1;
    out->runtime_supported = 0;
    out->complete = 1;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: reconstruct the pinned DeepSeek admission from canonical constants.
 * Inputs: an already-open selected artifact and caller-owned result records.
 * Effects: validates the immutable snapshot and fills materialization facts.
 * Failure: clears output and reports size or snapshot disagreement.
 * Boundary: preserves prior artifact proof without revalidating payload bytes. */
int yvex_artifact_admit_deepseek(
    const yvex_artifact *artifact,
    yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure,
    yvex_error *err)
{
    yvex_artifact_snapshot snapshot;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!artifact || !out)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_INVALID_ARGUMENT, "artifact",
            1u, 0u, err, YVEX_ERR_INVALID_ARG,
            "opened selected artifact handle and output are required");
    if (yvex_artifact_size(artifact) != YVEX_SELECTED_DEEPSEEK_FILE_BYTES)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH,
            "file-bytes", YVEX_SELECTED_DEEPSEEK_FILE_BYTES,
            yvex_artifact_size(artifact), err, YVEX_ERR_FORMAT,
            "selected DeepSeek artifact size is not the admitted size");
    rc = yvex_artifact_snapshot_get(artifact, &snapshot, err);
    if (rc == YVEX_OK)
        rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK)
        return admission_fail(
            failure, YVEX_ARTIFACT_ADMISSION_FILE_DRIFT, "file-snapshot",
            YVEX_SELECTED_DEEPSEEK_FILE_BYTES, 0u, err, (yvex_status)rc,
            "selected DeepSeek artifact snapshot is not stable");

    out->artifact_class = YVEX_ARTIFACT_CLASS_COMPLETE_YVEX;
    out->metadata_count = YVEX_SELECTED_DEEPSEEK_METADATA_COUNT;
    out->tensor_count = YVEX_SELECTED_DEEPSEEK_TENSOR_COUNT;
    out->payload_bytes = YVEX_SELECTED_DEEPSEEK_PAYLOAD_BYTES;
    out->file_bytes = YVEX_SELECTED_DEEPSEEK_FILE_BYTES;
    out->source_snapshot_identity = YVEX_SELECTED_DEEPSEEK_SOURCE_IDENTITY;
    out->mapping_identity = YVEX_SELECTED_DEEPSEEK_MAPPING_IDENTITY;
    out->file_snapshot = snapshot;
    (void)snprintf(out->artifact_path, sizeof(out->artifact_path), "%s",
                   yvex_artifact_path(artifact));
    (void)snprintf(out->payload_identity, sizeof(out->payload_identity), "%s",
                   YVEX_SELECTED_DEEPSEEK_PAYLOAD_IDENTITY);
    (void)snprintf(out->transform_identity, sizeof(out->transform_identity),
                   "%s", YVEX_SELECTED_DEEPSEEK_TRANSFORM_IDENTITY);
    (void)snprintf(out->profile_identity, sizeof(out->profile_identity), "%s",
                   YVEX_SELECTED_DEEPSEEK_PROFILE_IDENTITY);
    (void)snprintf(out->profile_name, sizeof(out->profile_name), "%s",
                   YVEX_SELECTED_DEEPSEEK_PROFILE_NAME);
    (void)snprintf(out->quant_execution_identity,
                   sizeof(out->quant_execution_identity), "%s",
                   YVEX_SELECTED_DEEPSEEK_EXECUTION_IDENTITY);
    (void)snprintf(out->writer_plan_identity,
                   sizeof(out->writer_plan_identity), "%s",
                   YVEX_SELECTED_DEEPSEEK_WRITER_PLAN_IDENTITY);
    (void)snprintf(out->artifact_identity, sizeof(out->artifact_identity),
                   "%s", YVEX_SELECTED_DEEPSEEK_ARTIFACT_IDENTITY);
    (void)snprintf(out->official_reader_revision,
                   sizeof(out->official_reader_revision), "%s",
                   YVEX_GGUF_OFFICIAL_READER_REVISION);
    out->tokenizer_complete = 1;
    out->native_reader_accepted = 1;
    out->official_reader_accepted = 1;
    out->payload_integrity_accepted = 1;
    out->materialization_input_ready = 1;
    out->runtime_supported = 0;
    out->complete = 1;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: project an artifact class to its stable diagnostic spelling.
 * Inputs: typed artifact class.
 * Effects: none.
 * Failure: unknown values map to the fail-closed refused spelling.
 * Boundary: diagnostic projection only; never classifies an artifact. */
const char *yvex_artifact_class_name(yvex_artifact_class artifact_class)
{
    switch (artifact_class) {
    case YVEX_ARTIFACT_CLASS_REFUSED: return "refused";
    case YVEX_ARTIFACT_CLASS_TENSOR_PROOF: return "tensor-proof-artifact";
    case YVEX_ARTIFACT_CLASS_EXTERNAL_UNADMITTED:
        return "external-unadmitted-artifact";
    case YVEX_ARTIFACT_CLASS_COMPLETE_YVEX:
        return "complete-yvex-artifact";
    default: return "refused";
    }
}

/* Purpose: project an admission refusal code to stable diagnostic text.
 * Inputs: typed admission code.
 * Effects: none.
 * Failure: unknown values map to an explicit unknown spelling.
 * Boundary: rendering aid only; refusal policy remains typed. */
const char *yvex_artifact_admission_code_name(
    yvex_artifact_admission_code code)
{
    switch (code) {
    case YVEX_ARTIFACT_ADMISSION_OK: return "ok";
    case YVEX_ARTIFACT_ADMISSION_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_ARTIFACT_ADMISSION_WRITER_INCOMPLETE: return "writer-incomplete";
    case YVEX_ARTIFACT_ADMISSION_EMISSION_INCOMPLETE:
        return "emission-incomplete";
    case YVEX_ARTIFACT_ADMISSION_NATIVE_ROUNDTRIP:
        return "native-roundtrip";
    case YVEX_ARTIFACT_ADMISSION_OFFICIAL_READER:
        return "official-reader";
    case YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH:
        return "identity-mismatch";
    case YVEX_ARTIFACT_ADMISSION_TOKENIZER_INCOMPLETE:
        return "tokenizer-incomplete";
    case YVEX_ARTIFACT_ADMISSION_TENSOR_COVERAGE:
        return "tensor-coverage";
    case YVEX_ARTIFACT_ADMISSION_FILE_OPEN: return "file-open";
    case YVEX_ARTIFACT_ADMISSION_FILE_DRIFT: return "file-drift";
    default: return "unknown";
    }
}

/* Purpose: publish one structured native-roundtrip refusal.
 * Inputs: failing record coordinates, expected/actual facts, and status.
 * Effects: resets the failure record and updates the shared error object.
 * Failure: returns the supplied status without preserving partial success facts.
 * Boundary: centralizes evidence formatting, not verification decisions. */
static int roundtrip_fail(yvex_gguf_roundtrip_failure *failure,
                          yvex_gguf_roundtrip_code code,
                          const char *name,
                          unsigned long long metadata,
                          unsigned long long tensor,
                          unsigned long long expected,
                          unsigned long long actual,
                          unsigned long long offset,
                          yvex_error *err,
                          yvex_status status,
                          const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->metadata_index = metadata;
        failure->tensor_index = tensor;
        failure->expected = expected;
        failure->actual = actual;
        failure->file_offset = offset;
        if (name)
            (void)snprintf(failure->name, sizeof(failure->name), "%s", name);
    }
    yvex_error_set(err, status, "gguf.roundtrip", message);
    return status;
}

/* Purpose: verify one tokenizer metadata array's type and cardinality.
 * Inputs: admitted GGUF, metadata key, element type, and expected count.
 * Effects: none.
 * Failure: returns false when the entry is absent, malformed, or mismatched.
 * Boundary: checks tokenizer completeness without owning metadata parsing. */
static int roundtrip_array_count(const yvex_gguf *gguf,
                                 const char *key,
                                 unsigned int element_type,
                                 unsigned long long expected)
{
    const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, key);
    yvex_gguf_array_info array;
    return value && yvex_gguf_value_array_info(value, &array) == YVEX_OK &&
           array.element_type == (yvex_gguf_value_type)element_type &&
           array.count == expected;
}

/* Purpose: hash one exact file range and optionally enforce zero-filled bytes.
 * Inputs: admitted artifact, range, bounded scratch, hash states, and counters.
 * Effects: performs positioned reads and advances caller-owned hashes/statistics.
 * Failure: returns false on arithmetic, read, hash, or zero-padding disagreement.
 * Boundary: retains no payload and never reads beyond the requested range. */
static int roundtrip_hash_range(
    const yvex_artifact *artifact,
    unsigned long long offset,
    unsigned long long byte_count,
    unsigned char *buffer,
    size_t buffer_bytes,
    yvex_artifact_identity_stream *whole,
    yvex_sha256 *terminal,
    int require_zero,
    yvex_gguf_roundtrip_summary *summary,
    unsigned long long *first_nonzero,
    yvex_error *err)
{
    unsigned long long delivered = 0u;
    while (delivered < byte_count) {
        unsigned long long remaining = byte_count - delivered;
        size_t request = remaining < buffer_bytes
            ? (size_t)remaining : buffer_bytes;
        size_t index;
        if (offset > ULLONG_MAX - delivered ||
            yvex_artifact_read_at(
                artifact, offset + delivered, buffer, request, err) != YVEX_OK)
            return 0;
        if (yvex_artifact_identity_stream_update(
                whole, buffer, request, err) != YVEX_OK ||
            (terminal && !yvex_sha256_update(terminal, buffer, request)))
            return 0;
        if (require_zero)
            for (index = 0u; index < request; ++index)
                if (buffer[index] != 0u) {
                    if (first_nonzero)
                        *first_nonzero = offset + delivered + index;
                    return 0;
                }
        delivered += request;
        summary->bytes_hashed += request;
        summary->read_calls++;
    }
    return 1;
}

/* Purpose: initialize bounded native-roundtrip options.
 * Inputs: caller-owned options storage.
 * Effects: resets the structure and selects the canonical chunk size.
 * Failure: a null destination is ignored.
 * Boundary: supplies defaults only and performs no I/O. */
void yvex_gguf_roundtrip_options_default(
    yvex_gguf_roundtrip_options *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->verification_chunk_bytes = ROUNDTRIP_DEFAULT_CHUNK;
}

/* Purpose: publish a synchronous borrowed verification progress snapshot.
 * Inputs: optional callback options, current summary, and planned file bytes.
 * Effects: may invoke the caller callback exactly once for this checkpoint.
 * Failure: callback behavior is observational and cannot alter verifier state.
 * Boundary: neither pointers nor payload bytes may be retained by this owner. */
static void roundtrip_progress_publish(
    const yvex_gguf_roundtrip_options *options,
    const yvex_gguf_roundtrip_summary *summary,
    unsigned long long planned_file_bytes)
{
    if (options && options->progress)
        options->progress(options->progress_context, summary,
                          planned_file_bytes);
}

typedef struct {
    const char *path;
    const yvex_gguf_writer_plan *writer_plan;
    yvex_quant_digest_sink *digest_sink;
    const yvex_gguf_writer_plan_summary *plan;
    yvex_gguf_roundtrip_options options;
    yvex_gguf_roundtrip_summary *summary;
    yvex_gguf_roundtrip_failure *failure;
    yvex_error *err;
    yvex_artifact *artifact;
    yvex_artifact_snapshot before;
    yvex_artifact_snapshot after;
    yvex_gguf *gguf;
    unsigned char *buffer;
    const unsigned char *prefix;
    size_t prefix_bytes;
    yvex_artifact_identity_stream whole_hash;
    unsigned long long cursor;
    unsigned long long first_nonzero;
} roundtrip_context;

/* Purpose: admit immutable roundtrip inputs and bounded scratch configuration.
 * Inputs: partially initialized verification context and optional options.
 * Effects: copies options into owned context state.
 * Failure: returns typed invalid-argument refusal before opening the artifact.
 * Boundary: validates planning facts only and reads zero file bytes. */
static int roundtrip_context_admit(roundtrip_context *context,
                                   const yvex_gguf_roundtrip_options *options)
{
    if (!context->path || !context->path[0] || !context->plan ||
        !context->digest_sink || !context->summary)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u, 0u, 0u, context->err,
            YVEX_ERR_INVALID_ARG,
            "artifact path, sealed plan, digest sink, and output are required");
    yvex_gguf_roundtrip_options_default(&context->options);
    if (options) context->options = *options;
    if (!context->options.verification_chunk_bytes ||
        context->options.verification_chunk_bytes > (size_t)SSIZE_MAX)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u,
            context->options.verification_chunk_bytes, 0u, context->err,
            YVEX_ERR_INVALID_ARG, "verification chunk size is invalid");
    return YVEX_OK;
}

/* Purpose: open the immutable artifact and admit its native GGUF layout.
 * Inputs: admitted context carrying path and sealed writer-plan summary.
 * Effects: owns artifact/GGUF handles and records reader/layout acceptance.
 * Failure: returns typed open, parse, size, or layout refusal for cleanup.
 * Boundary: parser/layout owners supply facts; this owner binds them to the plan. */
static int roundtrip_open_container(roundtrip_context *context)
{
    yvex_artifact_options artifact_options;
    yvex_gguf_reader_options reader_options;
    yvex_gguf_parse_result parse;
    yvex_gguf_layout_result layout;

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = context->path;
    artifact_options.readonly = 1;
    if (yvex_artifact_open(&context->artifact, &artifact_options,
                           context->err) != YVEX_OK ||
        yvex_artifact_snapshot_get(context->artifact, &context->before,
                                   context->err) != YVEX_OK)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_ARTIFACT_OPEN,
            context->path, ULLONG_MAX, ULLONG_MAX,
            context->plan->final_file_bytes,
            context->artifact ? yvex_artifact_size(context->artifact) : 0u,
            0u, context->err, YVEX_ERR_IO,
            "artifact open or snapshot capture failed");
    if (context->before.size != context->plan->final_file_bytes)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE,
            context->path, ULLONG_MAX, ULLONG_MAX,
            context->plan->final_file_bytes, context->before.size, 0u,
            context->err, YVEX_ERR_FORMAT,
            "artifact file size differs from plan");

    yvex_gguf_reader_options_default(&reader_options);
    reader_options.max_metadata_entries = context->plan->metadata_count + 16u;
    reader_options.max_tensor_entries = context->plan->tensor_count;
    memset(&parse, 0, sizeof(parse));
    if (yvex_gguf_open_ex(&context->gguf, context->artifact, &reader_options,
                          &parse, context->err) != YVEX_OK)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_READER_REFUSAL,
            context->path, parse.record_index, ULLONG_MAX,
            YVEX_GGUF_PARSE_OK, parse.code, parse.byte_offset, context->err,
            YVEX_ERR_FORMAT, "native GGUF reader refused emitted artifact");
    context->summary->reader_accepted = 1;
    memset(&layout, 0, sizeof(layout));
    if (yvex_gguf_layout_validate(context->artifact, context->gguf, &layout,
                                  context->err) != YVEX_OK ||
        !layout.accepted)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_LAYOUT_REFUSAL,
            layout.tensor_name, ULLONG_MAX, layout.tensor_index,
            YVEX_GGUF_LAYOUT_OK, layout.code, layout.failure_absolute_offset,
            context->err, YVEX_ERR_FORMAT,
            "native global layout validator refused emitted artifact");
    context->summary->layout_accepted = 1;
    return YVEX_OK;
}

/* Purpose: compare header, tokenizer, and tensor directory against the plan.
 * Inputs: open admitted GGUF and immutable writer-plan directory.
 * Effects: records admitted metadata/tensor counts and tokenizer completeness.
 * Failure: returns at the first typed header, tokenizer, or tensor divergence.
 * Boundary: compares structural facts and reads no tensor payload. */
static int roundtrip_validate_directory(roundtrip_context *context)
{
    const yvex_gguf_header *header = yvex_gguf_header_view(context->gguf);
    unsigned long long ordinal;

    if (!header || header->version != context->plan->gguf_version ||
        header->metadata_count != context->plan->metadata_count ||
        header->tensor_count != context->plan->tensor_count ||
        yvex_gguf_alignment(context->gguf) != context->plan->alignment ||
        yvex_gguf_tensor_data_offset(context->gguf) !=
            context->plan->structural_bytes +
                context->plan->pre_data_padding_bytes ||
        yvex_gguf_file_size(context->gguf) != context->plan->final_file_bytes)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE,
            context->path, ULLONG_MAX, ULLONG_MAX,
            context->plan->final_file_bytes,
            yvex_gguf_file_size(context->gguf), 0u, context->err,
            YVEX_ERR_FORMAT,
            "reader header/alignment/data offset differs from writer plan");
    context->summary->metadata_count = header->metadata_count;
    context->summary->tensor_count = header->tensor_count;
    if (context->plan->tokenizer_token_count &&
        (!roundtrip_array_count(
             context->gguf, "tokenizer.ggml.tokens", YVEX_GGUF_VALUE_STRING,
             context->plan->tokenizer_token_count) ||
         !roundtrip_array_count(
             context->gguf, "tokenizer.ggml.token_type", YVEX_GGUF_VALUE_INT32,
             context->plan->tokenizer_token_count) ||
         !roundtrip_array_count(
             context->gguf, "tokenizer.ggml.merges", YVEX_GGUF_VALUE_STRING,
             context->plan->tokenizer_merge_count) ||
         !yvex_gguf_metadata_find(context->gguf, "tokenizer.huggingface.json") ||
         !yvex_gguf_metadata_find(context->gguf, "yvex.tokenizer.config.json")))
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_TOKENIZER_INCOMPLETE,
            "tokenizer", ULLONG_MAX, ULLONG_MAX,
            context->plan->tokenizer_token_count, 0u, 0u, context->err,
            YVEX_ERR_FORMAT, "artifact tokenizer material is incomplete");
    context->summary->tokenizer_complete =
        context->plan->tokenizer_token_count != 0u;

    for (ordinal = 0u; ordinal < context->plan->tensor_count; ++ordinal) {
        const yvex_gguf_writer_tensor *expected =
            yvex_gguf_writer_plan_tensor_at(context->writer_plan, ordinal);
        const yvex_gguf_tensor_info *actual =
            yvex_gguf_tensor_at(context->gguf, ordinal);
        unsigned int dimension;
        int matches = expected && actual &&
            actual->name_len == strlen(expected->name) &&
            memcmp(actual->name, expected->name, actual->name_len) == 0 &&
            actual->rank == expected->rank &&
            actual->ggml_type == expected->qtype &&
            actual->relative_offset == expected->relative_offset &&
            actual->absolute_offset == expected->absolute_offset &&
            actual->storage_bytes == expected->raw_bytes &&
            actual->absolute_end_offset == expected->absolute_end;
        if (!matches)
            return roundtrip_fail(
                context->failure, YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE,
                expected ? expected->name : NULL, ULLONG_MAX, ordinal,
                expected ? expected->raw_bytes : 0u,
                actual ? actual->storage_bytes : 0u,
                expected ? expected->absolute_offset : 0u, context->err,
                YVEX_ERR_FORMAT,
                "reader tensor directory row differs from writer plan");
        for (dimension = 0u; dimension < expected->rank; ++dimension)
            if (actual->dims[dimension] != expected->dims[dimension])
                return roundtrip_fail(
                    context->failure,
                    YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE, expected->name,
                    ULLONG_MAX, ordinal, expected->dims[dimension],
                    actual->dims[dimension], expected->absolute_offset,
                    context->err, YVEX_ERR_FORMAT,
                    "reader tensor dimension differs from writer plan");
    }
    return YVEX_OK;
}

/* Purpose: verify and hash the serialized structural prefix exactly once.
 * Inputs: open artifact, sealed plan prefix, bounded chunk configuration.
 * Effects: allocates scratch and advances whole-file identity and counters.
 * Failure: refuses allocation, exact-read, prefix, or hash disagreement.
 * Boundary: covers only bytes preceding the first tensor payload. */
static int roundtrip_verify_prefix(roundtrip_context *context)
{
    context->prefix = yvex_gguf_writer_plan_prefix(
        context->writer_plan, &context->prefix_bytes);
    context->buffer = (unsigned char *)malloc(
        context->options.verification_chunk_bytes);
    if (!context->prefix || !context->buffer)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_ALLOCATION, NULL,
            ULLONG_MAX, ULLONG_MAX,
            context->options.verification_chunk_bytes, 0u, 0u, context->err,
            YVEX_ERR_NOMEM,
            "roundtrip verification buffer allocation failed");
    context->summary->peak_owned_bytes =
        context->options.verification_chunk_bytes;
    yvex_artifact_identity_stream_init(&context->whole_hash);
    while (context->cursor < context->prefix_bytes) {
        size_t remaining = context->prefix_bytes - (size_t)context->cursor;
        size_t request = remaining < context->options.verification_chunk_bytes
            ? remaining : context->options.verification_chunk_bytes;
        if (yvex_artifact_read_at(context->artifact, context->cursor,
                                  context->buffer, request,
                                  context->err) != YVEX_OK)
            return roundtrip_fail(
                context->failure, YVEX_GGUF_ROUNDTRIP_SHORT_READ, NULL,
                ULLONG_MAX, ULLONG_MAX, request, 0u, context->cursor,
                context->err, YVEX_ERR_IO,
                "structural prefix exact read failed");
        if (memcmp(context->buffer, context->prefix + context->cursor,
                   request) != 0 ||
            yvex_artifact_identity_stream_update(
                &context->whole_hash, context->buffer, request,
                context->err) != YVEX_OK)
            return roundtrip_fail(
                context->failure, YVEX_GGUF_ROUNDTRIP_PREFIX_DIVERGENCE,
                NULL, ULLONG_MAX, ULLONG_MAX, request, 0u,
                context->cursor, context->err, YVEX_ERR_FORMAT,
                "serialized structural prefix differs from writer plan");
        context->cursor += request;
        context->summary->bytes_hashed += request;
        context->summary->prefix_bytes_verified += request;
        context->summary->read_calls++;
    }
    roundtrip_progress_publish(&context->options, context->summary,
                               context->plan->final_file_bytes);
    return YVEX_OK;
}

/* Purpose: verify one tensor payload digest and its canonical zero padding.
 * Inputs: verification context and canonical terminal ordinal.
 * Effects: advances file cursor, hashes, payload counters, and progress evidence.
 * Failure: refuses missing digest, short read, digest mismatch, or dirty padding.
 * Boundary: processes one terminal incrementally without retaining its payload. */
static int roundtrip_verify_terminal(roundtrip_context *context,
                                     unsigned long long ordinal)
{
    const yvex_gguf_writer_tensor *tensor =
        yvex_gguf_writer_plan_tensor_at(context->writer_plan, ordinal);
    yvex_quant_terminal_digest expected_digest;
    yvex_quant_failure quant_failure;
    yvex_sha256 terminal_hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    char terminal_hex[YVEX_SHA256_HEX_BYTES];
    unsigned long long padding;

    if (!tensor || context->cursor != tensor->absolute_offset ||
        yvex_quant_digest_sink_terminal_at(
            context->digest_sink, ordinal, &expected_digest,
            &quant_failure, context->err) != YVEX_OK)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST,
            tensor ? tensor->name : NULL, ULLONG_MAX, ordinal,
            tensor ? tensor->absolute_offset : 0u, context->cursor,
            context->cursor, context->err, YVEX_ERR_FORMAT,
            "terminal digest or canonical payload position is unavailable");
    yvex_sha256_init(&terminal_hash);
    if (!roundtrip_hash_range(
            context->artifact, context->cursor, tensor->raw_bytes,
            context->buffer, context->options.verification_chunk_bytes,
            &context->whole_hash, &terminal_hash, 0, context->summary, NULL,
            context->err) ||
        !yvex_sha256_final(&terminal_hash, digest))
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_SHORT_READ, tensor->name,
            ULLONG_MAX, ordinal, tensor->raw_bytes, 0u, context->cursor,
            context->err, YVEX_ERR_IO,
            "terminal payload exact read failed");
    yvex_sha256_hex(digest, terminal_hex);
    if (expected_digest.delivered_bytes != tensor->raw_bytes ||
        strcmp(expected_digest.sha256, terminal_hex) != 0)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST,
            tensor->name, ULLONG_MAX, ordinal,
            expected_digest.delivered_bytes, tensor->raw_bytes,
            context->cursor, context->err, YVEX_ERR_FORMAT,
            "terminal payload digest differs from quant execution");
    context->cursor += tensor->raw_bytes;
    context->summary->payload_bytes_verified += tensor->raw_bytes;
    padding = tensor->padded_bytes - tensor->raw_bytes;
    context->first_nonzero = ULLONG_MAX;
    if (padding && !roundtrip_hash_range(
            context->artifact, context->cursor, padding, context->buffer,
            context->options.verification_chunk_bytes, &context->whole_hash,
            NULL, 1, context->summary, &context->first_nonzero,
            context->err))
        return roundtrip_fail(
            context->failure,
            context->first_nonzero != ULLONG_MAX
                ? YVEX_GGUF_ROUNDTRIP_NONZERO_PADDING
                : YVEX_GGUF_ROUNDTRIP_SHORT_READ,
            tensor->name, ULLONG_MAX, ordinal, 0u, 1u,
            context->first_nonzero != ULLONG_MAX
                ? context->first_nonzero : context->cursor,
            context->err, context->first_nonzero != ULLONG_MAX
                ? YVEX_ERR_FORMAT : YVEX_ERR_IO,
            "tensor padding is nonzero or unreadable");
    context->cursor += padding;
    context->summary->padding_bytes_verified += padding;
    context->summary->terminals_verified++;
    roundtrip_progress_publish(&context->options, context->summary,
                               context->plan->final_file_bytes);
    return YVEX_OK;
}

/* Purpose: seal whole-file identity and prove the open inode remained stable.
 * Inputs: fully traversed context with before snapshot and hash stream.
 * Effects: finalizes artifact identity and publishes immutable snapshot facts.
 * Failure: refuses incomplete byte coverage, digest failure, or file drift.
 * Boundary: completes native proof but does not admit runtime support. */
static int roundtrip_finish(roundtrip_context *context)
{
    if (context->cursor != context->plan->final_file_bytes ||
        context->summary->bytes_hashed != context->plan->final_file_bytes ||
        yvex_artifact_identity_stream_final(
            &context->whole_hash, context->plan->final_file_bytes,
            context->summary->artifact_identity, context->err) != YVEX_OK)
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_ARTIFACT_DIGEST, NULL,
            ULLONG_MAX, ULLONG_MAX, context->plan->final_file_bytes,
            context->cursor, context->cursor, context->err, YVEX_ERR_FORMAT,
            "whole-file byte coverage or artifact digest failed");
    context->summary->file_bytes = context->plan->final_file_bytes;
    context->summary->payload_accepted = 1;
    if (yvex_artifact_snapshot_validate(context->artifact, &context->after,
                                        context->err) != YVEX_OK ||
        !yvex_artifact_snapshot_equal(&context->before, &context->after))
        return roundtrip_fail(
            context->failure, YVEX_GGUF_ROUNDTRIP_FILE_DRIFT,
            context->path, ULLONG_MAX, ULLONG_MAX, context->before.size,
            context->after.size, 0u, context->err, YVEX_ERR_STATE,
            "artifact file identity drifted during roundtrip");
    context->summary->snapshot_stable = 1;
    context->summary->file_device = context->before.device;
    context->summary->file_inode = context->before.inode;
    context->summary->file_mtime_seconds = context->before.mtime_seconds;
    context->summary->file_mtime_nanoseconds = context->before.mtime_nanoseconds;
    context->summary->file_ctime_seconds = context->before.ctime_seconds;
    context->summary->file_ctime_nanoseconds = context->before.ctime_nanoseconds;
    context->summary->complete = 1;
    return YVEX_OK;
}

/* Purpose: validate every planned byte of one complete temporary/final artifact.
 * Inputs: path, sealed plan, execution digests, options, and caller records.
 * Effects: bounded full-file reads plus deterministic summary/failure publication.
 * Failure: clears partial success, releases all handles/scratch, and returns typed error.
 * Boundary: borrows plan/digests and never writes or maps the complete file. */
int yvex_gguf_roundtrip_validate(
    const char *path,
    const yvex_gguf_writer_plan *writer_plan,
    yvex_quant_digest_sink *digest_sink,
    const yvex_gguf_roundtrip_options *options,
    yvex_gguf_roundtrip_summary *out,
    yvex_gguf_roundtrip_failure *failure,
    yvex_error *err)
{
    roundtrip_context context;
    unsigned long long ordinal;
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    memset(&context, 0, sizeof(context));
    context.path = path;
    context.writer_plan = writer_plan;
    context.digest_sink = digest_sink;
    context.plan = yvex_gguf_writer_plan_summary_get(writer_plan);
    context.summary = out;
    context.failure = failure;
    context.err = err;
    context.first_nonzero = ULLONG_MAX;
    rc = roundtrip_context_admit(&context, options);
    if (rc == YVEX_OK) rc = roundtrip_open_container(&context);
    if (rc == YVEX_OK) rc = roundtrip_validate_directory(&context);
    if (rc == YVEX_OK) rc = roundtrip_verify_prefix(&context);
    for (ordinal = 0u;
         rc == YVEX_OK && ordinal < context.plan->tensor_count; ++ordinal)
        rc = roundtrip_verify_terminal(&context, ordinal);
    if (rc == YVEX_OK) rc = roundtrip_finish(&context);
    if (rc == YVEX_OK) {
        if (failure) memset(failure, 0, sizeof(*failure));
        yvex_error_clear(err);
    }
    free(context.buffer);
    yvex_gguf_close(context.gguf);
    yvex_artifact_close(context.artifact);
    if (rc != YVEX_OK && out) memset(out, 0, sizeof(*out));
    return rc;
}

/* Purpose: project a native-roundtrip code to stable diagnostic text.
 * Inputs: typed roundtrip result code.
 * Effects: none.
 * Failure: unknown values map to an explicit unknown spelling.
 * Boundary: diagnostics never substitute for typed recovery logic. */
const char *yvex_gguf_roundtrip_code_name(yvex_gguf_roundtrip_code code)
{
    switch (code) {
    case YVEX_GGUF_ROUNDTRIP_OK: return "ok";
    case YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_GGUF_ROUNDTRIP_ARTIFACT_OPEN: return "artifact-open";
    case YVEX_GGUF_ROUNDTRIP_READER_REFUSAL: return "reader-refusal";
    case YVEX_GGUF_ROUNDTRIP_LAYOUT_REFUSAL: return "layout-refusal";
    case YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE: return "header-divergence";
    case YVEX_GGUF_ROUNDTRIP_METADATA_DIVERGENCE: return "metadata-divergence";
    case YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE: return "tensor-divergence";
    case YVEX_GGUF_ROUNDTRIP_PREFIX_DIVERGENCE: return "prefix-divergence";
    case YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST: return "payload-digest";
    case YVEX_GGUF_ROUNDTRIP_ARTIFACT_DIGEST: return "artifact-digest";
    case YVEX_GGUF_ROUNDTRIP_SHORT_READ: return "short-read";
    case YVEX_GGUF_ROUNDTRIP_NONZERO_PADDING: return "nonzero-padding";
    case YVEX_GGUF_ROUNDTRIP_TOKENIZER_INCOMPLETE: return "tokenizer-incomplete";
    case YVEX_GGUF_ROUNDTRIP_FILE_DRIFT: return "file-drift";
    case YVEX_GGUF_ROUNDTRIP_ALLOCATION: return "allocation";
    default: return "unknown";
    }
}

/* Purpose: report availability of native structural and payload roundtrip.
 * Inputs: optional borrowed reason output.
 * Effects: publishes a static implementation-boundary explanation.
 * Failure: this compiled capability is unconditional and returns supported.
 * Boundary: reports native verification only, never runtime/model support. */
int yvex_gguf_roundtrip_supported(const char **reason)
{
    if (reason) *reason = "native structural and full-payload roundtrip is implemented";
    return 1;
}
