/* A test caller owns compiled ingress results until all deferred consumers finish. */
#ifndef TESTS_SUPPORT_MOE_BACKEND_PROGRAM_H
#define TESTS_SUPPORT_MOE_BACKEND_PROGRAM_H
#include <yvex/internal/program_stage.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/internal/moe.h>
#include "tests/test.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_backend *backend;
    yvex_device_tensor *owners[5], views[5];
    yvex_moe_device_ingress ingress;
    yvex_program_physical *post_program;
    yvex_program_stage *post_stage;
    yvex_device_tensor *result_owners[3], result_views[3], *destination;
    yvex_moe_device_results results;
    yvex_device_tensor single_input;
    const float *input;
    unsigned long long rows, expanded, widths[5];
} test_moe_program;

/* The fixture composes the same admitted operation as the outer compiled
 * program. Backend expert execution does not own this residual equation. */
static int test_moe_post_open(test_moe_program *c, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = c->rows, .multiple = 1u};
    yvex_ir_id dim, types[4], result, function, block = YVEX_IR_NONE, op;
    int rc = yvex_ir_module_open(&m, "fixture_post", identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);

    for (size_t i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32,
            .rank = i == 0u || i == 3u ? 3u : 2u};
        t.shape[0] = (yvex_ir_extent){dim, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, c->widths[i == 1u ? 0u : 1u]};
        if (t.rank == 3u) t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE, c->widths[i == 0u ? 0u : 1u]};
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
        if (rc == YVEX_OK && i == 0u) {
            t.scalar = YVEX_IR_BF16;
            rc = yvex_ir_type_intern(m, &t, &result, err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "post", types, 4u, &result, 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        yvex_ir_operation_request r = {.operation = "mhc.residual_post",
            .operands = yvex_ir_block_at(m, block)->arguments, .operand_count = 4u,
            .result_types = &result, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_id value = yvex_ir_operation_at(m, op)->results[0];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&c->post_program, execution,
        "post", NULL, 0u, identity, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&c->post_stage, c->post_program, NULL, 0u,
        c->backend, c->rows, 1, 0u, 0u, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int test_moe_program_open(test_moe_program *c, yvex_backend *backend,
    const yvex_moe_layer_job *job, const float *input, unsigned long long rows, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    const unsigned int slots[] = {YVEX_MOE_WEIGHT_MHC_FUNCTION, YVEX_MOE_WEIGHT_MHC_SCALE,
        YVEX_MOE_WEIGHT_MHC_BASE, YVEX_MOE_WEIGHT_FFN_NORM, YVEX_MOE_WEIGHT_ROUTER};
    yvex_moe_layer_plan layer = *job->layer;
    yvex_program_kernel_parameter parameters[5] = {0};
    yvex_program_physical *program = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend_operation_facts facts;
    *c = (test_moe_program){.backend = backend, .input = input, .rows = rows, .expanded = layer.expanded_width,
        .widths = {layer.hidden_width, layer.residual_streams,
            layer.residual_streams * layer.residual_streams, layer.routed_experts, layer.hidden_width}};
    for (size_t i = 0u; i < 5u; ++i) {
        const yvex_moe_weight_view *w = &job->weights[slots[i]];
        layer.tensor_ids[slots[i]] = w->tensor_id; layer.qtypes[slots[i]] = w->qtype;
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = w->tensor_id,
            .weight = {w->encoded, w->encoded_bytes, w->row_count, w->row_width, w->row_bytes, w->qtype}};
    }
    int rc = yvex_moe_ingress_program_import(&program, &layer, identity, identity, rows, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&stage, program, parameters, 5u, backend, rows, 1, 0u, 0u, err);
    unsigned long long count = rows * (c->widths[0] + c->widths[1] + c->widths[2] + c->widths[3] + c->widths[4]);
    float *data = calloc((size_t)count, sizeof(float));
    float *out[] = {data, data ? data + rows * c->widths[0] : NULL,
        data ? data + rows * (c->widths[0] + c->widths[1]) : NULL,
        data ? data + rows * (c->widths[0] + c->widths[1] + c->widths[2]) : NULL,
        data ? data + rows * (c->widths[0] + c->widths[1] + c->widths[2] + c->widths[3]) : NULL};
    if (!data) rc = YVEX_ERR_NOMEM;
    if (rc == YVEX_OK) rc = yvex_program_stage_host(stage, rows, &input, 1u, out, 4u, NULL, NULL, &facts, err);
    int closed = yvex_program_stage_close(&stage, rc == YVEX_OK ? err : NULL);
    if (rc == YVEX_OK) rc = closed;
    yvex_program_physical_close(&program);
    for (size_t i = 0u; i < 3u; ++i) {
        const yvex_moe_weight_view *w = &job->weights[YVEX_MOE_WEIGHT_SHARED_GATE + i];
        layer.tensor_ids[YVEX_MOE_WEIGHT_SHARED_GATE + i] = w->tensor_id;
        layer.qtypes[YVEX_MOE_WEIGHT_SHARED_GATE + i] = w->qtype;
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = w->tensor_id,
            .weight = {w->encoded, w->encoded_bytes, w->row_count, w->row_width, w->row_bytes, w->qtype}};
    }
    if (rc == YVEX_OK) rc = yvex_moe_shared_program_import(&program, &layer, identity, identity, rows, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&stage, program, parameters, 3u, backend, rows, 1, 0u, 0u, err);
    const float *normalized = out[0];
    if (rc == YVEX_OK) rc = yvex_program_stage_host(stage, rows, &normalized, 1u, out + 4u, 1u,
        NULL, NULL, &facts, err);
    for (size_t i = 0u; rc == YVEX_OK && i < 5u; ++i) {
        yvex_backend_tensor_desc d = {.name = "test-compiled-ingress", .dtype = YVEX_DTYPE_F32,
            .rank = 1u, .dims = {rows * c->widths[i]}, .bytes = rows * c->widths[i] * sizeof(float)};
        rc = yvex_backend_tensor_alloc(backend, &d, &c->owners[i], err);
        if (rc == YVEX_OK) rc = yvex_backend_tensor_write(backend, c->owners[i], out[i], d.bytes, err);
    }
    closed = yvex_program_stage_close(&stage, rc == YVEX_OK ? err : NULL);
    if (rc == YVEX_OK) rc = closed;
    yvex_program_physical_close(&program); free(data);
    for (size_t i = 0u; rc == YVEX_OK && i < 3u; ++i) {
        yvex_backend_tensor_desc d = {.name = "test-expert-result", .dtype = YVEX_DTYPE_F32,
            .rank = 1u, .dims = {rows * c->widths[i]}, .bytes = rows * c->widths[i] * sizeof(float)};
        rc = yvex_backend_tensor_alloc(backend, &d, &c->result_owners[i], err);
    }
    if (rc == YVEX_OK) rc = test_moe_post_open(c, err);
    if (rc != YVEX_OK) fprintf(stderr, "MoE fixture ingress: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return rc;
}

static int test_moe_program_select(test_moe_program *c, yvex_moe_layer_job *job,
    const float *input, unsigned long long rows, yvex_error *err)
{
    if (!input || !rows || rows > c->rows) goto invalid;
    unsigned long long first;
    for (first = 0u; first + rows <= c->rows; ++first)
        if (!memcmp(c->input + first * c->expanded, input, (size_t)(rows * c->expanded) * sizeof(float))) break;
    if (first + rows > c->rows) goto invalid;
    for (size_t i = 0u; i < 5u; ++i)
        if (!yvex_backend_tensor_f32_subview(c->owners[i], first * c->widths[i], rows * c->widths[i], &c->views[i]))
            goto invalid;
    c->ingress = (yvex_moe_device_ingress){c->views, c->views + 1u, c->views + 2u, c->views + 3u, c->views + 4u};
    job->device_ingress = &c->ingress;
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "test.moe-program", "fixture lacks the requested compiled input population");
    return YVEX_ERR_FORMAT;
}

static int test_moe_program_post(test_moe_program *c, const yvex_moe_row_batch *rows, yvex_error *err)
{
    const yvex_device_tensor *owners[] = {rows->device_rows, c->results.combined,
        c->results.post, c->results.combination, c->destination};
    yvex_device_tensor views[5];
    for (size_t i = 0u; i < 5u; ++i) {
        const yvex_ir_type *t = &yvex_program_physical_value_at(c->post_program,
            i < 4u ? i : yvex_program_physical_result_at(c->post_program, 0u))->type;
        unsigned long long count = rows->row_count;
        for (unsigned int axis = 1u; axis < t->rank; ++axis) count *= t->shape[axis].extent;
        if (!yvex_backend_tensor_f32_subview(owners[i], 0u, count, views + i)) return YVEX_ERR_BOUNDS;
        views[i].rank = t->rank; views[i].dims[0] = rows->row_count;
        for (unsigned int axis = 1u; axis < t->rank; ++axis) views[i].dims[axis] = t->shape[axis].extent;
    }
    const yvex_device_tensor *inputs[] = {views, views + 1u, views + 2u, views + 3u};
    yvex_device_tensor *output = views + 4u;
    yvex_backend_operation_facts facts;
    int rc = yvex_program_stage_device(c->post_stage, rows->row_count, inputs, 4u,
        &output, 1u, NULL, NULL, &facts, err);
    if (rc == YVEX_OK) c->destination->is_written = output->is_written;
    return rc;
}

static int test_moe_program_results(test_moe_program *c, unsigned long long rows)
{
    for (size_t i = 0u; i < 3u; ++i)
        if (!yvex_backend_tensor_f32_subview(c->result_owners[i], 0u,
            rows * c->widths[i], &c->result_views[i])) return YVEX_ERR_BOUNDS;
    c->results = (yvex_moe_device_results){c->result_views, c->result_views + 1u, c->result_views + 2u};
    return YVEX_OK;
}

static int test_moe_program_rows(test_moe_program *c, const yvex_backend_moe_operations *ops,
    yvex_backend *backend, yvex_moe_layer_job *job, const yvex_moe_row_batch *rows,
    const yvex_moe_row_batch_output *output, yvex_moe_row_batch_result *result, yvex_error *err)
{
    yvex_moe_row_batch admitted = *rows;
    int rc = test_moe_program_select(c, job, rows->expanded_rows, rows->row_count, err);
    if (rc == YVEX_OK && c->destination) {
        rc = test_moe_program_results(c, rows->row_count);
        admitted.device_results = &c->results;
    }
    if (rc == YVEX_OK) rc = ops->execute_rows(backend, job, &admitted, output, result, err);
    if (rc == YVEX_OK && c->destination && result->completed) rc = test_moe_program_post(c, rows, err);
    return rc;
}

static int test_moe_program_begin(test_moe_program *c, yvex_backend_moe_execution **out,
    yvex_backend *backend, yvex_moe_layer_job *job, yvex_moe_layer_result *result, yvex_error *err)
{
    int rc = test_moe_program_select(c, job, job->expanded_input, 1u, err);
    if (rc == YVEX_OK && c->destination) {
        rc = test_moe_program_results(c, 1u);
        if (rc == YVEX_OK) {
            c->single_input = *job->device_input;
            job->device_results = &c->results;
        }
    }
    return rc == YVEX_OK ? yvex_backend_moe_begin(out, backend, job, result, err) : rc;
}

static int test_moe_program_finish(test_moe_program *c, yvex_backend_moe_execution *execution,
    yvex_moe_layer_result *result, yvex_error *err)
{
    int rc = yvex_backend_moe_finish(execution, result, err);
    yvex_moe_row_batch row = {.row_count = 1u, .device_rows = &c->single_input};
    if (rc == YVEX_OK && c->destination) rc = test_moe_program_post(c, &row, err);
    return rc;
}

static int test_moe_program_close(test_moe_program *c, yvex_error *err)
{
    int closed = yvex_program_stage_close(&c->post_stage, err);
    if (closed != YVEX_OK) return closed;
    yvex_program_physical_close(&c->post_program);
    for (size_t i = 0u; i < 3u; ++i) {
        int rc = c->result_owners[i] ? yvex_backend_tensor_release(c->backend, &c->result_owners[i], err) : YVEX_OK;
        if (rc != YVEX_OK) return rc;
    }
    for (size_t i = 0u; i < 5u; ++i) {
        int rc = c->owners[i] ? yvex_backend_tensor_release(c->backend, &c->owners[i], err) : YVEX_OK;
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}
#endif
