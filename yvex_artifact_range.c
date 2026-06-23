/*
 * YVEX - Range checks
 *
 * File: yvex_artifact_range.c
 * Layer: artifact implementation
 *
 * Purpose:
 *   Implements checked byte-range validation for mapped/read artifact views.
 *   Parsers use this helper before reading file-provided offsets or lengths.
 *
 * Implements:
 *   - yvex_range_check
 *
 * Invariants:
 *   - offset + len overflow is avoided by subtraction form
 *   - zero-length ranges at EOF are accepted
 *   - failures report YVEX_ERR_BOUNDS
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_artifact
 */
#include <yvex/artifact.h>

int yvex_range_check(unsigned long long file_size,
                     unsigned long long offset,
                     unsigned long long len,
                     yvex_error *err)
{
    if (offset > file_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_range_check",
                        "offset %llu exceeds file size %llu", offset, file_size);
        return YVEX_ERR_BOUNDS;
    }

    if (len > file_size - offset) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_range_check",
                        "range offset=%llu len=%llu exceeds file size %llu",
                        offset, len, file_size);
        return YVEX_ERR_BOUNDS;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
