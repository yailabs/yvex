/*
 * tests/test_cuda.c - CUDA YVEX test runner.
 *
 * This runner groups CUDA probe, tensor, kernel parity, and materialization coverage.
 */

#include "test.h"

int yvex_cuda_test_info(void);
int yvex_cuda_test_tensor(void);
int yvex_cuda_test_ops(void);
int yvex_cuda_test_parity(void);
int yvex_cuda_test_quant_qtype(void);
int yvex_cuda_test_materialize_cuda(void);

static int run_cuda_test(const char *name, int (*fn)(void))
{
    int rc;

    fprintf(stderr, "cuda test: %s\n", name);
    rc = fn();
    if (rc != 0) {
        fprintf(stderr, "FAIL: %s exited %d\n", name, rc);
    }
    return rc;
}

int main(void)
{
    if (run_cuda_test("info", yvex_cuda_test_info) != 0) return 1;
    if (run_cuda_test("tensor", yvex_cuda_test_tensor) != 0) return 1;
    if (run_cuda_test("ops", yvex_cuda_test_ops) != 0) return 1;
    if (run_cuda_test("parity", yvex_cuda_test_parity) != 0) return 1;
    if (run_cuda_test("quant_qtype", yvex_cuda_test_quant_qtype) != 0) return 1;
    if (run_cuda_test("materialize_cuda", yvex_cuda_test_materialize_cuda) != 0) return 1;
    return 0;
}
