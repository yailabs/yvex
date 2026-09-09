/* Typed-program contracts. These fixtures are not model numerical evidence. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/internal/ir.h>
#include <yvex/internal/program.h>
#include "tests/test.h"

typedef struct {
    yvex_ir_module *module;
    yvex_ir_id tensor, state, index, function, block, input, memory;
    yvex_error error;
} ir_fixture;

static const char ir_source[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

static int ir_fixture_open(ir_fixture *f, uint32_t effects)
{
    yvex_ir_type type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
        .shape = {{YVEX_IR_NONE, 4u}, {YVEX_IR_NONE, 8u}}};
    yvex_ir_id signature[2];
    int rc;
    memset(f, 0, sizeof(*f));
    rc = yvex_ir_module_open(&f->module, "test", ir_source, yvex_ir_core_dialect(), 1u, &f->error);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(f->module, &type, &f->tensor, &f->error);
    type.kind = YVEX_IR_STATE;
    snprintf(type.domain, sizeof(type.domain), "ssm.recurrent");
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(f->module, &type, &f->state, &f->error);
    type = (yvex_ir_type){.kind = YVEX_IR_SCALAR, .scalar = YVEX_IR_INDEX};
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(f->module, &type, &f->index, &f->error);
    signature[0] = f->tensor;
    signature[1] = f->state;
    if (rc == YVEX_OK) rc = yvex_ir_function_add(f->module, "forward", signature, 2u,
                                                signature, 2u, effects, &f->function, &f->error);
    if (rc != YVEX_OK) return rc;
    f->block = yvex_ir_function_at(f->module, f->function)->body;
    f->input = yvex_ir_block_at(f->module, f->block)->arguments[0];
    f->memory = yvex_ir_block_at(f->module, f->block)->arguments[1];
    return YVEX_OK;
}

static int ir_add(ir_fixture *f, const char *name, const yvex_ir_id *inputs, size_t input_count,
                  const yvex_ir_id *types, size_t result_count, yvex_ir_id *op)
{
    yvex_ir_operation_request request = {.operation = name, .operands = inputs,
        .operand_count = input_count, .result_types = types, .result_count = result_count};
    return yvex_ir_operation_add(f->module, f->block, &request, op, &f->error);
}

static int ir_finish(ir_fixture *f, yvex_ir_id tensor, yvex_ir_id state)
{
    yvex_ir_id args[] = {tensor, state}, op;
    return ir_add(f, "core.return", args, 2u, NULL, 0u, &op);
}

static int ir_state_versions(void)
{
    ir_fixture f;
    yvex_ir_id op, read, next, inputs[2];
    yvex_core_bytes dump = {.maximum = 16384u};
    yvex_core_bytes encoded = {.maximum = 16384u}, imported_dump = {.maximum = 16384u};
    yvex_ir_module *imported = NULL;
    char identity[65];
    YVEX_TEST_ASSERT(ir_fixture_open(&f, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE) == YVEX_OK,
                     "open typed state program");
    YVEX_TEST_ASSERT(ir_add(&f, "state.read", &f.memory, 1u, &f.tensor, 1u, &op) == YVEX_OK,
                     "explicit state read");
    read = yvex_ir_operation_at(f.module, op)->results[0];
    inputs[0] = f.memory;
    inputs[1] = f.input;
    YVEX_TEST_ASSERT(ir_add(&f, "state.update", inputs, 2u, &f.state, 1u, &op) == YVEX_OK,
                     "semantic update produces a new value, not a storage allocation");
    next = yvex_ir_operation_at(f.module, op)->results[0];
    YVEX_TEST_ASSERT(next != f.memory && ir_finish(&f, read, next) == YVEX_OK, "return updated state");
    YVEX_TEST_ASSERT(yvex_ir_seal(f.module, &f.error) == YVEX_OK, "verify and seal");
    snprintf(identity, sizeof(identity), "%s", yvex_ir_identity(f.module));
    YVEX_TEST_ASSERT(yvex_ir_print(f.module, &dump, &f.error) == YVEX_OK, "canonical inspection");
    YVEX_TEST_ASSERT(yvex_core_bytes_append(&dump, "", 1u) &&
                     strstr((char *)dump.data, "state<ssm.recurrent; 4x8xf32>") &&
                     strstr((char *)dump.data, "state.update.v1"), "dump carries geometry and operation version");
    YVEX_TEST_ASSERT(yvex_ir_seal(f.module, &f.error) == YVEX_OK &&
                     strcmp(identity, yvex_ir_identity(f.module)) == 0, "reseal is idempotent");
    YVEX_TEST_ASSERT(yvex_ir_encode(f.module, &encoded, &f.error) == YVEX_OK &&
                     yvex_ir_decode(&imported, encoded.data, encoded.count,
                                      yvex_ir_core_dialect(), 1u, &f.error) == YVEX_OK,
                     "binary imports through the same typed verifier");
    YVEX_TEST_ASSERT(strcmp(identity, yvex_ir_identity(imported)) == 0 &&
                     yvex_ir_print(imported, &imported_dump, &f.error) == YVEX_OK &&
                     imported_dump.count + 1u == dump.count &&
                     memcmp(dump.data, imported_dump.data, imported_dump.count) == 0,
                     "binary roundtrip preserves semantic identity and canonical text exactly");
    yvex_ir_module_close(&imported);
    {
        size_t length;
        for (length = 0u; length < encoded.count; ++length)
            YVEX_TEST_ASSERT(yvex_ir_decode(&imported, encoded.data, length,
                             yvex_ir_core_dialect(), 1u, &f.error) != YVEX_OK && !imported,
                             "every truncated binary is refused without partial publication");
        encoded.data[8] ^= 1u;
        YVEX_TEST_ASSERT(yvex_ir_decode(&imported, encoded.data, encoded.count,
                         yvex_ir_core_dialect(), 1u, &f.error) != YVEX_OK && !imported,
                         "incompatible schema is refused before layout interpretation");
    }
    YVEX_TEST_ASSERT(ir_add(&f, "core.identity", &f.input, 1u, &f.tensor, 1u, &op) != YVEX_OK,
                     "sealed program cannot mutate");
    free(dump.data);
    free(encoded.data);
    free(imported_dump.data);
    yvex_ir_module_close(&f.module);
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_negative_programs(void)
{
    unsigned int scenario;
    for (scenario = 0u; scenario < 8u; ++scenario) {
        ir_fixture f;
        yvex_ir_id op, tensor, memory, inputs[2];
        YVEX_TEST_ASSERT(ir_fixture_open(&f, scenario == 0u ? 0u :
                         YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE) == YVEX_OK, "negative fixture");
        tensor = f.input;
        memory = f.memory;
        if (scenario < 3u) {
            inputs[0] = memory;
            inputs[1] = tensor;
            YVEX_TEST_ASSERT(ir_add(&f, "state.update", inputs, 2u, &f.state, 1u, &op) == YVEX_OK,
                             "construct update before verification");
            memory = yvex_ir_operation_at(f.module, op)->results[0];
            if (scenario == 1u) memory = f.memory; /* consumed version */
            if (scenario == 2u)
                YVEX_TEST_ASSERT(ir_add(&f, "state.read", &f.memory, 1u, &f.tensor, 1u, &op) == YVEX_OK,
                                 "construct stale read for verifier");
        } else if (scenario == 3u) tensor = memory; /* signature mismatch */
        else if (scenario == 4u) {
            YVEX_TEST_ASSERT(ir_add(&f, "core.identity", &tensor, 1u, &f.index, 1u, &op) == YVEX_OK,
                             "construct illegal type conversion");
        } else if (scenario == 5u) {
            YVEX_TEST_ASSERT(ir_add(&f, "state.read", &tensor, 1u, &f.tensor, 1u, &op) == YVEX_OK,
                             "construct non-state read");
        } else if (scenario == 6u) {
            yvex_ir_id other, body;
            YVEX_TEST_ASSERT(yvex_ir_function_add(f.module, "other", &f.tensor, 1u, &f.tensor, 1u,
                             0u, &other, &f.error) == YVEX_OK, "separate function");
            body = yvex_ir_function_at(f.module, other)->body;
            tensor = yvex_ir_block_at(f.module, body)->arguments[0]; /* illegal cross-function use */
            f.block = body;
            YVEX_TEST_ASSERT(ir_add(&f, "core.return", &tensor, 1u, NULL, 0u, &op) == YVEX_OK,
                             "close other function");
            f.block = yvex_ir_function_at(f.module, f.function)->body;
        }
        if (scenario != 7u)
            YVEX_TEST_ASSERT(ir_finish(&f, tensor, memory) == YVEX_OK, "terminate negative fixture");
        YVEX_TEST_ASSERT(yvex_ir_seal(f.module, &f.error) == YVEX_ERR_FORMAT &&
                         !yvex_ir_identity(f.module), "malformed program fails before execution/identity");
        yvex_ir_module_close(&f.module);
    }
    return 0;
}

static int ir_shapes_and_construction(void)
{
    ir_fixture f;
    yvex_ir_id id, batch, op, missing = YVEX_IR_NONE;
    yvex_ir_dimension dimension = {.name = "batch", .minimum = 1u, .maximum = 8u, .multiple = 1u};
    yvex_ir_type type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u};
    YVEX_TEST_ASSERT(ir_fixture_open(&f, 3u) == YVEX_OK, "shape fixture");
    YVEX_TEST_ASSERT(yvex_ir_dimension_add(f.module, &dimension, &batch, &f.error) == YVEX_OK,
                     "shared bounded symbolic dimension");
    type.shape[0] = (yvex_ir_extent){batch, 0u};
    type.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 8u};
    YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &id, &f.error) == YVEX_OK,
                     "symbolic logical tensor");
    type.shape[0].extent = 2u;
    YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &id, &f.error) != YVEX_OK,
                     "dimension cannot be static and symbolic");
    type.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, UINT64_MAX};
    YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &id, &f.error) != YVEX_OK,
                     "shape product overflow refused");
    type.shape[0] = (yvex_ir_extent){batch + 1u, 0u};
    YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &id, &f.error) != YVEX_OK,
                     "undefined dimension refused");
    type = (yvex_ir_type){.kind = YVEX_IR_TUPLE, .member_count = 1u, .members = {f.state}};
    YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &id, &f.error) == YVEX_OK,
                     "program signature may describe state members");
    {
        yvex_ir_id handle, empty, aggregate = id;
        type = (yvex_ir_type){.kind = YVEX_IR_TUPLE};
        YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &empty, &f.error) == YVEX_OK,
                         "empty argument signature is representable");
        type = (yvex_ir_type){.kind = YVEX_IR_PROGRAM, .member_count = 2u,
                              .members = {aggregate, aggregate}};
        YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &handle, &f.error) == YVEX_OK,
                         "program type represents explicit state-bearing input and output signatures");
        YVEX_TEST_ASSERT(yvex_ir_function_add(f.module, "packed", &aggregate, 1u, NULL, 0u,
                         0u, &op, &f.error) != YVEX_OK,
                         "actual state values cannot hide from version verification in aggregates");
        YVEX_TEST_ASSERT(ir_add(&f, "core.identity", &f.input, 1u, &aggregate, 1u, &op) != YVEX_OK,
                         "operation cannot manufacture packed state value");
        type.members[0] = f.tensor;
        YVEX_TEST_ASSERT(yvex_ir_type_intern(f.module, &type, &id, &f.error) != YVEX_OK,
                         "program type requires argument/result tuple signatures, not arbitrary types");
    }
    YVEX_TEST_ASSERT(ir_add(&f, "unregistered.operation", NULL, 0u, NULL, 0u, &op) != YVEX_OK,
                     "unregistered operation rejected");
    YVEX_TEST_ASSERT(ir_add(&f, "core.identity", &missing, 1u, &f.tensor, 1u, &op) != YVEX_OK,
                     "undefined operand refused at construction");
    YVEX_TEST_ASSERT(ir_finish(&f, f.input, f.memory) == YVEX_OK, "valid fixture after failed construction");
    YVEX_TEST_ASSERT(ir_finish(&f, f.input, f.memory) != YVEX_OK, "no operations after terminator");
    YVEX_TEST_ASSERT(yvex_ir_seal(f.module, &f.error) == YVEX_OK, "failed constructions do not publish partial IR");
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_regions(void)
{
    ir_fixture f;
    yvex_ir_id constant, loop, region, op, inputs[3], signature[3], results[2], root;
    yvex_ir_attribute count = {.name = "value", .kind = YVEX_IR_ATTR_U64, .value.integer = 3u};
    yvex_ir_operation_request request = {.operation = "core.index", .result_count = 1u,
        .attributes = &count, .attribute_count = 1u};
    YVEX_TEST_ASSERT(ir_fixture_open(&f, YVEX_IR_ORDERED | YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE)
                     == YVEX_OK, "region fixture");
    request.result_types = &f.index;
    YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &request, &constant, &f.error) == YVEX_OK,
                     "explicit loop count");
    inputs[0] = yvex_ir_operation_at(f.module, constant)->results[0];
    inputs[1] = f.input;
    inputs[2] = f.memory;
    signature[0] = f.index;
    signature[1] = f.tensor;
    signature[2] = f.state;
    YVEX_TEST_ASSERT(ir_add(&f, "core.loop", inputs, 3u, signature + 1u, 2u, &loop) == YVEX_OK,
                     "typed loop-carried data/state");
    YVEX_TEST_ASSERT(yvex_ir_region_add(f.module, loop, signature, 3u, &region, &f.error) == YVEX_OK,
                     "explicit region arguments");
    root = f.block;
    f.block = region;
    inputs[0] = yvex_ir_block_at(f.module, region)->arguments[2];
    inputs[1] = yvex_ir_block_at(f.module, region)->arguments[1];
    YVEX_TEST_ASSERT(ir_add(&f, "state.update", inputs, 2u, &f.state, 1u, &op) == YVEX_OK,
                     "region state version update");
    results[0] = inputs[1];
    results[1] = yvex_ir_operation_at(f.module, op)->results[0];
    YVEX_TEST_ASSERT(ir_add(&f, "core.yield", results, 2u, NULL, 0u, &op) == YVEX_OK,
                     "region yields new state");
    f.block = root;
    results[0] = yvex_ir_operation_at(f.module, loop)->results[0];
    results[1] = yvex_ir_operation_at(f.module, loop)->results[1];
    YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK &&
                     yvex_ir_seal(f.module, &f.error) == YVEX_OK, "structured iteration verified");
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_passes(void)
{
    ir_fixture f;
    yvex_ir_module *first = NULL, *second = NULL, *again = NULL;
    yvex_ir_pass passes[2] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    yvex_ir_pass_evidence evidence[2] = {0};
    yvex_ir_id op, value;
    char source_identity[65];
    YVEX_TEST_ASSERT(ir_fixture_open(&f, 3u) == YVEX_OK, "pass fixture");
    YVEX_TEST_ASSERT(ir_add(&f, "core.identity", &f.input, 1u, &f.tensor, 1u, &op) == YVEX_OK,
                     "representational alias");
    value = yvex_ir_operation_at(f.module, op)->results[0];
    {
        yvex_ir_attribute attr = {.name = "value", .kind = YVEX_IR_ATTR_U64, .value.integer = 9u};
        yvex_ir_operation_request request = {.operation = "core.index", .result_types = &f.index,
            .result_count = 1u, .attributes = &attr, .attribute_count = 1u};
        YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &request, &op, &f.error) == YVEX_OK,
                         "dead pure computation");
    }
    YVEX_TEST_ASSERT(ir_finish(&f, value, f.memory) == YVEX_OK &&
                     yvex_ir_seal(f.module, &f.error) == YVEX_OK, "seal pass input");
    snprintf(source_identity, sizeof(source_identity), "%s", yvex_ir_identity(f.module));
    YVEX_TEST_ASSERT(yvex_ir_pass_pipeline(f.module, passes, 2u, &first, evidence, &f.error) == YVEX_OK,
                     "verified deterministic pipeline");
    YVEX_TEST_ASSERT(evidence[0].input_operations == 3u && evidence[0].output_operations == 2u &&
                     evidence[1].input_operations == 2u && evidence[1].output_operations == 1u,
                     "3 -> 2 alias removal -> 1 dead-value removal");
    YVEX_TEST_ASSERT(strcmp(evidence[0].output_identity, evidence[1].input_identity) == 0 &&
                     strcmp(source_identity, yvex_ir_identity(f.module)) == 0,
                     "pass lineage connects and input remains unchanged");
    YVEX_TEST_ASSERT(yvex_ir_pass_pipeline(f.module, passes, 2u, &second, NULL, &f.error) == YVEX_OK &&
                     strcmp(yvex_ir_identity(first), yvex_ir_identity(second)) == 0,
                     "repeat produces the same canonical program");
    YVEX_TEST_ASSERT(yvex_ir_pass_pipeline(first, passes, 2u, &again, evidence, &f.error) == YVEX_OK &&
                     strcmp(yvex_ir_identity(first), yvex_ir_identity(again)) == 0,
                     "canonical pipeline reaches an idempotent fixed point");
    yvex_ir_module_close(&again);
    yvex_ir_module_close(&second);
    yvex_ir_module_close(&first);
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_calls(void)
{
    unsigned int recursive;
    for (recursive = 0u; recursive < 2u; ++recursive) {
        ir_fixture f;
        yvex_ir_id callee, body, inputs[2], types[2], op, results[2];
        yvex_ir_attribute attribute = {.name = "callee", .kind = YVEX_IR_ATTR_SYMBOL};
        yvex_ir_operation_request call = {.operation = "core.call", .operand_count = 2u,
            .result_count = 2u, .attributes = &attribute, .attribute_count = 1u};
        YVEX_TEST_ASSERT(ir_fixture_open(&f, 3u) == YVEX_OK, "component call fixture");
        types[0] = f.tensor;
        types[1] = f.state;
        inputs[0] = f.input;
        inputs[1] = f.memory;
        snprintf(attribute.value.text, sizeof(attribute.value.text), "%s", recursive ? "forward" : "component");
        if (!recursive) {
            YVEX_TEST_ASSERT(yvex_ir_function_add(f.module, "component", types, 2u, types, 2u,
                             3u, &callee, &f.error) == YVEX_OK, "typed component entrypoint");
            body = f.block;
            f.block = yvex_ir_function_at(f.module, callee)->body;
            results[0] = yvex_ir_block_at(f.module, f.block)->arguments[0];
            results[1] = yvex_ir_block_at(f.module, f.block)->arguments[1];
            YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK, "component state transfer");
            f.block = body;
        }
        call.operands = inputs;
        call.result_types = types;
        YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &call, &op, &f.error) == YVEX_OK,
                         "call has explicit operand/result values");
        results[0] = yvex_ir_operation_at(f.module, op)->results[0];
        results[1] = yvex_ir_operation_at(f.module, op)->results[1];
        YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK, "call results flow to return");
        YVEX_TEST_ASSERT(yvex_ir_seal(f.module, &f.error) == (recursive ? YVEX_ERR_FORMAT : YVEX_OK),
                         "acyclic component call accepted; implicit recursion refused");
        yvex_ir_module_close(&f.module);
    }
    return 0;
}

static int ir_call_legalization(void)
{
    ir_fixture f;
    yvex_ir_module *lowered = NULL, *repeat = NULL;
    yvex_ir_id types[2], inputs[2], results[2], component, block, op, first_state = YVEX_IR_NONE;
    yvex_ir_attribute callee = {.name = "callee", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "component"};
    yvex_ir_operation_request call = {.operation = "core.call", .operands = inputs, .operand_count = 2u,
        .result_types = types, .result_count = 2u, .attributes = &callee, .attribute_count = 1u};
    yvex_ir_pass passes[] = {*yvex_ir_inline_pass(), *yvex_ir_canonical_pass()};
    yvex_ir_pass_evidence evidence[2];
    unsigned int index, writes = 0u, reads = 0u;
    YVEX_TEST_ASSERT(ir_fixture_open(&f, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE) == YVEX_OK,
                     "stateful call legalization fixture");
    types[0] = f.tensor;
    types[1] = f.state;
    block = f.block;
    YVEX_TEST_ASSERT(yvex_ir_function_add(f.module, "component", types, 2u, types, 2u,
                     YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, &component, &f.error) == YVEX_OK,
                     "component has independent typed state signature");
    f.block = yvex_ir_function_at(f.module, component)->body;
    memcpy(inputs, yvex_ir_block_at(f.module, f.block)->arguments, sizeof(inputs));
    YVEX_TEST_ASSERT(ir_add(&f, "state.read", &inputs[1], 1u, &f.tensor, 1u, &op) == YVEX_OK,
                     "component observes its input state version");
    results[0] = yvex_ir_operation_at(f.module, op)->results[0];
    { yvex_ir_id swap = inputs[0]; inputs[0] = inputs[1]; inputs[1] = swap; }
    YVEX_TEST_ASSERT(ir_add(&f, "state.update", inputs, 2u, &f.state, 1u, &op) == YVEX_OK,
                     "component produces its own next state version");
    results[1] = yvex_ir_operation_at(f.module, op)->results[0];
    YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK, "typed component results");
    f.block = block;
    inputs[0] = f.input;
    inputs[1] = f.memory;
    for (index = 0u; index < 2u; ++index) {
        YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &call, &op, &f.error) == YVEX_OK,
                         "repeated component call");
        memcpy(results, yvex_ir_operation_at(f.module, op)->results, sizeof(results));
        inputs[1] = results[1];
    }
    YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK &&
                     yvex_ir_seal(f.module, &f.error) == YVEX_OK &&
                     yvex_ir_pass_pipeline(f.module, passes, 2u, &lowered, evidence, &f.error) == YVEX_OK,
                     "direct calls legalize transactionally into explicit data/state dependencies");
    block = yvex_ir_function_at(lowered, f.function)->body;
    for (op = yvex_ir_block_at(lowered, block)->first_operation; op != YVEX_IR_NONE;
         op = yvex_ir_operation_at(lowered, op)->next) {
        const yvex_ir_operation *operation = yvex_ir_operation_at(lowered, op);
        YVEX_TEST_ASSERT(strcmp(operation->definition->name, "core.call"), "no unresolved calls in execution form");
        if (!strcmp(operation->definition->name, "state.read")) {
            if (reads) YVEX_TEST_ASSERT(operation->operands[0] == first_state,
                                        "second component reads the first component's committed semantic successor");
            reads++;
        }
        if (!strcmp(operation->definition->name, "state.update")) {
            if (writes) YVEX_TEST_ASSERT(operation->operands[0] == first_state &&
                                         operation->results[0] != first_state, "distinct successor values after expansion");
            else first_state = operation->results[0];
            writes++;
        }
    }
    YVEX_TEST_ASSERT(reads == 2u && writes == 2u &&
                     yvex_ir_pass_pipeline(f.module, passes, 2u, &repeat, NULL, &f.error) == YVEX_OK &&
                     !strcmp(yvex_ir_identity(lowered), yvex_ir_identity(repeat)) &&
                     !strcmp(evidence[0].input_identity, yvex_ir_identity(f.module)) &&
                     yvex_ir_verify(f.module, &f.error) == YVEX_OK, "repeatable expansion preserves immutable input lineage");
    yvex_ir_module_close(&repeat);
    yvex_ir_module_close(&lowered);
    yvex_ir_module_close(&f.module);
    printf("IR call legalization: 2 stateful calls -> 2 reads / 2 updates; successor dependencies retained\n");
    return 0;
}

static int ir_identity_ordering(void)
{
    yvex_ir_module *modules[2] = {NULL, NULL};
    yvex_error error;
    unsigned int variant, index;
    for (variant = 0u; variant < 2u; ++variant) {
        yvex_ir_type type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 1u,
                             .shape = {{YVEX_IR_NONE, 8u}}};
        yvex_ir_id tensor;
        YVEX_TEST_ASSERT(yvex_ir_module_open(&modules[variant], "ordering", ir_source,
                         yvex_ir_core_dialect(), 1u, &error) == YVEX_OK, "ordered module");
        YVEX_TEST_ASSERT(yvex_ir_type_intern(modules[variant], &type, &tensor, &error) == YVEX_OK, "logical type");
        for (index = 0u; index < 2u; ++index) {
            const char *name = (variant ? 1u - index : index) ? "score" : "encode";
            yvex_ir_id function, block, op, value;
            yvex_ir_attribute attrs[2] = {
                {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "weight"},
                {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
            yvex_ir_operation_request parameter = {.operation = "core.parameter", .result_types = &tensor,
                .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
            yvex_ir_operation_request ret = {.operation = "core.return", .operands = &value, .operand_count = 1u};
            snprintf(attrs[1].value.text, sizeof(attrs[1].value.text), "%s", ir_source);
            if (variant) { yvex_ir_attribute swap = attrs[0]; attrs[0] = attrs[1]; attrs[1] = swap; }
            YVEX_TEST_ASSERT(yvex_ir_function_add(modules[variant], name, NULL, 0u, &tensor, 1u,
                             0u, &function, &error) == YVEX_OK, "independent callable output");
            block = yvex_ir_function_at(modules[variant], function)->body;
            YVEX_TEST_ASSERT(yvex_ir_operation_add(modules[variant], block, &parameter, &op, &error) == YVEX_OK,
                             "source-bound parameter");
            value = yvex_ir_operation_at(modules[variant], op)->results[0];
            YVEX_TEST_ASSERT(yvex_ir_operation_add(modules[variant], block, &ret, &op, &error) == YVEX_OK,
                             "typed result, no generation runner assumption");
        }
        YVEX_TEST_ASSERT(yvex_ir_seal(modules[variant], &error) == YVEX_OK, "seal ordered variant");
    }
    YVEX_TEST_ASSERT(strcmp(yvex_ir_identity(modules[0]), yvex_ir_identity(modules[1])) == 0,
                     "function/attribute insertion order and module-local IDs do not change identity");
    yvex_ir_module_close(&modules[1]);
    yvex_ir_module_close(&modules[0]);
    return 0;
}

static int ir_reserved_contracts(void)
{
    yvex_ir_operation_definition forged = yvex_ir_core_dialect()->operations[0];
    yvex_ir_dialect dialect = {&forged, 1u};
    yvex_ir_module *module = NULL;
    yvex_error error;
    forged.minimum_results = forged.maximum_results = 0u;
    YVEX_TEST_ASSERT(yvex_ir_module_open(&module, "forged", ir_source, &dialect, 1u, &error) != YVEX_OK &&
                     !module, "reserved canonical operation cannot be replaced by a namesake dialect contract");
    forged = yvex_ir_core_dialect()->operations[0];
    YVEX_TEST_ASSERT(yvex_ir_module_open(&module, "copied", ir_source, &dialect, 1u, &error) != YVEX_OK &&
                     !module, "core operation semantics have one static owner even for equal copies");
    forged = yvex_ir_neural_dialect()->operations[2];
    YVEX_TEST_ASSERT(yvex_ir_module_open(&module, "neural", ir_source, &dialect, 1u, &error) != YVEX_OK &&
                     !module, "standard neural semantics cannot be shadowed by an extension");
    return 0;
}

static int ir_retained_program(void)
{
    ir_fixture f;
    yvex_ir_module *references[32] = {0};
    yvex_ir_id operands[2], operation;
    char identity[YVEX_SHA256_HEX_BYTES];
    size_t index;
    YVEX_TEST_ASSERT(ir_fixture_open(&f, 0u) == YVEX_OK, "owned mutable compiler module");
    YVEX_TEST_ASSERT(!yvex_ir_module_retain(f.module, &f.error), "mutable compiler storage cannot escape through retain");
    operands[0] = f.input;
    operands[1] = f.memory;
    YVEX_TEST_ASSERT(ir_add(&f, "core.return", operands, 2u, NULL, 0u, &operation) == YVEX_OK &&
                     yvex_ir_seal(f.module, &f.error) == YVEX_OK, "immutable admitted program");
    snprintf(identity, sizeof(identity), "%s", yvex_ir_identity(f.module));
    for (index = 0u; index < 32u; ++index) {
        references[index] = yvex_ir_module_retain(f.module, &f.error);
        YVEX_TEST_ASSERT(references[index] == f.module, "retain shares immutable storage, not state/materialization copies");
    }
    yvex_ir_module_close(&f.module);
    for (index = 0u; index < 32u; ++index) {
        YVEX_TEST_ASSERT(!strcmp(yvex_ir_identity(references[index]), identity) &&
                         !strcmp(yvex_ir_source_identity(references[index]), ir_source) &&
                         yvex_ir_verify(references[index], &f.error) == YVEX_OK,
                         "closing source owner preserves independent retained compiler lifetime");
        yvex_ir_module_close(&references[index]);
        YVEX_TEST_ASSERT(!references[index], "close clears each retained owner");
    }
    return 0;
}

static int ir_sequence_constraints(void)
{
    unsigned int family, scenario;
    for (family = 0u; family < 2u; ++family) {
        for (scenario = 0u; scenario < 8u; ++scenario) {
            yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_sequence_dialect()};
            yvex_ir_module *module = NULL;
            yvex_ir_type types[10] = {0}, output;
            yvex_ir_id inputs[10], outputs[3], function, block, op, result_ids[3];
            yvex_ir_attribute delta[] = {
                {.name = "key_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = 1u},
                {.name = "value_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = 2u},
                {.name = "key_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = 2u},
                {.name = "value_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = 4u},
                {.name = "convolution_kernel", .kind = YVEX_IR_ATTR_U64, .value.integer = 3u},
                {.name = "qk_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1.0e-6},
                {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1.0e-6},
                {.name = "query_scale", .kind = YVEX_IR_ATTR_F64, .value.real = 0.7071067811865475}};
            yvex_ir_attribute attention[] = {
                {.name = "query_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = 2u},
                {.name = "kv_heads", .kind = YVEX_IR_ATTR_U64, .value.integer = 1u},
                {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = 4u},
                {.name = "rotary_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = 2u},
                {.name = "maximum_context", .kind = YVEX_IR_ATTR_U64, .value.integer = 16u},
                {.name = "theta", .kind = YVEX_IR_ATTR_U64, .value.integer = 10000u},
                {.name = "qk_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1.0e-6}};
            const uint64_t delta_shape[10][4] = {
                {3u,12u}, {3u,8u}, {3u,2u}, {3u,2u}, {12u,1u,3u}, {2u}, {2u}, {4u}, {12u,2u}, {2u,2u,4u}};
            const uint64_t attention_shape[7][4] = {
                {3u,16u}, {3u,4u}, {3u,4u}, {4u}, {4u}, {0u}, {2u,16u,1u,4u}};
            const uint32_t delta_rank[] = {2u,2u,2u,2u,3u,1u,1u,1u,2u,3u};
            const uint32_t attention_rank[] = {2u,2u,2u,1u,1u,0u,4u};
            uint32_t input_count = family ? 7u : 10u, result_count = family ? 2u : 3u;
            uint32_t state_index = family ? 6u : 8u, index, axis;
            uint32_t effects = YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE;
            yvex_ir_operation_request request = {.operation = family ? "attention.gated_causal" : "sequence.gated_delta",
                .operand_count = input_count, .result_count = result_count,
                .result_types = outputs, .attributes = family ? attention : delta, .attribute_count = family ? 7u : 8u};
            yvex_error error;
            int rc;
            for (index = 0u; index < input_count; ++index) {
                types[index].kind = index >= state_index ? YVEX_IR_STATE : YVEX_IR_TENSOR;
                types[index].scalar = !family && index >= state_index ? YVEX_IR_F32 : YVEX_IR_BF16;
                types[index].rank = family ? attention_rank[index] : delta_rank[index];
                for (axis = 0u; axis < types[index].rank; ++axis)
                    types[index].shape[axis] = (yvex_ir_extent){YVEX_IR_NONE,
                        family ? attention_shape[index][axis] : delta_shape[index][axis]};
            }
            if (family) {
                types[5].kind = YVEX_IR_SCALAR;
                types[5].scalar = YVEX_IR_INDEX;
                snprintf(types[6].domain, sizeof(types[6].domain), "attention.causal_kv");
            } else {
                snprintf(types[8].domain, sizeof(types[8].domain), "convolution.causal");
                snprintf(types[9].domain, sizeof(types[9].domain), "recurrent.gated_delta");
            }
            output = types[0];
            output.shape[1].extent = 8u;
            if (scenario == 1u) types[state_index].shape[1].extent++;
            if (scenario == 2u) types[state_index].scalar = family ? YVEX_IR_F32 : YVEX_IR_BF16;
            if (scenario == 3u) {
                if (family) attention[3].value.integer = 3u;
                else delta[1].value.integer = 3u;
            }
            if (scenario == 4u) {
                if (family) attention[6].value.real = 0.0;
                else delta[6].value.real = 0.0;
            }
            if (scenario == 5u) effects = YVEX_IR_READ_STATE;
            if (scenario == 6u) types[0].shape[1].extent++;
            YVEX_TEST_ASSERT(yvex_ir_module_open(&module, "sequence_contract", ir_source,
                                                 dialects, 2u, &error) == YVEX_OK, "sequence module");
            for (index = 0u; index < input_count; ++index)
                YVEX_TEST_ASSERT(yvex_ir_type_intern(module, &types[index], &inputs[index], &error) == YVEX_OK,
                                 "typed projected operands and unlike state geometry");
            YVEX_TEST_ASSERT(yvex_ir_type_intern(module, &output, &outputs[0], &error) == YVEX_OK, "output type");
            outputs[1] = inputs[state_index];
            if (!family) outputs[2] = inputs[9];
            YVEX_TEST_ASSERT(yvex_ir_function_add(module, "forward", inputs, input_count, outputs,
                                                  result_count, effects, &function, &error) == YVEX_OK,
                             "explicit stateful signature");
            block = yvex_ir_function_at(module, function)->body;
            memcpy(inputs, yvex_ir_block_at(module, block)->arguments, input_count * sizeof(*inputs));
            request.operands = inputs;
            YVEX_TEST_ASSERT(yvex_ir_operation_add(module, block, &request, &op, &error) == YVEX_OK,
                             "construct untrusted sequence operation before verification");
            memcpy(result_ids, yvex_ir_operation_at(module, op)->results, result_count * sizeof(*result_ids));
            if (scenario == 7u) result_ids[1] = inputs[state_index];
            request = (yvex_ir_operation_request){.operation = "core.return", .operands = result_ids,
                                                  .operand_count = result_count};
            YVEX_TEST_ASSERT(yvex_ir_operation_add(module, block, &request, &op, &error) == YVEX_OK,
                             "return candidate or intentionally stale state");
            rc = yvex_ir_seal(module, &error);
            YVEX_TEST_ASSERT(scenario == 0u ? rc == YVEX_OK : rc != YVEX_OK,
                             "accept valid state transition; reject geometry, scalar, attributes, effects and stale state");
            yvex_ir_module_close(&module);
        }
    }
    printf("IR sequence contracts: valid=2; rejected geometry/type/attribute/effect/stale-state cases=14\n");
    return 0;
}

static int ir_neural_constraints(void)
{
    unsigned int scenario;
    for (scenario = 0u; scenario < 6u; ++scenario) {
        yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
        yvex_ir_module *module = NULL;
        yvex_ir_type input = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
            .shape = {{YVEX_IR_NONE, 4u}, {YVEX_IR_NONE, 8u}}}, weight = input, output = input;
        yvex_ir_id types[3], function, block, operation, result;
        yvex_ir_attribute attrs[2] = {
            {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1.0e-6},
            {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64, .value.real = 0.0}};
        yvex_ir_operation_request request = {.operation = scenario < 2u ? "nn.linear" : "nn.rms_norm",
            .operand_count = 2u, .result_types = &types[2], .result_count = 1u};
        yvex_error error;
        if (scenario < 2u) {
            weight.shape[0].extent = output.shape[1].extent = 3u;
            if (scenario == 1u) weight.shape[1].extent = 7u;
        } else {
            weight.rank = 1u;
            weight.shape[0].extent = 8u;
            request.attributes = attrs;
            request.attribute_count = 2u;
            if (scenario == 3u) attrs[0].value.real = 0.0;
            if (scenario == 4u) attrs[0].value.real = NAN;
            if (scenario == 5u) snprintf(attrs[1].name, sizeof(attrs[1].name), "unknown");
        }
        YVEX_TEST_ASSERT(yvex_ir_module_open(&module, "neural", ir_source, dialects, 2u, &error) == YVEX_OK &&
                         yvex_ir_type_intern(module, &input, &types[0], &error) == YVEX_OK &&
                         yvex_ir_type_intern(module, &weight, &types[1], &error) == YVEX_OK &&
                         yvex_ir_type_intern(module, &output, &types[2], &error) == YVEX_OK &&
                         yvex_ir_function_add(module, "forward", types, 2u, types + 2u, 1u, 0u,
                                               &function, &error) == YVEX_OK, "neural contract fixture");
        block = yvex_ir_function_at(module, function)->body;
        request.operands = yvex_ir_block_at(module, block)->arguments;
        YVEX_TEST_ASSERT(yvex_ir_operation_add(module, block, &request, &operation, &error) == YVEX_OK,
                         "construct explicit neural operation");
        result = yvex_ir_operation_at(module, operation)->results[0];
        request = (yvex_ir_operation_request){.operation = "core.return", .operands = &result, .operand_count = 1u};
        YVEX_TEST_ASSERT(yvex_ir_operation_add(module, block, &request, &operation, &error) == YVEX_OK,
                         "close typed result signature");
        YVEX_TEST_ASSERT(yvex_ir_seal(module, &error) ==
                         (scenario == 0u || scenario == 2u ? YVEX_OK : YVEX_ERR_FORMAT),
                         "linear 4x8 * 3x8 -> 4x3 and norm accepted; bad contraction/epsilon/attribute refused");
        yvex_ir_module_close(&module);
    }
    return 0;
}

static int ir_parameter_authority(void)
{
    ir_fixture f;
    yvex_ir_attribute attrs[2] = {
        {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "weight"},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    yvex_ir_operation_request request = {.operation = "core.parameter", .result_count = 1u,
        .attributes = attrs, .attribute_count = 2u};
    yvex_ir_type other = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 1u,
        .shape = {{YVEX_IR_NONE, 8u}}};
    yvex_ir_id type, op;
    YVEX_TEST_ASSERT(ir_fixture_open(&f, 0u) == YVEX_OK &&
                     yvex_ir_type_intern(f.module, &other, &type, &f.error) == YVEX_OK, "parameter fixture");
    snprintf(attrs[1].value.text, sizeof(attrs[1].value.text), "%s", ir_source);
    request.result_types = &f.tensor;
    YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &request, &op, &f.error) == YVEX_OK,
                     "first source parameter declaration");
    request.result_types = &type;
    YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &request, &op, &f.error) == YVEX_OK &&
                     ir_finish(&f, f.input, f.memory) == YVEX_OK &&
                     yvex_ir_seal(f.module, &f.error) == YVEX_ERR_FORMAT,
                     "same source/symbol cannot acquire another logical geometry, even when unused");
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_illegal_pass(const yvex_ir_module *input, yvex_ir_module **out, yvex_error *error)
{
    (void)error;
    *out = (yvex_ir_module *)input;
    return YVEX_OK;
}

static int ir_pass_failure(void)
{
    ir_fixture f;
    yvex_ir_module *output = NULL;
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), {"test.illegal_alias", 1u, ir_illegal_pass}};
    yvex_ir_pass_evidence receipt[2], saved[2];
    char identity[65];
    YVEX_TEST_ASSERT(ir_fixture_open(&f, 0u) == YVEX_OK &&
                     ir_finish(&f, f.input, f.memory) == YVEX_OK &&
                     yvex_ir_seal(f.module, &f.error) == YVEX_OK, "failure pipeline fixture");
    snprintf(identity, sizeof(identity), "%s", yvex_ir_identity(f.module));
    memset(receipt, 0x5a, sizeof(receipt));
    memcpy(saved, receipt, sizeof(saved));
    YVEX_TEST_ASSERT(yvex_ir_pass_pipeline(f.module, passes, 2u, &output, receipt, &f.error) == YVEX_ERR_STATE &&
                     !output && !memcmp(saved, receipt, sizeof(saved)) &&
                     !strcmp(identity, yvex_ir_identity(f.module)) && yvex_ir_verify(f.module, &f.error) == YVEX_OK,
                     "failed second pass publishes neither partial module nor receipts; immutable input survives");
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_conditional_state(void)
{
    ir_fixture f;
    yvex_ir_type boolean = {.kind = YVEX_IR_SCALAR, .scalar = YVEX_IR_BOOL};
    yvex_ir_id types[3], function, branch, region, op, inputs[3], results[2];
    unsigned int side;
    YVEX_TEST_ASSERT(ir_fixture_open(&f, 0u) == YVEX_OK &&
                     ir_finish(&f, f.input, f.memory) == YVEX_OK &&
                     yvex_ir_type_intern(f.module, &boolean, &types[0], &f.error) == YVEX_OK,
                     "conditional state fixture");
    types[1] = f.tensor;
    types[2] = f.state;
    YVEX_TEST_ASSERT(yvex_ir_function_add(f.module, "choose", types, 3u, types + 1u, 2u,
                     YVEX_IR_ORDERED | YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
                     &function, &f.error) == YVEX_OK, "explicit condition and state inputs");
    f.block = yvex_ir_function_at(f.module, function)->body;
    memcpy(inputs, yvex_ir_block_at(f.module, f.block)->arguments, sizeof(inputs));
    YVEX_TEST_ASSERT(ir_add(&f, "core.if", inputs, 3u, types + 1u, 2u, &branch) == YVEX_OK,
                     "typed conditional results");
    for (side = 0u; side < 2u; ++side) {
        yvex_ir_id update[2];
        YVEX_TEST_ASSERT(yvex_ir_region_add(f.module, branch, types + 1u, 2u, &region, &f.error) == YVEX_OK,
                         "branch receives independent explicit state argument");
        f.block = region;
        results[0] = yvex_ir_block_at(f.module, region)->arguments[0];
        results[1] = yvex_ir_block_at(f.module, region)->arguments[1];
        if (side) {
            update[0] = results[1];
            update[1] = results[0];
            YVEX_TEST_ASSERT(ir_add(&f, "state.update", update, 2u, &f.state, 1u, &op) == YVEX_OK,
                             "one branch produces a new state version");
            results[1] = yvex_ir_operation_at(f.module, op)->results[0];
        }
        YVEX_TEST_ASSERT(ir_add(&f, "core.yield", results, 2u, NULL, 0u, &op) == YVEX_OK,
                         "both branches yield compatible data/state signatures");
    }
    f.block = yvex_ir_function_at(f.module, function)->body;
    memcpy(results, yvex_ir_operation_at(f.module, branch)->results, sizeof(results));
    YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK &&
                     yvex_ir_function_add(f.module, "dispatch", types, 3u, types + 1u, 2u,
                         YVEX_IR_ORDERED | YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
                         &function, &f.error) == YVEX_OK, "callable conditional state flow");
    {
        yvex_ir_attribute callee = {.name = "callee", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "choose"};
        yvex_ir_operation_request call = {.operation = "core.call", .operand_count = 3u,
            .result_types = types + 1u, .result_count = 2u, .attributes = &callee, .attribute_count = 1u};
        yvex_ir_module *lowered = NULL;
        yvex_program_execution *execution = NULL;
        unsigned int branches = 0u;
        f.block = yvex_ir_function_at(f.module, function)->body;
        memcpy(inputs, yvex_ir_block_at(f.module, f.block)->arguments, sizeof(inputs));
        call.operands = inputs;
        YVEX_TEST_ASSERT(yvex_ir_operation_add(f.module, f.block, &call, &op, &f.error) == YVEX_OK,
                         "component invocation includes explicit condition/data/state");
        memcpy(results, yvex_ir_operation_at(f.module, op)->results, sizeof(results));
        YVEX_TEST_ASSERT(ir_finish(&f, results[0], results[1]) == YVEX_OK &&
                         yvex_ir_seal(f.module, &f.error) == YVEX_OK &&
                         yvex_ir_pass_pipeline(f.module, yvex_ir_inline_pass(), 1u,
                                               &lowered, NULL, &f.error) == YVEX_OK,
                         "conditional regions survive call legalization with explicit state arguments");
        for (op = 0u; op < yvex_ir_operation_count(lowered); ++op) {
            const yvex_ir_operation *operation = yvex_ir_operation_at(lowered, op);
            YVEX_TEST_ASSERT(strcmp(operation->definition->name, "core.call"), "conditional calls fully expanded");
            branches += !strcmp(operation->definition->name, "core.if");
        }
        YVEX_TEST_ASSERT(branches == 2u, "original callable conditional and expanded caller retain their regions");
        YVEX_TEST_ASSERT(yvex_program_execution_compile(&execution, f.module, &f.error) == YVEX_ERR_UNSUPPORTED &&
            !execution && yvex_program_execution_compile(&execution, lowered, &f.error) == YVEX_ERR_UNSUPPORTED &&
            !execution, "straight-line execution lowering refuses unlegalized regions/calls instead of flattening them");
        yvex_ir_module_close(&lowered);
    }
    yvex_ir_module_close(&f.module);
    return 0;
}

static int ir_shape_identity(void)
{
    ir_fixture f[2];
    unsigned int variant;
    for (variant = 0u; variant < 2u; ++variant) {
        yvex_ir_dimension dim = {.name = "a", .minimum = 1u, .maximum = 8u, .multiple = 1u};
        yvex_ir_type tensor = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32};
        yvex_ir_id a, ax1, type, function, input, operation;
        YVEX_TEST_ASSERT(ir_fixture_open(&f[variant], 0u) == YVEX_OK &&
                         ir_finish(&f[variant], f[variant].input, f[variant].memory) == YVEX_OK &&
                         yvex_ir_dimension_add(f[variant].module, &dim, &a, &f[variant].error) == YVEX_OK,
                         "first shape symbol");
        snprintf(dim.name, sizeof(dim.name), "ax1");
        YVEX_TEST_ASSERT(yvex_ir_dimension_add(f[variant].module, &dim, &ax1, &f[variant].error) == YVEX_OK,
                         "symbol name contains the dimension separator spelling");
        tensor.rank = variant ? 1u : 2u;
        tensor.shape[0] = (yvex_ir_extent){variant ? ax1 : a, 0u};
        if (!variant) tensor.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 1u};
        YVEX_TEST_ASSERT(yvex_ir_type_intern(f[variant].module, &tensor, &type, &f[variant].error) == YVEX_OK &&
                         yvex_ir_function_add(f[variant].module, "shape", &type, 1u, &type, 1u,
                                               0u, &function, &f[variant].error) == YVEX_OK,
                         "rank two [$a,1] and rank one [$ax1] are different signatures");
        f[variant].block = yvex_ir_function_at(f[variant].module, function)->body;
        input = yvex_ir_block_at(f[variant].module, f[variant].block)->arguments[0];
        YVEX_TEST_ASSERT(ir_add(&f[variant], "core.return", &input, 1u, NULL, 0u, &operation) == YVEX_OK &&
                         yvex_ir_seal(f[variant].module, &f[variant].error) == YVEX_OK,
                         "seal exact shape semantics");
    }
    YVEX_TEST_ASSERT(strcmp(yvex_ir_identity(f[0].module), yvex_ir_identity(f[1].module)) != 0,
                     "symbol delimiters prevent different logical ranks/shapes from hashing to identical text");
    yvex_ir_module_close(&f[1].module);
    yvex_ir_module_close(&f[0].module);
    return 0;
}

int yvex_test_ir(void)
{
    if (ir_state_versions() || ir_negative_programs() || ir_shapes_and_construction() ||
        ir_regions() || ir_passes() || ir_calls() || ir_identity_ordering() || ir_reserved_contracts() ||
        ir_neural_constraints() || ir_parameter_authority() || ir_pass_failure() || ir_conditional_state() ||
        ir_shape_identity() || ir_sequence_constraints() || ir_retained_program() || ir_call_legalization()) return 1;
    fprintf(stderr, "IR contracts: typed state/read/update/loop accepted; stale state, wrong effects, types, "
                    "dominance, shapes, arity and unknown operations refused before seal. No model execution claim.\n");
    return 0;
}
