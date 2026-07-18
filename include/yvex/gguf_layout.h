/*
 * Owner: abi.gguf_layout (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Canonical GGUF global layout admission
 *
 * File: include/yvex/gguf_layout.h
 * Layer: public format API
 *
 * Purpose:
 *   Defines the typed, immutable result of validating canonical GGUF tensor
 *   order, padded continuation, complete file span, and required zero padding.
 *
 * Owns:
 *   - global directory-order layout admission
 *   - power-of-two alignment admission
 *   - aggregate raw, padding, data-span, and tail facts
 *   - padding-only IO accounting and stable refusal codes
 *
 * Does not own:
 *   - runtime-role-to-emitted-layout mapping
 *   - tensor payload interpretation or model completeness
 *   - digest policy, materialization, backend execution, or generation
 *
 * Boundary:
 *   an accepted global layout is a container property, not a supported model
 *   artifact or runtime capability.
 */
#ifndef YVEX_GGUF_LAYOUT_H
#define YVEX_GGUF_LAYOUT_H

#include <yvex/artifact.h>
#include <yvex/error.h>
#include <yvex/gguf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_GGUF_LAYOUT_TENSOR_NAME_CAP 128u
#define YVEX_GGUF_LAYOUT_REASON_CAP 256u

typedef enum {
    YVEX_GGUF_LAYOUT_OK = 0,
    YVEX_GGUF_LAYOUT_INVALID_ARGUMENT = 1,
    YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_TYPE = 2,
    YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_VALUE = 3,
    YVEX_GGUF_LAYOUT_ALIGNMENT_NOT_POWER_OF_TWO = 4,
    YVEX_GGUF_LAYOUT_ALIGNMENT_OVERFLOW = 5,
    YVEX_GGUF_LAYOUT_STORAGE_REFUSED = 6,
    YVEX_GGUF_LAYOUT_FIRST_OFFSET_NOT_ZERO = 7,
    YVEX_GGUF_LAYOUT_OFFSET_REVERSED = 8,
    YVEX_GGUF_LAYOUT_UNEXPECTED_GAP = 9,
    YVEX_GGUF_LAYOUT_DUPLICATE_START = 10,
    YVEX_GGUF_LAYOUT_COMPLETE_OVERLAP = 11,
    YVEX_GGUF_LAYOUT_PARTIAL_OVERLAP = 12,
    YVEX_GGUF_LAYOUT_PADDED_OVERLAP = 13,
    YVEX_GGUF_LAYOUT_RAW_END_OVERFLOW = 14,
    YVEX_GGUF_LAYOUT_PADDED_END_OVERFLOW = 15,
    YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW = 16,
    YVEX_GGUF_LAYOUT_TENSOR_PAYLOAD_TRUNCATED = 17,
    YVEX_GGUF_LAYOUT_PADDING_TRUNCATED = 18,
    YVEX_GGUF_LAYOUT_DIRECTORY_PADDING_NONZERO = 19,
    YVEX_GGUF_LAYOUT_TENSOR_PADDING_NONZERO = 20,
    YVEX_GGUF_LAYOUT_NONCANONICAL_TAIL = 21,
    YVEX_GGUF_LAYOUT_FILE_IDENTITY_DRIFT = 22,
    YVEX_GGUF_LAYOUT_IO_FAILURE = 23,
    YVEX_GGUF_LAYOUT_VIEW_FILE_MISMATCH = 24,
    YVEX_GGUF_LAYOUT_TENSOR_INFO_MISSING = 25,
    YVEX_GGUF_LAYOUT_TENSOR_RANGE_MISMATCH = 26
} yvex_gguf_layout_code;

typedef struct {
    yvex_gguf_layout_code code;
    int accepted;
    char reason[YVEX_GGUF_LAYOUT_REASON_CAP];
    unsigned int alignment;
    unsigned long long tensor_count;
    unsigned long long tensors_validated;
    unsigned long long tensor_index;
    char tensor_name[YVEX_GGUF_LAYOUT_TENSOR_NAME_CAP];
    unsigned long long expected_relative_offset;
    unsigned long long declared_relative_offset;
    unsigned long long tensor_raw_size;
    unsigned long long tensor_padded_size;
    unsigned long long tensor_raw_end;
    unsigned long long tensor_padded_end;
    unsigned long long failure_absolute_offset;
    unsigned long long directory_end;
    unsigned long long directory_padding_bytes;
    unsigned long long tensor_data_offset;
    unsigned long long raw_tensor_bytes;
    unsigned long long inter_tensor_padding_bytes;
    unsigned long long final_tensor_padding_bytes;
    unsigned long long total_padding_bytes;
    unsigned long long data_section_span;
    unsigned long long required_file_end;
    unsigned long long actual_file_size;
    unsigned long long trailing_bytes;
    unsigned long long structural_bytes_read;
    unsigned long long padding_bytes_read;
    unsigned long long tensor_payload_bytes_read;
    unsigned long long padding_read_calls;
} yvex_gguf_layout_result;

const char *yvex_gguf_layout_code_name(yvex_gguf_layout_code code);

yvex_gguf_layout_code yvex_gguf_layout_interval_measure(
    unsigned long long relative_offset,
    unsigned long long raw_size,
    unsigned int alignment,
    unsigned long long *raw_end,
    unsigned long long *padded_end);
yvex_gguf_layout_code yvex_gguf_layout_sum_checked(
    unsigned long long current,
    unsigned long long addition,
    unsigned long long *out);

/*
 * Validates one parsed view against the exact opened artifact snapshot. The
 * result owns all diagnostic text and remains valid independently of both
 * borrowed inputs. Tensor payload bytes are never read.
 */
int yvex_gguf_layout_validate(const yvex_artifact *artifact,
                              const yvex_gguf *gguf,
                              yvex_gguf_layout_result *out,
                              yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GGUF_LAYOUT_H */
