/*
 * YVEX - qtype support tests
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

int yvex_test_qtype_support(void)
{
    const yvex_qtype_support_info *row;

    YVEX_TEST_ASSERT(yvex_qtype_support_count() > 0, "support count");
    row = yvex_qtype_support_by_name("F32");
    YVEX_TEST_ASSERT(row && row->policy_supported && row->storage_supported && row->emit_supported, "F32 row");
    row = yvex_qtype_support_by_name("F16");
    YVEX_TEST_ASSERT(row && row->emit_supported && row->quantize_supported, "F16 row");
    row = yvex_qtype_support_by_name("Q8_0");
    YVEX_TEST_ASSERT(row && row->policy_supported && row->storage_supported && !row->emit_supported, "Q8_0 row");
    row = yvex_qtype_support_by_name("Q2_K");
    YVEX_TEST_ASSERT(row && row->policy_supported && !row->emit_supported, "Q2_K row");
    YVEX_TEST_ASSERT(yvex_qtype_support_by_name("NOPE") == NULL, "unknown missing");
    return 0;
}
