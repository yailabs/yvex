/* Parameter lowering evidence uses metadata, never model payloads or execution. */
#include "tests/test.h"
#include "tests/support/tensor_program.h"
#include "tests/support/linear_program.h"
#include "tests/support/mhc_program.h"
#include "tests/support/program_sequence.h"

#include <yvex/internal/compilation.h>
#include <yvex/internal/core.h>
#include <yvex/internal/execution.h>
#include <yvex/internal/program.h>
#include <yvex/internal/program_kernels.h>
#include <yvex/qtype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char program_source[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char program_other[] =
    "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

static int program_test_module(yvex_ir_module **out, int variant, yvex_error *err)
{
    yvex_ir_type type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
        .shape = {{YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 4u}}};
    yvex_ir_id tensor, function, block, op, value;
    unsigned int index;
    int rc = yvex_ir_module_open(out, "parameters", program_source, yvex_ir_core_dialect(), 1u, err);
    if (variant == 12 && rc == YVEX_OK) {
        yvex_ir_dimension dim = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
        rc = yvex_ir_dimension_add(*out, &dim, &type.shape[0].symbol, err);
        type.shape[0].extent = 0u;
    }
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(*out, &type, &tensor, err);
    for (index = 0u; index < 2u && rc == YVEX_OK; ++index) {
        yvex_ir_attribute attributes[] = {
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT},
            {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "weight"}};
        yvex_ir_operation_request request = {.operation = "core.parameter", .result_types = &tensor,
            .result_count = 1u, .attributes = attributes, .attribute_count = 2u};
        yvex_core_text_copy(attributes[0].value.text, sizeof(attributes[0].value.text), program_source);
        rc = yvex_ir_function_add(*out, index ? "second" : "first", NULL, 0u, &tensor, 1u,
                                  0u, &function, err);
        if (rc != YVEX_OK) break;
        block = yvex_ir_function_at(*out, function)->body;
        rc = yvex_ir_operation_add(*out, block, &request, &op, err);
        if (rc != YVEX_OK) break;
        value = yvex_ir_operation_at(*out, op)->results[0];
        request = (yvex_ir_operation_request){.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(*out, block, &request, &op, err);
    }
    return rc == YVEX_OK ? yvex_ir_seal(*out, err) : rc;
}

static int program_test_transform(yvex_transform_ir **out, int variant, yvex_error *err)
{
    unsigned int copies = variant == 5 || variant == 10 ? 2u : 1u, index;
    yvex_transform_header header = {.schema_version = YVEX_TRANSFORM_IR_SCHEMA_VERSION,
        .logical_model_identity = program_other, .source_snapshot_identity = 7u, .coverage_identity = 9u,
        .required_payload_identity = variant == 1 ? program_other : program_source,
        .payload_trust_class = "local_payload_snapshot_sealed", .expected_source_count = 1u,
        .expected_terminal_count = copies};
    yvex_transform_source_spec source = {.source_name = variant == 2 ? "another_weight" : "weight",
        .shard_name = "fixture.safetensors", .source_snapshot_identity = 7u,
        .source_dtype = variant == 4 ? YVEX_NATIVE_DTYPE_F32 : YVEX_NATIVE_DTYPE_BF16,
        .value_dtype = variant == 4 ? YVEX_TRANSFORM_DTYPE_F32 : YVEX_TRANSFORM_DTYPE_BF16,
        .shape = {.rank = 2u, .dims = {3u, 4u}}, .relative_begin = 0u, .relative_end = variant == 4 ? 48u : 24u,
        .requirement_identity = 11u, .scope = YVEX_TRANSFORM_SCOPE_GLOBAL,
        .subsystem = YVEX_TRANSFORM_SUBSYSTEM_OUTPUT, .role_hint = YVEX_TENSOR_ROLE_OUTPUT_HEAD,
        .layer_index = YVEX_TRANSFORM_IR_NO_ID, .auxiliary_index = YVEX_TRANSFORM_IR_NO_ID,
        .expert_index = YVEX_TRANSFORM_IR_NO_ID, .required_uses = copies};
    yvex_transform_builder *builder = NULL;
    yvex_transform_failure failure;
    unsigned long long input, output, node;
    int rc;
    if (variant == 3) { source.shape.dims[0] = 4u; source.shape.dims[1] = 3u; }
    rc = yvex_transform_builder_create(&builder, &header, NULL, &failure, err);
    if (rc == YVEX_OK) rc = yvex_transform_builder_add_source(builder, &source, &input, &failure, err);
    for (index = 0u; rc == YVEX_OK && index < copies; ++index) {
        yvex_transform_value_spec terminal = {.kind = YVEX_TRANSFORM_VALUE_TERMINAL,
            .semantic_id = 100u + index, .canonical_ordinal = index, .shape = source.shape,
            .dtype = source.value_dtype, .precision = {.flags = YVEX_TRANSFORM_PRECISION_EXACT,
                .allowed_physical_classes = YVEX_TRANSFORM_PHYSICAL_BF16 | YVEX_TRANSFORM_PHYSICAL_F32},
            .logical_key = {.scope = YVEX_TRANSFORM_SCOPE_GLOBAL, .subsystem = YVEX_TRANSFORM_SUBSYSTEM_OUTPUT,
                .role = YVEX_TENSOR_ROLE_OUTPUT_HEAD, .layer_index = YVEX_TRANSFORM_IR_NO_ID,
                .auxiliary_index = YVEX_TRANSFORM_IR_NO_ID, .group_index = index}};
        yvex_transform_node_spec operation = {.kind = YVEX_TRANSFORM_OP_IDENTITY,
            .input_value_ids = &input, .input_count = 1u, .numeric = YVEX_TRANSFORM_NUMERIC_EXACT,
            .ordering = YVEX_TRANSFORM_ORDER_INPUT, .payload_execution_required = 1};
        if (variant == 13) terminal.precision.allowed_physical_classes = YVEX_TRANSFORM_PHYSICAL_BF16;
        if (variant == 11) {
            operation.kind = YVEX_TRANSFORM_OP_TRANSPOSE;
            operation.ordering = YVEX_TRANSFORM_ORDER_AXIS;
            operation.permutation_rank = 2u;
            operation.permutation[0] = 1u;
            operation.permutation[1] = 0u;
            terminal.shape.dims[0] = 4u;
            terminal.shape.dims[1] = 3u;
        }
        rc = yvex_transform_builder_declare_value(builder, &terminal, &output, &failure, err);
        operation.output_value_id = output;
        if (rc == YVEX_OK) rc = yvex_transform_builder_add_node(builder, &operation, &node, &failure, err);
    }
    if (rc == YVEX_OK) rc = yvex_transform_builder_seal(builder, out, &failure, err);
    yvex_transform_builder_release(&builder);
    return rc;
}

static int program_test_physical(yvex_physical_execution_ir **out, int variant, yvex_error *err)
{
    unsigned int count = variant == 5 || variant == 10 ? 2u : 1u, index;
    yvex_physical_execution_summary summary = {.schema_version = YVEX_PHYSICAL_EXECUTION_SCHEMA_V5,
        .decision_count = count};
    yvex_physical_execution_decision decisions[2] = {0};
    yvex_core_text_copy(summary.physical_variant_identity, sizeof(summary.physical_variant_identity), program_other);
    for (index = 0u; index < count; ++index) {
        decisions[index] = (yvex_physical_execution_decision){.schema_version = YVEX_PHYSICAL_EXECUTION_SCHEMA_V5,
            .terminal_tensor_id = variant == 10 ? 0u : index,
            .role = variant == 9 ? YVEX_TENSOR_ROLE_OUTPUT_NORM : YVEX_TENSOR_ROLE_OUTPUT_HEAD,
            .scope = YVEX_TENSOR_SCOPE_GLOBAL, .layer_index = YVEX_TRANSFORM_IR_NO_ID,
            .predictor_index = YVEX_TRANSFORM_IR_NO_ID, .canonical_qtype = YVEX_GGUF_QTYPE_BF16,
            .canonical_row_width = 4u, .canonical_row_count = 3u,
            .encoded_offset = 64u + index * 64u, .encoded_bytes = variant == 8 ? 22u : 24u, .alignment = 32u,
            .consumer = YVEX_EXECUTION_CONSUMER_OUTPUT_HEAD, .layout = YVEX_EXECUTION_LAYOUT_CANONICAL_ROW,
            .sharing = YVEX_EXECUTION_SHARING_MODEL_READ_ONLY};
        if (variant == 6) { decisions[index].canonical_row_width = 3u; decisions[index].canonical_row_count = 4u; }
        if (variant == 7) decisions[index].canonical_qtype = 99u;
        if (variant == 13 || variant == 14) {
            decisions[index].canonical_qtype = YVEX_GGUF_QTYPE_F32;
            decisions[index].encoded_bytes = 48u;
        }
        yvex_core_text_copy(decisions[index].terminal_identity, sizeof(decisions[index].terminal_identity), program_source);
    }
    return yvex_physical_execution_ir_import(out, &summary, decisions, count, err);
}

static int program_execution_fixture(yvex_ir_module **out, int reverse, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_type type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 1u,
        .shape = {{YVEX_IR_NONE, 4u}}};
    yvex_ir_id tensor, state, function, block, signature[3], values[8], op;
    unsigned int ordinal, index;
    int rc = yvex_ir_module_open(out, "effects", program_source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(*out, &type, &tensor, err);
    type.kind = YVEX_IR_STATE;
    yvex_core_text_copy(type.domain, sizeof(type.domain), "fixture.recurrent");
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(*out, &type, &state, err);
    signature[0] = tensor;
    signature[1] = state;
    signature[2] = state;
    for (ordinal = 0u; rc == YVEX_OK && ordinal < 2u; ++ordinal) {
        int copy = reverse ? !ordinal : ordinal;
        yvex_ir_operation_request request;
        rc = yvex_ir_function_add(*out, copy ? "copy" : "compute", signature, copy ? 1u : 3u,
            signature, copy ? 1u : 3u, copy ? 0u : YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, &function, err);
        if (rc != YVEX_OK) break;
        block = yvex_ir_function_at(*out, function)->body;
        memcpy(values, yvex_ir_block_at(*out, block)->arguments, (copy ? 1u : 3u) * sizeof(*values));
        if (!copy) {
            for (index = 0u; index < 2u && rc == YVEX_OK; ++index) {
                request = (yvex_ir_operation_request){.operation = "state.read", .operands = &values[index + 1u],
                    .operand_count = 1u, .result_types = &tensor, .result_count = 1u};
                rc = yvex_ir_operation_add(*out, block, &request, &op, err);
                if (rc == YVEX_OK) values[index + 3u] = yvex_ir_operation_at(*out, op)->results[0];
            }
            for (index = 0u; index < 3u && rc == YVEX_OK; ++index) {
                yvex_ir_id operands[2] = {index == 0u ? values[3] : index == 1u ? values[1] : values[4],
                    index == 0u ? values[3] : index == 1u ? values[0] : values[5]};
                request = (yvex_ir_operation_request){.operation = index == 1u ? "state.update" : "tensor.add",
                    .operands = operands, .operand_count = 2u, .result_types = index == 1u ? &state : &tensor,
                    .result_count = 1u};
                rc = yvex_ir_operation_add(*out, block, &request, &op, err);
                if (rc == YVEX_OK) values[5u + index] = yvex_ir_operation_at(*out, op)->results[0];
            }
            values[0] = values[7];
            values[1] = values[6];
        }
        request = (yvex_ir_operation_request){.operation = "core.return", .operands = values,
            .operand_count = copy ? 1u : 3u};
        if (rc == YVEX_OK) rc = yvex_ir_operation_add(*out, block, &request, &op, err);
    }
    return rc == YVEX_OK ? yvex_ir_seal(*out, err) : rc;
}

static int program_test_execution(void)
{
    yvex_ir_module *module = NULL, *reordered = NULL;
    yvex_program_execution *execution = NULL, *repeat = NULL;
    yvex_core_bytes text = {.maximum = 1024u * 1024u}, again = {.maximum = 1024u * 1024u};
    yvex_core_bytes short_text = {.maximum = 4u};
    const yvex_program_entry *entry;
    const yvex_ir_id last_uses[] = {3u, 3u, 5u, 2u, 4u, 4u, 5u, 5u};
    yvex_error err;
    size_t index;
    YVEX_TEST_ASSERT(program_execution_fixture(&module, 0, &err) == YVEX_OK &&
        program_execution_fixture(&reordered, 1, &err) == YVEX_OK, "independent entry import order fixture");
    YVEX_TEST_ASSERT(yvex_program_execution_compile(&execution, module, &err) == YVEX_OK &&
        yvex_program_execution_compile(&repeat, reordered, &err) == YVEX_OK,
        "typed computational entries lower to explicit execution values and dependencies");
    YVEX_TEST_ASSERT(!strcmp(yvex_ir_identity(module), yvex_ir_identity(reordered)) &&
        !strcmp(yvex_program_execution_identity(execution), yvex_program_execution_identity(repeat)),
        "execution identity excludes construction-local operation/function/value IDs");
    entry = yvex_program_execution_entry_at(execution, 0u);
    YVEX_TEST_ASSERT(yvex_program_execution_entry_count(execution) == 2u && entry &&
        !strcmp(entry->symbol, "compute") && entry->input_count == 3u && entry->value_count == 8u &&
        entry->step_count == 6u && entry->result_count == 3u && !yvex_program_execution_entry_at(execution, 2u),
        "entry signatures and work population are explicit without decoder topology");
    YVEX_TEST_ASSERT(entry->steps[0].dependency_count == 0u &&
        entry->steps[1].dependency_count == 1u && entry->steps[1].dependencies[0] == 0u &&
        entry->steps[2].dependency_count == 1u && entry->steps[2].dependencies[0] == 0u &&
        entry->steps[3].dependency_count == 1u && entry->steps[3].dependencies[0] == 1u &&
        entry->steps[4].dependency_count == 2u && entry->steps[4].dependencies[0] == 1u &&
        entry->steps[4].dependencies[1] == 2u && entry->steps[5].dependency_count == 2u &&
        entry->steps[5].dependencies[0] == 3u && entry->steps[5].dependencies[1] == 4u,
        "data dependencies deduplicate producers; read/write order cannot be elided for in-place state lowering");
    for (index = 0u; index < entry->value_count; ++index)
        YVEX_TEST_ASSERT(entry->values[index].last_use == last_uses[index], "all eight value lifetimes match oracle");
    YVEX_TEST_ASSERT(yvex_program_execution_print(execution, &text, &err) == YVEX_OK &&
        yvex_program_execution_print(repeat, &again, &err) == YVEX_OK && text.count == again.count &&
        !memcmp(text.data, again.data, text.count), "execution dumps are canonical across import order");
    YVEX_TEST_ASSERT(yvex_program_execution_print(execution, &short_text, &err) != YVEX_OK && !short_text.count,
        "bounded inspection failure never publishes a partial result");
    yvex_ir_module_close(&module);
    yvex_ir_module_close(&reordered);
    YVEX_TEST_ASSERT(yvex_ir_verify(yvex_program_execution_module(execution), &err) == YVEX_OK,
        "lowered program retains exact semantic lineage after import owners close");
    free(short_text.data);
    free(again.data);
    free(text.data);
    yvex_program_execution_close(&repeat);
    yvex_program_execution_close(&execution);
    printf("Execution lowering: 2 entries; 6-step state/data dependency oracle and 8 value lifetimes exact; "
           "identity/dump unchanged by import order; no backend execution claim\n");
    return 0;
}

static int program_test_tensor(void)
{
    yvex_program_tensor_plan *plan = NULL, *repeat = NULL, *decoded = NULL;
    yvex_core_bytes wire = {.maximum = 65536u};
    yvex_error err;
    const yvex_program_tensor_summary *summary;
    size_t i;
    YVEX_TEST_ASSERT(test_tensor_program(&plan, 0, &err) == YVEX_OK &&
        test_tensor_program(&repeat, 0, &err) == YVEX_OK, "typed tensor program lowers without a decoder");
    summary = yvex_program_tensor_summary_get(plan);
    YVEX_TEST_ASSERT(summary->input_count == 4u && summary->result_count == 1u &&
        summary->step_count == 4u && summary->value_count == 8u &&
        summary->minimum_rows == 1u && summary->maximum_rows == 3u &&
        !strcmp(summary->identity, yvex_program_tensor_summary_get(repeat)->identity),
        "deterministic physical plan seals values, work and row envelope");
    YVEX_TEST_ASSERT(yvex_program_tensor_encode(plan, &wire, &err) == YVEX_OK &&
        yvex_program_tensor_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK &&
        !strcmp(summary->identity, yvex_program_tensor_summary_get(decoded)->identity),
        "physical program round trip preserves canonical identity");
    yvex_program_tensor_close(&decoded);
    for (i = 0u; i < wire.count; i += 7u) {
        wire.data[i] ^= 1u;
        YVEX_TEST_ASSERT(yvex_program_tensor_decode(&decoded, wire.data, wire.count, &err) != YVEX_OK &&
            !decoded, "corrupted physical program refuses before execution");
        wire.data[i] ^= 1u;
    }
    YVEX_TEST_ASSERT(yvex_program_tensor_decode(&decoded, wire.data, wire.count - 1u, &err) != YVEX_OK &&
        !decoded, "truncated physical program refuses");
    YVEX_TEST_ASSERT(test_tensor_program(&decoded, 1, &err) != YVEX_OK && !decoded,
        "duplicate external result binding requires a different legalization, not silent aliasing");
    printf("Tensor program: 4 inputs -> 4 verified steps -> 1 output; rows=1..3; "
           "deterministic binary round trip; %zu corrupted offsets refused\n", (wire.count + 6u) / 7u);
    free(wire.data);
    yvex_program_tensor_close(&repeat);
    yvex_program_tensor_close(&plan);
    return 0;
}

static int program_device_fixture(yvex_program_physical **out, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension dim = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
        .shape = {{0u, 0u}, {YVEX_IR_NONE, 32u}}};
    yvex_ir_id dimension, type, function, block, value, op;
    unsigned int i;
    int rc = yvex_ir_module_open(&m, "physical_lifetimes", program_source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &dim, &dimension, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &t, &type, err);
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", &type, 1u, &type, 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        value = yvex_ir_block_at(m, block)->arguments[0];
    }
    for (i = 0u; rc == YVEX_OK && i < 6u; ++i) {
        yvex_ir_id args[] = {value, value};
        yvex_ir_operation_request request = {.operation = "tensor.add", .operands = args, .operand_count = 2u,
            .result_types = &type, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &request, &op, err);
        if (rc == YVEX_OK) value = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request request = {.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &request, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", NULL, 0u, program_source, err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&m);
    return rc;
}

static int program_device_invoke(void *context, const yvex_program_device_invocation *r,
                                  yvex_backend_operation_facts *facts, yvex_error *err)
{
    float left[96], right[96];
    yvex_device_tensor *out = &r->values[r->step->results[0]];
    size_t i;
    int rc = yvex_backend_tensor_read(context, &r->values[r->step->operands[0]], left, out->bytes, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_read(context, &r->values[r->step->operands[1]], right, out->bytes, err);
    if (rc != YVEX_OK) return rc;
    for (i = 0u; i < out->bytes / sizeof(float); ++i) left[i] += right[i];
    memset(facts, 0, sizeof(*facts));
    return yvex_backend_tensor_write(context, out, left, out->bytes, err);
}

static int program_device_cancel(void *context)
{
    unsigned int *remaining = context;
    return (*remaining)-- == 0u;
}

static int program_test_device(void)
{
    const yvex_program_device_kernel implementations[] = {{"add.bf16.v1", program_device_invoke}};
    yvex_program_physical *p = NULL, *decoded = NULL;
    yvex_program_device *device = NULL, *refused = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CPU};
    yvex_backend_tensor_desc d = {.name = "physical-test", .dtype = YVEX_DTYPE_F32, .rank = 2u,
        .dims = {3u, 32u}, .bytes = 96u * sizeof(float)};
    yvex_device_tensor *input = NULL, *output = NULL;
    yvex_program_device_argument args = {0};
    yvex_program_device_result result;
    yvex_backend_memory_stats before, after;
    yvex_core_bytes wire = {.maximum = 65536u};
    yvex_error err = {0};
    float values[96], observed[96];
    size_t i;
    unsigned int until_cancel = 2u;
    int rc;
    YVEX_TEST_ASSERT(program_device_fixture(&p, &err) == YVEX_OK &&
        yvex_program_physical_summary_get(p)->storage_count == 2u,
        "compiler proves six SSA operations need only two non-overlapping intermediate slots");
    for (i = 0u; i < 6u; ++i)
        YVEX_TEST_ASSERT(yvex_program_physical_step_at(p, i)->attribute_count == 0u,
            "attribute-free tensor.add lowers without reading absent semantic attribute storage");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
        yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK,
        "runtime consumes reopened physical work after source/semantic/execution owners close");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK,
        "real CPU backend opens before program resources");
    rc = yvex_program_device_open(&device, decoded, backend, 3u, 0u, 0u,
        implementations, 1u, backend, &err);
    if (rc != YVEX_OK) fprintf(stderr, "physical CPU bind: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "physical instructions bind a deterministic storage/dispatch fixture");
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &input, &err) == YVEX_OK &&
        yvex_backend_tensor_alloc(backend, &d, &output, &err) == YVEX_OK, "independent input/output storage");
    for (i = 0u; i < 96u; ++i) values[i] = (float)((int)i - 48) / 64.0f;
    YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, input, values, sizeof(values), &err) == YVEX_OK,
        "deterministic exactly representable BF16 input");
    args.tensor = input;
    rc = yvex_program_device_run(device, 3u, &args, 1u, &output, 1u, NULL, NULL, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "physical CPU invocation: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.operations == 6u && output->is_written &&
        yvex_backend_tensor_read(backend, output, observed, sizeof(observed), &err) == YVEX_OK,
        "all six physical instructions execute through compiler-assigned reusable slots");
    for (i = 0u; i < 96u; ++i)
        YVEX_TEST_ASSERT(observed[i] == values[i] * 64.0f, "scalar doubling oracle agrees exactly on every result");
    YVEX_TEST_ASSERT(yvex_program_device_run(device, 3u, &args, 1u, &output, 1u,
        program_device_cancel, &until_cancel, &result, &err) == YVEX_ERR_CANCELLED &&
        result.operations == 2u && !output->is_written, "cancellation after two operations never publishes a partial result");
    until_cancel = 6u;
    YVEX_TEST_ASSERT(yvex_program_device_run(device, 3u, &args, 1u, &output, 1u,
        program_device_cancel, &until_cancel, &result, &err) == YVEX_ERR_CANCELLED &&
        result.operations == 6u && !output->is_written,
        "cancellation during the final operation prevents publication even when every value was produced");
    YVEX_TEST_ASSERT(yvex_program_device_run(device, 3u, &args, 1u, &input, 1u, NULL, NULL, &result, &err) != YVEX_OK &&
        !result.operations, "aliasing refuses before execution");
    YVEX_TEST_ASSERT(yvex_program_device_run(device, 4u, &args, 1u, &output, 1u, NULL, NULL, &result, &err) == YVEX_ERR_BOUNDS &&
        !result.operations, "oversized population refuses before dispatch");
    YVEX_TEST_ASSERT(yvex_program_device_open(&refused, decoded, backend, 3u, 1u, 0u,
        implementations, 1u, backend, &err) == YVEX_ERR_BOUNDS && !refused,
        "host budget refusal publishes no execution owner");
    YVEX_TEST_ASSERT(yvex_program_device_open(&refused, decoded, backend, 3u, 0u, 1u,
        implementations, 1u, backend, &err) == YVEX_ERR_BOUNDS && !refused,
        "device budget refusal cleans its partially prepared owner");
    YVEX_TEST_ASSERT(yvex_program_device_run(device, 3u, &args, 1u, &output, 1u, NULL, NULL, &result, &err) == YVEX_OK &&
        result.operations == 6u && output->is_written, "cancelled owner can be invoked again");
    YVEX_TEST_ASSERT(yvex_program_device_close(&device, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &input, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &output, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK && before.allocated_bytes == after.allocated_bytes &&
        yvex_backend_close_checked(&backend, &err) == YVEX_OK, "all physical allocations return to baseline");
    printf("Physical CPU storage/dispatch fixture (not CPU model kernels): six operations, two reusable slots, 96 values; max_abs=0 tolerance=0; "
           "cancel after two/final operation -> unpublished; retry -> six; alias/capacity/budget negatives; cleanup allocation delta=0\n");
    free(wire.data);
    yvex_program_physical_close(&decoded);
    yvex_program_physical_close(&p);
    return 0;
}

static int program_token_fixture(yvex_program_physical **out, int variant, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_program_parameter_binding bindings[2];
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 8u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_INDEX, .rank = 1u};
    yvex_ir_id dim, inputs[2], output, hidden, weight_type, function, block, op, value, args[2];
    unsigned int i, count = variant == 1 || variant == 2 ? 2u : 1u;
    int rc = yvex_ir_module_open(&m, "token_interface", program_source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dim, 0u};
        rc = yvex_ir_type_intern(m, &t, &inputs[0], err);
    }
    if (rc == YVEX_OK) {
        t = (yvex_ir_type){.kind = YVEX_IR_SCALAR, .scalar = YVEX_IR_INDEX};
        if (variant == 3) t = (yvex_ir_type){.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16,
            .rank = 1u, .shape = {{dim, 0u}}};
        rc = yvex_ir_type_intern(m, &t, &inputs[1], err);
    }
    if (rc == YVEX_OK) {
        t = (yvex_ir_type){.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
            .shape = {{dim, 0u}, {YVEX_IR_NONE, 4u}}};
        rc = yvex_ir_type_intern(m, &t, &hidden, err);
    }
    if (rc == YVEX_OK) {
        t.shape[1].extent = variant == 2 ? 2u : 4u;
        rc = yvex_ir_type_intern(m, &t, &output, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", inputs, 2u, &output, 1u, 0u, &function, err);
    if (rc == YVEX_OK) block = yvex_ir_function_at(m, function)->body;
    for (i = 0u; rc == YVEX_OK && i < count; ++i) {
        yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, i ? (variant == 2 ? 2u : 3u) : 5u};
        t.shape[1].extent = 4u;
        rc = yvex_ir_type_intern(m, &t, &weight_type, err);
        snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "weight_%u", i);
        yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), program_source);
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = &weight_type,
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc != YVEX_OK) break;
        args[0] = i && variant == 2 ? value : yvex_ir_block_at(m, block)->arguments[0];
        args[1] = yvex_ir_operation_at(m, op)->results[0];
        bindings[i] = (yvex_program_parameter_binding){args[1], i, YVEX_GGUF_QTYPE_BF16};
        r = (yvex_ir_operation_request){.operation = i && variant == 2 ? "nn.linear" : "nn.embedding",
            .operands = args, .operand_count = 2u, .result_types = i ? &output : &hidden, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) value = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", bindings, count, program_source, err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&m);
    return rc;
}

static int program_test_token_interface(void)
{
    int variant;
    for (variant = 0; variant < 6; ++variant) {
        yvex_program_physical *p = NULL, *decoded = NULL;
        yvex_ir_module *m = NULL;
        yvex_core_bytes wire = {.maximum = 1048576u};
        yvex_program_token_interface view = {0}, reopened = {0};
        yvex_error err = {0};
        int rc = variant < 4 ? program_token_fixture(&p, variant, &err) :
            variant == 4 ? test_sequence_program(&m, &p, &err) : test_sequence_program_kind(&m, &p, 1, &err);
        if (rc != YVEX_OK) fprintf(stderr, "token interface fixture %d: %s\n", variant, yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "token runner fixtures are verified physical programs");
        rc = yvex_program_physical_token_interface(p, &view, &err);
        if (variant == 3) {
            YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED && !view.vocabulary_size && !view.hidden_width,
                "tensor position input is valid computation but not the index-based token runner");
        } else {
            YVEX_TEST_ASSERT(rc == YVEX_OK && view.vocabulary_size == (variant >= 4 ? 64u : variant == 1 ? 3u : 5u) &&
                view.hidden_width == (variant >= 4 ? 32u : variant == 2 ? 2u : 4u),
                "executable operands determine token bounds and output geometry, including distinct embedding/output widths");
            YVEX_TEST_ASSERT(view.state_inputs == (variant == 4 ? 4u : variant == 5 ? 5u : 0u) &&
                view.recurrent_operations == (variant >= 4 ? 2u : 0u) &&
                view.attention_operations == (variant == 5 ? 1u : 0u),
                "state populations come from actual program inputs and operations, not decoder records");
            YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
                yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK &&
                yvex_program_physical_token_interface(decoded, &reopened, &err) == YVEX_OK &&
                reopened.hidden_width == view.hidden_width && reopened.vocabulary_size == view.vocabulary_size &&
                reopened.state_inputs == view.state_inputs && reopened.attention_operations == view.attention_operations &&
                reopened.recurrent_operations == view.recurrent_operations,
                "runner facts derive identically after authenticated physical import without a new persisted schema");
        }
        free(wire.data);
        yvex_program_physical_close(&decoded);
        yvex_program_physical_close(&p);
        yvex_ir_module_close(&m);
    }
    printf("Token interface: vocab=5/3/64 hidden=4/2/32 state_inputs=0/4/5; "
           "reopened facts identical; non-index position refused\n");
    return 0;
}

int yvex_test_program(void)
{
    if (program_test_token_interface() != 0) return 1;
    if (test_mhc_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    if (test_linear_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    char semantic_identity[YVEX_SHA256_HEX_BYTES] = {0}, physical_identity[YVEX_SHA256_HEX_BYTES] = {0};
    unsigned int variant;
    for (variant = 0u; variant <= 14u; ++variant) {
        yvex_ir_module *module = NULL;
        yvex_transform_ir *transform = NULL;
        yvex_physical_execution_ir *physical = NULL;
        yvex_program_parameters *binding = NULL, *repeat = NULL;
        yvex_error error;
        int valid = variant == 0u || variant == 14u, rc;
        rc = program_test_module(&module, (int)variant, &error);
        if (rc == YVEX_OK) rc = program_test_transform(&transform, (int)variant, &error);
        if (rc == YVEX_OK) rc = program_test_physical(&physical, (int)variant, &error);
        if (rc != YVEX_OK) fprintf(stderr, "program fixture %u: %s\n", variant, yvex_error_message(&error));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "fixture owners are individually sealed before cross-IR validation");
        rc = yvex_program_parameters_compile(&binding, module, transform, physical, &error);
        if (valid && rc != YVEX_OK) fprintf(stderr, "program lowering: %s\n", yvex_error_message(&error));
        YVEX_TEST_ASSERT(valid ? rc == YVEX_OK : rc != YVEX_OK && !binding,
                         "cross-IR compilation admits exact joins and refuses inconsistent/ambiguous lowering");
        if (valid) {
            YVEX_TEST_ASSERT(yvex_program_parameters_count(binding) == 2u &&
                             yvex_program_parameter_at(binding, 0u)->terminal == 0u &&
                             yvex_program_parameter_at(binding, 1u)->terminal == 0u &&
                             !yvex_program_parameter_at(binding, 2u), "two function constants share exact terminal");
            YVEX_TEST_ASSERT(yvex_program_parameters_compile(&repeat, module, transform, physical, &error) == YVEX_OK &&
                             !strcmp(yvex_program_parameters_identity(binding), yvex_program_parameters_identity(repeat)),
                             "physical lowering is deterministic");
            if (!variant) {
                yvex_core_text_copy(semantic_identity, sizeof(semantic_identity), yvex_ir_identity(module));
                yvex_core_text_copy(physical_identity, sizeof(physical_identity), yvex_program_parameters_identity(binding));
            } else {
                YVEX_TEST_ASSERT(!strcmp(semantic_identity, yvex_ir_identity(module)) &&
                                 strcmp(physical_identity, yvex_program_parameters_identity(binding)),
                                 "different admitted physical representation preserves semantic identity");
            }
            yvex_ir_module_close(&module);
            yvex_transform_ir_release(&transform);
            yvex_physical_execution_ir_close(&physical);
            YVEX_TEST_ASSERT(!strcmp(semantic_identity, yvex_ir_identity(yvex_program_parameters_module(binding))),
                             "lowered parameter owner survives closing all compiler inputs");
        }
        yvex_program_parameters_close(&repeat);
        yvex_program_parameters_close(&binding);
        yvex_physical_execution_ir_close(&physical);
        yvex_transform_ir_release(&transform);
        yvex_ir_module_close(&module);
    }
    printf("Program lowering: 2 exact physical recipes accepted, 13 inconsistent joins refused before runtime; "
           "semantic identity unchanged across BF16/F32 physical recipes\n");
    if (program_test_execution() != 0) return 1;
    if (program_test_device() != 0) return 1;
    return program_test_tensor();
}
