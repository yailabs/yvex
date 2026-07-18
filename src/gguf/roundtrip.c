/*
 * roundtrip.c - native complete GGUF equivalence verification.
 *
 * Owner: TRACK.ARTIFACT.
 * Owns: canonical reader/layout admission, planned-field equivalence, one-pass
 *   canonical full-byte artifact identity, per-terminal digest checks,
 *   padding checks, tokenizer
 *   completeness, file-snapshot stability, and deterministic cleanup.
 * Does not own: writer execution, official reader process, publication,
 *   model-support admission, materialization, runtime, or rendering.
 * Invariants: validation uses bounded positioned reads and hashes every byte
 *   exactly once; no tensor payload is retained after its verification chunk.
 * Boundary: accepted native roundtrip is necessary but not sufficient support.
 */
#include "roundtrip.h"

#include "src/artifact/identity.h"
#include "src/core/sha256.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDTRIP_DEFAULT_CHUNK (8u * 1024u * 1024u)

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

/* Compares every stable file identity field without relying on struct padding. */
static int roundtrip_snapshot_equal(const yvex_artifact_snapshot *left,
                                    const yvex_artifact_snapshot *right)
{
    return left && right &&
           left->device == right->device &&
           left->inode == right->inode &&
           left->size == right->size &&
           left->mtime_seconds == right->mtime_seconds &&
           left->mtime_nanoseconds == right->mtime_nanoseconds &&
           left->ctime_seconds == right->ctime_seconds &&
           left->ctime_nanoseconds == right->ctime_nanoseconds;
}

/* Reads one exact range while updating whole-file and optional terminal hashes. */
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

void yvex_gguf_roundtrip_options_default(
    yvex_gguf_roundtrip_options *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->verification_chunk_bytes = ROUNDTRIP_DEFAULT_CHUNK;
}

/* Publishes a borrowed progress snapshot synchronously without retaining it. */
static void roundtrip_progress_publish(
    const yvex_gguf_roundtrip_options *options,
    const yvex_gguf_roundtrip_summary *summary,
    unsigned long long planned_file_bytes)
{
    if (options && options->progress)
        options->progress(options->progress_context, summary,
                          planned_file_bytes);
}

/*
 * Reopens one complete temp/final artifact and validates every planned byte.
 * It borrows plan/digests, owns all reader/hash scratch, and leaves no state on
 * refusal. The input file is never written or mapped in full.
 */
int yvex_gguf_roundtrip_validate(
    const char *path,
    const yvex_gguf_writer_plan *writer_plan,
    yvex_quant_digest_sink *digest_sink,
    const yvex_gguf_roundtrip_options *options,
    yvex_gguf_roundtrip_summary *out,
    yvex_gguf_roundtrip_failure *failure,
    yvex_error *err)
{
    const yvex_gguf_writer_plan_summary *plan =
        yvex_gguf_writer_plan_summary_get(writer_plan);
    yvex_gguf_roundtrip_options local;
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_artifact_snapshot before;
    yvex_artifact_snapshot after;
    yvex_gguf_reader_options reader_options;
    yvex_gguf_parse_result parse;
    yvex_gguf *gguf = NULL;
    yvex_gguf_layout_result layout;
    const yvex_gguf_header *header;
    unsigned char *buffer = NULL;
    const unsigned char *prefix;
    size_t prefix_bytes;
    yvex_artifact_identity_stream whole_hash;
    unsigned long long ordinal;
    unsigned long long cursor;
    unsigned long long first_nonzero = ULLONG_MAX;
    int rc = YVEX_OK;

    if (out) memset(out, 0, sizeof(*out));
    if (!path || !path[0] || !plan || !digest_sink || !out)
        return roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u, 0u, 0u, err,
            YVEX_ERR_INVALID_ARG,
            "artifact path, sealed plan, digest sink, and output are required");
    yvex_gguf_roundtrip_options_default(&local);
    if (options) local = *options;
    if (!local.verification_chunk_bytes ||
        local.verification_chunk_bytes > (size_t)SSIZE_MAX)
        return roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u,
            local.verification_chunk_bytes, 0u, err,
            YVEX_ERR_INVALID_ARG, "verification chunk size is invalid");
    memset(&artifact_options, 0, sizeof(artifact_options));
    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    artifact_options.path = path;
    artifact_options.readonly = 1;
    if (yvex_artifact_open(&artifact, &artifact_options, err) != YVEX_OK ||
        yvex_artifact_snapshot_get(artifact, &before, err) != YVEX_OK) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_ARTIFACT_OPEN, path,
            ULLONG_MAX, ULLONG_MAX, plan->final_file_bytes,
            artifact ? yvex_artifact_size(artifact) : 0u, 0u, err,
            YVEX_ERR_IO, "artifact open or snapshot capture failed");
        goto cleanup;
    }
    if (before.size != plan->final_file_bytes) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE, path,
            ULLONG_MAX, ULLONG_MAX, plan->final_file_bytes, before.size,
            0u, err, YVEX_ERR_FORMAT, "artifact file size differs from plan");
        goto cleanup;
    }
    yvex_gguf_reader_options_default(&reader_options);
    reader_options.max_metadata_entries = plan->metadata_count + 16u;
    reader_options.max_tensor_entries = plan->tensor_count;
    memset(&parse, 0, sizeof(parse));
    if (yvex_gguf_open_ex(
            &gguf, artifact, &reader_options, &parse, err) != YVEX_OK) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_READER_REFUSAL, path,
            parse.record_index, ULLONG_MAX, YVEX_GGUF_PARSE_OK,
            parse.code, parse.byte_offset, err, YVEX_ERR_FORMAT,
            "native GGUF reader refused emitted artifact");
        goto cleanup;
    }
    out->reader_accepted = 1;
    memset(&layout, 0, sizeof(layout));
    if (yvex_gguf_layout_validate(artifact, gguf, &layout, err) != YVEX_OK ||
        !layout.accepted) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_LAYOUT_REFUSAL,
            layout.tensor_name, ULLONG_MAX, layout.tensor_index,
            YVEX_GGUF_LAYOUT_OK, layout.code,
            layout.failure_absolute_offset, err, YVEX_ERR_FORMAT,
            "native global layout validator refused emitted artifact");
        goto cleanup;
    }
    out->layout_accepted = 1;
    header = yvex_gguf_header_view(gguf);
    if (!header || header->version != plan->gguf_version ||
        header->metadata_count != plan->metadata_count ||
        header->tensor_count != plan->tensor_count ||
        yvex_gguf_alignment(gguf) != plan->alignment ||
        yvex_gguf_tensor_data_offset(gguf) !=
            plan->structural_bytes + plan->pre_data_padding_bytes ||
        yvex_gguf_file_size(gguf) != plan->final_file_bytes) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE, path,
            ULLONG_MAX, ULLONG_MAX, plan->final_file_bytes,
            yvex_gguf_file_size(gguf), 0u, err, YVEX_ERR_FORMAT,
            "reader header/alignment/data offset differs from writer plan");
        goto cleanup;
    }
    out->metadata_count = header->metadata_count;
    out->tensor_count = header->tensor_count;
    if (plan->tokenizer_token_count &&
        (!roundtrip_array_count(
             gguf, "tokenizer.ggml.tokens", YVEX_GGUF_VALUE_STRING,
             plan->tokenizer_token_count) ||
         !roundtrip_array_count(
             gguf, "tokenizer.ggml.token_type", YVEX_GGUF_VALUE_INT32,
             plan->tokenizer_token_count) ||
         !roundtrip_array_count(
             gguf, "tokenizer.ggml.merges", YVEX_GGUF_VALUE_STRING,
             plan->tokenizer_merge_count) ||
         !yvex_gguf_metadata_find(gguf, "tokenizer.huggingface.json") ||
         !yvex_gguf_metadata_find(gguf, "yvex.tokenizer.config.json"))) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_TOKENIZER_INCOMPLETE,
            "tokenizer", ULLONG_MAX, ULLONG_MAX,
            plan->tokenizer_token_count, 0u, 0u, err, YVEX_ERR_FORMAT,
            "artifact tokenizer material is incomplete");
        goto cleanup;
    }
    out->tokenizer_complete = plan->tokenizer_token_count != 0u;
    for (ordinal = 0u; ordinal < plan->tensor_count; ++ordinal) {
        const yvex_gguf_writer_tensor *expected =
            yvex_gguf_writer_plan_tensor_at(writer_plan, ordinal);
        const yvex_gguf_tensor_info *actual =
            yvex_gguf_tensor_at(gguf, ordinal);
        unsigned int dimension;
        if (!expected || !actual ||
            actual->name_len != strlen(expected->name) ||
            memcmp(actual->name, expected->name, actual->name_len) != 0 ||
            actual->rank != expected->rank ||
            actual->ggml_type != expected->qtype ||
            actual->relative_offset != expected->relative_offset ||
            actual->absolute_offset != expected->absolute_offset ||
            actual->storage_bytes != expected->raw_bytes ||
            actual->absolute_end_offset != expected->absolute_end) {
            rc = roundtrip_fail(
                failure, YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE,
                expected ? expected->name : NULL, ULLONG_MAX, ordinal,
                expected ? expected->raw_bytes : 0u,
                actual ? actual->storage_bytes : 0u,
                expected ? expected->absolute_offset : 0u, err,
                YVEX_ERR_FORMAT,
                "reader tensor directory row differs from writer plan");
            goto cleanup;
        }
        for (dimension = 0u; dimension < expected->rank; ++dimension)
            if (actual->dims[dimension] != expected->dims[dimension]) {
                rc = roundtrip_fail(
                    failure, YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE,
                    expected->name, ULLONG_MAX, ordinal,
                    expected->dims[dimension], actual->dims[dimension],
                    expected->absolute_offset, err, YVEX_ERR_FORMAT,
                    "reader tensor dimension differs from writer plan");
                goto cleanup;
            }
    }
    prefix = yvex_gguf_writer_plan_prefix(writer_plan, &prefix_bytes);
    buffer = (unsigned char *)malloc(local.verification_chunk_bytes);
    if (!prefix || !buffer) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_ALLOCATION, NULL,
            ULLONG_MAX, ULLONG_MAX, local.verification_chunk_bytes, 0u,
            0u, err, YVEX_ERR_NOMEM,
            "roundtrip verification buffer allocation failed");
        goto cleanup;
    }
    out->peak_owned_bytes = local.verification_chunk_bytes;
    yvex_artifact_identity_stream_init(&whole_hash);
    cursor = 0u;
    while (cursor < prefix_bytes) {
        size_t request = prefix_bytes - (size_t)cursor <
                local.verification_chunk_bytes
            ? prefix_bytes - (size_t)cursor
            : local.verification_chunk_bytes;
        if (yvex_artifact_read_at(
                artifact, cursor, buffer, request, err) != YVEX_OK) {
            rc = roundtrip_fail(
                failure, YVEX_GGUF_ROUNDTRIP_SHORT_READ, NULL,
                ULLONG_MAX, ULLONG_MAX, request, 0u, cursor, err,
                YVEX_ERR_IO, "structural prefix exact read failed");
            goto cleanup;
        }
        if (memcmp(buffer, prefix + cursor, request) != 0 ||
            yvex_artifact_identity_stream_update(
                &whole_hash, buffer, request, err) != YVEX_OK) {
            rc = roundtrip_fail(
                failure, YVEX_GGUF_ROUNDTRIP_PREFIX_DIVERGENCE, NULL,
                ULLONG_MAX, ULLONG_MAX, request, 0u, cursor, err,
                YVEX_ERR_FORMAT,
                "serialized structural prefix differs from writer plan");
            goto cleanup;
        }
        cursor += request;
        out->bytes_hashed += request;
        out->prefix_bytes_verified += request;
        out->read_calls++;
    }
    roundtrip_progress_publish(&local, out, plan->final_file_bytes);
    for (ordinal = 0u; ordinal < plan->tensor_count; ++ordinal) {
        const yvex_gguf_writer_tensor *tensor =
            yvex_gguf_writer_plan_tensor_at(writer_plan, ordinal);
        yvex_quant_terminal_digest expected_digest;
        yvex_quant_failure quant_failure;
        yvex_sha256 terminal_hash;
        unsigned char terminal_digest[YVEX_SHA256_DIGEST_BYTES];
        char terminal_hex[YVEX_SHA256_HEX_BYTES];
        unsigned long long padding;

        if (!tensor || cursor != tensor->absolute_offset ||
            yvex_quant_digest_sink_terminal_at(
                digest_sink, ordinal, &expected_digest,
                &quant_failure, err) != YVEX_OK) {
            rc = roundtrip_fail(
                failure, YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST,
                tensor ? tensor->name : NULL, ULLONG_MAX, ordinal,
                tensor ? tensor->absolute_offset : 0u, cursor, cursor,
                err, YVEX_ERR_FORMAT,
                "terminal digest or canonical payload position is unavailable");
            goto cleanup;
        }
        yvex_sha256_init(&terminal_hash);
        if (!roundtrip_hash_range(
                artifact, cursor, tensor->raw_bytes, buffer,
                local.verification_chunk_bytes, &whole_hash, &terminal_hash,
                0, out, NULL, err) ||
            !yvex_sha256_final(&terminal_hash, terminal_digest)) {
            rc = roundtrip_fail(
                failure, YVEX_GGUF_ROUNDTRIP_SHORT_READ, tensor->name,
                ULLONG_MAX, ordinal, tensor->raw_bytes, 0u, cursor,
                err, YVEX_ERR_IO, "terminal payload exact read failed");
            goto cleanup;
        }
        yvex_sha256_hex(terminal_digest, terminal_hex);
        if (expected_digest.delivered_bytes != tensor->raw_bytes ||
            strcmp(expected_digest.sha256, terminal_hex) != 0) {
            rc = roundtrip_fail(
                failure, YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST,
                tensor->name, ULLONG_MAX, ordinal,
                expected_digest.delivered_bytes, tensor->raw_bytes,
                cursor, err, YVEX_ERR_FORMAT,
                "terminal payload digest differs from quant execution");
            goto cleanup;
        }
        cursor += tensor->raw_bytes;
        out->payload_bytes_verified += tensor->raw_bytes;
        padding = tensor->padded_bytes - tensor->raw_bytes;
        if (padding && !roundtrip_hash_range(
                artifact, cursor, padding, buffer,
                local.verification_chunk_bytes, &whole_hash, NULL, 1,
                out, &first_nonzero, err)) {
            rc = roundtrip_fail(
                failure, first_nonzero != ULLONG_MAX
                    ? YVEX_GGUF_ROUNDTRIP_NONZERO_PADDING
                    : YVEX_GGUF_ROUNDTRIP_SHORT_READ,
                tensor->name, ULLONG_MAX, ordinal, 0u, 1u,
                first_nonzero != ULLONG_MAX ? first_nonzero : cursor,
                err, first_nonzero != ULLONG_MAX
                    ? YVEX_ERR_FORMAT : YVEX_ERR_IO,
                "tensor padding is nonzero or unreadable");
            goto cleanup;
        }
        cursor += padding;
        out->padding_bytes_verified += padding;
        out->terminals_verified++;
        roundtrip_progress_publish(&local, out, plan->final_file_bytes);
    }
    if (cursor != plan->final_file_bytes ||
        out->bytes_hashed != plan->final_file_bytes ||
        yvex_artifact_identity_stream_final(
            &whole_hash, plan->final_file_bytes,
            out->artifact_identity, err) != YVEX_OK) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_ARTIFACT_DIGEST, NULL,
            ULLONG_MAX, ULLONG_MAX, plan->final_file_bytes, cursor,
            cursor, err, YVEX_ERR_FORMAT,
            "whole-file byte coverage or artifact digest failed");
        goto cleanup;
    }
    out->file_bytes = plan->final_file_bytes;
    out->payload_accepted = 1;
    if (yvex_artifact_snapshot_validate(artifact, &after, err) != YVEX_OK ||
        !roundtrip_snapshot_equal(&before, &after)) {
        rc = roundtrip_fail(
            failure, YVEX_GGUF_ROUNDTRIP_FILE_DRIFT, path,
            ULLONG_MAX, ULLONG_MAX, before.size, after.size,
            0u, err, YVEX_ERR_STATE,
            "artifact file identity drifted during roundtrip");
        goto cleanup;
    }
    out->snapshot_stable = 1;
    out->file_device = before.device;
    out->file_inode = before.inode;
    out->file_mtime_seconds = before.mtime_seconds;
    out->file_mtime_nanoseconds = before.mtime_nanoseconds;
    out->file_ctime_seconds = before.ctime_seconds;
    out->file_ctime_nanoseconds = before.ctime_nanoseconds;
    out->complete = 1;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);

cleanup:
    free(buffer);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    if (rc != YVEX_OK) memset(out, 0, sizeof(*out));
    return rc;
}

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

/* Reports the native complete roundtrip implementation boundary. */
int yvex_gguf_roundtrip_supported(const char **reason)
{
    if (reason) *reason = "native structural and full-payload roundtrip is implemented";
    return 1;
}
