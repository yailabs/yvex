/*
 * YVEX - qtype support tests
 */
#include <string.h>

#include <yvex/api.h>

#include "src/backend/cuda/qtype.h"

#include "tests/test.h"

int yvex_test_qtype_support(void)
{
    const yvex_qtype_support_info *row;
    yvex_backend_qtype_fact backend;
    yvex_cuda_qtype_fact cuda;

    YVEX_TEST_ASSERT(yvex_qtype_support_count() == 43u,
                     "support projection covers every pinned qtype identity");
    row = yvex_qtype_support_by_name("F32");
    YVEX_TEST_ASSERT(row && row->policy_supported &&
                     yvex_qtype_support_storage_supported(row) && row->emit_supported,
                     "F32 row");
    YVEX_TEST_ASSERT_STREQ(yvex_qtype_support_name(row), "F32", "F32 projected name");
    row = yvex_qtype_support_by_name("F16");
    YVEX_TEST_ASSERT(row && row->emit_supported && row->quantize_supported, "F16 row");
    row = yvex_qtype_support_by_name("Q8_0");
    YVEX_TEST_ASSERT(row && row->policy_supported &&
                     yvex_qtype_support_storage_supported(row) &&
                     row->emit_supported && row->quantize_supported &&
                     row->compute_supported,
                     "Q8_0 row");
    row = yvex_qtype_support_by_name("Q2_K");
    YVEX_TEST_ASSERT(row && row->policy_supported &&
                     yvex_qtype_support_storage_supported(row) &&
                     row->emit_supported && row->quantize_supported &&
                     row->compute_supported,
                     "Q2_K row");
    row = yvex_qtype_support_by_name("Q4_2");
    YVEX_TEST_ASSERT(row && !row->policy_supported && !row->emit_supported,
                     "removed qtype projects deterministic refusal");
    yvex_backend_qtype_refuse(&backend, "cpu", "Q8_0");
    YVEX_TEST_ASSERT_STREQ(backend.compute_status, "available",
                           "CPU backend projects canonical Q8_0 compute");
    yvex_backend_qtype_refuse(&backend, "cpu", "Q4_K");
    YVEX_TEST_ASSERT_STREQ(backend.compute_status, "unavailable",
                           "CPU backend projects canonical refusal");
    yvex_cuda_qtype_refuse(&cuda, "Q2_K");
    YVEX_TEST_ASSERT_STREQ(cuda.status, "available",
                           "CUDA backend projects canonical Q2_K compute");
    YVEX_TEST_ASSERT(yvex_qtype_support_by_name("NOPE") == NULL, "unknown missing");
    return 0;
}
