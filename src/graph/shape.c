/*
 * YVEX - Shape helpers
 *
 * File: src/graph/shape.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Implements small checked shape helpers for F0 graph planning. These
 *   helpers validate ranks, reject zero dimensions, and detect product
 *   overflow before memory estimates use shape products.
 *
 * Implements:
 *   - yvex_shape_product
 *   - yvex_shape_equal
 *   - yvex_shape_copy
 *
 * Invariants:
 *   - ranks are bounded by YVEX_GRAPH_MAX_DIMS
 *   - dimensions must be non-zero
 *   - product overflow returns YVEX_ERR_BOUNDS
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_shape
 */
#include <yvex/graph.h>

#include <limits.h>

int yvex_shape_product(const unsigned long long *dims,
                       unsigned int rank,
                       unsigned long long *out,
                       yvex_error *err)
{
    unsigned long long product = 1;
    unsigned int i;

    if (!out || (rank > 0 && !dims)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_shape_product", "dims and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = 0;

    if (rank == 0 || rank > YVEX_GRAPH_MAX_DIMS) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_shape_product", "rank is out of range");
        return YVEX_ERR_FORMAT;
    }

    for (i = 0; i < rank; ++i) {
        if (dims[i] == 0) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_shape_product", "dimension must be non-zero");
            return YVEX_ERR_FORMAT;
        }
        if (product > ULLONG_MAX / dims[i]) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_shape_product", "dimension product overflow");
            return YVEX_ERR_BOUNDS;
        }
        product *= dims[i];
    }

    *out = product;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_shape_equal(const unsigned long long *a,
                     unsigned int a_rank,
                     const unsigned long long *b,
                     unsigned int b_rank)
{
    unsigned int i;

    if (a_rank != b_rank || a_rank > YVEX_GRAPH_MAX_DIMS || !a || !b) {
        return 0;
    }
    for (i = 0; i < a_rank; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int yvex_shape_copy(unsigned long long *dst,
                    unsigned int dst_cap,
                    const unsigned long long *src,
                    unsigned int src_rank,
                    yvex_error *err)
{
    unsigned int i;

    if (!dst || (src_rank > 0 && !src)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_shape_copy", "src and dst are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (src_rank > dst_cap || src_rank > YVEX_GRAPH_MAX_DIMS) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_shape_copy", "destination shape is too small");
        return YVEX_ERR_BOUNDS;
    }
    for (i = 0; i < src_rank; ++i) {
        if (src[i] == 0) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_shape_copy", "dimension must be non-zero");
            return YVEX_ERR_FORMAT;
        }
        dst[i] = src[i];
    }
    for (; i < dst_cap; ++i) {
        dst[i] = 0;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
