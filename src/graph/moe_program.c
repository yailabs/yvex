/* Cold source/import normalization of mHC feed-forward ingress into typed
 * computational dependencies. Physical parameter policy remains independent. */
#include <yvex/internal/moe.h>
#include <yvex/internal/core.h>

static int moe_program_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.moe.ingress", reason);
    return status;
}

int yvex_moe_shared_program_import(yvex_program_physical **out, const yvex_moe_layer_plan *l,
    const char *source_identity, const char *physical_identity, unsigned long long maximum_rows, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = maximum_rows, .multiple = 1u};
    yvex_ir_id dim, types[6], fn, block = YVEX_IR_NONE, op, input = YVEX_IR_NONE, values[7] = {0}, weights[3];
    yvex_program_parameter_binding bindings[3];
    const char *symbols[] = {"gate", "up", "down"};
    if (out) *out = NULL;
    if (!out || !l || !maximum_rows || !l->hidden_width || !l->shared_intermediate_width || l->shared_experts != 1u)
        return moe_program_refuse(err, YVEX_ERR_FORMAT, "shared expert source geometry is inconsistent");
    int rc = yvex_ir_module_open(&m, "shared_expert_import", source_identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    for (unsigned int i = 0u; rc == YVEX_OK && i < 6u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .rank = 2u,
            .scalar = i == 0u || i == 4u ? YVEX_IR_BF16 : YVEX_IR_F32};
        t.shape[0] = (yvex_ir_extent){i == 1u || i == 2u ? YVEX_IR_NONE : dim,
            i == 1u ? l->shared_intermediate_width : i == 2u ? l->hidden_width : 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE,
            i == 2u || i == 3u || i == 4u ? l->shared_intermediate_width : l->hidden_width};
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "shared", types, 1u, types, 1u, 0u, &fn, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, fn)->body;
        input = yvex_ir_block_at(m, block)->arguments[0];
    }
    for (unsigned int i = 0u; rc == YVEX_OK && i < 3u; ++i) {
        yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        yvex_core_text_copy(attrs[0].value.text, sizeof(attrs[0].value.text), symbols[i]);
        yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), source_identity);
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = types + (i == 2u ? 2u : 1u),
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) {
            weights[i] = yvex_ir_operation_at(m, op)->results[0];
            bindings[i] = (yvex_program_parameter_binding){weights[i],
                l->tensor_ids[YVEX_MOE_WEIGHT_SHARED_GATE + i], l->qtypes[YVEX_MOE_WEIGHT_SHARED_GATE + i]};
        }
    }
    for (unsigned int i = 0u; rc == YVEX_OK && i < 7u; ++i) {
        yvex_ir_id operands[] = {i == 0u ? input : i < 3u ? values[0] : i == 3u ? values[1] : values[i - 1u],
            i == 1u ? weights[0] : i == 2u ? weights[1] : i == 3u ? values[2] : weights[2]};
        yvex_ir_attribute limit = {.name = "limit", .kind = YVEX_IR_ATTR_F64, .value.real = l->activation_limit};
        yvex_ir_attribute reduction = {.name = "reduction", .kind = YVEX_IR_ATTR_U64,
            .value.integer = YVEX_IR_REDUCTION_ROW_DOT};
        int cast = i == 0u || i == 4u || i == 6u;
        unsigned int result = i == 0u || i == 5u ? 5u : i == 3u ? 4u : i == 6u ? 0u : 3u;
        yvex_ir_operation_request r = {.operation = i == 3u ? "nn.clamped_swiglu" :
            cast ? "tensor.cast" : "nn.linear", .operands = operands, .operand_count = cast ? 1u : 2u,
            .result_types = types + result, .result_count = 1u,
            .attributes = i == 3u ? &limit : cast ? NULL : &reduction, .attribute_count = cast ? 0u : 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) values[i] = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = values + 6u, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK)
        rc = yvex_program_physical_compile(out, execution, "shared", bindings, 3u, physical_identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

int yvex_moe_ingress_program_import(yvex_program_physical **out, const yvex_moe_layer_plan *l,
    const char *source_identity, const char *physical_identity, unsigned long long maximum_rows, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = maximum_rows, .multiple = 1u};
    yvex_ir_id dim, types[14], fn, block = YVEX_IR_NONE, op, values[14] = {0}, results[4];
    yvex_program_parameter_binding bindings[5];
    const unsigned int parameter_types[] = {2u, 5u, 6u, 11u, 12u};
    const yvex_moe_weight_slot slots[] = {YVEX_MOE_WEIGHT_MHC_FUNCTION, YVEX_MOE_WEIGHT_MHC_SCALE,
        YVEX_MOE_WEIGHT_MHC_BASE, YVEX_MOE_WEIGHT_FFN_NORM, YVEX_MOE_WEIGHT_ROUTER};
    const char *symbols[] = {"ingress_function", "ingress_scale", "ingress_base", "ingress_norm", "router_projection"};
    unsigned long long expanded, mixing;
    if (out) *out = NULL;
    if (!out || !l || !maximum_rows || !l->routed_experts ||
        !yvex_core_u64_mul(l->hidden_width, l->residual_streams, &expanded) ||
        expanded != l->expanded_width || !yvex_core_u64_add(l->residual_streams, 2u, &mixing) ||
        !yvex_core_u64_mul(mixing, l->residual_streams, &mixing) || mixing != l->mhc_mixing_rows)
        return moe_program_refuse(err, YVEX_ERR_FORMAT, "ingress import geometry is inconsistent");
    int rc = yvex_ir_module_open(&m, "moe_ingress_import", source_identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    for (unsigned int i = 0u; rc == YVEX_OK && i < 14u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR,
            .scalar = i == 4u || i == 7u ? YVEX_IR_BF16 : YVEX_IR_F32,
            .rank = i == 0u || i == 4u || i == 9u ? 3u : i == 5u || i == 6u || i == 11u ? 1u : 2u};
        t.shape[0] = i == 2u || i == 12u || t.rank == 1u ? (yvex_ir_extent){YVEX_IR_NONE,
            i == 12u ? l->routed_experts : i == 5u ? 3u : i == 11u ? l->hidden_width : mixing} :
            (yvex_ir_extent){dim, 0u};
        if (t.rank > 1u) t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE,
            i == 1u || i == 2u ? expanded : i == 3u ? mixing :
            i == 7u || i == 10u || i == 12u ? l->hidden_width :
            i == 13u ? l->routed_experts : l->residual_streams};
        if (t.rank == 3u) t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE,
            i == 9u ? l->residual_streams : l->hidden_width};
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
    }
    if (rc == YVEX_OK) {
        results[0] = types[7]; results[1] = types[8]; results[2] = types[9]; results[3] = types[13];
        rc = yvex_ir_function_add(m, "ingress", types, 1u, results, 4u, 0u, &fn, err);
    }
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, fn)->body;
        values[0] = yvex_ir_block_at(m, block)->arguments[0];
    }
    for (unsigned int i = 0u; rc == YVEX_OK && i < 5u; ++i) {
        yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        yvex_core_text_copy(attrs[0].value.text, sizeof(attrs[0].value.text), symbols[i]);
        yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), source_identity);
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = types + parameter_types[i],
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) {
            values[parameter_types[i]] = yvex_ir_operation_at(m, op)->results[0];
            bindings[i] = (yvex_program_parameter_binding){values[parameter_types[i]], l->tensor_ids[slots[i]],
                l->qtypes[slots[i]]};
        }
    }
    /* Explicit ordered dataflow, including each precision conversion. The
     * affine projection sees the original input, before ingress BF16 rounding. */
    for (unsigned int step = 0u; rc == YVEX_OK && step < 7u; ++step) {
        const char *names[] = {"tensor.reshape", "nn.linear", "tensor.cast", "mhc.residual_pre",
            "nn.weighted_rms", "tensor.cast", "nn.linear"};
        unsigned int destinations[] = {1u, 3u, 4u, 7u, 7u, 10u, 13u};
        yvex_ir_id args[4] = {values[step == 0u || step == 2u ? 0u : step == 1u ? 1u :
            step == 3u ? 4u : step == 6u ? 10u : 7u], values[step == 1u ? 2u : step == 6u ? 12u : 11u]};
        yvex_ir_attribute attrs[] = {
            {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = l->rms_epsilon},
            {.name = "mhc_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = l->mhc_epsilon},
            {.name = "post_multiplier", .kind = YVEX_IR_ATTR_F64, .value.real = l->mhc_post_multiplier},
            {.name = "sinkhorn_iterations", .kind = YVEX_IR_ATTR_U64, .value.integer = l->mhc_sinkhorn_iterations}};
        yvex_ir_operation_request r = {.operation = names[step], .operands = args,
            .operand_count = step == 1u || step == 4u || step == 6u ? 2u : 1u,
            .result_types = types + destinations[step], .result_count = 1u};
        yvex_ir_attribute reduction = {.name = "reduction", .kind = YVEX_IR_ATTR_U64,
            .value.integer = YVEX_IR_REDUCTION_ROW_DOT};
        if (step == 1u || step == 6u) { r.attributes = &reduction; r.attribute_count = 1u; }
        if (step == 3u) {
            args[1] = values[3]; args[2] = values[5]; args[3] = values[6];
            r.operand_count = 4u; r.result_count = 3u; r.attributes = attrs; r.attribute_count = 4u;
        } else if (step == 4u) {
            r.attributes = attrs; r.attribute_count = 1u;
        }
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK)
            for (unsigned int i = 0u; i < r.result_count; ++i)
                values[destinations[step] + i] = yvex_ir_operation_at(m, op)->results[i];
    }
    if (rc == YVEX_OK) {
        results[0] = values[7]; results[1] = values[8]; results[2] = values[9]; results[3] = values[13];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = results, .operand_count = 4u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK)
        rc = yvex_program_physical_compile(out, execution, "ingress", bindings, 5u, physical_identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}
