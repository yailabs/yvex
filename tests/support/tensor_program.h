/* Small typed FFN used by compiler and physical-execution contract tests. */
#ifndef TESTS_SUPPORT_TENSOR_PROGRAM_H
#define TESTS_SUPPORT_TENSOR_PROGRAM_H
#include <yvex/internal/program.h>
#include <string.h>

static int test_tensor_program(yvex_program_tensor_plan **out, int duplicate_result, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *module = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u};
    yvex_ir_id dimension, hidden, intermediate, signature[4], returns[2], function, op;
    yvex_ir_id block = YVEX_IR_NONE;
    yvex_ir_id args[4], values[4], operands[2], results[2];
    size_t i;
    int rc = yvex_ir_module_open(&module, "tensor_fixture", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(module, &rows, &dimension, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dimension, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 32u};
        rc = yvex_ir_type_intern(module, &t, &hidden, err);
    }
    if (rc == YVEX_OK) {
        signature[0] = returns[0] = returns[1] = hidden;
        t.shape[1].extent = 64u;
        rc = yvex_ir_type_intern(module, &t, &intermediate, err);
    }
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 64u};
        t.shape[1].extent = 32u;
        rc = yvex_ir_type_intern(module, &t, &signature[1], err);
    }
    if (rc == YVEX_OK) {
        signature[2] = signature[1];
        t.shape[0].extent = 32u;
        t.shape[1].extent = 64u;
        rc = yvex_ir_type_intern(module, &t, &signature[3], err);
    }
    if (rc == YVEX_OK)
        rc = yvex_ir_function_add(module, "forward", signature, 4u, returns,
                                  duplicate_result ? 2u : 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(module, function)->body;
        memcpy(args, yvex_ir_block_at(module, block)->arguments, sizeof(args));
    }
    for (i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        yvex_ir_operation_request r = {.operation = i == 2u ? "nn.silu_product" : "nn.linear",
            .operands = operands, .operand_count = 2u, .result_types = i == 3u ? &hidden : &intermediate,
            .result_count = 1u};
        operands[0] = i < 2u ? args[0] : i == 2u ? values[0] : values[2];
        operands[1] = i < 2u ? args[i + 1u] : i == 2u ? values[1] : args[3];
        rc = yvex_ir_operation_add(module, block, &r, &op, err);
        if (rc == YVEX_OK) values[i] = yvex_ir_operation_at(module, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = results,
            .operand_count = duplicate_result ? 2u : 1u};
        results[0] = results[1] = values[3];
        rc = yvex_ir_operation_add(module, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(module, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, module, err);
    if (rc == YVEX_OK) rc = yvex_program_tensor_compile(out, execution, "forward", err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&module);
    return rc;
}
#endif
