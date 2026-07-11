/*
 * yvex_gguf_layout_integrity.c - canonical parsed GGUF layout admission.
 *
 * Owner:
 *   src/gguf global layout integrity
 *
 * Owns:
 *   power-of-two alignment, directory-order tensor packing, padded interval
 *   continuation, aggregate span arithmetic, zero padding, tail policy, and
 *   opened-file drift checks.
 *
 * Does not own:
 *   emitted runtime-role layout mapping, payload interpretation, model-family
 *   completeness, digest policy, materialization, backend work, or generation.
 *
 * Invariants:
 *   validation is one linear tensor pass; it reads only directory and tensor
 *   padding and reports zero tensor payload bytes read.
 *
 * Boundary:
 *   accepted layout is structural container admission only.
 */
#include <yvex/gguf_layout.h>
#include <yvex/gguf_qtype.h>

#include "yvex_gguf_layout_integrity.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define YVEX_GGUF_LAYOUT_PADDING_CHUNK 4096u

/* Contract: copies owned result text without allocation or IO. */
static void layout_copy(char *dst, size_t cap, const char *text)
{
    if (!dst || cap == 0u) return;
    (void)snprintf(dst, cap, "%s", text ? text : "");
    dst[cap - 1u] = '\0';
}

/* Contract: initializes a self-contained rejected result without IO. */
static void layout_result_init(yvex_gguf_layout_result *result)
{
    memset(result, 0, sizeof(*result));
    result->code = YVEX_GGUF_LAYOUT_INVALID_ARGUMENT;
    result->tensor_index = ULLONG_MAX;
    layout_copy(result->reason, sizeof(result->reason), "layout not evaluated");
}

/* Contract: exposes pure checked aggregate arithmetic for validator and tests. */
yvex_gguf_layout_code yvex_gguf_layout_sum_checked(
    unsigned long long current,
    unsigned long long addition,
    unsigned long long *out)
{
    if (!out) return YVEX_GGUF_LAYOUT_INVALID_ARGUMENT;
    if (addition > ULLONG_MAX - current) return YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW;
    *out = current + addition;
    return YVEX_GGUF_LAYOUT_OK;
}

/* Contract: aligns through the pinned power-of-two mask with checked addition. */
static int align_up_power_of_two(unsigned long long value,
                                 unsigned int alignment,
                                 unsigned long long *out)
{
    unsigned long long mask;
    unsigned long long sum;
    if (!out || alignment == 0u || (alignment & (alignment - 1u)) != 0u) return 0;
    mask = (unsigned long long)alignment - 1ull;
    if (yvex_gguf_layout_sum_checked(value, mask, &sum) != YVEX_GGUF_LAYOUT_OK) return 0;
    *out = sum & ~mask;
    return 1;
}

/* Contract: computes one raw and padded relative interval without IO. */
yvex_gguf_layout_code yvex_gguf_layout_interval_measure(
    unsigned long long relative_offset,
    unsigned long long raw_size,
    unsigned int alignment,
    unsigned long long *raw_end,
    unsigned long long *padded_end)
{
    if (!raw_end || !padded_end) return YVEX_GGUF_LAYOUT_INVALID_ARGUMENT;
    if (alignment == 0u) return YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_VALUE;
    if ((alignment & (alignment - 1u)) != 0u) {
        return YVEX_GGUF_LAYOUT_ALIGNMENT_NOT_POWER_OF_TWO;
    }
    if (raw_size > ULLONG_MAX - relative_offset) {
        return YVEX_GGUF_LAYOUT_RAW_END_OVERFLOW;
    }
    *raw_end = relative_offset + raw_size;
    if (!align_up_power_of_two(*raw_end, alignment, padded_end)) {
        return YVEX_GGUF_LAYOUT_PADDED_END_OVERFLOW;
    }
    return YVEX_GGUF_LAYOUT_OK;
}

/* Contract: records one stable refusal and performs no cleanup or additional IO. */
static int layout_fail(yvex_gguf_layout_result *result,
                       yvex_gguf_layout_code code,
                       const char *reason,
                       yvex_error *err)
{
    result->accepted = 0;
    result->code = code;
    layout_copy(result->reason, sizeof(result->reason), reason);
    yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_gguf_layout_validate",
                    "%s: %s", yvex_gguf_layout_code_name(code), reason);
    return YVEX_ERR_FORMAT;
}

/*
 * Contract: reads exactly one required padding interval in bounded chunks,
 * records bytes and calls, and refuses the first nonzero byte without reading
 * adjacent tensor payload.
 */
static int read_zero_padding(const yvex_artifact *artifact,
                             unsigned long long offset,
                             unsigned long long length,
                             yvex_gguf_layout_code nonzero_code,
                             yvex_gguf_layout_result *result,
                             yvex_error *err)
{
    unsigned char bytes[YVEX_GGUF_LAYOUT_PADDING_CHUNK];
    unsigned long long done = 0ull;

    while (done < length) {
        unsigned long long remaining = length - done;
        size_t take = remaining > (unsigned long long)sizeof(bytes)
                          ? sizeof(bytes) : (size_t)remaining;
        size_t i;
        int rc = yvex_artifact_read_at(artifact, offset + done, bytes, take, err);
        if (rc != YVEX_OK) {
            result->failure_absolute_offset = offset + done;
            return layout_fail(result, YVEX_GGUF_LAYOUT_IO_FAILURE,
                               "required GGUF padding could not be read", err);
        }
        result->padding_bytes_read += (unsigned long long)take;
        result->padding_read_calls += 1ull;
        for (i = 0u; i < take; ++i) {
            if (bytes[i] != 0u) {
                result->failure_absolute_offset = offset + done + (unsigned long long)i;
                return layout_fail(result, nonzero_code,
                                   nonzero_code == YVEX_GGUF_LAYOUT_DIRECTORY_PADDING_NONZERO
                                       ? "GGUF directory-to-data padding contains a nonzero byte"
                                       : "GGUF tensor padding contains a nonzero byte",
                                   err);
            }
        }
        done += (unsigned long long)take;
    }
    return YVEX_OK;
}

const char *yvex_gguf_layout_code_name(yvex_gguf_layout_code code)
{
    switch (code) {
    case YVEX_GGUF_LAYOUT_OK: return "ok";
    case YVEX_GGUF_LAYOUT_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_TYPE: return "invalid-alignment-type";
    case YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_VALUE: return "invalid-alignment-value";
    case YVEX_GGUF_LAYOUT_ALIGNMENT_NOT_POWER_OF_TWO: return "alignment-not-power-of-two";
    case YVEX_GGUF_LAYOUT_ALIGNMENT_OVERFLOW: return "alignment-overflow";
    case YVEX_GGUF_LAYOUT_STORAGE_REFUSED: return "tensor-storage-refused";
    case YVEX_GGUF_LAYOUT_FIRST_OFFSET_NOT_ZERO: return "first-offset-not-zero";
    case YVEX_GGUF_LAYOUT_OFFSET_REVERSED: return "tensor-offset-reversed";
    case YVEX_GGUF_LAYOUT_UNEXPECTED_GAP: return "unexpected-layout-gap";
    case YVEX_GGUF_LAYOUT_DUPLICATE_START: return "duplicate-tensor-start";
    case YVEX_GGUF_LAYOUT_COMPLETE_OVERLAP: return "complete-tensor-overlap";
    case YVEX_GGUF_LAYOUT_PARTIAL_OVERLAP: return "partial-tensor-overlap";
    case YVEX_GGUF_LAYOUT_PADDED_OVERLAP: return "padded-tensor-overlap";
    case YVEX_GGUF_LAYOUT_RAW_END_OVERFLOW: return "tensor-raw-end-overflow";
    case YVEX_GGUF_LAYOUT_PADDED_END_OVERFLOW: return "tensor-padded-end-overflow";
    case YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW: return "layout-aggregate-overflow";
    case YVEX_GGUF_LAYOUT_TENSOR_PAYLOAD_TRUNCATED: return "tensor-payload-truncated";
    case YVEX_GGUF_LAYOUT_PADDING_TRUNCATED: return "layout-padding-truncated";
    case YVEX_GGUF_LAYOUT_DIRECTORY_PADDING_NONZERO: return "directory-padding-nonzero";
    case YVEX_GGUF_LAYOUT_TENSOR_PADDING_NONZERO: return "tensor-padding-nonzero";
    case YVEX_GGUF_LAYOUT_NONCANONICAL_TAIL: return "noncanonical-trailing-bytes";
    case YVEX_GGUF_LAYOUT_FILE_IDENTITY_DRIFT: return "file-identity-drift";
    case YVEX_GGUF_LAYOUT_IO_FAILURE: return "layout-io-failure";
    case YVEX_GGUF_LAYOUT_VIEW_FILE_MISMATCH: return "parsed-view-file-mismatch";
    case YVEX_GGUF_LAYOUT_TENSOR_INFO_MISSING: return "tensor-info-missing";
    case YVEX_GGUF_LAYOUT_TENSOR_RANGE_MISMATCH: return "tensor-range-mismatch";
    }
    return "unknown-layout-failure";
}

/*
 * Contract: borrows one artifact and immutable parsed view for a linear
 * padding-only validation pass, owns no allocation, and leaves a complete
 * typed result on success or refusal.
 */
int yvex_gguf_layout_validate(const yvex_artifact *artifact,
                              const yvex_gguf *gguf,
                              yvex_gguf_layout_result *out,
                              yvex_error *err)
{
    const yvex_gguf_reader_stats *stats;
    const yvex_gguf_value *alignment_value;
    unsigned long long alignment_metadata = 0ull;
    unsigned long long expected = 0ull;
    unsigned long long previous_start = 0ull;
    unsigned long long previous_raw_end = 0ull;
    unsigned long long i;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_layout_validate",
                       "layout output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    layout_result_init(out);
    if (!artifact || !gguf) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_INVALID_ARGUMENT,
                           "artifact and parsed GGUF view are required", err);
    }

    out->alignment = yvex_gguf_alignment(gguf);
    out->tensor_count = yvex_gguf_tensor_count(gguf);
    out->tensor_data_offset = yvex_gguf_tensor_data_offset(gguf);
    out->actual_file_size = yvex_artifact_size(artifact);
    stats = yvex_gguf_reader_stats_view(gguf);
    if (!stats || stats->file_size != out->actual_file_size) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_VIEW_FILE_MISMATCH,
                           "parsed GGUF view does not describe the opened artifact size", err);
    }
    out->structural_bytes_read = stats->structural_bytes_read;
    out->directory_end = stats->directory_end_offset;
    out->tensor_payload_bytes_read = 0ull;

    rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_FILE_IDENTITY_DRIFT,
                           "artifact identity changed before layout validation", err);
    }

    alignment_value = yvex_gguf_metadata_find(gguf, "general.alignment");
    if (alignment_value) {
        if (yvex_gguf_value_type_of(alignment_value) != YVEX_GGUF_VALUE_UINT32 ||
            yvex_gguf_value_as_u64(alignment_value, &alignment_metadata) != YVEX_OK) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_TYPE,
                               "general.alignment must have GGUF uint32 type", err);
        }
        if (alignment_metadata != (unsigned long long)out->alignment) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_VIEW_FILE_MISMATCH,
                               "parsed alignment differs from metadata", err);
        }
    }
    if (out->alignment == 0u) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_INVALID_ALIGNMENT_VALUE,
                           "general.alignment must be nonzero", err);
    }
    if ((out->alignment & (out->alignment - 1u)) != 0u) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_ALIGNMENT_NOT_POWER_OF_TWO,
                           "general.alignment must be a power of two", err);
    }
    if (!align_up_power_of_two(out->directory_end, out->alignment,
                               &out->tensor_data_offset)) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_ALIGNMENT_OVERFLOW,
                           "directory-to-data alignment overflows", err);
    }
    if (out->tensor_data_offset != yvex_gguf_tensor_data_offset(gguf)) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_VIEW_FILE_MISMATCH,
                           "parsed tensor-data offset is not the canonical aligned directory end", err);
    }
    out->directory_padding_bytes = out->tensor_data_offset - out->directory_end;
    if (out->tensor_data_offset > out->actual_file_size) {
        out->failure_absolute_offset = out->directory_end;
        return layout_fail(out, YVEX_GGUF_LAYOUT_PADDING_TRUNCATED,
                           "directory-to-data padding is truncated", err);
    }
    rc = read_zero_padding(artifact, out->directory_end,
                           out->directory_padding_bytes,
                           YVEX_GGUF_LAYOUT_DIRECTORY_PADDING_NONZERO, out, err);
    if (rc != YVEX_OK) return rc;

    for (i = 0ull; i < out->tensor_count; ++i) {
        const yvex_gguf_tensor_info *tensor = yvex_gguf_tensor_at(gguf, i);
        yvex_gguf_qtype_storage_result storage;
        yvex_gguf_qtype_storage_status storage_status;
        unsigned long long raw_end;
        unsigned long long padded_end;
        unsigned long long raw_absolute;
        unsigned long long raw_absolute_end;
        unsigned long long padded_absolute;
        unsigned long long padding_bytes;

        out->tensor_index = i;
        if (!tensor) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_TENSOR_INFO_MISSING,
                               "parsed tensor directory row is unavailable", err);
        }
        layout_copy(out->tensor_name, sizeof(out->tensor_name), tensor->name);
        out->expected_relative_offset = expected;
        out->declared_relative_offset = tensor->relative_offset;
        storage_status = yvex_gguf_qtype_tensor_storage(tensor->ggml_type,
                                                        tensor->dims,
                                                        tensor->rank,
                                                        &storage);
        if (storage_status != YVEX_GGUF_QTYPE_STORAGE_OK) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_STORAGE_REFUSED,
                               storage.reason ? storage.reason : "tensor qtype storage is refused", err);
        }
        out->tensor_raw_size = storage.total_bytes;
        if (tensor->storage_bytes != storage.total_bytes) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_STORAGE_REFUSED,
                               "parsed tensor storage differs from canonical qtype geometry", err);
        }
        {
            yvex_gguf_layout_code interval_code =
                yvex_gguf_layout_interval_measure(tensor->relative_offset,
                                                  storage.total_bytes,
                                                  out->alignment,
                                                  &raw_end,
                                                  &padded_end);
            if (interval_code != YVEX_GGUF_LAYOUT_OK) {
                return layout_fail(out, interval_code,
                                   interval_code == YVEX_GGUF_LAYOUT_RAW_END_OVERFLOW
                                       ? "tensor relative raw end overflows"
                                       : "tensor padded end overflows",
                                   err);
            }
        }
        out->tensor_raw_end = raw_end;
        out->tensor_padded_end = padded_end;
        out->tensor_padded_size = padded_end - tensor->relative_offset;

        if (i == 0ull && tensor->relative_offset != 0ull) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_FIRST_OFFSET_NOT_ZERO,
                               "the first tensor must begin at relative offset zero", err);
        }
        if (i > 0ull && tensor->relative_offset != expected) {
            if (tensor->relative_offset > expected) {
                return layout_fail(out, YVEX_GGUF_LAYOUT_UNEXPECTED_GAP,
                                   "tensor offset leaves a gap after the previous padded interval", err);
            }
            if (tensor->relative_offset == previous_start) {
                return layout_fail(out, YVEX_GGUF_LAYOUT_DUPLICATE_START,
                                   "two directory entries declare the same tensor start", err);
            }
            if (tensor->relative_offset < previous_start) {
                return layout_fail(out, YVEX_GGUF_LAYOUT_OFFSET_REVERSED,
                                   "tensor offset reverses directory order", err);
            }
            if (tensor->relative_offset < previous_raw_end) {
                return layout_fail(out,
                                   raw_end <= previous_raw_end
                                       ? YVEX_GGUF_LAYOUT_COMPLETE_OVERLAP
                                       : YVEX_GGUF_LAYOUT_PARTIAL_OVERLAP,
                                   raw_end <= previous_raw_end
                                       ? "tensor interval is contained in the previous tensor payload"
                                       : "tensor interval partially overlaps the previous tensor payload",
                                   err);
            }
            return layout_fail(out, YVEX_GGUF_LAYOUT_PADDED_OVERLAP,
                               "tensor starts inside required padding", err);
        }

        if (yvex_gguf_layout_sum_checked(out->tensor_data_offset,
                                         tensor->relative_offset,
                                         &raw_absolute) != YVEX_GGUF_LAYOUT_OK ||
            yvex_gguf_layout_sum_checked(raw_absolute,
                                         storage.total_bytes,
                                         &raw_absolute_end) != YVEX_GGUF_LAYOUT_OK ||
            yvex_gguf_layout_sum_checked(out->tensor_data_offset,
                                         padded_end,
                                         &padded_absolute) != YVEX_GGUF_LAYOUT_OK) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW,
                               "absolute tensor layout arithmetic overflows", err);
        }
        if (tensor->absolute_offset != raw_absolute ||
            tensor->absolute_end_offset != raw_absolute_end) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_TENSOR_RANGE_MISMATCH,
                               "parsed tensor absolute range differs from canonical layout", err);
        }
        if (tensor->absolute_end_offset > out->actual_file_size) {
            out->failure_absolute_offset = tensor->absolute_end_offset;
            return layout_fail(out, YVEX_GGUF_LAYOUT_TENSOR_PAYLOAD_TRUNCATED,
                               "tensor payload interval exceeds the opened file", err);
        }
        if (padded_absolute > out->actual_file_size) {
            out->failure_absolute_offset = tensor->absolute_end_offset;
            return layout_fail(out, YVEX_GGUF_LAYOUT_PADDING_TRUNCATED,
                               "required tensor padding exceeds the opened file", err);
        }
        if (yvex_gguf_layout_sum_checked(out->raw_tensor_bytes,
                                         storage.total_bytes,
                                         &out->raw_tensor_bytes) != YVEX_GGUF_LAYOUT_OK) {
            return layout_fail(out, YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW,
                               "aggregate raw tensor bytes overflow", err);
        }
        padding_bytes = padded_end - raw_end;
        if (i + 1ull < out->tensor_count) {
            if (yvex_gguf_layout_sum_checked(out->inter_tensor_padding_bytes,
                                             padding_bytes,
                                             &out->inter_tensor_padding_bytes) !=
                YVEX_GGUF_LAYOUT_OK) {
                return layout_fail(out, YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW,
                                   "aggregate inter-tensor padding overflows", err);
            }
        } else {
            out->final_tensor_padding_bytes = padding_bytes;
        }
        rc = read_zero_padding(artifact, tensor->absolute_end_offset, padding_bytes,
                               YVEX_GGUF_LAYOUT_TENSOR_PADDING_NONZERO, out, err);
        if (rc != YVEX_OK) return rc;

        previous_start = tensor->relative_offset;
        previous_raw_end = raw_end;
        expected = padded_end;
        out->tensors_validated += 1ull;
    }

    out->data_section_span = expected;
    if (yvex_gguf_layout_sum_checked(out->directory_padding_bytes,
                                     out->inter_tensor_padding_bytes,
                                     &out->total_padding_bytes) != YVEX_GGUF_LAYOUT_OK ||
        yvex_gguf_layout_sum_checked(out->total_padding_bytes,
                                     out->final_tensor_padding_bytes,
                                     &out->total_padding_bytes) != YVEX_GGUF_LAYOUT_OK ||
        yvex_gguf_layout_sum_checked(out->tensor_data_offset,
                                     out->data_section_span,
                                     &out->required_file_end) != YVEX_GGUF_LAYOUT_OK) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_AGGREGATE_OVERFLOW,
                           "aggregate GGUF layout span overflows", err);
    }
    if (out->required_file_end > out->actual_file_size) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_PADDING_TRUNCATED,
                           "GGUF file ends before the required padded data span", err);
    }
    out->trailing_bytes = out->actual_file_size - out->required_file_end;
    if (out->trailing_bytes != 0ull) {
        out->failure_absolute_offset = out->required_file_end;
        return layout_fail(out, YVEX_GGUF_LAYOUT_NONCANONICAL_TAIL,
                           "bytes after the canonical padded data span are not admitted", err);
    }
    rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK) {
        return layout_fail(out, YVEX_GGUF_LAYOUT_FILE_IDENTITY_DRIFT,
                           "artifact identity changed during layout validation", err);
    }

    out->tensor_index = ULLONG_MAX;
    out->tensor_name[0] = '\0';
    out->code = YVEX_GGUF_LAYOUT_OK;
    out->accepted = 1;
    layout_copy(out->reason, sizeof(out->reason),
                "canonical GGUF directory order, padded spans, and zero padding accepted");
    yvex_error_clear(err);
    return YVEX_OK;
}
