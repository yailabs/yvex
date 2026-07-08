/*
 * yvex_graph_guard.c - graph guard facts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   graph preflight guard facts over selected graph fixture/partial/segment
 *   execution inputs.
 *
 * Does not own:
 *   CLI input parsing, command dispatch, rendering, stdout/stderr output,
 *   primitive execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   guard code validates metadata, ranges, slices, and backend capability facts
 *   and never emits operator output.
 *
 * Boundary:
 *   graph guard success is not graph execution or generation readiness.
 */
#include "yvex_graph_guard.h"
#include "yvex_graph_private.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int cli_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void init_graph_guard_report(yvex_cli_graph_guard_report *report,
                                    const char *graph_kind,
                                    int needs_slice_range,
                                    const yvex_model_ref *model_ref)
{
    memset(report, 0, sizeof(*report));
    report->guard_status = "fail";
    report->phase = "preflight";
    report->graph_kind = graph_kind ? graph_kind : "unknown";
    report->integrity_status = "unchecked";
    report->identity_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->metadata_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->shape_status = "unchecked";
    report->range_status = "unchecked";
    report->slice_range_status = needs_slice_range ? "unchecked" : "not-needed";
    report->backend_status = "not-opened";
    report->backend_op_status = "unchecked";
    report->cleanup_status = "not-needed";
}

void yvex_graph_guard_report_init(yvex_cli_graph_guard_report *report)
{
    if (!report) {
        return;
    }
    init_graph_guard_report(report, "unknown", 0, NULL);
}

static const yvex_tensor_info *cli_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight",
        "blk.0.attention_norm.weight",
        "blk.0.input_layernorm.weight",
        "model.layers.0.input_layernorm.weight",
    };
    unsigned int i;
    unsigned long long count;
    unsigned long long index;

    if (!tensors) {
        return NULL;
    }
    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, preferred[i]);
        if (tensor) {
            return tensor;
        }
    }
    count = yvex_tensor_table_count(tensors);
    for (index = 0; index < count; ++index) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
        if (tensor && tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM) {
            return tensor;
        }
    }
    return NULL;
}

static int cli_has_rmsnorm_epsilon(const yvex_gguf *gguf)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon",
        "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, keys[i]);
        double epsilon = 0.0;
        if (value && yvex_gguf_value_as_f64(value, &epsilon) == YVEX_OK && epsilon > 0.0) {
            return 1;
        }
    }
    return 0;
}

int preflight_graph_guard(const yvex_model_ref *model_ref,
                                 const char *backend_name,
                                 int execute_fixture,
                                 int execute_segment,
                                 unsigned int token_id,
                                 yvex_cli_graph_guard_report *report,
                                 yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_artifact_integrity_report integrity_report;
    yvex_tensor_range tensor_range;
    yvex_tensor_slice_range slice_range;
    yvex_selected_embedding_shape embedding_shape;
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    const yvex_tensor_info *tensor;
    const yvex_tensor_info *rmsnorm_tensor = NULL;
    unsigned long long hidden_size;
    unsigned long long output_bytes;
    unsigned long long planned_bytes;
    int rc;

    init_graph_guard_report(report,
                            execute_fixture ? "fixture-embedding" :
                            (execute_segment ? "selected-embedding-rmsnorm"
                                             : "selected-embedding-partial"),
                            !execute_fixture,
                            model_ref);
    memset(&ctx, 0, sizeof(ctx));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&tensor_range, 0, sizeof(tensor_range));
    memset(&slice_range, 0, sizeof(slice_range));
    memset(&embedding_shape, 0, sizeof(embedding_shape));
    memset(&backend_options, 0, sizeof(backend_options));

    rc = open_model_context(model_ref->path, &ctx, err);
    if (rc != YVEX_OK) {
        report->integrity_status = "fail";
        return rc;
    }
    rc = yvex_artifact_integrity_validate(ctx.artifact,
                                          ctx.gguf,
                                          ctx.table,
                                          NULL,
                                          &integrity_report,
                                          err);
    report->integrity_status = (rc == YVEX_OK && integrity_report.passed) ? "pass" : "fail";
    report->shape_status =
        integrity_report.tensor_shapes_invalid == 0 &&
        integrity_report.tensor_dtypes_invalid == 0 &&
        integrity_report.tensor_byte_counts_invalid == 0 ? "pass" : "fail";
    report->range_status = integrity_report.tensor_ranges_invalid == 0 ? "pass" : "fail";
    if (rc != YVEX_OK || !integrity_report.passed) {
        close_model_context(&ctx);
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "graph_integrity_preflight",
                           "artifact integrity preflight failed");
        }
        return rc == YVEX_OK ? YVEX_ERR_STATE : rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (!tensor) {
        report->shape_status = "fail";
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, tensor, &tensor_range, err);
    if (rc != YVEX_OK) {
        report->range_status = "fail";
        close_model_context(&ctx);
        return rc;
    }
    report->range_status = "pass";

    if (execute_fixture) {
        if (tensor->dtype != YVEX_DTYPE_F32) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "fixture graph embed execution requires F32 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        if (tensor->rank != 2 || tensor->dims[0] == 0 || tensor->dims[1] == 0) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                           "fixture graph token embedding must be rank 2 with non-zero dims");
            return YVEX_ERR_FORMAT;
        }
        if ((unsigned long long)token_id >= tensor->dims[1]) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                            "fixture token id %u exceeds embedding vocab size %llu",
                            token_id, tensor->dims[1]);
            return YVEX_ERR_BOUNDS;
        }
        hidden_size = tensor->dims[0];
        if (hidden_size > (unsigned long long)(~(size_t)0 / sizeof(float))) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                           "fixture graph output is too large");
            return YVEX_ERR_BOUNDS;
        }
        report->shape_status = "pass";
        report->slice_range_status = "not-needed";
        report->output_bytes_planned = hidden_size * (unsigned long long)sizeof(float);
    } else {
        if (tensor->dtype != YVEX_DTYPE_F16) {
            report->shape_status = "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "real partial embedding segment requires F16 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        rc = yvex_selected_embedding_shape_validate(tensor, token_id, &embedding_shape, err);
        if (rc != YVEX_OK) {
            const char *msg = yvex_error_message(err);
            report->shape_status = msg && strstr(msg, "token-out-of-range") ? "pass" : "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            if (msg && strstr(msg, "token-out-of-range")) {
                yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                                "partial token out of range: %u >= %llu",
                                token_id, embedding_shape.vocab_size);
            }
            return rc;
        }
        rc = yvex_tensor_embedding_slice_range_validate(&tensor_range,
                                                        token_id,
                                                        &slice_range,
                                                        err);
        if (rc != YVEX_OK) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            return rc;
        }
        report->shape_status = "pass";
        report->slice_range_status = "pass";
        report->output_bytes_planned = embedding_shape.output_bytes;
        report->reference_bytes_planned = slice_range.slice_bytes;

        if (execute_segment) {
            rmsnorm_tensor = cli_find_first_rmsnorm_tensor(ctx.table);
            if (!rmsnorm_tensor) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-tensor-missing");
                return YVEX_ERR_UNSUPPORTED;
            }
            if (rmsnorm_tensor->rank != 1 || rmsnorm_tensor->dims[0] != embedding_shape.hidden_size) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-shape-invalid");
                return YVEX_ERR_FORMAT;
            }
            if (rmsnorm_tensor->dtype != YVEX_DTYPE_F16 &&
                rmsnorm_tensor->dtype != YVEX_DTYPE_F32) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-dtype-invalid");
                return YVEX_ERR_UNSUPPORTED;
            }
            rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, rmsnorm_tensor, &tensor_range, err);
            if (rc != YVEX_OK) {
                report->range_status = "fail";
                close_model_context(&ctx);
                return rc;
            }
            if (!cli_has_rmsnorm_epsilon(ctx.gguf)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-epsilon-missing");
                return YVEX_ERR_FORMAT;
            }
            if (embedding_shape.output_bytes > ULLONG_MAX / 2ull ||
                embedding_shape.output_bytes > (unsigned long long)(~(size_t)0)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                               "segment-memory-plan-overflow");
                return YVEX_ERR_BOUNDS;
            }
            output_bytes = embedding_shape.output_bytes;
            planned_bytes = output_bytes * 2ull;
            report->output_bytes_planned = planned_bytes;
            report->reference_bytes_planned = output_bytes;
        }
    }

    backend_options.kind = yvex_graph_backend_kind_from_name(backend_name);
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        report->backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        report->backend_op_status = "unsupported";
        close_model_context(&ctx);
        return rc;
    }
    report->backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        (execute_segment && !yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM)) ||
        cli_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        report->backend_op_status = "unsupported";
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "backend-op-unsupported");
        return YVEX_ERR_UNSUPPORTED;
    }
    report->backend_op_status = "supported";
    report->guard_status = "pass";
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return YVEX_OK;
}

int yvex_graph_guard_report_build(const yvex_graph_report_request *request,
                                  yvex_graph_report *report,
                                  yvex_error *err)
{
    (void)request;
    (void)report;
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_guard_report", "graph guard report command is not exposed directly");
    return YVEX_ERR_UNSUPPORTED;
}
