/* Registry-generated accelerator qualification runner, separate from CPU default selection. */
#include <tests/test.h>
#include <tests/support/runner.h>
#include "qa/metal_registry.inc"
int main(void)
{
    return yvex_test_runner_run(yvex_metal_tests, yvex_metal_test_count,
                                "YVEX_METAL_TEST_FILTER", "metal test", 0);
}
