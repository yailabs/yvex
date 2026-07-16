/*
 * yvex_gguf_roundtrip.h - complete writer/reader equivalence ABI.
 *
 * Owner: TRACK.ARTIFACT.
 * Owns: native reader/layout comparison, exact structural-prefix comparison,
 *   per-terminal payload digest verification, whole-file artifact identity,
 *   tokenizer completeness, and snapshot-drift refusal.
 * Does not own: writing, official external reader execution, publication,
 *   support admission, materialization, runtime, or generation.
 * Invariants: one bounded sequential verification pass covers every file byte;
 *   accepted output is tied to the exact open file snapshot.
 * Boundary: YVEX roundtrip acceptance alone is not complete-artifact admission.
 */
#ifndef YVEX_GGUF_ROUNDTRIP_H
#define YVEX_GGUF_ROUNDTRIP_H

#include "yvex_gguf_writer.h"
#include "yvex_quant_sink.h"

#include <yvex/artifact_identity.h>
#include <yvex/gguf_layout.h>

typedef enum {
    YVEX_GGUF_ROUNDTRIP_OK = 0,
    YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT,
    YVEX_GGUF_ROUNDTRIP_ARTIFACT_OPEN,
    YVEX_GGUF_ROUNDTRIP_READER_REFUSAL,
    YVEX_GGUF_ROUNDTRIP_LAYOUT_REFUSAL,
    YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_METADATA_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_PREFIX_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST,
    YVEX_GGUF_ROUNDTRIP_ARTIFACT_DIGEST,
    YVEX_GGUF_ROUNDTRIP_SHORT_READ,
    YVEX_GGUF_ROUNDTRIP_NONZERO_PADDING,
    YVEX_GGUF_ROUNDTRIP_TOKENIZER_INCOMPLETE,
    YVEX_GGUF_ROUNDTRIP_FILE_DRIFT,
    YVEX_GGUF_ROUNDTRIP_ALLOCATION
} yvex_gguf_roundtrip_code;

typedef struct {
    yvex_gguf_roundtrip_code code;
    unsigned long long metadata_index;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    unsigned long long file_offset;
    char name[YVEX_GGUF_WRITER_NAME_CAP];
} yvex_gguf_roundtrip_failure;

typedef struct yvex_gguf_roundtrip_summary {
    unsigned long long file_bytes;
    unsigned long long bytes_hashed;
    unsigned long long prefix_bytes_verified;
    unsigned long long payload_bytes_verified;
    unsigned long long padding_bytes_verified;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long terminals_verified;
    unsigned long long read_calls;
    unsigned long long peak_owned_bytes;
    unsigned long long file_device;
    unsigned long long file_inode;
    long long file_mtime_seconds;
    long long file_mtime_nanoseconds;
    long long file_ctime_seconds;
    long long file_ctime_nanoseconds;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    int tokenizer_complete;
    int reader_accepted;
    int layout_accepted;
    int payload_accepted;
    int snapshot_stable;
    int complete;
} yvex_gguf_roundtrip_summary;

typedef void (*yvex_gguf_roundtrip_progress_fn)(
    void *context,
    const yvex_gguf_roundtrip_summary *summary,
    unsigned long long planned_file_bytes);

typedef struct {
    size_t verification_chunk_bytes;
    yvex_gguf_roundtrip_progress_fn progress;
    void *progress_context;
} yvex_gguf_roundtrip_options;

void yvex_gguf_roundtrip_options_default(
    yvex_gguf_roundtrip_options *options);
int yvex_gguf_roundtrip_validate(
    const char *path,
    const yvex_gguf_writer_plan *writer_plan,
    yvex_quant_digest_sink *digest_sink,
    const yvex_gguf_roundtrip_options *options,
    yvex_gguf_roundtrip_summary *out,
    yvex_gguf_roundtrip_failure *failure,
    yvex_error *err);
const char *yvex_gguf_roundtrip_code_name(yvex_gguf_roundtrip_code code);

#endif
