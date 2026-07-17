/*
 * yvex_artifact_roundtrip_gate.c - canonical complete artifact admission.
 *
 * Owner: TRACK.ARTIFACT / TRACK.INTEGRITY.
 * Owns: one fail-closed admission result binding writer, physical emission,
 *   native/full-payload roundtrip, pinned official parsing, tokenizer facts,
 *   artifact SHA-256, and the published immutable file snapshot.
 * Does not own: production of those proofs, registry persistence,
 *   materialization, runtime descriptors, execution, generation, or release.
 * Invariants: a complete result is independently owned and never upgrades a
 *   tensor proof, external GGUF, incomplete file, or changed file snapshot.
 * Boundary: acceptance means complete artifact ready for materialization only.
 */
#include "yvex_artifact_roundtrip_gate.h"

#include <stdio.h>
#include <string.h>

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

/* Compares the stable snapshot fields captured across independent owners. */
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

/* Binds the pinned parser to the post-flush pre-publication snapshot. */
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

/*
 * Reopens the published file and produces one immutable admission fact only
 * when every supplied proof agrees with the writer plan and physical inode.
 * It reads no tensor payload; the native roundtrip already owns that proof.
 */
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

/*
 * Reconstructs the selected DeepSeek complete-artifact admission from the
 * canonical artifact-block facts and the already-open immutable artifact
 * snapshot. It reads zero payload bytes; the writer/roundtrip milestone owns
 * the full identity proof. This function only preserves that admission truth
 * for materialization consumers.
 */
int yvex_complete_artifact_admit_selected_deepseek(
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
