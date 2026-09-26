/* Parameter lowering evidence uses metadata, never model payloads or execution. */
#include "tests/test.h"
#include "tests/support/signal_program.h"
#include "tests/support/spatial_program.h"
#include "tests/support/conditioning_program.h"
#include "tests/support/joint_program.h"
#include "tests/support/tensor_program.h"
#include "tests/support/linear_program.h"
#include "tests/support/mhc_program.h"
#include "tests/support/mhc_ingress_program.h"
#include "tests/support/shared_expert_program.h"
#include "tests/support/program_sequence.h"
#include "tests/support/text_program.h"
#include "tests/support/population_program.h"
#include "tests/support/dense_program.h"

#include <yvex/internal/compilation.h>
#include <yvex/internal/core.h>
#include <yvex/internal/dense_program.h>
#include <yvex/internal/families/laya.h>
#include <yvex/internal/tensor_binding.h>
#include <yvex/internal/execution.h>
#include <yvex/internal/program.h>
#include <yvex/internal/program_kernels.h>
#include <yvex/qtype.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static const char program_source[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static const char program_other[] =
    "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";

static int program_dense_projection(void)
{
    const unsigned long long blocks[] = {1u, 2u, 36u};
    for (size_t test = 0u; test < 3u; ++test) {
        yvex_dense_program_recipe r = {.semantic_identity = program_source, .rows = 6u, .output_rows = 1u,
            .width = 2048u, .heads = 32u, .head_dimension = 64u, .rotary_dimension = 48u,
            .ffn_width = 8192u, .block_count = blocks[test], .output_width = 3072u, .epsilon = (double)1.0e-5f};
        yvex_program_physical *p = NULL, *decoded = NULL;
        yvex_core_bytes wire = {.maximum = 16u * 1024u * 1024u};
        yvex_error err;
        YVEX_TEST_ASSERT(yvex_dense_program_compile(&p, &r, &err) == YVEX_OK &&
            yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
            yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK,
            "dense component source projection and authenticated physical import");
        const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p);
        size_t parameters = 0u, attention = 0u, norms = 0u, partitions = 0u, slices = 0u;
        for (size_t i = 0u; i < s->step_count; ++i) {
            const yvex_program_physical_step *op = yvex_program_physical_step_at(p, i);
            parameters += !strcmp(op->implementation, "parameter.encoded.v1");
            attention += !strcmp(op->implementation, "attention.full.f32.v1");
            norms += !strcmp(op->implementation, "rms_normalize.f32.v1");
            partitions += !strcmp(op->implementation, "split_interleaved_three.f32.v1");
            slices += !strcmp(op->implementation, "slice_rows.f32.v1");
        }
        YVEX_TEST_ASSERT(s->input_count == 3u && s->result_count == 1u && s->maximum_rows == 6u &&
            parameters == blocks[test] * 12u + 4u && attention == blocks[test] && norms == blocks[test] * 2u &&
            partitions == blocks[test] && slices == 1u &&
            !strcmp(s->identity, yvex_program_physical_summary_get(decoded)->identity),
            "every block operation and exact source parameter has one canonical executable owner");
        printf("Dense component compiler: blocks=%llu parameters=%zu attention=%zu unit_norm=%zu steps=%zu; "
            "three typed inputs, one prefix result; physical reimport identity exact\n",
            blocks[test], parameters, attention, norms, s->step_count);
        free(wire.data);
        yvex_program_physical_close(&p);
        yvex_program_physical_close(&decoded);
        for (unsigned int bad = 0u; bad < 5u; ++bad) {
            yvex_dense_program_recipe invalid = r;
            if (bad == 0u) invalid.output_rows = invalid.rows + 1u;
            if (bad == 1u) invalid.heads++;
            if (bad == 2u) invalid.rotary_dimension++;
            if (bad == 3u) invalid.block_count = ULLONG_MAX;
            if (bad == 4u) invalid.epsilon = 0.0;
            YVEX_TEST_ASSERT(yvex_dense_program_compile(&p, &invalid, &err) == YVEX_ERR_FORMAT && !p,
                "dense population/head/rotary/storage/numerical source negatives refuse before lowering");
        }
    }
    return 0;
}

static int program_laya_bidirectional_recipe(void)
{
    yvex_laya_program_recipe recipe = {.source_identity = program_source,
        .maximum_tokens = 4u, .layer_count = 1u, .vocabulary_size = 16u,
        .hidden_width = 8u, .attention_heads = 2u, .intermediate_width = 12u,
        .head_layer_count = 1u, .epsilon = 1e-5};
    yvex_laya_program *first = NULL, *repeat = NULL;
    yvex_error err = {0};
    int rc = yvex_laya_program_compile(&first, &recipe, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Laya recipe: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_laya_program_parameter_count(first) == 27u,
        "source-owned encoder and typed head compile without a generation plan");
    const yvex_program_physical_summary *summary =
        yvex_program_physical_summary_get(yvex_laya_program_physical(first));
    size_t attention = 0u, centered = 0u, causal = 0u;
    for (size_t i = 0u; i < summary->step_count; ++i) {
        const yvex_program_physical_step *step =
            yvex_program_physical_step_at(yvex_laya_program_physical(first), i);
        attention += !strcmp(step->implementation, "attention.full.f32.v1");
        centered += !strcmp(step->implementation, "layer_norm_unbiased.f32.v1");
        if (!strcmp(step->implementation, "attention.full.f32.v1"))
            causal += yvex_program_physical_attribute(step, "causal")->value.integer != 0u;
    }
    YVEX_TEST_ASSERT(summary->input_count == 6u && summary->result_count == 1u &&
        summary->maximum_rows == 4u && attention == 2u && centered == 3u && !causal,
        "encoder and head use two full noncausal attentions and centered bias-free norms");
    char name[256];
    YVEX_TEST_ASSERT(yvex_laya_program_parameter_name(first, 0u, name, &err) == YVEX_OK &&
        !strcmp(name, "encoder.embeddings.tok_embeddings.weight") &&
        yvex_laya_program_parameter_name(first, 27u, name, &err) != YVEX_OK,
        "compiled source parameter namespace is exact and bounded");
    YVEX_TEST_ASSERT(yvex_laya_program_compile(&repeat, &recipe, &err) == YVEX_OK &&
        !strcmp(summary->identity,
            yvex_program_physical_summary_get(yvex_laya_program_physical(repeat))->identity),
        "non-generative source recipe recompiles to the same physical identity");
    char root[] = "/tmp/yvex-tensor-binding-unit-XXXXXX";
    char path[256];
    YVEX_TEST_ASSERT(mkdtemp(root) != NULL &&
        snprintf(path, sizeof(path), "%s/model.binding", root) > 0,
        "bounded tensor binding publication directory");
    yvex_tensor_binding_build_request binding = {
        .schema_version = YVEX_TENSOR_BINDING_SCHEMA_V1,
        .source_identity = program_source, .tokenizer_identity = program_other,
        .logical_model_identity = program_source,
        .source_bytes = 1024u, .source_tensor_count = 27u,
        .input_format = YVEX_TENSOR_INPUT_TOKEN_TYPE_DUAL_ROPE_V1,
        .rotary_width = 4u, .primary_theta = 160000u, .secondary_theta = 10000u,
        .token_domain_size = 16u, .type_domain_size = 3u, .marker_token_id = 4u,
        .program = yvex_laya_program_physical(first),
        .parameter_count = yvex_laya_program_parameter_count(first),
        .parameter_name = yvex_laya_program_parameter_name, .parameter_context = first};
    yvex_tensor_binding_summary published = {0};
    yvex_tensor_binding *reopened = NULL;
    YVEX_TEST_ASSERT(yvex_tensor_binding_publish(path, &binding, &published, &err) == YVEX_OK &&
        yvex_tensor_binding_open(&reopened, path, &err) == YVEX_OK &&
        !strcmp(published.identity, yvex_tensor_binding_summary_get(reopened)->identity) &&
        !strcmp(summary->identity,
            yvex_tensor_binding_summary_get(reopened)->physical_program_identity),
        "compiled physical program and parameter directory reopen under one sealed binding");
    yvex_tensor_binding_close(&reopened);
    YVEX_TEST_ASSERT(yvex_tensor_binding_publish(path, &binding, &published, &err) != YVEX_OK,
        "immutable binding publication refuses overwrite");
    int fd = open(path, O_WRONLY);
    unsigned char altered = 0u;
    YVEX_TEST_ASSERT(fd >= 0 && pwrite(fd, &altered, 1u, 8) == 1 && close(fd) == 0 &&
        yvex_tensor_binding_open(&reopened, path, &err) == YVEX_ERR_FORMAT && !reopened,
        "corrupted binding bytes refuse before program or parameter publication");
    YVEX_TEST_ASSERT(unlink(path) == 0 && rmdir(root) == 0,
        "binding test publication cleans exact temporary path");
    yvex_laya_program_close(&repeat);
    yvex_laya_program_close(&first);
    recipe.maximum_tokens = 64u;
    recipe.layer_count = 28u;
    recipe.head_layer_count = 2u;
    YVEX_TEST_ASSERT(yvex_laya_program_compile(&first, &recipe, &err) == YVEX_OK &&
        yvex_laya_program_parameter_count(first) == 201u,
        "full 28-layer source and two-layer decision head retain 201 exact used parameter roles");
    yvex_laya_program_close(&first);
    recipe.maximum_tokens = 129u;
    YVEX_TEST_ASSERT(yvex_laya_program_compile(&first, &recipe, &err) == YVEX_ERR_FORMAT && !first,
        "unqualified sliding-window population refuses rather than silently becoming full attention");
    return 0;
}

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

static int program_device_fixture(yvex_program_physical **out, unsigned int variant, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension dim = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
        .shape = {{0u, 0u}, {YVEX_IR_NONE, 32u}}};
    yvex_ir_id dimension, type, function, block, value, op;
    unsigned int i;
    if (variant == 1u) t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
    if (variant == 2u) { dim.minimum = 2u; dim.maximum = 6u; dim.multiple = 2u; }
    if (variant == 3u) t.shape[1].extent = ULLONG_MAX / 4u;
    if (variant == 4u) { dim.minimum = 2u; dim.maximum = 5u; dim.multiple = 2u; }
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

static int program_test_f32_encoder_lowering(void)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *module = NULL;
    yvex_program_execution *execution = NULL;
    yvex_program_physical *physical = NULL, *reopened = NULL;
    yvex_core_bytes wire = {.maximum = 65536u};
    yvex_ir_type input = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
        .shape = {{YVEX_IR_NONE, 1u}, {YVEX_IR_NONE, 6u}}};
    yvex_ir_type half = input;
    yvex_ir_id input_type, half_type, function, block, op, values[2], result;
    yvex_error err = {0};
    half.shape[1].extent = 3u;
    int rc = yvex_ir_module_open(&module, "encoder_f32", program_source, dialects, 2u, &err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(module, &input, &input_type, &err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(module, &half, &half_type, &err);
    if (rc == YVEX_OK) rc = yvex_ir_function_add(module, "forward", &input_type, 1u,
        &half_type, 1u, 0u, &function, &err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(module, function)->body;
        yvex_ir_id source = yvex_ir_block_at(module, block)->arguments[0];
        yvex_ir_id result_types[] = {half_type, half_type};
        yvex_ir_operation_request request = {.operation = "tensor.split_two", .operands = &source,
            .operand_count = 1u, .result_types = result_types, .result_count = 2u};
        rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        if (rc == YVEX_OK) {
            values[0] = yvex_ir_operation_at(module, op)->results[0];
            values[1] = yvex_ir_operation_at(module, op)->results[1];
            request = (yvex_ir_operation_request){.operation = "tensor.multiply", .operands = values,
                .operand_count = 2u, .result_types = &half_type, .result_count = 1u};
            rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        }
        if (rc == YVEX_OK) {
            source = yvex_ir_operation_at(module, op)->results[0];
            request = (yvex_ir_operation_request){.operation = "nn.gelu", .operands = &source,
                .operand_count = 1u, .result_types = &half_type, .result_count = 1u};
            rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        }
        if (rc == YVEX_OK) {
            result = yvex_ir_operation_at(module, op)->results[0];
            request = (yvex_ir_operation_request){.operation = "core.return", .operands = &result,
                .operand_count = 1u};
            rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(module, &err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, module, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&physical, execution,
        "forward", NULL, 0u, program_source, &err);
    if (rc != YVEX_OK) fprintf(stderr, "F32 encoder lowering: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "F32 encoder operations lower through generic program owner");
    const yvex_program_physical_summary *summary = yvex_program_physical_summary_get(physical);
    YVEX_TEST_ASSERT(summary && summary->step_count == 3u &&
        !strcmp(yvex_program_physical_step_at(physical, 0u)->implementation, "split_two.f32.v1") &&
        !strcmp(yvex_program_physical_step_at(physical, 1u)->implementation, "multiply.f32.v1") &&
        !strcmp(yvex_program_physical_step_at(physical, 2u)->implementation, "gelu.erf.f32.v1") &&
        yvex_program_physical_encode(physical, &wire, &err) == YVEX_OK &&
        yvex_program_physical_decode(&reopened, wire.data, wire.count, &err) == YVEX_OK &&
        !strcmp(summary->identity, yvex_program_physical_summary_get(reopened)->identity),
        "F32 split, multiply and GELU retain one deterministic physical identity");
    free(wire.data);
    yvex_program_physical_close(&reopened);
    yvex_program_physical_close(&physical);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&module);
    return 0;
}

static int program_test_unbiased_layer_norm_lowering(void)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *module = NULL;
    yvex_program_execution *execution = NULL;
    yvex_program_physical *physical = NULL;
    yvex_ir_type input = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
        .shape = {{YVEX_IR_NONE, 1u}, {YVEX_IR_NONE, 2u}}};
    yvex_ir_type weight = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 1u,
        .shape = {{YVEX_IR_NONE, 2u}}};
    yvex_ir_id input_type, weight_type, function, block, op, gamma = YVEX_IR_NONE, result;
    yvex_error err = {0};
    int rc = yvex_ir_module_open(&module, "unbiased_norm", program_source, dialects, 2u, &err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(module, &input, &input_type, &err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(module, &weight, &weight_type, &err);
    if (rc == YVEX_OK) rc = yvex_ir_function_add(module, "forward", &input_type, 1u,
        &input_type, 1u, 0u, &function, &err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(module, function)->body;
        yvex_ir_attribute attrs[] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
            {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL}};
        yvex_core_text_copy(attrs[0].value.text, sizeof(attrs[0].value.text), program_source);
        yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), "gamma");
        yvex_ir_operation_request request = {.operation = "core.parameter", .result_types = &weight_type,
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        if (rc == YVEX_OK) gamma = yvex_ir_operation_at(module, op)->results[0];
        if (rc == YVEX_OK) {
            yvex_ir_id args[] = {yvex_ir_block_at(module, block)->arguments[0], gamma};
            yvex_ir_attribute norm[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64,
                .value.real = 1e-5}, {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64}};
            request = (yvex_ir_operation_request){.operation = "nn.layer_norm_unbiased",
                .operands = args, .operand_count = 2u, .result_types = &input_type, .result_count = 1u,
                .attributes = norm, .attribute_count = 2u};
            rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        }
        if (rc == YVEX_OK) {
            result = yvex_ir_operation_at(module, op)->results[0];
            request = (yvex_ir_operation_request){.operation = "core.return", .operands = &result,
                .operand_count = 1u};
            rc = yvex_ir_operation_add(module, block, &request, &op, &err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(module, &err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, module, &err);
    yvex_program_parameter_binding binding = {.semantic_value = gamma, .tensor_id = 0u,
        .qtype = YVEX_GGUF_QTYPE_F32};
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&physical, execution,
        "forward", &binding, 1u, program_source, &err);
    if (rc != YVEX_OK) fprintf(stderr, "unbiased norm lowering: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
        !strcmp(yvex_program_physical_step_at(physical, 1u)->implementation,
            "layer_norm_unbiased.f32.v1"),
        "bias-free centered LayerNorm is a distinct physical operation from RMSNorm");
    yvex_program_physical_close(&physical);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&module);
    return 0;
}

static int program_test_value_layout(void)
{
    yvex_error err = {0};
    unsigned int accepted = 0u, refused = 0u;
    for (unsigned int variant = 0u; variant < 3u; ++variant) {
        yvex_program_physical *p = NULL, *decoded = NULL;
        yvex_core_bytes wire = {.maximum = 65536u};
        YVEX_TEST_ASSERT(program_device_fixture(&p, variant, &err) == YVEX_OK &&
            yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
            yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK,
            "physical layouts reopen from the existing authenticated schema");
        for (unsigned long long rows = 0u; rows < 8u; ++rows) {
            yvex_program_value_layout a, b;
            int admitted = variant == 0u ? rows >= 1u && rows <= 3u :
                variant == 1u ? rows == 1u : rows >= 2u && rows <= 6u && rows % 2u == 0u;
            for (size_t value = 0u; value < 7u; ++value) {
                memset(&a, 0xff, sizeof(a)); memset(&b, 0xff, sizeof(b));
                int rc = yvex_program_physical_value_layout(p, value, rows, &a, &err);
                int reopened = yvex_program_physical_value_layout(decoded, value, rows, &b, &err);
                if (admitted) {
                    unsigned long long expected_rows = variant == 1u ? 3u : rows;
                    YVEX_TEST_ASSERT(rc == YVEX_OK && reopened == YVEX_OK && a.rank == 2u &&
                        a.dims[0] == expected_rows && a.dims[1] == 32u && a.elements == expected_rows * 32u &&
                        a.bytes == expected_rows * 128u && a.storage_scalar == YVEX_IR_F32 &&
                        a.dynamic_rows == (variant != 1u) && a.bytes == b.bytes &&
                        a.elements == b.elements && a.rank == b.rank && a.storage_scalar == b.storage_scalar &&
                        a.dynamic_rows == b.dynamic_rows && !memcmp(a.dims, b.dims, sizeof(a.dims)),
                        "compiler resolves exact fixed/dynamic BF16-in-F32 layout, preserved after import");
                    accepted++;
                } else {
                    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS && reopened == rc && !a.elements && !a.bytes &&
                        !a.rank && !b.elements && !b.bytes, "invalid population publishes no partial layout");
                    refused++;
                }
            }
        }
        yvex_program_value_layout layout;
        YVEX_TEST_ASSERT(yvex_program_physical_value_layout(p, SIZE_MAX, 1u, &layout, &err) ==
            YVEX_ERR_INVALID_ARG && !layout.bytes &&
            yvex_program_physical_value_layout(p, 0u, 1u, NULL, &err) == YVEX_ERR_INVALID_ARG &&
            yvex_program_physical_value_layout(NULL, 0u, 1u, &layout, &err) == YVEX_ERR_INVALID_ARG,
            "missing program, value or destination refuses before inspection");
        free(wire.data); yvex_program_physical_close(&decoded); yvex_program_physical_close(&p);
    }
    printf("Physical value layout: %u exact views, %u population refusals; "
        "fixed/dynamic/multiple geometry; BF16 in F32; authenticated round trip\n", accepted, refused);
    yvex_program_physical *overflow = NULL;
    YVEX_TEST_ASSERT(program_device_fixture(&overflow, 3u, &err) != YVEX_OK && !overflow,
        "overflowing maximum activation storage refuses during compilation, before a runtime allocation");
    YVEX_TEST_ASSERT(program_device_fixture(&overflow, 4u, &err) != YVEX_OK && !overflow,
        "dimension endpoints must respect the declared multiple under the existing IR contract");
    return 0;
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
    YVEX_TEST_ASSERT(program_device_fixture(&p, 0u, &err) == YVEX_OK &&
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
        const yvex_program_physical_summary *summary = yvex_program_physical_summary_get(p);
        for (size_t i = 0u; i < summary->value_count; ++i) {
            const yvex_program_physical_value *v = yvex_program_physical_value_at(p, i);
            yvex_program_value_layout layout;
            int layout_rc = yvex_program_physical_value_layout(p, i, summary->minimum_rows, &layout, &err);
            if (v->parameter || v->type.kind != YVEX_IR_TENSOR)
                YVEX_TEST_ASSERT(layout_rc == YVEX_ERR_UNSUPPORTED && !layout.bytes,
                    "parameters, state lifetimes and scalar controls never become activation allocations");
            else if (v->type.scalar == YVEX_IR_INDEX) {
                yvex_backend_tensor_desc desc;
                YVEX_TEST_ASSERT(layout_rc == YVEX_OK && layout.rank == 1u &&
                    layout.storage_scalar == YVEX_IR_INDEX && layout.bytes == layout.elements * 4u &&
                    yvex_program_device_descriptor(p, i, summary->minimum_rows, &desc, &err) ==
                        YVEX_ERR_UNSUPPORTED && !desc.bytes,
                    "host U32 indices have exact compiler geometry but are not F32 device activations");
            } else YVEX_TEST_ASSERT(layout_rc == YVEX_OK, "token program activation has a physical carrier");
        }
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

static int program_text_compile(void)
{
    const unsigned long long layers[] = {0u, 1u, 2u, 50u};
    yvex_error err;
    for (size_t run = 0u; run < 4u; ++run) {
        yvex_program_physical *p = NULL, *copy = NULL;
        yvex_core_bytes bytes = {.maximum = 16u * 1024u * 1024u};
        int rc = yvex_text_program_compile(&p, &test_text_recipe, layers[run], 5u, NULL, 0u, &err);
        if (rc != YVEX_OK) fprintf(stderr, "text compile: %s\n", yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK && p, "source text component lowers without backend or payload");
        const yvex_program_physical_summary *s = yvex_program_physical_summary_get(p);
        size_t norms = 0u, attention = 0u, tables = 0u, parameters = 0u;
        for (size_t i = 0u; i < s->step_count; ++i) {
            const char *op = yvex_program_physical_step_at(p, i)->implementation;
            norms += !strcmp(op, "group_rms_norm.bf16.vector4.v1");
            attention += !strcmp(op, "attention.full.f32acc.bf16.v1");
            tables += !strcmp(op, "rotary_tables.f64.bf16.v1");
            parameters += !strcmp(op, "parameter.encoded.v1");
        }
        YVEX_TEST_ASSERT(s->input_count == 4u && s->result_count == 1u && s->maximum_rows == 5u &&
            norms == 4u * layers[run] && attention == layers[run] && tables == (layers[run] != 0u) &&
            parameters == 1u + layers[run] * 11u,
            "explicit normalization, attention, positions and parameter dependencies preserve source composition");
        YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &bytes, &err) == YVEX_OK &&
            yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
            !strcmp(s->identity, yvex_program_physical_summary_get(copy)->identity),
            "complete component program round-trips with deterministic physical identity");
        printf("Text program: layers=%llu parameters=%zu norms=%zu attention=%zu tables=%zu steps=%zu slots=%zu; "
            "binary identity exact\n", layers[run], parameters, norms, attention, tables, s->step_count, s->storage_count);
        yvex_program_physical *retained = yvex_program_physical_retain(p, &err);
        YVEX_TEST_ASSERT(retained == p, "immutable executable truth is shared without copying or re-lowering");
        yvex_program_physical_close(&p);
        YVEX_TEST_ASSERT(!p && !strcmp(yvex_program_physical_summary_get(retained)->identity,
            yvex_program_physical_summary_get(copy)->identity), "retained program survives compiler owner release");
        yvex_program_physical_close(&retained);
        yvex_program_physical_close(&copy); free(bytes.data);
    }
    for (size_t variant = 0u; variant < 6u; ++variant) {
        yvex_component_text_recipe r = test_text_recipe;
        yvex_program_physical *p = NULL;
        if (variant == 0u) r.normalization_epsilon = 0.0f;
        if (variant == 1u) r.query_heads = 3u;
        if (variant == 2u) r.head_dimension = 6u;
        if (variant == 3u) r.hidden_width = 31u;
        if (variant == 4u) r.rope_theta = 0u;
        if (variant == 5u) r.schema_version = 99u;
        YVEX_TEST_ASSERT(yvex_text_program_compile(&p, &r, 2u, 5u, NULL, 0u, &err) != YVEX_OK && !p,
            "invalid geometry, numerical policy and import schema fail before execution");
    }
    return 0;
}

static int program_parameter_views(void)
{
    for (unsigned int test = 0u; test < 10u; ++test) {
        yvex_program_physical_value v = {.parameter = 1, .qtype = YVEX_GGUF_QTYPE_F32,
            .type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 5u,
                .shape = {{YVEX_IR_NONE, 8u}, {YVEX_IR_NONE, 2u}, {YVEX_IR_NONE, 3u},
                          {YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 5u}}}};
        unsigned long long dims[] = {8u, 2u, 3u, 15u}, rows = 99u, width = 99u, bytes = 2880u;
        unsigned int rank = 4u, qtype = YVEX_GGUF_QTYPE_F32;
        if (test == 1u) { dims[0] = 2u; dims[1] = 8u; }
        if (test == 2u) { dims[2] = 5u; dims[3] = 9u; }
        if (test == 3u) bytes--;
        if (test == 4u) rank = 3u;
        if (test == 5u) v.type.shape[4].symbol = 0u;
        if (test == 6u) v.type.shape[4].extent = ULLONG_MAX;
        if (test == 7u) qtype = YVEX_GGUF_QTYPE_Q4_0;
        if (test == 8u) v.parameter = 0;
        yvex_error err;
        int rc = test == 9u ? yvex_program_physical_parameter_view(&v, rank, dims, qtype, bytes,
            &rows, &width, &err) : yvex_program_physical_source_parameter_view(&v, rank, dims, qtype, bytes,
            &rows, &width, &err);
        YVEX_TEST_ASSERT(test ? rc == YVEX_ERR_FORMAT && rows == 0u && width == 0u :
            rc == YVEX_OK && rows == 144u && width == 5u,
            "explicit scalar source-tail import preserves leading axes; ordinary view never guesses folding");
    }
    printf("Source-tail parameter views: exact 8x2x3x3x5 -> 8x2x3x15; 9 malformed/profile negatives refused\n");
    for (unsigned int test = 0u; test < 13u; ++test) {
        yvex_program_physical_value v = {.parameter = 1, .qtype = YVEX_GGUF_QTYPE_F32,
            .type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
                .shape = {{YVEX_IR_NONE, 24u}, {YVEX_IR_NONE, 16u}}}};
        unsigned long long dims[] = {24u, 16u, 1u, 1u}, bytes = 1536u, rows = 99u, width = 99u;
        unsigned int rank = 4u, qtype = YVEX_GGUF_QTYPE_F32;
        if (test == 1u) rank = 2u;
        if (test == 2u) { dims[0] = 1u; dims[1] = 24u; dims[2] = 16u; }
        if (test == 3u) { dims[0] = 16u; dims[1] = 24u; }
        if (test == 4u) { dims[0] = 12u; dims[1] = 32u; }
        if (test == 5u) bytes--;
        if (test == 6u) qtype = YVEX_GGUF_QTYPE_BF16;
        if (test == 7u) v.parameter = 0;
        if (test == 8u) rank = 0u;
        if (test == 9u) v.type.rank = YVEX_IR_RANK_CAP + 1u;
        if (test == 10u) v.type.shape[0] = (yvex_ir_extent){0u, 0u};
        if (test == 11u) dims[2] = ULLONG_MAX;
        if (test == 12u) dims[1] = 0u;
        yvex_error err;
        int rc = yvex_program_physical_parameter_view(&v, rank, dims, qtype, bytes, &rows, &width, &err);
        YVEX_TEST_ASSERT(rc == (test < 3u ? YVEX_OK : YVEX_ERR_FORMAT), "only exact singleton-axis views admitted");
        YVEX_TEST_ASSERT(test < 3u ? rows == 24u && width == 16u : rows == 0u && width == 0u,
            "no transposition/factorization or partial view escapes failed cold binding");
    }
    printf("Physical parameter views: 3 exact singleton normalizations; 10 transpose/factorization/shape/storage "
        "negatives refused before payload access\n");
    return 0;
}

static int program_target_state_cleanup(void)
{
    yvex_ir_module *module = NULL;
    yvex_program_physical *source = NULL;
    yvex_sequence_state_plan before = {0};
    yvex_error err = {0};
    YVEX_TEST_ASSERT(test_sequence_program(&module, &source, &err) == YVEX_OK &&
        yvex_program_physical_sequence_state(source, &before),
        "stateful target source has compiled provider bindings");
    const yvex_program_physical_summary *summary = yvex_program_physical_summary_get(source);
    yvex_program_target_choice choice = {SIZE_MAX, "gated_delta.bf16.f32state.v1"};
    for (size_t i = 0u; i < summary->step_count; ++i)
        if (!strcmp(yvex_program_physical_step_at(source, i)->implementation, choice.implementation)) choice.step = i;
    YVEX_TEST_ASSERT(choice.step != SIZE_MAX, "fixture contains actual recurrent target work");
    for (size_t i = 0u; i < 8u; ++i) {
        yvex_program_physical *target = NULL;
        yvex_sequence_state_plan after = {0};
        YVEX_TEST_ASSERT(yvex_program_physical_target_compile(&target, source, &choice, 1u, &err) == YVEX_OK &&
            yvex_program_physical_sequence_state(target, &after) &&
            !strcmp(summary->identity, yvex_program_physical_summary_get(target)->identity),
            "repeated equivalent specialization preserves physical identity and reconstructs state once");
        YVEX_TEST_ASSERT(before.binding_count == after.binding_count &&
            !memcmp(before.bindings, after.bindings, before.binding_count * sizeof(*before.bindings)),
            "state roots, geometry and identities remain exact without duplicate bindings");
        yvex_program_physical_close(&target);
    }
    yvex_program_physical_close(&source);
    yvex_ir_module_close(&module);
    printf("Stateful target cleanup: 8 specializations; provider geometry/identity exact; "
        "no duplicated state bindings; allocation cleanup checked by sanitizer\n");
    return 0;
}

int yvex_test_program(void)
{
    if (test_shared_expert(YVEX_BACKEND_KIND_CPU)) return 1;
    if (test_shared_target(YVEX_BACKEND_KIND_CPU)) return 1;
    if (program_test_value_layout()) return 1;
    if (program_test_f32_encoder_lowering()) return 1;
    if (program_test_unbiased_layer_norm_lowering()) return 1;
    if (test_spatial_programs(YVEX_BACKEND_KIND_CPU)) return 1;
    if (program_target_state_cleanup()) return 1;
    if (test_conditioning_program(YVEX_BACKEND_KIND_CPU)) return 1;
    if (test_joint_compiler()) return 1;
    if (test_signal_programs(YVEX_BACKEND_KIND_CPU)) return 1;
    if (program_parameter_views()) return 1;
    if (test_dense_program(YVEX_BACKEND_KIND_CPU)) return 1;
    if (program_dense_projection() != 0) return 1;
    if (program_laya_bidirectional_recipe() != 0) return 1;
    if (test_program_populations(YVEX_BACKEND_KIND_CPU, 0)) return 1;
    if (test_program_index_values(YVEX_BACKEND_KIND_CPU)) return 1;
    if (program_text_compile() != 0) return 1;
    if (program_test_token_interface() != 0) return 1;
    if (test_mhc_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    if (test_ingress_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    if (test_post_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    if (test_stream_mean_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    if (test_linear_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
    if (test_linear_residual_execute(YVEX_BACKEND_KIND_CPU) != 0) return 1;
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
